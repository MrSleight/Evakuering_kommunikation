
#include "PeopleCounter.h"

namespace {
    // The VL53L1X receives light on a grid of 16 x 16 detectors (SPADs).
    // A zone is a rectangle on that grid.
    constexpr uint8_t GRID_SIZE = 16;
}

static_assert(COUNTER_ZONE_WIDTH >= 4 && COUNTER_ZONE_WIDTH <= GRID_SIZE / 2,
              "COUNTER_ZONE_WIDTH must be 4..8 so both zones fit side by side");
static_assert(COUNTER_ZONE_HEIGHT >= 4 && COUNTER_ZONE_HEIGHT <= GRID_SIZE,
              "COUNTER_ZONE_HEIGHT must be 4..16");
static_assert(COUNTER_MIN_FLOOR_MM > COUNTER_MIN_TRIGGER_GAP_MM,
              "COUNTER_MIN_FLOOR_MM must be larger than COUNTER_MIN_TRIGGER_GAP_MM");
static_assert(COUNTER_EMPTY_CONFIRM >= 1, "COUNTER_EMPTY_CONFIRM must be at least 1");

PeopleCounter::PeopleCounter(TwoWire& i2cBus) : bus(i2cBus) {
    zones[ZONE_A].name = 'A';
    zones[ZONE_B].name = 'B';
}

// SETUP

void PeopleCounter::begin() {
    // Zone A uses the first columns of the grid, zone B the last ones.
    zones[ZONE_A].centerSpad = zoneCenter(0);
    zones[ZONE_B].centerSpad = zoneCenter(GRID_SIZE - COUNTER_ZONE_WIDTH);

    bus.begin(COUNTER_SDA_PIN, COUNTER_SCL_PIN, COUNTER_I2C_HZ);
    sensor.setBus(&bus);

    // Without a timeout the library waits forever for a sensor that
    // answers on I2C but never finishes booting.
    sensor.setTimeout(500);

    state = State::Starting;
}

bool PeopleCounter::startSensor() {
    // Besides stopping the sensor, stopContinuous() makes the library redo
    // its own start-up calibration with the next reading. That matters
    // when the sensor is restarted after an error.
    sensor.stopContinuous();

    if (!sensor.init()) {
        Serial.printf("[COUNTER] No VL53L1X found (SDA=%d, SCL=%d). Retrying in %lu s\n",
                      COUNTER_SDA_PIN, COUNTER_SCL_PIN,
                      (unsigned long)(COUNTER_RETRY_DELAY_MS / 1000));
        return false;
    }

    bool ok = sensor.setDistanceMode(COUNTER_DISTANCE_MODE);
    ok = ok && sensor.setMeasurementTimingBudget(COUNTER_TIMING_BUDGET_MS * 1000UL);
    sensor.setROISize(COUNTER_ZONE_WIDTH, COUNTER_ZONE_HEIGHT);
    ok = ok && sensor.last_status == 0;

    if (!ok) {
        Serial.printf("[COUNTER] Could not set up the VL53L1X. Retrying in %lu s\n",
                      (unsigned long)(COUNTER_RETRY_DELAY_MS / 1000));
        return false;
    }

    for (Zone& zone : zones) {
        zone.spads = 0;
        zone.badInRow = 0;
    }
    busFailures = 0;

    Serial.printf("[COUNTER] Sensor online. Zones %d x %d, centres %u (A) and %u (B)\n",
                  COUNTER_ZONE_WIDTH, COUNTER_ZONE_HEIGHT,
                  zones[ZONE_A].centerSpad, zones[ZONE_B].centerSpad);
    return true;
}

// MAIN UPDATE

void PeopleCounter::update() {
    checkZoneTimeouts();

    if (millis() - pauseStartedAt < pauseMs) {
        return;
    }
    pauseMs = 0;

    if (state == State::Starting) {
        if (!startSensor()) {
            pause(COUNTER_RETRY_DELAY_MS);
            return;
        }

        if (calibrated) {
            // The sensor was restarted after an error. It has not moved, so
            // the old calibration still holds, and measuring again now could
            // go wrong if people are walking through.
            forgetZones();
            state = State::Counting;
            Serial.println("[COUNTER] Counting again with the previous calibration");
        }
        else {
            startCalibration(0);
        }
        return;
    }

    if (!readingInProgress) {
        startReading();
    }
    else {
        collectReading();
    }
}

// READINGS

void PeopleCounter::startReading() {
    Zone& zone = zones[currentZone];

    // Choose which part of the grid to use. The zone is set before the
    // measurement starts, so every reading belongs to exactly one zone.
    sensor.setROICenter(zone.centerSpad);
    bool ok = sensor.last_status == 0;

    // After every reading the library works out how many SPADs to switch
    // on next time, so the next reading gets the right amount of light,
    // and writes that to the sensor. With the zones taking turns, that
    // setting would come from the other zone. Each zone keeps its own
    // setting instead (see collectReading) and puts it back here.
    if (ok && spadsPerZone && zone.spads != 0) {
        sensor.writeReg16Bit(VL53L1X::DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT, zone.spads);
        ok = sensor.last_status == 0;
    }

    if (ok) {
        sensor.readSingle(false);   // start one measurement, do not wait for it
        ok = sensor.last_status == 0;
    }

    if (!ok) {
        busFailed("I2C error while starting a reading");
        return;
    }

    readingInProgress = true;
    readingStartedAt = millis();
}

void PeopleCounter::collectReading() {
    const uint32_t elapsed = millis() - readingStartedAt;

    // A reading takes the whole timing budget, no point asking earlier.
    if (elapsed < COUNTER_TIMING_BUDGET_MS) {
        return;
    }

    if (elapsed > COUNTER_READING_TIMEOUT_MS) {
        busFailed("reading timed out");
        return;
    }

    const bool ready = sensor.dataReady();
    if (sensor.last_status != 0) {
        busFailed("I2C error while waiting for a reading");
        return;
    }
    if (!ready) {
        return;
    }

    const uint16_t mm = sensor.read(false);
    readingInProgress = false;
    if (sensor.last_status != 0) {
        busFailed("I2C error while collecting a reading");
        return;
    }

    // The sensor answered, so the bus is fine, whatever the reading says.
    busFailures = 0;
    readingNumber++;

    Zone& zone = zones[currentZone];
    zone.lastMm = mm;
    zone.lastStatus = sensor.ranging_data.range_status;

    if (spadsPerZone || debug) {
        // read() has just written the SPAD setting worked out from this
        // reading. Keep it for this zone's next reading.
        zone.spads = sensor.readReg16Bit(VL53L1X::DSS_CONFIG__MANUAL_EFFECTIVE_SPADS_SELECT);
        if (sensor.last_status != 0) {
            zone.spads = 0;
        }
    }

    if (debug) {
        printReading(zone);
    }

    const bool good = isGood(mm, zone.lastStatus);

    if (state == State::Calibrating) {
        addCalibrationReading(zone, mm, good);
    }
    else if (good || showsSomeone(zone, mm, zone.lastStatus)) {
        usableReading(currentZone, mm);
    }
    else {
        badReading(currentZone);
    }

    // Take turns: A, B, A, B, ...
    currentZone = (currentZone == ZONE_A) ? ZONE_B : ZONE_A;
}

// The sensor did not answer, or a reading never finished. This is a
// problem with the bus or the sensor, not a strange reading.
void PeopleCounter::busFailed(const char* reason) {
    // Abort the reading in case the sensor is still working on it.
    sensor.stopContinuous();
    readingInProgress = false;

    Serial.printf("[COUNTER] Zone %c: %s\n", zones[currentZone].name, reason);
    countUnknown("the sensor did not answer");

    busFailures++;
    if (busFailures >= COUNTER_MAX_BUS_FAILURES) {
        Serial.println("[COUNTER] Sensor stopped answering. Restarting it");
        busFailures = 0;
        state = State::Starting;
        pause(COUNTER_RETRY_DELAY_MS);
    }
}

// CALIBRATION

void PeopleCounter::startCalibration(uint32_t delayMs) {
    countUnknown("calibrating");
    for (Zone& zone : zones) {
        zone.sampleCount = 0;
        zone.attempts = 0;
    }

    state = State::Calibrating;
    pause(delayMs);

    if (delayMs == 0) {
        Serial.println("[COUNTER] Calibrating. Keep the doorway empty");
    }
    else {
        Serial.printf("[COUNTER] Calibrating again in %lu s. Keep the doorway empty\n",
                      (unsigned long)(delayMs / 1000));
    }
}

void PeopleCounter::addCalibrationReading(Zone& zone, uint16_t mm, bool good) {
    if (zone.sampleCount == COUNTER_CALIBRATION_SAMPLES) {
        return;   // this zone is done, waiting for the other one
    }

    zone.attempts++;

    if (good) {
        zone.samples[zone.sampleCount++] = mm;
    }

    if (zone.sampleCount < COUNTER_CALIBRATION_SAMPLES &&
        zone.attempts >= COUNTER_CALIBRATION_ATTEMPTS) {

        Serial.printf("[COUNTER] Calibration failed: zone %c gave %u good readings out of %u "
                      "(last: %s). The floor may be too far away or too dark\n",
                      zone.name, zone.sampleCount, zone.attempts,
                      statusText(zone.lastStatus));

        startCalibration(COUNTER_RETRY_DELAY_MS);
        return;
    }

    if (zones[ZONE_A].sampleCount == COUNTER_CALIBRATION_SAMPLES &&
        zones[ZONE_B].sampleCount == COUNTER_CALIBRATION_SAMPLES) {
        finishCalibration();
    }
}

void PeopleCounter::finishCalibration() {
    for (Zone& zone : zones) {
        // The median ignores a few odd readings, e.g. someone stepping
        // into the doorway for a moment during calibration.
        zone.floorMm = median(zone.samples, zone.sampleCount);

        if (zone.floorMm < COUNTER_MIN_FLOOR_MM) {
            Serial.printf("[COUNTER] Calibration failed: zone %c sees something only %u mm away. "
                          "Is the protective film still on, or something in front of the sensor?\n",
                          zone.name, zone.floorMm);

            startCalibration(COUNTER_RETRY_DELAY_MS);
            return;
        }

        // A reading means "person" when it is clearly shorter than the floor:
        // below COUNTER_TRIGGER_PERCENT of it, and at least
        // COUNTER_MIN_TRIGGER_GAP_MM closer. The lower of the two wins.
        const uint16_t byPercent = zone.floorMm * COUNTER_TRIGGER_PERCENT / 100;
        const uint16_t byGap = zone.floorMm - COUNTER_MIN_TRIGGER_GAP_MM;

        zone.triggerMm = (byPercent < byGap) ? byPercent : byGap;
    }

    calibrated = true;
    forgetZones();
    state = State::Counting;

    Serial.printf("[COUNTER] Calibrated. Zone A: floor %u mm, person below %u mm | "
                  "Zone B: floor %u mm, person below %u mm\n",
                  zones[ZONE_A].floorMm, zones[ZONE_A].triggerMm,
                  zones[ZONE_B].floorMm, zones[ZONE_B].triggerMm);
    Serial.println("[COUNTER] Counting");
}

// COUNTING
//
// Every reading that comes back gets the next number (readingNumber).
// Each zone remembers the numbers of the readings that saw someone and
// the readings that saw the floor. Comparing those numbers tells which
// zone saw the person first and which saw them last.

// A reading the counter can use: a good reading, or a near reading that
// only failed the wrap-around check (see showsSomeone).
void PeopleCounter::usableReading(uint8_t index, uint16_t mm) {
    Zone& zone = zones[index];
    zone.badInRow = 0;
    zone.lastGoodAt = millis();

    if (mm < zone.triggerMm) {
        // Someone is in this zone. One near reading is enough.
        zone.lastNear = readingNumber;
        zone.farInRow = 0;

        if (!zone.known || !zone.occupied) {
            zone.known = true;
            zone.occupied = true;
            zone.occupiedSince = readingNumber;
            zoneBecameOccupied(index);
        }
    }
    else {
        // The zone sees the floor. It only counts as empty after
        // COUNTER_EMPTY_CONFIRM of these in a row.
        zone.lastFar = readingNumber;
        if (zone.farInRow == 0) {
            zone.farSince = readingNumber;
        }
        if (zone.farInRow < 255) {
            zone.farInRow++;
        }

        if (zone.farInRow >= COUNTER_EMPTY_CONFIRM) {
            zone.known = true;
            zone.occupied = false;
        }
    }

    const bool bothEmpty = zones[ZONE_A].known && !zones[ZONE_A].occupied &&
                           zones[ZONE_B].known && !zones[ZONE_B].occupied;
    if (bothEmpty) {
        if (doorway == Doorway::Crossing) {
            finishCrossing();
        }
        doorway = Doorway::Empty;
    }
}

// A failed reading: the sensor answered, but the reading has a bad
// status or an impossible distance. That happens most often when a zone
// sees half a person and half the floor.
void PeopleCounter::badReading(uint8_t index) {
    Zone& zone = zones[index];

    // A failed reading never helps to show that a zone is empty.
    zone.farInRow = 0;
    if (zone.badInRow < 255) {
        zone.badInRow++;
    }

    if (!zone.known) {
        return;   // nothing known about this zone, so nothing to lose
    }

    if (zone.badInRow == 1) {
        // One failed reading on its own is skipped: the zone keeps what
        // its last good reading saw. The next reading of this zone has to
        // be good again, otherwise the count becomes unknown.
        Serial.printf("[COUNTER] Zone %c: failed reading skipped (%u mm, %s)\n",
                      zone.name, zone.lastMm, statusText(zone.lastStatus));
        return;
    }

    char reason[80];
    snprintf(reason, sizeof(reason), "zone %c gave %u failed readings in a row (last: %u mm, %s)",
             zone.name, zone.badInRow, zone.lastMm, statusText(zone.lastStatus));
    countUnknown(reason);
}

void PeopleCounter::zoneBecameOccupied(uint8_t index) {
    if (doorway == Doorway::Empty) {
        // Someone arrives, and this zone saw them first.
        doorway = Doorway::Crossing;
        firstZone = index;
        crossingStart = readingNumber;
        return;
    }

    if (doorway == Doorway::Crossing && index != firstZone) {
        // The second zone must have seen the floor after the first zone
        // saw the person. If not, the person reached both zones between
        // two readings and we cannot tell which zone was first.
        if (zones[index].lastFar < crossingStart) {
            countUnknown("someone reached both zones between two readings, direction unclear");
        }
    }
}

void PeopleCounter::finishCrossing() {
    const Zone& a = zones[ZONE_A];
    const Zone& b = zones[ZONE_B];

    // The zone that saw the person last must still have seen them after
    // the other zone started to see the floor again.
    uint8_t lastZone;
    if (a.lastNear > b.farSince) {
        lastZone = ZONE_A;
    }
    else if (b.lastNear > a.farSince) {
        lastZone = ZONE_B;
    }
    else {
        countUnknown("someone left both zones between two readings, direction unclear");
        return;
    }

    if (lastZone == firstZone) {
        Serial.printf("[COUNTER] Turned back in zone %c, not counted\n", zones[firstZone].name);
        return;
    }

    bool entered = (firstZone == ZONE_A);
    if (COUNTER_SWAP_DIRECTION) {
        entered = !entered;
    }
    countCrossing(entered);
}

void PeopleCounter::countCrossing(bool entered) {
    if (entered) {
        entries++;
        if (occupancyKnown) {
            peopleInside++;
        }
    }
    else {
        exits++;
        if (occupancyKnown && peopleInside > 0) {
            peopleInside--;
        }
        else if (occupancyKnown) {
            // Someone left an empty room, so someone was missed earlier.
            occupancyKnown = false;
            Serial.println("[COUNTER] Exit while the count was 0. Count unknown");
        }
    }

    if (occupancyKnown) {
        Serial.printf("[COUNTER] %s | inside %d | entries %lu, exits %lu\n",
                      entered ? "ENTRY" : "EXIT ", peopleInside,
                      (unsigned long)entries, (unsigned long)exits);
    }
    else {
        Serial.printf("[COUNTER] %s | inside unknown | entries %lu, exits %lu\n",
                      entered ? "ENTRY" : "EXIT ",
                      (unsigned long)entries, (unsigned long)exits);
    }
}

void PeopleCounter::checkZoneTimeouts() {
    if (state != State::Counting) {
        return;
    }

    const uint32_t now = millis();
    for (const Zone& zone : zones) {
        if (zone.known && now - zone.lastGoodAt > COUNTER_ZONE_TIMEOUT_MS) {
            char reason[64];
            snprintf(reason, sizeof(reason), "no good reading from zone %c for %lu ms",
                     zone.name, (unsigned long)(now - zone.lastGoodAt));
            countUnknown(reason);
            return;
        }
    }
}

// The counter may have missed someone. The count stays unknown until an
// operator confirms the room is empty again (resetCounts).
void PeopleCounter::countUnknown(const char* reason) {
    if (occupancyKnown || doorway != Doorway::Unknown) {
        Serial.printf("[COUNTER] Lost track: %s\n", reason);
    }
    occupancyKnown = false;
    forgetZones();
}

// Forget what the zones saw. Counting starts again once both zones have
// been seen empty.
void PeopleCounter::forgetZones() {
    for (Zone& zone : zones) {
        zone.known = false;
        zone.occupied = false;
        zone.farInRow = 0;
    }
    doorway = Doorway::Unknown;
}

// CONTROL

void PeopleCounter::recalibrate() {
    calibrated = false;
    if (readingInProgress) {
        sensor.stopContinuous();
        readingInProgress = false;
    }

    // While Starting, calibration begins as soon as the sensor is found.
    if (state != State::Starting) {
        startCalibration(0);
    }
}

bool PeopleCounter::resetCounts() {
    const uint32_t now = millis();
    const bool zonesFresh = now - zones[ZONE_A].lastGoodAt <= COUNTER_ZONE_TIMEOUT_MS &&
                            now - zones[ZONE_B].lastGoodAt <= COUNTER_ZONE_TIMEOUT_MS;

    if (state != State::Counting || doorway != Doorway::Empty || !zonesFresh) {
        Serial.println("[COUNTER] Cannot start at 0 yet: both zones must see the floor");
        return false;
    }

    peopleInside = 0;
    entries = 0;
    exits = 0;
    occupancyKnown = true;
    Serial.println("[COUNTER] Room confirmed empty. Count is 0");
    return true;
}

void PeopleCounter::setDebug(bool enabled) {
    debug = enabled;
}

bool PeopleCounter::isDebugEnabled() const {
    return debug;
}

void PeopleCounter::setSpadsPerZone(bool enabled) {
    spadsPerZone = enabled;
}

bool PeopleCounter::isSpadsPerZone() const {
    return spadsPerZone;
}

void PeopleCounter::pause(uint32_t ms) {
    pauseStartedAt = millis();
    pauseMs = ms;
}

// GETTERS

PeopleCounter::State PeopleCounter::getState() const {
    return state;
}

bool PeopleCounter::isReady() const {
    const uint32_t now = millis();
    return state == State::Counting && occupancyKnown &&
           now - zones[ZONE_A].lastGoodAt <= COUNTER_ZONE_TIMEOUT_MS &&
           now - zones[ZONE_B].lastGoodAt <= COUNTER_ZONE_TIMEOUT_MS;
}

bool PeopleCounter::tryGetOccupancy(int& occupancy) const {
    if (!isReady()) {
        return false;
    }
    occupancy = peopleInside;
    return true;
}

uint32_t PeopleCounter::getEntries() const {
    return entries;
}

uint32_t PeopleCounter::getExits() const {
    return exits;
}

// PRINTING

// One line per reading, for example:
//   [COUNTER] #1234   56789 ms  A  412 mm near  signal 10.31 ambient 0.12 spads 18.5
void PeopleCounter::printReading(const Zone& zone) const {
    const bool good = isGood(zone.lastMm, zone.lastStatus);
    char kind[40];

    if (state != State::Counting) {
        snprintf(kind, sizeof(kind), "%s", good ? "-" : statusText(zone.lastStatus));
    }
    else if (good) {
        snprintf(kind, sizeof(kind), "%s", (zone.lastMm < zone.triggerMm) ? "near" : "far");
    }
    else if (showsSomeone(zone, zone.lastMm, zone.lastStatus)) {
        snprintf(kind, sizeof(kind), "near, %s", statusText(zone.lastStatus));
    }
    else {
        snprintf(kind, sizeof(kind), "%s", statusText(zone.lastStatus));
    }

    Serial.printf("[COUNTER] #%lu %7lu ms  %c %4u mm %-4s  signal %.2f ambient %.2f spads %.1f\n",
                  (unsigned long)readingNumber, (unsigned long)millis(), zone.name,
                  zone.lastMm, kind,
                  sensor.ranging_data.peak_signal_count_rate_MCPS,
                  sensor.ranging_data.ambient_count_rate_MCPS,
                  zone.spads / 256.0);
}

void PeopleCounter::printStatus() const {
    static const char* const stateNames[] = {"starting", "calibrating", "counting"};
    static const char* const doorwayNames[] = {"unknown", "empty", "crossing"};

    if (isReady()) {
        Serial.printf("[COUNTER] State: %s | inside %d | entries %lu, exits %lu\n",
                      stateNames[static_cast<uint8_t>(state)], peopleInside,
                      (unsigned long)entries, (unsigned long)exits);
    }
    else {
        Serial.printf("[COUNTER] State: %s | inside unknown | entries %lu, exits %lu\n",
                      stateNames[static_cast<uint8_t>(state)],
                      (unsigned long)entries, (unsigned long)exits);
    }
    Serial.printf("[COUNTER] Doorway: %s | SPADs per zone: %s\n",
                  doorwayNames[static_cast<uint8_t>(doorway)], spadsPerZone ? "on" : "off");

    for (const Zone& zone : zones) {
        Serial.printf("[COUNTER] Zone %c: floor %u mm, person below %u mm, "
                      "last reading %u mm (%s), %s\n",
                      zone.name, zone.floorMm, zone.triggerMm, zone.lastMm,
                      statusText(zone.lastStatus),
                      !zone.known ? "unknown" : zone.occupied ? "occupied" : "empty");
    }
}

// HELPERS

bool PeopleCounter::isGood(uint16_t mm, uint8_t status) {
    // Status 6 means the sensor skipped one of its checks, which single
    // readings can do. The distance itself is fine.
    const bool goodStatus = status == VL53L1X::RangeValid ||
                            status == VL53L1X::RangeValidNoWrapCheckFail;
    return goodStatus && mm >= COUNTER_MIN_RANGE_MM && mm <= COUNTER_MAX_RANGE_MM;
}

// "Wrap target fail" means the sensor could not rule out that the light
// came back from something much farther away than the distance it gives,
// beyond its range. In a doorway the floor is much closer than that, so
// there is nothing that far away. When such a reading is also clearly
// shorter than the floor, something is in the zone. It happens at the
// edge of a person, when a zone sees part person and part floor.
//
// Such a reading is only ever used as a sign that someone is there,
// never as a sign that the zone is empty.
bool PeopleCounter::showsSomeone(const Zone& zone, uint16_t mm, uint8_t status) {
    return COUNTER_WRAP_FAIL_SHOWS_SOMEONE &&
           status == VL53L1X::WrapTargetFail &&
           mm >= COUNTER_MIN_RANGE_MM && mm < zone.triggerMm;
}

const char* PeopleCounter::statusText(uint8_t status) {
    return VL53L1X::rangeStatusToString(static_cast<VL53L1X::RangeStatus>(status));
}

// The sensor numbers its detectors in an unusual order (from ST's
// datasheet, seen from the front with pin 1 at the top left):
//
//   row 0:   128 136 144 ... 248     the top half counts down the columns
//   row 7:   135 143 151 ... 255
//   row 8:   127 119 111 ...   7     the bottom half counts up, mirrored
//   row 15:  120 112 104 ...   0
uint8_t PeopleCounter::spadNumber(uint8_t row, uint8_t column) {
    if (row < 8) {
        return 128 + column * 8 + row;
    }
    return 127 - column * 8 - (row - 8);
}

// The sensor wants the detector at the centre of the zone. For an even
// size the centre falls between detectors; the sensor then uses the one
// right of and above the middle (the full 16 x 16 grid has centre 199).
uint8_t PeopleCounter::zoneCenter(uint8_t firstColumn) {
    const uint8_t firstRow = (GRID_SIZE - COUNTER_ZONE_HEIGHT) / 2;

    const uint8_t row = firstRow + (COUNTER_ZONE_HEIGHT - 1) / 2;
    const uint8_t column = firstColumn + COUNTER_ZONE_WIDTH / 2;

    return spadNumber(row, column);
}

uint16_t PeopleCounter::median(const uint16_t* values, uint8_t count) {
    uint16_t sorted[COUNTER_CALIBRATION_SAMPLES];

    // Insertion sort: fine for 15 numbers.
    for (uint8_t i = 0; i < count; i++) {
        uint16_t value = values[i];
        uint8_t j = i;

        while (j > 0 && sorted[j - 1] > value) {
            sorted[j] = sorted[j - 1];
            j--;
        }
        sorted[j] = value;
    }

    return sorted[count / 2];
}

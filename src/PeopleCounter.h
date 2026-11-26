
#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>

#include "PeopleCounterConfig.h"

// Counts people walking through a doorway with one VL53L1X sensor.
//
// The sensor's view is split into two zones, A and B, that a person
// passes one after the other. The counter reads A, B, A, B, ... and
// numbers every reading. The order in which the zones see the person
// gives the direction:
//
//     first in A, last in B   ->  ENTRY
//     first in B, last in A   ->  EXIT
//     first and last the same ->  turned back, not counted
//
// The count is only reported while it is known to be right. It starts
// unknown, becomes known when an operator confirms the room is empty
// (resetCounts), and becomes unknown again whenever the counter could
// have missed someone: failed readings, a sensor that stops answering, or
// a crossing too fast to tell which zone came first.
//
// update() never waits for the sensor: it checks whether a reading is
// ready and returns straight away, so it can share loop() with
// mesh.update().
class PeopleCounter {
public:
    enum class State : uint8_t {
        Starting,     // finding and setting up the sensor
        Calibrating,  // measuring the empty doorway
        Counting      // ready
    };

    explicit PeopleCounter(TwoWire& i2cBus);

    void begin();
    void update();

    // Measure the empty doorway again, e.g. after moving the sensor.
    void recalibrate();

    // The operator confirms the room is empty: the count becomes a known 0.
    // Refused (returns false) unless both zones currently see the floor.
    bool resetCounts();

    // Print every reading (for tuning and testing).
    void setDebug(bool enabled);
    bool isDebugEnabled() const;

    // Test switch for COUNTER_SPADS_PER_ZONE.
    void setSpadsPerZone(bool enabled);
    bool isSpadsPerZone() const;

    void printStatus() const;

    State getState() const;

    // True while the count is known. Use this before sending it anywhere.
    bool isReady() const;

    // Gives the number of people inside and returns true, or returns false
    // when the count is unknown.
    bool tryGetOccupancy(int& occupancy) const;

    uint32_t getEntries() const;
    uint32_t getExits() const;

private:
    struct Zone {
        char name;               // 'A' or 'B', for log messages
        uint8_t centerSpad;      // where on the sensor grid the zone sits
        uint16_t floorMm;        // empty doorway distance, from calibration
        uint16_t triggerMm;      // closer than this = someone is in the zone

        // What the zone sees now
        bool known;              // false until good readings show empty or occupied
        bool occupied;
        uint8_t farInRow;        // good far readings in a row
        uint8_t badInRow;        // failed readings in a row
        uint32_t lastGoodAt;     // millis() of the last good reading

        // Reading numbers, to tell which zone saw what first
        uint32_t occupiedSince;  // the reading that made the zone occupied
        uint32_t lastNear;       // the last reading that saw someone
        uint32_t lastFar;        // the last reading that saw the floor
        uint32_t farSince;       // the first reading of the current run of far readings

        uint16_t lastMm;         // last reading, for printing
        uint8_t lastStatus;
        uint16_t spads;          // SPAD setting for this zone's next reading, 0 = none yet

        // Calibration
        uint16_t samples[COUNTER_CALIBRATION_SAMPLES];
        uint8_t sampleCount;
        uint8_t attempts;
    };

    // The doorway as a whole
    enum class Doorway : uint8_t {
        Unknown,   // waiting until both zones are seen empty
        Empty,     // both zones empty, waiting for someone
        Crossing   // someone is in one or both zones
    };

    static constexpr uint8_t ZONE_A = 0;
    static constexpr uint8_t ZONE_B = 1;

    TwoWire& bus;
    VL53L1X sensor;

    State state = State::Starting;
    Zone zones[2] = {};
    bool calibrated = false;

    // The reading the sensor is working on
    uint8_t currentZone = ZONE_A;
    bool readingInProgress = false;
    uint32_t readingStartedAt = 0;
    uint32_t readingNumber = 0;      // counts every reading that came back
    uint8_t busFailures = 0;         // I2C errors or timeouts in a row

    // Waiting before a retry
    uint32_t pauseStartedAt = 0;
    uint32_t pauseMs = 0;

    // The crossing going on now
    Doorway doorway = Doorway::Unknown;
    uint8_t firstZone = ZONE_A;      // the zone that saw the person first
    uint32_t crossingStart = 0;      // the reading that started the crossing

    // The count
    bool occupancyKnown = false;
    int peopleInside = 0;
    uint32_t entries = 0;
    uint32_t exits = 0;

    bool debug = COUNTER_DEBUG_READINGS;
    bool spadsPerZone = COUNTER_SPADS_PER_ZONE;

    bool startSensor();
    void startReading();
    void collectReading();
    void busFailed(const char* reason);

    void startCalibration(uint32_t delayMs);
    void addCalibrationReading(Zone& zone, uint16_t mm, bool good);
    void finishCalibration();

    void usableReading(uint8_t index, uint16_t mm);
    void badReading(uint8_t index);
    void zoneBecameOccupied(uint8_t index);
    void finishCrossing();
    void countCrossing(bool entered);
    void checkZoneTimeouts();
    void countUnknown(const char* reason);
    void forgetZones();

    void pause(uint32_t ms);
    void printReading(const Zone& zone) const;

    static bool isGood(uint16_t mm, uint8_t status);
    static bool showsSomeone(const Zone& zone, uint16_t mm, uint8_t status);
    static const char* statusText(uint8_t status);
    static uint8_t spadNumber(uint8_t row, uint8_t column);
    static uint8_t zoneCenter(uint8_t firstColumn);
    static uint16_t median(const uint16_t* values, uint8_t count);
};

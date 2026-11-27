#include "PeopleCounter.h"

#include <Arduino.h>
#include <Wire.h>
#include <VL53L1X.h>

namespace
{
enum class DetectionState : uint8_t
{
  Clear,
  AOnly,
  BOnly,
  Both
};

VL53L1X sensor;

const uint16_t thresholdA = 890;
const uint16_t thresholdB = 880;
const uint16_t hysteresisMm = 100;
const uint32_t maxMeasurementAgeMs = 1000;

bool measuringZoneA = true;
bool detectedA = false;
bool detectedB = false;
bool hasValidA = false;
bool hasValidB = false;

bool measurementRunning = false;
uint32_t measurementStarted = 0;

uint32_t lastValidA = 0;
uint32_t lastValidB = 0;

int peopleCount = 0;

DetectionState previousState = DetectionState::Clear;
DetectionState passage[3];
uint8_t passageLength = 0;
bool readyToCount = false;

void switchZone()
{
  measuringZoneA = !measuringZoneA;

  if (measuringZoneA)
  {
    sensor.setROICenter(167);
  }
  else
  {
    sensor.setROICenter(231);
  }
}
} // namespace

void initPeopleCounter()
{
  Wire.begin(21, 22);

  sensor.setTimeout(500);

  if (!sensor.init())
  {
    Serial.println("FEL: Sensorn kunde inte starta.");

    while (true)
    {
      delay(1000);
    }
  }

  sensor.setDistanceMode(VL53L1X::Short);
  sensor.setMeasurementTimingBudget(20000);
  sensor.setROISize(8, 16);
  sensor.setROICenter(167);

  
  Serial.println("Startad | Kvar: 0");
}

void updatePeopleCounter()
{
  const uint32_t readTime = millis();

  // Avbryt en pågående följd om mätningarna blivit för gamla.
  if (readyToCount &&
      ((readTime - lastValidA > maxMeasurementAgeMs) ||
       (readTime - lastValidB > maxMeasurementAgeMs)))
  {
    readyToCount = false;
    passageLength = 0;

    Serial.println("FEL: Matningar for gamla. Rakning pausad.");
  }

  // Starta mätningen utan att vänta på resultatet.
  if (!measurementRunning)
  {
    sensor.readSingle(false);
    measurementStarted = millis();
    measurementRunning = true;
    return;
  }

  // Fortsätt med andra uppgifter tills resultatet är klart.
  if (!sensor.dataReady())
  {
    if (millis() - measurementStarted >= 500)
    {
      Serial.println("FEL: Timeout vid avlasning.");

      sensor.stopContinuous();
      measurementRunning = false;
      switchZone();
    }

    return;
  }

  const uint16_t distance = sensor.read(false);
  measurementRunning = false;

  // Ogiltiga mätningar används inte för detektering.
  if (sensor.ranging_data.range_status != VL53L1X::RangeValid)
  {
    switchZone();
    return;
  }

  /*Serial.printf("Zon %s | Avstand: %u mm\n",
                measuringZoneA ? "A" : "B",
                static_cast<unsigned int>(distance));*/

  if (measuringZoneA)
  {
    detectedA = distance <
                (detectedA ? thresholdA : thresholdA - hysteresisMm);

    hasValidA = true;
    lastValidA = readTime;
  }
  else
  {
    detectedB = distance <
                (detectedB ? thresholdB : thresholdB - hysteresisMm);

    hasValidB = true;
    lastValidB = readTime;
  }

  const bool measurementsFresh =
      hasValidA &&
      hasValidB &&
      (readTime - lastValidA <= maxMeasurementAgeMs) &&
      (readTime - lastValidB <= maxMeasurementAgeMs);

  if (!measurementsFresh)
  {
    readyToCount = false;
    passageLength = 0;
    switchZone();
    return;
  }

  DetectionState currentState;

  if (detectedA && detectedB)
  {
    currentState = DetectionState::Both;
  }
  else if (detectedA)
  {
    currentState = DetectionState::AOnly;
  }
  else if (detectedB)
  {
    currentState = DetectionState::BOnly;
  }
  else
  {
    currentState = DetectionState::Clear;
  }

  // Bedöm passagen när båda zonerna är fria.
  if (currentState == DetectionState::Clear)
  {
    if (readyToCount && passageLength == 3)
    {
      if (passage[0] == DetectionState::AOnly &&
          passage[1] == DetectionState::Both &&
          passage[2] == DetectionState::BOnly)
      {
        peopleCount++;
        Serial.printf("IN | Kvar: %d\n", peopleCount);
      }
      else if (passage[0] == DetectionState::BOnly &&
               passage[1] == DetectionState::Both &&
               passage[2] == DetectionState::AOnly)
      {
        if (peopleCount > 0)
        {
          peopleCount--;
          Serial.printf("UT | Kvar: %d\n", peopleCount);
        }
        else
        {
          Serial.println("UT | Kvar: 0 (redan 0)");
        }
      }
    }

    readyToCount = true;
    passageLength = 0;
  }

  // Spara endast ändringar mellan tillstånden.
  if (currentState != previousState)
  {
    if (readyToCount && currentState != DetectionState::Clear)
    {
      if (passageLength < 3)
      {
        passage[passageLength] = currentState;
        passageLength++;
      }
      else
      {
        readyToCount = false;
        passageLength = 0;
      }
    }

    previousState = currentState;
  }

  switchZone();
}

int getPeopleCount()
{
  return peopleCount;
}

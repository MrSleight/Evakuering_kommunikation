
#pragma once

#include <Arduino.h>

// Message identifiers
enum class MessageType : uint8_t {
    SENSOR_A = 1,
    SENSOR_B = 2,
    BLOCKAGE = 3
};

// Sensor A: Boolean value (0 or 1)
struct SensorAMessage {
    bool value;
};

// Sensor B: Integer value
struct SensorBMessage {
    int value;
};

// Blockage: ID and blockage state
struct BlockageMessage {
    int id;
    bool blocked;
};

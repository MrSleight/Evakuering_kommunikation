
#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "MessageTypes.h"

// Initialise the messaging system
void beginMessageManager();

// Process incoming and outgoing messages
void updateMessageManager();

// Update sensor buffers
void updateSensorA(bool value);
void updateSensorB(int value);

// Update blockage state
void updateBlockage(uint32_t id, bool blocked);
void handleBlockageMessage(JsonDocument &doc);

bool isNodeBlocked(uint32_t nodeId);
bool consumeBlockageChanged();

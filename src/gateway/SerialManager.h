
#pragma once

#include <ArduinoJson.h>

void beginSerialManager();
void processSerialInput();
void updateSerialConnection();

bool isHostConnected();

void sendSerialJson(JsonDocument &doc);


#pragma once

#include <Arduino.h>

void beginGatewayManager();

void updateGatewayManager();

void announceGateway(bool active);

bool sendToServer(const String &payload);

void handleGatewayMessage(uint32_t from, JsonDocument &doc);

bool isGatewayAvailable();

uint32_t getGatewayNodeId();

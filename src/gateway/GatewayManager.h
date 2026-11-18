
#pragma once
#include "painlessMesh.h"
#include <Arduino.h>

void beginGatewayManager();

void updateGatewayManager();

void announceGateway(bool active);

bool sendToServer(const String &payload);

bool sendToServerReliable(
    const String &payload,
    painlessmesh::ack::deliveryCallback_t callback
);

void handleGatewayMessage(uint32_t from, JsonDocument &doc);

bool isGatewayAvailable();

uint32_t getGatewayNodeId();

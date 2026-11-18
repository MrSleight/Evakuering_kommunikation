
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

#include "GatewayManager.h"
#include "SerialManager.h"
#include "mesh/MeshManager.h"
#include "Config.h"

namespace {
    uint32_t lastGatewayAnnounce = 0;
    uint32_t lastGatewaySeen = 0;

    uint32_t gatewayNodeId = 0;

    bool gatewayAvailable = false;
    bool previousHostState = false;

    void sendGatewayData(const String &payload) {
        JsonDocument output;

        output["event"] = "server_data";
        output["from"] = mesh.getNodeId();
        output["payload"] = payload;

        sendSerialJson(output);
    }
}

void beginGatewayManager() {
    lastGatewayAnnounce = 0;
    lastGatewaySeen = 0;
}

void announceGateway(bool active) {
    JsonDocument doc;

    doc["type"] = "gateway_announce";
    doc["nodeId"] = mesh.getNodeId();
    doc["active"] = active;

    String packet;
    serializeJson(doc, packet);

    mesh.sendBroadcast(packet);

    Serial.printf("[GATEWAY] Announcement: %s\n",
                  active ? "ACTIVE" : "INACTIVE");
}

bool sendToServer(const String &payload) {
    JsonDocument doc;

    doc["type"] = "server_data";
    doc["from"] = mesh.getNodeId();
    doc["payload"] = payload;

    String packet;
    serializeJson(doc, packet);

    if (isHostConnected()) {
        sendGatewayData(payload);
        return true;
    }

    if (!gatewayAvailable) {
        return false;
    }

    bool success = mesh.sendSingle(gatewayNodeId, packet);
    return success;
}

bool sendToServerReliable(
    const String &payload,
    painlessmesh::ack::deliveryCallback_t callback
) {
    JsonDocument doc;

    doc["type"] = "server_data";
    doc["from"] = mesh.getNodeId();
    doc["payload"] = payload;

    String packet;
    serializeJson(doc, packet);

    // This node is the USB-connected gateway
    if (isHostConnected()) {
        sendGatewayData(payload);

        // Confirms handoff to the local Serial output,
        // not receipt by the Python application.
        if (callback) {
            callback(mesh.getNodeId(), true, 0);
        }
        return true;
    }

    // No gateway available
    if (!gatewayAvailable) {
        return false;
    }

    // Send with PainlessMesh delivery acknowledgement
    bool success = mesh.sendSingle(
        gatewayNodeId,
        packet,
        callback
    );

    return success;
}

void handleGatewayMessage(uint32_t from, JsonDocument &doc) {
    const char* type = doc["type"] | "";

    if (strcmp(type, "gateway_announce") == 0) {
        uint32_t announcedNode = doc["nodeId"] | 0UL;
        bool active = doc["active"] | false;

        if (active) {
            gatewayNodeId = announcedNode;
            gatewayAvailable = true;
            lastGatewaySeen = millis();

            Serial.printf("[GATEWAY DISCOVERED] Node: %u\n",
                          gatewayNodeId);
        }
        else if (announcedNode == gatewayNodeId) {
            gatewayAvailable = false;
            gatewayNodeId = 0;

            Serial.println("[GATEWAY LOST]");
        }

        return;
    }

    if (strcmp(type, "server_data") == 0 && isHostConnected()) {
        JsonDocument output;

        output["event"] = "server_data";
        output["from"] = doc["from"] | from;
        output["payload"] = doc["payload"] | "";

        sendSerialJson(output);

        Serial.printf("[SERVER FORWARD] From node %u\n", from);
    }
}

void updateGatewayManager() {
    uint32_t now = millis();

    if (isHostConnected() &&
        now - lastGatewayAnnounce >= GATEWAY_ANNOUNCE_MS) {

        lastGatewayAnnounce = now;
        announceGateway(true);
    }

    if (previousHostState && !isHostConnected()) {
        announceGateway(false);
    }

    previousHostState = isHostConnected();

    if (gatewayAvailable &&
        now - lastGatewaySeen > GATEWAY_TIMEOUT) {

        gatewayAvailable = false;
        gatewayNodeId = 0;

        Serial.println("[GATEWAY] Announcement timeout");
    }
}

bool isGatewayAvailable() {
    return gatewayAvailable;
}

uint32_t getGatewayNodeId() {
    return gatewayNodeId;
}

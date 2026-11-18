
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

#include "mesh/MeshManager.h"
#include "diagnostics/NetworkDiagnostics.h"
#include "gateway/SerialManager.h"
#include "gateway/GatewayManager.h"
#include "messaging/MessageManager.h"
#include "Config.h"

// MESSAGE CALLBACK

void receivedCallback(uint32_t from, String &msg) {
    JsonDocument doc;

    if (deserializeJson(doc, msg)) {
        Serial.printf("[RECEIVE ERROR] Node: %u\n", from);
        return;
    }

    const char* type = doc["type"] | "";

    if (strcmp(type, "gateway_announce") == 0 ||
        strcmp(type, "server_data") == 0) {
        handleGatewayMessage(from, doc);
    }
}

// CONNECTION CALLBACKS

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("[NODE CONNECTED] %u\n", nodeId);
}

void changedConnectionCallback() {
    Serial.println("[MESH] Topology changed");
}
    //  callback time sync

void nodeTimeAdjustedCallback(int32_t offset) {
}

// SETUP

void setup() {
    Serial.begin(115200);

    updateSensorA(true);
    updateSensorB(42);

    beginMessageManager();
    beginSerialManager();

    delay(1000);

    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

    mesh.setDebugMsgTypes(0);

    beginMesh();
    beginGatewayManager();

    Serial.printf("[MESH READY] Node ID: %u\n", mesh.getNodeId());
}

// MAIN LOOP

void loop() {
    updateMesh();
    processSerialInput();
    updateSerialConnection();

    updateGatewayManager();
    updateMessageManager();
}

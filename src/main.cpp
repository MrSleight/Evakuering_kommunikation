
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>
#include "mesh/MeshManager.h"
#include "diagnostics/NetworkDiagnostics.h"
#include "gateway/SerialManager.h"
#include "gateway/GatewayManager.h"
// ==========================================
// MESH CONFIGURATION
// ==========================================

#include "Config.h"

// ==========================================
// GLOBAL OBJECTS
// ==========================================
uint32_t lastBroadcast = 0;
uint32_t lastStatus = 0;
uint32_t lastServerTelemetry = 0;

// ==========================================
// MESSAGE CALLBACK
// ==========================================

void receivedCallback(uint32_t from, String &msg) {

    JsonDocument doc;

    DeserializationError error = deserializeJson(doc, msg);

    if (error) {
        Serial.printf("[RECEIVED] From %u: %s\n",
                      from, msg.c_str());
        return;
    }

    const char* type = doc["type"] | "";

    if (strcmp(type, "gateway_announce") == 0 ||
        strcmp(type, "server_data") == 0) {

        handleGatewayMessage(from, doc);
        return;
    }

    Serial.println();
    Serial.println("[RECEIVED MESSAGE]");
    Serial.printf("Sender: %u\n", from);
    Serial.printf("Message: %s\n", msg.c_str());
    Serial.println();
}
// ==========================================
// CONNECTION CALLBACKS
// ==========================================

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("[NEW NODE CONNECTED] Node ID: %u\n", nodeId);
    printNetworkStatus(
    mesh,
    isGatewayAvailable(),
    getGatewayNodeId()
    );}

void changedConnectionCallback() {
    Serial.println("[MESH TOPOLOGY CHANGED]");
    printNetworkStatus(
    mesh,
    isGatewayAvailable(),
    getGatewayNodeId()
    );}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("[TIME SYNC] Offset: %ld us\n", (long)offset);
}

// ==========================================
// SETUP
// ==========================================

void setup() {
    beginSerialManager();
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("       ESP32 MESH NETWORK");
    Serial.println("=================================");

    mesh.setDebugMsgTypes(
        ERROR |
        STARTUP |
        CONNECTION |
        COMMUNICATION
    );

    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

    beginMesh();
    beginGatewayManager();

    Serial.printf("[MESH STARTED] Node ID: %u\n",
                  mesh.getNodeId());
}

// ==========================================
// MAIN LOOP
// ==========================================


void loop() {

    updateMesh();
    processSerialInput();
    updateSerialConnection();

    uint32_t now = millis();

    updateGatewayManager();

    // Mesh broadcast
    if (now - lastBroadcast >= BROADCAST_INTERVAL) {

        lastBroadcast = now;

        String message = "Hello from node ";
        message += String(mesh.getNodeId());

        mesh.sendBroadcast(message);

        Serial.printf("[MESH BROADCAST] %s\n",
                      message.c_str());
    }

    // Periodic telemetry to Python
    if (now - lastServerTelemetry >= SERVER_TELEMETRY_INTERVAL) {

        lastServerTelemetry = now;

        String payload = "Telemetry from node ";
        payload += String(mesh.getNodeId());

        sendToServer(payload);
    }

    // Network diagnostics
    if (now - lastStatus >= STATUS_INTERVAL) {

        lastStatus = now;

        printNetworkStatus(
            mesh,
            isGatewayAvailable(),
            getGatewayNodeId()
        );
    }
}


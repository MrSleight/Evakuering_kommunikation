
#include <Arduino.h>
#include "painlessMesh.h"

// ==========================================
// MESH CONFIGURATION
// ==========================================

#define MESH_PREFIX     "ESP32_MESH"
#define MESH_PASSWORD   "mesh_password_123"
#define MESH_PORT       5555
#define MESH_CHANNEL    6

// ==========================================
// GLOBAL OBJECTS
// ==========================================

Scheduler userScheduler;
painlessMesh mesh;

uint32_t lastBroadcast = 0;
uint32_t lastStatus = 0;

const uint32_t BROADCAST_INTERVAL = 5000;
const uint32_t STATUS_INTERVAL = 15000;

// ==========================================
// NETWORK DIAGNOSTICS
// ==========================================

void printNetworkStatus() {

    auto nodes = mesh.getNodeList();

    Serial.println();
    Serial.println("========== MESH STATUS ==========");

    Serial.printf("Own Node ID: %u\n", mesh.getNodeId());

    Serial.printf("Connected nodes: %d\n",
                  (int)nodes.size());

    for (uint32_t nodeId : nodes) {
        Serial.printf("  Node ID: %u\n", nodeId);
    }

    Serial.println();
    Serial.println("Network topology:");

    Serial.println(mesh.subConnectionJson());

    Serial.println("=================================");
    Serial.println();
}

// ==========================================
// MESSAGE CALLBACK
// ==========================================

void receivedCallback(uint32_t from, String &msg) {

    Serial.println();
    Serial.println("[RECEIVED MESSAGE]");

    Serial.printf("Sender: %u\n", from);
    Serial.printf("Message: %s\n", msg.c_str());

    Serial.println();
}

// ==========================================
// CONNECTION CALLBACK
// ==========================================

void newConnectionCallback(uint32_t nodeId) {

    Serial.println();
    Serial.println("[NEW NODE CONNECTED]");

    Serial.printf("Node ID: %u\n", nodeId);

    printNetworkStatus();
}

// ==========================================
// TOPOLOGY CALLBACK
// ==========================================

void changedConnectionCallback() {

    Serial.println();
    Serial.println("[MESH TOPOLOGY CHANGED]");

    printNetworkStatus();
}

// ==========================================
// TIME SYNCHRONIZATION CALLBACK
// ==========================================

void nodeTimeAdjustedCallback(int32_t offset) {

    Serial.printf(
        "[TIME SYNC] Offset: %ld us\n",
        (long)offset
    );
}

// ==========================================
// SETUP
// ==========================================

void setup() {

    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("=================================");
    Serial.println("       ESP32 MESH NETWORK");
    Serial.println("=================================");

    // Enable diagnostic output
    mesh.setDebugMsgTypes(
        ERROR |
        STARTUP |
        CONNECTION |
        COMMUNICATION
    );

    // Register callbacks
    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);
    mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);

    // Initialize mesh
    mesh.init(
        MESH_PREFIX,
        MESH_PASSWORD,
        &userScheduler,
        MESH_PORT,
        WIFI_AP_STA,
        MESH_CHANNEL
    );

    Serial.println();
    Serial.println("[MESH STARTED]");

    Serial.printf(
        "Own Node ID: %u\n",
        mesh.getNodeId()
    );

    Serial.println("Waiting for other nodes...");
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {

    // Required mesh maintenance
    mesh.update();

    uint32_t now = millis();

    // Broadcast message
    if (now - lastBroadcast >= BROADCAST_INTERVAL) {

        lastBroadcast = now;

        String message = "Hello from node ";
        message += String(mesh.getNodeId());

        bool success = mesh.sendBroadcast(message);

        if (success) {
            Serial.printf("[SENT] %s\n", message.c_str());
        } else {
            Serial.println("[SEND FAILED]");
        }
    }

    // Periodic network status
    if (now - lastStatus >= STATUS_INTERVAL) {

        lastStatus = now;

        printNetworkStatus();
    }
}

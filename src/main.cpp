
#include <Arduino.h>
#include <ArduinoJson.h>
#include "painlessMesh.h"

// ==========================================
// MESH CONFIGURATION
// ==========================================

#define MESH_PREFIX     "ESP32_MESH"
#define MESH_PASSWORD   "mesh_password_123"
#define MESH_PORT       5555
#define MESH_CHANNEL    6

#define HOST_TIMEOUT          10000
#define GATEWAY_ANNOUNCE_MS    3000
#define GATEWAY_TIMEOUT       10000

// ==========================================
// GLOBAL OBJECTS
// ==========================================

Scheduler userScheduler;
painlessMesh mesh;

uint32_t lastBroadcast = 0;
uint32_t lastStatus = 0;
uint32_t lastGatewayAnnounce = 0;
uint32_t lastHostSeen = 0;
uint32_t lastServerTelemetry = 0;

const uint32_t BROADCAST_INTERVAL = 5000;
const uint32_t STATUS_INTERVAL = 15000;
const uint32_t SERVER_TELEMETRY_INTERVAL = 10000;

bool hostConnected = false;
bool previousHostState = false;

uint32_t gatewayNodeId = 0;
uint32_t lastGatewaySeen = 0;
bool gatewayAvailable = false;

char serialBuffer[512];
size_t serialBufferIndex = 0;

// ==========================================
// SERIAL JSON OUTPUT
// ==========================================

void sendSerialJson(JsonDocument &doc) {
    serializeJson(doc, Serial);
    Serial.println();
}

// ==========================================
// NETWORK DIAGNOSTICS
// ==========================================

void printNetworkStatus() {
    auto nodes = mesh.getNodeList();

    Serial.println();
    Serial.println("========== MESH STATUS ==========");

    Serial.printf("Own Node ID: %u\n", mesh.getNodeId());
    Serial.printf("Connected nodes: %d\n", (int)nodes.size());

    for (uint32_t nodeId : nodes) {
        Serial.printf("  Node ID: %u\n", nodeId);
    }

    Serial.println("Network topology:");
    Serial.println(mesh.subConnectionJson());

    Serial.printf("Gateway available: %s\n",
                  gatewayAvailable ? "YES" : "NO");

    if (gatewayAvailable) {
        Serial.printf("Gateway Node ID: %u\n", gatewayNodeId);
    }

    Serial.println("=================================");
}

// ==========================================
// GATEWAY ANNOUNCEMENT
// ==========================================

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

// ==========================================
// SEND DATA TO PYTHON SERVER
// ==========================================

bool sendToServer(const String &payload) {

    JsonDocument doc;

    doc["type"] = "server_data";
    doc["from"] = mesh.getNodeId();
    doc["payload"] = payload;

    String packet;
    serializeJson(doc, packet);

    // This node is the USB gateway.
    if (hostConnected) {
        JsonDocument output;

        output["event"] = "server_data";
        output["from"] = mesh.getNodeId();
        output["payload"] = payload;

        sendSerialJson(output);

        return true;
    }

    // No known gateway.
    if (!gatewayAvailable) {
        Serial.println("[SERVER] No gateway available");
        return false;
    }

    // Route through PainlessMesh to the gateway.
    bool success = mesh.sendSingle(gatewayNodeId, packet);

    Serial.printf("[SERVER] Routed to gateway %u: %s\n",
                  gatewayNodeId,
                  success ? "QUEUED" : "FAILED");

    return success;
}

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

    // Gateway discovery.
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

    // Data intended for the Python server.
    if (strcmp(type, "server_data") == 0 && hostConnected) {

        JsonDocument output;

        output["event"] = "server_data";
        output["from"] = doc["from"] | from;
        output["payload"] = doc["payload"] | "";

        sendSerialJson(output);

        Serial.printf("[SERVER FORWARD] From node %u\n", from);

        return;
    }

    // Normal mesh message.
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
    printNetworkStatus();
}

void changedConnectionCallback() {
    Serial.println("[MESH TOPOLOGY CHANGED]");
    printNetworkStatus();
}

void nodeTimeAdjustedCallback(int32_t offset) {
    Serial.printf("[TIME SYNC] Offset: %ld us\n", (long)offset);
}

// ==========================================
// USB INPUT
// ==========================================

void processSerialCommand(const char* line) {

    JsonDocument doc;

    if (deserializeJson(doc, line)) {
        return;
    }

    const char* cmd = doc["cmd"] | "";

    // Python sends only a keepalive.
    if (strcmp(cmd, "hello") == 0) {
        lastHostSeen = millis();

        if (!hostConnected) {
            hostConnected = true;
            Serial.println("[USB] Python connected");
        }
    }
}

void processSerialInput() {

    while (Serial.available()) {

        char c = Serial.read();

        if (c == '\n') {

            serialBuffer[serialBufferIndex] = '\0';

            if (serialBufferIndex > 0) {
                processSerialCommand(serialBuffer);
            }

            serialBufferIndex = 0;
        }
        else if (c != '\r') {

            if (serialBufferIndex < sizeof(serialBuffer) - 1) {
                serialBuffer[serialBufferIndex++] = c;
            }
            else {
                serialBufferIndex = 0;
                Serial.println("[USB] Input overflow");
            }
        }
    }
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

    mesh.init(
        MESH_PREFIX,
        MESH_PASSWORD,
        &userScheduler,
        MESH_PORT,
        WIFI_AP_STA,
        MESH_CHANNEL
    );

    Serial.printf("[MESH STARTED] Node ID: %u\n",
                  mesh.getNodeId());
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {

    mesh.update();
    processSerialInput();

    uint32_t now = millis();

    // Detect Python connection loss.
    if (hostConnected &&
        now - lastHostSeen > HOST_TIMEOUT) {

        hostConnected = false;

        Serial.println("[USB] Python disconnected");
    }

    // Announce gateway state periodically.
    if (hostConnected &&
        now - lastGatewayAnnounce >= GATEWAY_ANNOUNCE_MS) {

        lastGatewayAnnounce = now;
        announceGateway(true);
    }

    // Announce gateway shutdown once.
    if (previousHostState && !hostConnected) {
        announceGateway(false);
    }

    previousHostState = hostConnected;

    // Detect lost gateway announcements.
    if (gatewayAvailable &&
        now - lastGatewaySeen > GATEWAY_TIMEOUT) {

        gatewayAvailable = false;
        gatewayNodeId = 0;

        Serial.println("[GATEWAY] Announcement timeout");
    }

    // Existing mesh broadcast.
    if (now - lastBroadcast >= BROADCAST_INTERVAL) {

        lastBroadcast = now;

        String message = "Hello from node ";
        message += String(mesh.getNodeId());

        mesh.sendBroadcast(message);

        Serial.printf("[MESH BROADCAST] %s\n",
                      message.c_str());
    }

    // Example: periodic data sent to Python.
    if (now - lastServerTelemetry >= SERVER_TELEMETRY_INTERVAL) {

        lastServerTelemetry = now;

        String payload = "Telemetry from node ";
        payload += String(mesh.getNodeId());

        sendToServer(payload);
    }

    // Existing network diagnostics.
    if (now - lastStatus >= STATUS_INTERVAL) {

        lastStatus = now;
        printNetworkStatus();
    }
}

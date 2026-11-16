
#include <Arduino.h>
#include <ArduinoJson.h>
#include "painlessMesh.h"

// =====================================================
// MESH CONFIGURATION - IDENTICAL ON ALL NODES
// =====================================================

#define MESH_PREFIX       "ESP32_MESH"
#define MESH_PASSWORD     "mesh_password_123"
#define MESH_PORT         5555
#define MESH_CHANNEL      6

#define TELEMETRY_INTERVAL 5000
#define HOST_TIMEOUT       10000

// =====================================================
// GLOBAL OBJECTS
// =====================================================

Scheduler userScheduler;
painlessMesh mesh;

uint32_t messageCounter = 0;

uint32_t lastTelemetry = 0;
uint32_t lastHostSeen = 0;

bool hostConnected = false;

// USB input buffer
char serialBuffer[512];
size_t serialBufferIndex = 0;

// =====================================================
// JSON OUTPUT TO PYTHON
// =====================================================

void sendSerialJson(JsonDocument &doc) {
    serializeJson(doc, Serial);
    Serial.println();
}

void sendSerialEvent(const char* eventName) {
    JsonDocument doc;

    doc["event"] = eventName;
    doc["nodeId"] = mesh.getNodeId();

    sendSerialJson(doc);
}

// =====================================================
// CREATE MESH PACKET
// =====================================================

String createPacket(
    const char* type,
    uint32_t destination,
    const String& payload
) {
    JsonDocument doc;

    doc["v"] = 1;
    doc["type"] = type;
    doc["id"] = ++messageCounter;
    doc["from"] = mesh.getNodeId();
    doc["to"] = destination;
    doc["payload"] = payload;

    String packet;
    serializeJson(doc, packet);

    return packet;
}

// =====================================================
// SEND APPLICATION ACKNOWLEDGEMENT
// =====================================================

void sendAcknowledgement(
    uint32_t destination,
    uint32_t originalMessageId
) {
    JsonDocument doc;

    doc["v"] = 1;
    doc["type"] = "ack";
    doc["id"] = ++messageCounter;
    doc["from"] = mesh.getNodeId();
    doc["to"] = destination;
    doc["replyTo"] = originalMessageId;
    doc["payload"] = "received";

    String packet;
    serializeJson(doc, packet);

    bool success = mesh.sendSingle(destination, packet);

    Serial.printf(
        "[ACK] To %u: %s\n",
        destination,
        success ? "queued" : "failed"
    );
}

// =====================================================
// NETWORK STATUS
// =====================================================

void printNetworkStatus() {
    auto nodes = mesh.getNodeList();

    JsonDocument doc;

    doc["event"] = "nodes";
    doc["nodeId"] = mesh.getNodeId();
    doc["count"] = nodes.size();

    JsonArray array = doc["nodes"].to<JsonArray>();

    for (uint32_t nodeId : nodes) {
        array.add(nodeId);
    }

    doc["topology"] = mesh.subConnectionJson();

    Serial.println("[MESH STATUS]");
    sendSerialJson(doc);
}

// =====================================================
// MESH RECEIVE CALLBACK
// =====================================================

void receivedCallback(uint32_t from, String &msg) {

    Serial.printf(
        "[MESH RX] From %u: %s\n",
        from,
        msg.c_str()
    );

    JsonDocument packet;

    DeserializationError error = deserializeJson(packet, msg);

    if (error) {
        Serial.println("[ERROR] Invalid mesh JSON");
        return;
    }

    const char* type = packet["type"] | "";
    uint32_t destination = packet["to"] | 0UL;
    uint32_t messageId = packet["id"] | 0UL;

    // Forward received mesh packet to Python
    // if this node has an active USB host.

    if (hostConnected) {
        JsonDocument output;

        output["event"] = "mesh_rx";
        output["transportFrom"] = from;
        output["message"] = msg;

        sendSerialJson(output);
    }

    // Handle directed messages
    if (
        strcmp(type, "message") == 0 &&
        destination == mesh.getNodeId()
    ) {
        Serial.printf(
            "[MESSAGE] Intended for this node. ID: %u\n",
            messageId
        );

        // Send application-level reply
        sendAcknowledgement(from, messageId);
    }

    // Handle acknowledgement
    else if (strcmp(type, "ack") == 0) {

        uint32_t replyTo = packet["replyTo"] | 0UL;

        Serial.printf(
            "[ACK RECEIVED] From %u, message ID: %u\n",
            from,
            replyTo
        );
    }
}

// =====================================================
// CONNECTION CALLBACKS
// =====================================================

void newConnectionCallback(uint32_t nodeId) {

    Serial.printf(
        "[CONNECTED] New node: %u\n",
        nodeId
    );

    printNetworkStatus();
}

void changedConnectionCallback() {

    Serial.println("[MESH] Topology changed");

    printNetworkStatus();
}

// =====================================================
// SEND MESSAGE TO SPECIFIC NODE
// =====================================================

void sendToNode(uint32_t destination, String payload) {

    if (destination == 0) {
        Serial.println("[ERROR] Invalid destination");
        return;
    }

    if (!mesh.isConnected(destination)) {
        Serial.printf(
            "[WARNING] Node %u is not currently known as connected\n",
            destination
        );
    }

    String packet = createPacket(
        "message",
        destination,
        payload
    );

    bool success = mesh.sendSingle(destination, packet);

    Serial.printf(
        "[UNICAST] To %u: %s\n",
        destination,
        success ? "queued" : "failed"
    );

    if (hostConnected) {
        JsonDocument output;

        output["event"] = "send";
        output["to"] = destination;
        output["queued"] = success;

        sendSerialJson(output);
    }
}

// =====================================================
// USB COMMAND HANDLER
// =====================================================

void processSerialCommand(const char* line) {

    JsonDocument command;

    DeserializationError error = deserializeJson(command, line);

    if (error) {
        Serial.println("[USB] Invalid JSON command");
        return;
    }

    const char* cmd = command["cmd"] | "";

    // Python host handshake
    if (strcmp(cmd, "hello") == 0) {

        lastHostSeen = millis();

        if (!hostConnected) {
            hostConnected = true;

            Serial.println("[USB] Python host connected");

            sendSerialEvent("ready");
        }

        return;
    }

    // Refresh host connection
    lastHostSeen = millis();

    // Request node list
    if (strcmp(cmd, "nodes") == 0) {

        printNetworkStatus();
        return;
    }

    // Send a directed mesh message
    if (strcmp(cmd, "send") == 0) {

        uint32_t destination = command["to"] | 0UL;
        String payload = command["payload"] | "";

        sendToNode(destination, payload);
        return;
    }

    Serial.println("[USB] Unknown command");
}

// =====================================================
// READ USB SERIAL WITHOUT BLOCKING MESH
// =====================================================

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
                Serial.println("[USB] Input buffer overflow");
            }
        }
    }
}

// =====================================================
// PERIODIC TELEMETRY
// =====================================================

void sendTelemetry() {

    String payload = "Hello from node ";
    payload += String(mesh.getNodeId());

    String packet = createPacket(
        "telemetry",
        0,
        payload
    );

    bool success = mesh.sendBroadcast(packet);

    Serial.printf(
        "[TELEMETRY] %s\n",
        success ? "broadcast queued" : "failed"
    );
}

// =====================================================
// SETUP
// =====================================================

void setup() {

    Serial.begin(115200);
    delay(1000);

    Serial.println();
    Serial.println("================================");
    Serial.println("       ESP32 MESH SYSTEM");
    Serial.println("================================");

    mesh.setDebugMsgTypes(
        ERROR |
        STARTUP |
        CONNECTION
    );

    mesh.onReceive(&receivedCallback);
    mesh.onNewConnection(&newConnectionCallback);
    mesh.onChangedConnections(&changedConnectionCallback);

    mesh.init(
        MESH_PREFIX,
        MESH_PASSWORD,
        &userScheduler,
        MESH_PORT,
        WIFI_AP_STA,
        MESH_CHANNEL
    );

    Serial.printf(
        "[STARTED] Node ID: %u\n",
        mesh.getNodeId()
    );

    Serial.println("[USB] Waiting for Python host");
}

// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

    // Maintain mesh
    mesh.update();

    // Process USB commands
    processSerialInput();

    uint32_t now = millis();

    // Detect disconnected Python host
    if (
        hostConnected &&
        now - lastHostSeen > HOST_TIMEOUT
    ) {
        hostConnected = false;
        Serial.println("[USB] Python host disconnected");
    }

    // Broadcast telemetry every 5 seconds
    if (now - lastTelemetry >= TELEMETRY_INTERVAL) {

        lastTelemetry = now;

        sendTelemetry();
    }
}

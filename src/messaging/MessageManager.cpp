
#include "MessageManager.h"
#include "../mesh/MeshManager.h"
#include "../gateway/GatewayManager.h"

#include <ArduinoJson.h>

// Configuration
static constexpr uint8_t MAX_BLOCKAGE_RECORDS = 30;
static constexpr uint32_t BLOCKAGE_BROADCAST_INTERVAL = 10000;

// Sensor state
static bool sensorAValue = false;
static bool sensorAInitialized = false;
static bool sensorAPending = false;
static bool sensorAInFlight = false;
static uint32_t sensorARevision = 0;

static int sensorBValue = 0;
static bool sensorBInitialized = false;
static bool sensorBPending = false;
static bool sensorBInFlight = false;
static uint32_t sensorBRevision = 0;

// Blockage state
struct BlockageRecord {
    int id;
    bool blocked;
    bool used;
};

static BlockageRecord blockageBuffer[MAX_BLOCKAGE_RECORDS] = {};
static bool blockageDirty = false;
static uint32_t lastBlockageBroadcast = 0;

// Initialise
void beginMessageManager() {
    Serial.println("[MESSAGING] Ready");
}

// Send SENSOR_A
static void sendSensorA() {
    const uint32_t revision = sensorARevision;

    JsonDocument doc;
    doc["type"] = "SENSOR_A";
    doc["value"] = sensorAValue;

    String payload;
    serializeJson(doc, payload);

    sensorAInFlight = true;

    bool queued = sendToServerReliable(
        payload,
        [revision](uint32_t, bool delivered, uint32_t) {
            sensorAInFlight = false;

            if (delivered && sensorARevision == revision) {
                sensorAPending = false;
            }
        }
    );

    if (!queued) {
        sensorAInFlight = false;
    }
}

// Send SENSOR_B
static void sendSensorB() {
    const uint32_t revision = sensorBRevision;

    JsonDocument doc;
    doc["type"] = "SENSOR_B";
    doc["value"] = sensorBValue;

    String payload;
    serializeJson(doc, payload);

    sensorBInFlight = true;

    bool queued = sendToServerReliable(
        payload,
        [revision](uint32_t, bool delivered, uint32_t) {
            sensorBInFlight = false;

            if (delivered && sensorBRevision == revision) {
                sensorBPending = false;
            }
        }
    );

    if (!queued) {
        sensorBInFlight = false;
    }
}

// Broadcast complete blockage state
static void broadcastBlockageState() {
    JsonDocument doc;

    doc["type"] = "BLOCKAGE_STATE";
    doc["nodeId"] = mesh.getNodeId();

    JsonArray records = doc["records"].to<JsonArray>();

    for (uint8_t i = 0; i < MAX_BLOCKAGE_RECORDS; i++) {
        if (!blockageBuffer[i].used) {
            continue;
        }

        JsonObject record = records.add<JsonObject>();
        record["id"] = blockageBuffer[i].id;
        record["blocked"] = blockageBuffer[i].blocked;
    }

    String payload;
    serializeJson(doc, payload);

    mesh.sendBroadcast(payload);

    blockageDirty = false;
    lastBlockageBroadcast = millis();
}

// Process messaging
void updateMessageManager() {
    if (sensorAPending && !sensorAInFlight) {
        sendSensorA();
    }

    if (sensorBPending && !sensorBInFlight) {
        sendSensorB();
    }

    uint32_t now = millis();

    if (blockageDirty ||
        now - lastBlockageBroadcast >= BLOCKAGE_BROADCAST_INTERVAL) {
        broadcastBlockageState();
    }
}

// Update SENSOR_A
void updateSensorA(bool value) {
    if (!sensorAInitialized || sensorAValue != value) {
        sensorAInitialized = true;
        sensorAValue = value;
        sensorAPending = true;
        sensorARevision++;
    }
}

// Update SENSOR_B
void updateSensorB(int value) {
    if (!sensorBInitialized || sensorBValue != value) {
        sensorBInitialized = true;
        sensorBValue = value;
        sensorBPending = true;
        sensorBRevision++;
    }
}

// Update local blockage state
void updateBlockage(int id, bool blocked) {
    for (uint8_t i = 0; i < MAX_BLOCKAGE_RECORDS; i++) {
        if (blockageBuffer[i].used &&
            blockageBuffer[i].id == id) {

            if (blockageBuffer[i].blocked != blocked) {
                blockageBuffer[i].blocked = blocked;
                blockageDirty = true;
            }

            return;
        }
    }

    for (uint8_t i = 0; i < MAX_BLOCKAGE_RECORDS; i++) {
        if (!blockageBuffer[i].used) {
            blockageBuffer[i].id = id;
            blockageBuffer[i].blocked = blocked;
            blockageBuffer[i].used = true;
            blockageDirty = true;
            return;
        }
    }

    Serial.printf("[ERROR] Blockage buffer full: %d\n", id);
}

// Apply received blockage state without rebroadcasting it
void handleBlockageMessage(JsonDocument &doc) {
    JsonArrayConst records = doc["records"].as<JsonArrayConst>();

    for (JsonObjectConst record : records) {
        if (!record["id"].is<int>() ||
            !record["blocked"].is<bool>()) {
            continue;
        }

        int id = record["id"].as<int>();
        bool blocked = record["blocked"].as<bool>();

        bool found = false;

        for (uint8_t i = 0; i < MAX_BLOCKAGE_RECORDS; i++) {
            if (blockageBuffer[i].used &&
                blockageBuffer[i].id == id) {
                blockageBuffer[i].blocked = blocked;
                found = true;
                break;
            }
        }

        if (!found) {
            for (uint8_t i = 0; i < MAX_BLOCKAGE_RECORDS; i++) {
                if (!blockageBuffer[i].used) {
                    blockageBuffer[i].id = id;
                    blockageBuffer[i].blocked = blocked;
                    blockageBuffer[i].used = true;
                    break;
                }
            }
        }
    }
}

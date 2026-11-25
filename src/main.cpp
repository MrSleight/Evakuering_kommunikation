
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

#include "Display/DisplayManager.h"
#include "mmwave_sensor/mmwave_sensor.h"
#include "mesh/MeshManager.h"
#include "gateway/SerialManager.h"
#include "gateway/GatewayManager.h"
#include "messaging/MessageManager.h"
#include "dijkstra/DijkstraManager.h"
#include "Config.h"

#define DEBOUNCE_MS 1000

uint32_t lastPress = 0;
bool blocked = false;

MmWaveSensor waveSensor(Serial2);

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
    else if (strcmp(type, "BLOCKAGE_STATE") == 0) {
        handleBlockageMessage(doc);
    }
}

// CONNECTION CALLBACKS

void newConnectionCallback(uint32_t nodeId) {
    Serial.printf("[NODE CONNECTED] %u\n", nodeId);
}

void changedConnectionCallback() {
    Serial.println("[MESH] Topology changed");
}

void nodeTimeAdjustedCallback(int32_t offset) {
    (void)offset;
}

// SETUP

void setup() {
    Serial.begin(115200);

    pinMode(GPIO_NUM_32, INPUT_PULLDOWN);



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

    waveSensor.begin();
    displayInitialize();

    Serial.printf("[MESH READY] Node ID: %u\n", mesh.getNodeId());
}

// MAIN LOOP

void loop() {
    updateMesh();

    processSerialInput();
    updateSerialConnection();

    updateGatewayManager();
    updateMessageManager();

    // ROUTE RECALCULATION

    if (consumeBlockageChanged()) {
        RoutingResult result = calculateRoute(mesh.getNodeId());

        if (result.valid) {
            Serial.printf("[ROUTING] Selected exit: %u\n",
                          result.selectedExit);

            Serial.printf("[ROUTING] Total cost: %d\n",
                          result.totalCost);
        }
        else {
            Serial.println("[ROUTING] No valid route");
        }
    }

    // BUTTON HANDLING

    uint32_t now = millis();

    if (now - lastPress >= DEBOUNCE_MS) {
        if (digitalRead(GPIO_NUM_32) == HIGH && !blocked) {

            blocked = true;

            Serial.printf("[BUTTON] Node %u is now BLOCKED\n",
                          mesh.getNodeId());

            updateBlockage(mesh.getNodeId(), true);

            lastPress = now;
        }
    }

    // MM-WAVE SENSOR

    waveSensor.update();

    if (waveSensor.hasValidSignal()) {
        updateSensorA(waveSensor.isHumanPresent());
    }
}

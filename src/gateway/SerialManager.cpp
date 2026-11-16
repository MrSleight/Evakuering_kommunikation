
#include <Arduino.h>
#include <ArduinoJson.h>
#include <cstring>

#include "SerialManager.h"
#include "Config.h"

namespace {
    bool hostConnected = false;
    uint32_t lastHostSeen = 0;

    char serialBuffer[512];
    size_t serialBufferIndex = 0;

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
}

void beginSerialManager() {
    Serial.begin(115200);
}

bool isHostConnected() {
    return hostConnected;
}

void sendSerialJson(JsonDocument &doc) {
    serializeJson(doc, Serial);
    Serial.println();
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

void updateSerialConnection() {
    if (hostConnected &&
        millis() - lastHostSeen > HOST_TIMEOUT) {

        hostConnected = false;
        Serial.println("[USB] Python disconnected");
    }
}

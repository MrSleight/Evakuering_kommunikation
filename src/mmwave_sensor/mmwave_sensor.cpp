#include "mmwave_sensor.h"

MmWaveSensor::MmWaveSensor(HardwareSerial& serialPort)
    : sensorSerial(serialPort) {}

void MmWaveSensor::begin() {
    sensorSerial.begin(115200, SERIAL_8N1, 16, 17);
}

void MmWaveSensor::update() {
    while (sensorSerial.available() > 0) {
        char ch = sensorSerial.read();
        if (ch == '\n') {
            incomingText.trim();
            if (!bufferOverflow && incomingText == "ON") {
                humanPresent = true;
                signalValid = true;
                lastSignalTime = millis();
            } else if (!bufferOverflow && incomingText == "OFF") {
                humanPresent = false;
                signalValid = true;
                lastSignalTime = millis();
            }
            incomingText = "";
            bufferOverflow = false;
        } else {
            if (incomingText.length() < 32) {
                incomingText += ch;
            } else {
                bufferOverflow = true;
            }
        }
    }
}

bool MmWaveSensor::isHumanPresent() const {
    return humanPresent;
}

bool MmWaveSensor::hasValidSignal() const {
    return signalValid && (millis() - lastSignalTime < SIGNAL_TIMEOUT_MS);
}

unsigned long MmWaveSensor::getLastSignalTime() const {
    return lastSignalTime;
}

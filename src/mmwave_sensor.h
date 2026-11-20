#ifndef MMWAVE_SENSOR_H
#define MMWAVE_SENSOR_H

#include <Arduino.h>

class MmWaveSensor {
public:
    MmWaveSensor(HardwareSerial& serialPort);
    void begin();
    void update();
    bool isHumanPresent() const;
    bool hasValidSignal() const;
    unsigned long getLastSignalTime() const;

private:
    HardwareSerial& sensorSerial;
    String incomingText = "";
    bool bufferOverflow = false;
    bool humanPresent = false;
    bool signalValid = false;
    unsigned long lastSignalTime = 0;

    static const unsigned long SIGNAL_TIMEOUT_MS = 5000;
};

#endif

#include "Sensor_4_20mA_RK.h"

SerialLogHandler logHandler;

Sensor_4_20mA sensor;

void setup() {
    sensor
        .withNativeADC()
        .init();
}

void loop() {
    static unsigned long lastReport = 0;
    if (millis() - lastReport >= 2000) {
        lastReport = millis();

        Log.info("mA=%.3f", sensor.readPinValue(A0).mA);
    }
}
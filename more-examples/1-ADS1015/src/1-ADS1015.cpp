#include "Particle.h"


// To include ADS1015 support, be sure to include the ADS1015 library
// header BEFORE including Sensor_4_20mA_RK.h.
#include "SparkFun_ADS1015_Arduino_Library.h"
#include "Sensor_4_20mA_RK.h"

SerialLogHandler logHandler;

const size_t NUM_SENSOR_CONFIG = 2;
SensorConfig sensorConfig[NUM_SENSOR_CONFIG] = {
    { 100, "sen1" },
    { 101, "sen2", 0, 100, false }
};

Sensor_4_20mA sensor;

void setup() {
    sensor
        .withADS1015(100)
        .withConfig(sensorConfig, NUM_SENSOR_CONFIG)
        .init();
}

void loop() {
    static unsigned long lastReport = 0;
    if (millis() - lastReport >= 2000) {
        lastReport = millis();

        for(size_t ii = 0; ii < NUM_SENSOR_CONFIG; ii++) {
            SensorValue value = sensor.readPinValue(sensorConfig[ii].virtualPin);

            Log.info("%s: value=%.3f mA=%.3f adcValue=%d", sensorConfig[ii].name, value.value, value.mA, value.adcValue);
        }
    }

}


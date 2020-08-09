# Sensor_4_20mA_RK

**4-20mA Sensor Library for Particle Devices**

This library can be used in two different ways:

- Using the native ADC built into Particle devices
- With an external ADC, such as an ADS1015 connected by I2C

This library is only the software part; a separate note addresses the hardware issues of sensing the 24V 4-20mA signal. The assumption is that you have some way of getting an analog signal that is proportional to the current. The values encoded in the library assume at 100 ohm low-side sense resistor with a 30 mA current limiter (MAX14626) but can be overridden for other designs.

## API

### Simple Example (examples/1-simple)

```cpp
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
```

This is about as simple as it gets. 

```cpp
Sensor_4_20mA sensor;
```

Create a global variable for the sensor library.

```cpp
void setup() {
    sensor
        .withNativeADC()
        .init();
}
```

Initialize it during setup with support for native ADC (like pin A0).

```cpp
Log.info("mA=%.3f", sensor.readPinValue(A0).mA);
```

Every two seconds, print the mA value.

```cpp
typedef struct {
    int         adcValue;
    float       mA;
    float       value;
} SensorValue;
```

The result from readPinValue() is a SensorValue structure. It contains the raw ADC value in adcValue and mA value. Since we did not set up a configuration object, value will be also be in mA. With a configuration, you can map the mA value into more useful values, like temperature for a 4-20 mA RTD temperature sensor.

### Using an external ADC

There's built-in support for the ADS1015 I2C quad ADC.

```cpp
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

```

In this example there's a config structure, which is discussed below. The other major difference is the initialization:

```cpp
sensor
    .withADS1015(100)
    .withConfig(sensorConfig, NUM_SENSOR_CONFIG)
    .init();
```

Since the ADC1015 ADCs don't have pin numbers (in the analogRead() sense), we assign them virtual pin numbers, which typically start at 100. They don't need to be contiguous. Each ADS1015 has 4 ADCs, so it always uses 4 virtual pins, even if you don't use all 4 ADCs. An 8-channel ADC would use 8. The value 100 is the virtual pin number to start with for that ADC.

You can add multiple I2C ADC easily, as well, by specifying different virtual pin start numbers and I2C addresses:

```cpp
sensor
    .withADS1015(100, 0x48)
    .withADS1015(104, 0x49)
    .withConfig(sensorConfig, NUM_SENSOR_CONFIG)
    .init();
```

You can also use a different Wire interface, such as when using the multi-function pins on the Tracker One M8 connector:

```cpp
sensor
    .withADS1015(100, ADS1015_ADDRESS_GND, Wire3)
    .withConfig(sensorConfig, NUM_SENSOR_CONFIG)
    .init();
```

You can include both native and I2C ADC as well.

```cpp
sensor
    .withNativeADC()
    .withADS1015(100)
    .withConfig(sensorConfig, NUM_SENSOR_CONFIG)
    .init();
```

Note that the order is important! .init() must be last, after all of the .withXXX() methods. 

### Config

The configuration object looks like this:

```cpp
const size_t NUM_SENSOR_CONFIG = 2;
SensorConfig sensorConfig[NUM_SENSOR_CONFIG] = {
    { 100, "sen1" },
    { 101, "sen2", 0, 100, false }
};
```

The SensorConfig structure has these fields:

```cpp
typedef struct {
    int         virtualPin = 0;
    const char *name;
    float       valueLow = 4.0;
    float       value20mA = 20.0;
    bool        valueLowIs4mA = true;
    float       offset = 0.0;
    float       multiplier = 1.0;
} SensorConfig;
```

- virtualPin The pin to read

For native pins, this is in the range of 0-99. Make sure you call withNativeADC() in the Sensor_4_20mA object to enable native ADC support. It's the pin number passed to analogRead().

For external ADC like the ADS1015, you assign a virtual pin for the device, like 100, and it will set aside an appropriate number of additional pins. For example, an ADS1015 has 4 channels so it would be 100, 101, 102, 103 for ports 1-4.
 
 Make sure you don't let virtual pin numbers overlap.
 
- name Name to use when writing the sensor value to JSON
 
 This value is not copied because it's typically string constant.
 
- valueLow The low-side value

You use this to convert the values to something more reasonable. For example, if you have 0-100°C 4-20mA temperature sensor, you'd set the low side value to 0.

If you do not set this, the value will be in mA (default is 4.0). 

- value20mA The high-side (20 mA) value

You use this to convert the values to something more reasonable. For example, if you have 0-100°C 4-20mA temperature sensor, you'd set this to 100.

- valueLowIs4mA Set whether the low value is the 4 mA value of 0 mA value. Default is 4 mA (true).

I got some cheap 4-20mA temperature sensors from Amazon. They're listed as 0-100°C, however it turns out that 0°C corresponds to 0 mA, not 4 mA. Setting this field to false will handle this use case without having to do the math to adjust for this yourself.

- offset Adjustment offset. This is added to the value as a calibration adjustment. 

Default is 0.0 (no adjustment)


- multiplier Adjustment multiplier. This value is multiplied by this to adjust. 

This is done after adding the offset. The default is 1.0 (no adjustment).


### JSON Output

On the Tracker One you can easily output JSON in your location generation callback by using the writeJSON() method. This uses the name field of the config object as the key and the value as the value.

```cpp
void locationGenerationCallback(JSONWriter &writer, LocationPoint &point, const void *context)
{
    sensor.writeJSON(writer);
}
```


## Revision History

### 0.0.1 (2020-08-08)

- Initial version


#ifndef __SENSOR_4_20_MA_RK_H
#define __SENSOR_4_20_MA_RK_H

// Github: https://github.com/rickkas7/Sensor_4_20mA_RK
// License: MIT

#include "Particle.h"

#include <vector>

#ifdef ADS1015_ADDRESS_GND
#define HAS_ADS1015 1
#else
#define HAS_ADS1015 0
#endif

/**
 * @brief Configures a virtual pin
 */
typedef struct {
    /**
     * @brief The pin to read
     * 
     * For native pins, this is in the range of 0-99. Make sure you call withNativeADC() in the
     * Sensor_4_20mA object to enable native ADC support. It's the pin number passed to analogRead().
     * 
     * For external ADC like the ADS1015, you assign a virtual pin for the device, like 100, and
     * it will set aside an appropriate number of additional pins. For example, an ADS1015 has 4
     * channels so it would be 100, 101, 102, 103 for ports 1-4.
     * 
     * Make sure you don't let virtual pin numbers overlap.
     */
    int     virtualPin = 0;

    /**
     * @brief Name to use when writing the sensor value to JSON
     * 
     * This value is not copied because it's typically string constant.
     */
    const char *name;

    /**
     * @brief The low-side value
     * 
     * You use this to convert the values to something more reasonable. For example, if you
     * have 0-100°C 4-20mA temperature sensor, you'd set the low side value to 0.
     * 
     * If you do not set this, the value will be in mA. 
     */
    float   valueLow = 4.0;

    /**
     * @brief The high-side (20 mA) value
     * 
     * You use this to convert the values to something more reasonable. For example, if you
     * have 0-100°C 4-20mA temperature sensor, you'd set this to 100.
     */
    float   value20mA = 20.0;

    /**
     * @brief Set whether the low value is the 4 mA value of 0 mA value. Default is 4 mA (true).
     * 
     * I got some cheap 4-20mA temperature sensors from Amazon. They're listed as 0-100°C, however
     * it turns out that 0°C corresponds to 0 mA, not 4 mA. Setting this field to false will handle
     * this use case without having to do the math to adjust for this yourself.
     */
    bool    valueLowIs4mA = true;

    /**
     * @brief Adjustment offset. This is added to the value as a calibration adjustment. 
     * 
     * Default is 0.0 (no adjustment)
     */
    float   offset = 0.0;

    /**
     * @brief Adjustment multiplier. This value is multiplied by this to adjust. 
     * 
     * This is done after adding the offset. The default is 1.0 (no adjustment).
     */
    float   multiplier = 1.0;
} SensorConfig;

typedef struct {
    /**
     * @brief Raw value from the ADC
     */
    int         adcValue;

    /**
     * @brief Value from the ADC converted to mA
     */
    float       mA;

    /**
     * @brief If there's a value mapping config, converted to that value
     * 
     * If no config, then this is the same as mA
     */
    float          value;
} SensorValue;

class SensorVirtualPinBase {
public:
    /**
     * @brief Construct a virtual pin class. 
     * 
     * This class is abstract so you never instantiate one of these directly, but rather one of 
     * its subclasses.
     * 
     * @param virtualPinStart The pin number to start from, typically starts at 100.
     * 
     * @param numVirtualPins The number of pins this instance supports. For the ADS1015, this is
     * 4, the number of ADCs it contains.
     * 
     * @param adcValue4mA The value of the ADC at 4 mA current.
     * 
     * @param adcValue20mA The value of the ADC at 20 mA current.
     */
    SensorVirtualPinBase(int virtualPinStart, int numVirtualPins, int adcValue4mA, int adcValue20mA);
    virtual ~SensorVirtualPinBase();

    /**
     * @brief Override if you need to do initialization
     * 
     * This is better than in the constructor, as you should avoid doing things at global object
     * construction time.
     */
    virtual bool init();

    /**
     * @brief Returns the raw ADC value
     */
    virtual int readPin(int virtualPin) = 0;

    /**
     * @brief Returns the ADC value in mA, typically in the range of 4.0 to 20.0.
     * 
     * This is calculated in this class based on the value from readPin() and the
     * values for adcValue4mA and adcValue20mA.
     */
    virtual int convert_mA(int adcValue);

    /**
     * @brief Returns true if virtualPinStart <= virtualPin < (virtualPinStart + numVirtualPins)
     */
    bool isInRange(int virtualPin) const;

public:
    int     virtualPinStart;
    int     numVirtualPins;
    int     adcValue4mA;
    int     adcValue20mA;
};

/**
 * @brief Class for using the native (nRF52/STM32) ADC using analogRead()
 * 
 * To enable using the native ADC, use withNativeADC() in the Sensor_4_20mA class.
 */
class SensorVirtualPinNative : public SensorVirtualPinBase {
public:
    SensorVirtualPinNative();
    virtual ~SensorVirtualPinNative();

    virtual int readPin(int virtualPin);
};
 
#if HAS_ADS1015
/**
 * @brief Support for the ADS1015 I2C ADC
 */
class SensorVirtualPinADS1015 : public SensorVirtualPinBase {
public:
    SensorVirtualPinADS1015(int virtualPinStart, uint8_t i2cAddr, TwoWire &wire = Wire) : 
        SensorVirtualPinBase(virtualPinStart, 4, 199, 1004), i2cAddr(i2cAddr), wire(wire) {};
    virtual ~SensorVirtualPinADS1015() {};

    virtual bool init() {
        bool bResult = adc.begin(i2cAddr, wire);
        if (bResult) {
            // Set gain to PGA1: FSR = +/-4.096V
            // This parameter expresses the full-scale range of the ADC scaling. 
            // Do not apply more than VDD + 0.3 V to the analog inputs of the device.
            // (This means 3.3V will be approximately 1652)
            adc.setGain(ADS1015_CONFIG_PGA_1);        
        }
        else {
            Log.error("ADC initialization failed");
        }
        return bResult;
    };

    virtual int readPin(int virtualPin) {
        return adc.getSingleEnded(virtualPin - virtualPinStart);
    };

protected:
    uint8_t i2cAddr;
    TwoWire &wire;
    ADS1015 adc;
};
#endif /* HAS_ADS1015 */


/**
 * @brief Class for managing 4-20mA sensors
 * 
 * You normally instantiate one of these objects as a global variable. From your setup() function call:
 * 
 * - withNativeADC() if you are using the native nFC52/STM32 ADC using analogRead
 * - withADS1015() if you are using sensors connected via an ADC1015 I2C ADC. You can call this multiple
 * times for multiple ADS1015.
 * - withConfig() if you are using sensor value configuration (optional)
 * 
 * - init() AFTER calling the withXXX() methods.
 * 
 * For example:
 * 
 * ```
 * void setup() {
 *    sensor
 *        .withADS1015(100)
 *        .withConfig(sensorConfig, NUM_SENSOR_CONFIG)
 *        .init();
 * }
 * ```
 */
class Sensor_4_20mA {
public:
    /**
     * @brief Construct an object. Typically you have one of these as a global variable.
     */
    Sensor_4_20mA();

    /**
     * @brief Destructor. You seldom delete one of these.
     */
    virtual ~Sensor_4_20mA();

    /**
     * @brief Call init() from the main setup() function 
     * 
     * Be sure to call withNativeADC() and/or withADS1015() before calling init()!
     */
    bool init();

    /**
     * @brief Uses the config to add the key/value pairs to a JSON object
     */
    void writeJSON(JSONWriter &writer);

    /**
     * @brief Read the raw ADC value for a pin or virtual pin

     * @param pin A pin number (like A0), or a virtual pin number if using an external ADC.
     */
    int readPin(int pin);

    /**
     * @brief Read a value for a pin or virtual pin
     * 
     * @param pin A pin number (like A0), or a virtual pin number if using an external ADC.
     * 
     * If a config specifies a value range (for example, 0-100°C), this will return the
     * configured value. If there is no config, the default is to return mA, typically 4.0 - 20.0.
     */
    SensorValue readPinValue(int pin);

#if HAS_ADS1015
    /**
     * @brief Enable support for ADS1015
     * 
     * @param virtualPinStart The virtual pin number to start at. Typically 100.
     * 
     * @param i2cAddr The I2C address of the chip. Typically ADS1015_ADDRESS_GND (0x44).
     * 
     * @param wire The interface, typically Wire, except on the Tracker One/Tracker Carrier Board,
     * where Wire3 is typically used for the shared port on the M8 connector.
     * 
     * This is only available if you include Sparkfun_ADS1015_Arduino_Library.h BEFORE
     * including this header.
     * 
     * #include "Sparkfun_ADS1015_Arduino_Library.h"
     * #include "Sensor_4_20mA_RK.h"
     */
    Sensor_4_20mA &withADS1015(int virtualPinStart, uint8_t i2cAddr = ADS1015_ADDRESS_GND, TwoWire &wire = Wire) {
        virtualPins.push_back(new SensorVirtualPinADS1015(virtualPinStart, i2cAddr, wire));
        return *this;
    }
#endif /* HAS_ADS1015 */

    /**
     * @brief Enable support for the native nRF52/STM32 ADC
     * 
     * This supports analogRead() of built-in ADCs (12-bit, 0-4095, 3.3V max).
     */
    Sensor_4_20mA &withNativeADC();

    /**
     * @brief Specify the configuration for sensors, including the JSON keys and value mapping
     * 
     * @param config A pointer to an array of SensorConfig structs with the configuration data.
     * 
     * @param numConfig The number of entries in the array.
     * 
     * Note that config must remain valid during the scope of this object. Typically the data is
     * a structure in flash and is not copied to RAM.
     * 
     * You can omit the config if you only want to read mA values and do not need to generate
     * JSON directly from this class.
     */
    Sensor_4_20mA &withConfig(const SensorConfig *config, size_t numConfig);

protected:
    std::vector<SensorVirtualPinBase *> virtualPins;
    const SensorConfig *config = NULL;
    size_t numConfig;
};



#endif /* __SENSOR_4_20_MA_RK_H */
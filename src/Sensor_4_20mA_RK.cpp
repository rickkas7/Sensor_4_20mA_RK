#include "Sensor_4_20mA_RK.h"

// Github: https://github.com/rickkas7/Sensor_4_20mA_RK
// License: MIT

#include <math.h> // isnan()

Sensor_4_20mA::Sensor_4_20mA() {

}
Sensor_4_20mA::~Sensor_4_20mA() {
    for(size_t ii = 0; ii < virtualPins.size(); ii++) {
        delete virtualPins[ii];
    }
}

bool Sensor_4_20mA::init() {
    for(size_t ii = 0; ii < virtualPins.size(); ii++) {
        bool bResult = virtualPins[ii]->init();
        if (!bResult) {
            return false;
        }
    }
    return true;
}

void Sensor_4_20mA::writeJSON(JSONWriter &writer) {
    for(size_t jj = 0; jj < numConfig; jj++) {
        SensorValue value = readPinValue(config[jj].virtualPin);
        if (!isnan(value.value)) {
            writer.name(config[jj].name).value(value.value);
        }
    }
}


int Sensor_4_20mA::readPin(int pin) {
    for(size_t ii = 0; ii < virtualPins.size(); ii++) {
        if (virtualPins[ii]->isInRange(pin)) {
            return virtualPins[ii]->readPin(pin);
        }
    }
    return 0;
}

SensorValue Sensor_4_20mA::readPinValue(int pin) {
    SensorValue result;

    result.adcValue = 0;
    result.mA = 0.0;
    result.value = 0.0;

    for(size_t ii = 0; ii < virtualPins.size(); ii++) {
        if (virtualPins[ii]->isInRange(pin)) {
            result.adcValue = virtualPins[ii]->readPin(pin);

            result.mA = result.value = virtualPins[ii]->convert_mA(result.adcValue);

            if (config && !isnan(result.mA)) {
                for(size_t jj = 0; jj < numConfig; jj++) {
                    if (config[jj].virtualPin == pin) {
                        if (config[jj].valueLowIs4mA) {
                            // valueLow is the 4 mA value.
                            result.value = (result.mA - 4.0) * (config[jj].value20mA - config[jj].valueLow) / 16.0 + config[jj].valueLow;
                        }
                        else {
                            // valueLow is the 0 mA value. One of the temperature sensors I got was
                            // a 4-20mA sensor and was rated at 0-100°C. However, 0°C the value at 0 mA, not
                            // the value at 4 mA. This seems suboptimal but that's the way it worked out.
                            result.value = result.mA * (config[jj].value20mA - config[jj].valueLow) / 20.0;
                        }

                        result.value += config[jj].offset;
                        result.value *= config[jj].multiplier;

                        break;
                    }
                }
            }
            break;
        }
    }

    return result;    
}
Sensor_4_20mA &Sensor_4_20mA::withNativeADC() {
    virtualPins.push_back(new SensorVirtualPinNative());
    return *this;
}

Sensor_4_20mA &Sensor_4_20mA::withConfig(const SensorConfig *config, size_t numConfig) {
    this->config = config;
    this->numConfig = numConfig;
    return *this;
}


SensorVirtualPinBase::SensorVirtualPinBase(int virtualPinStart, int numVirtualPins, int adcValue4mA, int adcValue20mA) :
    virtualPinStart(virtualPinStart), numVirtualPins(numVirtualPins), adcValue4mA(adcValue4mA), adcValue20mA(adcValue20mA) {

}
SensorVirtualPinBase::~SensorVirtualPinBase() {

}

bool SensorVirtualPinBase::init() {
    return true;
}

bool SensorVirtualPinBase::isInRange(int virtualPin) const {
    return (virtualPin >= virtualPinStart) && (virtualPin < (virtualPinStart + numVirtualPins));
}

int SensorVirtualPinBase::convert_mA(int adcValue) {
    adcValue -= adcValue4mA;
    int delta = adcValue20mA - adcValue4mA;

    return (float)adcValue * 16.0 / delta + 4.0;
}


SensorVirtualPinNative::SensorVirtualPinNative() : SensorVirtualPinBase(0, 100, 491, 2469) {

}
SensorVirtualPinNative::~SensorVirtualPinNative() {

}

int SensorVirtualPinNative::readPin(int virtualPin) {
    return analogRead(virtualPin);
}

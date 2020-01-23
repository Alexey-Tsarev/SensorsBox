#include <Arduino.h>


class Pinner {
public:
#ifdef ESP8266
    const uint8_t allowedPins[11] = {0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16};
#else
    const uint8_t allowedPins[1] = {0};
#endif
    const uint8_t allowedPinsLength = sizeof(allowedPins) / sizeof(allowedPins[0]);
    const uint8_t ignoredPin = 255; //may not used?!


    bool isAllowedPin(uint8_t pin) {
        for (uint8_t i = 0; i < allowedPinsLength; i++)
            if (pin == allowedPins[i])
                return true;

        return false;
    }


    bool fixPin(uint8_t &pin) {
        if (isAllowedPin(pin))
            return false;
        else {
            pin = ignoredPin;
            return true;
        }
    }


    bool fixPinAndSetStatus(uint8_t &pin, bool &status) {
        bool fixed = fixPin(pin);

        if (fixed)
            status = false;

        return fixed;
    }
};

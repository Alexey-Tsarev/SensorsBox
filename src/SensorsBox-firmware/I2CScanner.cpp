#include <Wire.h>
#include <Arduino.h>


class I2CScanner {
private:
    uint8_t i2cSdaPin = 255, i2cSclPin = 255;
    uint8_t foundDevices, iterateAddress, lastAddress;
    bool endFlag;


public:
    I2CScanner() {
        init();
    }


    // I2C with provided SDA and SCL pins
    I2CScanner(uint8_t sda, uint8_t scl) {
        initI2C(sda, scl);
    }


    void initI2C(uint8_t sda, uint8_t scl) {
        i2cSdaPin = sda;
        i2cSclPin = scl;
        init();
    }


    bool scanI2CAndGetResult(uint8_t address) {
        return scan(address);
    }


    bool scanI2CAndGetResult() {
        bool scanResult = false;

        if (iterateAddress < 127) {
            lastAddress = iterateAddress;
            scanResult = scan(iterateAddress);

            if (scanResult)
                foundDevices++;

            iterateAddress++;
        } else
            endFlag = true;

        return scanResult;
    }


    bool isEnd() {
        return endFlag;
    }


    uint8_t getAddress() {
        return lastAddress;
    }


    void returnAddressStr(char *str, size_t strLen) {
        snprintf(str, strLen, "%02X", lastAddress);
    }


    uint8_t getFoundDevices() {
        return foundDevices;
    }


private:
    void init() {
        foundDevices = 0;
        iterateAddress = 1;
        endFlag = false;

        if ((i2cSdaPin != 255) && (i2cSclPin != 255))
            Wire.begin(i2cSdaPin, i2cSclPin);
    }


    bool scan(uint8_t address) {
        Wire.beginTransmission(address);
        return Wire.endTransmission() == 0;
    }
};

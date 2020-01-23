#include <Arduino.h>
#include <SoftwareSerial.h> // ESP32 only: https://github.com/akshaybaweja/SoftwareSerial
#include <memory>


enum MHZ19_STATUSES {
    MHZ19_NOT_INITIALIZED,
    MHZ19_WRONG_START_BYTE,
    MHZ19_WRONG_COMMAND,
    MHZ19_WRONG_CHECKSUM,
    MHZ19_GOOD
};


class MHZ19 {
public:
    MHZ19() {
    }


    MHZ19(uint8_t rx, uint8_t tx) {
        init(rx, tx);
    }


    void init(uint8_t rx, uint8_t tx) {
        rxPin = rx;
        txPin = tx;
        initSoftwareSerial();
    }


    bool readCo2() {
        if (softwareSerialCreatedFlag) {
            softwareSerial->write(readCo2Command, sizeof(readCo2Command));

            memset(response, 0, sizeof(response));
            softwareSerial->readBytes(response, sizeof(response));

            if (response[0] != 0xFF) {
                mhZ19Status = MHZ19_WRONG_START_BYTE;
                return false;
            }

            if (response[1] != 0x86) {
                mhZ19Status = MHZ19_WRONG_COMMAND;
                return false;
            }

            if (response[8] != calculateCheckSum()) {
                mhZ19Status = MHZ19_WRONG_CHECKSUM;
                return false;
            }

            co2 = word(response[2], response[3]);
            mhZ19Status = MHZ19_GOOD;

            if (co2FixFlag)
                co2FixedFlag = fixCo2();

            return true;
        } else {
            mhZ19Status = MHZ19_NOT_INITIALIZED;
            return false;
        }
    }


    bool fixCo2() {
        if ((co2 <= 128) || (co2 == 5000)) {
            co2 = fixedCo2;
            return true;
        } else
            return false;
    }


    bool getCo2FixedFlag() {
        return co2FixedFlag;
    }


    uint8_t getRxPin() {
        return rxPin;
    }


    uint8_t getTxPin() {
        return txPin;
    }


    uint16_t getCo2() {
        if (mhZ19Status == MHZ19_GOOD)
            return co2;
        else
            return 0;
    }


    MHZ19_STATUSES getReadStatus() {
        return mhZ19Status;
    }


private:
    uint8_t readCo2Command[9] = {0xFF, 0x01, 0x86, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
    uint8_t response[9];
    uint8_t rxPin, txPin;
    uint16_t co2 = 0;
    MHZ19_STATUSES mhZ19Status;
    bool co2FixFlag = true;
    bool co2FixedFlag;
    uint16_t fixedCo2 = 333;

    std::unique_ptr<SoftwareSerial> softwareSerial;
    bool softwareSerialCreatedFlag = false;


    void initSoftwareSerial() {
        softwareSerial.reset();
        softwareSerial.reset(new SoftwareSerial(rxPin, txPin));
        softwareSerial->begin(9600);
        softwareSerial->setTimeout(100);

        if (!softwareSerialCreatedFlag)
            softwareSerialCreatedFlag = true;
    }


    uint8_t calculateCheckSum() {
        uint8_t check_sum = 0;

        for (uint8_t i = 1; i < 8; i++)
            check_sum += response[i];

        return ~check_sum + 1;
    }
};

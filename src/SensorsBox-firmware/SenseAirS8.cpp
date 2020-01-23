/*
http://co2meter.com/products/s8-miniature-co2-sensor-1
http://co2meters.com/Documentation/Datasheets/DS-S8-1.pdf
http://co2meters.com/Documentation/AppNotes/AN168-S8-raspberry-pi-uart.pdf
http://co2meters.com/Documentation/AppNotes/AN162-LP8-sensor-arduino-modbus-uart.pdf
http://rmtplusstoragesenseair.blob.core.windows.net/docs/Dev/publicerat/TDE2067.pdf
 */

#include <Arduino.h>
#include <SoftwareSerial.h> // ESP32 only: https://github.com/akshaybaweja/SoftwareSerial
#include <memory>


enum SENSEAIR_STATUSES {
    SENSEAIR_NOT_INITIALIZED,
    SENSEAIR_WRONG_START_BYTE,
    SENSEAIR_WRONG_COMMAND,
    SENSEAIR_WRONG_CHECKSUM,
    SENSEAIR_RESPONSE_TOO_LONG,
    SENSEAIR_GOOD
};


class SENSEAIRS8 {
public:
    SENSEAIRS8() {
    }


    SENSEAIRS8(uint8_t rx, uint8_t tx) {
        init(rx, tx);
    }


    void init(uint8_t rx, uint8_t tx) {
        rxPin = rx;
        txPin = tx;
        initSoftwareSerial();
    }


    void setCo2FixFlag(bool flag) {
        co2FixFlag = flag;
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


    SENSEAIR_STATUSES getReadStatus() {
        return senseAirStatus;
    }


    uint16_t getCo2() {
        if (senseAirStatus == SENSEAIR_GOOD)
            return co2;
        else
            return 0;
    }


    uint16_t getStatus() {
        return status;
    }


    uint8_t *getTypeId() {
        return typeId;
    }


    uint8_t getTypeIdLen() {
        return sizeof(typeId);
    }


    uint16_t getFwVersion() {
        return fwVersion;
    }


    uint8_t *getSensorId() {
        return sensorId;
    }


    uint8_t getSensorIdLen() {
        return sizeof(sensorId);
    }


    void printCommandsAndCrc() {
        printCommandAndCrc(readCo2Command, sizeof(readCo2Command));
        printCommandAndCrc(readStatusCommand, sizeof(readStatusCommand));
        printCommandAndCrc(readTypeIdHighCommand, sizeof(readTypeIdHighCommand));
        printCommandAndCrc(readTypeIdLowCommand, sizeof(readTypeIdLowCommand));
        printCommandAndCrc(readFwVersionCommand, sizeof(readFwVersionCommand));
        printCommandAndCrc(readSensorIdHighCommand, sizeof(readSensorIdHighCommand));
        printCommandAndCrc(readSensorIdLowCommand, sizeof(readSensorIdLowCommand));
    }


    bool readCo2() {
        if (sendAndReceive(readCo2Command, sizeof(readCo2Command),
                           respCo2, sizeof(respCo2))) {
            co2 = word(respCo2[0], respCo2[1]);

            if (co2FixFlag)
                co2FixedFlag = fixCo2();

            return true;
        } else
            return false;
    }


    bool readStatus() {
        if (sendAndReceive(readStatusCommand, sizeof(readStatusCommand),
                           respStatus, sizeof(respStatus))) {
            status = word(respStatus[0], respStatus[1]);
            return true;
        } else
            return false;
    }


    bool readTypeId() {
        if (!readTypeIdHigh())
            return false;

        if (!readTypeIdLow())
            return false;

        typeId[0] = respTypeIdHigh[1]; // or [0] ?
        typeId[1] = respTypeIdLow[0]; // or [1] ?
        typeId[2] = respTypeIdLow[1]; // or [0] ?

        return true;
    }


    bool readFwVersion() {
        if (sendAndReceive(readFwVersionCommand, sizeof(readFwVersionCommand),
                           respFwVersion, sizeof(respFwVersion))) {
            fwVersion = word(respFwVersion[0], respFwVersion[1]);
            return true;
        } else
            return false;
    }


    bool readSensorId() {
        if (!readSensorIdHigh())
            return false;

        if (!readSensorIdLow())
            return false;

        sensorId[0] = respSensorIdHigh[0];
        sensorId[1] = respSensorIdHigh[1];
        sensorId[2] = respSensorIdLow[0];
        sensorId[3] = respSensorIdLow[1];

        return true;
    }


private:
    // Read CO2
    uint8_t readCo2Command[8] = {0xFE, 0x04, 0x00, 0x03, 0x00, 0x01, 0xD5, 0xC5};
    uint8_t respCo2[2];
    uint16_t co2;

    // Read senseAirStatus
    uint8_t readStatusCommand[8] = {0xFE, 0x04, 0x00, 0x00, 0x00, 0x01, 0x25, 0xC5};
    uint8_t respStatus[2];
    uint8_t status;

    // Read Type ID high
    uint8_t readTypeIdHighCommand[8] = {0xFE, 0x04, 0x00, 0x19, 0x00, 0x01, 0xF4, 0x02};
    uint8_t respTypeIdHigh[2];

    // Read Type ID low
    uint8_t readTypeIdLowCommand[8] = {0xFE, 0x04, 0x00, 0x1A, 0x00, 0x01, 0x04, 0x02};
    uint8_t respTypeIdLow[2];

    uint8_t typeId[3];

    // FW version
    uint8_t readFwVersionCommand[8] = {0xFE, 0x04, 0x00, 0x1C, 0x00, 0x01, 0xE4, 0x03};
    uint8_t respFwVersion[2];
    uint16_t fwVersion;

    // Sensor ID high
    uint8_t readSensorIdHighCommand[8] = {0xFE, 0x04, 0x00, 0x1D, 0x00, 0x01, 0xB5, 0xC3};
    uint8_t respSensorIdHigh[2];

    // Sensor ID low
    uint8_t readSensorIdLowCommand[8] = {0xFE, 0x04, 0x00, 0x1E, 0x00, 0x01, 0x45, 0xC3};
    uint8_t respSensorIdLow[2];

    uint8_t sensorId[4];

    uint8_t response[7];
    uint8_t rxPin, txPin;
    SENSEAIR_STATUSES senseAirStatus;
    bool co2FixFlag = true;
    bool co2FixedFlag;
    uint16_t fixedCo2 = 333;

    std::unique_ptr <SoftwareSerial> softwareSerial;
    bool softwareSerialCreatedFlag = false;


    void initSoftwareSerial() {
        softwareSerial.reset();
        softwareSerial.reset(new SoftwareSerial(rxPin, txPin));
        softwareSerial->begin(9600);
        softwareSerial->setTimeout(181);

        if (!softwareSerialCreatedFlag)
            softwareSerialCreatedFlag = true;
    }


    bool calcModBusCrc(uint8_t *ModbusRtuFrame, size_t ModbusRtuFrameLen, uint16_t &crc) {
        if (ModbusRtuFrameLen >= 3) {
            crc = 0xFFFF;

            for (uint8_t i = 0; i < ModbusRtuFrameLen - 2; i++) {
                crc ^= (uint16_t) ModbusRtuFrame[i];

                for (uint8_t j = 8; j != 0; j--) {
                    if ((crc & 0x0001) != 0) {
                        crc >>= 1;
                        crc ^= 0xA001;
                    } else
                        crc >>= 1;
                }
            }

            return true;
        } else
            return false;
    }


    bool checkModBusCrc(uint8_t *ModbusRtuFrame, size_t ModbusRtuFrameLen) {
        uint16_t crc;
        if (calcModBusCrc(ModbusRtuFrame, ModbusRtuFrameLen, crc)) {
            if ((lowByte(crc) == ModbusRtuFrame[ModbusRtuFrameLen - 2]) &&
                (highByte(crc) == ModbusRtuFrame[ModbusRtuFrameLen - 1]))
                return true;
            else
                return false;
        } else
            return false;
    }


    bool sendAndReceive(uint8_t *command, size_t commandLen, uint8_t *result, size_t resultLen) {
        if (softwareSerialCreatedFlag) {
            softwareSerial->write(command, commandLen);

            memset(response, 0, sizeof(response));
            softwareSerial->readBytes(response, sizeof(response));

            if (command[0] != response[0]) {
                senseAirStatus = SENSEAIR_WRONG_START_BYTE;
                return false;
            }

            if (command[1] != response[1]) {
                senseAirStatus = SENSEAIR_WRONG_COMMAND;
                return false;
            }

            if (!checkModBusCrc(response, sizeof(response))) {
                senseAirStatus = SENSEAIR_WRONG_CHECKSUM;
                return false;
            }

            if (response[2] > resultLen) {
                senseAirStatus = SENSEAIR_RESPONSE_TOO_LONG;
                return false;
            }

            for (uint8_t i = 0; i < response[2]; i++)
                result[i] = response[i + 3];

            senseAirStatus = SENSEAIR_GOOD;
            return true;
        } else {
            senseAirStatus = SENSEAIR_NOT_INITIALIZED;
            return false;
        }
    }


    bool fixCo2() {
        if (co2 == 0) {
            co2 = fixedCo2;
            return true;
        } else
            return false;
    }


    bool readTypeIdHigh() {
        return sendAndReceive(readTypeIdHighCommand, sizeof(readTypeIdHighCommand),
                              respTypeIdHigh, sizeof(respTypeIdHigh));
    }


    bool readTypeIdLow() {
        return sendAndReceive(readTypeIdLowCommand, sizeof(readTypeIdLowCommand),
                              respTypeIdLow, sizeof(respTypeIdLow));
    }


    bool readSensorIdHigh() {
        return sendAndReceive(readSensorIdHighCommand, sizeof(readSensorIdHighCommand),
                              respSensorIdHigh, sizeof(respSensorIdHigh));
    }


    bool readSensorIdLow() {
        return sendAndReceive(readSensorIdLowCommand, sizeof(readSensorIdLowCommand),
                              respSensorIdLow, sizeof(respSensorIdLow));
    }


    void printHex(uint8_t hex) {
        if (hex < 16)
            Serial.print("0");

        Serial.print(hex, HEX);
    }


    void printCommandAndCrc(uint8_t *command, size_t commandLen) {
        for (uint8_t i = 0; i < commandLen; i++) {
            printHex(command[i]);
            Serial.print(" ");
        }

        uint16_t crc;
        calcModBusCrc(command, commandLen, crc);

        uint8_t crcLow = lowByte(crc);
        uint8_t crcHigh = highByte(crc);

        Serial.print("| CRC = ");
        printHex(crcLow);
        Serial.print(" ");
        printHex(crcHigh);

        if ((command[commandLen - 2] == crcLow) && (command[commandLen - 1] == crcHigh))
            Serial.println(" | OK");
        else
            Serial.println(" | ER");
    }
};

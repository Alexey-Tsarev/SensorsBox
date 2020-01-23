#include <Arduino.h>


struct EEPROMConfig {
    char startMarker[6];
    char webUser[32];
    char webUserPassword[32];
    char otaPassword[32];
    char blynkAuthToken[33];
    char blynkHost[100];
    uint16_t blynkPort;
    char blynkSSLFingerprint[60];
    uint16_t blynkSendDataInSeconds;

    uint8_t postDataFlag;
    char postDataURL[128];
    bool postDataSSLFlag;
    char postDataSSLFingerprint[60];
    bool postDataSSLAllowSelfSignedCertFlag;
    uint16_t postDataTimoutMillis;
    uint16_t postDataInSeconds;

    uint16_t deepSleepSeconds;
    uint16_t deepSleepMaxPreExecSeconds;

    // Time
    uint16_t showTimeMillis;
    int8_t timeZoneOffset;
    bool summerTime;
    char ntpHost[32];
    uint16_t ntpReadTimeoutMillis;
    uint16_t ntpSyncEverySeconds;

    // Sensors
    uint16_t showSensorsMillis;

    // OLED
    uint8_t oledType; // 0 - not connected, 1 - I2C, 2 - SPI
    uint8_t oledPar1; // I2C SDA     or SPI CLOCK
    uint8_t oledPar2; // I2C SCL     or SPI DATA
    uint8_t oledPar3; // I2C address or SPI CS
    uint8_t oledPar4; //                SPI DC
    uint8_t oledPar5; //                SPI RESET

    // CO2
    uint8_t co2Type; // 0 - not connected, 1 - auto detect, 2 - MH-Z19, 3 - SenseAir S8
    uint8_t co2RxPin;
    uint8_t co2TxPin;
    uint8_t co2UpdateInSeconds;

    // CO2 data
    uint16_t co2Min;
    uint16_t co2Max;
    uint8_t vPinCo2;
    uint8_t vPinCo2Min;
    uint8_t vPinCo2Max;
    uint8_t vPinCo2Alarm;

    // BME
    uint8_t bmeType; // 0 - not connected, 1 - I2C, 2 - SPI
    uint8_t bmePar1; // I2C SDA     or SPI ?
    uint8_t bmePar2; // I2C SCL     or SPI ?
    uint8_t bmePar3; // I2C address or SPI ?
    uint8_t bmePar4; //                SPI ?
    uint8_t bmeUpdateInSeconds;

    // BME data. Temperature
    uint16_t temperatureMin;
    uint16_t temperatureMax;
    uint8_t vPinTemperature;
    uint8_t vPinTemperatureMin;
    uint8_t vPinTemperatureMax;
    uint8_t vPinTemperatureAlarm;

    // BME data. Humidity
    uint16_t humidityMin;
    uint16_t humidityMax;
    uint8_t vPinHumidity;
    uint8_t vPinHumidityMin;
    uint8_t vPinHumidityMax;
    uint8_t vPinHumidityAlarm;

    // BME data. Pressure
    uint16_t pressureMin;
    uint16_t pressureMax;
    uint8_t vPinPressure;
    uint8_t vPinPressureMin;
    uint8_t vPinPressureMax;
    uint8_t vPinPressureAlarm;

    char finishMarker[7];
};


class Config {
public:
    EEPROMConfig eeprom;


    size_t getEEPROMConfigTotalSize() {
        return sizeof(EEPROMConfig);
    }


    bool checkConfigMarkersAndGetResult() {
        return (strcmp(eeprom.startMarker, startStr) == 0) &&
               (strcmp(eeprom.finishMarker, finishStr) == 0);
    }


    void setDefault() {
        strcpy(eeprom.startMarker, startStr);
        strcpy(eeprom.finishMarker, finishStr);

        strcpy(eeprom.webUser, "config");
        strcpy(eeprom.webUserPassword, "config");
        strcpy(eeprom.otaPassword, "ota!oda@begemota");

        strcpy(eeprom.blynkAuthToken, "");
        strcpy(eeprom.blynkHost, "1.2.3.4");
        eeprom.blynkPort = 0;
        strcpy(eeprom.blynkSSLFingerprint, "");
        eeprom.blynkSendDataInSeconds = 120;

        eeprom.postDataFlag = 1;

        // SSL is ON
//        eeprom.postDataSSLFlag = true;
//        eeprom.postDataSSLAllowSelfSignedCertFlag = true;
//        strcpy(eeprom.postDataURL, "https://1.2.3.4/feed/");
//        strcpy(eeprom.postDataSSLFingerprint, "26 24 FA 2B 26 1D DD 1F 2F 12 68 7C 19 49 13 DF D3 8A 7D 65"); // SHA-1

        // No SSL
        eeprom.postDataSSLFlag = false;
        strcpy(eeprom.postDataURL, "http://1.2.3.4/feed/");

        eeprom.postDataTimoutMillis = 1500;
        eeprom.postDataInSeconds = 5;

        eeprom.deepSleepSeconds = 0;
        eeprom.deepSleepMaxPreExecSeconds = 10;

        // Time
        eeprom.showTimeMillis = 5000;
        eeprom.timeZoneOffset = 4;
        eeprom.summerTime = false;
        strcpy(eeprom.ntpHost, "pool.ntp.org");
        eeprom.ntpReadTimeoutMillis = 5555;
        eeprom.ntpSyncEverySeconds = 3600;

        // Sensors
        eeprom.showSensorsMillis = 5000;

        //OLED
        eeprom.oledType = 2;
        eeprom.oledPar1 = 5;
        eeprom.oledPar2 = 16;
        eeprom.oledPar3 = 4;
        eeprom.oledPar4 = 0;
        eeprom.oledPar5 = 2;

        // CO2
        eeprom.co2Type = 1;
        eeprom.co2RxPin = 13;
        eeprom.co2TxPin = 15;
        eeprom.co2UpdateInSeconds = 10;

        // CO2 data
        eeprom.co2Min = 300;
        eeprom.co2Max = 900;
        eeprom.vPinCo2 = 8;
        eeprom.vPinCo2Min = 9;
        eeprom.vPinCo2Max = 10;
        eeprom.vPinCo2Alarm = 11;

        // BME
        eeprom.bmeType = 1;
        eeprom.bmePar1 = 12;
        eeprom.bmePar2 = 14;
        eeprom.bmePar3 = 0;
        eeprom.bmePar4 = 0;
        eeprom.bmeUpdateInSeconds = 2;

        // BME data. Temperature
        eeprom.temperatureMin = 19;
        eeprom.temperatureMax = 31;
        eeprom.vPinTemperature = 0;
        eeprom.vPinTemperatureMin = 1;
        eeprom.vPinTemperatureMax = 2;
        eeprom.vPinTemperatureAlarm = 3;

        // BME data. Humidity
        eeprom.humidityMin = 20;
        eeprom.humidityMax = 90;
        eeprom.vPinHumidity = 4;
        eeprom.vPinHumidityMin = 5;
        eeprom.vPinHumidityMax = 6;
        eeprom.vPinHumidityAlarm = 7;

        // BME data. Pressure
        eeprom.pressureMin = 700;
        eeprom.pressureMax = 800;
        eeprom.vPinPressure = 12;
        eeprom.vPinPressureMin = 13;
        eeprom.vPinPressureMax = 14;
        eeprom.vPinPressureAlarm = 15;
    }


private:
    char startStr[6] = "Start";
    char finishStr[7] = "Finish";
};

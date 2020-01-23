#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h> // Adafruit Unified Sensor
#include <Adafruit_BME280.h> // Adafruit BME280 Library


enum BME280_STATUSES {
    BME280_NOT_INITIALIZED,
    BME280_READ_FAILED,
    BME280_GOOD
};


class BME280 {
private:
    Adafruit_BME280 bme;
    BME280_STATUSES status = BME280_NOT_INITIALIZED;
    bool i2cFlag = false;
    bool spiFlag = false;
    uint8_t i2cSdaPin = 255, i2cSclPin = 255, i2cAddress = 255;
    uint8_t spiSckPin = 255, spiSdoPin = 255, spiSdiPin = 255, spiCsbPin = 255;
    float temperature = NAN, humidity = NAN, pressure = NAN;

public:
    // I2C with default pins
    bool initI2C() {
        bool initStatus;

        i2cFlag = true;
        spiFlag = false;

        if ((i2cSdaPin != 255) && (i2cSclPin != 255))
            Wire.begin(i2cSdaPin, i2cSclPin);

        if (i2cAddress != 255)
            initStatus = bme.begin(i2cAddress);
        else
            initStatus = bme.begin();

        if (initStatus) {
            status = BME280_GOOD;
            return true;
        } else {
            status = BME280_NOT_INITIALIZED;
            return false;
        }
    }


    // i2c with default pins and address
    bool initI2C(uint8_t address) {
        i2cAddress = address;
        return initI2C();
    }


    // i2c with provided SDA and SCL pins
    bool initI2C(uint8_t SDA, uint8_t SCL) {
        i2cSdaPin = SDA;
        i2cSclPin = SCL;
        return initI2C();
    }


    // i2c with provided SDA, SCL and i2c address
    bool initI2C(uint8_t SDA, uint8_t SCL, uint8_t address) {
        i2cSdaPin = SDA;
        i2cSclPin = SCL;
        i2cAddress = address;
        return initI2C();
    }


    // spi implementation is not ready!
    bool initSPI(uint8_t CSB) {
        spiFlag = true;
        i2cFlag = false;

        spiCsbPin = CSB;
    }


    // spi implementation is not ready!
    bool initSPI(uint8_t sck, uint8_t sdo, uint8_t sdi, uint8_t csb) {
        spiSckPin = sck;
        spiSdoPin = sdo;
        spiSdiPin = sdi;
        initSPI(csb);
    }


    bool readSensorDataAndGetResult() {
        temperature = bme.readTemperature();
        pressure = bme.readPressure();
        humidity = bme.readHumidity();

        if (isnan(temperature) || isnan(pressure) || isnan(humidity) ||
            ((temperature <= -141.30) && (temperature >= -141.33))) {
            status = BME280_READ_FAILED;

            temperature = NAN;
            pressure = NAN;
            humidity = NAN;

            return false;
        } else {
            status = BME280_GOOD;

            return true;
        }
    }


    BME280_STATUSES getStatus() {
        return status;
    }


    float getTemperatureInCelsius() {
        return temperature;
    }


    float getHumidity() {
        return humidity;
    }


    float getPressureInPascal() {
        return pressure;
    }


    float getPressureInMmHg() {
        if (!isnan(pressure))
            return static_cast<float>(pressure * 0.0075006156130264);
        else
            return pressure;
    }
};

#include "BME280.cpp"


BME280 bme;
bool init_status, read_status;


void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("BME280 test");

    init_status = bme.initI2C(12, 14, 0x76);
//    init_status = bme.initI2C(12, 14, 0x77);

    Serial.print("BME280 init senseAirStatus = ");
    Serial.println(init_status);
    Serial.println();
}


void loop() {
    read_status = bme.readSensorDataAndGetResult();
    Serial.print("Read senseAirStatus = ");
    Serial.println(read_status);

    Serial.print("Temperature, C = ");
    Serial.println(bme.getTemperatureInCelsius());

    Serial.print("Pressure, Pa = ");
    Serial.println(bme.getPressureInPascal());

    Serial.print("Pressure, mmHg = ");
    Serial.println(bme.getPressureInMmHg());

    Serial.print("Humidity, % = ");
    Serial.println(bme.getHumidity());

    Serial.println();

    delay(1000);
}

#include "SenseAirS8.cpp"

SENSEAIRS8 si8;
bool readFlag;

void printHex(uint8_t hex) {
    if (hex < 16)
        Serial.print("0");

    Serial.print(hex, HEX);
}


void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println();

    // For debug and development
    si8.printCommandsAndCrc();
    Serial.println();

    Serial.println("SenseAir S8 start");
    si8.init(13, 15);
}


void loop() {
    // Status
    readFlag = si8.readStatus();
    Serial.print("Read mhZ19Status=");
    Serial.println(si8.getReadStatus());

    if (readFlag) {
        Serial.print("Status=");
        Serial.println(si8.getStatus());
    } else
        Serial.println("Read mhZ19Status failed");

    Serial.println();
    // End Status

    // CO2
    readFlag = si8.readCo2();
    Serial.print("Read mhZ19Status=");
    Serial.println(si8.getReadStatus());

    if (readFlag) {
        Serial.print("CO2=");
        Serial.println(si8.getCo2());
    } else
        Serial.println("Read PPM failed");

    Serial.println();
    // End CO2

    // Sensor ID
    readFlag = si8.readSensorId();
    Serial.print("Read mhZ19Status=");
    Serial.println(si8.getReadStatus());

    if (readFlag) {
        Serial.print("Sensor ID=");

        for (uint8_t i = 0; i < si8.getSensorIdLen(); i++)
            printHex(si8.getSensorId()[i]);

        Serial.println();
    } else
        Serial.println("Read Sensor ID failed");

    Serial.println();
    // End Sensor ID

    // Type ID
    readFlag = si8.readTypeId();
    Serial.print("Read mhZ19Status=");
    Serial.println(si8.getReadStatus());

    if (readFlag) {
        Serial.print("Type ID=");

        for (uint8_t i = 0; i < si8.getTypeIdLen(); i++)
            printHex(si8.getTypeId()[i]);

        Serial.println();
    } else
        Serial.println("Read Type ID failed");

    Serial.println();
    // End Type ID

    // Fw version
    readFlag = si8.readFwVersion();
    Serial.print("Read mhZ19Status=");
    Serial.println(si8.getReadStatus());

    if (readFlag) {
        Serial.print("FwVersion=");
        printHex(lowByte(si8.getFwVersion()));
        printHex(highByte(si8.getFwVersion()));
        Serial.println();
    } else
        Serial.println("Read FwVersion failed");

    Serial.println();
    // End Fw version

    delay(5000);
}

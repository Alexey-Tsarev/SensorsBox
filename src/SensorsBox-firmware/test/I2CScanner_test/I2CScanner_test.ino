#include "I2CScanner.cpp"


I2CScanner i2c;
char strBuf[2048];


void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("I2C scanner start");
}


void loop() {
    i2c.initI2C(12, 14);
    Serial.println("scan loop start");

    while (!i2c.isEnd()) {
        if (i2c.scanI2CAndGetResult()) {
            Serial.print("Found=");
            Serial.print(i2c.getAddress());
            Serial.print(", HEX=");
            i2c.returnAddressStr(strBuf, sizeof(strBuf));
            Serial.print(strBuf);
            Serial.println();
        }
    }

    Serial.print("End. Found: ");
    Serial.print(i2c.getFoundDevices());
    Serial.println();
    Serial.println();

    delay(1000);
}

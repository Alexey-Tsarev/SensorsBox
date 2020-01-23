#include "MHZ19.cpp"


MHZ19 z19;
bool status;


void setup() {
    Serial.begin(115200);
    while (!Serial);
    Serial.println("MH-Z19 start");
    z19.init(13, 15);
}


void loop() {
    status = z19.readCo2();
    Serial.print("MHZ-19 mhZ19Status: ");
    Serial.println(status);

    if (!status)
        Serial.println("Read failed");

    Serial.print("Status=");
    Serial.println(z19.getReadStatus());

    Serial.print("PPM=");
    Serial.println(z19.getCo2());

    Serial.println();
    delay(5000);
}

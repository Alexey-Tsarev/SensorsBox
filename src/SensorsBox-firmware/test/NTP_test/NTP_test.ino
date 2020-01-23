#include <ESP8266WiFi.h>
#include "NTP.cpp"

NTP ntp;
bool wifiConnectedFlag, ntpTimeReceived;
uint32_t localTime;
time_t t;
char strBuf[2048];

void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
        case WIFI_EVENT_STAMODE_GOT_IP:
            if (!wifiConnectedFlag) {
                wifiConnectedFlag = true;

                Serial.println("WiFi connected");
            }

            break;

        case WIFI_EVENT_STAMODE_DISCONNECTED:
            if (wifiConnectedFlag) {
                wifiConnectedFlag = false;

                Serial.println("Wifi disconnected");
            }

            break;
    }
}


void setup() {
    WiFi.onEvent(WiFiEvent);

    ntp.setTimeZoneOffset(4);

    Serial.begin(115200);
    while (!Serial);

    Serial.println("NTP start");
}


void loop() {
    if (wifiConnectedFlag and !ntpTimeReceived) {
        ntp.sendNtpTimeRequest();

        delay(2000);
        localTime = ntp.readNtpTimeResponseAndGetLocalTime();

//        do {
//            localTime = ntp.readNtpTimeResponseAndGetLocalTime();
//        } while (localTime == 0);

        if (localTime) {
            Serial.print("Local time: ");
            Serial.println(localTime);

            ntpTimeReceived = true;
        } else {
            Serial.println("Time out");
        }
    }

    if (ntpTimeReceived) {
        t = now();

        snprintf(strBuf, sizeof(strBuf), "%02i:%02i:%02i", hour(t), minute(t), second(t));
        Serial.println(strBuf);
    } else
        Serial.println("Not synced");

    delay(1000);
}

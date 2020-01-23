/*
 * SensorsBox firmware main file.
 * Tested on Arduino 1.8.9
 * Created by Alexey Tsarev: Tsarev.Alexey@gmail.com, atsarev@griddynamics.com
 */

#include <Arduino.h>
#include <ArduinoOTA.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <BlynkSimpleEsp8266.h>
//#include <BlynkSimpleEsp8266_SSL.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>
#else

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <BlynkSimpleEsp32.h> // Blynk
//#include <BlynkSimpleEsp32_SSL.h>
#include <HTTPClient.h>
#include "SPIFFS.h"

#endif

#include <FS.h>
#include <WiFiManager.h> // WiFiManager. ESP32 only: https://github.com/tzapu/WiFiManager/archive/development.zip
#include <ArduinoJson.h>
#include <TimeLib.h>     // Time
#include <U8g2lib.h>     // U8g2
#include <RemoteDebug.h>

#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

#include "SensorsBox-firmware.h"
#include "Config.cpp"
#include "EEPROMer.h"
#include "Elapser.cpp"
#include "Uptimer.cpp"
#include "I2CScanner.cpp"
#include "MHZ19.cpp"
#include "SenseAirS8.cpp"
#include "BME280.cpp"
#include "NTP.cpp"

// Config
#define configSerialSpeed 115200

#define configWiFiManagerIsActive
#define configWiFiManagerActivateAfterSeconds  30
#define configWiFiManagerAPName                "SensorsBox"
#define configWiFiManagerConnectTimeoutSeconds 1
#define configWiFiManagerConfigTimeoutSeconds  180

#define configHTTPPort 80
#define configOTAisActive

#define oledFont1 u8g2_font_6x10_tf
#define oledFont2 u8g2_font_timR18_tn
#define oledFont3 u8g2_font_profont29_mn

//#define configBlinkSSL

#define configDebugIsActive
//#define configSerialDebugIsActive
//#define BLYNK_PRINT Serial
//#define BLYNK_DEBUG
// End Config

char strBuf[2048], strBuf2[2048], wifiManagerApName[32], wifiManagerApPassword[32];
bool i2cDeviceFoundFlag, oledCreatedFlag, oledUpdateFlag, wifiManagerActiveFlag, wifiManagerWasActivatedFlag, alarmFlashFlag, wifiConnectedFlag, blynkFlag, blynkSSLFlag, co2ReadForcedFlag = true, bmeReadForcedFlag = true, alarmFlagTriggered, co2AlarmFlag, temperatureAlarmFlag, humidityAlarmFlag, pressureAlarmFlag, ntpTimeSyncedFlag, ntpRequestWasSentFlag, ntpLastResultFlag, timeColonOnFlag, co2SwapRxTxFlag;
uint8_t co2Type;
uint16_t co2Prev;
float tempPrev, humPrev, presPrev;
uint32_t setupCompletedSec, lastCo2ReceivedSec, lastBmeReceivedSec, lastAlarmFlashMillis, lastBlynkSendDataSec, lastPostDataSec, lastNtpTimeSyncedSec, lastNtpSendRequestMillis, lastTimeScene1Millis, lastTimeScene2Millis, lastSensorsScene1Millis;
uint64_t chipId;

Config conf;
Uptimer uptimer;
uint32_t Elapser::lastTime;
I2CScanner i2c;
MHZ19 z19;
SENSEAIRS8 si8;
BME280 bme;

#ifdef configDebugIsActive
RemoteDebug Debug;
#endif

#ifdef ESP8266
std::unique_ptr <ESP8266WebServer> server;
#else
std::unique_ptr <WebServer> server;
#endif
#ifdef configWiFiManagerIsActive
std::unique_ptr <WiFiManager> wifiManager;
#endif
std::unique_ptr <U8G2> u8g2;
HTTPClient http;
NTP ntp;

enum displayScene {
    timeScene1,
    timeScene2,
    sensorsScene1,
    sensorsScene2
} scene;

void lg(const char *str) {
    Serial.print(str);

#ifdef configDebugIsActive
    rdebugI("%s", str);
#endif
}


void log(const char *str = "") {
    Serial.println(str);

#ifdef configDebugIsActive
    rdebugIln("%s", str);
#endif
}


void returnDateTimeStr(char *str, size_t strLen) {
    if (ntpTimeSyncedFlag) {
        ntp.calcLocalTime();
        setTime(ntp.getLocalTime());
        snprintf(str, strLen, "%04i-%02i-%02i %02i:%02i:%02i.%03i", year(), month(), day(), hour(), minute(), second(),
                 ntp.getLocalTimeMillis());
    } else {
        uptimer.returnUptimeStr(str, strLen);
    }
}


void goToDeepSleep() {
    returnDateTimeStr(strBuf2, sizeof(strBuf2));
    snprintf(strBuf, sizeof(strBuf), "%s. Going to deep sleep for %u seconds", strBuf2, conf.eeprom.deepSleepSeconds);
    log(strBuf);
    ESP.deepSleep(conf.eeprom.deepSleepSeconds * 1000000);
}


bool isCo2Type(uint8_t t) {
    return (conf.eeprom.co2Type == t) || (co2Type == t);
}


uint16_t getCo2() {
    if (isCo2Type(2))
        return z19.getCo2();

    if (isCo2Type(3))
        return si8.getCo2();

    return 0;
}


bool readCo2() {
    if (isCo2Type(2))
        return z19.readCo2();

    if (isCo2Type(3))
        return si8.readCo2();

    return false;
}


uint8_t getCo2ReadStatus() {
    if (isCo2Type(2))
        return z19.getReadStatus();

    if (isCo2Type(3))
        return si8.getReadStatus();

    return 0;
}


void blynkConnect(bool connect = false) {
    blynkFlag = strlen(conf.eeprom.blynkAuthToken) > 0;

    if (blynkFlag) {
        lg("Blynk: configure connection... ");

        blynkSSLFlag = strlen(conf.eeprom.blynkSSLFingerprint) > 0;
#ifdef configBlinkSSL
        if (blynkSSLFlag)
            Blynk.config(conf.eeprom.blynkAuthToken, conf.eeprom.blynkHost, conf.eeprom.blynkPort,
                         conf.eeprom.blynkSSLFingerprint);
#endif
        if (!blynkSSLFlag)
            if (strlen(conf.eeprom.blynkHost))
                if (conf.eeprom.blynkPort)
                    Blynk.config(conf.eeprom.blynkAuthToken, conf.eeprom.blynkHost, conf.eeprom.blynkPort);
                else
                    Blynk.config(conf.eeprom.blynkAuthToken, conf.eeprom.blynkHost);
            else
                Blynk.config(conf.eeprom.blynkAuthToken);

        log("completed (blynk configure)");

        if (connect) {
            log("Blynk: connecting");
            Blynk.connect();
        }
    } else {
        log("Blynk: skip connecting - blynkAuthToken is empty");
    }
}


void blynkVirtualWriteAlarmStatus(uint8_t vPin, bool alarmFlag) {
    if (alarmFlag)
        Blynk.virtualWrite(vPin, "ALARM");
    else
        Blynk.virtualWrite(vPin, "GOOD");
}


void blynkAlarmReport(uint8_t vPinAlarm, bool alarmFlag, char *str) {
    blynkVirtualWriteAlarmStatus(vPinAlarm, alarmFlag);

    if (alarmFlag)
        Blynk.notify(str);
}


void blynkSendAllSensorsData() {
    returnDateTimeStr(strBuf2, sizeof(strBuf2));
    snprintf(strBuf, sizeof(strBuf), "%s. Blynk: send all sensors data... ", strBuf2);
    lg(strBuf);

    Blynk.virtualWrite(conf.eeprom.vPinCo2, getCo2());
    Blynk.virtualWrite(conf.eeprom.vPinTemperature, bme.getTemperatureInCelsius());
    Blynk.virtualWrite(conf.eeprom.vPinHumidity, bme.getHumidity());
    Blynk.virtualWrite(conf.eeprom.vPinPressure, bme.getPressureInMmHg());

    log("completed (blynk send)");
}


BLYNK_CONNECTED() {
    log("Blynk: connected");

    if (conf.eeprom.deepSleepSeconds) {
        blynkSendAllSensorsData();
        Blynk.run();
        postAllData();
        goToDeepSleep();
    }
}


BLYNK_WRITE_DEFAULT() {
    uint8_t pin = request.pin;
    snprintf(strBuf, sizeof(strBuf), "Blynk: write: vPin: %u, val: %s", pin, param.asStr());
    log(strBuf);

    // CO2
    if (pin == conf.eeprom.vPinCo2Min) {
        conf.eeprom.co2Min = param.asInt();
        co2ReadForcedFlag = true;
    } else if (pin == conf.eeprom.vPinCo2Max) {
        conf.eeprom.co2Max = param.asInt();
        co2ReadForcedFlag = true;

        // Temperature
    } else if (pin == conf.eeprom.vPinTemperatureMin) {
        conf.eeprom.temperatureMin = param.asInt();
        bmeReadForcedFlag = true;
    } else if (pin == conf.eeprom.vPinTemperatureMax) {
        conf.eeprom.temperatureMax = param.asInt();
        bmeReadForcedFlag = true;

        // Humidity
    } else if (pin == conf.eeprom.vPinHumidityMin) {
        conf.eeprom.humidityMin = param.asInt();
        bmeReadForcedFlag = true;
    } else if (pin == conf.eeprom.vPinHumidityMax) {
        conf.eeprom.humidityMax = param.asInt();
        bmeReadForcedFlag = true;

        // Pressure
    } else if (pin == conf.eeprom.vPinPressureMin) {
        conf.eeprom.pressureMin = param.asInt();
        bmeReadForcedFlag = true;
    } else if (pin == conf.eeprom.vPinPressureMax) {
        conf.eeprom.pressureMax = param.asInt();
        bmeReadForcedFlag = true;
    }

    if (co2ReadForcedFlag || bmeReadForcedFlag)
        saveConfigToEEPROM();
}


BLYNK_READ_DEFAULT() {
    uint8_t pin = request.pin;
    snprintf(strBuf, sizeof(strBuf), "Blynk: read: pin: %u", pin);
    log(strBuf);

    if (pin == conf.eeprom.vPinCo2)
        Blynk.virtualWrite(pin, getCo2());
    else if (pin == conf.eeprom.vPinCo2Alarm)
        blynkVirtualWriteAlarmStatus(pin, co2AlarmFlag);

    else if (pin == conf.eeprom.vPinTemperature)
        Blynk.virtualWrite(pin, bme.getTemperatureInCelsius());
    else if (pin == conf.eeprom.vPinTemperatureAlarm)
        blynkVirtualWriteAlarmStatus(pin, temperatureAlarmFlag);

    else if (pin == conf.eeprom.vPinHumidity)
        Blynk.virtualWrite(pin, bme.getHumidity());
    else if (pin == conf.eeprom.vPinHumidityAlarm)
        blynkVirtualWriteAlarmStatus(pin, humidityAlarmFlag);

    else if (pin == conf.eeprom.vPinPressure)
        Blynk.virtualWrite(pin, bme.getPressureInMmHg());
    else if (pin == conf.eeprom.vPinPressureAlarm)
        blynkVirtualWriteAlarmStatus(pin, pressureAlarmFlag);
}


void initWebServer() {
    lg("Start Web server... ");

#ifdef ESP8266
    server.reset(new ESP8266WebServer(configHTTPPort));
#else
    server.reset(new WebServer(configHTTPPort));
#endif

    server->on("/", handleURIRoot);
    server->on("/settings/", HTTP_GET, handleURISettingsGET);
    server->on("/settings/", HTTP_POST, handleURISettingsPOST);
    server->on("/sensors/", handleURISensors);
    server->on("/reset_wifi", handleURIResetWiFi);
    server->on("/default", handleURIDefault);
    server->on("/restart", handleURIRestart);
    server->on("/test", handleURITest);
    server->onNotFound(handleURINotFound);
    server->begin();

    log("completed (start web server)");

    if (wifiConnectedFlag) {
        snprintf(strBuf, sizeof(strBuf),
                 "Listening the IP:port: %s:%u", WiFi.localIP().toString().c_str(), configHTTPPort);
        log(strBuf);
    }
}


void setupWiFi(bool disconnect = false) {
    if (disconnect)
        WiFi.disconnect(true);

#ifdef configWiFiManagerIsActive
    returnDateTimeStr(strBuf2, sizeof(strBuf2));
    snprintf(strBuf, sizeof(strBuf), "%s. wifiManager: Connect or setup", strBuf2);
    log(strBuf);

    server.reset();

    wifiManagerActiveFlag = true;
    wifiManager.reset(new WiFiManager);
    wifiManager->setConnectTimeout(configWiFiManagerConnectTimeoutSeconds);
    wifiManager->setConfigPortalTimeout(configWiFiManagerConfigTimeoutSeconds);
    wifiManager->setAPCallback(apConfigCallback);

    snprintf(wifiManagerApPassword, sizeof(wifiManagerApPassword), "%08i", micros());

    if (!wifiManager->autoConnect(wifiManagerApName, wifiManagerApPassword)) {
        returnDateTimeStr(strBuf2, sizeof(strBuf2));
        snprintf(strBuf, sizeof(strBuf), "%s. wifiManager: Connect timeout", strBuf2);
        log(strBuf);

        WiFi.begin();
    }

    wifiManager.reset();
    WiFi.mode(WIFI_STA);
    wifiManagerActiveFlag = false;

    initWebServer();
#endif
}


void WiFiEvent(WiFiEvent_t event) {
    bool event_STA_GOT_IP = false, event_STA_DISCONNECTED = false, event_AP_PROBEREQRECVED = false;

#ifdef ESP8266
    switch (event) {
        case WIFI_EVENT_SOFTAPMODE_PROBEREQRECVED:
            event_AP_PROBEREQRECVED = true;
            break;

        case WIFI_EVENT_STAMODE_GOT_IP:
            event_STA_GOT_IP = true;
            break;

        case WIFI_EVENT_STAMODE_DISCONNECTED:
            event_STA_DISCONNECTED= true;
            break;
    }
#else
    switch (event) {
        case SYSTEM_EVENT_AP_PROBEREQRECVED:
            event_AP_PROBEREQRECVED = true;
            break;

        case SYSTEM_EVENT_STA_GOT_IP:
            event_STA_GOT_IP = true;
            break;

        case SYSTEM_EVENT_STA_DISCONNECTED:
            event_STA_DISCONNECTED = true;
            break;
    }
#endif

    if (!event_AP_PROBEREQRECVED) {
        returnDateTimeStr(strBuf2, sizeof(strBuf2));
        snprintf(strBuf, sizeof(strBuf), "%s. WiFi event: %u", strBuf2, uint8_t(event));
        log(strBuf);
    }

    if (event_STA_GOT_IP && !wifiConnectedFlag) {
        snprintf(strBuf, sizeof(strBuf), "%s. WiFi: connected to: %s / %s", strBuf2, WiFi.SSID().c_str(),
                 WiFi.localIP().toString().c_str());
        log(strBuf);

        wifiConnectedFlag = true;
        WiFi.hostname(wifiManagerApName);

#ifdef configDebugIsActive
        lg("Setup debug... ");
        Debug.begin(wifiManagerApName, Debug.INFO);
//        Debug.showProfiler(true);
//        Debug.showColors(true);
        lg("completed (setup debug)");
#endif
    }

    if (event_STA_DISCONNECTED && wifiConnectedFlag) {
        wifiConnectedFlag = false;

        snprintf(strBuf, sizeof(strBuf), "%s. WiFi: disconnected", strBuf2);
        log(strBuf);
    }
}


void apConfigCallback(WiFiManager *myWiFiManager) {
    if (oledCreatedFlag) {
        returnDateTimeStr(strBuf2, sizeof(strBuf2));
        snprintf(strBuf, sizeof(strBuf), "%s. Drawing WiFiManager setup screen... ", strBuf2);
        lg(strBuf);

        u8g2->clearBuffer();
        u8g2->setFont(oledFont1);
        u8g2->drawStr(0, 10, "=> Access Point Setup");
        u8g2->drawStr(0, 25, "WiFi AP name:");
        u8g2->drawStr(0, 35, wifiManagerApName);
        u8g2->drawStr(0, 55, "Password:");
        u8g2->drawStr(0, 64, wifiManagerApPassword);
        u8g2->sendBuffer();
        log("completed (drawing WiFiManager)");
    }
}


void loadConfigFromEEPROM() {
    lg("Load config from EEPROM... ");
    EEPROMReader(conf.eeprom);
    log("completed (load)");
}


void saveConfigToEEPROM() {
    lg("Save config to EEPROM... ");
    EEPROMUpdater(conf.eeprom);
    log("completed (save)");
}


void initMhZ19() {
    lg("MH-Z19: init... ");
    snprintf(strBuf, sizeof(strBuf), "Rx: %u, Tx: %u... ", conf.eeprom.co2RxPin, conf.eeprom.co2TxPin);
    lg(strBuf);
    z19.init(conf.eeprom.co2RxPin, conf.eeprom.co2TxPin);
    log("completed (MH-Z19 init)");
}


void initSenseAirS8() {
    lg("SenseAir S8: init... ");
    snprintf(strBuf, sizeof(strBuf), "Rx: %u, Tx: %u... ", conf.eeprom.co2RxPin, conf.eeprom.co2TxPin);
    lg(strBuf);
    si8.init(conf.eeprom.co2RxPin, conf.eeprom.co2TxPin);
    log("completed (SenseAir S8 init)");
}


void initCo2() {
    if (conf.eeprom.co2Type == 1) {
        if ((co2Type != 2) && (co2Type != 3)) {
            co2Type = 2;
            co2SwapRxTxFlag = false;
        } else {
            if (co2SwapRxTxFlag) {
                co2Type++;
                co2SwapRxTxFlag = false;

                if (co2Type == 4)
                    co2Type = 2;
            } else {
                uint8_t t = conf.eeprom.co2RxPin;
                conf.eeprom.co2RxPin = conf.eeprom.co2TxPin;
                conf.eeprom.co2TxPin = t;
                co2SwapRxTxFlag = true;
            }
        }
    }

    if (isCo2Type(2))
        initMhZ19();

    if (isCo2Type(3))
        initSenseAirS8();
}


void initBME() {
    // Init BME I2C
    if (conf.eeprom.bmeType == 1) {
        lg("BME I2C: init... ");

        if (conf.eeprom.bmePar3) {
            // I2C address is set
            bme.initI2C(conf.eeprom.bmePar1, conf.eeprom.bmePar2, conf.eeprom.bmePar3);
        } else {
            // I2C address is not set, autodetect address
            lg("address is not set... ");
            i2c.initI2C(conf.eeprom.bmePar1, conf.eeprom.bmePar2);
            i2cDeviceFoundFlag = false;

            while (!i2c.isEnd()) {
                if (i2c.scanI2CAndGetResult()) {
                    snprintf(strBuf, sizeof(strBuf), "Found=%u... ", i2c.getAddress());
                    lg(strBuf);

                    i2cDeviceFoundFlag = true;
                    break;
                }
            }

            if (i2cDeviceFoundFlag)
                bme.initI2C(conf.eeprom.bmePar1, conf.eeprom.bmePar2, i2c.getAddress());
            else
                lg("[WARN] BME I2C device was not found... ");

        }

        log("completed (BME I2C init)");
    }
    // End Init BME I2C

    // Init BME SPI
    if (conf.eeprom.bmeType == 2) {
        lg("BME SPI: init... ");
        bme.initSPI(conf.eeprom.bmePar1, conf.eeprom.bmePar2, conf.eeprom.bmePar3, conf.eeprom.bmePar4);
        log("completed (BME SPI init)");
    }
    // End Init BME SPI
}


void loadConfigAndInit() {
    loadConfigFromEEPROM();

    if (!conf.checkConfigMarkersAndGetResult()) {
        lg("Config load failed. Setting default firmware config... ");
        conf.setDefault();
        log("completed (default config)");
    }

    initCo2();
    initBME();

    // OLED
    oledCreatedFlag = false;

    // OLED I2C
    if (conf.eeprom.oledType == 1) {
        u8g2.reset();
        u8g2.reset(new U8G2_SSD1306_128X64_NONAME_F_SW_I2C(U8G2_R0,
                                                           conf.eeprom.oledPar1,
                                                           conf.eeprom.oledPar2,
                                                           U8X8_PIN_NONE));
        oledCreatedFlag = true;
    }
    // End OLED I2C

    // OLED SPI
    if (conf.eeprom.oledType == 2) {
        u8g2.reset();
        u8g2.reset(new U8G2_SSD1306_128X64_NONAME_F_4W_SW_SPI(U8G2_R0,
                                                              conf.eeprom.oledPar1,
                                                              conf.eeprom.oledPar2,
                                                              conf.eeprom.oledPar3,
                                                              conf.eeprom.oledPar4,
                                                              conf.eeprom.oledPar5));
        oledCreatedFlag = true;
    }
    // End OLED SPI

    if (oledCreatedFlag) {
        lg("OLED: init... ");
        u8g2->begin();
        u8g2->setFlipMode(false);
        u8g2->clearBuffer();
        u8g2->setFont(oledFont1);
        strcpy(strBuf, "Initializing...");
        u8g2->drawStr(0, 36, strBuf);
        u8g2->sendBuffer();
        log("completed (OLED init)");
    }
    // End OLED

    // Blynk
    log("Blynk: disconnect");
    Blynk.disconnect();

    log("Blynk: connect");
    blynkConnect(true);
    // End Blynk
}


void returnSensorsData(DynamicJsonDocument &root) {
    root["device"] = configWiFiManagerAPName;

    snprintf(strBuf, sizeof(strBuf), "%llu", chipId);
    root["id"] = strBuf;

    if (wifiConnectedFlag) {
        root["ipStr"] = WiFi.localIP().toString();
        root["wifiStr"] = WiFi.SSID();
    }

    uptimer.returnUptimeStr(strBuf2, sizeof(strBuf2));
    root["uptime"] = strBuf2;

    root["freeHeap"] = ESP.getFreeHeap();
    root["heapFragmentation"] = ESP.getHeapFragmentation();
    root["maxFreeBlockSize"] = ESP.getMaxFreeBlockSize();

    root["co2Status"] = uint8_t(getCo2ReadStatus());

    root["co2"] = getCo2();
    root["co2Min"] = conf.eeprom.co2Min;
    root["co2Max"] = conf.eeprom.co2Max;
    root["co2AlarmFlag"] = uint8_t(co2AlarmFlag);

    root["bmeStatus"] = uint8_t(bme.getStatus());

    root["temperature"] = bme.getTemperatureInCelsius();
    root["temperatureMin"] = conf.eeprom.temperatureMin;
    root["temperatureMax"] = conf.eeprom.temperatureMax;
    root["temperatureAlarmFlag"] = uint8_t(temperatureAlarmFlag);

    root["humidity"] = bme.getHumidity();
    root["humidityMin"] = conf.eeprom.humidityMin;
    root["humidityMax"] = conf.eeprom.humidityMax;
    root["humidityAlarmFlag"] = uint8_t(humidityAlarmFlag);

    root["pressure"] = bme.getPressureInMmHg();
    root["pressureMin"] = conf.eeprom.pressureMin;
    root["pressureMax"] = conf.eeprom.pressureMax;
    root["pressureAlarmFlag"] = uint8_t(pressureAlarmFlag);
}


bool postAllData() {
    returnDateTimeStr(strBuf2, sizeof(strBuf2));
    snprintf(strBuf, sizeof(strBuf), "%s. Send data to: %s", strBuf2, conf.eeprom.postDataURL);
    log(strBuf);

    DynamicJsonDocument root(sizeof(strBuf));
    returnSensorsData(root);
    serializeJson(root, strBuf, sizeof(strBuf));
    log(strBuf);

    std::unique_ptr <BearSSL::WiFiClientSecure> clientSSL = NULL;
    std::unique_ptr <WiFiClient> client = NULL;
    bool httpStatus;

    if (conf.eeprom.postDataSSLFlag) {
        clientSSL.reset(new BearSSL::WiFiClientSecure);

#ifdef ESP8266
        // Hack to save RAM:
        // https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/bearssl-client-secure-class.html#mfln-or-maximum-fragment-length-negotiation-saving-ram
        clientSSL->setBufferSizes(sizeof(strBuf), sizeof(strBuf2));
#endif
        clientSSL->setFingerprint(conf.eeprom.postDataSSLFingerprint);

        if (conf.eeprom.postDataSSLAllowSelfSignedCertFlag)
            clientSSL->allowSelfSignedCerts();

        httpStatus = http.begin(*clientSSL, conf.eeprom.postDataURL);
    } else {
        client.reset(new WiFiClient);
        httpStatus = http.begin(*client, conf.eeprom.postDataURL);
    }

    if (httpStatus) {
        http.setTimeout(conf.eeprom.postDataTimoutMillis);
        http.addHeader("Content-Type", "application/json");

        lg("Sending data... ");
        yield();
        int httpCode = http.POST(strBuf);
        yield();
        log("completed (sending)");

        int httpResponseSize = http.getSize();

        returnDateTimeStr(strBuf2, sizeof(strBuf2));
        snprintf(strBuf, sizeof(strBuf), "%s. Got HTTP response code: %i. Response size: %i", strBuf2,
                 httpCode,
                 httpResponseSize);
        log(strBuf);

        if (httpResponseSize > 0) {
            log("Response:");
/*
            // Option 1: Buffered way (for a "big" response)
            WiFiClient *stream = &client;

            while (http.connected() && (httpResponseSize > 0 || httpResponseSize == -1)) {
                size_t size = stream->available();

                if (size) {
                    int readBytes = stream->readBytes(strBuf2, size > sizeof(strBuf2) ? sizeof(strBuf2) : size);
                    strBuf2[readBytes] = 0;
                    lg(strBuf2);

                    if (httpResponseSize > 0)
                        httpResponseSize -= readBytes;
                }

//            delay(10);
            }
*/
            // Option2: Alternative 1-one way
            log(http.getString().c_str());
        }

        http.end();

        if (conf.eeprom.postDataSSLFlag)
            clientSSL.reset();
        else
            client.reset();
    } else
        log("Failed to connect");
}


void sendResponse(const char *data, bool isHtml = false, uint16_t responseCode = 200) {
    if (isHtml)
        server->send(responseCode, "text/html; charset=utf-8", data);
    else
        server->send(responseCode, "text/plain; charset=utf-8", data);
}


bool handleAuthorized() {
    if (strlen(conf.eeprom.webUser) && (strlen(conf.eeprom.webUserPassword)) &&
        !server->authenticate(conf.eeprom.webUser, conf.eeprom.webUserPassword)) {

        server->requestAuthentication();
        return false;
    } else
        return true;
}


void returnContentType(const char *file, char *dstContentType) {
    uint16_t fileStrLen = strlen(file);

    if ((strcmp(file + fileStrLen - 4, ".htm") == 0) ||
        (strcmp(file + fileStrLen - 5, ".html") == 0))
        strcpy(dstContentType, "text/html; charset=utf-8");

    else if (strcmp(file + fileStrLen - 4, ".xml") == 0)
        strcpy(dstContentType, "text/xml");

    else if (strcmp(file + fileStrLen - 5, ".json") == 0)
        strcpy(dstContentType, "application/json");

    else if (strcmp(file + fileStrLen - 4, ".css") == 0)
        strcpy(dstContentType, "text/css");

    else if (strcmp(file + fileStrLen - 3, ".js") == 0)
        strcpy(dstContentType, "application/javascript");

    else if (strcmp(file + fileStrLen - 4, ".png") == 0)
        strcpy(dstContentType, "image/png");

    else if (strcmp(file + fileStrLen - 4, ".gif") == 0)
        strcpy(dstContentType, "image/gif");

    else if ((strcmp(file + fileStrLen - 4, ".jpg") == 0) ||
             (strcmp(file + fileStrLen - 5, ".jpeg") == 0))
        strcpy(dstContentType, "image/jpeg");

    else if (strcmp(file + fileStrLen - 4, ".ico") == 0)
        strcpy(dstContentType, "image/x-icon");
}


bool handleSpiFlashFileSystemRead(const char *file) {
    lg("Handling SPI Flash File System reading: '");
    lg(file);
    lg("'... ");

    if (SPIFFS.exists(file)) {
        File f = SPIFFS.open(file, "r");
        returnContentType(file, strBuf);
        server->streamFile(f, strBuf);
        f.close();

        log("completed (handling SPI)");
        return true;
    } else {
        log("not found (handling SPI)");
        return false;
    }
}


void handleURI(const char *uri) {
    if (handleAuthorized() && !handleSpiFlashFileSystemRead(uri)) {
        snprintf(strBuf, sizeof(strBuf), "Not found: %s", uri);
        sendResponse(strBuf, false, 404);
    }
}


void handleURIRoot() {
    handleURI("/index.html");
}


void handleURISettingsGET() {
    if (!handleAuthorized())
        return;

    DynamicJsonDocument r(sizeof(strBuf));

    r["name"] = configWiFiManagerAPName;
    r["id"] = chipId;
    r["freeHeap"] = ESP.getFreeHeap();

    returnDateTimeStr(strBuf2, sizeof(strBuf2));
    r["time"] = strBuf2;

    r["blynkFlag"] = uint8_t(blynkFlag);
    r["blynkConnected"] = uint8_t(Blynk.connected());

    r["wifiStatus"] = uint8_t(wifiConnectedFlag);

    if (wifiConnectedFlag) {
        r["wifiAccessPoint"] = WiFi.SSID();
        r["ipAddress"] = WiFi.localIP().toString();
    } else {
        r["wifiAccessPoint"] = "";
        r["ipAddress"] = "";
    }

    r["webUser"] = conf.eeprom.webUser;
    r["webUserPassword"] = conf.eeprom.webUserPassword;
    r["otaPassword"] = conf.eeprom.otaPassword;

    r["blynkAuthToken"] = conf.eeprom.blynkAuthToken;
    r["blynkHost"] = conf.eeprom.blynkHost;

    if (conf.eeprom.blynkPort)
        r["blynkPort"] = conf.eeprom.blynkPort;
    else
        r["blynkPort"] = "";

    r["blynkSSLFingerprint"] = conf.eeprom.blynkSSLFingerprint;
    r["blynkSendDataInSeconds"] = conf.eeprom.blynkSendDataInSeconds;

    r["postDataFlag"] = uint8_t(conf.eeprom.postDataFlag);
    r["postDataURL"] = conf.eeprom.postDataURL;
    r["postDataTimoutMillis"] = conf.eeprom.postDataTimoutMillis;
    r["postDataInSeconds"] = conf.eeprom.postDataInSeconds;

    r["deepSleepSeconds"] = conf.eeprom.deepSleepSeconds;
    r["deepSleepMaxPreExecSeconds"] = conf.eeprom.deepSleepMaxPreExecSeconds;

    // Time
    r["showTimeMillis"] = conf.eeprom.showTimeMillis;
    r["timeZoneOffset"] = conf.eeprom.timeZoneOffset;
    r["summerTime"] = uint8_t(conf.eeprom.summerTime);
    r["ntpHost"] = conf.eeprom.ntpHost;
    r["ntpReadTimeoutMillis"] = conf.eeprom.ntpReadTimeoutMillis;
    r["ntpSyncEverySeconds"] = conf.eeprom.ntpSyncEverySeconds;

    // Sensors
    r["showSensorsMillis"] = conf.eeprom.showSensorsMillis;

    // OLED
    r["oledType"] = conf.eeprom.oledType;
    r["oledPar1"] = conf.eeprom.oledPar1;
    r["oledPar2"] = conf.eeprom.oledPar2;
    r["oledPar3"] = conf.eeprom.oledPar3;
    r["oledPar4"] = conf.eeprom.oledPar4;
    r["oledPar5"] = conf.eeprom.oledPar5;

    // Z-19
    r["co2Type"] = conf.eeprom.co2Type;
    r["co2RxPin"] = conf.eeprom.co2RxPin;
    r["co2TxPin"] = conf.eeprom.co2TxPin;
    r["co2UpdateInSeconds"] = conf.eeprom.co2UpdateInSeconds;

    // Z-19 data
    r["co2Min"] = conf.eeprom.co2Min;
    r["co2Max"] = conf.eeprom.co2Max;
    r["vPinCo2"] = conf.eeprom.vPinCo2;
    r["vPinCo2Min"] = conf.eeprom.vPinCo2Min;
    r["vPinCo2Max"] = conf.eeprom.vPinCo2Max;
    r["vPinCo2Alarm"] = conf.eeprom.vPinCo2Alarm;

    // BME
    r["bmeType"] = conf.eeprom.bmeType;
    r["bmePar1"] = conf.eeprom.bmePar1;
    r["bmePar2"] = conf.eeprom.bmePar2;
    r["bmePar3"] = conf.eeprom.bmePar3;
    r["bmePar4"] = conf.eeprom.bmePar4;
    r["bmeUpdateInSeconds"] = conf.eeprom.bmeUpdateInSeconds;

    // BME data. Temperature
    r["temperatureMin"] = conf.eeprom.temperatureMin;
    r["temperatureMax"] = conf.eeprom.temperatureMax;
    r["vPinTemperature"] = conf.eeprom.vPinTemperature;
    r["vPinTemperatureMin"] = conf.eeprom.vPinTemperatureMin;
    r["vPinTemperatureMax"] = conf.eeprom.vPinTemperatureMax;
    r["vPinTemperatureAlarm"] = conf.eeprom.vPinTemperatureAlarm;

    // BME data. Humidity
    r["humidityMin"] = conf.eeprom.humidityMin;
    r["humidityMax"] = conf.eeprom.humidityMax;
    r["vPinHumidity"] = conf.eeprom.vPinHumidity;
    r["vPinHumidityMin"] = conf.eeprom.vPinHumidityMin;
    r["vPinHumidityMax"] = conf.eeprom.vPinHumidityMax;
    r["vPinHumidityAlarm"] = conf.eeprom.vPinHumidityAlarm;

    // BME data. Pressure
    r["pressureMin"] = conf.eeprom.pressureMin;
    r["pressureMax"] = conf.eeprom.pressureMax;
    r["vPinPressure"] = conf.eeprom.vPinPressure;
    r["vPinPressureMin"] = conf.eeprom.vPinPressureMin;
    r["vPinPressureMax"] = conf.eeprom.vPinPressureMax;
    r["vPinPressureAlarm"] = conf.eeprom.vPinPressureAlarm;

    if (server->arg("pretty") == "1")
        serializeJsonPretty(r, strBuf, sizeof(strBuf));
    else
        serializeJson(r, strBuf, sizeof(strBuf));

    sendResponse(strBuf);
}

void handleURISettingsPOST() {
    if (!handleAuthorized())
        return;

    DynamicJsonDocument r(sizeof(strBuf));
    DeserializationError error = deserializeJson(r, server->arg("plain"));

    if (!error) {
        bool configChanged = false;

        // Security
        setStrFromJsonObjAndSetFlag(conf.eeprom.webUser, r, "webUser",
                                    sizeof(conf.eeprom.webUser), configChanged);

        setStrFromJsonObjAndSetFlag(conf.eeprom.webUserPassword, r, "webUserPassword",
                                    sizeof(conf.eeprom.webUserPassword), configChanged);

        setStrFromJsonObjAndSetFlag(conf.eeprom.otaPassword, r, "otaPassword",
                                    sizeof(conf.eeprom.otaPassword), configChanged);
        // End Security

        // Blynk
        setStrFromJsonObjAndSetFlag(conf.eeprom.blynkAuthToken, r, "blynkAuthToken",
                                    sizeof(conf.eeprom.blynkAuthToken), configChanged);

        setStrFromJsonObjAndSetFlag(conf.eeprom.blynkHost, r, "blynkHost",
                                    sizeof(conf.eeprom.blynkHost), configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.blynkPort, r, "blynkPort", configChanged);

        setStrFromJsonObjAndSetFlag(conf.eeprom.blynkSSLFingerprint, r, "blynkSSLFingerprint",
                                    sizeof(conf.eeprom.blynkSSLFingerprint), configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.blynkSendDataInSeconds, r, "blynkSendDataInSeconds", configChanged);
        // End Blynk

        // Post Data
        setTypeFromJsonObjAndSetFlag(conf.eeprom.postDataFlag, r, "postDataFlag",
                                     configChanged);

        setStrFromJsonObjAndSetFlag(conf.eeprom.postDataURL, r, "postDataURL",
                                    sizeof(conf.eeprom.postDataURL), configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.postDataTimoutMillis, r, "postDataTimoutMillis",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.postDataInSeconds, r, "postDataInSeconds",
                                     configChanged);
        // End Post Data

        // Deep sleep
        setTypeFromJsonObjAndSetFlag(conf.eeprom.deepSleepSeconds, r, "deepSleepSeconds",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.deepSleepMaxPreExecSeconds, r, "deepSleepMaxPreExecSeconds",
                                     configChanged);
        // End Deep sleep

        // Time
        setTypeFromJsonObjAndSetFlag(conf.eeprom.showTimeMillis, r, "showTimeMillis",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.timeZoneOffset, r, "timeZoneOffset",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.summerTime, r, "summerTime",
                                     configChanged);

        setStrFromJsonObjAndSetFlag(conf.eeprom.ntpHost, r, "ntpHost",
                                    sizeof(conf.eeprom.ntpHost), configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.ntpReadTimeoutMillis, r, "ntpReadTimeoutMillis",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.ntpSyncEverySeconds, r, "ntpSyncEverySeconds",
                                     configChanged);
        // End Time

        // Sensors
        setTypeFromJsonObjAndSetFlag(conf.eeprom.showSensorsMillis, r, "showSensorsMillis",
                                     configChanged);
        // End Sensors

        // OLED
        setTypeFromJsonObjAndSetFlag(conf.eeprom.oledType, r, "oledType",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.oledPar1, r, "oledPar1",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.oledPar2, r, "oledPar2",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.oledPar3, r, "oledPar3",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.oledPar4, r, "oledPar4",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.oledPar5, r, "oledPar5",
                                     configChanged);
        // End OLED

        // Z19
        setTypeFromJsonObjAndSetFlag(conf.eeprom.co2Type, r, "co2Type",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.co2RxPin, r, "co2RxPin",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.co2TxPin, r, "co2TxPin",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.co2UpdateInSeconds, r, "co2UpdateInSeconds",
                                     configChanged);

        // Z19 data
        setTypeFromJsonObjAndSetFlag(conf.eeprom.co2Min, r, "co2Min",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.co2Max, r, "co2Max",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinCo2, r, "vPinCo2",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinCo2Min, r, "vPinCo2Min",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinCo2Max, r, "vPinCo2Max",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinCo2Alarm, r, "vPinCo2Alarm",
                                     configChanged);
        // Z19 End

        // BME
        setTypeFromJsonObjAndSetFlag(conf.eeprom.bmeType, r, "bmeType",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.bmePar1, r, "bmePar1",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.bmePar2, r, "bmePar2",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.bmePar3, r, "bmePar3",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.bmePar4, r, "bmePar4",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.bmeUpdateInSeconds, r, "bmeUpdateInSeconds",
                                     configChanged);

        // BME data. Temperature
        setTypeFromJsonObjAndSetFlag(conf.eeprom.temperatureMin, r, "temperatureMin",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.temperatureMax, r, "temperatureMax",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinTemperature, r, "vPinTemperature",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinTemperatureMin, r, "vPinTemperatureMin",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinTemperatureMax, r, "vPinTemperatureMax",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinTemperatureAlarm, r, "vPinTemperatureAlarm",
                                     configChanged);

        // BME data. Humidity
        setTypeFromJsonObjAndSetFlag(conf.eeprom.humidityMin, r, "humidityMin",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.humidityMax, r, "humidityMax",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinHumidity, r, "vPinHumidity",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinHumidityMin, r, "vPinHumidityMin",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinHumidityMax, r, "vPinHumidityMax",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinHumidityAlarm, r, "vPinHumidityAlarm",
                                     configChanged);

        // BME data. Pressure
        setTypeFromJsonObjAndSetFlag(conf.eeprom.pressureMin, r, "pressureMin",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.pressureMax, r, "pressureMax",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinPressure, r, "vPinPressure",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinPressureMin, r, "vPinPressureMin",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinPressureMax, r, "vPinPressureMax",
                                     configChanged);

        setTypeFromJsonObjAndSetFlag(conf.eeprom.vPinPressureAlarm, r, "vPinPressureAlarm",
                                     configChanged);
        // BME End

        if (configChanged) {
            saveConfigToEEPROM();
            loadConfigAndInit();
            strcpy(strBuf, "OK");
        } else
            strcpy(strBuf, "Nothing to do");

        sendResponse(strBuf);
    } else
        sendResponse("JSON parse failed");
}


void handleURISensors() {
    if (!handleAuthorized())
        return;

    DynamicJsonDocument root(sizeof(strBuf));
    returnSensorsData(root);

    if (server->arg("pretty") == "1")
        serializeJsonPretty(root, strBuf, sizeof(strBuf));
    else
        serializeJson(root, strBuf, sizeof(strBuf));

    sendResponse(strBuf);
}


void handleURIResetWiFi() {
    if (!handleAuthorized())
        return;

    snprintf(strBuf, sizeof(strBuf), "Configure the device using the Access Point: %s", wifiManagerApName);
    sendResponse(strBuf);

    server.reset();
    setupWiFi(true);
    initWebServer();
}


void handleURIDefault() {
    if (!handleAuthorized())
        return;

    lg("Set default settings... ");
    conf.setDefault();
    log("completed (default)");

    saveConfigToEEPROM();
    loadConfigAndInit();

    sendResponse("Resetted to default settings");
}


void handleURIRestart() {
    if (!handleAuthorized())
        return;

    sendResponse(server->uri().c_str());
    ESP.restart();
}


void handleURITest() {
    sendResponse(server->uri().c_str());
}


void handleURINotFound() {
    handleURI(server->uri().c_str());
}


#ifdef configDebugIsActive

void printDebugData() {
    DynamicJsonDocument root(sizeof(strBuf));
    returnSensorsData(root);
    serializeJson(root, strBuf, sizeof(strBuf));
    debugD("%s", strBuf);
}

#endif


void setup() {
    Serial.begin(configSerialSpeed);

#ifdef configSerialDebugIsActive
    Serial.setDebugOutput(true);
#endif
    while (!Serial)
        yield();

#ifdef ESP8266
    chipId = ESP.getChipId();
#else
    chipId = ESP.getEfuseMac();
#endif

    lg("Start. ChipId: ");
    snprintf(strBuf, sizeof(strBuf), "%llu", chipId);
    log(strBuf);

    WiFi.onEvent(WiFiEvent);
    WiFi.begin();
    WiFi.mode(WIFI_STA);
    http.setReuse(true);
    initWebServer();
    loadConfigAndInit();

    snprintf(wifiManagerApName, sizeof(wifiManagerApName), "%s-%llu", configWiFiManagerAPName, chipId);

#ifdef configOTAisActive
    ArduinoOTA.setHostname(wifiManagerApName);

    if (strlen(conf.eeprom.otaPassword))
        ArduinoOTA.setPassword(conf.eeprom.otaPassword);

    ArduinoOTA.onStart([]() {
        lg("Start updating: ");

        if (ArduinoOTA.getCommand() == U_FLASH)
            log("sketch");
        else
            log("filesystem");
    });

    ArduinoOTA.onProgress([](uint32 progress, uint32 total) {
        snprintf(strBuf, sizeof(strBuf), "Progress: %u", progress / (total / 100));
        log(strBuf);
    });

    ArduinoOTA.onEnd([]() {
        log("OTA completed");
    });

    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
            log("Auth failed");
        } else if (error == OTA_BEGIN_ERROR) {
            log("Begin failed");
        } else if (error == OTA_CONNECT_ERROR) {
            log("Connect failed");
        } else if (error == OTA_RECEIVE_ERROR) {
            log("Receive failed");
        } else if (error == OTA_END_ERROR) {
            log("End failed");
        }
    });

    ArduinoOTA.begin();
#endif

    SPIFFS.begin();

    if (conf.eeprom.deepSleepSeconds) {
        co2ReadForcedFlag = true;
        bmeReadForcedFlag = true;
    }

    ntp.setTimeZoneOffset(conf.eeprom.timeZoneOffset);
    ntp.setNtpHost(conf.eeprom.ntpHost);
    ntp.setSummerTime(conf.eeprom.summerTime);

    setupCompletedSec = Elapser::getElapsedTime(0, sec);
    returnDateTimeStr(strBuf2, sizeof(strBuf2));
    snprintf(strBuf, sizeof(strBuf), "%s. Setup completed at: %u seconds", strBuf2, setupCompletedSec);
    log(strBuf);
}


void loop() {
#ifdef configDebugIsActive
    debugD("Loop start");
#endif

    // Start WiFi Manager
#ifdef configWiFiManagerIsActive
    if (!wifiManagerWasActivatedFlag &&
        Elapser::isElapsedTimeFromStart(setupCompletedSec, configWiFiManagerActivateAfterSeconds, sec)) {
        if (!wifiConnectedFlag)
            setupWiFi();

        wifiManagerWasActivatedFlag = true;
        WiFi.mode(WIFI_STA);
    }
#endif
    // End Start WiFi Manager

    if (wifiConnectedFlag) {
        // Send all data
        if (conf.eeprom.blynkSendDataInSeconds &&
            Elapser::isElapsedTimeFromStart(lastBlynkSendDataSec, conf.eeprom.blynkSendDataInSeconds, sec, true) &&
            (scene != timeScene2))
            blynkSendAllSensorsData();

        if (conf.eeprom.postDataFlag && conf.eeprom.postDataInSeconds &&
            Elapser::isElapsedTimeFromStart(lastPostDataSec, conf.eeprom.postDataInSeconds, sec, true) &&
            (scene != timeScene2))
            postAllData();
        // Send all data

        // Time sync
        if (conf.eeprom.showTimeMillis &&
            (!ntpTimeSyncedFlag ||
             Elapser::isElapsedTimeFromStart(lastNtpTimeSyncedSec, conf.eeprom.ntpSyncEverySeconds, sec))) {
            if (!ntpRequestWasSentFlag) {
                // NTP Request
                returnDateTimeStr(strBuf2, sizeof(strBuf2));
                snprintf(strBuf, sizeof(strBuf), "%s. NTP: Send request", strBuf2);
                log(strBuf);

                ntp.sendNtpTimeRequest();
                ntpRequestWasSentFlag = true;
                lastNtpSendRequestMillis = Elapser::getElapsedTime(0, milliSec);
                // End NTP Request
            } else {
                // NTP Response
#ifdef configDebugIsActive
                returnDateTimeStr(strBuf2, sizeof(strBuf2));
                snprintf(strBuf, sizeof(strBuf), "%s. NTP: Check response", strBuf2);
                debugD("%s", strBuf);
#endif

                if (ntp.readNtpTimeResponseAndGetLocalTime()) {
                    returnDateTimeStr(strBuf2, sizeof(strBuf2));
                    snprintf(strBuf, sizeof(strBuf), "%s. NTP: Synced", strBuf2);
                    log(strBuf);

                    ntpTimeSyncedFlag = true;
                    ntpLastResultFlag = true;
                    ntpRequestWasSentFlag = false;
                    lastNtpTimeSyncedSec = Elapser::getElapsedTime(0, sec);
                } else if (Elapser::isElapsedTimeFromStart(lastNtpSendRequestMillis,
                                                           conf.eeprom.ntpReadTimeoutMillis,
                                                           milliSec)) {
                    returnDateTimeStr(strBuf2, sizeof(strBuf2));
                    snprintf(strBuf, sizeof(strBuf), "%s. NTP: timeout", strBuf2);
                    log(strBuf);
                    ntpRequestWasSentFlag = false;
                    ntpLastResultFlag = false;
                }
                // End NTP Response
            }
        }
        // End Time sync
    }

    // CO2 is ready to read data
    if (conf.eeprom.co2Type && (co2ReadForcedFlag ||
                                Elapser::isElapsedTimeFromStart(lastCo2ReceivedSec,
                                                                conf.eeprom.co2UpdateInSeconds,
                                                                sec,
                                                                true))) {
        if (readCo2()) {
            // Print sensor's data
            returnDateTimeStr(strBuf2, sizeof(strBuf2));
            snprintf(strBuf, sizeof(strBuf), "%s. CO2: %u", strBuf2, getCo2());
            log(strBuf);
            // End Print

            if (edgesValueChecker(co2ReadForcedFlag, co2Prev, getCo2(), conf.eeprom.co2Min,
                                  conf.eeprom.co2Max, co2AlarmFlag, "CO2", strBuf, sizeof(strBuf),
                                  alarmFlagTriggered)) {
                if (alarmFlagTriggered)
                    blynkAlarmReport(conf.eeprom.vPinCo2Alarm, co2AlarmFlag, strBuf);

                oledUpdateFlag = true;
                Blynk.virtualWrite(conf.eeprom.vPinCo2, getCo2());
            }
        } else {
            snprintf(strBuf, sizeof(strBuf), "CO2 read failed. Error code: %u", getCo2ReadStatus());
            log(strBuf);
        }

        if (co2ReadForcedFlag)
            co2ReadForcedFlag = false;
    }
    // End CO2

    // BME280 is ready to read data
    if (conf.eeprom.bmeType && (bmeReadForcedFlag ||
                                Elapser::isElapsedTimeFromStart(lastBmeReceivedSec,
                                                                conf.eeprom.bmeUpdateInSeconds,
                                                                sec,
                                                                true))) {
        if (bme.getStatus() == BME280_GOOD && bme.readSensorDataAndGetResult()) {
            // Print sensor's data
            returnDateTimeStr(strBuf2, sizeof(strBuf2));
            snprintf(strBuf, sizeof(strBuf), "%s. BME280 Temperature: ", strBuf2);

            dtostrf(bme.getTemperatureInCelsius(), 0, 3, strBuf2);
            snprintf(strBuf + strlen(strBuf), sizeof(strBuf) - strlen(strBuf), "%s, Humidity: ", strBuf2);

            dtostrf(bme.getHumidity(), 0, 3, strBuf2);
            snprintf(strBuf + strlen(strBuf), sizeof(strBuf) - strlen(strBuf), "%s, Pressure: ", strBuf2);

            dtostrf(bme.getPressureInMmHg(), 0, 3, strBuf2);
            snprintf(strBuf + strlen(strBuf), sizeof(strBuf) - strlen(strBuf), "%s", strBuf2);

            log(strBuf);
            // End Print

            // Temperature
            if (edgesValueChecker(bmeReadForcedFlag, tempPrev, bme.getTemperatureInCelsius(),
                                  conf.eeprom.temperatureMin,
                                  conf.eeprom.temperatureMax, temperatureAlarmFlag, "Temperature", strBuf,
                                  sizeof(strBuf),
                                  alarmFlagTriggered)) {
                oledUpdateFlag = true;

                if (0 == conf.eeprom.blynkSendDataInSeconds)
                    Blynk.virtualWrite(conf.eeprom.vPinTemperature, bme.getTemperatureInCelsius());

                if (alarmFlagTriggered)
                    blynkAlarmReport(conf.eeprom.vPinTemperatureAlarm, temperatureAlarmFlag, strBuf);
            }
            // End

            // Humidity
            if (edgesValueChecker(bmeReadForcedFlag, humPrev, bme.getHumidity(), conf.eeprom.humidityMin,
                                  conf.eeprom.humidityMax, humidityAlarmFlag, "Humidity", strBuf, sizeof(strBuf),
                                  alarmFlagTriggered)) {
                oledUpdateFlag = true;

                if (0 == conf.eeprom.blynkSendDataInSeconds)
                    Blynk.virtualWrite(conf.eeprom.vPinHumidity, bme.getHumidity());

                if (alarmFlagTriggered)
                    blynkAlarmReport(conf.eeprom.vPinHumidityAlarm, humidityAlarmFlag, strBuf);
            }
            // End

            // Pressure
            if (edgesValueChecker(bmeReadForcedFlag, presPrev, bme.getPressureInMmHg(), conf.eeprom.pressureMin,
                                  conf.eeprom.pressureMax, pressureAlarmFlag, "Pressure", strBuf, sizeof(strBuf),
                                  alarmFlagTriggered)) {
                oledUpdateFlag = true;

                if (0 == conf.eeprom.blynkSendDataInSeconds)
                    Blynk.virtualWrite(conf.eeprom.vPinPressure, bme.getPressureInMmHg());

                if (alarmFlagTriggered)
                    blynkAlarmReport(conf.eeprom.vPinPressureAlarm, pressureAlarmFlag, strBuf);
            }
            // End
        } else {
            snprintf(strBuf, sizeof(strBuf), "BME280 failed. Error code: %u", bme.getStatus());
            log(strBuf);
        }

        if (bmeReadForcedFlag)
            bmeReadForcedFlag = false;
    }
    // End BME280

    // OLED
    if (conf.eeprom.oledType) {
        // Time
        if (scene == timeScene1) {
            if (ntpTimeSyncedFlag) {
                lastTimeScene1Millis = millis();
                scene = timeScene2;
                oledUpdateFlag = true;
                timeColonOnFlag = true;
            } else
                scene = sensorsScene1;
        }

        if (scene == timeScene2) {
            if (Elapser::isElapsedTimeFromStart(lastTimeScene1Millis, conf.eeprom.showTimeMillis, milliSec)) {
                scene = sensorsScene1;
            } else if (oledUpdateFlag) {
                returnDateTimeStr(strBuf2, sizeof(strBuf2));
                snprintf(strBuf, sizeof(strBuf), "%s. OLED: print time... ", strBuf2);
                lg(strBuf);

                u8g2->clearBuffer();
                u8g2->setFont(oledFont3);

                ntp.calcLocalTime();
                setTime(ntp.getLocalTime());

                if (wifiConnectedFlag && ntpLastResultFlag && (ntp.getLocalTimeMillis() >= 500))
                    timeColonOnFlag = false;
                else
                    timeColonOnFlag = true;

                if (timeColonOnFlag)
                    snprintf(strBuf, sizeof(strBuf), "%02i:%02i:%02i", hour(), minute(), second());
                else
                    snprintf(strBuf, sizeof(strBuf), "%02i %02i %02i", hour(), minute(), second());

                u8g2->drawStr(0, 20, strBuf);
                u8g2->sendBuffer();

                log("completed (OLED print time)");

                oledUpdateFlag = false;
                lastTimeScene2Millis = ntp.getLastMillis();
            } else if (Elapser::isElapsedTimeFromStart(lastTimeScene2Millis, 200, milliSec))
                oledUpdateFlag = true;
        }
        // Time

        // Sensors
        if (scene == sensorsScene1) {
            lastSensorsScene1Millis = millis();
            scene = sensorsScene2;
            oledUpdateFlag = true;
        }

        if (scene == sensorsScene2) {
            if ((co2AlarmFlag || temperatureAlarmFlag || humidityAlarmFlag || pressureAlarmFlag) &&
                Elapser::isElapsedTimeFromStart(lastAlarmFlashMillis, 750, milliSec, true)) {
                alarmFlashFlag = !alarmFlashFlag;
                oledUpdateFlag = true;
            }

            if (Elapser::isElapsedTimeFromStart(lastSensorsScene1Millis, conf.eeprom.showSensorsMillis, milliSec)) {
                scene = timeScene1;

                if (bme.getStatus() != BME280_GOOD) {
                    log("Call init BME280 because of sensor error");
                    initBME();
                }

                if ((isCo2Type(2) && (z19.getReadStatus() != MHZ19_GOOD)) ||
                    (isCo2Type(3) && (si8.getReadStatus() != SENSEAIR_GOOD))) {
                    log("Call init CO2 because of sensor error");
                    initCo2();
                }
            } else if (oledUpdateFlag) {
                returnDateTimeStr(strBuf2, sizeof(strBuf2));
                snprintf(strBuf, sizeof(strBuf), "%s. OLED: print sensors... ", strBuf2);
                lg(strBuf);

                u8g2->clearBuffer();

                // Temperature
                if (!temperatureAlarmFlag || alarmFlashFlag) {
                    u8g2->setFont(oledFont1);
                    u8g2->drawStr(0, 10, "Temperature");

                    if (isnan(bme.getTemperatureInCelsius()))
                        strcpy(strBuf, "000");
                    else
                        dtostrf(bme.getTemperatureInCelsius(), 0, 2, strBuf);

                    u8g2->setFont(oledFont2);
                    u8g2->drawStr(5, 33, strBuf);
                }
                // End Temperature

                // CO2
                if (!co2AlarmFlag || alarmFlashFlag) {
                    u8g2->setFont(oledFont1);
                    u8g2->drawStr(20, 64, "CO2");

                    if (getCo2() == 0)
                        strcpy(strBuf, "000");
                    else
                        sprintf(strBuf, "%u", getCo2());

                    u8g2->setFont(oledFont2);
                    u8g2->drawStr(10, 54, strBuf);
                }
                // End CO2

                // Humidity
                if (!humidityAlarmFlag || alarmFlashFlag) {
                    u8g2->setFont(oledFont1);
                    u8g2->drawStr(80, 10, "Humidity");

                    if (isnan(bme.getHumidity()))
                        strcpy(strBuf, "000");
                    else
                        dtostrf(bme.getHumidity(), 0, 2, strBuf);

                    u8g2->setFont(oledFont2);
                    u8g2->drawStr(75, 33, strBuf);
                }
                // End Humidity

                // Pressure
                if (!pressureAlarmFlag || alarmFlashFlag) {
                    u8g2->setFont(oledFont1);
                    u8g2->drawStr(77, 64, "Pressure");

                    if (isnan(bme.getPressureInMmHg()))
                        strcpy(strBuf, "000");
                    else
                        dtostrf(bme.getPressureInMmHg(), 0, 0, strBuf);

                    u8g2->setFont(oledFont2);
                    u8g2->drawStr(82, 54, strBuf);
                }
                // End Pressure

                u8g2->sendBuffer();
                log("completed (OLED print sensors)");
                oledUpdateFlag = false;
            }
        }
        // End Sensors
    }
    // End OLED

    if (conf.eeprom.deepSleepSeconds &&
        Elapser::isElapsedTimeFromStart(setupCompletedSec, conf.eeprom.deepSleepMaxPreExecSeconds, sec)) {
        returnDateTimeStr(strBuf2, sizeof(strBuf2));
        snprintf(strBuf, sizeof(strBuf), "%s. Timeout sending sensors data (timeout is %u seconds)", strBuf2,
                 conf.eeprom.deepSleepMaxPreExecSeconds);
        log(strBuf);

        goToDeepSleep();
    }

#ifdef configDebugIsActive
    printDebugData();
#endif

#ifdef configOTAisActive
#ifdef configDebugIsActive
    debugD("ArduinoOTA start");
#endif
    ArduinoOTA.handle();
#ifdef configDebugIsActive
    debugD("ArduinoOTA finish");
#endif
#endif

#ifdef configDebugIsActive
    debugD("handleClient start");
#endif
    server->handleClient();
#ifdef configDebugIsActive
    debugD("handleClient finish");
#endif

#ifdef configDebugIsActive
    debugD("Blynk.run start");
#endif
    Blynk.run();
#ifdef configDebugIsActive
    debugD("Blynk.run finish");

    debugD("Debug.handle start");
    Debug.handle();
    debugD("Debug.handle finish");

    debugD("Loop end");
#endif
}

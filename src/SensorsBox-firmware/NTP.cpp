#include <Arduino.h>
#include <WiFiUdp.h>
#include <Udp.h>
#include <TimeLib.h> // Time library


class NTP {
public:
    void setNtpHost(char *host) {
        strncpy(host, ntpHost, sizeof(ntpHost));
    }

    void setNtpPort(uint16_t port) {
        ntpPort = port;
    }

    void setTimeZoneOffset(int8_t zone) {
        timeZoneOffset = zone;
    }

    void setMinutesOffset(int8_t minutes) {
        minutesOffset = minutes;
    }

    void setSummerTime(bool summer) {
        summerTime = summer;
    }

    void sendNtpTimeRequest() {
        udp.begin(ntpPort);
        while (udp.parsePacket() > 0);
        sendNtpPacket(ntpHost);
    }

    uint32_t readNtpTimeResponseAndGetLocalTime() {
        int size = udp.parsePacket();

        if (size >= NTP_PACKET_SIZE) {
            udp.read(packetBuffer, NTP_PACKET_SIZE);
            ntpResponseMillis = millis();

            uint32_t highWord = word(packetBuffer[40], packetBuffer[41]);
            uint32_t lowWord = word(packetBuffer[42], packetBuffer[43]);
            secsSince1900 = highWord << 16 | lowWord;
            unixTime = secsSince1900 - 2208988800UL;
            localTimeSrc = unixTime + timeZoneOffset * SECS_PER_HOUR + minutesOffset * SECS_PER_MIN;

            if (summerTime &&
                isSummerTime(year(localTime), month(localTime), day(localTime), hour(localTime), timeZoneOffset))
                localTimeSrc += SECS_PER_HOUR;

            localTime = localTimeSrc;
            return localTime;
        }

        return 0;
    }

    uint32_t getUnixTime() {
        return unixTime;
    }

    uint32_t getLocalTime() {
        return localTime;
    }

    uint16_t getLocalTimeMillis() {
        return localTimeMillis;
    }

    uint32_t getLastMillis() {
        return lastMillis;
    }

    uint32_t calcLocalTime() {
        uint32_t deltaMillis, deltaSeconds;
        lastMillis = millis();

        if (lastMillis >= ntpResponseMillis)
            deltaMillis = lastMillis - ntpResponseMillis;
        else
            deltaMillis = 0xFFFFFFFF - ntpResponseMillis + 1 + lastMillis;

        deltaSeconds = deltaMillis / 1000;

        localTime = localTimeSrc + deltaSeconds;
        localTimeMillis = deltaMillis - deltaSeconds * 1000;
    }


private:
    char ntpHost[32] = "pool.ntp.org";
    uint16_t ntpPort = 123;
    int8_t timeZoneOffset = 0;
    int8_t minutesOffset = 0;
    bool summerTime = false;
    const static uint8_t NTP_PACKET_SIZE = 48;
    uint8_t packetBuffer[NTP_PACKET_SIZE];
    WiFiUDP udp;
    uint32_t secsSince1900, unixTime, localTimeSrc, localTime, ntpResponseMillis, lastMillis;
    uint16_t localTimeMillis;

    void sendNtpPacket(char *address) {
        memset(packetBuffer, 0, NTP_PACKET_SIZE);

        packetBuffer[0] = 0b11100011;
        packetBuffer[1] = 0;
        packetBuffer[2] = 6;
        packetBuffer[3] = 0xEC;
        packetBuffer[12] = 49;
        packetBuffer[13] = 0x4E;
        packetBuffer[14] = 49;
        packetBuffer[15] = 52;

        udp.beginPacket(address, ntpPort);
        udp.write(packetBuffer, NTP_PACKET_SIZE);
        udp.endPacket();
    }

    bool isSummerTime(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t tzHours) {
        if ((month < 3) || (month > 10))
            return false;

        if ((month > 3) && (month < 10))
            return true;

        if ((month == 3 && (hour + 24 * day) >= (1 + tzHours + 24 * (31 - (5 * year / 4 + 4) % 7))) ||
            (month == 10 && (hour + 24 * day) < (1 + tzHours + 24 * (31 - (5 * year / 4 + 1) % 7))))
            return true;
        else
            return false;
    }
};

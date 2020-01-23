#include <Arduino.h>
#include <EEPROM.h>


template<class T>
uint32_t EEPROMReader(T &value, uint32_t size = 0, uint32_t startAddress = 0) {
    uint8_t *p = (uint8_t *) (void *) &value;
    uint32_t i;

    if (size == 0)
        size = sizeof(value);

    EEPROM.begin(startAddress + size);

    for (i = 0; i < size; i++)
        *p++ = EEPROM.read(startAddress++);

    EEPROM.end();

    return i;
}


template<class T>
uint32_t EEPROMUpdater(const T &value, uint32_t size = 0, uint32_t startAddress = 0, uint32_t totalSize = 0) {
    const uint8_t *uint8_tValue = (const uint8_t *) (const void *) &value;
    uint8_t b;
    uint32_t i;

    if (size == 0)
        size = sizeof(value);

    if (totalSize == 0)
        EEPROM.begin(startAddress + size);
    else
        EEPROM.begin(totalSize);

    for (i = 0; i < size; i++) {
        b = EEPROM.read(startAddress);

        if (b != *uint8_tValue)
            EEPROM.write(startAddress, *uint8_tValue);

        startAddress++;
        *uint8_tValue++;
    }

    EEPROM.end();

    return i;
}

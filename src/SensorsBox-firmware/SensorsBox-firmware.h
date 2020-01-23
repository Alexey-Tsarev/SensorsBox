#include <Arduino.h>


template<typename T>
bool setTypeFromJsonObj(T &src, DynamicJsonDocument &r, char *jsonKey) {
    bool setFlag;

    if (r.containsKey(jsonKey) && (src != r[jsonKey])) {
        src = r[jsonKey];
        setFlag = true;
    } else
        setFlag = false;

    return setFlag;
}


template<typename T>
bool setTypeFromJsonObjAndSetFlag(T &src, DynamicJsonDocument &r, char *jsonKey, bool &flag) {
    bool setResult = setTypeFromJsonObj(src, r, jsonKey);

    if (setResult)
        flag = true;

    return setResult;
}


bool setStrFromJsonObj(char *src, DynamicJsonDocument &r, char *jsonKey, size_t srcLen) {
    bool setFlag;

    if (r.containsKey(jsonKey) && (strcmp(src, r[jsonKey].as<String>().c_str()) != 0)) {
        strncpy(src, r[jsonKey].as<String>().c_str(), srcLen);
        setFlag = true;
    } else
        setFlag = false;

    return setFlag;
}


bool setStrFromJsonObjAndSetFlag(char *src, DynamicJsonDocument &r, char *jsonKey, size_t srcLen, bool &flag) {
    bool setResult = setStrFromJsonObj(src, r, jsonKey, srcLen);

    if (setResult)
        flag = true;

    return setResult;
}


template<typename T>
void numberToStr(T num, char *strBuf) {
    sprintf(strBuf, "%u", num);
}


void numberToStr(float num, char *strBuf) {
    dtostrf(num, 0, 3, strBuf);
}


template<typename T>
bool edgesValueChecker(bool &forceFlag, T &prevValue, T curValue, uint16_t min, uint16_t max, bool &alarmFlag,
                       char *component, char *strBuf, size_t strBufLen, bool &alarmFlagTriggered) {
    bool valChanged;
    alarmFlagTriggered = false;

    if (forceFlag || (prevValue != curValue)) {
        prevValue = curValue;
        valChanged = true;

        if ((curValue > max) || (curValue < min) || isnan(curValue)) {
            if (!alarmFlag) {
                alarmFlag = true;
                alarmFlagTriggered = true;

                snprintf(strBuf, strBufLen, "Alarm! %s: ", component);
                numberToStr(curValue, strBuf + strlen(strBuf));
                snprintf(strBuf + strlen(strBuf), strBufLen - strlen(strBuf), " (min/max are %u/%u)", min, max);
            }
        } else {
            if (alarmFlag) {
                alarmFlag = false;
                alarmFlagTriggered = true;
            }
        }
    } else
        valChanged = false;

    return valChanged;
}

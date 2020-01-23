#!/usr/bin/env sh

set -x -e

cd BME280_test
rm BME280.cpp || true
ln -s ../../BME280.cpp .
cd -

cd I2CScanner_test
rm I2CScanner.cpp || true
ln -s ../../I2CScanner.cpp .
cd -

cd MHZ19_test
rm MHZ19.cpp || true
ln -s ../../MHZ19.cpp .
cd -

cd NTP_test
rm NTP.cpp || true
ln -s ../../NTP.cpp .
cd -

cd SenseAirS8_test
rm SenseAirS8.cpp || true
ln -s ../../SenseAirS8.cpp .
cd -

# SensorsBox
Welcome to SensorsBox!

This is a device which uses ESP8266 and ESP32 (ESP32 is not well tested) and has the following features:
- Measurement: CO2 concentration (MH Z-19 or SenseAir S8 devices are supported)
- Measurement: Temperature, Humidity, Pressure (provided by a BME-280 sensor)
- OLED display
- Easy WiFi setup (WiFiManager library)
- Digital realtime clock (autosync via NTP)
- Threshold values for measured values (values are blink when the it happens)

Optional features:
- Send data to Zabbix
- Easy (I hope) deployment to a VDS/VPS host
- Blynk integration
- OTA update
- Remote debug

# Electrical schema / Circuit diagram
TODO

# Laser cut files (to make a real box)
TODO

# Deploy
There is a way to collect sensors data via Zabbix.  
You may use a VDS/VPS for it or, as example, an AWS/GCP free instance(s):
- https://aws.amazon.com/free/free-tier/
- https://cloud.google.com/free/
- https://cloud.google.com/free/docs/gcp-free-tier#always-free-usage-limits

## Deploy steps
Change `<IP>` to your IP address.  
````
# ssh to your your remote host.
ssh <IP>

# Install tools
sudo yum -y install git svn

# Install Docker. An instruction for CentOS:
# https://docs.docker.com/install/linux/docker-ce/centos/

# Install Docker Compose
# https://docs.docker.com/compose/install/

# Сhange PostgreSQL root password:
"${EDITOR}" quick_deploy/.env

# Clone repositories:
quick_deploy/home/www_default/public_html/clone_repos.sh

# Run applicaitons:
docker-compose -f quick_deploy/docker-compose.yml up
# In case of no errors, use:
# docker-compose -f quick_deploy/docker-compose.yml up -d

# Use Zabbix UI for setup:
http://<IP>/zabbix/

# Сhange Zabbix Admin password (if default password was changed)
"${EDITOR}" quick_deploy/home/www_default/public_html/www/feed/index.php

# Test feeder (for a new device this takes about a minute):
curl --header "Content-Type: application/json" --request POST --data \
'{"device":"SensorsBox","id":"8800871","ip":"192.168.3.97","wifiStr":"ArtLuch.RU-IoT","uptime":"1010.062","freeHeap":17952,"heapFragmentation":2,"maxFreeBlockSize":17664,"z19Status":1,"co2":953,"co2Min":300,"co2Max":900,"co2AlarmFlag":1,"bmeStatus":2,"temperature":25.96,"temperatureMin":19,"temperatureMax":31,"temperatureAlarmFlag":0,"humidity":39.9873,"humidityMin":20,"humidityMax":90,"humidityAlarmFlag":0,"pressure":757.938,"pressureMin":700,"pressureMax":800,"pressureAlarmFlag":0}' \
http://<IP>/feed/
````

The following result:
````
OK
````
means the data were stored to Zabbix DB.  
A SensorsBox performs a similar HTTP requests.
So, your server is ready to receive and store a SensorsBox data.

## Device
Connect wires according to schema, compile and upload a firmware to your device via Arduino IDE.  
Start with the following file:  
`src/SensorsBox-firmware/SensorsBox-firmware.ino` 


The device has some endpoints:  
`/settings/` `GET` - get all settings  
`/settings/` `POST` - set settings

Examples:
````
curl -u config:config http://IP/settings/
{"name":"SensorsBox","id":8800871,"freeHeap":17792,"uptime":"2019-09-27 18:50:35.083","blynkFlag":0,"blynkConnected":0,"wifiStatus":1,"wifiAccessPoint":"SmartOffice","ipAddress":"192.168.11.11","webUser":"config","webUserPassword":"config","otaPassword":"ota!oda@begemota","blynkAuthToken":"","blynkHost":"35.237.110.54","blynkPort":"","blynkSSLFingerprint":"","blynkSendDataInSeconds":120,"postDataFlag":1,"postDataURL":"http://35.237.110.54/feed/","postDataTimoutMillis":1500,"postDataInSeconds":5,"deepSleepSeconds":0,"deepSleepMaxPreExecSeconds":10,"showTimeMillis":5000,"timeZoneOffset":4,"summerTime":0,"ntpHost":"pool.ntp.org","ntpReadTimeoutMillis":5555,"ntpSyncEverySeconds":3600,"showSensorsMillis":5000,"oledType":2,"oledPar1":5,"oledPar2":16,"oledPar3":4,"oledPar4":0,"oledPar5":2,"co2Type":1,"co2RxPin":13,"co2TxPin":15,"co2UpdateInSeconds":10,"co2Min":300,"co2Max":900,"vPinCo2":8,"vPinCo2Min":9,"vPinCo2Max":10,"vPinCo2Alarm":11,"bmeType":1,"bmePar1":12,"bmePar2":14,"bmePar3":0,"bmePar4":0,"bmeUpdateInSeconds":2,"temperatureMin":19,"temperatureMax":31,"vPinTemperature":0,"vPinTemperatureMin":1,"vPinTemperatureMax":2,"vPinTemperatureAlarm":3,"humidityMin":20,"humidityMax":90,"vPinHumidity":4,"vPinHumidityMin":5,"vPinHumidityMax":6,"vPinHumidityAlarm":7,"pressureMin":700,"pressureMax":800,"vPinPressure":12,"vPinPressureMin":13,"vPinPressureMax":14,"vPinPressureAlarm":15}
````

Set parameter example:
````
curl -u config:config -X POST --data '{"postDataURL": "http://1.2.3.4/feed/"}' http://192.168.11.11/settings/
OK
````
Also take a look at the following script:
````
src/SensorsBox-firmware/test/set_settings_example.sh
````


To be continued...

---
Good luck!  
Alexey Tsarev, Tsarev.Alexey at gmail.com

# Smart LED Clock
Smart LED Clock<br>

Features:<br>
Wifi connected 12/24hour 8x32 Led Clock.<br>
Auto time syncronization via GPS and/or NTP.<br>
Weather updates, watches and warnings in the area (Based on GPS and/or IP Geolocation)<br>
Automatic timezone based on IP Geolocation<br>
Clock hue color change based on time of day<br>
Settings for static fixed GPS and Timezone settings.<br>
Web interface to change settings, setup wifi, OTA updates<br>
AP mode for initial setup.<br>

Components:<br>
ESP32<br>
8x32 WS2812B LED Matrix<br>
DS3231 RTC Module<br>
TSL2561 Light Luminosiy Sensor<br>
NEO-6M GPS Module<br>

Status LED:<br>
Red: Wifi offline, or old time data<br>
Yellow: Connecting to wifi<br>
BlueGreen: Wifi connected, not ntp sync yet<br>
Green: NTP sync<br>
Purple: GPS fix, no GPS time yet<br>
Blue: No GPS fix, recent GPS time within 10 min<br>
Off: GPS time sync within 10 min<br>

Disabled status led still shows red if time is not updated by ntp or gps in an hour<br>




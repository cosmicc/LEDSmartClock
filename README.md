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

Corner Status LED:<br>
Red: Wifi offline, or last time data is older then an hour (no ntp or gps source available)<br>
Magenta: LEDSmartClock in AP mode.
Yellow: Connecting to wifi<br>
BlueGreen: Wifi connected, no ntp or gps time sync yet<br>
Green: NTP sync within an hour<br>
Purple: GPS fix & no GPS time yet, or no GPS fix & last GPS time is greater then 10 min<br>
Blue: No GPS fix, last GPS time within 10 min<br>
Off: GPS fix & last gps time within 10 min<br>

Disabled status led still shows red if time is not updated by ntp or gps within an hour<br>




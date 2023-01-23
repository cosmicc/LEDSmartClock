#define HSPI_MOSI   23
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

#include <Preferences.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <IotWebConfTParameter.h>
#include <AceWire.h> 
#include <Wire.h>
#include <AceCommon.h>
#include <AceTime.h>
#include <AceTimeClock.h>
#include <esp_task_wdt.h>
#include <TinyGPSPlus.h>
#include <SPI.h>
#include <Tsl2561.h>
#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

// DO NOT USE DELAYS OR SLEEPS EVER! This breaks systemclock

#define DEBUG_GPS false            // Show gps debug serial messages
#define DEBUG_LIGHT false          // Show light debug serial messages
#define WDT_TIMEOUT 20             // Watchdog Timeout
#define GPS_BAUD 9600              // GPS UART gpsSpeed
#define GPS_RX_PIN 16              // GPS UART RX PIN
#define GPS_TX_PIN 17              // GPS UART TX PIN
#define DAYHUE 40                  // 6am daytime hue start
#define NIGHTHUE 175               // 10pm nighttime hue end
#define CONFIG_PIN 2               // Pin for the IotWebConf config pushbutton
#define mw 32                      // Width of led matrix
#define mh 8                       // Hight of led matrix
#define NUMMATRIX (mw*mh)

// second time aliases
#define T1S 1*1L  // 1 second
#define T1M 1*60L  // 1 minute
#define T5M 5*60L  // 5 minutes
#define T10M 10*60L  // 10 minutes
#define T1H 1*60*60L  // 1 hour
#define T2H 2*60*60L  // 2 hours 
#define T1D 24*60*60L  // 1 day
#define T1Y 365*24*60*60L  // 1 year

// AceTime refs
using namespace ace_time;
using ace_time::acetime_t;
//using ace_time::BasicZoneProcessor;
//using ace_time::TimeZone;
//using ace_time::ZonedDateTime;
using ace_time::clock::DS3231Clock;
using ace_time::clock::NtpClock;
using ace_time::clock::SystemClockLoop;

using WireInterface = ace_wire::TwoWireInterface<TwoWire>;
//static BasicZoneProcessor zoneProcessor;
static NtpClock ntpClock;

static const int CACHE_SIZE = 2;
static ExtendedZoneProcessorCache<CACHE_SIZE> zoneProcessorCache;
static ExtendedZoneManager manager(
    zonedbx::kZoneAndLinkRegistrySize,
    zonedbx::kZoneAndLinkRegistry,
    zoneProcessorCache);

static char intervals[][9] = {"31536000", "2592000", "604800", "86400", "3600", "60", "1"};
static char interval_names[][9] = {"yrs", "mon", "wks", "days", "hrs", "min", "sec"};

#include "src/structures/structures.h"
#include "src/colors/colors.h"
#include "src/bitmaps/bitmaps.h"

// Function Declarations
void wifiConnected();
void handleRoot();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);
void buildURLs();
String elapsedTime(int32_t seconds);
void net_getAlerts();
void net_getWeather();
void net_getIpgeo();

// Global Variables & Objects
const char thingName[] = "LedSmartClock";           // Default SSID used for new setup
const char wifiInitialApPassword[] = "setmeup";     // Default AP password for new setup
WireInterface wireInterface(Wire);                  // I2C hardware object
DS3231Clock<WireInterface> dsClock(wireInterface);  // Hardware DS3231 RTC object
SystemClockLoop systemClock(&ntpClock /*reference*/, &dsClock /*backup*/);  // reference & backup clock
CRGB leds[NUMMATRIX];           // Led matrix array object
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, mw, mh, NEO_MATRIX_BOTTOM+NEO_MATRIX_RIGHT+NEO_MATRIX_COLUMNS+NEO_MATRIX_ZIGZAG); // FastLED matrix object
TinyGPSPlus GPS;                // Hardware GPS object
Tsl2561 Tsl(Wire);              // Hardware Lux sensor object
DNSServer dnsServer;            // DNS Server object
WebServer server(80);           // Web server object for IotWebConf and OTA
Preferences preferences;        // ESP32 EEPROM preferences storage object
JSONVar weatherJson;            // JSON object for weather
JSONVar alertsJson;             // JSON object for alerts
JSONVar ipgeoJson;              // JSON object for ip geolocation
Weather weather;                // weather info data class
Alerts alerts;                  // wweather alerts data class
Ipgeo ipgeo;                    // ip geolocation data class
GPSData gps;                    // gps data class
String wurl;                    // Built Openweather API URL
String aurl;                    // Built AWS API URL
String gurl;                    // Built IPGeolocation.io API URL
uint16_t currbright;            // Current calculated brightness level
bool clock_display_offset;      // Clock display offset for single digit hour
uint16_t brightness_running_avg;// Running avg to smooth out brightness transitions
bool wifi_connected;            // Wifi is connected or not
bool displaying_alert;          // Alert is displaying or ready to display
bool displaying_weather;        // Weather is displaying 
bool colon;                     // colon on/off status
uint8_t currhue;                // Current calculated hue based on time of day
String currlat;
String currlon;
uint8_t temphue;                // Current calculated hue based on temp
TimeZone currtz;
uint8_t iconcycle;              // Current Weather animation cycle
acetime_t bootTime;            // boot time
char fixedTz[32];

uint32_t tstimer, debugTimer, wicon_time = 0L; // Delay timers

// IotWebConf User custom settings
// bool debugserial
// Group 1 (Display)
// uint8_t brightness_level;           // 1 low - 3 high
// uint8_t text_scroll_speed;          // 1 slow - 10 fast
// bool colonflicker;                  // flicker the colon ;)
// bool flickerfast;                   // flicker fast
// Group 2 (Weather)
// String weatherapi;                  // OpenWeather API Key
// uint8_t alert_check_interval;       // Time between alert checks
// uint8_t alert_show_interval;        // Time between alert displays
// uint8_t weather_check_interval;     // Time between weather checks
// uint8_t weather_show_interval;      // Time between weather displays
// Group 3 (Timezone)
// bool used_fixed_tz;                 // Use fixed Timezone
// String fixedTz;                     // Fixed Timezone
// Group 4 (Location)
// bool used_fixed_loc;                // Use fixed GPS coordinates
// String fixedLat;                    // Fixed GPS latitude
// String fixedLon;                    // Fixed GPS Longitude
// String ipgeoapi                     // IP Geolocation API

static char timezoneValues[][32] = { "kZoneUS_Eastern", "kZoneUS_Central", "kZoneUS_Mountain", "kZoneUS_Pacific", "kZoneUS_Hawaii", "kZoneUS_Aleutian", "kZoneUS_Alaska", "kZoneGMT"};
static char timezoneNames[][32] = { "US_Eastern", "US_Central", "US_Mountain", "US_Pacific", "US_Hawaii", "US_Atlantic", "US_Alaska", "GMT/UTC"};

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword);
  iotwebconf::CheckboxTParameter serialdebug =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("serialdebug").label("Debug to Serial").defaultValue(false).build();
iotwebconf::ParameterGroup group1 = iotwebconf::ParameterGroup("Display", "Display Settings");
  iotwebconf::IntTParameter<int8_t> brightness_level =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("brightness_level").label("Brightness Level (1-3)").defaultValue(2).min(1).max(3).step(1).placeholder("1(low)..3(high)").build();
  iotwebconf::IntTParameter<int8_t> text_scroll_speed =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("text_scroll_speed").label("Text Scroll Speed (1-10)").defaultValue(5).min(1).max(10).step(1).placeholder("1(low)..10(high)").build();
  iotwebconf::CheckboxTParameter colonflicker =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("colonflicker").label("Clock Colon Flicker").defaultValue(true).build();
  iotwebconf::CheckboxTParameter flickerfast =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("flickerfast").label("Fast Colon Flicker").defaultValue(true).build();
iotwebconf::ParameterGroup group2 = iotwebconf::ParameterGroup("Weather", "Weather Settings");
  iotwebconf::CheckboxTParameter fahrenheit =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("fahrenheit").label("Fahrenheit").defaultValue(true).build();
  iotwebconf::TextTParameter<33> weatherapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("weatherapi").label("OpenWeather API Key").defaultValue("").build();
 iotwebconf::IntTParameter<int8_t> alert_check_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("alert_check_interval").label("Weather Alert Check Interval Min (1-60)").defaultValue(5).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
 iotwebconf::IntTParameter<int8_t> alert_show_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("alert_show_interval").label("Weather Alert Display Interval Min (1-60)").defaultValue(5).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
 iotwebconf::IntTParameter<int8_t> weather_check_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("weather_check_interval").label("Weather Forecast Check Interval Min (1-60)").defaultValue(5).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
 iotwebconf::IntTParameter<int8_t> weather_show_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("weather_show_interval").label("Weather Forecast Display Interval Min (1-60)").defaultValue(5).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
iotwebconf::ParameterGroup group3 = iotwebconf::ParameterGroup("Timezone", "Timezone Settings");
  iotwebconf::CheckboxTParameter use_fixed_tz =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_tz").label("Use Fixed Timezone").defaultValue(false).build();
IotWebConfSelectParameter fixedTZ = IotWebConfSelectParameter("fixedTZ", "Fixed Timezone", fixedTz, 32, (char*)timezoneValues, (char*)timezoneNames, sizeof(timezoneValues) / 32, 32);
iotwebconf::ParameterGroup group4 = iotwebconf::ParameterGroup("Location", "Location Settings");
  iotwebconf::CheckboxTParameter use_fixed_loc =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_loc").label("Use Fixed Location").defaultValue(false).build();
  iotwebconf::TextTParameter<12> fixedLat =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLat").label("Fixed latitude").defaultValue("").build();
  iotwebconf::TextTParameter<12> fixedLon =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLon").label("Fixed Longitude").defaultValue("").build();
  iotwebconf::TextTParameter<33> ipgeoapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("ipgeoapi").label("IPGeolocation.io API Key").defaultValue("").build();

void debug_print(String message, bool cr) {
  if (serialdebug.isChecked())
    if (cr)
      Serial.println(message);
    else
      Serial.print(message);
}

String capString (String str) {
  	for(uint16_t i=0; str[i]!='\0'; i++)
	{
		if(i==0)
		{
			if((str[i]>='a' && str[i]<='z'))
				str[i]=str[i]-32;
			continue; 
		}
		if(str[i]==' ')
		{
			++i;
			if(str[i]>='a' && str[i]<='z')
			{
				str[i]=str[i]-32; 
				continue; 
			}
		}
		else
		{
			if(str[i]>='A' && str[i]<='Z')
				str[i]=str[i]+32; 
		}
	}
  return str;
}

  void processTimezone()
  {
    //String otz = preferences.getString("timezone", "");
    //if (otz.length() < 0)
    //{ // new setup no timezone saved, default to GMT
    //  debug_print("No timezone saved, saving default GMT");
    //  preferences.putString("timezone", "kZoneGMT");
    //  currtz = manager.createForZoneInfo(&zonedbx::kZoneGMT);
    //}
    //else {
      //String geotz = String(ipgeo.timezone);
      //String ntz;
      //if (geotz == "EST") ntz = "kZoneUS_Eastern";
      //else if (geotz == "PST") ntz = "kZoneUS_Pacific";
      //else {
      //debug_print((String)"Timezone [" + geotz + "] is unknown, defaulting to GMT");
      currtz = manager.createForZoneInfo(&zonedbx::kZoneAmerica_Detroit);
      
    //  debug_print((String) "IP Geo timezone [" + ntz + "] is different then saved timezone [" + otz + "], saving new timezone");
    //  preferences.putString("timezone", ntz);
    //  currtz = ntz;
    }

time_t gpsunixtime() {
  struct tm t;
  t.tm_year = GPS.date.year() - 1900;
  t.tm_mon = GPS.date.month() - 1;
  t.tm_mday = GPS.date.day();
  t.tm_hour = GPS.time.hour();
  t.tm_min = GPS.time.minute();
  t.tm_sec = GPS.time.second();
  t.tm_isdst = -1;
  time_t t_of_day;
  t_of_day = mktime(&t);
  return t_of_day;
}

void processLoc(){
    if (use_fixed_loc.isChecked())
  {
    gps.lat = fixedLat.value();
    gps.lon = fixedLon.value();
    currlat = fixedLat.value();
    currlon = fixedLon.value();
  }
  String savedlat = preferences.getString("lat", "");
  String savedlon = preferences.getString("lon", "");
  if ((gps.lon).length() == 0 && ((String)ipgeo.lon).length() == 0) {
    currlat = savedlat;
    currlon = savedlon;
  }
  else if ((gps.lon).length() == 0 && ((String)ipgeo.lon).length() != 0 && currlon.length() == 0) {
    //strcpy(currlat, ipgeo.lat);
    //strcpy(currlon, ipgeo.lon);
  } else if ((gps.lon).length() != 0) {
    currlat = gps.lat;
    currlon = gps.lon;
  }
  double nlat = currlat.toDouble();
  double nlon = currlon.toDouble();
  double olat = savedlat.toDouble();
  double olon = savedlon.toDouble();
  if (abs(nlat - olat) > 0.02 || abs(nlon - olon) > 0.02) {
    debug_print("Major location shift, saving values", true);
    debug_print((String) "Lat: ["+olat+"] -> ["+nlat+"] Lon: ["+olon+"] -> ["+nlon+"]", true);
    preferences.putString("lat", currlat);
    preferences.putString("lon", currlon);
  }
  buildURLs();
}

void gps_checkData() {
  gps.lastcheck = systemClock.getNow();
  while (Serial1.available() > 0)
    GPS.encode(Serial1.read());
  if (GPS.time.isUpdated())
  {
    time_t GPStime = gpsunixtime();
    if (DEBUG_GPS)
      debug_print((String) "GPS Time updated: " + GPStime, true);
  }
    if (GPS.satellites.isUpdated()) {
      gps.sats = GPS.satellites.value();
        if (DEBUG_GPS) debug_print((String)"GPS Satellites updated: " + gps.sats, true);
      if (gps.sats > 0 && !gps.fix)
      {
        gps.fix = true;
        gps.timestamp = systemClock.getNow();
        if (DEBUG_GPS) debug_print((String)"GPS fix aquired with "+gps.sats+" satellites", true);
      }
      if (gps.sats == 0 && gps.fix) {
        gps.fix = false;
        if (DEBUG_GPS) debug_print("GPS fix lost, no satellites", true);
      }
    }
    if (GPS.location.isUpdated()) {
      if (!use_fixed_loc.isChecked()) {
        double lat = GPS.location.lat();
        double lon = GPS.location.lng();
        gps.lockage = systemClock.getNow();
        processLoc();
      }
      else {
        gps.lat = fixedLat.value();
        gps.lon = fixedLon.value();
        processLoc();
      }
      if (DEBUG_GPS) debug_print((String)"GPS Location updated: Lat: " + gps.lat + " Lon: " + gps.lon, true);
    }
    if (GPS.altitude.isUpdated()) {
      gps.elevation = GPS.altitude.feet();
      if (DEBUG_GPS) debug_print((String)"GPS Elevation updated: " + gps.elevation, true);
    }
    if (GPS.hdop.isUpdated()) {
      gps.hdop = GPS.hdop.hdop();
      if (DEBUG_GPS) debug_print((String)"GPS HDOP updated: " + gps.hdop, true);
    }
    if (GPS.charsProcessed() < 10) debug_print("WARNING: No GPS data.  Check wiring.", true);
}

void display_setclockDigit(uint8_t bmp_num, uint8_t position, uint16_t color) { 
    if (position == 0) position = 0;
    else if (position == 1) position = 7;
    else if (position == 2) position = 16;
    else if (position == 3) position = 23;
    if (clock_display_offset) 
      matrix->drawBitmap(position-4, 0, num[bmp_num], 8, 8, color);    
    else
      matrix->drawBitmap(position, 0, num[bmp_num], 8, 8, color); 
}

void display_setBrightness() {
  Tsl.begin();
  if( Tsl.available() ) {
    Tsl.on();
    Tsl.setSensitivity(true, Tsl2561::EXP_14);
    uint16_t full;
    uint32_t bloop = millis();
    while (millis() - bloop < 16) {
      esp_task_wdt_reset();
      systemClock.loop();
      iotWebConf.doLoop();
    }
    Tsl.fullLuminosity(full);
    uint8_t min, max;
    min = 5;
    max = 100;
    uint16_t cb = map(full, 0, 500, min, max);
    if (cb < min)
      cb = min;
    else if (cb > max)
      cb = max;
    if (brightness_level.value() == 2)
      cb =+ 5;
    else if (brightness_level.value() == 3)
      cb =+ 15;
    currbright = cb;
    brightness_running_avg = (brightness_running_avg + currbright) / 2;
    if (DEBUG_LIGHT) debug_print((String)"Lux: " + full + " brightness: " + currbright + " avg: " + brightness_running_avg, true);
    Tsl.off();
    matrix->setBrightness(brightness_running_avg);
    matrix->show();
  }
  else {
    if (millis() - tstimer > 10000) {
      debug_print((String)"No Tsl2561 found. Check wiring: SCL=" + TSL2561_SCL + " SDA=" + TSL2561_SDA, true);
      tstimer = millis();
    }
  }
}

void display_setStatus() {
  uint16_t clr;
  uint16_t wclr;
  if (gps.fix) {
    clr = BLACK;
  }
  else if (wifi_connected) {
    clr = DARKGREEN;
  }
  else {
    clr = DARKRED;
  }
  if (alerts.inWarning) {
    wclr = RED;
  }
  else if (alerts.inWatch) {
    wclr = YELLOW;
  }
  else {
    wclr = BLACK;
  }
  matrix->drawPixel(0, 7, clr);
  matrix->drawPixel(0, 0, wclr);
}

void display_alertFlash(uint16_t clr, uint8_t laps) {
  debug_print("Displaying alert flash", true);
  uint32_t flashmilli;
  uint8_t flashcycles = 0;
  flashmilli = millis();
  while (millis() - flashmilli < 250)
    runMaintenance();
  while (flashcycles < laps)
  {
    flashmilli = millis();
    while (millis() - flashmilli < 250)
      runMaintenance();
    matrix->clear();
    matrix->fillScreen(clr);
    matrix->show();

    flashmilli = millis();
    while (millis() - flashmilli < 250)
      runMaintenance();
    matrix->clear();
    matrix->show();
    flashcycles++;
  }
  flashmilli = millis();
  while (millis() - flashmilli < 250)
    runMaintenance();
}

void display_scrollWeather(String message, uint8_t spd, uint16_t clr) {
  debug_print((String) "Scrolling weather text: " + message, true);
  uint8_t speed = map(spd, 1, 10, 100, 10);
  uint16_t size = message.length() * 6;
  uint32_t scrollmilli;
  for (int16_t x = mw-9; x >= size - size - size; x--)
  {
    scrollmilli = millis();
    while (millis() - scrollmilli < speed)
    {
    runMaintenance();
    matrix->clear();
    display_setStatus();
    matrix->setCursor(x, 1);
    matrix->setTextColor(clr);
    matrix->print(capString(message));
    display_weatherIcon();
    matrix->show();
    }
  }
    debug_print("Scrolling text complete.", true);
}

void display_scrollText(String message, uint8_t spd, uint16_t clr) {
  debug_print((String) "Scrolling fullscreen text: " + message, true);
  uint8_t speed = map(spd, 1, 10, 100, 10);
  uint16_t size = message.length() * 6;
  uint32_t scrollmilli;
  for (int16_t x = mw; x >= size - size - size; x--)
  {
    scrollmilli = millis();
    while (millis() - scrollmilli < speed)
    {
      runMaintenance();
      matrix->clear();
      display_setStatus();
      matrix->setCursor(x, 1);
      matrix->setTextColor(clr);
      matrix->print(message);
      matrix->show();
    }
  }
    debug_print("Scrolling text complete.", true);
}

void display_weather() {
    display_scrollWeather(weather.currentDescription, text_scroll_speed.value(), WHITE);
    uint32_t lp = millis();
    while (millis() - lp < 10000) {
      matrix->clear();
      display_weatherIcon();
      display_temperature();
      display_setStatus();
      matrix->show();
      runMaintenance();
    }
}

void display_weatherIcon() {
    bool night;
    if (weather.currentIcon[2] == 'n')
      night = true;
    char code1;
    char code2;
    code1 = weather.currentIcon[0];
    code2 = weather.currentIcon[1];

      if (code1 == '0' && code2 == '1') {  //clear
        if (night == true)
          matrix->drawRGBBitmap(mw-8, 0, clear_night[iconcycle], 8, 8);
        else
          matrix->drawRGBBitmap(mw-8, 0, clear_day[iconcycle], 8, 8);
      }
      else if ((code1 == '0' && code2 == '2') || (code1 == '0' && code2 == '3')) {  //partly cloudy
        if (night == true)
          matrix->drawRGBBitmap(mw-8, 0, partly_cloudy_night[iconcycle], 8, 8);
        else
          matrix->drawRGBBitmap(mw-8, 0, partly_cloudy_day[iconcycle], 8, 8);
      }
      else if ((code1 == '0') && (code2 == '4')) // cloudy
        matrix->drawRGBBitmap(mw-8, 0, cloudy[iconcycle], 8, 8);
      else if (code1 == '5' && code2 == '0')  //fog/mist 
        matrix->drawRGBBitmap(mw-8, 0, fog[iconcycle], 8, 8);
      else if (code1 == '1' && code2 == '3')  //snow
        matrix->drawRGBBitmap(mw-8, 0, snow[iconcycle], 8, 8);
      else if (code1 == '0' && code2 == '9') //rain
        matrix->drawRGBBitmap(mw-8, 0, rain[iconcycle], 8, 8);
      else if (code1 == '1' && code2 == '0') // heavy raid
        matrix->drawRGBBitmap(mw-8, 0, heavy_rain[iconcycle], 8, 8);
      else if (code1 == '1' && code2 == '1') //thunderstorm
        matrix->drawRGBBitmap(mw-8, 0, thunderstorm[iconcycle], 8, 8);
      else
        debug_print("No Weather icon found to use", true);

    if (millis() - wicon_time > 250)
    {
    if (iconcycle == 4)
      iconcycle = 0;
    else
      iconcycle++;
    wicon_time = millis();
    }
}

void display_temperature() {
    int temp;
    if (fahrenheit.isChecked())
    {
    temp = weather.currentFeelsLike * 1.8 + 32;
    }
    else
    temp = weather.currentFeelsLike;
    int xpos;
    int digits = (abs(temp == 0)) ? 1 : (log10(abs(temp)) + 1);
    bool isneg = false;
    if (temp + abs(temp) == 0) 
    {
      isneg = true;
      digits++;
    }
    if (digits == 3)
    xpos = 1;
    else if (digits == 2)
    xpos = 5;
    else if (digits == 1)
    xpos = 9;
    xpos = xpos * (digits);
    int score = abs(temp);
    temphue = map(weather.currentFeelsLike, 0, 40, NIGHTHUE, 0);
    if (temphue < 0)
      temphue = 0;
    else if (temphue > NIGHTHUE)
      temphue = NIGHTHUE;
    while (score)
    {
    matrix->drawBitmap(xpos, 0, num[score % 10], 8, 8, hsv2rgb(temphue));
    score /= 10;
    xpos = xpos - 7;
    }
    if (isneg == true) 
    {
      matrix->drawBitmap(xpos, 0, sym[1], 8, 8, hsv2rgb(temphue));
      xpos = xpos - 7;
    }
    matrix->drawBitmap(xpos+(digits*7)+7, 0, sym[0], 8, 8, hsv2rgb(temphue));
}

void display_setHue() { 
  ace_time::ZonedDateTime ldt = getSystemTime();
  uint8_t myHours = ldt.hour();
  uint8_t myMinutes = ldt.minute();
    if (myHours * 60 >= 1320)
      currhue = NIGHTHUE;
    else if (myHours * 60 < 360)
      currhue = NIGHTHUE;
    else {
      uint8_t thue = map(myHours * 60 + myMinutes, 360, 1319, DAYHUE, NIGHTHUE);
      if (thue > NIGHTHUE)
        currhue = NIGHTHUE;
      else if (thue < DAYHUE)
        currhue = DAYHUE;
      else
        currhue = thue;
    }
}

void display_showClock() {
  ace_time::ZonedDateTime ldt = getSystemTime();
  display_setHue();
  matrix->clear();
  uint8_t myhours = ldt.hour();
  uint8_t myminute = ldt.minute();
  uint8_t myhour = myhours % 12;
  if (myhour/10 == 0 && myhour%10 == 0) {
    display_setclockDigit(1, 0, hsv2rgb(currhue));
    display_setclockDigit(2, 1, hsv2rgb(currhue));
    clock_display_offset = false;
  } 
  else {
    if (myhour / 10 != 0) {
      clock_display_offset = false;
      display_setclockDigit(myhour / 10, 0, hsv2rgb(currhue)); // set first digit of hour
    } else {
      clock_display_offset = true;
    }
    display_setclockDigit(myhour%10, 1, hsv2rgb(currhue)); // set second digit of hour
  }
  display_setclockDigit(myminute/10, 2, hsv2rgb(currhue)); // set first digig of minute
  display_setclockDigit(myminute%10, 3, hsv2rgb(currhue));  // set second digit of minute
  if (!colonflicker.isChecked()) {
    if (clock_display_offset) {
      matrix->drawPixel(12, 5, hsv2rgb(currhue)); // draw colon
      matrix->drawPixel(12, 2, hsv2rgb(currhue)); // draw colon
    } else {
      matrix->drawPixel(16, 5, hsv2rgb(currhue)); // draw colon
      matrix->drawPixel(16, 2, hsv2rgb(currhue)); // draw colon     
    }
  }
  display_setStatus();
  matrix->show();
}

void printSystemTime() {
  acetime_t now = systemClock.getNow();
  auto TimeWZ = ZonedDateTime::forEpochSeconds(now, currtz);
  TimeWZ.printTo(SERIAL_PORT_MONITOR);
  SERIAL_PORT_MONITOR.println();
}

ace_time::ZonedDateTime getSystemTime() {
  acetime_t now = systemClock.getNow();
  ace_time::ZonedDateTime TimeWZ = ZonedDateTime::forEpochSeconds(now, currtz);
  return TimeWZ;
}

void wifiConnected() {
  debug_print("WiFi was connected.", true);
  wifi_connected = true;
  ntpClock.setup();
  net_getIpgeo();
  net_getWeather();
  if (alert_check_interval.value() != 0)
    net_getAlerts();
}

void runMaintenance() {
  esp_task_wdt_reset();
  systemClock.loop();
  iotWebConf.doLoop();
  display_setBrightness();
}

void loop() {
  runMaintenance();
  acetime_t now = systemClock.getNow();
  if (abs(now - bootTime) > (T1Y - 60)) {
    debug_print("1 year system reset!", true);
    ESP.restart();
  }
  if (displaying_alert) // Scroll weather alert
    {
      uint16_t acolor;
      if (alerts.inWarning)
        acolor = RED;
      else if (alerts.inWatch)
        acolor = YELLOW;
      display_alertFlash(acolor, 3);
      display_scrollText(alerts.description1, text_scroll_speed.value(), acolor);
      alerts.lastshown = systemClock.getNow();
      displaying_alert = false;
    }
  else if (displaying_weather)  // show weather forcast
  {
    display_alertFlash(BLUE, 1);
    display_weather();
    weather.lastshown = systemClock.getNow();
    displaying_weather = false;
  }
  else { // Show the clock
    display_showClock();
    ace_time::ZonedDateTime ldt = getSystemTime();
    uint32_t mysecs = ldt.second();
    colon = true;
    uint32_t flickertime = millis();
    uint16_t fstop;
    if (flickerfast.isChecked()) 
        fstop = 20;
    else
        fstop = 250;
    while (mysecs == ldt.second())
    { // now wait until seconds change
        if (colon && colonflicker.isChecked() && millis() - flickertime > fstop)
        {
          if (clock_display_offset)
          {
            matrix->drawPixel(12, 5, hsv2rgb(currhue)); // draw colon
            matrix->drawPixel(12, 2, hsv2rgb(currhue)); // draw colon
          } else {
          matrix->drawPixel(16, 5, hsv2rgb(currhue)); // draw colon
          matrix->drawPixel(16, 2, hsv2rgb(currhue)); // draw colon     
        }
        matrix->show();
        colon = false;
      }
        ldt = getSystemTime();
        runMaintenance();
    }
    }
  // looping tasks start here
  if (abs(now - gps.lastcheck > 1)) { // Check for GPS data
    if (DEBUG_GPS)
        debug_print("checking for new GPS data", true);
    gps_checkData();
  }
// check weather forcast
  if ((abs(now - weather.lastsuccess) > (weather_check_interval.value()*T1M)) && weather_check_interval.value() != 0)
  { 
    if (abs(now - weather.lastattempt) > T1M)
      net_getWeather();
  }
// Check weather alerts
//Serial.println((String)"Alerts: "+(now - alerts.lastshown));
  if ((abs(now - alerts.lastsuccess) > (alert_check_interval.value()*T1M)) && alert_check_interval.value() != 0) 
  { 
    if (abs(now - alerts.lastattempt) > T1M)
      net_getAlerts();
  }
// show weather alerts
  if ((abs(now - alerts.lastshown) > (alert_show_interval.value()*T1M)) && alert_show_interval.value() != 0) 
  {  
    if (alerts.active)
      displaying_alert = true;
  }
// show weather forcast
    //Serial.println((String) "Weather: "+(now - weather.lastshown));
    //Serial.println((String) "WLS: "+(weather.lastshown));
    //Serial.println((String) "Now: " + now);
    if ((abs(now - weather.lastshown) > (weather_show_interval.value() * T1M)) && weather_show_interval.value() != 0)
    {

    if (abs(now - weather.timestamp) < T2H)
      displaying_weather = true;
  }
// debug info
  if (serialdebug.isChecked()) {
    if (millis() - debugTimer > 10000) { 
      debugTimer = millis();
      debug_print("----------------------------------------------------------------", true);
      String gage = elapsedTime(now - gps.timestamp);
      String loca = elapsedTime(now - gps.lockage);
      String uptime = elapsedTime(now - bootTime);
      String igt = elapsedTime(now - ipgeo.timestamp);
      String wage = elapsedTime(now - weather.timestamp);
      String wlt = elapsedTime(now - weather.lastattempt);
      String alt = elapsedTime(now - alerts.lastattempt);
      String wls = elapsedTime(now - weather.lastshown);
      String als = elapsedTime(now - alerts.lastshown);
      String alg = elapsedTime(now - alerts.lastsuccess);
      String wlg = elapsedTime(now - weather.lastsuccess);
      String lst = elapsedTime(now - systemClock.getLastSyncTime());
      Serial.println(now);
      debug_print((String) "System - Brightness:" + currbright + " | ClockHue:" + currhue + " | TempHue:" + temphue + " | Uptime:" + uptime, true);
      debug_print((String) "Clock - Status:" + systemClock.getSyncStatusCode() + " | LastSync:" + lst + " | LastAttempt:" + elapsedTime(systemClock.getSecondsSinceSyncAttempt()) +" sec | NextAttempt:" + elapsedTime(systemClock.getSecondsToSyncAttempt()) +" sec | Skew:" + systemClock.getClockSkew() + " sec", true);
      debug_print((String) "Loc - SavedLat:" + preferences.getString("lat", "") + ", SavedLon:" + preferences.getString("lon", "") + " | CurrentLat:" + currlat + " | CurrentLon:" + currlon, true);
      debug_print((String) "IPGeo - Lat:" + ipgeo.lat + " | Lon:" + ipgeo.lon + " | Timezone:" + ipgeo.timezone + " | Age:" + igt, true);
      debug_print((String) "GPS - Chars:" + GPS.charsProcessed() + " | With-Fix:" + GPS.sentencesWithFix() + " | Failed:" + GPS.failedChecksum() + " | Passed:" + GPS.passedChecksum() + " | Sats:" + gps.sats + " | Hdop:" + gps.hdop + " | Elev:" + gps.elevation + " | Lat:" + gps.lat + " | Lon:" + gps.lon + " | FixAge:" + gage + " | LocAge:" + loca, true);
      debug_print((String) "Weather - Icon:" + weather.currentIcon + " | Temp:" + weather.currentFeelsLike + " | Humidity:" + weather.currentHumidity + " | Desc:" + weather.currentDescription + " | LastAttempt:" + wlt + " | LastSuccess:" + wlg + " | LastShown:" + wls + " | Age:" + wage, true);
      debug_print((String) "Alerts - Active:" + alerts.active + " | Watch:" + alerts.inWatch + " | Warn:" + alerts.inWarning + " | LastAttempt:" + alt + " | LastSuccess:" + alg + " | LastShown:" + als, true);
      if (alerts.active)
        debug_print((String) "*Alert1 Status:" + alerts.status1 + " | Severity:" + alerts.severity1 + " | Certainty:" + alerts.certainty1 + " | Urgency:" + alerts.urgency1 + " | Event:" + alerts.event1 + " | Desc:" + alerts.description1, true);
    }
  }
}

void setup () {
  Serial.begin(115200);
  debug_print("Initializing Hardware Watchdog...", false);
  esp_task_wdt_init(WDT_TIMEOUT, true);                //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                              //add current thread to WDT watch
  debug_print("COMPLETE.", true);
  debug_print("Reading stored location and timezone...", false);
  preferences.begin("location", false);
  debug_print("COMPLETE.", true);
  debug_print("Initializing IotWebConf...", true);
  group1.addItem(&brightness_level);
  group1.addItem(&text_scroll_speed);
  group1.addItem(&colonflicker);
  group1.addItem(&flickerfast);
  group2.addItem(&fahrenheit);
  group2.addItem(&weatherapi);
  group2.addItem(&alert_check_interval);
  group2.addItem(&weather_check_interval);
  group2.addItem(&alert_show_interval);
  group2.addItem(&weather_show_interval);
  group3.addItem(&use_fixed_tz);
  group3.addItem(&fixedTZ);
  group4.addItem(&use_fixed_loc);
  group4.addItem(&fixedLat);
  group4.addItem(&fixedLon);
  group4.addItem(&ipgeoapi);
  iotWebConf.addSystemParameter(&serialdebug);
  iotWebConf.addParameterGroup(&group1);
  iotWebConf.addParameterGroup(&group2);
  iotWebConf.addParameterGroup(&group3);
  iotWebConf.addParameterGroup(&group4);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.getApTimeoutParameter()->visible = true;
  //iotWebConf.setStatusPin(STATUS_PIN);
  //iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.init();
  gurl = (String)"https://api.ipgeolocation.io/ipgeo?apiKey=" + ipgeoapi.value();
  processTimezone();
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });
  debug_print("IotWebConf is READY.", true);
  debug_print("Initializing the RTC module...", false);
  Wire.begin();
  wireInterface.begin();
  systemClock.setup();
  dsClock.setup();
  debug_print("COMPLETE.", true);
  debug_print("System Clock: ", false);
  printSystemTime();
  debug_print("Initializing Light Sensor...", false);
  Wire.begin(TSL2561_SDA, TSL2561_SCL);
  debug_print("COMPLETE.", true);
  if (use_fixed_loc.isChecked())
    debug_print((String)"Setting Fixed GPS Location Lat: " + fixedLat.value()+ " Lon: " + fixedLon.value(), true);
  processLoc();
  debug_print("Initializing the display...", true);
  display_setHue();
  runMaintenance();
  FastLED.addLeds<NEOPIXEL, HSPI_MOSI>(leds, NUMMATRIX).setCorrection(TypicalLEDStrip);
  matrix->begin();
  matrix->setTextWrap(false);
  matrix->setBrightness(currbright);
  debug_print("Display initalization complete.", true);
  debug_print("Initializing GPS Module...", false);
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // Initialize GPS UART
  debug_print("COMPLETE.", true);
  bootTime = systemClock.getNow();
  debugTimer, wicon_time, tstimer = millis(); // reset all delay timers
  ipgeo.timestamp, gps.timestamp, gps.lastcheck, gps.lockage, alerts.timestamp, alerts.lastattempt, alerts.lastshown, alerts.lastsuccess, weather.timestamp, weather.lastattempt, weather.lastshown, weather.lastsuccess = bootTime - T1Y + 60;
}

void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>Smart Led Matrix Clock</title></head><body>Hello world!";
  s += "<ul>";
  s += "<li>Display Brightness: ";
  s += brightness_level.value();
  s += "<li>Text Scroll Speed: ";
  s += text_scroll_speed.value();
  s += "<li>Clock Colon Flicker: ";
  s += colonflicker.isChecked();
  s += "<li>Fast Flicker: ";
  s += flickerfast.isChecked();
  s += "<li>Weather Alert Check Interval Min: ";
  s += alert_check_interval.value();
  s += "<li>Use Fixed Location: ";
  s += use_fixed_loc.isChecked();
  s += "<li>Fixed latitude: ";
  s += fixedLat.value();
  s += "<li>Fixed Longitude: ";
  s += fixedLon.value();
  s += "</ul>";
  s += "Go to <a href='config'>configure page</a> to change values.";
  s += "</body></html>\n";
  server.send(200, "text/html", s);
}

void configSaved()
{
  buildURLs();
  Serial.println("Configuration was updated.");
  if (!serialdebug.isChecked())
    Serial.println("Serial debugging has been disabled.");
}

bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  debug_print("Validating web form...", true);
  bool valid = true;

/*
  int l = webRequestWrapper->arg(stringParam.getId()).length();
  if (l < 3)
  {
    stringParam.errorMessage = "Please provide at least 3 characters for this test!";
    valid = false;
  }
*/
  return valid;
}

boolean net_getAlertJSON() {
  boolean success = false;
  debug_print("Connecting to ", false);
  debug_print(aurl, true);
  HTTPClient http;
  runMaintenance();
  http.begin(aurl);
  int httpCode = http.GET();
  debug_print("HTTP code : ", false);
  debug_print((String)httpCode, true);
  if (httpCode == 200)
  {
    alertsJson = JSON.parse(http.getString());
    if (JSON.typeof(alertsJson) == "undefined") {
      debug_print("Parsing alertJson input failed!", true);
    } else {
      success = true;
    }
  } else {
    display_alertFlash(RED, 2);
    display_scrollText((String) "Error getting weather alerts: " + httpCode, text_scroll_speed.value(), RED);
  }
    http.end(); 
  return success;
}

void fillAlertsFromJson(Alerts* alerts) {
  if (alertsJson["features"].length() != 0)
  {
    sprintf(alerts->status1, "%s", (const char *)alertsJson["features"][0]["properties"]["status"]);
    sprintf(alerts->severity1, "%s", (const char *)alertsJson["features"][0]["properties"]["severity"]);
    sprintf(alerts->certainty1, "%s", (const char *)alertsJson["features"][0]["properties"]["certainty"]);
    sprintf(alerts->urgency1, "%s", (const char *)alertsJson["features"][0]["properties"]["urgency"]);
    sprintf(alerts->event1, "%s", (const char *)alertsJson["features"][0]["properties"]["event"]);
    sprintf(alerts->description1, "%s", (const char *)alertsJson["features"][0]["properties"]["parameters"]["NWSheadline"][0]);
    alerts->timestamp = systemClock.getNow();
    //debug_print("Sizeof:  ");
    //debug_print(sizeof(alertsJson["features"]));
    //debug_print(alertsJson["features"]);
    //debug_print("Sizeof:  ");
    //debug_print(sizeof(alertsJson["features"][0]));
    //debug_print(alertsJson["features"][0]);
    
    if ((String)alerts->certainty1 == "Observed" || (String)alerts->certainty1 == "Likely")
    {
      alerts->inWarning = true;
      alerts->active = true;
      displaying_alert = true;
    }
    else if ((String)alerts->certainty1 == "Possible") {
      alerts->inWatch= true;
      alerts->active = true;
      displaying_alert = true;
    } 
    else {
      alerts->active = false;
      alerts->inWarning = false;
      alerts->inWatch= false;     
    }
    debug_print("Alert found", true);
  }
  else {
    alerts->active = false;
    debug_print("No Alerts found", true);
  }
}

void net_getAlerts() {
  if (wifi_connected && (currlat).length() > 1) {
    debug_print("Checking weather alerts...", true);
    int64_t retries = 1;
    boolean jsonParsed = false;
    while (!jsonParsed && (retries-- > 0))
    {
      //delay(1000);
      jsonParsed = net_getAlertJSON();
    }
    runMaintenance();
    if (!jsonParsed)
    {
      debug_print("Error: JSON", true);
    }
    else
    {
      fillAlertsFromJson(&alerts);
      debug_print("Weather alert check complete.", true);
      alerts.lastsuccess = systemClock.getNow();
    }
  } else {
    debug_print("Weather alert check skipped, no wifi or loc.", true);
  }
  alerts.lastattempt = systemClock.getNow();
}

boolean net_getWeatherJSON() {
  boolean success = false;
  debug_print("Connecting to ", false);
  debug_print(wurl, true);
  HTTPClient http;
  runMaintenance();
  http.begin(wurl);
  int httpCode = http.GET();
  debug_print("HTTP code : ", false);
  debug_print((String)httpCode, true);
  runMaintenance();
  if (httpCode == 200)
  {
    weatherJson = JSON.parse(http.getString());
    if (JSON.typeof(weatherJson) == "undefined") {
      debug_print("Parsing weatherJson input failed!", true);
    } else {
      success = true;
    }
  }
  else if (httpCode == 401) {
    display_alertFlash(RED, 2);
    display_scrollText("Invalid Openweathermap.org API Key", text_scroll_speed.value(), RED);
  } else {
    display_alertFlash(RED, 2);
    display_scrollText((String)"Error getting weather forcast: " +httpCode, text_scroll_speed.value(), RED);
  }
    http.end(); 
  return success;
}

void net_fillWeatherValues(Weather* weather) {
  sprintf(weather->currentIcon, "%s", (const char*) weatherJson["current"]["weather"][0]["icon"]);
  weather->currentTemp = weatherJson["current"]["temp"];
  weather->currentFeelsLike = weatherJson["current"]["feels_like"];
  weather->currentHumidity = weatherJson["current"]["humidity"];
  weather->currentWindSpeed = weatherJson["current"]["wind_speed"];
  sprintf(weather->currentDescription, "%s", (const char*) weatherJson["current"]["weather"][0]["description"]);
  //sprintf(weather->iconD, "%s", (const char*) weatherJson["daily"][0]["weather"][0]["icon"]);
  //weather->tempMinD = weatherJson["daily"][0]["temp"]["min"];
  //weather->tempMaxD = weatherJson["daily"][0]["temp"]["max"];
  //weather->humidityD = weatherJson["daily"][0]["humidity"];
  //weather->windSpeedD = weatherJson["daily"][0]["wind_speed"];
  //weather->windGustD = weatherJson["daily"][0]["wind_gust"];
  //sprintf(weather->descriptionD, "%s", (const char*) weatherJson["daily"][0]["weather"][0]["description"]);
  //sprintf(weather->iconD1, "%s", (const char*) weatherJson["daily"][1]["weather"][0]["icon"]);
  //weather->tempMinD1 = weatherJson["daily"][1]["temp"]["min"];
  //weather->tempMaxD1 = weatherJson["daily"][1]["temp"]["max"];
  //weather->humidityD1 = weatherJson["daily"][1]["humidity"];
  //weather->windSpeedD1 = weatherJson["daily"][1]["wind_speed"];
  //weather->windGustD1 = weatherJson["daily"][1]["wind_gust"];
  //sprintf(weather->descriptionD1, "%s", (const char*) weatherJson["daily"][1]["weather"][0]["description"]);
  weather->timestamp = systemClock.getNow();
  
}

void net_getWeather() {
  if (wifi_connected && (currlat).length() > 1 && isApiKeyValid(weatherapi.value())) {
    debug_print("Checking weather forcast...", true);
    int64_t retries = 1;
    boolean jsonParsed = false;
    while (!jsonParsed && (retries-- > 0))
    {
      //delay(1000);
      jsonParsed = net_getWeatherJSON();
    }
    if (!jsonParsed) {
      debug_print("Error: JSON", true);
    } else {
      
      net_fillWeatherValues(&weather);
      debug_print("Weather forcast check complete.", true);
      weather.lastsuccess = systemClock.getNow();
    }
  } else {
    debug_print("Weather forcast check skipped, no wifi, loc, apikey", true);
  }
  weather.lastattempt = systemClock.getNow();
}

boolean getIpgeoJSON() {
  boolean success = false;
  debug_print("Connecting to ", false);
  debug_print(gurl, true);
  HTTPClient http;
  runMaintenance();
  http.begin(gurl);
  int httpCode = http.GET();
  debug_print("HTTP code : ", false);
  debug_print((String)httpCode, true);
  runMaintenance();
  if (httpCode == 200)
  {
    ipgeoJson = JSON.parse(http.getString());
    if (JSON.typeof(ipgeoJson) == "undefined") {
      debug_print("Parsing IPGeoJson input failed!", true);
    } else {
      success = true;
    }
  }
  else if (httpCode == 401) {
    display_alertFlash(RED, 2);
    display_scrollText("Invalid IPGeolocation.io API Key", text_scroll_speed.value(), RED);
  } else {
    display_alertFlash(RED, 2);
    display_scrollText((String)"Error getting IPGeolocation data: " +httpCode, text_scroll_speed.value(), RED);
  }
    http.end(); 
  return success;
}

void fillIpgeoFromJson(Ipgeo* ipgeo) {
  sprintf(ipgeo->timezone, "%s", (const char*) ipgeoJson["time_zone"]["name"]);
  sprintf(ipgeo->lat, "%s", (const char*) ipgeoJson["latitude"]);
  sprintf(ipgeo->lon, "%s", (const char*) ipgeoJson["longitude"]);
  ipgeo->timestamp = systemClock.getNow();
}

void net_getIpgeo() {
  if (wifi_connected && isApiKeyValid(ipgeoapi.value()))
  {
    debug_print("Checking IP geolocation..", true);
    int64_t retries = 1;
    bool jsonParsed = false;
    while (!jsonParsed && (retries-- > 0))
    {
      //delay(1000);
      jsonParsed = getIpgeoJSON();
    }
    if (!jsonParsed) {
      debug_print("Error: JSON", true);
    } else {
      fillIpgeoFromJson(&ipgeo);
      debug_print("IP Geolocation check complete.", true);
      processTimezone();
      processLoc();
    }
  }
  else
  {
    debug_print("IP Geolocation check skipped, no wifi or apikey", true);
  }
}

bool isApiKeyValid(char *apikey) {
  if (apikey[0] == '\0')
    return false;
  else
    return true;
}

void buildURLs() {
  wurl = (String) "http://api.openweathermap.org/data/2.5/onecall?units=metric&exclude=minutely&appid=" + weatherapi.value() + "&lat=" + currlat + "&lon=" + currlon + "&lang=en";
  aurl = (String) "https://api.weather.gov/alerts?active=true&status=actual&point=" + currlat + "%2C" + currlon + "&limit=500";
}

String elapsedTime(int32_t seconds) {
  String result;
  uint8_t granularity;
  seconds = abs(seconds);
  uint32_t tseconds = seconds;
  if (seconds > T1Y)
    return "never";
  if (seconds > 60 && seconds < 3600)
    granularity = 1;
  else
    granularity = 2;
  uint32_t value;
  for (uint8_t i = 0; i < 6; i++)
  {
    uint32_t vint = atoi(intervals[i]);
    value = seconds / vint;
    value = floor(value);
    if (value != 0) {
      seconds = seconds - value * vint;
      if (granularity != 0) {
        result = result + value + " " + interval_names[i];
        if (granularity > 1)
          result = result + ", ";
        granularity--;
      }
    }
  }
  if (granularity > 0) 
    return (String) tseconds + " sec";
  else
    return result;
}
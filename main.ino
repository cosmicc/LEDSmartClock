// LedSmartClock for a 8x32 LED neopixel display
//
// Created by: Ian Perry (ianperry99@gmail.com)
// https://github.com/cosmicc/led_clock

// Emable SPI for FastLED
#define HSPI_MOSI   23
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

#include <nvs_flash.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <Preferences.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <IotWebConfTParameter.h>
#include <AceWire.h> 
#include <AceCommon.h>
#include <AceTime.h>
#include <AceRoutine.h>
#include <AceTimeClock.h>
#include <TinyGPSPlus.h>
#include <SPI.h>
#include <Tsl2561.h>
#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

// DO NOT USE DELAYS OR SLEEPS EVER! This breaks systemclock (Everything is coroutines now)

#define VERSION "1.0b"             // firmware version
static const char* CONFIGVER = "1";// config version (advance if iotwebconf config additions to reset defaults)

#undef COROUTINE_PROFILER          // Enable the coroutine debug profiler
#undef DEBUG_LIGHT                 // Show light debug serial messages
#undef DISABLE_WEATHERCHECK        // Disable Weather forcast checks
#undef DISABLE_ALERTCHECK          // Disable Weather Alert checks
#undef DISABLE_IPGEOCHECK          // Disable IPGEO checks

#define PROFILER_DELAY 10          // Coroutine profiler delay in seconds
#define WDT_TIMEOUT 30             // Watchdog Timeout seconds
#define GPS_BAUD 9600              // GPS UART gpsSpeed
#define GPS_RX_PIN 16              // GPS UART RX PIN
#define GPS_TX_PIN 17              // GPS UART TX PIN
#define DAYHUE 40                  // 6am daytime hue start
#define NIGHTHUE 175               // 10pm nighttime hue end
#define LUXMIN 5                   // Lowest brightness min
#define LUXMAX 100                 // Lowest brightness max
#define mw 32                      // Width of led matrix
#define mh 8                       // Hight of led matrix
#define ANI_BITMAP_CYCLES 4        // Number of animation frames in each weather icon bitmap
#define ANI_SPEED 100              // Bitmap animation speed in ms (lower is faster)
#define NTPCHECKTIME 60            // NTP server check time in minutes
#define LIGHT_CHECK_DELAY 250      // delay for each brightness check in ms
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
using namespace ace_routine;
using namespace ace_time;
using ace_time::acetime_t;
using ace_time::clock::DS3231Clock;
using ace_time::clock::SystemClockLoop;
using ace_routine::CoroutineScheduler;
using WireInterface = ace_wire::TwoWireInterface<TwoWire>;

static char intervals[][9] = {"31536000", "2592000", "604800", "86400", "3600", "60", "1"};
static char interval_names[][9] = {"years", "months", "weeks", "days", "hours", "minutes", "seconds"};

#include "src/structures/structures.h"
#include "src/colors/colors.h"
#include "src/bitmaps/bitmaps.h"

// Global Variables & Class Objects
const char thingName[] = "ledsmartclock";           // Default SSID used for new setup
const char wifiInitialApPassword[] = "ledsmartclock";     // Default AP password for new setup
static const char* TAG = "LedClock";                // Logging tag
WireInterface wireInterface(Wire);                  // I2C hardware object
DS3231Clock<WireInterface> dsClock(wireInterface);  // Hardware DS3231 RTC object
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
Checkalerts checkalerts;
Checkweather checkweather;
Checkipgeo checkipgeo;
ShowClock showclock;
CoTimers cotimer;
ScrollText scrolltext;
Alertflash alertflash;
Current current;
acetime_t lastntpcheck;
String wurl;                    // Built Openweather API URL
String aurl;                    // Built AWS API URL
String gurl;                    // Built IPGeolocation.io API URL
bool clock_display_offset;      // Clock display offset for single digit hour
bool wifi_connected;            // Wifi is connected or not
bool resetme;                   // reset to factory defaults
acetime_t bootTime;             // boot time
String timesource = "none";     // Primary timeclock source gps/ntp
uint8_t userbrightness;         // Current saved brightness setting (from iotwebconf)

// Function Declarations
void processLoc();
void wifiConnected();
void handleRoot();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);
void buildURLs();
String elapsedTime(int32_t seconds);
void display_showStatus();
void display_setclockDigit(uint8_t bmp_num, uint8_t position, uint16_t color);
void fillAlertsFromJson(Alerts *alerts);
void fillWeatherFromJson(Weather *weather);
void fillIpgeoFromJson(Ipgeo *ipgeo);
void debug_print(String message, bool cr);
acetime_t convertUnixEpochToAceTime(uint32_t ntpSeconds);
void setTimeSource(String);
void resetLastNtpCheck();
void printSystemZonedTime();
void display_temperature();
void display_weatherIcon();
void processTimezone();
acetime_t Now();
String capString(String str);
ace_time::ZonedDateTime getSystemZonedTime();
uint16_t calcbright(uint16_t bl);
String getSystemZonedDateString();

// AceTimeClock GPSClock custom class
namespace ace_time {
namespace clock {
class GPSClock: public Clock {
  private:
    static const uint8_t kNtpPacketSize = 48;
    const char* const mServer = "us.pool.ntp.org";
    uint16_t const mLocalPort = 2390;
    uint16_t const mRequestTimeout = 1000;
    mutable WiFiUDP mUdp;
    mutable uint8_t mPacketBuffer[kNtpPacketSize];

  public:
    mutable bool ntpIsReady = false;
    static const acetime_t kInvalidSeconds = LocalTime::kInvalidSeconds;
    /** Number of millis to wait during connect before timing out. */
    static const uint16_t kConnectTimeoutMillis = 10000;

    bool isSetup() const { return ntpIsReady; }

    void setup() {
      if (wifi_connected) {
        mUdp.begin(mLocalPort);
        ESP_LOGD(TAG, "GPSClock: NTP ready");
        ntpIsReady = true;
      }
    }

    void ntpReady() {
      if (wifi_connected || !ntpIsReady) {
        mUdp.begin(mLocalPort);
        ESP_LOGD(TAG, "GPSClock: NTP ready");
        ntpIsReady = true;
      }
    }

  acetime_t getNow() const {
    if (GPS.time.isUpdated())
        readResponse();
    if (!ntpIsReady || !wifi_connected || abs(Now() - lastntpcheck) > NTPCHECKTIME*60)
      return kInvalidSeconds;
    sendRequest();
    uint16_t startTime = millis();
    while ((uint16_t)(millis() - startTime) < mRequestTimeout)
      if (isResponseReady())
           return readResponse();
    return kInvalidSeconds;
    }

  void sendRequest() const {
    while (Serial1.available() > 0)
        GPS.encode(Serial1.read());
    if (GPS.time.isUpdated())
        return;
    if (!ntpIsReady)
        return;
    if (!wifi_connected) {
    ESP_LOGW(TAG, "GPSClock: NtpsendRequest(): not connected");
    return;
    }
    if (abs(Now() - lastntpcheck) > NTPCHECKTIME*60) {
      while (mUdp.parsePacket() > 0) {}
      ESP_LOGD(TAG, "GPSClock: NtpsendRequest(): sending request");
      IPAddress ntpServerIP;
      WiFi.hostByName(mServer, ntpServerIP);
      sendNtpPacket(ntpServerIP);  
    }
  }

  bool isResponseReady() const {
    if (GPS.time.isUpdated()) return true;
    if (!ntpIsReady || !wifi_connected)
      return false;
    return mUdp.parsePacket() >= kNtpPacketSize;
  }

  acetime_t readResponse() const {
    if (GPS.time.isUpdated()) {
      if (GPS.time.age() < 100) {
        setTimeSource("gps");
        resetLastNtpCheck();
        auto localDateTime = LocalDateTime::forComponents(GPS.date.year(), GPS.date.month(), GPS.date.day(), GPS.time.hour(), GPS.time.minute(), GPS.time.second());
        acetime_t gpsSeconds = localDateTime.toEpochSeconds();
        ESP_LOGI(TAG, "GPSClock: readResponse(): gpsSeconds: %d | age: %d ms", gpsSeconds, GPS.time.age());
        return gpsSeconds;
      } else {
          ESP_LOGW(TAG, "GPSClock: readResponse(): GPS time update skipped, no gpsfix or time too old: %d ms", GPS.time.age());
          return kInvalidSeconds;
        }
    }
    if (!ntpIsReady) return kInvalidSeconds;
    if (!wifi_connected) {
      ESP_LOGW(TAG, "GPSClock: NtpreadResponse: not connected");
      return kInvalidSeconds;
    }
    mUdp.read(mPacketBuffer, kNtpPacketSize);
    uint32_t ntpSeconds =  (uint32_t) mPacketBuffer[40] << 24;
    ntpSeconds |= (uint32_t) mPacketBuffer[41] << 16;
    ntpSeconds |= (uint32_t) mPacketBuffer[42] << 8;
    ntpSeconds |= (uint32_t) mPacketBuffer[43];
    if (ntpSeconds != 0) {
      acetime_t epochSeconds = convertUnixEpochToAceTime(ntpSeconds);
      ESP_LOGI(TAG, "GPSClock: readResponse(): ntpSeconds: %d | epochSeconds: %d", ntpSeconds, epochSeconds);
      resetLastNtpCheck();
      setTimeSource("ntp");
      return epochSeconds;
    } else {
        ESP_LOGE("0 Ntp second recieved");
        return kInvalidSeconds;
    }
  }

  void sendNtpPacket(const IPAddress& address) const {
    memset(mPacketBuffer, 0, kNtpPacketSize);
    mPacketBuffer[0] = 0b11100011;   // LI, Version, Mode
    mPacketBuffer[1] = 0;     // Stratum, or type of clock
    mPacketBuffer[2] = 6;     // Polling Interval
    mPacketBuffer[3] = 0xEC;  // Peer Clock Precision
    mPacketBuffer[12] = 49;
    mPacketBuffer[13] = 0x4E;
    mPacketBuffer[14] = 49;
    mPacketBuffer[15] = 52;
    mUdp.beginPacket(address, 123); //NTP requests are to port 123
    mUdp.write(mPacketBuffer, kNtpPacketSize);
    mUdp.endPacket();
  }
};
}
}

using ace_time::clock::GPSClock;
static GPSClock gpsClock;
static SystemClockLoop systemClock(&gpsClock /*reference*/, &dsClock /*backup*/, 60, 5, 1000);  // reference & backup clock

// IotWebConf User custom settings reference
// bool debugserial

// Group 1 (Display)
// uint8_t brightness_level;           // 1 low - 3 high
// uint8_t text_scroll_speed;          // 1 slow - 10 fast
// bool show_date;                     // show date
// color datecolor                     // date color
// int8_t show_date_interval;          // show date interval in hours
// bool disable_status                 // disable status corner led
// bool disable_alertflash             // disable alertflash

// Group 2 (Clock)
// bool twelve_clock                   // enable 12 hour clock
// bool used_fixed_tz;                 // Use fixed Timezone
// String fixedTz;                     // Fixed Timezone
// bool colonflicker;                  // flicker the colon ;)
// bool flickerfast;                   // flicker fast
// bool use_fixed_clockcolor           // use fixed clock color
// color fixed_clockcolor              // fixed clock color

// Group 3 (Weather)
// bool show_weather;                  // Show weather toggle
// bool fahrenheit                     // use fahrenheit for temp
// color weather_color                 // Weather text color
// bool use_fixed_tempcolor            // used fixed temp color
// color fixed_tempcolor               // fixed temp color
// uint8_t weather_check_interval;     // Time between weather checks
// uint8_t weather_show_interval;      // Time between weather displays
// uint8_t alert_check_interval;       // Time between alert checks
// uint8_t alert_show_interval;        // Time between alert displays
// String weatherapi;                  // OpenWeather API Key

// Group 4 (Location)
// bool used_fixed_loc;                // Use fixed GPS coordinates
// String fixedLat;                    // Fixed GPS latitude
// String fixedLon;                    // Fixed GPS Longitude
// String ipgeoapi                     // IP Geolocation API

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIGVER);
iotwebconf::CheckboxTParameter resetdefaults =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("resetdefaults").label("Reset to Defaults (AP mode)").defaultValue(false).build();
iotwebconf::CheckboxTParameter serialdebug =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("serialdebug").label("Debug to Serial").defaultValue(false).build();
iotwebconf::ParameterGroup group1 = iotwebconf::ParameterGroup("Display", "Display Settings");
  iotwebconf::IntTParameter<int8_t> brightness_level =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("brightness_level").label("Brightness Level (1-5)").defaultValue(2).min(1).max(5).step(1).placeholder("1(low)..5(high)").build();
  iotwebconf::IntTParameter<int8_t> text_scroll_speed =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("text_scroll_speed").label("Text Scroll Speed (1-10)").defaultValue(5).min(1).max(10).step(1).placeholder("1(low)..10(high)").build();
  iotwebconf::CheckboxTParameter show_date =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_date").label("Show Date").defaultValue(true).build();
  iotwebconf::ColorTParameter datecolor =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("datecolor").label("Choose date color").defaultValue("#FF8800").build();
 iotwebconf::IntTParameter<int8_t> show_date_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("show_date_interval").label("Show Date Interval in Hours (1-24)").defaultValue(1).min(1).max(24).step(1).placeholder("1..24(hours)").build();
  iotwebconf::CheckboxTParameter disable_status =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("disable_status").label("Disable corner status LED").defaultValue(false).build();
iotwebconf::CheckboxTParameter disable_alertflash =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("disable_alertflash").label("Disable Notification Flashes").defaultValue(false).build();
iotwebconf::ParameterGroup group3 = iotwebconf::ParameterGroup("Weather", "Weather Settings");
  iotwebconf::CheckboxTParameter show_weather =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_weather").label("Show Weather Conditions").defaultValue(true).build();
  iotwebconf::CheckboxTParameter fahrenheit =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("fahrenheit").label("Fahrenheit").defaultValue(true).build();
  iotwebconf::ColorTParameter weather_color =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("weather_color").label("Weather Conditions Text Color").defaultValue("#FF8800").build();
iotwebconf::CheckboxTParameter use_fixed_tempcolor =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_tempcolor").label("Use Fixed Temperature Color (Disables Auto Color)").defaultValue(false).build();
  iotwebconf::ColorTParameter fixed_tempcolor =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("fixed_tempcolor").label("Fixed Temperature Color").defaultValue("#FF8800").build();
 iotwebconf::IntTParameter<int8_t> weather_check_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("weather_check_interval").label("Weather Conditions Check Interval Min (1-60)").defaultValue(10).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
 iotwebconf::IntTParameter<int8_t> show_weather_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("weather_show_interval").label("Weather Conditions Display Interval Min (1-60)").defaultValue(10).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
 iotwebconf::IntTParameter<int8_t> alert_check_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("alert_check_interval").label("Weather Alert Check Interval Min (1-60)").defaultValue(5).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
 iotwebconf::IntTParameter<int8_t> show_alert_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("alert_show_interval").label("Weather Alert Display Interval Min (1-60)").defaultValue(5).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
  iotwebconf::TextTParameter<33> weatherapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("weatherapi").label("OpenWeather API Key").defaultValue("").build();
iotwebconf::ParameterGroup group2 = iotwebconf::ParameterGroup("Clock", "Clock Settings");
  iotwebconf::CheckboxTParameter twelve_clock=
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("twelve_clock").label("Use 12 Hour Clock").defaultValue(true).build();
  iotwebconf::CheckboxTParameter use_fixed_tz =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_tz").label("Use Fixed Timezone").defaultValue(false).build();
  iotwebconf::IntTParameter<int8_t> fixed_offset =   
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("fixed_offset").label("Fixed GMT Hours Offset").defaultValue(0).min(-12).max(12).step(1).placeholder("-12...12").build();
  iotwebconf::CheckboxTParameter colonflicker =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("colonflicker").label("Clock Colon Flash").defaultValue(true).build();
  iotwebconf::CheckboxTParameter flickerfast =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("flickerfast").label("Flash Colon Fast").defaultValue(true).build();
  iotwebconf::CheckboxTParameter use_fixed_clockcolor=
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_clockcolor").label("Use Fixed Clock Color (Disables Auto Color)").defaultValue(false).build();
    iotwebconf::ColorTParameter fixed_clockcolor =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("fixed_clockcolor").label("Fixed Clock Color").defaultValue("#FF8800").build();
iotwebconf::ParameterGroup group4 = iotwebconf::ParameterGroup("Location", "Location Settings");
  iotwebconf::CheckboxTParameter use_fixed_loc =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_loc").label("Use Fixed Location").defaultValue(false).build();
  iotwebconf::TextTParameter<12> fixedLat =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLat").label("Fixed latitude").defaultValue("").build();
  iotwebconf::TextTParameter<12> fixedLon =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLon").label("Fixed Longitude").defaultValue("").build();
  iotwebconf::TextTParameter<33> ipgeoapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("ipgeoapi").label("IPGeolocation.io API Key").defaultValue("").build();

// Coroutines
#ifdef COROUTINE_PROFILER
COROUTINE(printProfiling) {
  COROUTINE_LOOP() {
    LogBinTableRenderer::printTo(Serial, 2 /*startBin*/, 13 /*endBin*/, false /*clear*/);
    CoroutineScheduler::list(Serial);
    COROUTINE_DELAY(PROFILER_DELAY * 1000);
  }
}
#endif

COROUTINE(sysClock) {
  COROUTINE_LOOP()
  {
    systemClock.loop();
    COROUTINE_YIELD();
  }
}

COROUTINE(showClock) {
  COROUTINE_LOOP()
  {
    ace_time::ZonedDateTime ldt = getSystemZonedTime();
    uint8_t myhours = ldt.hour();
    uint8_t myminute = ldt.minute();
    uint8_t mysecs = ldt.second();
    showclock.seconds = mysecs;
    uint8_t myhour;
    if (twelve_clock.isChecked())
      myhour = myhours % 12;
    else
      myhour = myhours;
    if (myhours * 60 >= 1320)
      current.clockhue = NIGHTHUE;
    else if (myhours * 60 < 360)
      current.clockhue = NIGHTHUE;
    else
      current.clockhue = constrain(map(myhours * 60 + myminute, 360, 1319, DAYHUE, NIGHTHUE), DAYHUE, NIGHTHUE);
    if (use_fixed_clockcolor.isChecked())
      showclock.color = hex2rgb(fixed_clockcolor.value());
    else
      showclock.color = hsv2rgb(current.clockhue);
    showclock.millis = millis();
    matrix->clear();
    if (myhour / 10 == 0 && myhour % 10 == 0)
    {
      display_setclockDigit(1, 0, showclock.color);
      display_setclockDigit(2, 1, showclock.color);
      clock_display_offset = false;
    }
    else
    {
      if (myhour / 10 != 0)
      {
        clock_display_offset = false;
        display_setclockDigit(myhour / 10, 0, showclock.color); // set first digit of hour
      }
      else
        clock_display_offset = true;
      display_setclockDigit(myhour % 10, 1, showclock.color); // set second digit of hour
    }
      display_setclockDigit(myminute / 10, 2, showclock.color); // set first digig of minute
      display_setclockDigit(myminute % 10, 3, showclock.color); // set second digit of minute
      if (showclock.colonflicker)
      {
        if (clock_display_offset)
        {
          matrix->drawPixel(12, 5, showclock.color); // draw colon
          matrix->drawPixel(12, 2, showclock.color); // draw colon
        }
        else
        {
          matrix->drawPixel(16, 5, showclock.color); // draw colon
          matrix->drawPixel(16, 2, showclock.color); // draw colon
        }
      display_showStatus();
      matrix->show();
      }
      else
      {
        display_showStatus();
        matrix->show();
        showclock.colonoff = true;
        COROUTINE_AWAIT(millis() - showclock.millis > showclock.fstop);
        if (clock_display_offset)
        {
          matrix->drawPixel(12, 5, showclock.color); // draw colon
          matrix->drawPixel(12, 2, showclock.color); // draw colon
        }
        else
        {
          matrix->drawPixel(16, 5, showclock.color); // draw colon
          matrix->drawPixel(16, 2, showclock.color); // draw colon
        }
      display_showStatus();
      matrix->show();
      }
    COROUTINE_AWAIT(showclock.seconds != getSystemZonedTime().second());
  }
}

#ifndef DISABLE_WEATHERCHECK
COROUTINE(checkWeather) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(abs(systemClock.getNow() - checkweather.lastsuccess) > (weather_check_interval.value() * T1M) && weather_check_interval.value() != 0 && abs(systemClock.getNow() - checkweather.lastattempt) > T1M && wifi_connected && (current.lat).length() > 1 && (weatherapi.value())[0] != '\0' && !cotimer.show_alert_ready && !cotimer.show_weather_ready);
    ESP_LOGD(TAG, "Checking weather forecast...");
    checkweather.retries = 1;
    checkweather.jsonParsed = false;
    while (!checkweather.jsonParsed && (checkweather.retries <= 2))
    {
      COROUTINE_DELAY(1000);
      ESP_LOGD(TAG, "Connecting to %s", wurl);
      HTTPClient http;
      http.begin(wurl);
      int httpCode = http.GET();
      ESP_LOGD(TAG, "HTTP code : %d", httpCode);
      if (httpCode == 200)
      {
        weatherJson = JSON.parse(http.getString());
        if (JSON.typeof(weatherJson) == "undefined")
          ESP_LOGE(TAG, "Parsing weatherJson input failed!");
        else
          checkweather.jsonParsed = true;
      } 
      else if (httpCode == 401) {
        alertflash.color = RED;
        alertflash.laps = 1;
        alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = "Invalid Openweathermap.org API Key";
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;
        COROUTINE_AWAIT(!scrolltext.active);
      } else {
        alertflash.color = RED;
        alertflash.laps = 1;
        alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = (String)"Error getting weather forcast: " +httpCode;
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;       
        COROUTINE_AWAIT(!scrolltext.active);
      }
      http.end();
      checkweather.retries++;
    }
    if (!checkweather.jsonParsed)
      ESP_LOGE(TAG, "Error: JSON");
    else
    {
      fillWeatherFromJson(&weather);
      checkweather.lastsuccess = systemClock.getNow();
    }
    checkweather.lastattempt = systemClock.getNow();
  }
}
#endif

#ifndef DISABLE_ALERTCHECK
COROUTINE(checkAlerts) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(abs(systemClock.getNow() - checkalerts.lastsuccess) > (alert_check_interval.value() * T1M) && alert_check_interval.value() != 0 && (systemClock.getNow() - checkalerts.lastattempt) > T1M && wifi_connected && (current.lat).length() > 1 && !cotimer.show_alert_ready && !cotimer.show_weather_ready);
    ESP_LOGD(TAG, "Checking weather alerts...");
    checkalerts.retries = 1;
    checkalerts.jsonParsed = false;
    while (!checkalerts.jsonParsed && (checkalerts.retries <= 2))
    {
      COROUTINE_DELAY(1000);
      ESP_LOGD(TAG, "Connecting to %s", aurl);
      HTTPClient http;
      http.begin(aurl);
      int httpCode = http.GET();
      ESP_LOGD(TAG, "HTTP code : %d", httpCode);
      if (httpCode == 200)
      {
        alertsJson = JSON.parse(http.getString());
        if (JSON.typeof(alertsJson) == "undefined")
          ESP_LOGE(TAG, "Parsing alertJson input failed!");
        else
          checkalerts.jsonParsed = true;
      } else 
      {
          alertflash.color = RED;
          alertflash.laps = 1;
          alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = (String) "Error getting weather alerts: " + httpCode;
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;
        COROUTINE_AWAIT(!scrolltext.active);
      }
      http.end();
      checkalerts.retries++;
    }
    if (!checkalerts.jsonParsed)
      ESP_LOGE(TAG, "Error: JSON");
    else
    {
      fillAlertsFromJson(&alerts);
      checkalerts.lastsuccess = systemClock.getNow();
    }
    checkalerts.lastattempt = systemClock.getNow();
  }
}
#endif

COROUTINE(showDate) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(cotimer.show_date_ready && showClock.isSuspended() && !cotimer.show_alert_ready && !cotimer.show_weather_ready && !scrolltext.active && !alertflash.active);
  scrolltext.message = getSystemZonedDateString();
  scrolltext.color = hex2rgb(datecolor.value());
  scrolltext.active = true;
  COROUTINE_AWAIT(!scrolltext.active);
  showclock.datelastshown = systemClock.getNow();
  cotimer.show_date_ready = false;
  }
}

COROUTINE(showWeather) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(cotimer.show_weather_ready && showClock.isSuspended() && !cotimer.show_alert_ready && !scrolltext.active && !alertflash.active);
  alertflash.color = hex2rgb(weather_color.value());
  alertflash.laps = 1;
  alertflash.active = true;
  COROUTINE_AWAIT(!alertflash.active);
  scrolltext.message = capString((String)weather.currentDescription);
  
  scrolltext.color = hex2rgb(weather_color.value());
  scrolltext.active = true;
  scrolltext.displayicon = true;
  COROUTINE_AWAIT(!scrolltext.active);
  cotimer.millis = millis();
  while (millis() - cotimer.millis < 10000) {
    matrix->clear();
    display_weatherIcon();
    display_temperature();
    display_showStatus();
    matrix->show();
    COROUTINE_DELAY(50);
  }
  weather.lastshown = systemClock.getNow();
  cotimer.show_weather_ready = false;
  }
}

COROUTINE(showAlerts) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(cotimer.show_alert_ready && showClock.isSuspended() && !scrolltext.active & !alertflash.active);
  uint16_t acolor;
  if (alerts.inWarning)
    acolor = RED;
  else if (alerts.inWatch)
    acolor = YELLOW;
  alertflash.color = RED;
  alertflash.laps = 3;
  alertflash.active = true;
  COROUTINE_AWAIT(!alertflash.active);
  scrolltext.message = (String)alerts.description1;
  scrolltext.color = RED;
  scrolltext.active = true;
  COROUTINE_AWAIT(!scrolltext.active);
  alerts.lastshown = systemClock.getNow();
  cotimer.show_alert_ready = false;
}
}

COROUTINE(print_debugData) {
  COROUTINE_LOOP() 
  {
    COROUTINE_DELAY_SECONDS(15);
    COROUTINE_AWAIT(serialdebug.isChecked() && showAlerts.isYielding() && showWeather.isYielding());
    debug_print("----------------------------------------------------------------", true);
    printSystemZonedTime();
    acetime_t now = systemClock.getNow();
    String gage = elapsedTime(now - gps.timestamp);
    String loca = elapsedTime(now - gps.lockage);
    String uptime = elapsedTime(now - bootTime);
    String igt = elapsedTime(now - checkipgeo.lastattempt);
    String igp = elapsedTime(now - checkipgeo.lastsuccess);
    String wage = elapsedTime(now - weather.timestamp);
    String wlt = elapsedTime(now - checkweather.lastattempt);
    String alt = elapsedTime(now - checkalerts.lastattempt);
    String wls = elapsedTime(now - weather.lastshown);
    String als = elapsedTime(now - alerts.lastshown);
    String alg = elapsedTime(now - checkalerts.lastsuccess);
    String wlg = elapsedTime(now - checkweather.lastsuccess);
    String lst = elapsedTime(now - systemClock.getLastSyncTime());
    String npt = elapsedTime((now - lastntpcheck) - NTPCHECKTIME * 60);
    String lip = (WiFi.localIP()).toString();
    printSystemZonedTime();
    debug_print((String) "Version - Firmware:v" + VERSION + " | Config:v" + CONFIGVER, true);
    debug_print((String) "System - RawLux:" + current.rawlux + " | Lux:" + current.lux + " | UsrBright:+" + userbrightness + " | Brightness:" + current.brightness + " | ClockHue:" + current.clockhue + " | temphue:" + current.temphue + " | WifiConnected:" + wifi_connected + " | IP:" + lip + " | Uptime:" + uptime, true);
    debug_print((String) "Clock - Status:" + systemClock.getSyncStatusCode() + " | TimeSource:" + timesource + " | CurrentTZ:" + current.tzoffset +  " | NtpReady:" + gpsClock.ntpIsReady + " | LastTry:" + elapsedTime(systemClock.getSecondsSinceSyncAttempt()) + " | NextTry:" + elapsedTime(systemClock.getSecondsToSyncAttempt()) + " | Skew:" + systemClock.getClockSkew() + " sec | NextNtp:" + npt + " | LastSync:" + lst, true);
    debug_print((String) "Loc - SavedLat:" + preferences.getString("lat", "") + " | SavedLon:" + preferences.getString("lon", "") + " | CurrentLat:" + current.lat + " | CurrentLon:" + current.lon, true);
    debug_print((String) "IPGeo - Complete:" + checkipgeo.complete + " | Lat:" + ipgeo.lat + " | Lon:" + ipgeo.lon + " | TZoffset:" + ipgeo.tzoffset + " | Timezone:" + ipgeo.timezone + " | LastAttempt:" + igt + " | LastSuccess:" + igp, true);
    debug_print((String) "GPS - Chars:" + GPS.charsProcessed() + " | With-Fix:" + GPS.sentencesWithFix() + " | Failed:" + GPS.failedChecksum() + " | Passed:" + GPS.passedChecksum() + " | Sats:" + gps.sats + " | Hdop:" + gps.hdop + " | Elev:" + gps.elevation + " | Lat:" + gps.lat + " | Lon:" + gps.lon + " | FixAge:" + gage + " | LocAge:" + loca, true);
    debug_print((String) "Weather - Icon:" + weather.currentIcon + " | Temp:" + weather.currentTemp + " | FeelsLike:" + weather.currentFeelsLike + " | Humidity:" + weather.currentHumidity + " | Wind:" + weather.currentWindSpeed + " | Desc:" + weather.currentDescription + " | LastTry:" + wlt + " | LastSuccess:" + wlg + " | LastShown:" + wls + " | Age:" + wage, true);
    debug_print((String) "Alerts - Active:" + alerts.active + " | Watch:" + alerts.inWatch + " | Warn:" + alerts.inWarning + " | LastTry:" + alt + " | LastSuccess:" + alg + " | LastShown:" + als, true);
    if (alerts.active)
      debug_print((String) "*Alert1 - Status:" + alerts.status1 + " | Severity:" + alerts.severity1 + " | Certainty:" + alerts.certainty1 + " | Urgency:" + alerts.urgency1 + " | Event:" + alerts.event1 + " | Desc:" + alerts.description1, true);
  }
}

#ifndef DISABLE_IPGEOCHECK
COROUTINE(checkIpgeo) {
  COROUTINE_BEGIN();
  COROUTINE_AWAIT(wifi_connected && (ipgeoapi.value())[0] != '\0' && !cotimer.show_alert_ready && !cotimer.show_weather_ready && !scrolltext.active && !alertflash.active);
  while (!checkipgeo.complete)
  {
    COROUTINE_AWAIT(abs(systemClock.getNow() - checkipgeo.lastattempt) > T1M);
    ESP_LOGD(TAG, "Checking IP Geolocation...");
    checkipgeo.retries = 1;
    checkipgeo.jsonParsed = false;
    while (!checkipgeo.jsonParsed && (checkipgeo.retries <= 1))
    {
      COROUTINE_DELAY(1000);
      ESP_LOGD(TAG, "Connecting to %s", gurl);
      HTTPClient http;
      http.begin(gurl);
      int httpCode = http.GET();
      ESP_LOGD(TAG, "HTTP code : %d", httpCode);
      if (httpCode == 200)
      {
        ipgeoJson = JSON.parse(http.getString());
        if (JSON.typeof(ipgeoJson) == "undefined")
          ESP_LOGE(TAG, "Parsing weatherJson input failed!");
        else
          checkipgeo.jsonParsed = true;
      }
      else if (httpCode == 401)
      {
        alertflash.color = RED;
        alertflash.laps = 1;
        alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = "Invalid IPGeolocation API Key";
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;
        COROUTINE_AWAIT(!scrolltext.active);
      }
      else
      {
        alertflash.color = RED;
        alertflash.laps = 1;
        alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = (String) "Error getting ip geolocation: " + httpCode;
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;
        COROUTINE_AWAIT(!scrolltext.active);
      }
      http.end();
      checkipgeo.retries++;
    }
    if (!checkipgeo.jsonParsed)
      ESP_LOGE(TAG, "Error: JSON");
    else
    {
      fillIpgeoFromJson(&ipgeo);
      checkipgeo.lastsuccess = systemClock.getNow();
      checkipgeo.complete = true;
      processLoc();
      processTimezone();
    }
    checkipgeo.lastattempt = systemClock.getNow();
  }
  COROUTINE_END();
}
#endif


COROUTINE(miscScrollers) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(iotWebConf.getState() == 1 || resetme);
    COROUTINE_AWAIT(!scrolltext.active && !alertflash.active && (millis() - scrolltext.resetmsgtime) > T1M*1000);
    if (resetme)
    {
      scrolltext.showingreset = true;
      showDate.suspend();
      showWeather.suspend();
      showAlerts.suspend();
      showClock.suspend();
      checkWeather.suspend();
      checkAlerts.suspend();
      alertflash.color = RED;
      alertflash.laps = 5;
      alertflash.active = true;
      COROUTINE_AWAIT(!alertflash.active);
      scrolltext.message = "Resetting to Defaults, Connect to ledsmartclock wifi to setup";
      scrolltext.color = RED;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      nvs_flash_erase(); // erase the NVS partition and...
      nvs_flash_init();  // initialize the NVS partition.
      ESP.restart();
     }
     else if (iotWebConf.getState() == 1 && (millis() - scrolltext.resetmsgtime) > T1M*1000) {
      scrolltext.showingreset = true;
      showDate.suspend();
      showWeather.suspend();
      showAlerts.suspend();
      showClock.suspend();
      checkWeather.suspend();
      checkAlerts.suspend();
      alertflash.color = RED;
      alertflash.laps = 5;
      alertflash.active = true;
      COROUTINE_AWAIT(!alertflash.active);
      scrolltext.message = (String)"Connect to ledsmartclock wifi to setup, version:" + VERSION;
      scrolltext.color = RED;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      scrolltext.resetmsgtime = millis();
      scrolltext.showingreset = false;
     }
  }
}

COROUTINE(scheduleManager) {
  COROUTINE_LOOP() 
  { // start the clock
  acetime_t now = systemClock.getNow();
  if (!resetme && !scrolltext.showingreset && !cotimer.show_alert_ready && !cotimer.show_weather_ready && showClock.isSuspended() && !scrolltext.active && !alertflash.active)
  {
    showClock.reset();
    showClock.resume();
  } // start the weather alert display
  if ((abs(now - alerts.lastshown) > (show_alert_interval.value()*T1M)) && show_alert_interval.value() != 0 && alerts.active && !cotimer.show_weather_ready && !cotimer.show_alert_ready)
    cotimer.show_alert_ready = true;
   // start weather conditions display
  else if ((abs(now - weather.lastshown) > (show_weather_interval.value() * T1M)) && (abs(now - weather.timestamp)) < T2H && show_weather_interval.value() != 0 && !cotimer.show_weather_ready && !cotimer.show_alert_ready)
    cotimer.show_weather_ready = true;
    else if ((abs(now - showclock.datelastshown) > (show_date_interval.value() * T1H)) && show_date_interval.value() != 0 && !cotimer.show_weather_ready && !cotimer.show_alert_ready)
    cotimer.show_date_ready = true;
  if (scrolltext.active || alertflash.active || cotimer.show_weather_ready || cotimer.show_alert_ready) 
  {
    #ifndef DISABLE_WEATHERCHECK
    if (!checkWeather.isSuspended()) {
      checkWeather.suspend();
    }
    #endif
    #ifndef DISABLE_ALERTCHECK
    if (!checkAlerts.isSuspended()) {
      checkAlerts.suspend();
    }
    #endif
    if (!serialdebug.isChecked() && !print_debugData.isSuspended())
      print_debugData.suspend();
    if (!showClock.isSuspended()) {
      showClock.suspend();
    }
  }
  if (!resetme && !scrolltext.showingreset && !scrolltext.active && !alertflash.active && !cotimer.show_weather_ready && !cotimer.show_alert_ready) 
  {
    #ifndef DISABLE_WEATHERCHECK
    if (checkWeather.isSuspended()) {
      checkWeather.resume();
    }
    #endif
    #ifndef DISABLE_ALERTCHECK
    if (checkAlerts.isSuspended()) {
      checkAlerts.resume();
    }
    #endif
  }
    if (serialdebug.isChecked() && !scrolltext.active && !alertflash.active && print_debugData.isSuspended() && !cotimer.show_alert_ready && !cotimer.show_weather_ready) 
    {
      print_debugData.reset();
      print_debugData.resume();
    }
    if (!serialdebug.isChecked() && !print_debugData.isSuspended()) {
      print_debugData.suspend();
    }
    //if (abs(now - bootTime) > (T1Y - 60))
    //{
    //  ESP_LOGE(TAG, "1 year system reset!");
      //ESP.restart();
    //}
    if (checkipgeo.complete && !checkIpgeo.isSuspended())
      checkIpgeo.suspend();
    if (!resetme && iotWebConf.getState() != 1 && show_weather.isChecked() && showWeather.isSuspended()) {
      showWeather.reset();
      checkWeather.reset();
      showWeather.resume();
      checkWeather.resume();
    }
     if (!show_weather.isChecked() && !showWeather.isSuspended()) {
      showWeather.suspend();
      checkWeather.suspend();
     }
     if (!resetme && iotWebConf.getState() != 1 && show_date.isChecked() && showDate.isSuspended()) {
      showDate.reset();
      checkWeather.reset();
      showDate.resume();
      checkWeather.resume();     
    }
     if (!show_date.isChecked() && !showDate.isSuspended()) {
      showDate.suspend();
     }
    COROUTINE_YIELD();
  }
}

COROUTINE(IotWebConf) {
  COROUTINE_LOOP() 
  {
    iotWebConf.doLoop();
    COROUTINE_DELAY(5);
  }
}

COROUTINE(AlertFlash) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(alertflash.active && showClock.isSuspended());
    if (!disable_alertflash.isChecked()) {
      alertflash.lap = 0;
      matrix->clear();
      matrix->show();
      COROUTINE_DELAY(250);
      while (alertflash.lap < alertflash.laps) {
        matrix->clear();
        matrix->fillScreen(alertflash.color);
        matrix->show();
        COROUTINE_DELAY(250);
        matrix->clear();
        matrix->show();
        COROUTINE_DELAY(250);
        alertflash.lap++;
      }
      alertflash.active = false;
    } else
        alertflash.active = false;
  }
}

COROUTINE(scrollText) {
  COROUTINE_LOOP() 
  {
      COROUTINE_AWAIT(scrolltext.active && showClock.isSuspended());
      COROUTINE_AWAIT(millis() - scrolltext.millis >= cotimer.scrollspeed);
      uint16_t size = (scrolltext.message).length() * 6;
      if (scrolltext.position >= size - size - size) {
        matrix->clear();
        matrix->setCursor(scrolltext.position, 0);
        matrix->setTextColor(scrolltext.color);
        matrix->print(scrolltext.message);
        display_showStatus();
        if (scrolltext.displayicon)
          display_weatherIcon();
        matrix->show();
        scrolltext.position--;
        scrolltext.millis = millis();
      } else {
      scrolltext.active = false;
      scrolltext.position = mw;
      scrolltext.displayicon = false;
      ESP_LOGD(TAG, "Scrolling text end");
      }
  }
}

COROUTINE(gps_checkData) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(Serial1.available() > 0);
    while (Serial1.available() > 0)
      GPS.encode(Serial1.read());
    if (GPS.satellites.isUpdated())
    {
      gps.sats = GPS.satellites.value();
      ESP_LOGV(TAG, "GPS Satellites updated: %d", gps.sats);
      if (gps.sats > 0 && gps.fix == false)
      {
        gps.fix = true;
        gps.timestamp = systemClock.getNow();
        ESP_LOGI(TAG, "GPS fix aquired with %d satellites", gps.sats);
      }
      if (gps.sats == 0 && gps.fix == true)
      {
        gps.fix = false;
        ESP_LOGI(TAG, "GPS fix lost, no satellites");
      }
    }
    if (GPS.location.isUpdated()) 
    {
      if (!use_fixed_loc.isChecked()) {
        gps.lat = String(GPS.location.lat(), 5);
        gps.lon = String(GPS.location.lng(), 5);
        gps.lockage = systemClock.getNow();
        processLoc();
      }
      else {
        gps.lat = fixedLat.value();
        gps.lon = fixedLon.value();
        processLoc();
      }
      ESP_LOGV(TAG, "GPS Location updated: Lat: %s Lon: %s", gps.lat, gps.lon);
    }
    if (GPS.altitude.isUpdated()) 
    {
      gps.elevation = GPS.altitude.feet();
      ESP_LOGV(TAG, "GPS Elevation updated: %f feet", gps.elevation);
    }
    if (GPS.hdop.isUpdated()) 
    {
      gps.hdop = GPS.hdop.hdop();
      ESP_LOGV(TAG, "GPS HDOP updated: %f m",gps.hdop);
    }
    if (GPS.charsProcessed() < 10) ESP_LOGE(TAG, "No GPS data. Check wiring.");
  COROUTINE_DELAY(1000);
  }
}

COROUTINE(setBrightness) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(Tsl.available());
    Tsl.fullLuminosity(current.rawlux);
    current.lux = constrain(map(current.rawlux, 0, 500, LUXMIN, LUXMAX), LUXMIN, LUXMAX);
    current.brightavg = ((current.brightness) + (current.lux + userbrightness)) / 2;
    if (current.brightavg != current.brightness) 
    {
      current.brightness = current.brightavg;
      matrix->setBrightness(current.brightness);
      matrix->show();
    }
    #ifdef DEBUG_LIGHT
    Serial.println((String) "Lux: " + full + " brightness: " + cb + " avg: " + current.brightness);
    #endif
    COROUTINE_DELAY(LIGHT_CHECK_DELAY);
  }
}

// Regular Functions
void debug_print(String message, bool cr = false) {
  if (serialdebug.isChecked())
    if (cr)
      Serial.println(message);
    else
      Serial.print(message);
}

void processTimezone()
{
  uint32_t timer = millis();
  int16_t savedoffset;
  savedoffset = preferences.getShort("tzoffset", 0);
  if (use_fixed_tz.isChecked())
  {
    current.tzoffset = fixed_offset.value();
    current.timezone = TimeZone::forHours(fixed_offset.value());
    ESP_LOGD(TAG, "Using fixed timezone offset: %d", fixed_offset.value());
  }
  else if (ipgeo.tzoffset != 127) {
    current.tzoffset = ipgeo.tzoffset;
    current.timezone = TimeZone::forHours(ipgeo.tzoffset);
    ESP_LOGD(TAG, "Using ipgeolocation offset: %d", ipgeo.tzoffset);
    if (ipgeo.tzoffset != savedoffset)
    {
      ESP_LOGI(TAG, "IP Geo timezone [%d] (%s) is different then saved timezone [%d], saving new timezone", ipgeo.tzoffset, ipgeo.timezone, savedoffset);
      preferences.putShort("tzoffset", ipgeo.tzoffset);
    }
  }
  else {
    current.tzoffset = savedoffset;
    current.timezone = TimeZone::forHours(savedoffset);
    ESP_LOGD(TAG, "Using saved timezone offset: %d", savedoffset);
  }
  ESP_LOGV(TAG, "Process timezone complete: %d ms", (millis()-timer));
}

void processLoc(){
  if (use_fixed_loc.isChecked())
  {
    gps.lat = fixedLat.value();
    gps.lon = fixedLon.value();
    current.lat = fixedLat.value();
    current.lon = fixedLon.value();
  }
  String savedlat = preferences.getString("lat", "0");
  String savedlon = preferences.getString("lon", "0");
  if (gps.lon == "0" && ipgeo.lon[0] == '\0' && current.lon == "0")
  {
    current.lat = savedlat;
    current.lon = savedlon;
  }
  else if (gps.lon == "0" && ipgeo.lon[0] != '\0' && current.lon == "0")
  {
    current.lat = (String)ipgeo.lat; 
    current.lon = (String)ipgeo.lon;
    ESP_LOGI(TAG, "Using IPGeo location information: Lat: %s Lon: %s", (ipgeo.lat), (ipgeo.lon));
  }
  else if (gps.lon != "0")
  {
    current.lat = gps.lat;
    current.lon = gps.lon;
  }
  double nlat = (current.lat).toDouble();
  double nlon = (current.lon).toDouble();
  double olat = savedlat.toDouble();
  double olon = savedlon.toDouble();
  if (abs(nlat - olat) > 0.02 || abs(nlon - olon) > 0.02) {
    ESP_LOGW(TAG, "Major location shift, saving values");
    ESP_LOGW(TAG, "Lat:[%0.6lf]->[%0.6lf] Lon:[%0.6lf]->[%0.6lf]", olat, nlat, olon, nlon);
    preferences.putString("lat", current.lat);
    preferences.putString("lon", current.lon);
  }
  if (olat == 0 || nlat == 0) {
    preferences.putString("lat", current.lat);
    preferences.putString("lon", current.lon);
  }
  buildURLs();
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

void display_showStatus() {
    uint16_t clr;
    uint16_t wclr;
    bool ds = disable_status.isChecked();
    if (ds)
      clr = BLACK;
    acetime_t now = systemClock.getNow();
    if (now - systemClock.getLastSyncTime() > (NTPCHECKTIME*60) + 60)
        clr = DARKRED;
    else if (timesource == "gps" && gps.fix && (now - systemClock.getLastSyncTime()) <= 600)
        clr = BLACK;
    else if (timesource == "gps" && !gps.fix && (now - systemClock.getLastSyncTime()) <= 600 && !ds)
        clr = DARKBLUE;
    else if (timesource == "gps" && !gps.fix && (now - systemClock.getLastSyncTime()) > 600 && !ds)
        clr = DARKPURPLE;
    else if (gps.fix && timesource == "ntp" && !ds)
        clr = DARKPURPLE;
    else if (timesource == "ntp" && iotWebConf.getState() == 4 && !ds)
      clr = DARKGREEN;
    else if (iotWebConf.getState() == 3 && !ds)  // connecting
      clr = DARKYELLOW; 
    else if (iotWebConf.getState() <= 2 && !ds)  // boot, apmode, notconfigured
      clr = DARKMAGENTA;
    else if (iotWebConf.getState() == 4 && !ds)  // online
      clr = DARKCYAN;
    else if (iotWebConf.getState() == 5 && !ds) // offline
      clr = DARKRED;
    else if (!ds)
      clr = DARKRED;
    if (alerts.inWarning)
      wclr = RED;
    else if (alerts.inWatch)
      wclr = YELLOW;
    else
      wclr = BLACK;
    matrix->drawPixel(0, 7, clr);
    matrix->drawPixel(0, 0, wclr);
    if (current.oldstatusclr != clr || current.oldstatuswclr != wclr) {
      matrix->show();
      current.oldstatusclr = clr;
      current.oldstatuswclr = wclr;
    }
}

void display_weatherIcon() {
    bool night;
    char dn;
    char code1;
    char code2;
    dn = weather.currentIcon[2];
    if ((String)dn == "n")
      night = true;
    else
      night = false;
    code1 = weather.currentIcon[0];
    code2 = weather.currentIcon[1];
    if (code1 == '0' && code2 == '1')
    { // clear
      if (night)
        matrix->drawRGBBitmap(mw-8, 0, clear_night[cotimer.iconcycle], 8, 8);
      else
        matrix->drawRGBBitmap(mw-8, 0, clear_day[cotimer.iconcycle], 8, 8);
    }
    else if ((code1 == '0' && code2 == '2') || (code1 == '0' && code2 == '3')) {  //partly cloudy
      if (night)
        matrix->drawRGBBitmap(mw-8, 0, partly_cloudy_night[cotimer.iconcycle], 8, 8);
      else
        matrix->drawRGBBitmap(mw-8, 0, partly_cloudy_day[cotimer.iconcycle], 8, 8);
    }
    else if ((code1 == '0') && (code2 == '4')) // cloudy
      matrix->drawRGBBitmap(mw-8, 0, cloudy[cotimer.iconcycle], 8, 8);
    else if (code1 == '5' && code2 == '0')  //fog/mist 
      matrix->drawRGBBitmap(mw-8, 0, fog[cotimer.iconcycle], 8, 8);
    else if (code1 == '1' && code2 == '3')  //snow
      matrix->drawRGBBitmap(mw-8, 0, snow[cotimer.iconcycle], 8, 8);
    else if (code1 == '0' && code2 == '9') //rain
      matrix->drawRGBBitmap(mw-8, 0, rain[cotimer.iconcycle], 8, 8);
    else if (code1 == '1' && code2 == '0') // heavy raid
      matrix->drawRGBBitmap(mw-8, 0, heavy_rain[cotimer.iconcycle], 8, 8);
    else if (code1 == '1' && code2 == '1') //thunderstorm
      matrix->drawRGBBitmap(mw-8, 0, thunderstorm[cotimer.iconcycle], 8, 8);
    else
      ESP_LOGE(TAG, "No Weather icon found to use");
    if (millis() - cotimer.icontimer > ANI_SPEED)
    {
    if (cotimer.iconcycle == ANI_BITMAP_CYCLES)
      cotimer.iconcycle = 0;
    else
      cotimer.iconcycle++;
    cotimer.icontimer = millis();
    }
}

void display_temperature() {
    int temp;
    if (fahrenheit.isChecked())
      temp = weather.currentFeelsLike * 1.8 + 32;
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
    uint32_t tc;
    if (!use_fixed_tempcolor.isChecked())
    {
      current.temphue = constrain(map(weather.currentFeelsLike, 0, 40, NIGHTHUE, 0), NIGHTHUE, 0);
      if (weather.currentFeelsLike < 0)
        current.temphue = NIGHTHUE + abs(weather.currentFeelsLike);
      tc = hsv2rgb(current.temphue);
    }
    else
      tc = hex2rgb(fixed_tempcolor.value());
    while (score)
    {
      matrix->drawBitmap(xpos, 0, num[score % 10], 8, 8, tc);
      score /= 10;
      xpos = xpos - 7;
    }
    if (isneg == true) 
    {
      matrix->drawBitmap(xpos, 0, sym[1], 8, 8, tc);
      xpos = xpos - 7;
    }
    matrix->drawBitmap(xpos+(digits*7)+7, 0, sym[0], 8, 8, tc);
}

void printSystemZonedTime() {
  acetime_t now = systemClock.getNow();
  auto TimeWZ = ZonedDateTime::forEpochSeconds(now, current.timezone);
  TimeWZ.printTo(SERIAL_PORT_MONITOR);
  SERIAL_PORT_MONITOR.println();
}

ace_time::ZonedDateTime getSystemZonedTime() {
  acetime_t now = systemClock.getNow();
  ace_time::ZonedDateTime TimeWZ = ZonedDateTime::forEpochSeconds(now, current.timezone);
  return TimeWZ;
}

String getSystemZonedTimeString() {
  ace_time::ZonedDateTime now = getSystemZonedTime();
  char *string;
  sprintf(string, "%d/%d/%d %d:%d:%d [%d]", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second(), now.timeOffset());
  return (String)string;
}

String getSystemZonedDateString() {
  ace_time::ZonedDateTime ldt = getSystemZonedTime();
  uint8_t day = ldt.day();
  char *string;
  sprintf(string, "%s, %s %d%s %04d", DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()), day, ordinal_suffix(day), ldt.year());
  return (String)string;
}

String getSystemZonedDateTimeString() {
  ace_time::ZonedDateTime ldt = getSystemZonedTime();
  uint8_t day = ldt.day();
  uint8_t hour = ldt.day();
  char *string;
  String ap = "am";
  //if (twelve_clock.isChecked())
  //{
  //  if (hour > 11)
  //    ap = "pm";
  //  if (hour > 12)
  //    hour = hour - 12;
  //  sprintf(string, "%d:%02d%s %s, %s %d%s %04d []", ldt.hour(), ldt.minute(), ap, DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()), day, ordinal_suffix(day), ldt.year());
  //} 
  //else
    sprintf(string, "%02d:%02d %s, %s %d%s %04d []", ldt.hour(), ldt.minute(), DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()), day, ordinal_suffix(day), ldt.year());
  return (String)string;
}

// System Loop
void loop() {
  esp_task_wdt_reset();
#ifdef COROUTINE_PROFILER
  CoroutineScheduler::loopWithProfiler();
  #else
  CoroutineScheduler::loop();
  #endif
}

// System Setup
void setup () {
  Serial.begin(115200);
  uint32_t timer = millis();
  ESP_EARLY_LOGD(TAG, "Initializing Hardware Watchdog...");
  esp_task_wdt_init(WDT_TIMEOUT, true);                //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                              //add current thread to WDT watch
  ESP_EARLY_LOGD(TAG, "Reading stored preferences...");
  preferences.begin("location", false);
  ESP_EARLY_LOGD(TAG, "Initializing coroutine scheduler...");
  sysClock.setName("sysclock");
  scheduleManager.setName("scheduler");
  showClock.setName("show_clock");
  setBrightness.setName("brightness");
  gps_checkData.setName("gpsdata");
  print_debugData.setName("debug_print");
  IotWebConf.setName("iotwebconf");
  scrollText.setName("scrolltext");
  AlertFlash.setName("alertflash");
  showWeather.setName("show_weather");
  showAlerts.setName("show_alerts");
  #ifndef DISABLE_WEATHERCHECK
  checkWeather.setName("check_weather");
  #endif
  #ifndef DISABLE_ALERTCHECK
  checkAlerts.setName("check_alerts");
  #endif
  #ifndef DISABLE_IPGEOCHECK
  checkIpgeo.setName("check_ipgeo");
  #endif
  #ifdef COROUTINE_PROFILER 
  printProfiling.setName("profiler");
  LogBinProfiler::createProfilers();
  #endif
  ESP_EARLY_LOGD(TAG, "Initializing the system clock...");
  Wire.begin();
  wireInterface.begin();
  systemClock.setup();
  dsClock.setup();
  gpsClock.setup();
  printSystemZonedTime();
  ESP_EARLY_LOGD(TAG, "Initializing IotWebConf...");
  group1.addItem(&brightness_level);
  group1.addItem(&text_scroll_speed);
  group1.addItem(&show_date);
  group1.addItem(&datecolor);
  group1.addItem(&show_date_interval);
  group1.addItem(&disable_status);
  group1.addItem(&disable_alertflash);
  group2.addItem(&twelve_clock);
  group2.addItem(&use_fixed_tz);
  group2.addItem(&fixed_offset);
  group2.addItem(&colonflicker);
  group2.addItem(&flickerfast);
  group2.addItem(&use_fixed_clockcolor);
  group2.addItem(&fixed_clockcolor);
  group3.addItem(&fahrenheit);
  group3.addItem(&show_weather);
  group3.addItem(&weather_color);
  group3.addItem(&use_fixed_tempcolor);
  group3.addItem(&fixed_tempcolor);
  group3.addItem(&show_weather_interval);
  group3.addItem(&weather_check_interval);
  group3.addItem(&show_alert_interval);
  group3.addItem(&alert_check_interval);
  group3.addItem(&weatherapi);
  group4.addItem(&use_fixed_loc);
  group4.addItem(&fixedLat);
  group4.addItem(&fixedLon);
  group4.addItem(&ipgeoapi);
  iotWebConf.addSystemParameter(&serialdebug);
  iotWebConf.addSystemParameter(&resetdefaults);
  iotWebConf.addParameterGroup(&group1);
  iotWebConf.addParameterGroup(&group2);
  iotWebConf.addParameterGroup(&group3);
  iotWebConf.addParameterGroup(&group4);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.getApTimeoutParameter()->visible = true;
  iotWebConf.init();
  gurl = (String)"https://api.ipgeolocation.io/ipgeo?apiKey=" + ipgeoapi.value();
  ipgeo.tzoffset = 127;
  processTimezone();
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });
  cotimer.scrollspeed = map(text_scroll_speed.value(), 1, 10, 100, 10);
  ESP_EARLY_LOGD(TAG, "IotWebConf initilization is complete. Web server is ready.");
  ESP_EARLY_LOGD(TAG, "Initializing Light Sensor...");
  userbrightness = calcbright(brightness_level.value());
  current.brightness = userbrightness;
  Wire.begin(TSL2561_SDA, TSL2561_SCL);
  Tsl.begin();
  while (!Tsl.available()) {
      systemClock.loop();
      debug_print("Waiting for Light Sensor...", true);
  }
  ESP_LOGE(TAG, "No Tsl2561 found. Check wiring: SCL=%d SDA=%d", TSL2561_SCL, TSL2561_SDA);
  Tsl.on();
  Tsl.setSensitivity(true, Tsl2561::EXP_14);
  ESP_EARLY_LOGD(TAG, "Initializing GPS Module...");
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // Initialize GPS UART
  ESP_EARLY_LOGD(TAG, "Setting class timestamps...");
  bootTime = systemClock.getNow();
  lastntpcheck= systemClock.getNow() - 3601;
  cotimer.icontimer = millis(); // reset all delay timers
  checkipgeo.lastattempt = bootTime - T1Y + 60;
  checkipgeo.lastsuccess = bootTime - T1Y + 60;
  gps.timestamp = bootTime - T1Y + 60;
  gps.lastcheck = bootTime - T1Y + 60;
  gps.lockage = bootTime - T1Y + 60;
  alerts.timestamp = bootTime - T1Y + 60;
  checkalerts.lastattempt = bootTime - T1Y + 60;
  alerts.lastshown = bootTime - T1Y + 60;
  checkalerts.lastsuccess = bootTime - T1Y + 60;
  weather.timestamp = bootTime - T1Y + 60;
  checkweather.lastattempt = bootTime - T1Y + 60;
  weather.lastshown = bootTime - T1Y + 60;
  checkweather.lastsuccess = bootTime - T1Y + 60;
  gps.lat = "0";
  gps.lon = "0";
  scrolltext.position = mw;
  if (use_fixed_loc.isChecked())
    ESP_EARLY_LOGI(TAG, "Setting Fixed GPS Location Lat: %s Lon: %s", fixedLat.value(), fixedLon.value());
  processLoc();
  showclock.fstop = 250;
  if (flickerfast.isChecked())
    showclock.fstop = 20;
  ESP_EARLY_LOGD(TAG, "Initializing the display...");
  FastLED.addLeds<NEOPIXEL, HSPI_MOSI>(leds, NUMMATRIX).setCorrection(TypicalLEDStrip);
  matrix->begin();
  matrix->setBrightness(userbrightness);
  matrix->setTextWrap(false);
  ESP_EARLY_LOGD(TAG, "Display initalization complete.");
  ESP_EARLY_LOGD(TAG, "Setup initialization complete: %d ms", (millis()-timer));
  scrolltext.resetmsgtime = millis() - 60000;
  CoroutineScheduler::setup();
}

//callbacks 
void configSaved()
{
  processLoc();
  processTimezone();
  buildURLs();
  showclock.fstop = 250;
  if (flickerfast.isChecked())
    showclock.fstop = 20;
  cotimer.scrollspeed = map(text_scroll_speed.value(), 1, 10, 100, 10);
  userbrightness = calcbright(brightness_level.value());
  ESP_LOGI(TAG, "Configuration was updated.");
  if (resetdefaults.isChecked())
    resetme = true;
  if (!serialdebug.isChecked())
    Serial.println("Serial debug info has been disabled.");
  scrolltext.message = "Settings Saved";
  scrolltext.color = GREEN;
  scrolltext.active = true;
}

bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  ESP_LOGD(TAG, "Validating web form...");
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

void wifiConnected() {
  ESP_LOGI(TAG, "WiFi was connected.");
  wifi_connected = true;
  gpsClock.ntpReady();
  display_showStatus();
  matrix->show();
  systemClock.forceSync();
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
    if ((String)alerts->certainty1 == "Observed" || (String)alerts->certainty1 == "Likely")
    {
      alerts->inWarning = true;
      alerts->active = true;
      cotimer.show_alert_ready = true;
    }
    else if ((String)alerts->certainty1 == "Possible") {
      alerts->inWatch= true;
      alerts->active = true;
      cotimer.show_alert_ready = true;
    } 
    else {
      alerts->active = false;
      alerts->inWarning = false;
      alerts->inWatch= false;     
    }
    ESP_LOGI(TAG, "Active weather alert received");
  }
  else {
    alerts->active = false;
    ESP_LOGI(TAG, "No current weather alerts exist");
  }
}

void fillWeatherFromJson(Weather* weather) {
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

void fillIpgeoFromJson(Ipgeo* ipgeo) {
  sprintf(ipgeo->timezone, "%s", (const char*) ipgeoJson["time_zone"]["name"]);
  ipgeo->tzoffset = ipgeoJson["time_zone"]["offset"];
  sprintf(ipgeo->lat, "%s", (const char*) ipgeoJson["latitude"]);
  sprintf(ipgeo->lon, "%s", (const char*) ipgeoJson["longitude"]);
}

bool isApiKeyValid(char *apikey) {
  if (apikey[0] == '\0')
    return false;
  else
    return true;
}

void buildURLs() {
  wurl = (String) "http://api.openweathermap.org/data/2.5/onecall?units=metric&exclude=minutely&appid=" + weatherapi.value() + "&lat=" + current.lat + "&lon=" + current.lon + "&lang=en";
  aurl = (String) "https://api.weather.gov/alerts?active=true&status=actual&point=" + current.lat + "%2C" + current.lon + "&limit=500";
}

String elapsedTime(int32_t seconds) {
  String result;
  uint8_t granularity;
  seconds = abs(seconds);
  uint32_t tseconds = seconds;
  if (seconds > T1Y - 60)
    return "never";
  if (seconds < 60)
    return (String)seconds + " seconds";
  else granularity = 2;
  uint32_t value;
  for (uint8_t i = 0; i < 6; i++)
  {
    uint32_t vint = atoi(intervals[i]);
    value = floor(seconds / vint);
    if (value != 0)
    {
      seconds = seconds - value * vint;
      if (granularity != 0) {
        result = result + value + " " + interval_names[i];
        if (granularity > 0)
          result = result + ", ";
        granularity--;
      }
    }
  }
  if (granularity != 0)
    result = result + seconds + " sec";
  return result;
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

acetime_t convertUnixEpochToAceTime(uint32_t ntpSeconds) {
    static const int32_t kDaysToConverterEpochFromNtpEpoch = 36524;
    int32_t daysToCurrentEpochFromNtpEpoch = Epoch::daysToCurrentEpochFromConverterEpoch() + kDaysToConverterEpochFromNtpEpoch;
    uint32_t secondsToCurrentEpochFromNtpEpoch = (uint32_t) 86400 * (uint32_t) daysToCurrentEpochFromNtpEpoch;
    uint32_t epochSeconds = ntpSeconds - secondsToCurrentEpochFromNtpEpoch;
    return (int32_t) epochSeconds;
 }

  void setTimeSource(String source) {
    timesource = source;
  }

  acetime_t Now() {
    return systemClock.getNow();
  }

  void resetLastNtpCheck() {
    lastntpcheck = systemClock.getNow();
  }

  uint16_t calcbright(uint16_t bl) {
  if (bl == 1)
    return 0;
  else if (bl == 2)
    return 5;
  else if (bl == 3)
    return 10;
  else if (bl == 4)
    return 20;
  else if (bl == 5)
    return 30;
  }

const char* ordinal_suffix(int n)
{
        static const char suffixes [][3] = {"th", "st", "nd", "rd"};
        auto ord = n % 100;
        if (ord / 10 == 1) { ord = 0; }
        ord = ord % 10;
        if (ord > 3) { ord = 0; }
        return suffixes[ord];
}

void handleRoot()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  acetime_t now = systemClock.getNow();
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><body style=\"background-color:#222222; font-family: Verdana, sans-serif;\">";
  s += "<style>a:link {color: #11ffff;background-color: transparent;text-decoration: none;} a:visited {color: #11ffff;background-color: transparent;text-decoration: none;} a:hover {color: #11ffff;background-color: transparent;text-decoration: underline;} a:active {color: #11ffff;background-color: transparent;text-decoration: underline;}</style>";
  s += "<span style=\"color: #ffffff; background-color: #222222;\"><table style=\"font-size: 14px; width: 500px; margin-left: auto; margin-right: auto; height: 22px;\"><tbody><tr><td style=\"width: 500px;\"><blockquote><h2 style=\"text-align: center;\"><span style=\"color: #aa11ff;\">LED Smart Clock</span><p style=\"text-align: center;\"></h2><span style=\"color: #ffff99;\"><h3><em><strong>";
  s += getSystemZonedDateTimeString();
  s += "</strong></h3></em></span></p></blockquote><p style=\"text-align: center;\"><a title=\"Configure Settings\" href=\"config\">->[ Configure Settings ]<-</a></p></td></tr></tbody></table><table style=\"width: 518px; height: 869px; margin-left: auto; margin-right: auto;\" border=\"0\"><tbody><tr style=\"height: 20px;\"><td style=\"height: 20px; text-align: right; width: 247px;\">Firmware Version:</td><td style=\"height: 20px; width: 255px;\">";
  s += (String)VERSION;
  s += "</td></tr><tr style=\"height: 20px;\"><td style=\"height: 20px; text-align: right; width: 247px;\">Uptime:</td><td style=\"height: 20px; width: 255px;\">";
  s += elapsedTime(now - bootTime);
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Time Source:</td><td style=\"height: 2px; width: 255px;\">";
  timesource.toUpperCase();
  s += timesource;
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current LED Brightness:</td><td style=\"height: 2px; width: 255px;\">";
  s += map(current.brightness, (5+userbrightness), (LUXMAX + userbrightness), 1, 100);
  s += "/100</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Timezone Offset:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)current.tzoffset;
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Last Time Sync:</td><td style=\"height: 2px; width: 255px;\">";
  s += elapsedTime(now - systemClock.getLastSyncTime());
  s += " ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Latitude:</td><td style=\"height: 2px; width: 255px;\">";
  s += current.lat;
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Longitude:</td><td style=\"height: 2px; width: 255px;\">";
  s += current.lon;
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Location Source:</td><td style=\"height: 2px; width: 255px;\">";
  s += "LOCATION_SOURCE</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">&nbsp;</td><td style=\"height: 2px; width: 255px;\">&nbsp;</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Fix:</td><td style=\"height: 2px; width: 255px;\">";
  if (gps.fix)
    s += "Yes";
  else
    s += "No";
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Aquired Sattelites:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)gps.sats;
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Fix Age:</td><td style=\"height: 2px; width: 255px;\">";
  s += elapsedTime(now - gps.timestamp);
  s += " ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Location Age:</td><td style=\"height: 2px; width: 255px;\">";
  s += elapsedTime(now - gps.lockage);
  s += " ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Precision:</td><td style=\"height: 2px; width: 255px;\">";
  s += gps.hdop;
  s += " meters</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Elevation:</td><td style=\"height: 2px; width: 255px;\">";
  s += gps.elevation;
  s += " meters</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Total Events:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)GPS.charsProcessed();
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Passed Events:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)GPS.passedChecksum();
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Failed Events:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)GPS.failedChecksum();
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Fix Events:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)GPS.sentencesWithFix();
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">&nbsp;</td><td style=\"height: 2px; width: 255px;\">&nbsp;</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Current Temp:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)weather.currentTemp;
  s += "&#8451;</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Feels Like Temp:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)weather.currentFeelsLike;
  s += "&#8451;</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Humidity:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)weather.currentHumidity;
  s += "%</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Wind Speed:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)weather.currentWindSpeed;
  s += "kph</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Conditions:</td><td style=\"height: 2px; width: 255px;\">";
  s += capString(weather.currentDescription);
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Last Forcast Check Attempt:</td><td style=\"height: 2px; width: 255px;\">";
  s += elapsedTime(now - checkweather.lastattempt);
  s += " ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Last Forcast Check Success:</td><td style=\"height: 2px; width: 255px;\">";
  s += elapsedTime(now - checkweather.lastsuccess);
  s += " ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Next Forcast Check Attempt:</td><td style=\"height: 2px; width: 255px;\">";
  
  s += "hhhh</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">&nbsp;</td><td style=\"height: 2px; width: 255px;\">&nbsp;</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Alert Active:</td><td style=\"height: 2px; width: 255px;\">";
  if (alerts.active)
    s += "Yes";
  else
    s += "No";
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Warnings:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)alerts.inWarning;
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Watches:</td><td style=\"height: 2px; width: 255px;\">";
  s += (String)alerts.inWatch;
  s += "</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Alerts Last Attempt:</td><td style=\"height: 2px; width: 255px;\">";
  s += elapsedTime(now - checkalerts.lastattempt);
  s += " ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Alerts Last Success:</td><td style=\"height: 2px; width: 255px;\">";
  s += elapsedTime(now - checkalerts.lastsuccess);
  s += " ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Last Weather Alert Shown:</td><td style=\"height: 2px; width: 255px;\">";
  s += elapsedTime(now - alerts.lastshown);
  s += " ago</td></tr></tbody></table>";
  s += "</span></body></html>\n";
  server.send(200, "text/html", s);
}


  //FIXME: string printouts on debug messages for scrolltext, etc showing garbled
  //TODO: web interface cleanup
  //TODO: add more animation frames
  //FIXME: invalid apis crashing on 401
  //FIXME: reboot after initial config

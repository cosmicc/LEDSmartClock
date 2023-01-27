#define HSPI_MOSI   23
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

#include <esp_task_wdt.h>
#include <esp_log.h>
#include <Preferences.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <IotWebConfTParameter.h>
#include <AceWire.h> 
#include <AceCommon.h>
#include <AceTime.h>
//#include <AceRoutine.h>
#include <AceTimeClock.h>
#include <TinyGPSPlus.h>
#include <SPI.h>
#include <Tsl2561.h>
#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>

// DO NOT USE DELAYS OR SLEEPS EVER! This breaks systemclock

#undef DEBUG_LIGHT                // Show light debug serial messages
#define WDT_TIMEOUT 20             // Watchdog Timeout
#define GPS_BAUD 9600              // GPS UART gpsSpeed
#define GPS_RX_PIN 16              // GPS UART RX PIN
#define GPS_TX_PIN 17              // GPS UART TX PIN
#define DAYHUE 40                  // 6am daytime hue start
#define NIGHTHUE 175               // 10pm nighttime hue end
#define CONFIG_PIN 2               // Pin for the IotWebConf config pushbutton
#define LUXMIN 5                   // Lowest brightness min
#define LUXMAX 100                 // Lowest brightness max
#define mw 32                      // Width of led matrix
#define mh 8                       // Hight of led matrix
#define ntpchecktime 3600
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

const static String error = "error";
const static String warn = "warning";
const static String info = "info";
const static String debug = "debug";
const static String verb = "verbose";

// AceTime refs
using namespace ace_time;
using ace_time::acetime_t;
using ace_time::clock::DS3231Clock;
using ace_time::clock::SystemClockLoop;

using WireInterface = ace_wire::TwoWireInterface<TwoWire>;

static char intervals[][9] = {"31536000", "2592000", "604800", "86400", "3600", "60", "1"};
static char interval_names[][9] = {"yrs", "mon", "wks", "days", "hrs", "min", "sec"};

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
void debug_print(String message, bool cr);
void logger(String type, String message);
acetime_t convertUnixEpochToAceTime(uint32_t ntpSeconds);
acetime_t gpsunixtime();
void runMaintenance();
void setTimeSource(String);
void checkGpsSerial();
void resetLastNtpCheck();
acetime_t Now();

#include "src/structures/structures.h"
#include "src/colors/colors.h"
#include "src/bitmaps/bitmaps.h"

// Global Variables & Objects
const char thingName[] = "LedSmartClock";           // Default SSID used for new setup
const char wifiInitialApPassword[] = "setmeup";     // Default AP password for new setup
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
String wurl;                    // Built Openweather API URL
String aurl;                    // Built AWS API URL
String gurl;                    // Built IPGeolocation.io API URL
uint8_t currbright;             // Current calculated brightness level
bool clock_display_offset;      // Clock display offset for single digit hour
bool wifi_connected;            // Wifi is connected or not
bool displaying_alert;          // Alert is displaying or ready to display
bool displaying_weather;        // Weather is displaying 
bool colon;                     // colon on/off status
uint8_t currhue;                // Current calculated hue based on time of day
String currlat = "0";                 // Current calculated latitude
String currlon = "0";                 // Current calculated longitude
uint8_t temphue;                // Current calculated hue based on temp
TimeZone currtz;                // Current calculated timezone
uint8_t iconcycle;              // Current Weather animation cycle
acetime_t bootTime;             // boot time
String timesource = "none";     // Primary timeclock source gps/ntp
acetime_t lastntpcheck;         // Last time ntp was checked in clock class
acetime_t timeage;              // Last time the time was updated
char fixedTz[32];

uint32_t tstimer, debugTimer, wicon_time = 0L; // Delay timers

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
    bool ntpIsReady = false;
    bool gpsready = false;

  public:
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

  acetime_t getNow() const {
    if (GPS.time.isUpdated())
        readResponse();
    if (!ntpIsReady || !wifi_connected || abs(Now() - lastntpcheck) > ntpchecktime)
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
    if (abs(Now() - lastntpcheck) > ntpchecktime) {
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
        if (GPS.time.age() < 100 || gps.fix == false) {
          setTimeSource("gps");
          ESP_LOGI(TAG, "GPSClock: GPS Time updated. Age: %d ms", GPS.time.age());
          resetLastNtpCheck();
          auto localDateTime = LocalDateTime::forComponents(GPS.date.year(), GPS.date.month(), GPS.date.day(), GPS.time.hour(), GPS.time.minute(), GPS.time.second());
          acetime_t epoch_seconds = localDateTime.toEpochSeconds();
          return epoch_seconds;
        } else {
            ESP_LOGW(TAG, "GPS time update skipped, no gpsfix or time too old: %d ms", GPS.time.age());
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
      ESP_LOGI(TAG, "GPSClock: NtpreadResponse: ntpSeconds= %d; epochSeconds=%d", ntpSeconds, epochSeconds);
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
static SystemClockLoop systemClock(&gpsClock /*reference*/, &dsClock /*backup*/, 60, 60);  // reference & backup clock

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
  iotwebconf::IntTParameter<int8_t> fixed_offset =   
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("fixed_offset").label("Fixed GMT Hours Offset").defaultValue(-1).min(-12).max(12).step(1).placeholder("-12...+12").build();
iotwebconf::ParameterGroup group4 = iotwebconf::ParameterGroup("Location", "Location Settings");
  iotwebconf::CheckboxTParameter use_fixed_loc =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_loc").label("Use Fixed Location").defaultValue(false).build();
  iotwebconf::TextTParameter<12> fixedLat =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLat").label("Fixed latitude").defaultValue("").build();
  iotwebconf::TextTParameter<12> fixedLon =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLon").label("Fixed Longitude").defaultValue("").build();
  iotwebconf::TextTParameter<33> ipgeoapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("ipgeoapi").label("IPGeolocation.io API Key").defaultValue("").build();

void display_setBrightness() {
#ifdef DEBUG_LIGHT
    uint32_t bms = millis();
#endif
    Tsl.begin();
    if (Tsl.available())
    {
      Tsl.on();
      Tsl.setSensitivity(true, Tsl2561::EXP_14);
      uint16_t full;
      uint32_t bloop = millis();
      while (millis() - bloop < 16)
      {
        esp_task_wdt_reset();
        systemClock.loop();
        iotWebConf.doLoop();
      }
      Tsl.fullLuminosity(full);
      Tsl.off();
      uint16_t cb = map(full, 0, 500, LUXMIN, LUXMAX);
      if (cb < LUXMIN)
        cb = LUXMIN;
      else if (cb > LUXMAX)
        cb = LUXMAX;
      if (brightness_level.value() == 2)
        cb = cb + 5;
      else if (brightness_level.value() == 3)
        cb = cb + 15;
      currbright = (currbright + cb) / 2;
  #ifdef DEBUG_LIGHT
        debug_print((String) "Lux: " + full + " brightness: " + cb + " avg: " + currbright + " Exe: " + (millis()-bms), true);
  #endif
      matrix->setBrightness(currbright);
      matrix->show();
    }
    else
    {
      if (millis() - tstimer > 10000)
      {
        ESP_LOGE(TAG, "No Tsl2561 found. Check wiring: SCL=%d SDA=%d", TSL2561_SCL, TSL2561_SDA);
        tstimer = millis();
      }
    }
}

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
    currtz = TimeZone::forHours(fixed_offset.value());
    ESP_LOGD(TAG, "Using fixed timezone offset: %d", fixed_offset.value());
  }
  else if (ipgeo.tzoffset != 127) {
    currtz = TimeZone::forHours(ipgeo.tzoffset);
    ESP_LOGD(TAG, "Using ipgeolocation offset: %d", ipgeo.tzoffset);
    if (ipgeo.tzoffset != savedoffset)
    {
      ESP_LOGW(TAG, "IP Geo timezone [%d] (%s) is different then saved timezone [%d], saving new timezone", ipgeo.tzoffset, ipgeo.timezone, savedoffset);
      preferences.putShort("tzoffset", ipgeo.tzoffset);
    }
  }
  else {
    currtz = TimeZone::forHours(savedoffset);
    ESP_LOGD(TAG, "Using saved timezone offset: %d", savedoffset);
  }
  ESP_LOGV(TAG, "Process timezone complete: %d ms", (millis()-timer));
}

void processLoc(){
  uint32_t timer = millis();
  if (use_fixed_loc.isChecked())
  {
    gps.lat = fixedLat.value();
    gps.lon = fixedLon.value();
    currlat = fixedLat.value();
    currlon = fixedLon.value();
  }
  String savedlat = preferences.getString("lat", "0");
  String savedlon = preferences.getString("lon", "0");
  if (gps.lon == "0" && ipgeo.lon[0] == '\0' && currlon == "0")
  {
    currlat = savedlat;
    currlon = savedlon;
  }
  else if (gps.lon == "0" && ipgeo.lon[0] != '\0' && currlon == "0")
  {
    currlat = (String)ipgeo.lat; 
    currlon = (String)ipgeo.lon;
    ESP_LOGI(TAG, "Using IPGeo location information: Lat: %s Lon: %s", (ipgeo.lat), (ipgeo.lon));
  }
  else if (gps.lon != "0")
  {
    currlat = gps.lat;
    currlon = gps.lon;
  }
  double nlat = currlat.toDouble();
  double nlon = currlon.toDouble();
  double olat = savedlat.toDouble();
  double olon = savedlon.toDouble();
  if (abs(nlat - olat) > 0.02 || abs(nlon - olon) > 0.02) {
    ESP_LOGW(TAG, "Major location shift, saving values");
    ESP_LOGW(TAG, "Lat:[%0.6lf]->[%0.6lf] Lon:[%0.6lf]->[%0.6lf]", olat, nlat, olon, nlon);
    preferences.putString("lat", currlat);
    preferences.putString("lon", currlon);
  }
  if (olat == 0 || nlat == 0) {
    preferences.putString("lat", currlat);
    preferences.putString("lon", currlon);
  }
  buildURLs();
  ESP_LOGD(TAG, "Process location complete: %d ms", (millis()-timer));
}

void gps_checkData() {
  uint32_t mgps = millis();
  ESP_LOGV(TAG, "Checking GPS data...");
  checkGpsSerial();
  if (GPS.satellites.isUpdated()) {
    gps.sats = GPS.satellites.value();
    ESP_LOGD(TAG, "GPS Satellites updated: %d", gps.sats);
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
  if (GPS.location.isUpdated()) {
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
  if (GPS.altitude.isUpdated()) {
    gps.elevation = GPS.altitude.feet();
    ESP_LOGV(TAG, "GPS Elevation updated: %f feet", gps.elevation);
  }
  if (GPS.hdop.isUpdated()) {
    gps.hdop = GPS.hdop.hdop();
    ESP_LOGV(TAG, "GPS HDOP updated: %f m",gps.hdop);
  }
  if (GPS.charsProcessed() < 10) ESP_LOGE(TAG, "No GPS data. Check wiring.");
  ESP_LOGV(TAG, "GPS data check complete: %d ms", (millis()-mgps));
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

void display_setStatus() {
  uint16_t clr;
  uint16_t wclr;
  if (timesource == "gps" && gps.fix == true)
      clr = BLACK;
  else if (timesource == "gps" && gps.fix == false && (Now() - timeage) > 3600)
      clr = DARKBLUE;
  else if (gps.fix == true && timesource == "ntp")
      clr = DARKPURPLE;
  else if (timesource == "ntp")
    clr = DARKGREEN;
  else if (wifi_connected)
    clr = DARKYELLOW;
  else clr = DARKRED;
  if (alerts.inWarning)
    wclr = RED;
  else if (alerts.inWatch)
    wclr = YELLOW;
  else
    wclr = BLACK;
  matrix->drawPixel(0, 7, clr);
  matrix->drawPixel(0, 0, wclr);
}

void display_alertFlash(uint16_t clr, uint8_t laps) {
  uint32_t timer = millis();
  ESP_LOGD(TAG, "Displaying alert flash [%d] %d times", clr, laps);
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
  ESP_LOGV(TAG, "Alert flash complete: %d ms", (millis()-timer));
}

void display_scrollWeather(String message, uint8_t spd, uint16_t clr) {
  uint32_t timer = millis();
  ESP_LOGD(TAG, "Scrolling weather text: %s", message);
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
    matrix->setCursor(x, 0);
    matrix->setTextColor(clr);
    matrix->print(capString(message));
    display_weatherIcon();
    matrix->show();
    }
  }
    ESP_LOGV(TAG, "Scrolling text complete: %d ms", (millis()-timer));
}

void display_scrollText(String message, uint8_t spd, uint16_t clr) {
  uint32_t timer = millis();
  ESP_LOGD(TAG, "Scrolling fullscreen text: %s", message);
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
      matrix->setCursor(x, 0);
      matrix->setTextColor(clr);
      matrix->print(message);
      matrix->show();
    }
  }
    ESP_LOGV(TAG, "Scrolling text complete: %d ms", (millis()-timer));
}

void display_weather() {
    display_scrollWeather(weather.currentDescription, text_scroll_speed.value(), WHITE);
    uint32_t lp = millis();
    uint32_t timer = millis();
    while (millis() - lp < 10000) {
      matrix->clear();
      display_weatherIcon();
      display_temperature();
      display_setStatus();
      matrix->show();
      runMaintenance();
    }
    ESP_LOGV(TAG, "Display temperature complete: %d ms", (millis()-timer));
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
        ESP_LOGE(TAG, "No Weather icon found to use");

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

String getSystemTimeString() {
  ace_time::ZonedDateTime now = getSystemTime();
  char *string;
  sprintf(string, "%d/%d/%d %d:%d:%d [%d]", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second(), now.timeOffset());
  return (String)string;
}

void wifiConnected() {
  ESP_LOGI(TAG, "WiFi was connected.");
  wifi_connected = true;
  display_setStatus();
  matrix->show();
  gpsClock.setup();
  net_getIpgeo();
  processTimezone();
  //net_getWeather();
  //if (alert_check_interval.value() != 0)
  //  net_getAlerts();
}



void runMaintenance() {
  esp_task_wdt_reset();
  systemClock.loop();
  display_setBrightness();
}

void checkGpsSerial() {
  while (Serial1.available() > 0)
    GPS.encode(Serial1.read());
}

void loop() {
  runMaintenance();
  gps_checkData();
  acetime_t now = systemClock.getNow();
  if (abs(now - bootTime) > (T1Y - 60)) {
    ESP_LOGE(TAG, "1 year system reset!");
    //ESP.restart();
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
    runMaintenance();
    display_showClock();
    runMaintenance();
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
        runMaintenance();
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
        display_setStatus();
        matrix->show();
        colon = false;
      }
        ldt = getSystemTime();
        runMaintenance();
    }
    runMaintenance();
    }
  // looping tasks start here
// check weather forcast
  if ((abs(now - weather.lastsuccess) > (weather_check_interval.value()*T1M)) && weather_check_interval.value() != 0)
  { 
    if (abs(now - weather.lastattempt) > T1M)
      net_getWeather();
  }
// Check weather alerts
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
    if ((abs(now - weather.lastshown) > (weather_show_interval.value() * T1M)) && weather_show_interval.value() != 0)
    {

    if (abs(now - weather.timestamp) < T2H)
      displaying_weather = true;
  }
// debug info
  if (serialdebug.isChecked()) {
    if (millis() - debugTimer > 30000) { 
      runMaintenance();
      debugTimer = millis();
      uint32_t timer = millis();
      debug_print("----------------------------------------------------------------", true);
      printSystemTime();
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
      String npt = elapsedTime(ntpchecktime - (now - lastntpcheck));
      String ooo = elapsedTime(now - timeage);
      debug_print((String) "System - Brightness:" + currbright + " | ClockHue:" + currhue + " | TempHue:" + temphue + " | Uptime:" + uptime, true);
      debug_print((String) "Clock - Status:" + systemClock.getSyncStatusCode() + " | TimeSource:" + timesource + " | LastSync:" + lst + " | LastTry:" + elapsedTime(systemClock.getSecondsSinceSyncAttempt()) +" | NextTry:" + elapsedTime(systemClock.getSecondsToSyncAttempt()) +" | Skew:" + systemClock.getClockSkew() + " sec | NextNtp:" + npt + " | TimeAge:" + ooo, true);
      debug_print((String) "Loc - SavedLat:" + preferences.getString("lat", "") + " | SavedLon:" + preferences.getString("lon", "") + " | CurrentLat:" + currlat + " | CurrentLon:" + currlon, true);
      debug_print((String) "IPGeo - Lat:" + ipgeo.lat + " | Lon:" + ipgeo.lon + " | TZoffset:" + ipgeo.tzoffset + " | Timezone:" + ipgeo.timezone + " | Age:" + igt, true);
      debug_print((String) "GPS - Chars:" + GPS.charsProcessed() + " | With-Fix:" + GPS.sentencesWithFix() + " | Failed:" + GPS.failedChecksum() + " | Passed:" + GPS.passedChecksum() + " | Sats:" + gps.sats + " | Hdop:" + gps.hdop + " | Elev:" + gps.elevation + " | Lat:" + gps.lat + " | Lon:" + gps.lon + " | FixAge:" + gage + " | LocAge:" + loca, true);
      debug_print((String) "Weather - Icon:" + weather.currentIcon + " | Temp:" + weather.currentTemp + " | FeelsLike:" + weather.currentFeelsLike + " | Humidity:" + weather.currentHumidity + " | Wind:" + weather.currentWindSpeed + " | Desc:" + weather.currentDescription + " | LastTry:" + wlt + " | LastSuccess:" + wlg + " | LastShown:" + wls + " | Age:" + wage, true);
      debug_print((String) "Alerts - Active:" + alerts.active + " | Watch:" + alerts.inWatch + " | Warn:" + alerts.inWarning + " | LastTry:" + alt + " | LastSuccess:" + alg + " | LastShown:" + als, true);
      if (alerts.active)
        debug_print((String) "*Alert1 - Status:" + alerts.status1 + " | Severity:" + alerts.severity1 + " | Certainty:" + alerts.certainty1 + " | Urgency:" + alerts.urgency1 + " | Event:" + alerts.event1 + " | Desc:" + alerts.description1, true);
      debug_print((String) "Debug info time: " + (millis() - timer) + " ms", true);
      runMaintenance();
    }
  }
}

void setup () {
  Serial.begin(115200);
  uint32_t timer = millis();
  ESP_EARLY_LOGD(TAG, "Initializing Hardware Watchdog...");
  esp_task_wdt_init(WDT_TIMEOUT, true);                //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                              //add current thread to WDT watch
  ESP_EARLY_LOGD(TAG, "Hardware Watchdog initilization complete.");
  ESP_EARLY_LOGD(TAG, "Reading stored preferences...");
  preferences.begin("location", false);
  ESP_EARLY_LOGD(TAG, "Stored preferences read complete.");
  ESP_EARLY_LOGD(TAG,"Initializing IotWebConf...");
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
  group3.addItem(&fixed_offset);
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
  ipgeo.tzoffset = 127;
  processTimezone();
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });
  ESP_EARLY_LOGD(TAG, "IotWebConf initilization is complete. Web server is ready.");
  ESP_EARLY_LOGD(TAG, "Initializing the system clock...");
  Wire.begin();
  wireInterface.begin();
  systemClock.setup();
  dsClock.setup();
  ESP_EARLY_LOGD(TAG, "System clock initilization complete.");
  //String ts = getSystemTimeString(); // FIXME:
  //ESP_LOGI(TAG, "System time: %s", ts);
  ESP_EARLY_LOGD(TAG, "Initializing Light Sensor...");
  Wire.begin(TSL2561_SDA, TSL2561_SCL);
  ESP_EARLY_LOGD(TAG, "Light Sensor initilization complete.");
  gps.lat = "0";
  gps.lon = "0";
  if (use_fixed_loc.isChecked())
    ESP_EARLY_LOGI(TAG, "Setting Fixed GPS Location Lat: %s Lon: %s", fixedLat.value(), fixedLon.value());
  processLoc();
  ESP_EARLY_LOGD(TAG, "Initializing the display...");
  display_setHue();
  FastLED.addLeds<NEOPIXEL, HSPI_MOSI>(leds, NUMMATRIX).setCorrection(TypicalLEDStrip);
  matrix->begin();
  matrix->setTextWrap(false);
  ESP_EARLY_LOGD(TAG, "Display initalization complete.");
  ESP_EARLY_LOGD(TAG, "Initializing GPS Module...");
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // Initialize GPS UART
  ESP_EARLY_LOGD(TAG, "GPS Module initilization complete.");
  bootTime = systemClock.getNow();
  lastntpcheck = systemClock.getNow() - 3601;
  debugTimer, wicon_time, tstimer = millis(); // reset all delay timers
  ipgeo.timestamp = bootTime - T1Y + 60;
  gps.timestamp = bootTime - T1Y + 60;
  gps.lastcheck = bootTime - T1Y + 60;
  gps.lockage = bootTime - T1Y + 60;
  alerts.timestamp = bootTime - T1Y + 60;
  alerts.lastattempt = bootTime - T1Y + 60;
  alerts.lastshown = bootTime - T1Y + 60;
  alerts.lastsuccess = bootTime - T1Y + 60;
  weather.timestamp = bootTime - T1Y + 60;
  weather.lastattempt = bootTime - T1Y + 60;
  weather.lastshown = bootTime - T1Y + 60;
  weather.lastsuccess = bootTime - T1Y + 60;
  ESP_EARLY_LOGD(TAG, "Setup initialization complete: %d ms", (millis()-timer));
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
  processLoc();
  processTimezone();
  buildURLs();
  ESP_LOGI(TAG, "Configuration was updated.");
  if (!serialdebug.isChecked())
    Serial.println("Serial debug info has been disabled.");
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

boolean net_getAlertJSON() {
  boolean success = false;
  ESP_LOGD(TAG, "Connecting to %s", aurl);
  HTTPClient http;
  runMaintenance();
  http.begin(aurl);
  int httpCode = http.GET();
  ESP_LOGD(TAG, "HTTP code : %d", httpCode);
  if (httpCode == 200)
  {
    alertsJson = JSON.parse(http.getString());
    if (JSON.typeof(alertsJson) == "undefined") {
      ESP_LOGE(TAG, "Parsing alertJson input failed!");
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
    ESP_LOGI(TAG, "Weather Alert found");
  }
  else {
    alerts->active = false;
    ESP_LOGD(TAG, "No Alerts found");
  }
}

void net_getAlerts() {
  if (wifi_connected && (currlat).length() > 1) {
    uint32_t timer = millis();
    ESP_LOGD(TAG, "Checking weather alerts...");
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
      ESP_LOGE(TAG, "Error: JSON");
    }
    else
    {
      fillAlertsFromJson(&alerts);
      ESP_LOGD(TAG, "Weather alert check complete: %d", (millis()-timer));
      alerts.lastsuccess = systemClock.getNow();
    }
  } else {
    ESP_LOGW(TAG, "Weather alert check skipped, no wifi or loc.");
  }
  alerts.lastattempt = systemClock.getNow();
}

boolean net_getWeatherJSON() {
  boolean success = false;
  ESP_LOGD(TAG, "Connecting to: %s", wurl);
  HTTPClient http;
  runMaintenance();
  http.begin(wurl);
  int httpCode = http.GET();
  ESP_LOGD(TAG, "HTTP code : %d", httpCode);
  runMaintenance();
  if (httpCode == 200)
  {
    weatherJson = JSON.parse(http.getString());
    if (JSON.typeof(weatherJson) == "undefined") {
      ESP_LOGE(TAG, "Parsing weatherJson input failed!");
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
    uint32_t timer = millis();
    ESP_LOGD(TAG, "Checking weather forcast...");
    int64_t retries = 1;
    boolean jsonParsed = false;
    while (!jsonParsed && (retries-- > 0))
    {
      //delay(1000);
      jsonParsed = net_getWeatherJSON();
    }
    if (!jsonParsed) {
      ESP_LOGE(TAG, "Error: JSON");
    } else {
      
      net_fillWeatherValues(&weather);
      ESP_LOGD(TAG, "Weather forcast check complete: %d ms", (millis()-timer));
      weather.lastsuccess = systemClock.getNow();
    }
  } else {
    ESP_LOGW(TAG, "Weather forcast check skipped, no wifi, loc, apikey");
  }
  weather.lastattempt = systemClock.getNow();
}

boolean getIpgeoJSON() {
  boolean success = false;
  ESP_LOGD(TAG, "Connecting to: %s", gurl);
  HTTPClient http;
  runMaintenance();
  http.begin(gurl);
  int httpCode = http.GET();
  ESP_LOGD(TAG, "HTTP code : %d", httpCode);
  runMaintenance();
  if (httpCode == 200)
  {
    ipgeoJson = JSON.parse(http.getString());
    if (JSON.typeof(ipgeoJson) == "undefined") {
      ESP_LOGE(TAG, "Parsing IPGeoJson input failed!");
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
  ipgeo->tzoffset = ipgeoJson["time_zone"]["offset"];
  sprintf(ipgeo->lat, "%s", (const char*) ipgeoJson["latitude"]);
  sprintf(ipgeo->lon, "%s", (const char*) ipgeoJson["longitude"]);
  ipgeo->timestamp = systemClock.getNow();
}

void net_getIpgeo() {
  if (wifi_connected && isApiKeyValid(ipgeoapi.value()))
  {
    uint32_t timer = millis();
    ESP_LOGD(TAG, "Checking IP geolocation..");
    int64_t retries = 1;
    bool jsonParsed = false;
    while (!jsonParsed && (retries-- > 0))
    {
      //delay(1000);
      jsonParsed = getIpgeoJSON();
    }
    if (!jsonParsed) {
      ESP_LOGE(TAG, "IPGeo Error: JSON");
    } else {
      fillIpgeoFromJson(&ipgeo);
      ESP_LOGD(TAG, "IP Geolocation check complete: %d ms", (millis()-timer));
      processTimezone();
      processLoc();
    }
  }
  else
  {
    ESP_LOGW(TAG, "IP Geolocation check skipped, no wifi or apikey");
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

  void resetLastNtpCheck() {
    acetime_t now = Now();
    lastntpcheck = now;
    timeage = now;
  }

  acetime_t Now() {
    return systemClock.getNow();
  }
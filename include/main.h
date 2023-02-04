// Emable SPI for FastLED
#define HSPI_MOSI   23
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0   

#include <nvs_flash.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <IotWebConfTParameter.h>
#include <IotWebConfESP32HTTPUpdateServer.h>
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

static const char* CONFIGVER = "4";// config version (advance if iotwebconf config additions to reset defaults)
#undef COROUTINE_PROFILER          // Enable the coroutine debug profiler
#define COROUTINE_STATES
#undef DEBUG_LIGHT                 // Show light debug serial messages
#undef DISABLE_WEATHERCHECK
#undef DISABLE_AIRCHECK
#undef DISABLE_ALERTCHECK          // Disable Weather Alert checks
#undef DISABLE_IPGEOCHECK          // Disable IPGEO checks
#define PROFILER_DELAY 10          // Coroutine profiler delay in seconds
#define WDT_TIMEOUT 30             // Watchdog Timeout seconds
#define CONFIG_PIN 19              // Config reset button pin
#define STATUS_PIN 2               // Use built-in ESP32 led for iotwebconf status
#define GPS_BAUD 9600              // GPS UART gpsSpeed
#define GPS_RX_PIN 16              // GPS UART RX PIN
#define GPS_TX_PIN 17              // GPS UART TX PIN
#define DAYHUE 35                  // 6am daytime hue start
#define NIGHTHUE 175               // 10pm nighttime hue end
#define LUXMIN 5                   // Lowest brightness min
#define LUXMAX 100                 // Lowest brightness max
#define mw 32                      // Width of led matrix
#define mh 8                       // Hight of led matrix
#define ANI_BITMAP_CYCLES 8        // Number of animation frames in each weather icon bitmap
#define ANI_SPEED 80             // Bitmap animation speed in ms (lower is faster)
#define NTPCHECKTIME 60            // NTP server check time in minutes
#define LIGHT_CHECK_DELAY 250      // delay for each brightness check in ms
#define STARTSHOWDELAY_LOW 60       // min seconds for startup show delay
#define STARTSHOWDELAY_HIGH 600      // max seconds for startup show delay
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


#include "bitmaps.h"

// AceTime refs
using namespace ace_routine;
using namespace ace_time;
using ace_time::acetime_t;
using ace_time::clock::DS3231Clock;
using ace_time::clock::SystemClockLoop;
using ace_routine::CoroutineScheduler;
using WireInterface = ace_wire::TwoWireInterface<TwoWire>;

static const char* TAG = "LEDSC";                // Logging tag
#include "structures.h"

// defs
static char intervals[][9] = {"31536000", "2592000", "604800", "86400", "3600", "60", "1"};
static char interval_names[][9] = {"years", "months", "weeks", "days", "hours", "minutes", "seconds"};
static char air_quality[][12] = {"N/A", "Good", "Fair", "Moderate", "Poor", "Very Poor"};
static char connection_state[][15] = {"Boot", "NotConfigured", "ApMode", "Connecting", "OnLine", "OffLine"};
static char imperial_units[][7] = {"\u2109", "Mph", "Feet"};
static char metric_units[][7] = {"\u2103", "Kph", "Meters"};
static char clock_status[][12] = {"Success", "Timeout", "Waiting"};
static char yesno[][4] = {"No", "Yes"};
static char truefalse[][6] = {"False", "True"};

// Global Variables & Class Objects
const char thingName[] = "LEDSMARTCLOCK";                 // Default SSID used for new setup
const char wifiInitialApPassword[] = "ledsmartclock";     // Default AP password for new setup
WireInterface wireInterface(Wire);                  // I2C hardware object
DS3231Clock<WireInterface> dsClock(wireInterface);  // Hardware DS3231 RTC object
CRGB leds[NUMMATRIX];           // Led matrix array object
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, mw, mh, NEO_MATRIX_BOTTOM+NEO_MATRIX_RIGHT+NEO_MATRIX_COLUMNS+NEO_MATRIX_ZIGZAG); // FastLED matrix object
TinyGPSPlus GPS;                // Hardware GPS object
Tsl2561 Tsl(Wire);              // Hardware Lux sensor object
DNSServer dnsServer;            // DNS Server object
WebServer server(80);           // Web server object for IotWebConf and OTA
HTTPUpdateServer httpUpdater;
JSONVar weatherJson;            // JSON object for weather
JSONVar alertsJson;             // JSON object for alerts
JSONVar ipgeoJson;              // JSON object for ip geolocation
JSONVar geocodeJson;    
JSONVar aqiJson;    
Weather weather;                // weather info data class
Alerts alerts;                  // wweather alerts data class
Ipgeo ipgeo;                    // ip geolocation data class
Geocode geocode;
GPSData gps; // gps data class
Checkalerts checkalerts;
Checkweather checkweather;
Checkipgeo checkipgeo;
Checkgeocode checkgeocode;
Checkaqi checkaqi;
ShowClock showclock;
CoTimers cotimer;
ScrollText scrolltext;
Alertflash alertflash;
Current current;
acetime_t lastntpcheck;
String wurl;                    // Built Openweather API URL
String aurl;                    // Built AWS API URL
String gurl;                    // Built IPGeolocation.io API URL
String curl;                    // Built geocoding url
String qurl;                    // Built air quality url
bool clock_display_offset;      // Clock display offset for single digit hour
bool resetme;                   // reset to factory defaults
acetime_t bootTime;             // boot time
String timesource = "none";     // Primary timeclock source gps/ntp
uint8_t userbrightness;         // Current saved brightness setting (from iotwebconf)
bool firsttimefailsafe;
bool httpbusy;

#include "colors.h"

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
void fillGeocodeFromJson(Geocode *geocode);
void fillAqiFromJson(Weather *weather);
void debug_print(String message, bool cr);
acetime_t convertUnixEpochToAceTime(uint32_t ntpSeconds);
void setTimeSource(String);
void resetLastNtpCheck();
void printSystemZonedTime();
void display_temperature();
void display_weatherIcon(char* icon);
void processTimezone();
acetime_t Now();
String capString(String str);
ace_time::ZonedDateTime getSystemZonedTime();
uint16_t calcbright(uint16_t bl);
String getSystemZonedDateString();
String getSystemZonedDateTimeString();
bool readyToDisplay();
const char *ordinal_suffix(int n);
char *cleanString(const char *p);

#include "iowebconf.h"
#include "gpsclock.h"
#include "coroutines.h"
#include "html.h"
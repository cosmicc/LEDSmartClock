#if !defined(ESP32)
#error "LEDSmartClock only runs on an ESP32, please make sure you have the proper board selected"
#endif

// Emable SPI for FastLED
#define HSPI_MOSI   23
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

// Versioning
#define VERSION "LED SmartClock v1.0.0"
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define VERSION_CONFIG "10"  //major&minor
// WARNING: Not advancing the config version after adding/deleting iotwebconf config options will result in system settings data corruption.  Iotwebconf will erase the config if it sees a different config version to avoid this corruption.


#undef COROUTINE_PROFILER          // Enable the coroutine debug profiler
#undef DEBUG_LIGHT                 // Show light debug serial messages
#undef DISABLE_WEATHERCHECK
#undef DISABLE_AIRCHECK
#undef DISABLE_ALERTCHECK          // Disable Weather Alert checks
#undef DISABLE_IPGEOCHECK          // Disable IPGEO checks
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
#define ANI_SPEED 80               // Bitmap animation speed in ms (lower is faster)
#define NTPCHECKTIME 60            // NTP server check time in minutes
#define LIGHT_CHECK_DELAY 100      // delay for each brightness check in ms
#define STARTSHOWDELAY_LOW 60      // min seconds for startup show delay
#define STARTSHOWDELAY_HIGH 600    // max seconds for startup show delay
#define NUMMATRIX (mw*mh)
// WebIotConf defaults
#define DEF_CLOCK_COLOR "#FF8800"
#define DEF_DATE_COLOR "#FF8800"
#define DEF_TEMP_COLOR "#FF8800"
#define DEF_SYSTEM_COLOR "#FF8800"
#define DEF_WEATHER_COLOR "#FF8800"
#define DEF_DAILY_COLOR "#FF8800"
#define DEF_AQI_COLOR "#FF8800"
#define DEF_DATE_INTERVAL 1  //hours
#define DEF_WEATHER_INTERVAL 10  //minutes
#define DEF_DAILY_INTERVAL 1  //hours
#define DEF_DATE_INTERVAL 4  //hours
#define DEF_AQI_INTERVAL 30  //minutes
#define DEF_ALERT_INTERVAL 10  //minutes
#define DEF_SCROLL_SPEED 5 
#define DEF_BRIGHTNESS_LEVEL 2

// Includes
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
#include <ArduinoJson.h>
#include "bitmaps.h"

// AceTime refs
using namespace ace_routine;
using namespace ace_time;
using ace_time::acetime_t;
using ace_time::clock::DS3231Clock;
using ace_time::clock::SystemClockLoop;
using ace_routine::CoroutineScheduler;
using WireInterface = ace_wire::TwoWireInterface<TwoWire>;

const char* TAG = "CLOCK";                             // ESP Logging tag
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
const char thingName[] = "LEDSMARTCLOCK";              // Default SSID used for new setup
const char wifiInitialApPassword[] = "ledsmartclock";  // Default AP password for new setup
char urls[6][256];                                     // Array of URLs
WireInterface wireInterface(Wire);                     // I2C hardware object
DS3231Clock<WireInterface> dsClock(wireInterface);     // Hardware DS3231 RTC object
CRGB leds[NUMMATRIX];                                  // Led matrix array object
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, mw, mh, NEO_MATRIX_BOTTOM+NEO_MATRIX_RIGHT+NEO_MATRIX_COLUMNS+NEO_MATRIX_ZIGZAG); // FastLED matrix object
TinyGPSPlus GPS;                                       // Hardware GPS object
Tsl2561 Tsl(Wire);                                     // Hardware Lux sensor object
DNSServer dnsServer;                                   // DNS Server object
WebServer server(80);                                  // Web server object for IotWebConf and OTA
HTTPClient request;
HTTPUpdateServer httpUpdater;
Weather weather;                                       // weather info data class
Alerts alerts;                                         // wweather alerts data class
Ipgeo ipgeo;                                           // ip geolocation data class
Geocode geocode;
GPSData gps;                                           // gps data class
Aqi aqi;
LastShown lastshown;
ShowReady showready;
CheckClass checkalerts;
CheckClass checkweather;
CheckClass checkipgeo;
CheckClass checkgeocode;
CheckClass checkaqi;
ShowClock showclock;
CoTimers cotimer;
ScrollText scrolltext;
Alertflash alertflash;
Current current;
acetime_t lastntpcheck;
bool clock_display_offset;                             // Clock display offset for single digit hour
acetime_t bootTime;                                    // boot time
String timesource = "none";                            // Primary timeclock source gps/ntp
uint8_t userbrightness;                                // Current saved brightness setting (from iotwebconf)
bool firsttimefailsafe;
bool httpbusy;

#include "colors.h"

// Function Declarations
void print_debugData();
void updateCoords();
void updateLocation();
void wifiConnected();
void handleRoot();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);
void buildURLs();
String elapsedTime(int32_t seconds);
void display_showStatus();
void display_setclockDigit(uint8_t bmp_num, uint8_t position, uint16_t color);
void fillAlertsFromJson(String payload);
void fillWeatherFromJson(String payload);
void fillIpgeoFromJson(String payload);
void fillGeocodeFromJson(String payload);
void fillAqiFromJson(String payload);
void debug_print(String message, bool cr);
acetime_t convertUnixEpochToAceTime(uint32_t ntpSeconds);
void setTimeSource(String);
void resetLastNtpCheck();
void printSystemZonedTime();
void display_temperature();
void display_weatherIcon(String icon);
void processTimezone();
acetime_t Now();
String capString(String str);
ace_time::ZonedDateTime getSystemZonedTime();
uint16_t calcbright(uint16_t bl);
String getSystemZonedDateString();
String getSystemZonedDateTimeString();
const char *ordinal_suffix(int n);
void cleanString(String &str);
bool httpRequest(uint8_t index);
bool isHttpReady();
bool isNextAttemptReady();
bool isLocValid(String source);
bool isCoordsValid();
bool isNextShowReady(acetime_t lastshown, uint8_t interval, uint32_t multiplier);
bool isNextAttemptReady(acetime_t lastattempt);
bool isApiValid(char *apikey);
bool connectAp(const char *apName, const char *password);
void connectWifi(const char *ssid, const char *password);
int32_t fromNow(acetime_t ctime);

#include "iowebconf.h"
#include "gpsclock.h"
#include "coroutines.h"
#include "html.h"
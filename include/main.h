// Emable SPI for FastLED
#define HSPI_MOSI   23
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

// Versioning
#define VERSION "LED SmartClock v1.0.0"
#define VERSION_MAJOR 1
#define VERSION_MINOR 0
#define VERSION_PATCH 0
#define VERSION_CONFIG "01"  //major&minor

// WARNING: Not advancing the config version after adding/deleting iotwebconf config options will result in system settings data corruption.  Iotwebconf will erase the config if it sees a different config version to avoid this corruption.

#define COROUTINE_PROFILER          // Enable the coroutine debug profiler
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
#define LUX_RANGE (LUXMAX - LUXMIN)
#define mw 32                      // Width of led matrix
#define mh 8                       // Hight of led matrix
#define ANI_BITMAP_CYCLES 8        // Number of animation frames in each weather icon bitmap
#define ANI_SPEED 100               // Bitmap animation speed in ms (lower is faster)
#define NTPCHECKTIME 60            // NTP server check time in minutes
#define LIGHT_CHECK_DELAY 100      // delay for each brightness check in ms
#define STARTSHOWDELAY_LOW 60      // min seconds for startup show delay
#define STARTSHOWDELAY_HIGH 600    // max seconds for startup show delay
#define HTTP_MAX_RETRIES 3         // number of consecutive failed http requests before disabling each service
#define NUMMATRIX (mw*mh)
// WebIotConf defaults
#define DEF_CLOCK_COLOR "#FF8800"
#define DEF_DATE_COLOR "#FF8800"
#define DEF_TEMP_COLOR "#FF8800"
#define DEF_SYSTEM_COLOR "#FF8800"
#define DEF_WEATHER_COLOR "#FF8800"
#define DEF_DAILY_COLOR "#FF8800"
#define DEF_AQI_COLOR "#FF8800"
#define DEF_WEATHER_INTERVAL 90  //minutes
#define DEF_DAILY_INTERVAL 3  //hours
#define DEF_DATE_INTERVAL 4  //hours
#define DEF_AQI_INTERVAL 120  //minutes
#define DEF_ALERT_INTERVAL 30  //minutes
#define DEF_SCROLL_SPEED 5 
#define DEF_BRIGHTNESS_LEVEL 3

// second aliases
#define T1S 1*1L  // 1 second
#define T1M 1*60L  // 1 minute
#define T5M 5*60L  // 5 minutes
#define T10M 10*60L  // 10 minutes
#define T1H 1*60*60L  // 1 hour
#define T2H 2*60*60L  // 2 hours 
#define T1D 24*60*60L  // 1 day
#define T1Y 365*24*60*60L  // 1 year

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
const char air_quality[][12] = {"N/A", "Good", "Fair", "Moderate", "Poor", "Very Poor"};
const char connection_state[][15] = {"Boot", "NotConfigured", "ApMode", "Connecting", "OnLine", "OffLine"};
const char imperial_units[][7] = {"\u2109", "Mph", "Feet", "°F"};
const char metric_units[][7] = {"\u2103", "Kph", "Meters", "°C"};
const char clock_status[][12] = {"Success", "Timeout", "Waiting"};
const char yesno[][4] = {"No", "Yes"};
const char truefalse[][6] = {"False", "True"};

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
WeatherData weather;                                   // weather info data class
Alerts alerts;                                         // wweather alerts data class
Ipgeo ipgeo;                                           // ip geolocation data class
Geocode geocode;
GPSData gps;                                           // gps data class
AqiData aqi;
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
char timesource[4] = "n/a";                            // Primary timeclock source gps/ntp
uint8_t userbrightness;                                // Current saved brightness setting (from iotwebconf)
bool firsttimefailsafe;
bool httpbusy;

#include "colors.h"
const uint32_t AQIColorLookup[] = { GREEN, YELLOW, ORANGE, RED, PURPLE, WHITE };
const uint32_t alertcolors[] = {RED, YELLOW};

// Function Declarations
void print_debugData();
void updateCoords();
void updateLocation();
void wifiConnected();
void handleRoot();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper);
void buildURLs();
String elapsedTime(uint32_t seconds);
void display_showStatus();
void display_setclockDigit(uint8_t bmp_num, uint8_t position, uint16_t color);
bool fillAlertsFromJson(String payload);
bool fillWeatherFromJson(String payload);
bool fillIpgeoFromJson(String payload);
bool fillGeocodeFromJson(String payload);
bool fillAqiFromJson(String payload);
acetime_t convertUnixEpochToAceTime(uint32_t ntpSeconds);
void setTimeSource(const String &inputString);
void resetLastNtpCheck();
void printSystemZonedTime();
void display_temperature();
void display_weatherIcon(char* icon);
void processTimezone();
acetime_t Now();
char* capString(char* str);
uint16_t calcbright(uint16_t bl);
ace_time::ZonedDateTime getSystemZonedTime();
String getSystemZonedTimestamp();
String getCustomZonedTimestamp(acetime_t now);
void getSystemZonedDateString(char* str);
char *getSystemZonedDateTimeString();
const char *ordinal_suffix(int n);
void cleanString(char *str);
void capitalize(char *str);
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
void toUpper(char *input);
String uv_index(uint8_t index);
acetime_t convert1970Epoch(acetime_t epoch1970);
String formatLargeNumber(int number);
String getHttpCodeName(int code);
bool cmpLocs(char a1[32], char a2[32]);
bool compareTimes(ZonedDateTime t1, ZonedDateTime t2);
void startFlash(uint16_t color, uint8_t laps);
void startScroll(uint16_t color, bool displayicon);
void processAqiDescription();

#include "iowebconf.h"
#include "gpsclock.h"
#include "coroutines.h"
#include "html.h"
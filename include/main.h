// Emable SPI for FastLED
#define HSPI_MOSI   23
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

#include "version.h"

#define COROUTINE_PROFILER          // Enable the coroutine debug profiler
#undef DEBUG_LIGHT                 // Show light debug serial messages
#undef DISABLE_WEATHERCHECK
#undef DISABLE_AIRCHECK
#undef DISABLE_ALERTCHECK          // Disable Weather Alert checks
#undef DISABLE_IPGEOCHECK          // Disable IPGEO checks
#define WDT_TIMEOUT 30             // Watchdog Timeout seconds
#define CONFIG_PIN 19              // Config reset button pin
#define STATUS_PIN 2               // Use built-in ESP32 led for iotwebconf status
#define DEFAULT_GPS_BAUD 9600      // Default GPS UART baud rate
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
#define DEFAULT_NTP_SERVER "pool.ntp.org"
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
#define DEF_WEATHER_INTERVAL 2   //hours
#define DEF_DAILY_INTERVAL 3  //hours
#define DEF_TEMP_INTERVAL 60  //minutes
#define DEF_TEMP_DISPLAY_DURATION 15  //seconds
#define DEF_DATE_INTERVAL 4  //hours
#define DEF_AQI_INTERVAL 120  //minutes
#define DEF_ALERT_INTERVAL 30  //minutes
#define DEF_SCROLL_SPEED 5 
#define DEF_BRIGHTNESS_LEVEL 3
#define OPENWEATHER_ONECALL_ENDPOINT "https://api.openweathermap.org/data/3.0/onecall"
#define OPENWEATHER_GEOCODE_ENDPOINT "https://api.openweathermap.org/geo/1.0/reverse"
#define OPENWEATHER_AQI_ENDPOINT "https://api.openweathermap.org/data/2.5/air_pollution/forecast"
#define WEATHER_GOV_ALERTS_ENDPOINT "https://api.weather.gov/alerts/active"
#define IPGEOLOCATION_ENDPOINT "https://api.ipgeolocation.io/ipgeo"

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
#include <Arduino.h>
#include <nvs_flash.h>
#include <esp_task_wdt.h>
#include <esp_log.h>
#include <esp_system.h>
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
#include <ArduinoJson.h>
#include <Update.h>

// AceTime refs
using namespace ace_routine;
using namespace ace_time;
using ace_time::acetime_t;
using ace_time::clock::DS3231Clock;
using ace_time::clock::SystemClockLoop;
using ace_routine::CoroutineScheduler;
using WireInterface = ace_wire::TwoWireInterface<TwoWire>;

#include "structures.h"
#include "colors.h"
#include "runtime_state.h"
#include "support.h"
#include "console_log.h"
#include "network_service.h"
#include "json_service.h"

extern const char TAG[];
/** Human-readable AQI labels indexed by the provider's AQI bucket. */
extern const char air_quality[][12];
/** Human-readable Wi-Fi/web configuration connection states. */
extern const char connection_state[][15];
/** Text labels for imperial weather units shown in logs and the web UI. */
extern const char imperial_units[][7];
/** Text labels for metric weather units shown in logs and the web UI. */
extern const char metric_units[][7];
/** Human-readable AceTime sync-state labels used in diagnostics. */
extern const char clock_status[][12];
/** Shared yes/no lookup strings for debug and status output. */
extern const char yesno[][4];
/** Shared true/false lookup strings for debug and status output. */
extern const char truefalse[][6];

/** Hostname and AP SSID used by IotWebConf. */
extern const char thingName[];
/** Initial password for the captive-portal access point before reconfiguration. */
extern const char wifiInitialApPassword[];
/** Primary I2C bus shared by the RTC and light sensor. */
extern WireInterface wireInterface;
/** DS3231 RTC driver bound to the shared I2C interface. */
extern DS3231Clock<WireInterface> dsClock;
/** Backing LED buffer for the 32x8 matrix. */
extern CRGB leds[NUMMATRIX];
/** Matrix abstraction used by rendering code to draw text and bitmaps. */
extern FastLED_NeoMatrix *matrix;
/** TinyGPS parser that consumes sentences from the hardware UART. */
extern TinyGPSPlus GPS;
/** Ambient light sensor driver used for automatic brightness control. */
extern Tsl2561 Tsl;
/** DNS server used by the captive portal while in AP mode. */
extern DNSServer dnsServer;
/** Shared HTTP server for status, config, and OTA endpoints. */
extern WebServer server;
/** Latest parsed current and daily weather data. */
extern WeatherData weather;
/** Active weather-alert set ranked from the latest weather.gov response. */
extern Alerts alerts;
/** Latest timezone and coarse location data returned by ipgeolocation.io. */
extern Ipgeo ipgeo;
/** Latest reverse-geocoded city/state/country data from OpenWeather. */
extern Geocode geocode;
/** Latest GPS fix metadata and timing information. */
extern GPSData gps;
/** Latest current and forecast air-quality measurements. */
extern AqiData aqi;
/** Timestamps describing when each rotating status item was last displayed. */
extern LastShown lastshown;
/** Flags indicating which messages or data blocks should render next. */
extern ShowReady showready;
/** Retry and success tracking for the weather-alert service. */
extern CheckClass checkalerts;
/** Retry and success tracking for the weather forecast service. */
extern CheckClass checkweather;
/** Retry and success tracking for the IP geolocation service. */
extern CheckClass checkipgeo;
/** Retry and success tracking for reverse geocoding. */
extern CheckClass checkgeocode;
/** Retry and success tracking for the air-quality service. */
extern CheckClass checkaqi;
/** Live clock-render state for the main time display coroutine. */
extern ShowClock showclock;
/** Shared timers used by multiple rendering coroutines. */
extern CoTimers cotimer;
/** Scroll-text renderer state and current message buffer. */
extern ScrollText scrolltext;
/** Full-screen flash animation state for attention-grabbing messages. */
extern Alertflash alertflash;
/** Current derived state shared across services, display, and web UI. */
extern Current current;
/** Color lookup used when AQI colors follow the measured AQI bucket. */
extern const uint32_t AQIColorLookup[];
/** Flash colors used for warning and watch alert presentations. */
extern const uint32_t alertcolors[];

/** Prints the current runtime state to the selected console output. */
void print_debugData(ConsoleMirrorPrint &out);
/** Prints the current runtime state to the default serial/web console output. */
void print_debugData();
/** Prints focused GPS receiver, parser, and fix information to the selected console output. */
void print_gpsStatus(ConsoleMirrorPrint &out);
/** Prints focused GPS receiver, parser, and fix information to the default serial/web console output. */
void print_gpsStatus();
/** Prints the retained raw NMEA tail to the selected console output. */
void print_gpsRawNmea(ConsoleMirrorPrint &out);
/** Prints the retained raw NMEA tail to the default serial/web console output. */
void print_gpsRawNmea();
/** Executes one single-character debug command from serial or the web console. */
bool handleDebugCommand(char input, ConsoleMirrorPrint &out, bool allowImmediateRestart = true);
/** Returns true when the supplied GPS UART baud is one of the supported receiver rates. */
bool isSupportedGpsBaud(uint32_t baud);
/** Returns the configured GPS UART baud, falling back to the default when invalid. */
uint32_t gpsConfiguredBaud();
/** Returns the baud rate currently active on the GPS UART. */
uint32_t gpsActiveBaud();
/** Restarts the GPS UART using the current configured baud. */
void restartGpsUart(const char *reason);
/** Resets the GPS parser state and optionally restarts the UART at the current baud. */
void resetGpsParser(const char *reason, bool restartUart = true);
/** Clears the retained raw NMEA troubleshooting buffer. */
void clearGpsRawNmea();
/** Returns the number of retained raw NMEA bytes currently buffered for troubleshooting. */
size_t gpsRawNmeaLength();
/** Returns the retained raw NMEA troubleshooting tail as plain text. */
String getGpsRawNmeaSnapshot();
/** Feeds one incoming GPS UART byte into the raw capture and TinyGPS parser. */
void processGpsSerialByte(int incomingByte);
/** Refreshes the active coordinates used by remote services. */
void updateCoords();
/** Recomputes the current location strings from the latest location sources. */
void updateLocation();
/** Callback fired after Wi-Fi connects successfully. */
void wifiConnected();
/** Callback fired after the web configuration has been saved. */
void configSaved();
/** Applies the current in-memory configuration to runtime services and schedulers. */
void applyRuntimeConfiguration();
/** Draws the status LEDs around the display edge. */
void display_showStatus();
/** Draws a single large clock digit bitmap at the selected display position. */
void display_setclockDigit(uint8_t bmp_num, uint8_t position, uint16_t color);
/** Logs the current zoned time to the serial console. */
void printSystemZonedTime();
/** Draws the current temperature block on the LED matrix. */
void display_temperature();
/** Draws the active weather icon animation on the LED matrix. */
void display_weatherIcon(char *icon);
/** Rebuilds the active timezone after config or location changes. */
void processTimezone();
/** Returns the current time converted into the active timezone. */
ace_time::ZonedDateTime getSystemZonedTime();
/** Returns the active local UTC offset in +HH:MM or -HH:MM format. */
String getSystemTimezoneOffsetString();
/** Returns the active timezone identifier or a readable fixed-offset label. */
String getSystemTimezoneName();
/** Returns true when the active timezone is currently observing DST. */
bool isSystemTimezoneDstActive();
/** Formats the current local time for logs and the web UI. */
String getSystemZonedTimestamp();
/** Formats an arbitrary timestamp using the active timezone. */
String getCustomZonedTimestamp(acetime_t now);
/** Formats the current date for scrolling text output. */
void getSystemZonedDateString(char *str);
/** Formats the current date/time for the web status page. */
String getSystemZonedDateTimeString();
/** Returns true when the requested location source contains a valid city. */
bool isLocationValid(String source);
/** Returns true when the active coordinates are usable. */
bool isCoordsValid();
/** Returns true when a display item has waited long enough to show again. */
bool isNextShowReady(acetime_t lastshown, uint32_t interval, uint32_t multiplier);
/** Returns true when enough time has passed since the last HTTP attempt. */
bool isNextAttemptReady(acetime_t lastattempt);
/** IotWebConf AP-mode callback used during first-time setup. */
bool connectAp(const char *apName, const char *password);
/** IotWebConf station-mode callback used during normal Wi-Fi connection. */
void connectWifi(const char *ssid, const char *password);
/** Starts a fullscreen alert flash sequence. */
void startFlash(uint16_t color, uint8_t laps);
/** Starts the scrolling text renderer with the current message buffer. */
void startScroll(uint16_t color, bool displayicon);

#include "iowebconf.h"
#include "gpsclock.h"
#include "html.h"

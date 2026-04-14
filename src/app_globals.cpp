#include "main.h"

// Shared string tables and log labels used throughout the firmware.
const char TAG[] = "CLOCK";
const char air_quality[][12] = {"N/A", "Good", "Fair", "Moderate", "Poor", "Very Poor"};
const char connection_state[][15] = {"Boot", "NotConfigured", "ApMode", "Connecting", "OnLine", "OffLine"};
const char imperial_units[][7] = {"\u2109", "Mph", "Feet", "°F"};
const char metric_units[][7] = {"\u2103", "Kph", "Meters", "°C"};
const char clock_status[][12] = {"Success", "Timeout", "Waiting"};
const char yesno[][4] = {"No", "Yes"};
const char truefalse[][6] = {"False", "True"};

const char thingName[] = "LEDSMARTCLOCK";
const char wifiInitialApPassword[] = "ledsmartclock";

// Hardware and protocol singletons bound to the ESP32 peripherals.
WireInterface wireInterface(Wire);
DS3231Clock<WireInterface> dsClock(wireInterface);
CRGB leds[NUMMATRIX];
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, mw, mh, NEO_MATRIX_BOTTOM + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
TinyGPSPlus GPS;
Tsl2561 Tsl(Wire);
DNSServer dnsServer;
WebServer server(80);

// Shared application state populated by services and consumed by the UI.
WeatherData weather;
Alerts alerts;
Ipgeo ipgeo;
Geocode geocode;
GPSData gps;
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

// Cross-cutting coordination state used by the service and display layers.
RuntimeState runtimeState;
NetworkService networkService;
DisplayToken displaytoken;

// Shared color lookup tables used by alert and AQI rendering.
const uint32_t AQIColorLookup[] = {GREEN, YELLOW, ORANGE, RED, PURPLE, WHITE};
const uint32_t alertcolors[] = {RED, YELLOW};

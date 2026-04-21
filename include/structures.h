#pragma once

#include <Arduino.h>
#include <AceTime.h>

using ace_time::TimeZone;
using ace_time::acetime_t;

/** Tracks the latest parsed GPS fix and timing metadata. */
struct GPSData
{
  bool fix;
  uint8_t sats;
  double lat;
  double lon;
  int32_t elevation;
  int32_t hdop;
  acetime_t timestamp;
  acetime_t lastcheck;
  acetime_t lockage;
  int32_t packetdelay;
  bool moduleDetected = false;
  bool waitingForFix = false;
  uint8_t lastReportedSats = 255;
  uint32_t firstByteMillis = 0;
  uint32_t lastByteMillis = 0;
  uint32_t lastNoDataLogMillis = 0;
  uint32_t lastProgressLogMillis = 0;
  uint32_t activeBaud = DEFAULT_GPS_BAUD;
  uint32_t parserResetCount = 0;
  uint32_t uartRestartCount = 0;
  uint32_t rawBytesCaptured = 0;
  uint32_t rawSentenceCount = 0;
  uint32_t lastResetMillis = 0;
  char lastResetReason[48]{};
};

struct RgbColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct HsvColor
{
    unsigned char h;
    unsigned char s;
    unsigned char v;
};

typedef struct {
  struct {
    char icon[4];
    int16_t temp;
    int16_t feelsLike;
    uint8_t humidity;
    char description[128];
    uint8_t windSpeed;
    uint8_t windGust;
    uint8_t uvi;
    uint8_t cloudcover;
  } current;

  struct {
    char icon[4];
    int16_t tempMin;
    int16_t tempMax;
    acetime_t sunrise;
    acetime_t sunset;
    acetime_t moonrise;
    acetime_t moonset;
    uint8_t uvi;
    uint8_t cloudcover;
    double moonPhase;
    uint8_t humidity;
    char description[128];
    uint8_t windSpeed;
    uint8_t windGust;
  } day;
} WeatherData;

typedef struct {
  struct {
    uint8_t aqi;
    uint16_t color;
    String description;
    double co; // carbon monoxide
    double no; // nitogen monoxide
    double no2; // nitrogen dioxide
    double o3; //ozone
    double so2; // sulfer dioxide
    double pm10; // particulate matter up to 10 micrometers
    double pm25; // particulate matter up to 2.5 micrometers
    double nh3; // ammonia
} current;
  struct {
    uint8_t aqi;
    String description;
    uint16_t color;
    double co;
    double no;
    double no2;
    double o3;
    double so2;
    double pm10;
    double pm25;
    double nh3;
  } day;
} AqiData;

/** Maximum number of active weather alerts retained from one weather.gov poll. */
constexpr uint8_t kMaxTrackedAlerts = 6;

/**
 * Stores one parsed weather.gov alert after classification and sort ranking.
 *
 * The firmware keeps a short ranked list so multiple simultaneous watches or
 * warnings can be rotated on the matrix and summarized in the web UI.
 */
struct AlertEntry
{
  /** weather.gov status field, typically "Actual". */
  char status[32]{};
  /** weather.gov severity field, such as Extreme or Severe. */
  char severity[32]{};
  /** weather.gov certainty field, such as Observed or Possible. */
  char certainty[32]{};
  /** weather.gov urgency field, such as Immediate or Expected. */
  char urgency[32]{};
  /** Event title such as Tornado Warning or Flood Watch. */
  char event[128]{};
  /** Short weather.gov headline used for concise summaries when present. */
  char headline[160]{};
  /** Longer descriptive text retained for the dashboard and diagnostics page. */
  char description[512]{};
  /** Optional instruction text returned by weather.gov. */
  char instruction[512]{};
  /** Condensed text prebuilt for the LED matrix scroller. */
  char displayText[512]{};
  /** Sort rank derived from event class keywords such as warning or watch. */
  uint8_t categoryRank = 0;
  /** Sort rank derived from the severity field. */
  uint8_t severityRank = 0;
  /** Sort rank derived from the urgency field. */
  uint8_t urgencyRank = 0;
  /** Sort rank derived from the certainty field. */
  uint8_t certaintyRank = 0;
  /** True when this alert should be treated like a warning-level event. */
  bool warning = false;
  /** True when this alert should be treated like a watch-level event. */
  bool watch = false;
};

/**
 * Stores the latest ranked set of active weather alerts returned by weather.gov.
 */
struct Alerts {
  /** True when at least one active alert is currently retained. */
  bool active = false;
  /** True when the retained alert set contains at least one watch. */
  bool inWatch = false;
  /** True when the retained alert set contains at least one warning. */
  bool inWarning = false;
  /** Number of retained active alerts currently stored in items[]. */
  uint8_t count = 0;
  /** Number of retained alerts classified as watches. */
  uint8_t watchCount = 0;
  /** Number of retained alerts classified as warnings. */
  uint8_t warningCount = 0;
  /** Index of the next alert that should be shown on the LED matrix. */
  uint8_t displayIndex = 0;
  /** Ranked active alerts with the most urgent item in slot 0. */
  AlertEntry items[kMaxTrackedAlerts]{};
  /** Time of the last successful alert download. */
  acetime_t lastsuccess = 0;
  /** Time of the last alert-request attempt. */
  acetime_t lastattempt = 0;
  /** Time when the current active alert set was last refreshed. */
  acetime_t timestamp = 0;
};

struct Ipgeo {
  int tzoffset;
  char timezone[64];
  double lat;
  double lon;
};

struct Geocode {
  char city[32];
  char state[32];
  char country[32];
};

struct CheckClass {
  bool ready;
  bool complete;
  bool firsttime = true;
  uint8_t retries;
  acetime_t lastattempt;
  acetime_t lastsuccess;
};

struct Alertflash {
  bool active;
  uint8_t fadecycle;
  uint16_t brightness;
  double multiplier;
  uint8_t lap;
  uint8_t laps;
  uint16_t color;
};

struct ScrollText {
  bool active;
  bool displayicon;
  char icon[4];
  char message[512];
  uint16_t size;
  uint16_t color;
  int16_t position;
  uint32_t millis;
  uint32_t resetmsgtime;
};

struct ShowReady {
  bool alerts;
  bool currentweather;
  bool currenttemp;
  bool date;
  bool dayweather;
  bool aqi;
  bool testalerts;
  bool testcurrentweather;
  bool testcurrenttemp;
  bool testdayweather;
  bool testaqi;
  bool reset;
  bool loc;
  bool ip;
  bool cfgupdate;
  bool sunrise;
  bool sunset;
  bool apierror;
  char apierrorname[32];
};

struct LastShown {
  acetime_t date;
  acetime_t alerts;
  acetime_t currentweather;
  acetime_t currenttemp;
  acetime_t dayweather;
  acetime_t aqi;
};

struct ShowClock {
  bool colonflicker;
  uint8_t fstop;
  uint8_t seconds;
  uint16_t color;
  uint32_t millis;
};

struct CoTimers {
  uint32_t millis;
  uint8_t scrollspeed;
  uint8_t iconcycle;
  uint32_t icontimer;
  acetime_t lasthttptime;
};

struct Current {
  uint16_t brightness; 
  uint8_t clockhue;
  double lat;
  double lon; 
  uint8_t temphue; 
  TimeZone timezone;
  int16_t tzoffset;
  uint16_t lux;
  uint16_t brightavg;
  uint16_t rawlux;
  uint16_t oldstatusclr;
  uint16_t oldstatuswclr;
  uint16_t oldaqiclr;
  uint16_t olduvicolor;
  char locsource[32];
  char city[32];
  char state[32];
  char country[32];
};

/**
 * Coordinates access to the LED matrix so only one display coroutine owns the
 * screen at a time.
 */
class DisplayToken
{
  public:
    /** Returns a comma-separated list of active tokens for debug output. */
    String showTokens() const;

    /** Returns true when the selected token is currently held. */
    bool getToken(uint8_t position) const;

    /** Marks the selected token as busy. */
    void setToken(uint8_t position);

    /** Releases the selected token. */
    void resetToken(uint8_t position);

    /** Clears every token back to the idle state. */
    void resetAllTokens();

    /** Returns true when no other display token is currently active. */
    bool isReady(uint8_t position) const;

  private:
    char name[11][32] = {"", "Date", "CurWeather", "Alerts", "Misc", "N/A", "AlertFlash", "Alertflash_test", "DayWeather", "AirQual", "ScrollText"};
    bool token1 = false;
    bool token2 = false;
    bool token3 = false;
    bool token4 = false;
    bool token5 = false;
    bool token6 = false;
    bool token7 = false;
    bool token8 = false;
    bool token9 = false;
    bool token10 = false;
};

/** Shared display-token broker that prevents overlapping matrix renderers. */
extern DisplayToken displaytoken;

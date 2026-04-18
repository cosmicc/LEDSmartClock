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

struct Alerts {
  bool active;
  bool inWatch;
  bool inWarning;
  char status1[32];
  char severity1[32];
  char certainty1[32];
  char urgency1[32];
  char event1[128];
  char description1[512];
  char instruction1[512];
  acetime_t lastsuccess;
  acetime_t lastattempt;
  acetime_t timestamp;
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

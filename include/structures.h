// second time aliases
#define T1S 1*1L  // 1 second
#define T1M 1*60L  // 1 minute
#define T5M 5*60L  // 5 minutes
#define T10M 10*60L  // 10 minutes
#define T1H 1*60*60L  // 1 hour
#define T2H 2*60*60L  // 2 hours 
#define T1D 24*60*60L  // 1 day
#define T1Y 365*24*60*60L  // 1 year

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

struct Weather {
  String currentIcon;
  int16_t currentTemp;
  int16_t currentFeelsLike;
  uint8_t currentHumidity;
  String currentDescription;
  int currentWindSpeed;
  int currentWindGust;
  String dayIcon;
  int16_t dayTempMin;
  int16_t dayTempMax;
  //acetime_t daySunrise;
  //acetime_t daySunset;
  int dayHumidity;
  String dayDescription;
  //double dayMoonPhase;
  int dayWindSpeed;
  int dayWindGust;
  acetime_t lastattempt;
  acetime_t lastsuccess;
};

struct Aqi {
  uint8_t currentaqi;
  double cur_carbon_monoxide;
  double cur_nitrogen_monoxide;
  double cur_nitrogen_dioxide;
  double cur_ozone;
  double cur_sulfer_dioxide;
  double cur_particulates_small;
  double cur_particulates_medium;
  double cur_ammonia;
  uint8_t dayaqi;
  double day_carbon_monoxide;
  double day_nitrogen_monoxide;
  double day_nitrogen_dioxide;
  double day_ozone;
  double day_sulfer_dioxide;
  double day_particulates_small;
  double day_particulates_medium;
  double day_ammonia;
};

struct Alerts {
  bool active;
  bool inWatch;
  bool inWarning;
  String status1;
  String severity1;
  String certainty1;
  String urgency1;
  String event1;
  String description1;
  String instruction1;
  acetime_t lastsuccess;
  acetime_t lastattempt;
  acetime_t timestamp;
};

struct Ipgeo {
  int tzoffset;
  String timezone;
  double lat;
  double lon;
};

struct Geocode {
  String city;
  String state;
  String country;
};

struct CheckClass {
  bool ready;
  bool jsonParsed;
  bool complete;
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
  String icon;
  String message;
  uint16_t color;
  int16_t position;
  uint32_t millis;
  uint32_t resetmsgtime;
};

struct ShowReady {
  bool alerts;
  bool currentweather;
  bool date;
  bool dayweather;
  bool aqi;
  bool reset;
  bool loc;
  bool ip;
  bool cfgupdate;
  bool apierror;
  String apierrorname;
};

struct LastShown {
  acetime_t date;
  acetime_t alerts;
  acetime_t currentweather;
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
  String locsource;
  String city;
  String state;
  String country;
};

class DisplayToken
{
  public:
    String showTokens() {
      String buf;
      if (token1)
        buf = buf + "Date(1),";
      if (token2)
        buf = buf + "CurWeather(2),";
      if (token3)
        buf = buf + "Alerts(3),";
      if (token4)
        buf = buf + "Misc(4),";
      if (token5)
        buf = buf + "5,";
      if (token6)
        buf = buf + "AlertFlash(6),";
      if (token7)
        buf = buf + "7,";
      if (token8)
        buf = buf + "DayWeather(8),";
      if (token9)
        buf = buf + "AirQual(9),";
      if (token10)
        buf = buf + "Scrolltext(10)";
      if (buf.length() == 0)
        buf = "0";
      return buf;
    }

    bool getToken(uint8_t position)
    {
      switch (position) {
        case 1:
        return token1;
        break;
        case 2:
          return token2;
          break;
        case 3:
          return token3;
          break;
        case 4:
          return token4;
          break;
        case 5:
          return token5;
          break;
        case 6:
          return token6;
          break;
        case 7:
          return token7;
          break;
        case 8:
          return token8;
          break;
        case 9:
          return token9;
          break;
        case 10:
          return token10;
          break;
        }
        return 0;
    }

    void setToken(uint8_t position)
    {
      ESP_LOGD(TAG, "Setting display token: %s(%d)", name[position], position);
      switch (position) {
        case 1:
          token1 = true;
          break;
        case 2:
          token2 = true;
          break;
        case 3:
          token3 = true;
          break;
        case 4:
          token4 = true;
          break;
        case 5:
          token5 = true;
          break;
        case 6:
          token6 = true;
          break;
        case 7:
          token7 = true;
          break;
        case 8:
          token8 = true;
          break;
        case 9:
          token9 = true;
          break;
        case 10:
          token10 = true;
          break;
        }
    }

    void resetToken(uint8_t position)
    {
      ESP_LOGD(TAG, "Releasing display token: %s(%d)", name[position],  position);
      switch (position) {
        case 1:
          token1 = false;
          break;
        case 2:
          token2 = false;
          break;
        case 3:
          token3 = false;
          break;
        case 4:
          token4 = false;
          break;
        case 5:
          token5 = false;
          break;
        case 6:
          token6 = false;
          break;
        case 7:
          token7 = false;
          break;
        case 8:
          token8 = false;
          break;
        case 9:
          token9 = false;
          break;
        case 10:
          token10 = false;
          break;
        }
    }

    void resetAllTokens()
    {
        token1 = token2 = token3 = token4 = token5 = token6 = token7 = token8 = token9 = token10 = false;
    }

    bool isReady(uint8_t position)
    {
        //ESP_LOGD(TAG, "Display token %d is requesting ready, set tokens: [%s]", position, showTokens());
        switch (position)
        {
        case 0:
           if (token1 || token2 || token3 || token4 || token5 || token6 || token7 || token8 || token9 || token10)
            return false;
          else
            return true;
          break;
        case 1:
          if (token2 || token3 || token4 || token5 || token6 || token7 || token8 || token9 || token10)
            return false;
          else
            return true;
          break;
        case 2:
          if (token1 || token3 || token4 || token5 || token6 || token7 || token8 || token9 || token10)
            return false;
          else
            return true;
          break;
        case 3:
          if (token1 || token2 || token4 || token5 || token6 || token7 || token8 || token9 || token10)
            return false;
          else
            return true;          
          break;
        case 4:
          if (token1 || token2 || token3 || token5 || token6 || token7 || token8 || token9 || token10)
            return false;
          else
            return true;          
          break;
        case 5:
          if (token1 || token2 || token3 || token4 || token6 || token7 || token8 || token9 || token10)
            return false;
          else
            return true;          
          break;
        case 6:
          if (token1 || token2 || token3 || token4 || token5 || token7 || token8 || token9 || token10)
            return false;
          else
            return true;          
          break;
        case 7:
          if (token1 || token2 || token3 || token4 || token5 || token6 || token8 || token9 || token10)
            return false;
          else
            return true;          
          break;
        case 8:
          if (token1 || token2 || token3 || token4 || token5 || token6 || token7 || token9 || token10)
            return false;          
          else
            return true;
          break;
        case 9:
          if (token1 || token2 || token3 || token4 || token5 || token6 || token7 || token8 || token10)
            return false;          
          else
            return true;
          break;
        case 10:
          if (token1 || token2 || token3 || token4 || token5 || token6 || token7 || token8 || token9)
            return false;          
          else
            return true;
          break;
        }
        return false;
    }

  private:
    char name[11][32] = {"", "Date", "CurWeather", "Alerts", "Misc", "N/A", "AlertFlash", "Alertflash_test", "DayWeather", "AirQual", "ScrollText"};
    bool token1;
    bool token2;
    bool token3;
    bool token4;
    bool token5;
    bool token6;
    bool token7;
    bool token8;
    bool token9;
    bool token10;
};

DisplayToken displaytoken;

typedef struct GPSData
{
  bool fix;
  uint8_t sats;
  String lat;
  String lon;
  int32_t elevation;
  int32_t hdop;
  acetime_t timestamp;
  acetime_t lastcheck;
  acetime_t lockage;
} GPSData;

typedef struct RgbColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RgbColor;

typedef struct HsvColor
{
    unsigned char h;
    unsigned char s;
    unsigned char v;
} HsvColor;

struct Weather {
  char currentIcon[5];
  int16_t currentTemp;
  int16_t currentFeelsLike;
  uint8_t currentHumidity;
  char currentDescription[20];
  int currentWindSpeed;
  int currentWindGust;
  char dayIcon[5];
  int16_t dayTempMin;
  int16_t dayTempMax;
  //acetime_t daySunrise;
  //acetime_t daySunset;
  int dayHumidity;
  char dayDescription[20];
  //double dayMoonPhase;
  int dayWindSpeed;
  int dayWindGust;
  uint8_t currentaqi;
  double carbon_monoxide;
  double nitrogen_monoxide;
  double nitrogen_dioxide;
  double ozone;
  double sulfer_dioxide;
  double particulates_small;
  double particulates_medium;
  double ammonia;

  acetime_t timestamp;
  acetime_t lastattempt;
  acetime_t lastshown;
  acetime_t lastsuccess;
  acetime_t lastdailyshown;
  acetime_t lastaqishown;
};

struct Alerts {
  bool active;
  bool inWatch;
  bool inWarning;
  char status1[15];
  char severity1[15];
  char certainty1[15];
  char urgency1[15];
  char event1[50];
  char description1[256];
  acetime_t timestamp;
  acetime_t lastshown;
  
};

struct Ipgeo {
  int tzoffset;
  char timezone[32];
  char lat[12];
  char lon[12];
};

struct Geocode {
  char city[32];
  char state[32];
  char country[32];
};

struct Checkalerts {
  bool active;
  bool complete;
  bool jsonParsed;
  uint8_t retries;
  acetime_t lastattempt;
  acetime_t lastsuccess;
  
};

struct Checkweather {
  bool active;
  bool complete;
  bool jsonParsed;
  uint8_t retries;
  acetime_t lastattempt;
  acetime_t lastsuccess;
};

struct Checkaqi {
  bool active;
  bool complete;
  bool jsonParsed;
  uint8_t retries;
  acetime_t lastattempt;
  acetime_t lastsuccess;
};

struct Checkipgeo {
  bool active;
  bool complete;
  bool jsonParsed;
  uint8_t retries;
  acetime_t lastattempt;
  acetime_t lastsuccess;
};

struct Checkgeocode {
  bool active;
  bool ready;
  bool complete;
  bool jsonParsed;
  uint8_t retries;
  acetime_t lastattempt;
  acetime_t lastsuccess;
};

struct Alertflash {
  bool active;
  uint8_t lap;
  uint8_t laps;
  uint16_t color;
};

struct ScrollText {
  bool active;
  bool displayicon;
  bool tempshown;
  bool showingreset;
  bool showingloc;
  bool showingip;
  bool showingcfg;
  char icon[5];
  String message;
  uint16_t color;
  int16_t position;
  uint32_t millis;
  uint32_t resetmsgtime;
};

struct ShowClock {
  uint8_t fstop;
  uint32_t millis;
  uint8_t seconds;
  bool colonflicker;
  bool colonoff;
  acetime_t datelastshown;
  uint16_t color;
};

struct CoTimers {
  bool firstboot;
  uint8_t flashcycles;
  uint32_t millis;
  uint8_t scrollspeed;
  uint32_t scrollsize;
  int16_t scrolliters;
  bool show_alert_ready;
  bool show_weather_ready;
  bool show_date_ready;
  bool show_weather_daily_ready;
  bool show_airquality_ready;
  uint8_t iconcycle;
  uint32_t icontimer;
  uint32_t iotloop;
  acetime_t lasthttprequest;
};

struct Current {
  uint16_t brightness; 
  uint8_t clockhue;
  String lat = "0";
  String lon = "0"; 
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
    char name[11][32] = {"", "Date", "CurWeather", "Alerts", "Misc", "N/A", "AlertFlash", "", "DayWeather", "AirQual", "ScrollText"};
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

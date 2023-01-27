typedef struct GPSData
{
  bool fix;
  uint8_t sats;
  String lat;
  String lon;
  double elevation;
  double hdop;
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
  int currentTemp;
  int currentFeelsLike;
  int currentHumidity;
  char currentDescription[20];
  int currentWindSpeed;

  char iconD[5];
  int tempMinD;
  int tempMaxD;
  int humidityD;
  char descriptionD[20];
  int windSpeedD;
  int windGustD;

  char iconD1[5];
  int tempMinD1;
  int tempMaxD1;
  int humidityD1;
  char descriptionD1[20];
  int windSpeedD1;
  int windGustD1;

  acetime_t timestamp;
  acetime_t lastattempt;
  acetime_t lastshown;
  acetime_t lastsuccess;
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

  char status2[15];
  char severity2[15];
  char certainty2[15];
  char urgency2[15];
  char event2[50];
  char description2[256];

  char status3[15];
  char severity3[15];
  char certainty3[15];
  char urgency3[15];
  char event3[50];
  char description3[256];

  acetime_t timestamp;
  acetime_t lastshown;
  
};

struct Ipgeo {
  int tzoffset;
  char timezone[32];
  char lat[12];
  char lon[12];
};

struct Checkalerts {
  int retries;
  boolean jsonParsed;
  acetime_t lastattempt;
  acetime_t lastsuccess;
};

struct Checkweather {
  int retries;
  boolean jsonParsed;
  acetime_t lastattempt;
  acetime_t lastsuccess;
};

struct Checkipgeo {
  int retries;
  boolean jsonParsed;
  acetime_t lastattempt;
  acetime_t lastsuccess;
  bool complete;
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
  String message;
  uint32_t color;
  int16_t position;
  uint32_t millis;
};

struct ShowClock {
  uint8_t fstop;
  uint32_t millis;
  uint8_t seconds;
  bool colonflicker;
  bool colonoff;
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
};
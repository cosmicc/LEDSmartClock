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
    double pm10; // particulates small
    double pm25; // particulates medium
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
  char timezone[32];
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

String getHttpCodeName(int code) {
  switch (code) {
    case HTTP_CODE_CONTINUE: return "Continue";
    case HTTP_CODE_SWITCHING_PROTOCOLS: return "Switching Protocols";
    case HTTP_CODE_PROCESSING: return "Processing";
    case HTTP_CODE_OK: return "OK";
    case HTTP_CODE_CREATED: return "Created";
    case HTTP_CODE_ACCEPTED: return "Accepted";
    case HTTP_CODE_NON_AUTHORITATIVE_INFORMATION: return "Non-Authoritative Information";
    case HTTP_CODE_NO_CONTENT: return "No Content";
    case HTTP_CODE_RESET_CONTENT: return "Reset Content";
    case HTTP_CODE_PARTIAL_CONTENT: return "Partial Content";
    case HTTP_CODE_MULTI_STATUS: return "Multi-Status";
    case HTTP_CODE_ALREADY_REPORTED: return "Already Reported";
    case HTTP_CODE_IM_USED: return "IM Used";
    case HTTP_CODE_MULTIPLE_CHOICES: return "Multiple Choices";
    case HTTP_CODE_MOVED_PERMANENTLY: return "Moved Permanently";
    case HTTP_CODE_FOUND: return "Found";
    case HTTP_CODE_SEE_OTHER: return "See Other";
    case HTTP_CODE_NOT_MODIFIED: return "Not Modified";
    case HTTP_CODE_USE_PROXY: return "Use Proxy";
    case HTTP_CODE_TEMPORARY_REDIRECT: return "Temporary Redirect";
    case HTTP_CODE_PERMANENT_REDIRECT: return "Permanent Redirect";
    case HTTP_CODE_BAD_REQUEST: return "Bad Request";
    case HTTP_CODE_UNAUTHORIZED: return "Unauthorized";
    case HTTP_CODE_PAYMENT_REQUIRED: return "Payment Required";
    case HTTP_CODE_FORBIDDEN: return "Forbidden";
    case HTTP_CODE_NOT_FOUND: return "Not Found";
    case HTTP_CODE_METHOD_NOT_ALLOWED: return "Method Not Allowed";
    case HTTP_CODE_NOT_ACCEPTABLE: return "Not Acceptable";
    case HTTP_CODE_PROXY_AUTHENTICATION_REQUIRED: return "Proxy Authentication Required";
    case HTTP_CODE_REQUEST_TIMEOUT: return "Request Timeout";
    case HTTP_CODE_CONFLICT: return "Conflict";
    case HTTP_CODE_GONE: return "Gone";
    case HTTP_CODE_LENGTH_REQUIRED: return "Length Required";
    case HTTP_CODE_PRECONDITION_FAILED: return "Precondition Failed";
    case HTTP_CODE_PAYLOAD_TOO_LARGE: return "Payload Too Large";
    case HTTP_CODE_URI_TOO_LONG: return "URI Too Long";
    case HTTP_CODE_UNSUPPORTED_MEDIA_TYPE: return "Unsupported Media Type";
    case HTTP_CODE_RANGE_NOT_SATISFIABLE: return "Range Not Satisfiable";
    case HTTP_CODE_EXPECTATION_FAILED: return "Expectation Failed";
    case HTTP_CODE_MISDIRECTED_REQUEST: return "Misdirected Request";
    case HTTP_CODE_UNPROCESSABLE_ENTITY: return "Unprocessable Entity";
    case HTTP_CODE_LOCKED: return "Locked";
    case HTTP_CODE_FAILED_DEPENDENCY: return "Failed Dependency";
    case HTTP_CODE_UPGRADE_REQUIRED: return "Upgrade Required";
    case HTTP_CODE_PRECONDITION_REQUIRED: return "Precondition Required";
    case HTTP_CODE_TOO_MANY_REQUESTS: return "Too Many Requests";
    case HTTP_CODE_REQUEST_HEADER_FIELDS_TOO_LARGE: return "Request Header Fields Too Large";
    case HTTP_CODE_INTERNAL_SERVER_ERROR: return "Internal Server Error";
    case HTTP_CODE_NOT_IMPLEMENTED: return "Not Implemented";
    case HTTP_CODE_BAD_GATEWAY: return "Bad Gateway";
    case HTTP_CODE_SERVICE_UNAVAILABLE: return "Service Unavailable";
    case HTTP_CODE_GATEWAY_TIMEOUT: return "Gateway Timeout";
    case HTTP_CODE_HTTP_VERSION_NOT_SUPPORTED: return "HTTP Version Not Supported";
    case HTTP_CODE_VARIANT_ALSO_NEGOTIATES: return "Variant Also Negotiates";
    case HTTP_CODE_INSUFFICIENT_STORAGE: return "Insufficient Storage";
    case HTTP_CODE_LOOP_DETECTED: return "Loop Detected";
    case HTTP_CODE_NOT_EXTENDED: return "Not Extended";
    case HTTP_CODE_NETWORK_AUTHENTICATION_REQUIRED: return "Network Authentication Required";
    case -1: return "Connection refused";
    case -2: return "Send Header Failed";
    case -3: return "Send Payload Failed";
    case -4: return "Not Connected";
    case -5: return "Connection Lost";
    case -6: return "No Stream";
    case -7: return "No HTTP Server";
    case -8: return "Too Less RAM";
    case -9: return "Encoding Error";
    case -10: return "Stream Write Error";
    case -11: return "Read Timeout";
    default: return "Unknown Error";
  }
}

class DisplayToken
{
  public:
    char* showTokens() {
        const int BUF_LEN = 128;
        char *buf = (char *)malloc(BUF_LEN);
        uint8_t len = 0;
        if (token1) len += snprintf(buf + len, BUF_LEN - len, "Date(1),");
        if (token2) len += snprintf(buf + len, BUF_LEN - len, "CurWeather(2),");
        if (token3) len += snprintf(buf + len, BUF_LEN - len, "Alerts(3),");
        if (token4) len += snprintf(buf + len, BUF_LEN - len, "Misc(4),");
        if (token5) len += snprintf(buf + len, BUF_LEN - len, "5,");
        if (token6) len += snprintf(buf + len, BUF_LEN - len, "AlertFlash(6),");
        if (token7) len += snprintf(buf + len, BUF_LEN - len, "7,");
        if (token8) len += snprintf(buf + len, BUF_LEN - len, "DayWeather(8),");
        if (token9) len += snprintf(buf + len, BUF_LEN - len, "AirQual(9),");
        if (token10) len += snprintf(buf + len, BUF_LEN - len, "Scrolltext(10)");
        if (len == 0) snprintf(buf, BUF_LEN, "0");
        else buf[len] = '\0';
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

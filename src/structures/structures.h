typedef struct GPSData
{
  bool fix;
  uint8_t sats;
  String lat;
  String lon;
  double elevation;
  double hdop;
  acetime_t timestamp;
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
  char iconH1[10];
  char tempH1[10];
  char feelsLikeH1[10];
  char humidityH1[6];
  char descriptionH1[20];
  char windSpeedH1[6];
  char windGustH1[6];

  char iconD[10];
  char tempMinD[10];
  char tempMaxD[10];
  char humidityD[6];
  char descriptionD[20];
  char windSpeedD[6];
  char windGustD[6];

  char iconD1[10];
  char tempMinD1[10];
  char tempMaxD1[10];
  char humidityD1[6];
  char descriptionD1[20];
  char windSpeedD1[6];
  char windGustD1[6];

  char updated[20];
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
};

struct Ipgeo {
  char timezone[32];
  char lat[32];
  char lon[15];
};
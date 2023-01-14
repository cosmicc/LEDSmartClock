#define HSPI_MOSI   23
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <RtcDS3231.h>
#include <Wire.h> // TwoWire, Wire
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <TinyGPSPlus.h>
#include <SPI.h>
#include <Tsl2561.h>
#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <FastLED.h>

// Allow temporaly dithering, does not work with ESP32 right now
#ifndef ESP32
#define delay FastLED.delay
#endif

#define DEBUG true
#define WDT_TIMEOUT 10             // Watchdog Timeout
#define GPS_BAUD 9600              // GPS UART gpsSpeed
#define GPS_RX_PIN 16              // GPS UART RX PIN
#define GPS_TX_PIN 17              // GPS UART TX PIN
#define BRIGHTNESS  30

#define DAYHUE 40
#define NIGHTHUE 184

#define T3S 1*30*1000L  // 30 seconds
#define T5M 1*60*1000L  // 2 minutes
#define T1H 60*60*1000L  // 60 minutes

#define mw 32
#define mh 8
#define NUMMATRIX (mw*mh)
CRGB leds[NUMMATRIX];
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, mw, mh, 
  NEO_MATRIX_BOTTOM     + NEO_MATRIX_RIGHT +
    NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);

const int8_t   GMToffset = -5;  // - 7 = US-MST;  set for your time zone)
const uint8_t   tformat = 12; // time format can be 12 or 24 hour
const char* ntpServer = "172.25.150.1";  // NTP server

String ssid;
String password;
uint8_t sats = 0;
bool gpsfix = false;
uint8_t status = 0; // 0(red)=No Sync, 1(Yellow)=Wireless Connected, 2=(Blue)NTP Sync, 3(Blue/Green)=GPS Sync <= 3sats, 4=(Green/Blue)GPS Sync <= 7 sats. 5(Green)=GPS Sync > 7 sats
double lat;
double lon;
RtcDateTime currTime;
RtcTemperature rtctemp;
bool gpssync = false;
uint8_t currhue;
uint8_t currbright;
uint32_t l1_time = 0L ;
uint32_t l2_time = 0L ;
bool displaying_alert = false;

uint16_t RGB16(uint8_t r, uint8_t g, uint8_t b);

TaskHandle_t NETLoop; 

Preferences preferences;
RtcDS3231<TwoWire> Rtc(Wire); // instance of RTC
WiFiUDP ntpUDP;  // instance of UDP
TinyGPSPlus GPS;
Tsl2561 Tsl(Wire);
NTPClient timeClient(ntpUDP, ntpServer, GMToffset*3600);
bool colon=false;

uint16_t BLACK	=	RGB16(0, 0, 0);
uint16_t RED	  =	RGB16(255, 0, 0);
uint16_t GREEN	=	RGB16(0, 255, 0);
uint16_t BLUE 	=	RGB16(0, 0, 255);
uint16_t YELLOW	=	RGB16(255, 255, 0);
uint16_t CYAN 	=	RGB16(0, 255, 255);
uint16_t PINK	=	RGB16(255, 0, 255);
uint16_t WHITE	=	RGB16(255, 255, 255);
uint16_t ORANGE	=	RGB16(255, 128, 0);
uint16_t PURPLE	=	RGB16(128, 0, 255);
uint16_t DAYBLUE=	RGB16(0, 128, 255);
uint16_t LIME 	=	RGB16(128, 255, 0);

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

static const uint8_t PROGMEM
   num[][8] =
    {
	{   // 0
	    B011110,
	    B110011,
	    B110011,
	    B110011,
	    B110011,
	    B110011,
	    B110011,
	    B011110
			},
 	{   // 1
	    B001100,
	    B011100,
	    B001100,
	    B001100,
	    B001100,
	    B001100,
	    B001100,
	    B111111
			},
    	{   // 2
	    B011110,
	    B110011,
	    B000011,
	    B000110,
	    B001100,
	    B011000,
	    B110000,
	    B111111
			},
    	{   // 3
	    B011110,
	    B110011,
	    B000011,
	    B001110,
	    B000011,
	    B000011,
	    B110011,
	    B011110
			},
    	{   // 4
	    B000110,
	    B001100,
	    B011000,
	    B110000,
	    B110110,
	    B111111,
	    B000110,
	    B000110
			},
    	{   // 5
	    B111111,
	    B110000,
	    B110000,
	    B111110,
	    B000011,
	    B000011,
	    B110011,
	    B011110
			},
    	{   // 6
	    B011110,
	    B110011,
	    B110000,
	    B110000,
	    B111110,
	    B110011,
	    B110011,
	    B011110
			},
    	{   // 7
	    B111111,
	    B000011,
	    B000011,
	    B000110,
	    B001100,
	    B001100,
	    B001100,
	    B001100
			},
    	{   // 8
	    B011110,
	    B110011,
	    B110011,
	    B011110,
	    B110011,
	    B110011,
	    B110011,
	    B011110
			},
    	{   // 9
	    B011110,
	    B110011,
	    B110011,
	    B110011,
	    B011111,
	    B000011,
	    B110011,
	    B011110
			},     
    };

uint16_t RGB16(uint8_t r, uint8_t g, uint8_t b) {
 return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

uint16_t hsv2rgb(uint8_t hsvr)
{
    RgbColor rgb;
    unsigned char region, remainder, p, q, t;
    HsvColor hsv;
    hsv.h = hsvr;
    hsv.s = 255;
    hsv.v = 255;
    region = hsv.h / 43;
    remainder = (hsv.h - (region * 43)) * 6; 
    p = (hsv.v * (255 - hsv.s)) >> 8;
    q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;
    switch (region)
    {
        case 0:
            rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
            rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
            rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
            rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
            rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }
    return RGB16(rgb.r, rgb.g, rgb.b);
}

void display_setclockDigit(uint8_t bmp_num, uint8_t position, uint16_t color) { 
    if (position == 0) position = 0;
    else if (position == 1) position = 7;
    else if (position == 2) position = 16;
    else if (position == 3) position = 23;
    matrix->drawBitmap(position, 0, num[bmp_num], 8, 8, color);    
}



char *format( const char *fmt, ... ) {
  static char buf[128];
  va_list arg;
  va_start(arg, fmt);
  vsnprintf(buf, sizeof(buf), fmt, arg);
  buf[sizeof(buf)-1] = '\0';
  va_end(arg);
  return buf;
}

void network_connect() {
  Serial.print("Connecting to wifi...");
  WiFi.begin(ssid.c_str(), password.c_str());
  int pass = 0;
  while (WiFi.status() != WL_CONNECTED && pass < 60)
  {
    esp_task_wdt_reset();
    delay(500);
    Serial.print(".");
    pass++;
  }
  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.print("CONNECTED IP: ");
    Serial.println(WiFi.localIP());  
    Serial.println("Starting NTP time client...");
    timeClient.begin();  // Start NTP Time Client
    delay(2000);
    updateTime();
  }
}

time_t gpsunixtime() {
  struct tm t;
  t.tm_year = GPS.date.year() - 1900;
  t.tm_mon = GPS.date.month() - 1;
  t.tm_mday = GPS.date.day();
  t.tm_hour = GPS.time.hour();
  t.tm_min = GPS.time.minute();
  t.tm_sec = GPS.time.second();
  t.tm_isdst = -1;
  time_t t_of_day;
  t_of_day = mktime(&t);
  return t_of_day;
}

void display_set_brightness() {
  Tsl.begin();
  if( Tsl.available() ) {
    Tsl.on();
    Tsl.setSensitivity(true, Tsl2561::EXP_14);
    delay(16);
    uint16_t full;
    Tsl.fullLuminosity(full);
    currbright = map(full, 0, 500, 1, 50);
    Tsl.off();
    matrix->setBrightness(currbright);
  }
  else {
    Serial.print(format("No Tsl2561 found. Check wiring: SCL=%u, SDA=%u\n", TSL2561_SCL, TSL2561_SDA));
  }
}

void gps_check() {
  while (Serial1.available() > 0) GPS.encode(Serial1.read());
  if (GPS.time.isUpdated()) {
      //Serial.println("GPS Time updated");
      time_t GPStime = gpsunixtime();
      int diff = Rtc.GetDateTime() - GPStime;
      if (GPS.time.age() == 0) {
        if (diff < -1 && diff > 1) {
          Serial.println((String) "Time Diff (RTC-GPS): " + diff);
          Rtc.SetDateTime(GPStime); // Set RTC to GPS time
        }
      }
  }
  if (GPS.satellites.isUpdated()) {
    sats = GPS.satellites.value();
    //Serial.println((String)"Sats: " + sats);
    if (sats > 0 && !gpsfix)
    {
      gpsfix = true;
      Serial.println((String)"GPS fix aquired with "+sats+" satellites");
    }
    if (sats == 0) gpsfix = false;
  }
  if (GPS.location.isUpdated()) {
    lat = GPS.location.lat();
    lon = GPS.location.lng();
    //Serial.println((String)"Lat: " + lat + " Lon: " + lon);
  }
  if (GPS.charsProcessed() < 10) Serial.println("WARNING: No GPS data.  Check wiring.");
}

void updateTime() {
  Serial.println("Fetching NTP time...");
  timeClient.update();
  // NTP/Epoch time starts in 1970.  RTC library starts in 2000.
  // So subtract the 30 extra yrs (946684800UL).
  unsigned long NTPtime = timeClient.getEpochTime() - 946684800UL;
  int diff = Rtc.GetDateTime() - NTPtime;
  if (!gpsfix) {
    if (diff < 0 || diff > 0) {
      Serial.println((String) "Time Diff (RTC-NTP): " + diff);
      Serial.println("Updating NTP time to RTC...");
      Rtc.SetDateTime(NTPtime); // Set RTC to NTP time
    } else {
      Serial.println("NTP time matches RTC time, skipping NTP time update");
    }
  } else {
    Serial.println("GPS time synced, skipping NTP time update");
  }
}

void setColon(int onOff){  // turn the colon on (1) or off (0)
  uint16_t clr;
  if (onOff) {
    clr = hsv2rgb(currhue);
  }
  else if (gpsfix) {
    clr = BLACK;
  }
  else if (WiFi.status() == WL_CONNECTED) {
    clr = PURPLE;
  }
  else {
    clr = ORANGE;
  }
  matrix->drawPixel(16, 5, clr);
  matrix->drawPixel(16, 2, clr);
  matrix->show();
}

void display_alertFlash(uint16_t clr) {
  for (uint8_t i=0; i<3; i++) {
    Serial.println(clr);
    delay(250);
    matrix->clear();
    matrix->fillScreen(clr);
    matrix->show();
    delay(250);
    matrix->clear();
    matrix->show();
  }
}

void display_scrollText(String message, uint8_t spd, uint16_t clr) {
    uint8_t speed = map(spd, 1, 10, 200, 10);
    uint16_t size = message.length()*6;
    matrix->clear();
    matrix->setTextWrap(false);  // we don't wrap text so it scrolls nicely
    matrix->setTextSize(1);
    matrix->setRotation(0);
    Serial.println(size * 5);
    for (int16_t x=mw; x>=size - size - size; x--) {
	    yield();
      esp_task_wdt_reset();
	    matrix->clear();
	    matrix->setCursor(x,1);
	    matrix->setTextColor(clr);
	    matrix->print(message);      
	    matrix->show();
      delay(speed);
    }
    displaying_alert = false;
}

void display_setHue() {     // make display color change from green
  uint8_t myHours = currTime.Hour();
  uint8_t myMinutes = currTime.Minute();
  if (myHours * 60 >= 1320)
    currhue = NIGHTHUE;
  else if (myHours * 60 < 360)
    currhue = NIGHTHUE;
  else currhue = map(myHours * 60 + myMinutes, 360, 1319, DAYHUE, NIGHTHUE);
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}

void display_time() {
  matrix->clear();
  uint8_t myhours = currTime.Hour();
  uint8_t myminute = currTime.Minute();
  uint8_t myhour = myhours % 12;
  if (myhour%10 == 0) {
    display_setclockDigit(1, 0, hsv2rgb(currhue));
    display_setclockDigit(2, 1, hsv2rgb(currhue)); 
  } 
  else {
    if (myhour / 10 != 0) display_setclockDigit(myhour / 10, 0, hsv2rgb(currhue)); // set first digit of hour
    display_setclockDigit(myhour%10, 1, hsv2rgb(currhue)); // set second digit of hour
  }
  display_setclockDigit(myminute/10, 2, hsv2rgb(currhue)); // set first digig of minute
  display_setclockDigit(myminute%10, 3, hsv2rgb(currhue));  // set second digit of minute
  matrix->show();
}

<<<<<<< Updated upstream
=======
void setColon(int onOff){  // turn the colon on (1) or off (0)
  uint16_t clr;
  if (onOff) {
    clr = hsv2rgb(currhue);
  }
  else if (gpsfix) {
    clr = BLACK;
  }
  else if (WiFi.status() == WL_CONNECTED) {
    clr = BLACK;
  }
  else {
    clr = BLACK;
  }
  matrix->drawPixel(16, 5, clr);
  matrix->drawPixel(16, 2, clr);
  matrix->show();
}

void display_alertFlash(uint16_t clr) {
  for (uint8_t i=0; i<3; i++) {
    Serial.println(clr);
    delay(250);
    matrix->clear();
    matrix->fillScreen(clr);
    matrix->show();
    delay(250);
    matrix->clear();
    matrix->show();
  }
}

void display_scrollText(String message, uint8_t spd, uint16_t clr) {
    uint8_t speed = map(spd, 1, 10, 200, 10);
    uint16_t size = message.length()*6;
    matrix->clear();
    matrix->setTextWrap(false);  // we don't wrap text so it scrolls nicely
    matrix->setTextSize(1);
    matrix->setRotation(0);
    Serial.println(size * 5);
    for (int16_t x=mw; x>=size - size - size; x--) {
	    yield();
      esp_task_wdt_reset();
	    matrix->clear();
	    matrix->setCursor(x,1);
	    matrix->setTextColor(clr);
	    matrix->print(message);      
	    matrix->show();
      delay(speed);
    }
    displaying_alert = false;
}

void display_setHue() {     // make display color change from green
  uint8_t myHours = currTime.Hour();
  uint8_t myMinutes = currTime.Minute();
  if (myHours * 60 >= 1320)
    currhue = NIGHTHUE;
  else if (myHours * 60 < 360)
    currhue = NIGHTHUE;
  else currhue = map(myHours * 60 + myMinutes, 360, 1319, DAYHUE, NIGHTHUE);
}

#define countof(a) (sizeof(a) / sizeof(a[0]))

void printDateTime(const RtcDateTime& dt)
{
    char datestring[20];

    snprintf_P(datestring, 
            countof(datestring),
            PSTR("%02u/%02u/%04u %02u:%02u:%02u"),
            dt.Month(),
            dt.Day(),
            dt.Year(),
            dt.Hour(),
            dt.Minute(),
            dt.Second() );
    Serial.print(datestring);
}

void display_time() {
  matrix->clear();
  uint8_t myhours = currTime.Hour();
  uint8_t myminute = currTime.Minute();
  uint8_t myhour = myhours % 12;
  if (myhour%10 == 0) {
    display_setclockDigit(1, 0, hsv2rgb(currhue));
    display_setclockDigit(2, 1, hsv2rgb(currhue)); 
  } 
  else {
    if (myhour / 10 != 0) display_setclockDigit(myhour / 10, 0, hsv2rgb(currhue)); // set first digit of hour
    display_setclockDigit(myhour%10, 1, hsv2rgb(currhue)); // set second digit of hour
  }
  display_setclockDigit(myminute/10, 2, hsv2rgb(currhue)); // set first digig of minute
  display_setclockDigit(myminute%10, 3, hsv2rgb(currhue));  // set second digit of minute
  matrix->show();
}

>>>>>>> Stashed changes
void network_loop( void * pvParameters ) {
  esp_task_wdt_add(NULL);
  Serial.println((String)"Network loop running on CPU core: " + xPortGetCoreID());
  Serial.print("Initializing GPS Module...");
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // Initialize GPS UART
  Serial.println("SUCCESS.");
  delay(2000);
  l1_time = millis();
  if (DEBUG) l2_time = millis();
  while (true)
  {
    esp_task_wdt_reset();
    gps_check();
    if (WiFi.status() != WL_CONNECTED)
    {
      network_connect();
    }
    if (millis() - l1_time > T5M) {
      l1_time = millis();
      updateTime();
    }
    if (millis() - l2_time > T3S) {
      
      Serial.println((String) "GPS Chars: " + GPS.charsProcessed() + "|With-Fix: " + GPS.sentencesWithFix() + "|Failed-checksum: " + GPS.failedChecksum() + "|Passed-checksum: " + GPS.passedChecksum() + "|RTC-Temp: " + rtctemp.AsFloatDegF() + "F|LightLevel: " + currbright + "|GPSFix: " + gpsfix + "|Sats:" + sats + "|Lat: " + lat + "|Lon: " + lon);
    l2_time = millis();
    }
    delay(10);
  }
}

void display_loop () {
  esp_task_wdt_reset();
  display_set_brightness();
  if (!Rtc.IsDateTimeValid()) 
  {
    if (Rtc.LastError() != 0)
    {
      Serial.print("RTC communications error: ");
      Serial.println(Rtc.LastError());
    }
    else
    {
      Serial.println("RTC lost confidence in the DateTime!");
    }
    delay(100);
  }
  else if (Rtc.GetIsRunning()) {
    currTime = Rtc.GetDateTime();
    display_setHue();
    if (!displaying_alert) { 
      display_time();
      setColon(0);
      delay(100);  // wait 1 sec
      setColon(1); // turn colon off
      int mysecs= currTime.Second();
      while (mysecs == currTime.Second())
      { // now wait until seconds change
        esp_task_wdt_reset();
        delay(100);
        if (!Rtc.IsDateTimeValid())
        {
          if (Rtc.LastError() != 0)
          {
            Serial.print("RTC communications error: ");
            Serial.println(Rtc.LastError());
          }
        else
        {
          Serial.println("RTC lost confidence in the DateTime!");
        }
      }
      else if (Rtc.GetIsRunning()) {
        currTime = Rtc.GetDateTime();
      }
      }
    } else {
      display_alertFlash(RED);
      display_scrollText("Alert!!", 5, RED);
    }
}
}

void loop() {
  display_loop();
}

void setup () {
  Serial.begin(115200);
  preferences.begin("credentials", false); 
  ssid = preferences.getString("ssid", "");
  //ssid = "MS";
  password = preferences.getString("password", "");
  if (ssid == "" || password == ""){
    Serial.println("No values saved for ssid or password, needs setup, halting");
    disableCore0WDT();
    disableCore1WDT();
  } else {
    Serial.println("Wifi credentials retrieved successfully");
  }
  Serial.print("Initializing Hardware Watchdog...");
  esp_task_wdt_init(WDT_TIMEOUT, true);                //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                              //add current thread to WDT watch
  Serial.println("SUCCESS.");


  Serial.println("Initializing the RTC module...");
  Serial.print("RTC Compiled: ");
  Serial.print(__DATE__);
  Serial.println(__TIME__);
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  printDateTime(compiled);
  Serial.println();
  if (!Rtc.IsDateTimeValid()) 
  {
    if (Rtc.LastError() != 0)
    {
      Serial.print("RTC communications error: ");
      Serial.println(Rtc.LastError());
    }
    else
    {
      Serial.println("RTC lost confidence in the DateTime!");
      Rtc.SetDateTime(compiled);
    }
  }
  if (!Rtc.GetIsRunning())
  {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }
  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) 
  {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  }
  else if (now > compiled) 
  {
    Serial.println("RTC is newer than compile time. (this is expected)");
  }
  else if (now == compiled) 
  {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
  Rtc.Enable32kHzPin(false);
  Rtc.SetSquareWavePin(DS3231SquareWavePin_ModeNone);
  
  
  Wire.begin(TSL2561_SDA, TSL2561_SCL);
  Serial.println((String) "Display loop running on CPU core: " + xPortGetCoreID());
  xTaskCreatePinnedToCore(network_loop, "NETLoop", 10000, NULL, 1, &NETLoop, 0);
  Serial.println("Initializing the display...");
  FastLED.addLeds<NEOPIXEL,HSPI_MOSI>(  leds, NUMMATRIX  ).setCorrection(TypicalLEDStrip);
  matrix->begin();
  matrix->setTextWrap(false);
  matrix->setBrightness(BRIGHTNESS);
  Serial.println("Display initialization complete.");
}
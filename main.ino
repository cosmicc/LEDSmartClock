#define HSPI_MOSI   23
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <AceWire.h> // TwoWireInterface
#include <Wire.h> // TwoWire, Wire
#include <AceTime.h>
#include <AceTimeClock.h>
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

using namespace ace_time;
using ace_time::acetime_t;
using ace_time::ZonedDateTime;
using ace_time::TimeZone;
using ace_time::BasicZoneProcessor;
//using ace_time::clock::SystemClockLoop;
using ace_time::clock::DS3231Clock;
//using ace_time::clock::NtpClock;
using ace_time::zonedb::kZoneAmerica_New_York;

static BasicZoneProcessor zoneProcessor;

const char* ntpServer = "172.25.150.1";  // NTP server
String ssid;
String password;
uint8_t sats = 0;
bool gpsfix = false;
uint8_t status = 0; // 0(red)=No Sync, 1(Yellow)=Wireless Connected, 2=(Blue)NTP Sync, 3(Blue/Green)=GPS Sync <= 3sats, 4=(Green/Blue)GPS Sync <= 7 sats. 5(Green)=GPS Sync > 7 sats
double lat;
double lon;
bool gpssync = false;
uint8_t currhue;
uint8_t currbright;
uint32_t l1_time = 0L ;
uint32_t l2_time = 0L ;
bool displaying_alert = false;
bool weather_watch = false;
bool weather_warning = false;
bool colon=false;

uint16_t RGB16(uint8_t r, uint8_t g, uint8_t b);

TaskHandle_t DISPLAYLoop; 

Preferences preferences;
using WireInterface = ace_wire::TwoWireInterface<TwoWire>;
WireInterface wireInterface(Wire);
DS3231Clock<WireInterface> dsClock(wireInterface);
//NtpClock ntpClock("GH", 352345236);
//SystemClockLoop systemClock(&dsClock /*reference*/, &dsClock /*backup*/);
WiFiUDP ntpUDP;  // instance of UDP
CRGB leds[NUMMATRIX];
FastLED_NeoMatrix *matrix = new FastLED_NeoMatrix(leds, mw, mh, NEO_MATRIX_BOTTOM+NEO_MATRIX_RIGHT+NEO_MATRIX_COLUMNS+NEO_MATRIX_ZIGZAG);
TinyGPSPlus GPS;
Tsl2561 Tsl(Wire);

// Display Colors
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
      //nt diff = Rtc.GetDateTime() - GPStime;
      //if (GPS.time.age() == 0) {
      //  if (diff < -1 && diff > 1) {
      //    Serial.println((String) "Time Diff (RTC-GPS): " + diff);
      //    Rtc.SetDateTime(GPStime); // Set RTC to GPS time
      //  }
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

void setStatus() {
  uint16_t clr;
  uint16_t wclr;
  if (gpsfix) {
    clr = BLACK;
  }
  else if (WiFi.status() == WL_CONNECTED) {
    clr = BLUE;
  }
  else {
    clr = YELLOW;
  }
  if (weather_warning) {
    wclr = RED;
  }
  else if (weather_watch) {
    wclr = YELLOW;
  }
  else {
    wclr = BLACK;
  }
  matrix->drawPixel(0, 7, clr);
  matrix->drawPixel(0, 0, wclr);
  matrix->show();
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

void display_setHue() { 
  LocalDateTime ldt = LocalDateTime::forEpochSeconds(dsClock.getNow());
  uint8_t myHours = ldt.hour();
  uint8_t myMinutes = ldt.minute();
  if (myHours * 60 >= 1320)
    currhue = NIGHTHUE;
  else if (myHours * 60 < 360)
    currhue = NIGHTHUE;
  else currhue = map(myHours * 60 + myMinutes, 360, 1319, DAYHUE, NIGHTHUE);
}

void display_time() {
  LocalDateTime ldt = LocalDateTime::forEpochSeconds(dsClock.getNow());
  matrix->clear();
  uint8_t myhours = ldt.hour();
  uint8_t myminute = ldt.hour();
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
  setStatus();
  matrix->show();
}

void printSystemTime() {
  acetime_t now = dsClock.getNow();
  auto EasternTz = TimeZone::forZoneInfo(&kZoneAmerica_New_York, &zoneProcessor);
  auto ESTime = ZonedDateTime::forEpochSeconds(now, EasternTz);
  ESTime.printTo(SERIAL_PORT_MONITOR);
  SERIAL_PORT_MONITOR.println();
}

void display_loop( void * pvParameters ) {
  esp_task_wdt_add(NULL);
  Serial.println((String) "Display loop running on CPU core: " + xPortGetCoreID());
  Serial.println("Initializing the display...");
  FastLED.addLeds<NEOPIXEL,HSPI_MOSI>(  leds, NUMMATRIX  ).setCorrection(TypicalLEDStrip);
  matrix->begin();
  matrix->setTextWrap(false);
  matrix->setBrightness(BRIGHTNESS);
  Serial.println("Display initalization complete.");
  while (true){
    esp_task_wdt_reset();
    display_set_brightness();
    display_setHue();
    if (!displaying_alert) { 
      display_time();
      setColon(0);
      delay(100);  // wait 1 sec
      setColon(1); // turn colon off
      LocalDateTime ldt = LocalDateTime::forEpochSeconds(dsClock.getNow());
      uint32_t mysecs = ldt.second();
      Serial.print("mysecs: ");
      Serial.println(mysecs);
      while (mysecs == ldt.second())
      { // now wait until seconds change
        esp_task_wdt_reset();
        ldt = LocalDateTime::forEpochSeconds(dsClock.getNow());
        Serial.println(ldt.second());
        delay(250);
      }
    } else {
      display_alertFlash(RED);
      display_scrollText("Alert!!", 5, RED);
    }
  }
}

void loop() {
    //systemClock.loop();
    esp_task_wdt_reset();
    gps_check();
    if (WiFi.status() != WL_CONNECTED)
    {
      network_connect();
    }
    if (millis() - l2_time > T3S) {
      Serial.println((String) "GPS Chars: " + GPS.charsProcessed() + "|With-Fix: " + GPS.sentencesWithFix() + "|Failed-checksum: " + GPS.failedChecksum() + "|Passed-checksum: " + GPS.passedChecksum() + "F|LightLevel: " + currbright + "|GPSFix: " + gpsfix + "|Sats:" + sats + "|Lat: " + lat + "|Lon: " + lon);
      l2_time = millis();
    }
}

void setup () {
  Serial.begin(115200);
  preferences.begin("credentials", false); 
  ssid = preferences.getString("ssid", "");
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
  Serial.print("Initializing the RTC module...");
  Wire.begin();
  wireInterface.begin();
  dsClock.setup();
  //ntpClock.setup();
  //systemClock.setup();
  Serial.println("SUCCESS.");
  xTaskCreatePinnedToCore(display_loop, "DISPLAYLoop", 10000, NULL, 1, &DISPLAYLoop, 0);
  Wire.begin(TSL2561_SDA, TSL2561_SCL);
  Serial.println((String)"Logic loop running on CPU core: " + xPortGetCoreID());
  delay(50);
  Serial.print("Initializing GPS Module...");
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // Initialize GPS UART
  delay(100);
  Serial.println("SUCCESS.");
  l2_time = millis();
}
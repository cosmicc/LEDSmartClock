#define HSPI_MOSI   23
#define FASTLED_ALL_PINS_HARDWARE_SPI
#define FASTLED_ESP32_SPI_BUS HSPI

#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h> 
#include <RtcDS3231.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include <TinyGPSPlus.h>
#include <SPI.h>
#include <Tsl2561.h>
#include "FastLED.h"

#define DEBUG true
#define WDT_TIMEOUT 10             // Watchdog Timeout
#define GPS_BAUD 9600              // GPS UART gpsSpeed
#define GPS_RX_PIN 16              // GPS UART RX PIN
#define GPS_TX_PIN 17              // GPS UART TX PIN
#define COLOR_ORDER GRB
#define CHIPSET     WS2812B
#define BRIGHTNESS  30

#define T3S 1*30*1000L  // 30 seconds
#define T5M 5*60*1000L  // 2 minutes
#define T1H 60*60*1000L  // 60 minutes

#define kMatrixWidth  32
#define kMatrixHeight 8

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)

const int   GMToffset = -5;  // - 7 = US-MST;  set for your time zone)
const int   tformat = 12; // time format can be 12 or 24 hour
const char* ntpServer = "172.25.150.1";  // NTP server

String ssid;
String password;
int sats = 0;
bool gpsfix = false;
int status = 0; // 0(red)=No Sync, 1(Yellow)=Wireless Connected, 2=(Blue)NTP Sync, 3(Blue/Green)=GPS Sync <= 3sats, 4=(Green/Blue)GPS Sync <= 7 sats. 5(Green)=GPS Sync > 7 sats
double lat;
double lon;
RtcTemperature rtctemp;
bool gpssync = false;
int currhue;
int currbright;

unsigned long l1_time = 0L ;
unsigned long l2_time = 0L ;

TaskHandle_t NETLoop; 

Preferences preferences;
RtcDS3231<TwoWire> Rtc(Wire); // instance of RTC
WiFiUDP ntpUDP;  // instance of UDP
TinyGPSPlus GPS;
Tsl2561 Tsl(Wire);
NTPClient timeClient(ntpUDP, ntpServer, GMToffset*3600);
CRGB leds[NUM_LEDS];
bool colon=false;

byte myArray[8][32];  // display buffer

byte let[3][8] = {
    // our font for letters
    {0x1E,  /* 011110   letter S */
     0x33,  /* 110011 */
     0x33,  /* 110000 */
     0x33,  /* 111110 */
     0x33,  /* 011111 */
     0x33,  /* 000011 */
     0x33,  /* 110011 */
     0x1E}, /* 011110 */
    {0x0C,  /* 111111   letter M */
     0x1C,  /* 101101 */
     0x0C,  /* 101101 */
     0x0C,  /* 100001 */
     0x0C,  /* 100001 */
     0x0C,  /* 100001 */
     0x0C,  /* 100001 */
     0x1E}, /* 100001 */
    {0x1E,  /* 011110   number 2  */
     0x33,  /* 110011 */
     0x03,  /* 000011 */
     0x06,  /* 000110 */
     0x0C,  /* 001100 */
     0x18,  /* 011000 */
     0x30,  /* 110000 */
     0x3F}  /* 111111 */
};

byte num [10][8] = {  // our font for numbers 0-9
  { 0x1E,  /* 011110   number 0 */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x1E}, /* 011110 */      
  { 0x0C,  /* 001100   number 1 */
    0x1C,  /* 011100 */
    0x0C,  /* 001100 */
    0x0C,  /* 001100 */
    0x0C,  /* 001100 */
    0x0C,  /* 001100 */
    0x0C,  /* 001100 */
    0x1E}, /* 011110 */
  { 0x1E,  /* 011110   number 2  */
    0x33,  /* 110011 */
    0x03,  /* 000011 */
    0x06,  /* 000110 */
    0x0C,  /* 001100 */
    0x18,  /* 011000 */
    0x30,  /* 110000 */
    0x3F}, /* 111111 */
  { 0x1E,  /* 011110   number 3  */
    0x33,  /* 110011 */
    0x03,  /* 000011 */
    0x0E,  /* 001110 */
    0x03,  /* 000011 */
    0x03,  /* 000011 */
    0x33,  /* 110011 */
    0x1E}, /* 011110 */
  { 0x06,  /* 000110   number 4  */
    0x0C,  /* 001100 */
    0x18,  /* 011000 */
    0x30,  /* 110000 */
    0x36,  /* 110110 */
    0x3F,  /* 111111 */
    0x06,  /* 000110 */
    0x06}, /* 000110 */
  { 0x3F,  /* 111111   number 5  */
    0x30,  /* 110000 */
    0x30,  /* 110000 */
    0x3E,  /* 111110 */
    0x03,  /* 000011 */
    0x03,  /* 000011 */
    0x33,  /* 110011 */
    0x1E}, /* 011110 */
  { 0x1E,  /* 011110   number 6  */
    0x33,  /* 110011 */
    0x30,  /* 110000 */
    0x30,  /* 110000 */
    0x3E,  /* 111110 */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x1E}, /* 011110 */
  { 0x3F,  /* 111111   number 7  */
    0x03,  /* 000011 */
    0x03,  /* 000011 */
    0x06,  /* 000110 */
    0x0C,  /* 001100 */
    0x0C,  /* 001100 */
    0x0C,  /* 001100 */
    0x0C}, /* 001100 */
  { 0x1E,  /* 011110   number 8  */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x1E,  /* 011110 */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x1E}, /* 011110 */
  { 0x1E,  /* 011110   number 9  */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x33,  /* 110011 */
    0x1F,  /* 011111 */
    0x03,  /* 000011 */
    0x33,  /* 110011 */
    0x1E}  /* 011110 */
};

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

int getLumens() {
  Tsl.begin();
  if( Tsl.available() ) {
    Tsl.on();
    Tsl.setSensitivity(true, Tsl2561::EXP_14);
    delay(16);
    uint16_t full, ir;
    Tsl.fullLuminosity(full);
    Tsl.irLuminosity(ir);
    //if (DEBUG) Serial.print(format("luminosity is %5u (full) and %5u (ir)\n", full, ir));
    Tsl.off();
    return full;
  }
  else {
    Serial.print(format("No Tsl2561 found. Check wiring: SCL=%u, SDA=%u\n", TSL2561_SCL, TSL2561_SDA));
    return 0;
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
  FastLED.addLeds<CHIPSET, HSPI_MOSI, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
  FastLED.clear();  //clear the display
  FastLED.show();
  Serial.println("Display initialization complete.");
}

void loop () {
  esp_task_wdt_reset();
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
    RtcDateTime currTime = Rtc.GetDateTime();
    int lum = getLumens();
    currbright = map(lum, 0, 500, 1, 100);
    FastLED.setBrightness(currbright);
    int myhours = currTime.Hour();
    int myhour = myhours%12; 
    if (myhours==0) myhour=12;
    if (myhour / 10 != 0) setNum(myhour / 10, 0); // set first digit of hour
    setNum(myhour%10,1); // set second digit of hour
    int myminute = currTime.Minute();
    setNum(myminute/10,2); // set first digig of minute
    setNum(myminute%10,3);  // set second digit of minute
    int mysecs= currTime.Second();
    myHue(myhours, myminute);
    refreshDisplay(currhue);
    setColon(0);
    delay(100);  // wait 1 sec
    setColon(1); // turn colon off
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
    memset(myArray, 0, sizeof myArray);  // clear the display buffer 
  }
}

// Sets the LED display to match content of display buffer at a specified color (myHue).
// Takes into account the zig-zap order of the array up and down from right to left
void refreshDisplay(byte myHue) {
  //Serial.println((String)"Refeshing Display Hue: " + myHue);
  int myCount = 0;
  FastLED.clear();
  for (int x = 31; x > -1; x -= 2) {
    for (int y = 7; y > -1; y--) {
      if (myArray[y][x]) leds[myCount] = CHSV(myHue, 255, 255);
      myCount++;
    }
    for (int y = 0; y < 8; y++) {
      if (myArray[y][x - 1] == 1) leds[myCount] = CHSV(myHue, 255, 255);
      myCount++;
    }
  }
  FastLED.show();
}


// put numbers onto the display using our num[][] font in positions 0-3. 
void setNum(byte number, byte pos) {
  if (pos==0) pos=2;   // change pos to the real location in the array
  else if (pos==1) pos=8;
  else if (pos==2) pos=17;
  else if (pos==3) pos=24;
  for (int x = 0; x < 6; x++) {  // now place the digit's 6x8 font in the display buffer
    for (int y = 0; y < 8; y++) {
     if (num[number][y] & (0b00100000 >> x)) myArray[y][pos+ x] = 1;
    }
  }
}

void setColon(int onOff){  // turn the colon on (1) or off (0)
  int shue = currhue;
  int br;
  if (onOff) {
    shue = currhue;
    br = 255;
  }
  else if (gpsfix) {
    shue = currhue;
    br = 0;
  }
  else if (WiFi.status() == WL_CONNECTED) {
    shue = 192;
    br = 255;
  }
  else {
    shue = 32;
    br = 255;
  }
  leds[130] = CHSV(shue, 255, br);
  leds[133] = CHSV(shue, 255, br);
  FastLED.show();
}

void myHue(int myHours, int myMinutes) {     // make display color change from green
  int dayhue = 64;
  int nighthue = 160;
  if (myHours * 60 >= 1320) currhue = nighthue;
  else if (myHours * 60 < 360) currhue = nighthue;
  else currhue = map(myHours * 60 + myMinutes, 360, 1319, dayhue, nighthue);
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
    Serial.println(datestring);
}
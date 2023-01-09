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
#define T2M 2*60*1000L  // 2 minutes
#define T1H 60*60*1000L  // 60 minutes

#define kMatrixWidth  32
#define kMatrixHeight 8

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)

const int   GMToffset = -5;  // - 7 = US-MST;  set for your time zone)
const int   format = 12; // time format can be 12 or 24 hour
const char* ntpServer = "172.25.150.1";  // NTP server

String ssid;
String password;
int sats = 0;
bool gpsfix = false;
int status = 0;  // 0(red)=No Sync, 1(Yellow)=Wireless Connected, 2=(Blue)NTP Sync, 3(Blue/Green)=GPS Sync <= 3sats, 4=(Green/Blue)GPS Sync <= 7 sats. 5(Green)=GPS Sync > 7 sats
double lat;
double lon;
RtcTemperature rtctemp;

unsigned long l1_time = 0L ;
unsigned long l2_time = 0L ;

TaskHandle_t NETLoop; 

Preferences preferences;
RtcDS3231<TwoWire> Rtc(Wire); // instance of RTC
WiFiUDP ntpUDP;  // instance of UDP
TinyGPSPlus GPS;
NTPClient timeClient(ntpUDP, ntpServer, GMToffset*3600);
CRGB leds[NUM_LEDS];
bool colon=false;
byte myArray[8][32];  // display buffer

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

void gps_check() {
  while (Serial1.available() > 0) GPS.encode(Serial1.read());
  if (GPS.time.isUpdated()) {
      Serial.println("GPS Time updated");
      time_t GPStime = gpsunixtime() - 946684800UL - (GMToffset*3600);
      int diff = Rtc.GetDateTime() - GPStime;
      if (diff > 0 || diff < 0) {
        Serial.println((String) "Time Diff (RTC-GPS): " + diff);
        Rtc.SetDateTime(GPStime); // Set RTC to GPS time
      }
  }
  else if (GPS.satellites.isUpdated()) {
    sats = GPS.satellites.value();
    Serial.println((String)"Sats: " + sats);
    if (sats > 0 && !gpsfix)
    {
      gpsfix = true;
      Serial.println((String)"GPS fix aquired with "+sats+" satellites");
    }
  }
  else if (GPS.location.isUpdated()) {
    lat = GPS.location.lat();
    lon = GPS.location.lng();
    Serial.println((String)"Lat: " + lat + " Lon: " + lon);
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
  if (diff < 0 || diff > 0) {
    Serial.println((String) "Time Diff (RTC-NTP): " + diff);
    Serial.println("Updating NTP time to RTC...");
    Rtc.SetDateTime(NTPtime); // Set RTC to NTP time
  } else {
    Serial.println("NTP time matches RTC time, skipping update");
  }
}

void network_loop( void * pvParameters ) {
  esp_task_wdt_add(NULL);
  Serial.println((String)"Network loop running on CPU core: " + xPortGetCoreID());
  Serial.println("Initializing GPS Module...\n\r");
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // Initialize GPS UART
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
    if (millis() - l1_time > T1H) {
      l1_time = millis();
      updateTime();
    }
    if (millis() - l2_time > T3S) {
    rtctemp = Rtc.GetTemperature();
      if (DEBUG)
        Serial.println((String) "GPS Chars: " + GPS.charsProcessed() + "|With-Fix: " + GPS.sentencesWithFix() + "|Failed-checksum: " + GPS.failedChecksum() + "|Passed-checksum: " + GPS.passedChecksum() + "|RTC-Temp: " + rtctemp.AsFloatDegF() + "F|Sats:" + sats + "|Lat: " + lat + "|Lon: " + lon);
      l2_time = millis();
    }
    delay(500);
  }
}

void setup () {
  Serial.begin(115200);
  if (DEBUG) {
    Serial.println("DEBUG ENABLED - Executing startup delay...");
    delay(10000);
    Serial.println("Starting after debug delay...");
  }
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
  Serial.println("Initializing Hardware Watchdog...\n\r");
  esp_task_wdt_init(WDT_TIMEOUT, true);                //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                              //add current thread to WDT watch
  Serial.println("Initializing the RTC module...");
  Serial.print("compiled: ");
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
      Serial.print("RTC communications error = ");
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
  rtctemp = Rtc.GetTemperature();
  Serial.print(rtctemp.AsFloatDegF());
  Serial.println("F");

  Serial.println((String) "Display loop running on CPU core: " + xPortGetCoreID());
  xTaskCreatePinnedToCore(network_loop, "NETLoop", 10000, NULL, 1, &NETLoop, 0);
  Serial.println("Initializing the display...");
  FastLED.addLeds<CHIPSET, HSPI_MOSI, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
  FastLED.setBrightness( BRIGHTNESS );
  FastLED.clear();  //clear the display
  FastLED.show();  
} 

void loop () {
  esp_task_wdt_reset();
  RtcDateTime currTime = Rtc.GetDateTime();
  int myhour = currTime.Hour();
  if (format==12) {  // if we are using a 12 hour format
    myhour = myhour%12; 
    if (myhour==0)myhour=12;
  }
  setNum(myhour/10,0);  // set first digit of hour
  setNum(myhour%10,1); // set second digit of hour
  int myminute = currTime.Minute();
  setNum(myminute/10,2); // set first digig of minute
  setNum(myminute%10,3);  // set second digit of minute
  int mysecs= currTime.Second();
  setColon(1); // turn colon on
  setStatus();
  refreshDisplay(myHue(mysecs));
  delay(1000); // wait 1 sec
  setColon(0); // turn colon off
  setStatus();
  refreshDisplay(myHue(mysecs));
  delay(50);
  while (mysecs == currTime.Second())
  { // now wait until seconds change
    delay(10); 
    currTime = Rtc.GetDateTime();
  }
  memset(myArray, 0, sizeof myArray);  // clear the display buffer
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

void setStatus() {
  myArray[7][31] = 1;
}

// put numbers onto the display using our num[][] font in positions 0-3. 
void setNum(byte number, byte pos) {
  if (pos==0) pos=1;   // change pos to the real location in the array
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
  myArray[2][15]=onOff;
  myArray[5][15]=onOff;
}

byte myHue(int mySeconds) {     // make display color change from green
  return 160;
  // if (mySeconds<2) return 85;   // through blue to red as seconds advance
  // else if (mySeconds > 57)return 0;  // by returning the correct hue
  // else return 85 + mySeconds*3;
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
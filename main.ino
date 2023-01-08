#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <Wire.h> 
#include <RtcDS3231.h>
#include <esp_task_wdt.h>
#include <Preferences.h>
#include "FastLED.h"

#define NUM_LEDS 256  // # of LEDs in the display
#define DATA_PIN 21 // pin controlling the display

// Your wifi credentials 
String ssid;
String password;
// Other User Settings
const int   GMToffset = -5;  // - 7 = US-MST;  set for your time zone)
const int   format = 12; // time format can be 12 or 24 hour
const char* ntpServer = "172.25.150.1";  // NTP server

TaskHandle_t NTPLoop; 

Preferences preferences;
RtcDS3231<TwoWire> Rtc(Wire); // instance of RTC
WiFiUDP ntpUDP;  // instance of UDP
NTPClient timeClient(ntpUDP, ntpServer, GMToffset*3600);
CRGB leds[NUM_LEDS];
bool colon=false;
byte myArray[8][32];  // display buffer

byte num [10][8] = {  // our font for numbers 0-9
  { 0x38,  /* 001110   number 0 */
    0x44,  /* 010001 */
    0x4C,  /* 010011 */
    0x54,  /* 010101 */
    0x64,  /* 011001 */
    0x44,  /* 010001 */
    0x38,  /* 001110 */
    0x00},   /* 000000 */      
  { 0x10,  /* 000100   number 1 */
    0x30,  /* 001100 */
    0x10,  /* 000100 */
    0x10,  /* 000100 */
    0x10,  /* 000100 */
    0x10,  /* 000100 */
    0x38,  /* 001110 */
    0x00},   /* 000000 */
  { 0x38,  /* 001110   number 2  */
    0x44,  /* 010001 */
    0x04,  /* 000001 */
    0x18,  /* 000110 */
    0x20,  /* 001000 */
    0x40,  /* 010000 */
    0x7C,  /* 011111 */
    0x00},  /* 000000 */
  { 0x38,  /* 001110   number 3  */
    0x44,  /* 010001 */
    0x04,  /* 000001 */
    0x38,  /* 001110 */
    0x04,  /* 000001 */
    0x44,  /* 010001 */
    0x38,  /* 001110 */
    0x00},  /* 000000 */
  { 0x08,  /* 000010   number 4  */
    0x18,  /* 000110 */
    0x28,  /* 001010 */
    0x48,  /* 010010 */
    0x7C,  /* 011111 */
    0x08,  /* 000010 */
    0x08,  /* 000010 */
    0x00},  /* 000000 */
  { 0x7C,  /* 011111   number 5  */
    0x40,  /* 010000 */
    0x40,  /* 010000 */
    0x78,  /* 011110 */
    0x04,  /* 000001 */
    0x44,  /* 010001 */
    0x38,  /* 001110 */
    0x00},  /* 000000 */
  { 0x18,  /* 000110   number 6  */
    0x20,  /* 001000 */
    0x40,  /* 010000 */
    0x78,  /* 011110 */
    0x44,  /* 010001 */
    0x44,  /* 010001 */
    0x38,  /* 001110 */
    0x00},  /* 000000 */
  { 0x7C,  /* 011111   number 7  */
    0x04,  /* 000001 */
    0x08,  /* 000010 */
    0x10,  /* 000100 */
    0x20,  /* 001000 */
    0x20,  /* 001000 */
    0x20,  /* 001000 */
    0x00},  /* 000000 */
  { 0x38,  /* 001110   number 8  */
    0x44,  /* 010001 */
    0x44,  /* 010001 */
    0x38,  /* 001110 */
    0x44,  /* 010001 */
    0x44,  /* 010001 */
    0x38,  /* 001110 */
    0x00},  /* 000000 */
  { 0x38,  /* 001110   number 9  */
    0x44,  /* 010001 */
    0x44,  /* 010001 */
    0x3C,  /* 001111 */
    0x04,  /* 000001 */
    0x08,  /* 000010 */
    0x30,  /* 001100 */
    0x00}  /* 000000 */
};

void time_update_loop( void * pvParameters ) {
  Serial.println((String)"NTP update running on CPU core: " + xPortGetCoreID());
  while(true){
    sleep(3600);
    updateTime();
  }
}

void updateTime() {
  Serial.println("Updating NTP time to RTC...");
  timeClient.update();
  // NTP/Epoch time starts in 1970.  RTC library starts in 2000. 
  // So subtract the 30 extra yrs (946684800UL).
  unsigned long NTPtime = timeClient.getEpochTime()-946684800UL;
  Rtc.SetDateTime(NTPtime); // Set RTC to NTP time
}

void setup () {
  // Connect to WiFi and get NTP time
  Serial.begin(115200);
  Serial.println("Initializing Hardware Watchdog...\n\r");
  esp_task_wdt_init(30, true);                //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                              //add current thread to WDT watch
  preferences.begin("credentials", false); 
  ssid = preferences.getString("ssid", ""); 
  password = preferences.getString("password", "");
  if (ssid == "" || password == ""){
    Serial.println("No values saved for ssid or password");
  }
  else {
    Serial.print("Connecting to wifi...");
    WiFi.begin(ssid.c_str(), password.c_str());
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.print("CONNECTED IP: ");
    Serial.println(WiFi.localIP());  
  }
  esp_task_wdt_reset();
  Serial.println("Starting NTP time client...");
  timeClient.begin();  // Start NTP Time Client
  delay(2000);
  Serial.println("Initializing the RTC module...");
  Rtc.Begin();
  updateTime();
  Serial.println((String)"Main loop running on CPU core: " + xPortGetCoreID());
  xTaskCreatePinnedToCore(time_update_loop, "NTPLoop", 10000, NULL, 1, &NTPLoop, 0);
  // setup display
  FastLED.setBrightness(15); // even 5 is pretty bright!
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);  // configure for WS2812B progrmmable LEDs
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
  refreshDisplay(myHue(mysecs));
  delay(500); // wait 1/2 sec
  setColon(0); // turn colon off
  refreshDisplay(myHue(mysecs));
  while (mysecs == currTime.Second()){ // now wait until seconds change
    delay(10); 
    currTime = Rtc.GetDateTime();
  }
  memset(myArray, 0, sizeof myArray);  // clear the display buffer
}

// Sets the LED display to match content of display buffer at a specified color (myHue).
// Takes into account the zig-zap order of the array up and down from right to left
void refreshDisplay(byte myHue) {
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
  if (pos==0) pos=3;   // change pos to the real location in the array
  else if (pos==1) pos=9;
  else if (pos==2) pos=18;
  else if (pos==3) pos=24;
  for (int x = 0; x < 6; x++) {  // now place the digit's 6x8 font in the display buffer
    for (int y = 0; y < 8; y++) {
     if (num[number][y] & (0b01000000 >> x)) myArray[y][pos+ x] = 1;
    }
  }
}

void setColon(int onOff){  // turn the colon on (1) or off (0)
  myArray[2][16]=onOff;
  myArray[1][16]=onOff;
  myArray[5][16]=onOff;
  myArray[4][16]=onOff;
  myArray[2][15]=onOff;
  myArray[1][15]=onOff;
  myArray[5][15]=onOff;
  myArray[4][15]=onOff;
}

byte myHue(int mySeconds) {     // make display color change from green 
  if (mySeconds<2) return 85;   // through blue to red as seconds advance
  else if (mySeconds > 57)return 0;  // by returning the correct hue
  else return 85 + mySeconds*3; 
}
// LedSmartClock for a 8x32 LED neopixel display
//
// Created by: Ian Perry (ianperry99@gmail.com)
// https://github.com/cosmicc/led_clock         // firmware version

// DO NOT USE DELAYS OR SLEEPS EVER! This breaks systemclock (Everything is coroutines now)

#include "main.h"

// System Loop
void loop() 
{
  esp_task_wdt_reset();
  #ifdef COROUTINE_PROFILER
  CoroutineScheduler::loopWithProfiler();
  #else
  CoroutineScheduler::loop();
  #endif
}

// System Setup
void setup () 
{
  Serial.begin(115200);
  uint32_t timer = millis();
  ESP_EARLY_LOGD(TAG, "Initializing Hardware Watchdog...");
  esp_task_wdt_init(WDT_TIMEOUT, true);                //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                              //add current thread to WDT watch
  ESP_EARLY_LOGD(TAG, "Initializing the system clock...");
  Wire.begin();
  wireInterface.begin();
  systemClock.setup();
  dsClock.setup();
  gpsClock.setup();
  printSystemZonedTime();
  ESP_EARLY_LOGD(TAG, "Initializing IotWebConf...");
  group1.addItem(&imperial);
  group1.addItem(&brightness_level);
  group1.addItem(&text_scroll_speed);
  group1.addItem(&systemcolor);
  group1.addItem(&show_date);
  group1.addItem(&datecolor);
  group1.addItem(&show_date_interval);
  group1.addItem(&enable_status);
  group1.addItem(&enable_alertflash);
  group2.addItem(&twelve_clock);
  group2.addItem(&use_fixed_tz);
  group2.addItem(&fixed_offset);
  group2.addItem(&colonflicker);
  group2.addItem(&flickerfast);
  group2.addItem(&use_fixed_clockcolor);
  group2.addItem(&fixed_clockcolor);
  group3.addItem(&use_fixed_tempcolor);
  group3.addItem(&fixed_tempcolor);
  group3.addItem(&show_weather);
  group3.addItem(&weather_color);
  group3.addItem(&show_weather_interval);
  group3.addItem(&show_weather_daily);
  group3.addItem(&weather_daily_color);
  group3.addItem(&show_weather_daily_interval);
  group3.addItem(&show_airquality);
  group3.addItem(&use_fixed_aqicolor);
  group3.addItem(&airquality_color);
  group3.addItem(&airquality_interval);  
  group3.addItem(&show_alert_interval);
  group3.addItem(&weatherapi);
  group4.addItem(&use_fixed_loc);
  group4.addItem(&fixedLat);
  group4.addItem(&fixedLon);
  group4.addItem(&ipgeoapi);
  iotWebConf.addSystemParameter(&serialdebug);
  iotWebConf.addSystemParameter(&resetdefaults);
  iotWebConf.addHiddenParameter(&savedtzoffset);
  iotWebConf.addHiddenParameter(&savedlat);
  iotWebConf.addHiddenParameter(&savedlon);
  iotWebConf.addHiddenParameter(&savedcity);
  iotWebConf.addHiddenParameter(&savedstate);
  iotWebConf.addHiddenParameter(&savedcountry);
  iotWebConf.addParameterGroup(&group1);
  iotWebConf.addParameterGroup(&group2);
  iotWebConf.addParameterGroup(&group3);
  iotWebConf.addParameterGroup(&group4);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.getApTimeoutParameter()->visible = true;
  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.setApConnectionHandler(&connectAp);
  iotWebConf.setWifiConnectionHandler(&connectWifi);
  iotWebConf.setupUpdateServer(
    [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
    [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });
  iotWebConf.init();
  if (serialdebug.isChecked()) {
    
  }
  ESP_EARLY_LOGD(TAG, "Initializing Light Sensor...");
  userbrightness = calcbright(brightness_level.value());
  current.brightness = userbrightness;
  Wire.begin(TSL2561_SDA, TSL2561_SCL);
  Tsl.begin();
  while (!Tsl.available()) {
      systemClock.loop();
       ESP_EARLY_LOGD(TAG, "Waiting for Light Sensor...");
  }
  Tsl.on();
  Tsl.setSensitivity(true, Tsl2561::EXP_14);
  ESP_EARLY_LOGD(TAG, "Initializing GPS Module...");
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);  // Initialize GPS UART
  ESP_EARLY_LOGD(TAG, "Initializing coroutine scheduler...");
  sysClock.setName("sysclock");
  coroutineManager.setName("manager");
  showClock.setName("show_clock");
  showDate.setName("show_date");
  systemMessages.setName("systemMessages");
  setBrightness.setName("brightness");
  gps_checkData.setName("gpsdata");
  serialInput.setName("serialInput");
  IotWebConf.setName("iotwebconf");
  scrollText.setName("scrolltext");
  AlertFlash.setName("alertflash");
  showWeather.setName("show_weather");
  showAlerts.setName("show_alerts");
  checkWeather.setName("check_weather");
  showAirquality.setName("show_air");
  showWeatherDaily.setName("show_weatherday");
  checkAirquality.setName("check_air");
  checkGeocode.setName("check_geocode");
  #ifndef DISABLE_ALERTCHECK
  checkAlerts.setName("check_alerts");
  #endif
  #ifndef DISABLE_IPGEOCHECK
  checkIpgeo.setName("check_ipgeo");
  #endif
  #ifdef COROUTINE_PROFILER 
  LogBinProfiler::createProfilers();
  #endif
  ipgeo.tzoffset = 127;
  processTimezone();
  server.on("/", handleRoot);
  server.on("/config", []{ iotWebConf.handleConfig(); });
  server.onNotFound([](){ iotWebConf.handleNotFound(); });
  cotimer.scrollspeed = map(text_scroll_speed.value(), 1, 10, 100, 10);
  ESP_EARLY_LOGD(TAG, "IotWebConf initilization is complete. Web server is ready.");
  ESP_EARLY_LOGD(TAG, "Setting class timestamps...");
  bootTime = systemClock.getNow();
  lastntpcheck= systemClock.getNow() - 3601;
  cotimer.icontimer = millis(); // reset all delay timers
  gps.timestamp = bootTime - T1Y + 60;
  gps.lastcheck = bootTime - T1Y + 60;
  gps.lockage = bootTime - T1Y + 60;
  checkweather.lastsuccess = bootTime;
  checkalerts.lastsuccess = bootTime - (60*3);
  checkipgeo.lastsuccess = bootTime - T1Y + 60;
  checkaqi.lastsuccess = bootTime;
  checkgeocode.lastsuccess = bootTime - T1Y + 60;
  checkalerts.lastattempt = bootTime - T1Y + 60;
  checkweather.lastattempt = bootTime - T1Y + 60;
  checkipgeo.lastattempt = bootTime - T1Y + 60;
  checkaqi.lastattempt = bootTime - T1Y + 60;
  checkgeocode.lastattempt = bootTime - T1Y + 60;
  lastshown.alerts = bootTime - T1Y + 60;
  lastshown.dayweather = bootTime;
  lastshown.date = bootTime;
  lastshown.currentweather = bootTime;
  lastshown.aqi = bootTime;
  gps.lat = "0";
  gps.lon = "0";
  scrolltext.position = mw;
  if (use_fixed_loc.isChecked())
    ESP_EARLY_LOGI(TAG, "Setting Fixed GPS Location Lat: %s Lon: %s", fixedLat.value(), fixedLon.value());
  updateCoords();
  updateLocation();
  showclock.fstop = 250;
  if (flickerfast.isChecked())
    showclock.fstop = 20;
  ESP_EARLY_LOGD(TAG, "Initializing the display...");
  FastLED.addLeds<NEOPIXEL, HSPI_MOSI>(leds, NUMMATRIX).setCorrection(TypicalLEDStrip);
  matrix->begin();
  matrix->setBrightness(userbrightness);
  matrix->setTextWrap(false);
  ESP_EARLY_LOGD(TAG, "Display initalization complete.");
  ESP_EARLY_LOGD(TAG, "Setup initialization complete: %d ms", (millis()-timer));
  scrolltext.resetmsgtime = millis() - 60000;
  cotimer.iotloop = millis();
  displaytoken.resetAllTokens();
  CoroutineScheduler::setup();
}

//callbacks 
void configSaved()
{
  updateCoords();
  processTimezone();
  buildURLs();
  showclock.fstop = 250;
  if (flickerfast.isChecked())
    showclock.fstop = 20;
  cotimer.scrollspeed = map(text_scroll_speed.value(), 1, 10, 100, 10);
  userbrightness = calcbright(brightness_level.value());
  ESP_LOGI(TAG, "Configuration was updated.");
  if (resetdefaults.isChecked())
    showready.reset = true;
  if (!serialdebug.isChecked())
    Serial.println("Serial debug info has been disabled.");
  firsttimefailsafe = false;
}

void wifiConnected() {
  ESP_LOGI(TAG, "WiFi was connected.");
  gpsClock.ntpReady();
  display_showStatus();
  matrix->show();
  systemClock.forceSync();
  showready.ip = true;
}

bool connectAp(const char* apName, const char* password)
{
  return WiFi.softAP(apName, password, 4);
}
void connectWifi(const char* ssid, const char* password)
{
  WiFi.begin(ssid, password);
}

// Regular Functions
bool isNextShowReady(acetime_t lastshown, uint8_t interval, uint32_t multiplier)
{
  if (abs(systemClock.getNow() - lastshown) > (interval * multiplier))
    return true;
  else
    return false;
}

bool isNextAttemptReady(acetime_t lastattempt) {
  if (abs(systemClock.getNow() - lastattempt) > T1M)
    return true;
  else
    return false;
}

bool isHttpReady() {
  if (!httpbusy && iotWebConf.getState() == 4 && displaytoken.isReady(0))
    return true;
  else
    return false;
}

bool httpRequest(uint8_t index)
{
    ESP_LOGD(TAG, "Sending request [%d]: %s", index, urls[index]);
    request.begin(urls[index]);
    return true;
}


bool isCoordsValid() {
  if ((current.lat).length() > 1 && (current.lon).length() > 1) 
  {
    return true;
  }
  else
  {
    return false;
  }
}

bool isLocationValid(String source) {
  bool result = false;
  if (source == "geocode")
  {
    if (geocode.city[0] != '\0')
      result = true;
  }
  else if (source == "current")
  {
    if (current.city[0] != '\0')
      result = true;
  }
  else if (source == "saved")
  {
    if (savedcity.value()[0] != '\0')
      result = true;
  }
  return result;
}

bool isApiValid(char *apikey) {
  if (apikey[0] != '\0' && strlen(apikey) == 32)
  {
    return true;
  }
  else
  {
    return false;
  }
}

void print_debugData() {
    debug_print("----------------------------------------------------------------", true);
    printSystemZonedTime();
    acetime_t now = systemClock.getNow();
    String gage = elapsedTime(now - gps.timestamp);
    String loca = elapsedTime(now - gps.lockage);
    String uptime = elapsedTime(now - bootTime);
    String iga = elapsedTime(now - checkipgeo.lastattempt);
    String igs = elapsedTime(now - checkipgeo.lastsuccess);
    String wls = elapsedTime(now - checkweather.lastsuccess);
    String wla = elapsedTime(now - checkweather.lastattempt);
    String cwns = elapsedTime((now - lastshown.currentweather) - (show_weather_interval.value()*60));
    String dwns = elapsedTime((now - lastshown.dayweather) - (show_weather_daily_interval.value()*60*60));
    String alla = elapsedTime(now - checkalerts.lastattempt);
    String alls = elapsedTime(now - checkalerts.lastsuccess);
    String alns = elapsedTime(now - lastshown.alerts);
    String aqla = elapsedTime(now - checkaqi.lastattempt);
    String aqls = elapsedTime(now - checkaqi.lastsuccess);
    String aqns = elapsedTime((now - lastshown.aqi) - (airquality_interval.value()*60));
    String dtns = elapsedTime((now - lastshown.date) - (show_date_interval.value()*60*60));
    String lst = elapsedTime(now - systemClock.getLastSyncTime());
    String npt = elapsedTime((now - lastntpcheck) - NTPCHECKTIME * 60);
    String lip = (WiFi.localIP()).toString();
    String tempunit;
    String speedunit;
    if (imperial.isChecked()) {
      tempunit = imperial_units[0];
      speedunit = imperial_units[1];
    } else {
      tempunit = metric_units[0];
      speedunit = metric_units[1];
    }
    debug_print((String) "Version - Firmware:v" + VERSION_MAJOR + "." + VERSION_MINOR + "." + VERSION_PATCH + " | Config:v" + VERSION_CONFIG, true);
    debug_print((String) "System - RawLux:" + current.rawlux + " | Lux:" + current.lux + " | UsrBright:+" + userbrightness + " | Brightness:" + current.brightness + " | ClockHue:" + current.clockhue + " | temphue:" + current.temphue + " | WifiState:" + connection_state[iotWebConf.getState()] + " | HttpReady:" + yesno[isHttpReady()] + " | IP:" + lip + " | Uptime:" + uptime, true);
    debug_print((String) "Clock - Status:" + clock_status[systemClock.getSyncStatusCode()] + " | TimeSource:" + timesource + " | CurrentTZ:" + current.tzoffset +  " | NtpReady:" + gpsClock.ntpIsReady + " | LastAttempt:" + elapsedTime(systemClock.getSecondsSinceSyncAttempt()) + " | NextAttempt:" + elapsedTime(systemClock.getSecondsToSyncAttempt()) + " | Skew:" + systemClock.getClockSkew() + " Seconds | NextNtp:" + npt + " | LastSync:" + lst, true);
    debug_print((String) "Loc - SavedLat:" + savedlat.value() + " | SavedLon:" + savedlon.value() + " | CurrentLat:" + current.lat + " | CurrentLon:" + current.lon + " | LocValid:" + yesno[isCoordsValid()], true);
    debug_print((String) "IPGeo - Complete:" + yesno[checkipgeo.complete] + " | Lat:" + ipgeo.lat + " | Lon:" + ipgeo.lon + " | TZoffset:" + ipgeo.tzoffset + " | Timezone:" + ipgeo.timezone + " | ValidApi:" + yesno[isApiValid(ipgeoapi.value())] + " | LastAttempt:" + iga + " | LastSuccess:" + igs, true);
    debug_print((String) "GPS - Chars:" + GPS.charsProcessed() + " | With-Fix:" + GPS.sentencesWithFix() + " | Failed:" + GPS.failedChecksum() + " | Passed:" + GPS.passedChecksum() + " | Sats:" + gps.sats + " | Hdop:" + gps.hdop + " | Elev:" + gps.elevation + " | Lat:" + gps.lat + " | Lon:" + gps.lon + " | FixAge:" + gage + " | LocAge:" + loca, true);
    debug_print((String) "Weather Current - Icon:" + weather.currentIcon + " | Temp:" + weather.currentTemp + tempunit + " | FeelsLike:" + weather.currentFeelsLike + tempunit + " | Humidity:" + weather.currentHumidity + "% | Wind:" + weather.currentWindSpeed + "/" + weather.currentWindGust + speedunit + " | Desc:" + weather.currentDescription + " | ValidApi:" + yesno[isApiValid(weatherapi.value())] + " | LastAttempt:" + wla + " | LastSuccess:" + wls + " | NextShow:" + cwns, true);
    debug_print((String) "Weather Day - Icon:" + weather.dayIcon + " | LoTemp:" + weather.dayTempMin + tempunit + " | HiTemp:" + weather.dayTempMax + tempunit + " | Humidity:" + weather.dayHumidity + "% | Wind:" + weather.dayWindSpeed + "/" + weather.dayWindGust + speedunit + " | Desc:" + weather.currentDescription + " | NextShow:" + dwns, true);
    debug_print((String) "Air Quality - Aqi:" + air_quality[weather.currentaqi] + " | Co:" + weather.carbon_monoxide + " | No:" + weather.nitrogen_monoxide + " | No2:" + weather.nitrogen_dioxide + " | Ozone:" + weather.ozone + " | So2:" + weather.sulfer_dioxide + " | Pm2.5:" + weather.particulates_small + " | Pm10:" + weather.particulates_medium + " | Ammonia:" + weather.ammonia + " | LastAttempt:" + aqla + " | LastSuccess:" + aqls + " | NextShow:" + aqns, true);
    debug_print((String) "Alerts - Active:" + yesno[alerts.active] + " | Watch:" + yesno[alerts.inWatch] + " | Warn:" + yesno[alerts.inWarning] + " | LastAttempt:" + alla + " | LastSuccess:" + alls + " | LastShown:" + alns, true);
    debug_print((String) "Location: ", false);
    if (isLocationValid("current"))
      debug_print((String) current.city + ", " + current.state + ", " + current.country, true);
    else
      debug_print("Currently not known", true);
    if (alerts.active)
      debug_print((String) "*Alert1 - Status:" + alerts.status1 + " | Severity:" + alerts.severity1 + " | Certainty:" + alerts.certainty1 + " | Urgency:" + alerts.urgency1 + " | Event:" + alerts.event1 + " | Desc:" + alerts.description1, true);
    debug_print((String)"Display Tokens: " + displaytoken.showTokens(), true);
}

void debug_print(String message, bool cr = false)
{
  if (serialdebug.isChecked())
    if (cr)
      Serial.println(message);
    else
      Serial.print(message);
}

void processTimezone()
{
  uint32_t timer = millis();
  int16_t savedoffset;
  savedoffset = savedtzoffset.value();
  if (use_fixed_tz.isChecked())
  {
    current.tzoffset = fixed_offset.value();
    current.timezone = TimeZone::forHours(fixed_offset.value());
    ESP_LOGD(TAG, "Using fixed timezone offset: %d", fixed_offset.value());
  }
  else if (ipgeo.tzoffset != 127) {
    current.tzoffset = ipgeo.tzoffset;
    current.timezone = TimeZone::forHours(ipgeo.tzoffset);
    ESP_LOGD(TAG, "Using ipgeolocation offset: %d", ipgeo.tzoffset);
    if (ipgeo.tzoffset != savedoffset)
    {
      ESP_LOGI(TAG, "IP Geo timezone [%d] (%s) is different then saved timezone [%d], saving new timezone", ipgeo.tzoffset, ipgeo.timezone, savedoffset);
      savedtzoffset.value() = ipgeo.tzoffset;
      iotWebConf.saveConfig();
    }
  }
  else {
    current.tzoffset = savedoffset;
    current.timezone = TimeZone::forHours(savedoffset);
    ESP_LOGD(TAG, "Using saved timezone offset: %d", savedoffset);
  }
  ESP_LOGV(TAG, "Process timezone complete: %d ms", (millis()-timer));
}

bool cmpLocs(char a1[32], char a2[32])
{
  if (memcmp((const void *)a1, (const void *)a2, sizeof(a1)) == 0)
    return true;
  else
    return false;
}

void updateLocation()
{
  if (isLocationValid("geocode"))
  {
    if (!cmpLocs(geocode.city, current.city))
      showready.loc = true;
    memcpy(current.city, geocode.city, 32);
    memcpy(current.state, geocode.state, 32);
    memcpy(current.country, geocode.country, 32);
    ESP_LOGD(TAG, "Using Geocode location: %s, %s, %s", current.city, current.state, current.country);
  }
  else if (isLocationValid("saved"))
  {
    memcpy(current.city, savedcity.value(), 32);
    memcpy(current.state, savedstate.value(), 32);
    memcpy(current.country, savedcountry.value(), 32);
    ESP_LOGD(TAG, "Using saved location: %s, %s, %s", current.city, current.state, current.country);
  }
  else if (isLocationValid("current"))
  {
    ESP_LOGD(TAG, "No new location update, using current location: %s, %s, %s", current.city, current.state, current.country);
  }
  else
  {
    ESP_LOGD(TAG, "No valid locations found");
  }
  if (!cmpLocs(savedcity.value(), current.city))
  {
    ESP_LOGI(TAG, "Location not saved, saving new location: %s, %s, %s", current.city, current.state, current.country);
    memcpy(savedcity.value(), current.city, 32);
    memcpy(savedstate.value(), current.state, 32);
    memcpy(savedcountry.value(), current.country, 32);
    iotWebConf.saveConfig();
  }
}

void updateCoords()
{
  if (use_fixed_loc.isChecked())
  {
    gps.lat = fixedLat.value();
    gps.lon = fixedLon.value();
    current.lat = fixedLat.value();
    current.lon = fixedLon.value();
    current.locsource = "User Defined";
  }
  else if (gps.lon == "0" && ipgeo.lon[0] == '\0' && current.lon == "0")
  {
    current.lat = savedlat.value();
    current.lon = savedlon.value();
    current.locsource = "Previous Saved";
  }
  else if (gps.lon == "0" && ipgeo.lon[0] != '\0' && current.lon == "0")
  {
    current.lat = (String)ipgeo.lat; 
    current.lon = (String)ipgeo.lon;
    current.locsource = "IP Geolocation";
    ESP_LOGI(TAG, "Using IPGeo location information: Lat: %s Lon: %s", (ipgeo.lat), (ipgeo.lon));
  }
  else if (gps.lon != "0")
  {
    current.lat = gps.lat;
    current.lon = gps.lon;
    current.locsource = "GPS";
  }
  double nlat = (current.lat).toDouble();
  double nlon = (current.lon).toDouble();
  String sa = savedlat.value();
  String so = savedlon.value();
  double olat = sa.toDouble();
  double olon = so.toDouble();
  if (abs(nlat - olat) > 0.02 || abs(nlon - olon) > 0.02) {
    ESP_LOGW(TAG, "Major location shift, saving values");
    ESP_LOGW(TAG, "Lat:[%0.6lf]->[%0.6lf] Lon:[%0.6lf]->[%0.6lf]", olat, nlat, olon, nlon);
    char sla[12];
    char slo[12];
    (current.lat).toCharArray(sla, 12);
    (current.lon).toCharArray(slo, 12);
    memcpy(savedlat.value(), sla, sizeof(sla));
    memcpy(savedlon.value(), slo, sizeof(slo));
    iotWebConf.saveConfig();
    checkgeocode.ready = true;
  }
  buildURLs();
}

void display_setclockDigit(uint8_t bmp_num, uint8_t position, uint16_t color) 
{ 
    if (position == 0) position = 0;
    else if (position == 1) position = 7;
    else if (position == 2) position = 16;
    else if (position == 3) position = 23;
    if (clock_display_offset) 
      matrix->drawBitmap(position-4, 0, num[bmp_num], 8, 8, color);    
    else
      matrix->drawBitmap(position, 0, num[bmp_num], 8, 8, color); 
}

void display_showStatus() 
{
    uint16_t sclr;
    uint16_t wclr;
    uint16_t aclr;
    bool ds = !enable_status.isChecked();
    if (ds)
      sclr = BLACK;
    acetime_t now = systemClock.getNow();
    if (now - systemClock.getLastSyncTime() > (NTPCHECKTIME*60) + 60)
        sclr = DARKRED;
    else if (timesource == "gps" && gps.fix && (now - systemClock.getLastSyncTime()) <= 600)
        sclr = BLACK;
    else if (timesource == "gps" && !gps.fix && (now - systemClock.getLastSyncTime()) <= 600)
        sclr = DARKBLUE;
    else if (timesource == "gps" && !gps.fix && (now - systemClock.getLastSyncTime()) > 600)
        sclr = DARKPURPLE;
    else if (gps.fix && timesource == "ntp")
        sclr = DARKPURPLE;
    else if (timesource == "ntp" && iotWebConf.getState() == 4 && !ds)
      sclr = DARKGREEN;
    else if (iotWebConf.getState() == 3)  // connecting
      sclr = DARKYELLOW; 
    else if (iotWebConf.getState() <= 2)  // boot, apmode, notconfigured
      sclr = DARKMAGENTA;
    else if (iotWebConf.getState() == 4)  // online
      sclr = DARKCYAN;
    else if (iotWebConf.getState() == 5) // offline
      sclr = DARKRED;
    else if (!ds)
      sclr = DARKRED;
    if (weather.currentaqi == 2)
      aclr = DARKYELLOW;
    else if (weather.currentaqi == 3)
      aclr = DARKORANGE;
    else if (weather.currentaqi == 4)
      aclr = DARKRED;
    else if (weather.currentaqi == 5)
      aclr = DARKPURPLE;
    else 
      aclr = BLACK;
    if (alerts.inWarning)
      wclr = RED;
    else if (alerts.inWatch)
      wclr = YELLOW;
    else
      wclr = BLACK;
    if (enable_status.isChecked())
      matrix->drawPixel(0, 7, sclr);
    if (show_airquality.isChecked())
      matrix->drawPixel(0, 0, aclr);
    if (!scrolltext.active) {
      matrix->drawPixel(0, 3, wclr);
      matrix->drawPixel(0, 4, wclr);
      matrix->drawPixel(mw-1, 3, wclr);
      matrix->drawPixel(mw-1, 4, wclr);
    }
    if (current.oldaqiclr != aclr || current.oldstatusclr != sclr || current.oldstatuswclr != wclr) {
      matrix->show();
      current.oldstatusclr = sclr;
      current.oldstatuswclr = wclr;
      current.oldaqiclr = aclr;
    }
}

void display_weatherIcon(char* icon) 
{
    bool night;
    char dn;
    char code1;
    char code2;
    dn = icon[2];
    if ((String)dn == "n")
      night = true;
    else
      night = false;
    code1 = icon[0];
    code2 = icon[1];
    if (code1 == '0' && code2 == '1')
    { // clear
      if (night)
        matrix->drawRGBBitmap(mw-8, 0, clear_night[cotimer.iconcycle], 8, 8);
      else
        matrix->drawRGBBitmap(mw-8, 0, clear_day[cotimer.iconcycle], 8, 8);
    }
    else if ((code1 == '0' && code2 == '2') || (code1 == '0' && code2 == '3')) {  //partly cloudy
      if (night)
        matrix->drawRGBBitmap(mw-8, 0, partly_cloudy_night[cotimer.iconcycle], 8, 8);
      else
        matrix->drawRGBBitmap(mw-8, 0, partly_cloudy_day[cotimer.iconcycle], 8, 8);
    }
    else if ((code1 == '0') && (code2 == '4')) // cloudy
      matrix->drawRGBBitmap(mw-8, 0, cloudy[cotimer.iconcycle], 8, 8);
    else if (code1 == '5' && code2 == '0')  //fog/mist 
      matrix->drawRGBBitmap(mw-8, 0, fog[cotimer.iconcycle], 8, 8);
    else if (code1 == '1' && code2 == '3')  //snow
      matrix->drawRGBBitmap(mw-8, 0, snow[cotimer.iconcycle], 8, 8);
    else if (code1 == '0' && code2 == '9') //rain
      matrix->drawRGBBitmap(mw-8, 0, rain[cotimer.iconcycle], 8, 8);
    else if (code1 == '1' && code2 == '0') // heavy rain
      matrix->drawRGBBitmap(mw-8, 0, heavy_rain[cotimer.iconcycle], 8, 8);
    else if (code1 == '1' && code2 == '1') //thunderstorm
      matrix->drawRGBBitmap(mw-8, 0, thunderstorm[cotimer.iconcycle], 8, 8);
    else
      ESP_LOGE(TAG, "No Weather icon found to use");
    if (millis() - cotimer.icontimer > ANI_SPEED)
    {
    if (cotimer.iconcycle == ANI_BITMAP_CYCLES-1)
      cotimer.iconcycle = 0;
    else
      cotimer.iconcycle++;
    cotimer.icontimer = millis();
    }
}

void display_temperature() 
{
    int temp;
    temp = weather.currentFeelsLike;
    int xpos;
    int digits = (abs(temp == 0)) ? 1 : (log10(abs(temp)) + 1);
    bool isneg = false;
    if (temp + abs(temp) == 0) 
    {
      isneg = true;
      digits++;
    }
    if (digits == 3)
      xpos = 1;
    else if (digits == 2)
      xpos = 5;
    else if (digits == 1)
      xpos = 9;
    xpos = xpos * (digits);
    int score = abs(temp);
    uint32_t tc;
    int16_t tl = 0;
    int16_t th = 40;
    if (imperial.isChecked()) {
      tl = 32;
      th = 104;
    }
    if (!use_fixed_tempcolor.isChecked())
    {
      current.temphue = constrain(map(weather.currentFeelsLike, tl, th, NIGHTHUE, 0), NIGHTHUE, 0);
      if (weather.currentFeelsLike < tl)
        if (imperial.isChecked() && weather.currentFeelsLike + abs(weather.currentFeelsLike) == 0)
          current.temphue = NIGHTHUE + tl;
        else
          current.temphue = NIGHTHUE + (abs(weather.currentFeelsLike)/2);
      tc = hsv2rgb(current.temphue);
    }
    else
      tc = hex2rgb(fixed_tempcolor.value());
    while (score)
    {
      matrix->drawBitmap(xpos, 0, num[score % 10], 8, 8, tc);
      score /= 10;
      xpos = xpos - 7;
    }
    if (isneg == true) 
    {
      matrix->drawBitmap(xpos, 0, sym[1], 8, 8, tc);
      xpos = xpos - 7;
    }
    matrix->drawBitmap(xpos+(digits*7)+7, 0, sym[0], 8, 8, tc);
}

void printSystemZonedTime() 
{
  acetime_t now = systemClock.getNow();
  auto TimeWZ = ZonedDateTime::forEpochSeconds(now, current.timezone);
  TimeWZ.printTo(SERIAL_PORT_MONITOR);
  SERIAL_PORT_MONITOR.println();
}

ace_time::ZonedDateTime getSystemZonedTime() 
{
  acetime_t now = systemClock.getNow();
  ace_time::ZonedDateTime TimeWZ = ZonedDateTime::forEpochSeconds(now, current.timezone);
  return TimeWZ;
}

String getSystemZonedTimeString() 
{
  ace_time::ZonedDateTime now = getSystemZonedTime();
  char *string;
  sprintf(string, "%d/%d/%d %d:%d:%d [%d]", now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second(), now.timeOffset());
  return (String)string;
}

String getSystemZonedDateString() 
{
  ace_time::ZonedDateTime ldt = getSystemZonedTime();
  uint8_t day = ldt.day();
  char buf[100];
  sprintf(buf, "%s, %s %d%s %d", DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()), day, ordinal_suffix(day), ldt.year());
  return (String)buf;
}

String getSystemZonedDateTimeString() 
{
  ace_time::ZonedDateTime ldt = getSystemZonedTime();
  uint8_t day = ldt.day();
  uint8_t hour = ldt.day();
  char buf[100];
  String ap = "am";
  if (twelve_clock.isChecked())
  {
    if (hour > 11)
      ap = "pm";
    if (hour > 12)
      hour = hour - 12;
    sprintf(buf, "%d:%02d%s %s, %s %d%s %04d", ldt.hour(), ldt.minute(), ap, DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()), day, ordinal_suffix(day), ldt.year());
  } 
  else
    sprintf(buf, "%02d:%02d %s, %s %d%s %04d", ldt.hour(), ldt.minute(), DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()), day, ordinal_suffix(day), ldt.year());
  return (String)buf;
}

uint16_t calcaqi(double c, double i_hi, double i_low, double c_hi, double c_low) 
{
  return (i_hi - i_low)/(c_hi - c_low)*(c - c_low) + i_low;
}

void fillAlertsFromJson(Alerts* alerts) 
{
  if (Json["features"].length() != 0)
  {
    sprintf(alerts->status1, "%s", (const char *)Json["features"][0]["properties"]["status"]);
    sprintf(alerts->severity1, "%s", (const char *)Json["features"][0]["properties"]["severity"]);
    sprintf(alerts->certainty1, "%s", (const char *)Json["features"][0]["properties"]["certainty"]);
    sprintf(alerts->urgency1, "%s", (const char *)Json["features"][0]["properties"]["urgency"]);
    sprintf(alerts->event1, "%s", (const char *)Json["features"][0]["properties"]["event"]);
    const char *j = (const char *)Json["features"][0]["properties"]["description"];
    Serial.println((String) "!!!!!Alert Size: " + sizeof(j) + " length: " + ((String)j).length());
    sprintf(alerts->description1, "%s", (char *)cleanString(j));
    if ((String)alerts->certainty1 == "Observed" || (String)alerts->certainty1 == "Likely")
    {
      alerts->inWarning = true;
      alerts->active = true;
      showready.alerts = true;
      alerts->timestamp = systemClock.getNow();
    }
    else if ((String)alerts->certainty1 == "Possible") {
      alerts->inWatch= true;
      alerts->active = true;
      showready.alerts = true;
      alerts->timestamp = systemClock.getNow();
    } 
    else {
      alerts->active = false;
      alerts->inWarning = false;
      alerts->inWatch= false;     
    }
    ESP_LOGI(TAG, "Active weather alert received");
  }
  else {
    alerts->active = false;
    alerts->inWarning = false;
    alerts->inWatch= false;    
    ESP_LOGI(TAG, "No current weather alerts exist");
  }
}

void fillWeatherFromJson(Weather* weather) 
{
  sprintf(weather->currentIcon, "%s", (const char*) Json["current"]["weather"][0]["icon"]);
  weather->currentTemp = Json["current"]["temp"];
  weather->currentFeelsLike = Json["current"]["feels_like"];
  weather->currentHumidity = Json["current"]["humidity"];
  weather->currentWindSpeed = Json["current"]["wind_speed"];
  weather->currentWindSpeed = Json["current"]["wind_gust"];
  sprintf(weather->currentDescription, "%s", (const char*) Json["current"]["weather"][0]["description"]);
  weather->dayTempMin = Json["daily"][0]["feels_like"]["min"];
  weather->dayTempMax = Json["daily"][0]["feels_like"]["max"];
  weather->dayHumidity = Json["daily"][0]["humidity"];
  weather->dayWindSpeed = Json["daily"][0]["wind_speed"];
  weather->dayWindGust = Json["daily"][0]["wind_gust"];
  sprintf(weather->dayDescription, "%s", (const char*) Json["daily"][0]["weather"][0]["description"]);
  sprintf(weather->dayIcon, "%s", (const char*) Json["daily"][0]["weather"][0]["icon"]);
  //weather->dayMoonPhase = weatherJson["daily"][0]["moon_phase"];
  //weather->daySunrise = weatherJson["daily"][0]["sunrise"];
  //weather->daySunset = weatherJson["daily"][0]["sunset"];
}

void fillIpgeoFromJson(Ipgeo* ipgeo) {
  sprintf(ipgeo->timezone, "%s", (const char*) Json["time_zone"]["name"]);
  ipgeo->tzoffset = Json["time_zone"]["offset"];
  sprintf(ipgeo->lat, "%s", (const char*) Json["latitude"]);
  sprintf(ipgeo->lon, "%s", (const char*) Json["longitude"]);
}

void fillGeocodeFromJson(Geocode* geocode) {
  sprintf(geocode->city, "%s", (const char*) Json[0]["name"]);
  sprintf(geocode->state, "%s", (const char*) Json[0]["state"]);
  sprintf(geocode->country, "%s", (const char*) Json[0]["country"]);
}

void fillAqiFromJson(Weather* weather) {
  weather->currentaqi = Json["list"][0]["main"]["aqi"];
  weather->carbon_monoxide = Json["list"][0]["components"]["co"];
  weather->nitrogen_monoxide = Json["list"][0]["components"]["no"];
  weather->nitrogen_dioxide = Json["list"][0]["components"]["no2"];
  weather->ozone = Json["list"][0]["components"]["o3"];
  weather->sulfer_dioxide = Json["list"][0]["components"]["so2"];
  weather->particulates_small = Json["list"][0]["components"]["pm2_5"];
  weather->particulates_medium = Json["list"][0]["components"]["pm10"];
  weather->ammonia = Json["list"][0]["components"]["nh3"];
}

void buildURLs() 
{
  String units;
  if (imperial.isChecked())
    units = "imperial";
  else
    units = "metric";
  String wurl = (String) "https://api.openweathermap.org/data/2.5/onecall?units=" + units + "&exclude=minutely,hourly&appid=" + weatherapi.value() + "&lat=" + current.lat + "&lon=" + current.lon + "&lang=en";  // weather forecast url
  wurl.toCharArray(urls[0], 256);
  String aurl = (String) "https://api.weather.gov/alerts/active?status=actual&point=" + current.lat + "," + current.lon + "&limit=1";                                                                     // Weather alert url
  aurl.toCharArray(urls[1], 256);
  String curl = (String) "https://api.openweathermap.org/geo/1.0/reverse?lat=" + current.lat + "&lon=" + current.lon + "&limit=5&appid=" + weatherapi.value();                                                    // Geocoding url
  curl.toCharArray(urls[2], 256);
  String qurl = (String) "https://api.openweathermap.org/data/2.5/air_pollution?lat=" + current.lat + "&lon=" + current.lon + "&appid=" + weatherapi.value();                                                                                 //air pollution url
  qurl.toCharArray(urls[3], 256);
  String gurl = (String) "https://api.ipgeolocation.io/ipgeo?apiKey=" + ipgeoapi.value();
  gurl.toCharArray(urls[4], 256);
}

String elapsedTime(int32_t seconds) 
{
  String result;
  uint8_t granularity;
  seconds = abs(seconds);
  uint32_t tseconds = seconds;
  if (seconds > T1Y - 60)
    return "never";
  if (seconds < 60)
    return (String)seconds + " seconds";
  else granularity = 2;
  uint32_t value;
  for (uint8_t i = 0; i < 6; i++)
  {
    uint32_t vint = atoi(intervals[i]);
    value = floor(seconds / vint);
    if (value != 0)
    {
      seconds = seconds - value * vint;
      if (granularity != 0) {
        result = result + value + " " + interval_names[i];
        granularity--;
        if (granularity > 0)
          result = result + ", ";
      }
    }
  }
  if (granularity != 0)
    result = result + seconds + " seconds";
  return result;
}

String capString (String str) 
{
  	for(uint16_t i=0; str[i]!='\0'; i++)
	{
		if(i==0)
		{
			if((str[i]>='a' && str[i]<='z'))
				str[i]=str[i]-32;
			continue; 
		}
		if(str[i]==' ')
		{
			++i;
			if(str[i]>='a' && str[i]<='z')
			{
				str[i]=str[i]-32; 
				continue; 
			}
		}
		else
		{
			if(str[i]>='A' && str[i]<='Z')
				str[i]=str[i]+32; 
		}
	}
  return str;
}

acetime_t convertUnixEpochToAceTime(uint32_t ntpSeconds) 
{
    static const int32_t kDaysToConverterEpochFromNtpEpoch = 36524;
    int32_t daysToCurrentEpochFromNtpEpoch = Epoch::daysToCurrentEpochFromConverterEpoch() + kDaysToConverterEpochFromNtpEpoch;
    uint32_t secondsToCurrentEpochFromNtpEpoch = (uint32_t) 86400 * (uint32_t) daysToCurrentEpochFromNtpEpoch;
    uint32_t epochSeconds = ntpSeconds - secondsToCurrentEpochFromNtpEpoch;
    return (int32_t) epochSeconds;
 }

  void setTimeSource(String source) 
  {
    timesource = source;
  }

  acetime_t Now() 
  {
    return systemClock.getNow();
  }

  void resetLastNtpCheck() 
  {
    lastntpcheck = systemClock.getNow();
  }

  uint16_t calcbright(uint16_t bl) 
  {
  if (bl == 1)
    return 0;
  else if (bl == 2)
    return 5;
  else if (bl == 3)
    return 10;
  else if (bl == 4)
    return 20;
  else if (bl == 5)
    return 30;
  else
    return 0;
  }

const char* ordinal_suffix(int n)
{
        static const char suffixes [][3] = {"th", "st", "nd", "rd"};
        auto ord = n % 100;
        if (ord / 10 == 1) { ord = 0; }
        ord = ord % 10;
        if (ord > 3) { ord = 0; }
        return suffixes[ord];
}

char* cleanString(const char* p) 
{
    char* q = (char *)p;
    while (p != 0 && *p != '\0') {
        if (*p == '\n' || *p == '*') {
            p++;
            *q = *p;
        } 
        else {
            *q++ = *p++;
        }
    }
    *q = '\0';
    return q;
}


 //FIXME: string printouts on debug messages for scrolltext, etc showing garbled
 //TODO: web interface cleanup
 //TODO: advanced aqi calulations
 //TODO: table titles
 //TODO: remove tables is show is disabled 
 //TODO: weather daily in web
 //TODO: timezone name in web
 //FIXME: make sure checkaqi is ran before weather display



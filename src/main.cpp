// LedSmartClock for a 8x32 LED neopixel display
//
// Created by: Ian Perry (ianperry99@gmail.com)
// https://github.com/cosmicc/LEDSmartClock     

// DO NOT USE DELAYS OR SLEEPS EVER! This breaks systemclock (Everything is coroutines now)

#include "main.h"

extern "C" void app_main()
{
  Serial.begin(115200);
  uint32_t init_timer = millis();
  nvs_flash_init();
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
  iotWebConf.addSystemParameter(&serialdebug);
  iotWebConf.addSystemParameter(&resetdefaults);
  iotWebConf.addSystemParameter(&ipgeoapi);
  iotWebConf.addSystemParameter(&weatherapi);
  iotWebConf.addHiddenParameter(&savedtzoffset);
  iotWebConf.addHiddenParameter(&savedlat);
  iotWebConf.addHiddenParameter(&savedlon);
  iotWebConf.addHiddenParameter(&savedcity);
  iotWebConf.addHiddenParameter(&savedstate);
  iotWebConf.addHiddenParameter(&savedcountry);
  group1.addItem(&brightness_level);
  group1.addItem(&text_scroll_speed);
  group1.addItem(&system_color);
  group1.addItem(&imperial);
  group1.addItem(&alert_interval);
  group1.addItem(&enable_alertflash);
  group1.addItem(&show_date);
  group1.addItem(&date_color);
  group1.addItem(&date_interval);
  group2.addItem(&twelve_clock);
  group2.addItem(&colonflicker);
  group2.addItem(&flickerfast);
  group2.addItem(&enable_clock_color);
  group2.addItem(&clock_color);
  group2.addItem(&enable_fixed_tz);
  group2.addItem(&fixed_offset);
  group3.addItem(&show_current_weather);
  group3.addItem(&current_weather_color);
  group3.addItem(&current_weather_interval);
  group3.addItem(&enable_temp_color);
  group3.addItem(&temp_color);
  group4.addItem(&show_daily_weather);
  group4.addItem(&daily_weather_color);
  group4.addItem(&daily_weather_interval);
  group5.addItem(&show_aqi);
  group5.addItem(&enable_aqi_color);
  group5.addItem(&aqi_color);
  group5.addItem(&aqi_interval);  
  group6.addItem(&enable_system_status);
  group6.addItem(&enable_aqi_status);
  group6.addItem(&enable_uvi_status);
  group6.addItem(&green_status);
  group7.addItem(&show_sunrise);
  group7.addItem(&sunrise_color);
  group7.addItem(&sunrise_message);
  group7.addItem(&show_sunset);
  group7.addItem(&sunset_color);
  group7.addItem(&sunset_message);
  group8.addItem(&show_loc_change);
  group8.addItem(&enable_fixed_loc);
  group8.addItem(&fixedLat);
  group8.addItem(&fixedLon);
  iotWebConf.addParameterGroup(&group1);
  iotWebConf.addParameterGroup(&group2);
  iotWebConf.addParameterGroup(&group3);
  iotWebConf.addParameterGroup(&group4);
  iotWebConf.addParameterGroup(&group5);
  iotWebConf.addParameterGroup(&group6);
  iotWebConf.addParameterGroup(&group7);
  iotWebConf.getApTimeoutParameter()->visible = true;
  //iotWebConf.setApConnectionHandler(&connectAp);
  iotWebConf.setWifiConnectionHandler(&connectWifi);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);  
  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  iotWebConf.setupUpdateServer(
    [](const char* updatePath) { httpUpdater.setup(&server, updatePath); },
    [](const char* userName, char* password) { httpUpdater.updateCredentials(userName, password); });
  iotWebConf.init();
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
  checkweather.lastsuccess = bootTime - T1Y + 60;
  checkalerts.lastsuccess = bootTime - T1Y + 60;
  checkipgeo.lastsuccess = bootTime - T1Y + 60;
  checkaqi.lastsuccess = bootTime - T1Y + 60;
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
  scrolltext.position = mw;
  if (enable_fixed_loc.isChecked())
    ESP_EARLY_LOGI(TAG, "Setting Fixed GPS Location Lat: %s Lon: %s", fixedLat.value(), fixedLon.value());
  updateCoords();
  updateLocation();
  showclock.fstop = 250;
  if (flickerfast.isChecked())
    showclock.fstop = 20;
  ESP_EARLY_LOGD(TAG, "Display initalization complete.");
  ESP_EARLY_LOGD(TAG, "Setup initialization complete: %d ms", (millis()-init_timer));
  scrolltext.resetmsgtime = millis() - 60000;
  displaytoken.resetAllTokens();
  CoroutineScheduler::setup();
  ESP_EARLY_LOGD(TAG, "Initializing the display...");
  FastLED.addLeds<NEOPIXEL, HSPI_MOSI>(leds, NUMMATRIX).setCorrection(TypicalLEDStrip);
  matrix->begin();
  matrix->setBrightness(userbrightness);
  matrix->setTextWrap(false);
  ESP_EARLY_LOGD(TAG, "Display initalization complete.");
  ESP_EARLY_LOGD(TAG, "Setup initialization complete: %d ms", (millis()-init_timer));
  scrolltext.resetmsgtime = millis() - 60000;
  displaytoken.resetAllTokens();
  CoroutineScheduler::setup();

  for (;;)
  {
    esp_task_wdt_reset();
    #ifdef COROUTINE_PROFILER
    CoroutineScheduler::loopWithProfiler();
    #else
    CoroutineScheduler::loop();
    #endif
  }
}

// callbacks
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
    printf("Serial debug info has been disabled.\n");
  firsttimefailsafe = false;
}

void wifiConnected() {
  ESP_LOGI(TAG, "WiFi is now connected.");
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
bool isNextShowReady(acetime_t lastshown, uint8_t interval, uint32_t multiplier) {
  // Calculate current time in seconds
  uint32_t now = systemClock.getNow();
  // Check if the elapsed time since the last show is greater than the interval multiplied by the multiplier
  return (now - lastshown) > (interval * multiplier);
}

bool isNextAttemptReady(acetime_t lastattempt) {
  // wait 1 minute between attempts
  return (systemClock.getNow() - lastattempt) > T1M;
}

bool isHttpReady() {
  // Returns true if httpbusy is false, IotWebConf is in state 4, and displaytoken.isReady(0)
  return !httpbusy && iotWebConf.getState() == 4 && displaytoken.isReady(0);
}

// Function for sending HTTP request
bool httpRequest(uint8_t index)
{
  ESP_LOGD(TAG, "Sending request [%d]: %s", index, urls[index]); // Log the request
  request.begin(urls[index]); // Begin the request
  return true; // Return true to denote successful request
}

// Function to check if current coordinates are valid
bool isCoordsValid() 
{
  // Returns true if lat and lon are both not 0
  return (current.lat != 0 && current.lon != 0);
}

bool isLocationValid(String source) 
{
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

// checks if apikey is valid by checking its length
bool isApiValid(char *apikey) 
{
  return (apikey[0] != '\0' && strlen(apikey) == 32);
}

void print_debugData() {
    acetime_t now = systemClock.getNow();
    String dtns = elapsedTime((now - lastshown.date) - (date_interval.value()*60*60));
    char tempunit[7];
    char speedunit[7];
    char distanceunit[7];
    if (imperial.isChecked())
    {
      memcpy(tempunit, imperial_units[3], 7);
      memcpy(speedunit, imperial_units[1], 7);
      memcpy(distanceunit, imperial_units[2], 7);
    }
    else
    {
      memcpy(tempunit, metric_units[3], 7);
      memcpy(speedunit, metric_units[1], 7);
      memcpy(distanceunit, metric_units[2], 7);
    }
    printf("[^----------------------------------------------------------------^]\n");
    printf("Current Time: %s\n", getSystemZonedTimestamp().c_str());
    printf("Boot Time: %s\n", getCustomZonedTimestamp(bootTime).c_str());
    printf("Sunrise Time: %s\n", getCustomZonedTimestamp(weather.day.sunrise).c_str());
    printf("Sunset Time: %s\n", getCustomZonedTimestamp(weather.day.sunset).c_str());
    printf("Moonrise Time: %s\n", getCustomZonedTimestamp(weather.day.moonrise).c_str());
    printf("Moonset Time: %s\n", getCustomZonedTimestamp(weather.day.moonset).c_str());
    printf("Location: ");
    if (isLocationValid("current"))
      printf("%s, %s, %s\n", current.city, current.state, current.country);
    else
      printf("Currently not known\n");
    printf("[^----------------------------------------------------------------^]\n");
    printf("Firmware - Version:v%d.%d.%d | Config:v%s\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_CONFIG);
    printf("System - RawLux:%d | Lux:%d | UsrBright:+%d | Brightness:%d | ClockHue:%d | TempHue:%d | WifiState:%s | HttpReady:%s | IP:%s | Uptime:%s\n", current.rawlux, current.lux, userbrightness, current.brightness, current.clockhue, current.temphue, (connection_state[iotWebConf.getState()]), (yesno[isHttpReady()]), ((WiFi.localIP()).toString()).c_str(), elapsedTime(now - bootTime));
    printf("Clock - Status:%s | TimeSource:%s | CurrentTZ:%d | NtpReady:%s | LastAttempt:%s | NextAttempt:%s | Skew:%d Seconds | NextNtp:%s | LastSync:%s\n", clock_status[systemClock.getSyncStatusCode()], timesource, current.tzoffset, yesno[gpsClock.ntpIsReady], (elapsedTime(systemClock.getSecondsSinceSyncAttempt())), (elapsedTime(systemClock.getSecondsToSyncAttempt())), systemClock.getClockSkew(), elapsedTime((now - lastntpcheck) - NTPCHECKTIME * 60), elapsedTime(now - systemClock.getLastSyncTime()));
    printf("Loc - SavedLat:%.5f | SavedLon:%.5f | CurrentLat:%.5f | CurrentLon:%.5f | LocValid:%s\n", atof(savedlat.value()), atof(savedlon.value()), current.lat, current.lon, yesno[isCoordsValid()]);
    printf("IPGeo - Complete:%s | Lat:%.5f | Lon:%.5f | TZoffset:%d | Timezone:%s | ValidApi:%s | Retries:%d | LastAttempt:%s | LastSuccess:%s\n", yesno[checkipgeo.complete], ipgeo.lat, ipgeo.lon, ipgeo.tzoffset, ipgeo.timezone, yesno[isApiValid(ipgeoapi.value())], checkipgeo.retries, elapsedTime(now - checkipgeo.lastattempt), elapsedTime(now - checkipgeo.lastsuccess));
    printf("GPS - Chars:%s | With-Fix:%s | Failed:%s | Passed:%s | Sats:%d | Hdop:%d | Elev:%d | Lat:%.5f | Lon:%.5f | FixAge:%s | LocAge:%s\n", formatLargeNumber(GPS.charsProcessed()).c_str(), formatLargeNumber(GPS.sentencesWithFix()).c_str(), formatLargeNumber(GPS.failedChecksum()).c_str(), formatLargeNumber(GPS.passedChecksum()).c_str(), gps.sats, gps.hdop, gps.elevation, gps.lat, gps.lon, elapsedTime(now - gps.timestamp), elapsedTime(now - gps.lockage));
    printf("Weather Current - Icon:%s | Temp:%d%s | FeelsLike:%d%s | Humidity:%d%% | Clouds:%d%% | Wind:%d/%d%s | UVI:%d(%s) | Desc:%s | ValidApi:%s | Retries:%d | LastAttempt:%s | LastSuccess:%s | NextShow:%s\n", weather.current.icon, weather.current.temp, tempunit, weather.current.feelsLike, tempunit, weather.current.humidity, weather.current.cloudcover, weather.current.windSpeed, weather.current.windGust, speedunit, weather.current.uvi, uv_index(weather.current.uvi).c_str(), weather.current.description, yesno[isApiValid(weatherapi.value())], checkweather.retries, elapsedTime(now - checkweather.lastattempt), elapsedTime(now - checkweather.lastsuccess), elapsedTime((now - lastshown.currentweather) - (current_weather_interval.value()*60)));
    printf("Weather Day - Icon:%s | LoTemp:%d%s | HiTemp:%d%s | Humidity:%d%% | Clouds:%d%% | Wind: %d/%d%s | UVI:%d(%s) | MoonPhase:%.2f | Desc: %s | NextShow:%s\n", weather.day.icon, weather.day.tempMin, tempunit, weather.day.tempMax, tempunit, weather.day.humidity, weather.day.cloudcover, weather.day.windSpeed, weather.day.windGust, speedunit, weather.day.uvi, uv_index(weather.day.uvi).c_str(), weather.day.moonPhase, weather.current.description, elapsedTime((now - lastshown.dayweather) - (daily_weather_interval.value()*60*60)));
    printf("AQI Current - Index:%s | Co:%.2f | No:%.2f | No2:%.2f | Ozone:%.2f | So2:%.2f | Pm2.5:%.2f | Pm10:%.2f | Ammonia:%.2f | Retries:%d | LastAttempt:%s | LastSuccess:%s | NextShow:%s\n", air_quality[aqi.current.aqi], aqi.current.co, aqi.current.no, aqi.current.no2, aqi.current.o3, aqi.current.so2, aqi.current.pm10, aqi.current.pm25, aqi.current.nh3, checkaqi.retries, elapsedTime(now - checkaqi.lastattempt), elapsedTime(now - checkaqi.lastsuccess), elapsedTime((now - lastshown.aqi) - (aqi_interval.value()*60)));
    printf("AQI Day - Index:%s | Co:%.2f | No:%.2f | No2:%.2f | Ozone:%.2f | So2:%.2f | Pm2.5:%.2f | Pm10:%.2f | Ammonia:%.2f\n", air_quality[aqi.day.aqi], aqi.day.co, aqi.day.no, aqi.day.no2, aqi.day.o3, aqi.day.so2, aqi.day.pm10, aqi.day.pm25, aqi.day.nh3);
    printf("Alerts - Active:%s | Watch:%s | Warn:%s | Retries:%d | LastAttempt:%s | LastSuccess:%s | LastShown:%s\n", yesno[alerts.active], yesno[alerts.inWatch], yesno[alerts.inWarning], checkalerts.retries, elapsedTime(now - checkalerts.lastattempt), elapsedTime(now - checkalerts.lastsuccess), elapsedTime(now - lastshown.alerts));    
    printf("[^----------------------------------------------------------------^]\n");
    printf("Main task stack size: %s bytes remaining\n", formatLargeNumber(uxTaskGetStackHighWaterMark(NULL)).c_str());
    printf("Current Display Tokens: %s\n", displaytoken.showTokens());
    printf("[^----------------------------------------------------------------^]\n");
    printf("now: %lu\n", now);
    printf("aqils: %lu\n", checkaqi.lastsuccess);
    printf("et: %s\n", elapsedTime(now - checkaqi.lastsuccess));
    if (alerts.active)
      printf("*Alert - Status: %s | Severity: %s | Certainty: %s | Urgency: %s | Event: %s | Desc: %s %s\n", alerts.status1, alerts.severity1, alerts.certainty1, alerts.urgency1, alerts.event1, alerts.description1,alerts.instruction1);
}

void startFlash(uint16_t color, uint8_t laps)
{
  alertflash.color = color;
  alertflash.laps = laps;
  alertflash.active = true;
  showClock.suspend();
}

void startScroll(uint16_t color, bool displayicon)
{
  scrolltext.color = color;
  scrolltext.active = true;
  scrolltext.displayicon = displayicon;
  showClock.suspend();
}

// Process timezone to set current timezone offsets by taking into account user setting or ipgeolocation data
void processTimezone() 
{
  uint32_t timer = millis();
  // Get saved offset value
  int16_t savedoffset = savedtzoffset.value();
  // Check if the user has set a fixed offset in the settings
  if (enable_fixed_tz.isChecked())
  {
    // If so, save it and set the current timezone with the corresponding TimeZone object
    current.tzoffset = fixed_offset.value();
    current.timezone = TimeZone::forHours(fixed_offset.value());
    ESP_LOGD(TAG, "Using fixed timezone offset: %d", fixed_offset.value());
  }
  // Check if any valid ipgeolocation offset is available 
  else if (ipgeo.tzoffset != 127) 
  {
    // If so, save it and set the current timezone with the corresponding TimeZone object 
    current.tzoffset = ipgeo.tzoffset;
    current.timezone = TimeZone::forHours(ipgeo.tzoffset);
    ESP_LOGD(TAG, "Using ipgeolocation offset: %d", ipgeo.tzoffset);
    // Save the new offset to config if it's different from the saved one
    if (ipgeo.tzoffset != savedoffset)
    {
      ESP_LOGI(TAG, "IP Geo timezone [%d] (%s) is different then saved timezone [%d], saving new timezone", ipgeo.tzoffset, ipgeo.timezone, savedoffset);
      savedtzoffset.value() = ipgeo.tzoffset;
      iotWebConf.saveConfig();
    }
  }
  // If none of the above conditions is true, we fallback to the saved offset
  else 
  {
    current.tzoffset = savedoffset;
    current.timezone = TimeZone::forHours(savedoffset);
    ESP_LOGD(TAG, "Using saved timezone offset: %d", savedoffset);
  }
  ESP_LOGV(TAG, "Process timezone complete: %lu ms", (millis()-timer));
}

void updateLocation() 
{
  // Update location information
  if (isLocationValid("geocode"))
  {
    if (!cmpLocs(geocode.city, current.city))
    {
      showClock.suspend();
      showready.loc = true;
    }
    // Set current location fields
    strcpy(current.city, geocode.city);
    strcpy(current.state, geocode.state);
    strcpy(current.country, geocode.country);
    ESP_LOGI(TAG, "Using Geocode location: %s, %s, %s", current.city, current.state, current.country);
  }
  else if (isLocationValid("saved"))
  {
    // Set current location fields to saved location
    strcpy(current.city, savedcity.value());
    strcpy(current.state, savedstate.value());
    strcpy(current.country, savedcountry.value());
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
  // Save the new location if needed
  if ((String)savedcity.value() != current.city)
  {
    ESP_LOGI(TAG, "Location not saved, saving new location: %s, %s, %s", current.city, current.state, current.country);
    // Convert city/state/country to char array and save to savedcity/state/country
    strcpy(savedcity.value(), current.city);
    strcpy(savedstate.value(), current.state);
    strcpy(savedcountry.value(), current.country);
    iotWebConf.saveConfig();
  }
}  

void updateCoords()
{
  if (enable_fixed_loc.isChecked())
  {
    gps.lat = strtod(fixedLat.value(), NULL);
    gps.lon = strtod(fixedLon.value(), NULL);
    current.lat = strtod(fixedLat.value(), NULL);
    current.lon = strtod(fixedLon.value(), NULL);
    strcpy(current.locsource, "User Defined");
  }
  else if (gps.lon == 0 && ipgeo.lon == 0 && current.lon == 0)
  {
    // Set coordinates to saved location
    current.lat = strtod(savedlat.value(), NULL);
    current.lon = strtod(savedlon.value(), NULL);
    strcpy(current.locsource, "Previous Saved");
  }
  else if (gps.lon == 0 && ipgeo.lon != 0 && current.lon == 0)
  {
    // Set coordinates to ip geolocation
    current.lat = ipgeo.lat; 
    current.lon = ipgeo.lon;
    strcpy(current.locsource, "IP Geolocation");
    ESP_LOGI(TAG, "Using IPGeo location information: Lat: %lf Lon: %lf", (ipgeo.lat), (ipgeo.lon));
  }
  else if (gps.lon != 0)
  {
    // Set coordinates to gps location
    current.lat = gps.lat;
    current.lon = gps.lon;
    strcpy(current.locsource, "GPS");
  }
  // Calculate if major location shift has taken place
  double olat = strtod(savedlat.value(), NULL);
  double olon = strtod(savedlon.value(), NULL);
  if (abs(current.lat - olat) > 0.02 || abs(current.lon - olon) > 0.02) 
  {
      // Log warning and save new coordinates
      ESP_LOGW(TAG, "Major location shift, saving values");
      ESP_LOGW(TAG, "Lat:[%0.6lf]->[%0.6lf] Lon:[%0.6lf]->[%0.6lf]", olat, current.lat, olon, current.lon);
      dtostrf(current.lat, 9, 5, savedlat.value());
      dtostrf(current.lon, 9, 5, savedlon.value());
      iotWebConf.saveConfig();
      checkgeocode.ready = true;
  }
  // Call buildURLs() to create URLS
  buildURLs();
}

void display_setclockDigit(uint8_t bmp_num, uint8_t position, uint16_t color) 
{
  // define the column value based on the position parameter
  uint8_t pos;
  switch (position) 
  {
    case 0 : pos = 0; break; // set to 0 when position is 0
    case 1 : pos = 7; break; // set to 7 when position is 1
    case 2 : pos = 16; break; // set to 16 when position is 2
    case 3 : pos = 23; break; // set to 23 when position is 3
  }
  // draw the bitmap using column and color parameters
  matrix->drawBitmap((clock_display_offset) ? (pos - 4) : pos, 0, num[bmp_num], 8, 8, color);
}

void display_showStatus()
{
  uint16_t sclr;
  uint16_t wclr;
  uint16_t aclr;
  uint16_t uclr;
  uint16_t gclr;
  if (green_status.isChecked())
    gclr = DARKGREEN;
  else
    gclr = BLACK;
  acetime_t now = systemClock.getNow();
  // Set the status color
  if (now - systemClock.getLastSyncTime() > (NTPCHECKTIME*60) + 60)
      sclr = DARKRED;
  else if (timesource[0] == 'g' && gps.fix && (now - systemClock.getLastSyncTime()) <= 600)
      sclr = BLACK;
  else if (timesource[0] == 'n' && iotWebConf.getState() == 4)
      sclr = DARKGREEN;
  else if (iotWebConf.getState() <= 2)  // boot, apmode, notconfigured
      sclr = DARKMAGENTA;
  else if (iotWebConf.getState() == 3 || iotWebConf.getState() == 4)  // connecting, online
      sclr = DARKCYAN;
  else
      sclr = DARKRED;
  if (enable_system_status.isChecked() || sclr == DARKRED)
    matrix->drawPixel(0, 7, sclr);
  // Check the AQI and wether an alert is present
  if (enable_aqi_status.isChecked()&& checkaqi.complete)
  {
    switch (aqi.current.aqi)
    {
        case 2: aclr = DARKYELLOW; break;
        case 3: aclr = DARKORANGE; break;
        case 4: aclr = DARKRED; break;
        case 5: aclr = DARKPURPLE; break;
        default: aclr = gclr; break;
    }
    matrix->drawPixel(0, 0, aclr);
  }
  if (enable_uvi_status.isChecked() && checkweather.complete)
  {
    switch (weather.current.uvi)
    {
        case 0: uclr = gclr; break;
        case 1: uclr = gclr; break;
        case 2: uclr = DARKYELLOW; break;
        case 3: uclr = DARKORANGE; break;
        case 4: uclr = DARKRED; break;
        case 5: uclr = DARKPURPLE; break;
        default: uclr = gclr; break;
    }
  matrix->drawPixel(mw-1, 0, uclr);
  }
  if (alerts.inWarning)
    wclr = RED;
  else if (alerts.inWatch)
    wclr = YELLOW;
  else 
    wclr = BLACK;
  if (wclr != BLACK) 
  {
    matrix->drawPixel(0, 3, wclr);
    matrix->drawPixel(0, 4, wclr);
    matrix->drawPixel(mw - 1, 3, wclr);
    matrix->drawPixel(mw - 1, 4, wclr);
  }
  // If colors are different from before show matrix again
  if (current.oldaqiclr != aclr || current.oldstatusclr != sclr || current.oldstatuswclr != wclr || current.olduvicolor != uclr)
  {
    matrix->show();
    current.oldstatusclr = sclr;
    current.oldstatuswclr = wclr;
    current.oldaqiclr = aclr;
    current.olduvicolor = uclr;
  }
}

void display_weatherIcon(char* icon) 
{
  bool night;
  char dn = icon[2];
  night = (dn == 'n' ? true : false);
  char code1 = icon[0];
  char code2 = icon[1];
  // Clear
  if (code1 == '0' && code2 == '1') 
  {
    matrix->drawRGBBitmap(mw-8, 0, (night ? clear_night[cotimer.iconcycle] : clear_day[cotimer.iconcycle]), 8, 8);
  }
  // Partly cloudy
  else if ((code1 == '0' && code2 == '2') || (code1 == '0' && code2 == '3')) 
  {
    matrix->drawRGBBitmap(mw-8, 0, (night ? partly_cloudy_night[cotimer.iconcycle] : partly_cloudy_day[cotimer.iconcycle]), 8, 8);
  }
  // Cloudy
  else if ((code1 == '0') && (code2 == '4')) 
  {
    matrix->drawRGBBitmap(mw-8, 0, cloudy[cotimer.iconcycle], 8, 8);
  }
  // Fog/mist 
  else if (code1 == '5' && code2 == '0') 
  {
    matrix->drawRGBBitmap(mw-8, 0, fog[cotimer.iconcycle], 8, 8);
  }
  // Snow
  else if (code1 == '1' && code2 == '3') 
  {
    matrix->drawRGBBitmap(mw-8, 0, snow[cotimer.iconcycle], 8, 8);
  }
  // Rain
  else if (code1 == '0' && code2 == '9') 
  {
    matrix->drawRGBBitmap(mw-8, 0, rain[cotimer.iconcycle], 8, 8);
  }
  // Heavy rain
  else if (code1 == '1' && code2 == '0') 
  {
    matrix->drawRGBBitmap(mw-8, 0, heavy_rain[cotimer.iconcycle], 8, 8);
  }
  // Thunderstorm
  else if (code1 == '1' && code2 == '1') 
  {
    matrix->drawRGBBitmap(mw-8, 0, thunderstorm[cotimer.iconcycle], 8, 8);
  } 
  else 
  {
    ESP_LOGE(TAG, "No Weather icon found to use");
    return;
  }
  // Animate
  if (millis() - cotimer.icontimer > ANI_SPEED) 
  {
    cotimer.iconcycle = (cotimer.iconcycle == ANI_BITMAP_CYCLES-1) ? 0 : cotimer.iconcycle + 1;
    cotimer.icontimer = millis();
  }
}

void display_temperature() 
{
  int temp = weather.current.feelsLike;
  bool imperial_setting = imperial.isChecked();
  int tl = (imperial_setting)? 32 : 0;
  int th = (imperial_setting)? 104 : 40;
  uint32_t tc;
  if(enable_temp_color.isChecked())
    tc = hex2rgb(temp_color.value());
  else
    current.temphue = constrain(map(weather.current.feelsLike, tl, th, NIGHTHUE, 0), 0, NIGHTHUE);

  if (weather.current.feelsLike < tl) {
    if (imperial_setting && weather.current.feelsLike + abs(weather.current.feelsLike) == 0)
      current.temphue = NIGHTHUE + tl;
    else
      current.temphue = NIGHTHUE + (abs(weather.current.feelsLike) / 2);
  }
  tc = hsv2rgb(current.temphue);
  int xpos;
  int digits = (temp == 0) ? 1 : (log10(abs(temp)) + 1);
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
  getSystemZonedTime().printTo(SERIAL_PORT_MONITOR);
  SERIAL_PORT_MONITOR.println();
}

ZonedDateTime getSystemZonedTime() 
{
  return ZonedDateTime::forEpochSeconds(systemClock.getNow(), current.timezone);
}

String getSystemZonedTimestamp() 
{
 acetime_t now = systemClock.getNow();
 ZonedDateTime TimeWZ = ZonedDateTime::forEpochSeconds(now, current.timezone);
 char str[64];
 // Create string with year, month, day, hour, minute, second and time offset values
 snprintf(str, sizeof(str), "%02d/%02d/%02d %02d:%02d:%02d [GMT%+d]", TimeWZ.month(), TimeWZ.day(), TimeWZ.year(), 
 TimeWZ.hour(), TimeWZ.minute(), TimeWZ.second(), current.tzoffset);
 return (String)str;
}

String getCustomZonedTimestamp(acetime_t now) 
{
 ZonedDateTime TimeWZ = ZonedDateTime::forEpochSeconds(now, current.timezone);
 char str[64];
 // Create string with year, month, day, hour, minute, second and time offset values
 snprintf(str, sizeof(str), "%02d/%02d/%02d %02d:%02d:%02d [GMT%+d]", TimeWZ.month(), TimeWZ.day(), TimeWZ.year(), 
 TimeWZ.hour(), TimeWZ.minute(), TimeWZ.second(), current.tzoffset);
 return (String)str;
}

void getSystemZonedDateString(char *str) 
{
  // Get zoned date time object
  ZonedDateTime ldt = getSystemZonedTime();
  // Format date string
  sprintf(str, "%s, %s %d%s %d",
          DateStrings().dayOfWeekLongString(ldt.dayOfWeek()),
          DateStrings().monthLongString(ldt.month()),
          ldt.day(), ordinal_suffix(ldt.day()), ldt.year());
}

char *getSystemZonedDateTimeString() 
{
  // Get time from the system
  ZonedDateTime ldt = getSystemZonedTime();
  // Create buffer for string representation
  static char buf[100];
  // Check if 12H format is selected
  if (twelve_clock.isChecked()) {
      // Get AM/PM notation from the hour
      const char* ap = ldt.hour() > 11 ? "PM" : "AM";
      // In case of 12H format, adjust the hour
      uint8_t hour = ldt.hour() > 12 ? ldt.hour() - 12 : ldt.hour();
      // Construct the string with formatting parameters
      sprintf(buf, "%d:%02d%s %s, %s %u%s %04d", hour, ldt.minute(), ap, DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()), 
      ldt.day(), ordinal_suffix(ldt.day()), ldt.year());
  } 
  else {
      // Construct the string with formatting parameters for 24H formats
      sprintf(buf, "%02d:%02d %s, %s %d%s %04d", ldt.hour(), ldt.minute(), DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()), 
      ldt.day(), ordinal_suffix(ldt.day()), ldt.year());
  }
  // Return the constructed string
  return buf;
}

uint16_t calcaqi(double c, double i_hi, double i_low, double c_hi, double c_low) 
{
  return (i_hi - i_low)/(c_hi - c_low)*(c - c_low) + i_low;
}

bool fillAlertsFromJson(String payload) 
{
  StaticJsonDocument<2048> filter;
  filter["features"][0]["properties"]["status"] = true;
  filter["features"][0]["properties"]["severity"] = true;
  filter["features"][0]["properties"]["certainty"] = true;
  filter["features"][0]["properties"]["urgency"] = true;
  filter["features"][0]["properties"]["event"] = true;
  filter["features"][0]["properties"]["instruction"] = true;
  filter["features"][0]["properties"]["parameters"]["NWSheadline"] = true;
  filter["features"][1]["properties"]["status"] = true;
  filter["features"][1]["properties"]["severity"] = true;
  filter["features"][1]["properties"]["certainty"] = true;
  filter["features"][1]["properties"]["urgency"] = true;
  filter["features"][1]["properties"]["event"] = true;
  filter["features"][1]["properties"]["instruction"] = true;
  filter["features"][1]["properties"]["parameters"]["NWSheadline"] = true;
  filter["features"][2]["properties"]["status"] = true;
  filter["features"][2]["properties"]["severity"] = true;
  filter["features"][2]["properties"]["certainty"] = true;
  filter["features"][2]["properties"]["urgency"] = true;
  filter["features"][2]["properties"]["event"] = true;
  filter["features"][2]["properties"]["instruction"] = true;
  filter["features"][2]["properties"]["parameters"]["NWSheadline"] = true;
  StaticJsonDocument <8192> doc;
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error) 
  {
      ESP_LOGE(TAG, "Alerts deserializeJson() failed: %s", error.f_str());
      return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  uint8_t actwatch = 0;
  uint8_t actwarn = 0;
  bool result;
  if (!obj["features"][0].isNull())
  {
    if ((obj["features"][0]["properties"]["severity"].as<String>()) != "Unknown")
    {
      if ((obj["features"][0]["properties"]["certainty"].as<String>()) == "Observed" || (obj["features"][0]["properties"]["certainty"].as<String>()) == "Likely")
        actwarn = 1;
      else if ((obj["features"][0]["properties"]["certainty"].as<String>()) == "Possible")
        actwatch = 1;
    }
    else
      ESP_LOGD(TAG, "Weather alert 1 received but is of status Unknown, skipping");
    if (!obj["features"][1].isNull())
    {
      if ((obj["features"][1]["properties"]["severity"].as<String>()) != "Unknown")
      {
        if ((obj["features"][1]["properties"]["certainty"].as<String>()) == "Observed" || (obj["features"][1]["properties"]["certainty"].as<String>()) == "Likely")
        {
          if (actwarn == 0) actwarn = 2;
        }
        else if ((obj["features"][1]["properties"]["certainty"].as<String>()) == "Possible")
          if (actwatch == 0) actwatch = 2;
      }
      else
        ESP_LOGD(TAG, "Weather alert 2 received but is of status Unknown, skipping");
      if(!obj["features"][2].isNull())
      {
        if ((obj["features"][2]["properties"]["severity"].as<String>()) != "Unknown")
        {
          if ((obj["features"][2]["properties"]["certainty"].as<String>()) == "Observed" || (obj["features"][2]["properties"]["certainty"].as<String>()) == "Likely")
          {
            if (actwarn == 0)
              actwarn = 3;
          }
          else if ((obj["features"][2]["properties"]["certainty"].as<String>()) == "Possible")
          {
            if (actwatch == 0)
              actwatch = 3;
          }
        }
        else
          ESP_LOGD(TAG, "Weather alert 3 received but is of status Unknown, skipping");
      }
    }
  }
  if (actwarn > 0) 
  {
    alerts.inWarning = true;
    alerts.inWatch = false;
    alerts.active = true;
    showready.alerts = true;
    alerts.timestamp = systemClock.getNow();
    ESP_LOGI(TAG, "Active weather WARNING alert received");
  }
  else if (actwatch > 0)
  {
    alerts.inWarning = false;
    alerts.inWatch = true;
    alerts.active = true;
    showready.alerts = true;
    alerts.timestamp = systemClock.getNow();    
    ESP_LOGI(TAG, "Active weather WATCH alert received");
  }
  if (actwarn == 1 || actwatch == 1)
  {
    if (!obj["features"][0]["properties"]["status"].isNull())
    {
      sprintf(alerts.status1, "%s", (const char *)obj["features"][0]["properties"]["status"]);
      sprintf(alerts.severity1, "%s", (const char *)obj["features"][0]["properties"]["severity"]);
      sprintf(alerts.certainty1, "%s", (const char *)obj["features"][0]["properties"]["certainty"]);
      sprintf(alerts.urgency1, "%s", (const char *)obj["features"][0]["properties"]["urgency"]);
      sprintf(alerts.event1, "%s", (const char *)obj["features"][0]["properties"]["event"]);
    }
    if (!obj["features"][0]["properties"]["parameters"]["NWSheadline"][0].isNull()) {
      sprintf(alerts.description1, "%s", (const char *)obj["features"][0]["properties"]["parameters"]["NWSheadline"][0]);
      cleanString(alerts.description1);
      result = true;
    }
    if (!obj["features"][0]["properties"]["instruction"].isNull())
    {
      sprintf(alerts.instruction1, "%s", (const char *)obj["features"][0]["properties"]["instruction"]);
      cleanString(alerts.instruction1);
    }
    else
      sprintf(alerts.instruction1, "%s", '\0');
  }
  else if (actwarn == 2 || actwatch == 2)
  {
    sprintf(alerts.status1, "%s", (const char *)obj["features"][1]["properties"]["status"]);
    sprintf(alerts.severity1, "%s", (const char *)obj["features"][1]["properties"]["severity"]);
    sprintf(alerts.certainty1, "%s", (const char *)obj["features"][1]["properties"]["certainty"]);
    sprintf(alerts.urgency1, "%s", (const char *)obj["features"][1]["properties"]["urgency"]);
    sprintf(alerts.event1, "%s", (const char *)obj["features"][1]["properties"]["event"]);
    if (!obj["features"][1]["properties"]["parameters"]["NWSheadline"][0].isNull()) 
    {
      sprintf(alerts.description1, "%s", (const char *)obj["features"][1]["properties"]["parameters"]["NWSheadline"][0]);
      cleanString(alerts.description1);
      result = true;
    }
    if (!obj["features"][1]["properties"]["instruction"].isNull())
    {
      sprintf(alerts.instruction1, "%s", (const char *)obj["features"][1]["properties"]["instruction"]);
      cleanString(alerts.instruction1);
    }
    else
      sprintf(alerts.instruction1, "%s", '\0');
  }
  else if (actwarn == 3 || actwatch == 3)
  {
    sprintf(alerts.status1, "%s", (const char *)obj["features"][2]["properties"]["status"]);
    sprintf(alerts.severity1, "%s", (const char *)obj["features"][2]["properties"]["severity"]);
    sprintf(alerts.certainty1, "%s", (const char *)obj["features"][2]["properties"]["certainty"]);
    sprintf(alerts.urgency1, "%s", (const char *)obj["features"][2]["properties"]["urgency"]);
    sprintf(alerts.event1, "%s", (const char *)obj["features"][2]["properties"]["event"]);
    if (!obj["features"][2]["properties"]["parameters"]["NWSheadline"][0].isNull()) 
    {
      sprintf(alerts.description1, "%s", (const char *)obj["features"][2]["properties"]["parameters"]["NWSheadline"][0]);
      cleanString(alerts.description1);
      result = true;
    }
    if (!obj["features"][2]["properties"]["instruction"].isNull())
    {
      sprintf(alerts.instruction1, "%s", (const char *)obj["features"][2]["properties"]["instruction"]);
      cleanString(alerts.instruction1);
    }
    else
      sprintf(alerts.instruction1, "%s", '\0');
  }
  else 
  {
    alerts.active = false;
    alerts.inWarning = false;
    alerts.inWatch= false;
    ESP_LOGI(TAG, "No current active weather alerts");
    result = true;
  }
  return result;
}

bool fillWeatherFromJson(String payload) 
{
  bool result;
  StaticJsonDocument<2048> filter;
  filter["current"]["weather"][0]["icon"] = true;
  filter["current"]["temp"] = true;
  filter["current"]["feels_like"] = true;
  filter["current"]["humidity"] = true;
  filter["current"]["wind_speed"] = true;
  filter["current"]["uvi"] = true;
  filter["current"]["clouds"] = true;
  filter["current"]["weather"][0]["description"] = true;
  filter["hourly"][0]["wind_gust"] = true;
  filter["daily"][0]["temp"]["min"] = true;
  filter["daily"][0]["temp"]["max"] = true;
  filter["daily"][0]["sunrise"] = true;
  filter["daily"][0]["sunset"] = true;
  filter["daily"][0]["moonrise"] = true;
  filter["daily"][0]["moonset"] = true;
  filter["daily"][0]["uvi"] = true;
  filter["daily"][0]["clouds"] = true;
  filter["daily"][0]["humidity"] = true;
  filter["daily"][0]["wind_speed"] = true;
  filter["daily"][0]["wind_gust"]= true;
  filter["daily"][0]["moon_phase"]= true;
  filter["daily"][0]["weather"][0]["description"] = true;
  filter["daily"][0]["weather"][0]["icon"]= true;
  StaticJsonDocument<8192> doc;
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error) {
    ESP_LOGE(TAG, "Weather deserializeJson() failed: %s", error.f_str());
    return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  if (!obj["current"]["weather"][0].isNull())
  {
    sprintf(weather.current.icon, "%s", (const char *)obj["current"]["weather"][0]["icon"]);
    weather.current.temp = obj["current"]["temp"];
    weather.current.feelsLike = obj["current"]["feels_like"];
    weather.current.humidity = obj["current"]["humidity"];
    weather.current.windSpeed = obj["current"]["wind_speed"];
    weather.current.uvi = obj["current"]["uvi"];
    weather.current.cloudcover = obj["current"]["clouds"];
    sprintf(weather.current.description, "%s", (const char *)obj["current"]["weather"][0]["description"]);
    result = true;
  }
  weather.current.windGust = obj["hourly"][0]["wind_gust"];
  if (!obj["daily"][0].isNull())
  {
    weather.day.tempMin = obj["daily"][0]["temp"]["min"];
    weather.day.tempMax = obj["daily"][0]["temp"]["max"];
    weather.day.humidity = obj["daily"][0]["humidity"];
    weather.day.windSpeed = obj["daily"][0]["wind_speed"];
    weather.day.windGust = obj["daily"][0]["wind_gust"];
    weather.day.uvi = obj["daily"][0]["uvi"];
    weather.day.cloudcover = obj["daily"][0]["clouds"];
    weather.day.moonPhase = obj["daily"][0]["moon_phase"];
    weather.day.sunrise = convert1970Epoch(obj["daily"][0]["sunrise"]);
    weather.day.sunset = convert1970Epoch(obj["daily"][0]["sunset"]);
    weather.day.moonrise = convert1970Epoch(obj["daily"][0]["moonrise"]);
    weather.day.moonset = convert1970Epoch(obj["daily"][0]["moonset"]);
    sprintf(weather.day.description, "%s", (const char *)obj["daily"][0]["weather"][0]["description"]);
    sprintf(weather.day.icon, "%s", (const char *)obj["daily"][0]["weather"][0]["icon"]);
    result = true;
  }
  return result;
}

bool fillIpgeoFromJson(String payload) {
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    ESP_LOGE(TAG, "IPGeo deserializeJson() failed: %s", error.f_str());
    return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  if (!obj["time_zone"].isNull())
  {
    sprintf(ipgeo.timezone, "%s", (const char *)obj["time_zone"]["name"]);
    ipgeo.tzoffset = obj["time_zone"]["offset"];
    ipgeo.lat = obj["latitude"];
    ipgeo.lon = obj["longitude"];
    return true;
  }
  return false;
}

bool fillGeocodeFromJson(String payload) {
  StaticJsonDocument<256> doc;
  payload = (String)"{data:" + payload + "}";
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    ESP_LOGE(TAG, "GEOcode deserializeJson() failed: %s", error.f_str());
    return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  if (!obj["data"][0].isNull())
  {
    sprintf(geocode.city, "%s", (const char *)obj["data"][0]["name"]);
    sprintf(geocode.state, "%s", (const char *)obj["data"][0]["state"]);
    sprintf(geocode.country, "%s", (const char *)obj["data"][0]["country"]);
    return true;
  }
  return false;
}

bool fillAqiFromJson(String payload) {
  bool ready = false;
  /*
  StaticJsonDocument<8196> filter;
  filter["list"][0]["main"]["aqi"] = true;
  filter["list"][1]["main"]["aqi"] = true;
  filter["list"][1]["components"]["co"] = true;
  filter["list"][1]["components"]["no"] = true;
  filter["list"][1]["components"]["no2"] = true;
  filter["list"][1]["components"]["o3"] = true;
  filter["list"][1]["components"]["so2"] = true;
  filter["list"][1]["components"]["pm2_5"] = true;
  filter["list"][1]["components"]["pm10"] = true;
  filter["list"][1]["components"]["nh3"] = true;
  filter["list"][7]["main"]["aqi"] = true;
  filter["list"][7]["components"]["co"] = true;
  filter["list"][7]["components"]["no"] = true;
  filter["list"][7]["components"]["no2"] = true;
  filter["list"][7]["components"]["o3"] = true;
  filter["list"][7]["components"]["so2"] = true;
  filter["list"][7]["components"]["pm2_5"] = true;
  filter["list"][7]["components"]["pm10"] = true;
  filter["list"][7]["components"]["nh3"] = true;
  */
  StaticJsonDocument<20480> doc;
  DeserializationError error = deserializeJson(doc, payload);
  //DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error) {
    ESP_LOGE(TAG, "AQI deserializeJson() failed: %s", error.f_str());
    return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  if (!obj["list"][1].isNull())
  {
    aqi.current.aqi = obj["list"][1]["main"]["aqi"];
    aqi.current.co = obj["list"][1]["components"]["co"];
    aqi.current.no = obj["list"][1]["components"]["no"];
    aqi.current.no2 = obj["list"][1]["components"]["no2"];
    aqi.current.o3 = obj["list"][1]["components"]["o3"];
    aqi.current.so2 = obj["list"][1]["components"]["so2"];
    aqi.current.pm10 = obj["list"][1]["components"]["pm2_5"];
    aqi.current.pm25 = obj["list"][1]["components"]["pm10"];
    aqi.current.nh3 = obj["list"][1]["components"]["nh3"];
    ready = true;
    ESP_LOGD(TAG, "New current air quality data received");
  }
  else
    ESP_LOGE(TAG, "checkAQI error cannot find current aqi data from response");
  if (!obj["list"][7].isNull())
  {
    aqi.day.aqi = obj["list"][7]["main"]["aqi"];
    aqi.day.co = obj["list"][7]["components"]["co"];
    aqi.day.no = obj["list"][7]["components"]["no"];
    aqi.day.no2 = obj["list"][7]["components"]["no2"];
    aqi.day.o3 = obj["list"][7]["components"]["o3"];
    aqi.day.so2 = obj["list"][7]["components"]["so2"];
    aqi.day.pm10 = obj["list"][7]["components"]["pm2_5"];
    aqi.day.pm25 = obj["list"][7]["components"]["pm10"];
    aqi.day.nh3 = obj["list"][7]["components"]["nh3"];
    ready = true;
    ESP_LOGD(TAG, "New daily air quality data received");
  }
  else
    ESP_LOGE(TAG, "checkAQI error cannot find daily aqi data from response");
  return ready;
}

void buildURLs() 
{
  char units[32];
  if (imperial.isChecked())
    strcpy(units, "imperial");
  else
    strcpy(units, "metric");
  char url[256];
  // Weather forecast url
  snprintf(url, 256, "https://api.openweathermap.org/data/2.5/onecall?units=%s&exclude=minutely&appid=%s&lat=%f&lon=%f&lang=en",
    units, weatherapi.value(), current.lat, current.lon);
  strncpy(urls[0], url, 256);
  // Weather alert url
  snprintf(url, 256, "https://api.weather.gov/alerts/active?status=actual&point=%f,%f&limit=1",
    current.lat, current.lon);
  strncpy(urls[1], url, 256);
  // Geocoding url
  snprintf(url, 256, "https://api.openweathermap.org/geo/1.0/reverse?lat=%f&lon=%f&limit=5&appid=%s",
    current.lat, current.lon, weatherapi.value());
  strncpy(urls[2], url, 256);
  // Air pollution url
  snprintf(url, 256, "https://api.openweathermap.org/data/2.5/air_pollution/forecast?lat=%f&lon=%f&appid=%s",
    current.lat, current.lon, weatherapi.value());
  strncpy(urls[3], url, 256);
  // ipGeo API Key
  snprintf(url, 256, "https://api.ipgeolocation.io/ipgeo?apiKey=%s",
    ipgeoapi.value());
  strncpy(urls[4], url, 256);
  // Air pollution forecast url
  snprintf(url, 256, "notusedyet",
    current.lat, current.lon, weatherapi.value());
  strncpy(urls[5], url, 256);
}

char* elapsedTime(uint32_t seconds) {
    static char buf[32];
    if (seconds < 60) {
        sprintf(buf, "%u seconds", seconds);
        return buf;
    }
    uint32_t minutes = seconds / 60;
    if (minutes < 60) {
        sprintf(buf, "%u minutes, %u seconds", minutes, seconds % 60);
        return buf;
    }
    uint32_t hours = minutes / 60;
    if (hours < 24) {
        sprintf(buf, "%u hours, %u minutes", hours, minutes % 60);
        return buf;
    }
    uint32_t days = hours / 24;
    if (days < 30) {
        sprintf(buf, "%u days, %u hours", days, hours % 24);
        return buf;
    }
    uint32_t months = days / 30;
    if (months < 12) {
        sprintf(buf, "%u months, %u days", months, days % 30);
        return buf;
    }
    uint32_t years = months / 12;
    sprintf(buf, "Never");
    return buf;
}



char* capString(char* str) {
  bool capNext = true;
  for (size_t i = 0; str[i] != '\0'; i++) {
    if (str[i] >= 'a' && str[i] <= 'z') {
      if (capNext) {
        str[i] = str[i] - ('a' - 'A');
        capNext = false;
      }
    } else if (str[i] == ' ') {
      capNext = true;
    } else {
      capNext = false;
    }
  }
  return str;
}

acetime_t convertUnixEpochToAceTime(uint32_t ntpSeconds) {
    constexpr int32_t kDaysToConverterEpochFromNtpEpoch = 36524;
    int32_t daysToCurrentEpochFromNtpEpoch = Epoch::daysToCurrentEpochFromConverterEpoch();
    daysToCurrentEpochFromNtpEpoch += kDaysToConverterEpochFromNtpEpoch;
    uint32_t secondsToCurrentEpochFromNtpEpoch = 86400uL * daysToCurrentEpochFromNtpEpoch;
    uint32_t epochSeconds = ntpSeconds - secondsToCurrentEpochFromNtpEpoch;
    return epochSeconds;
}

acetime_t convert1970Epoch(acetime_t epoch1970) {
  acetime_t epoch2050 = epoch1970 - Epoch::secondsToCurrentEpochFromUnixEpoch64();
  return epoch2050;
}

 void setTimeSource(const String& inputString) {
    for (int i = 0; i < 4; i++) {
        if (i < inputString.length()) {
            timesource[i] = inputString.charAt(i);
        } else {
            timesource[i] = '\0';
        }
    }
}

acetime_t Now() 
{
  return systemClock.getNow();
}

void resetLastNtpCheck() 
{
  lastntpcheck = systemClock.getNow();
}

bool compareTimes(ZonedDateTime t1, ZonedDateTime t2) {
  // Get the hour, minute, and second components of the two DateTimes
  int hour1 = t1.hour();
  int minute1 = t1.minute();
  int second1 = t1.second();
  int hour2 = t2.hour();
  int minute2 = t2.minute();
  int second2 = t2.second();
  // Compare the hour, minute, and second components of the two DateTimes
  if (hour1 == hour2 && minute1 == minute2 && second1 == second2) {
    // The times are the same
    return true;
  } else {
    // The times are different
    return false;
  }
}

void toUpper(char* input) {
    int count = strlen(input);
    for (int i = 0; i < count; i++)
        input[i] = toupper(input[i]);
}

uint16_t calcbright (uint16_t bl) {
    uint16_t retVal = 0;
    switch(bl) {
        case 1:
            retVal = 0;
            break;
        case 2:
            retVal = 5;
            break;
        case 3:
            retVal = 10;
            break;
        case 4:
            retVal = 20;
            break;
        case 5:
            retVal = 30;
            break;
        default:
            retVal = 0;
            break;
    }
    return retVal;
}


const char* ordinal_suffix(int n)
{
  // Check if number is divisible by 10
  int div = n % 10;
  switch (div) 
  {
    case 1: return "st";
    case 2: return "nd";
    case 3: return "rd";
    default: return "th";
  }
}

void cleanString(char *str) 
{
  char* src = str;
  char* dst = str;
  while (*src) 
  {
    if (*src != '\n' && *src != '"' && *src != '[' && *src != ']') 
    {
      *dst++ = *src;
    }
    src++;
  }
  *dst = '\0';
}

void capitalize(char* str) 
{
  while (*str) 
  {
    *str = toupper(*str);
    str++;
  }
}

// Compares two strings for equality
bool cmpLocs(char a1[32], char a2[32]) 
{
  return memcmp((const void *)a1, (const void *)a2, sizeof(a1)) == 0;
}

String uv_index(uint8_t index)
{
  if (index <= 2)
    return "Low";
  else if (index <= 5)
    return "Moderate";
  else if (index <= 7)
    return "High";
  else if (index <= 10)
    return "Very High";
  else if (index > 10)
    return "Extreme";
}

int32_t fromNow(acetime_t ctime)
{
  return (systemClock.getNow() - ctime);
}

String formatLargeNumber(int number) {
    char str[20];
    String formattedNumber;
    int i = 0, j = 0;
    if (number < 0) {
        str[i++] = '-';
        number = -number;
    }
    do {
        str[i++] = number % 10 + '0';
        if (i % 4 == 3 && number / 10 != 0) {
            str[i++] = ',';
        }
        number /= 10;
    } while (number);
    str[i] = '\0';
    for (j = i - 1; j >= 0; j--) {
        formattedNumber += str[j];
    }
    return formattedNumber;
}

 //TODO: web interface cleanup
 //TODO: advanced aqi calulations
 //TODO: table titles
 //TODO: remove tables if show is disabled 
 //TODO: weather daily in web
 //TODO: timezone name in web
 //TODO: full/new moon scroll and status icon?
 //TODO: make very severe weather alerts (like tornado warnings) scroll continuosly
//FIXME: location change is adding scrolltext and color, needs to be only for changes


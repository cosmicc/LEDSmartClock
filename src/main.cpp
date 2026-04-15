// LedSmartClock for a 8x32 LED neopixel display
//
// Created by: Ian Perry (ianperry99@gmail.com)
// https://github.com/cosmicc/LEDSmartClock

// DO NOT USE DELAYS OR SLEEPS EVER! This breaks systemclock (Everything is coroutines now)

#include "main.h"
#include "bitmaps.h"
#include "config_backup.h"
#include "coroutines.h"

namespace
{
/** Formats the remaining time until the next scheduled display for debug output. */
String nextShowDebug(acetime_t now, acetime_t lastShown, uint32_t intervalSeconds)
{
  acetime_t nextDue = lastShown + static_cast<acetime_t>(intervalSeconds);
  if (nextDue <= now)
    return F("Ready now");

  return String(F("in ")) + elapsedTime(static_cast<uint32_t>(nextDue - now));
}

/** Returns a bounded random startup offset so hourly jobs do not align exactly. */
uint32_t getStartupShowStaggerSeconds(uint32_t intervalSeconds)
{
  if (intervalSeconds < T1M)
    return 0;

  uint32_t staggerCap = intervalSeconds / 4U;
  staggerCap = min(staggerCap, 15U * 60U);
  staggerCap = max(staggerCap, 30U);
  return esp_random() % (staggerCap + 1U);
}

/** Seeds a display schedule so the first run waits a full interval plus a small stagger. */
acetime_t seedLastShown(acetime_t bootTime, uint32_t intervalSeconds)
{
  return bootTime + static_cast<acetime_t>(getStartupShowStaggerSeconds(intervalSeconds));
}

/** Initializes the non-clock display schedules after boot. */
void seedDisplaySchedules()
{
  lastshown.date = seedLastShown(runtimeState.bootTime, static_cast<uint32_t>(date_interval.value()) * T1H);
  lastshown.currenttemp = seedLastShown(runtimeState.bootTime, static_cast<uint32_t>(current_temp_interval.value()) * T1M);
  lastshown.currentweather = seedLastShown(runtimeState.bootTime, static_cast<uint32_t>(current_weather_interval.value()) * T1H);
  lastshown.dayweather = seedLastShown(runtimeState.bootTime, static_cast<uint32_t>(daily_weather_interval.value()) * T1H);
  lastshown.aqi = seedLastShown(runtimeState.bootTime, static_cast<uint32_t>(aqi_interval.value()) * T1M);
}

/** Persists the live configuration into the key-based store and logs failures. */
bool persistConfigurationState(const char *reason)
{
  String error;
  if (!saveStoredConfiguration(error))
  {
    ESP_LOGE(TAG, "Failed to persist configuration after %s: %s", reason, error.c_str());
    return false;
  }

  ESP_LOGI(TAG, "Persisted configuration after %s.", reason);
  return true;
}

/** Persists the already-loaded legacy blob into the new key-based store. */
bool migrateLegacyConfigurationStore(const char *reason)
{
  if (persistConfigurationState(reason))
  {
    ESP_EARLY_LOGI(TAG, "Migrated legacy positional configuration into the key-based store.");
    return true;
  }

  ESP_EARLY_LOGE(TAG, "Failed to migrate legacy positional configuration into the key-based store.");
  return false;
}
} // namespace

extern "C" void app_main()
{
  Serial.begin(115200);
  uint32_t init_timer = millis();
  nvs_flash_init();
  ESP_EARLY_LOGD(TAG, "Initializing Hardware Watchdog...");
  esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);               // add current thread to WDT watch
  ESP_EARLY_LOGD(TAG, "Initializing the system clock...");
  Wire.begin();
  wireInterface.begin();
  systemClock.setup();
  setTimeSource(systemClock.isInit() ? F("rtc") : F("none"));
  dsClock.setup();
  gpsClock.setup();
  printSystemZonedTime();
  ESP_EARLY_LOGD(TAG, "Initializing IotWebConf...");
  bool legacyConfigLoaded = setupIotWebConf();
  if (hasStoredConfiguration())
  {
    String configStoreError;
    if (loadStoredConfiguration(configStoreError))
      ESP_EARLY_LOGI(TAG, "Loaded key-based configuration store.");
    else if (legacyConfigLoaded)
    {
      ESP_EARLY_LOGE(TAG, "Failed to load key-based configuration store: %s", configStoreError.c_str());
      ESP_EARLY_LOGW(TAG, "Falling back to the legacy positional configuration blob.");
      migrateLegacyConfigurationStore("recovery from invalid key-based store");
    }
    else
      ESP_EARLY_LOGE(TAG, "Failed to load key-based configuration store: %s", configStoreError.c_str());
  }
  else if (legacyConfigLoaded)
  {
    migrateLegacyConfigurationStore("legacy positional-config migration");
  }
  else
  {
    ESP_EARLY_LOGI(TAG, "No persisted configuration store found. Using defaults until first save.");
  }
  gpsClock.refreshNtpServer();
  ESP_EARLY_LOGD(TAG, "Initializing Light Sensor...");
  runtimeState.userBrightness = calcbright(brightness_level.value());
  current.brightness = runtimeState.userBrightness;
  Wire.begin(TSL2561_SDA, TSL2561_SCL);
  Tsl.begin();
  while (!Tsl.available())
  {
    systemClock.loop();
    ESP_EARLY_LOGD(TAG, "Waiting for Light Sensor...");
  }
  Tsl.on();
  Tsl.setSensitivity(true, Tsl2561::EXP_14);
  ESP_EARLY_LOGD(TAG, "Initializing GPS Module...");
  Serial1.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN); // Initialize GPS UART
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
  registerWebRoutes();
  cotimer.scrollspeed = map(text_scroll_speed.value(), 1, 10, 100, 10);
  ESP_EARLY_LOGD(TAG, "IotWebConf initilization is complete. Web server is ready.");
  ESP_EARLY_LOGD(TAG, "Setting class timestamps...");
  runtimeState.bootTime = systemClock.getNow();
  runtimeState.lastNtpCheck = systemClock.getNow() - 3601;
  cotimer.icontimer = millis(); // reset all delay timers
  gps.timestamp = runtimeState.bootTime - T1Y + 60;
  gps.lastcheck = runtimeState.bootTime - T1Y + 60;
  gps.lockage = runtimeState.bootTime - T1Y + 60;
  checkweather.lastsuccess = runtimeState.bootTime - T1Y + 60;
  checkalerts.lastsuccess = runtimeState.bootTime - T1Y + 60;
  checkipgeo.lastsuccess = runtimeState.bootTime - T1Y + 60;
  checkaqi.lastsuccess = runtimeState.bootTime - T1Y + 60;
  checkgeocode.lastsuccess = runtimeState.bootTime - T1Y + 60;
  checkalerts.lastattempt = runtimeState.bootTime - T1Y + 60;
  checkweather.lastattempt = runtimeState.bootTime - T1Y + 60;
  checkipgeo.lastattempt = runtimeState.bootTime - T1Y + 60;
  checkaqi.lastattempt = runtimeState.bootTime - T1Y + 60;
  checkgeocode.lastattempt = runtimeState.bootTime - T1Y + 60;
  lastshown.alerts = runtimeState.bootTime - T1Y + 60;
  seedDisplaySchedules();
  scrolltext.position = mw;
  if (enable_fixed_loc.isChecked())
    ESP_EARLY_LOGI(TAG, "Setting Fixed GPS Location Lat: %s Lon: %s", fixedLat.value(), fixedLon.value());
  updateCoords();
  updateLocation();
  showclock.fstop = 250;
  if (flickerfast.isChecked())
    showclock.fstop = 20;
  ESP_EARLY_LOGD(TAG, "Initializing the display...");
  FastLED.addLeds<NEOPIXEL, HSPI_MOSI>(leds, NUMMATRIX).setCorrection(TypicalLEDStrip);
  matrix->begin();
  matrix->setBrightness(runtimeState.userBrightness);
  matrix->setTextWrap(false);
  ESP_EARLY_LOGD(TAG, "Display initalization complete.");
  ESP_EARLY_LOGD(TAG, "Setup initialization complete: %d ms", (millis() - init_timer));
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
  persistConfigurationState("config save");
  applyRuntimeConfiguration();
}

void applyRuntimeConfiguration()
{
  updateCoords();
  processTimezone();
  gpsClock.refreshNtpServer();
  rebuildApiUrls();
  showclock.fstop = 250;
  if (flickerfast.isChecked())
    showclock.fstop = 20;
  cotimer.scrollspeed = map(text_scroll_speed.value(), 1, 10, 100, 10);
  runtimeState.userBrightness = calcbright(brightness_level.value());
  ESP_LOGI(TAG, "Configuration was updated.");
  if (resetdefaults.isChecked())
    showready.reset = true;
  if (!serialdebug.isChecked())
    printf("Serial debug info has been disabled.\n");
  runtimeState.firstTimeFailsafe = false;
}

// This function is called when the WiFi module is connected to the network.
void wifiConnected()
{
  ESP_LOGI(TAG, "WiFi is now connected.");
  gpsClock.refreshNtpServer();
  gpsClock.ntpReady();
  display_showStatus();
  matrix->show();
  systemClock.forceSync();
  showready.ip = true;
}

bool connectAp(const char *apName, const char *password)
{
  return WiFi.softAP(apName, password, 4);
}

void connectWifi(const char *ssid, const char *password)
{
  gpsClock.prepareServerSelection();
  WiFi.begin(ssid, password);
}

// Regular Functions
bool isNextShowReady(acetime_t lastshown, uint32_t interval, uint32_t multiplier)
{
  // Calculate current time in seconds
  acetime_t now = systemClock.getNow();
  // Check if the elapsed time since the last show has reached the configured interval.
  bool showReady = (now - lastshown) >= static_cast<acetime_t>(interval * multiplier);
  return showReady;
}

bool isNextAttemptReady(acetime_t lastattempt)
{
  // wait 1 minute between attempts
  return (systemClock.getNow() - lastattempt) > T1M;
}

// Returns true if the current coordinates are valid
bool isCoordsValid()
{
  return !(current.lat == 0.0 && current.lon == 0.0);
}

bool isLocationValid(String source)
{
  auto hasLocationText = [](const char *value) {
    return value[0] != '\0' && !(value[0] == '0' && value[1] == '\0');
  };

  bool result = false;
  if (source == "geocode")
  {
    if (hasLocationText(geocode.city))
      result = true;
  }
  else if (source == "current")
  {
    if (hasLocationText(current.city))
      result = true;
  }
  else if (source == "saved")
  {
    if (hasLocationText(savedcity.value()))
      result = true;
  }
  return result;
}

void print_debugData()
{
  acetime_t now = systemClock.getNow();
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
  printf("Boot Time: %s\n", getCustomZonedTimestamp(runtimeState.bootTime).c_str());
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
  printf("Firmware - Version:v%s\n", VERSION_SEMVER);
  printf("System - RawLux:%d | Lux:%d | UsrBright:+%d | Brightness:%d | ClockHue:%d | TempHue:%d | WifiState:%s | HttpReady:%s | IP:%s | Uptime:%s\n", current.rawlux, current.lux, runtimeState.userBrightness, current.brightness, current.clockhue, current.temphue, (connection_state[iotWebConf.getState()]), (yesno[isHttpReady()]), ((WiFi.localIP()).toString()).c_str(), elapsedTime(now - runtimeState.bootTime).c_str());
  printf("Clock - Status:%s | TimeSource:%s | CurrentTZ:%d | NtpReady:%s | NtpServer:%s(%s) | Skew:%d Seconds | LastAttempt:%s | NextAttempt:%s | NextNtp:%s | LastSync:%s\n", clock_status[systemClock.getSyncStatusCode()], runtimeState.timeSource, current.tzoffset, yesno[gpsClock.ntpIsReady], runtimeState.ntpServer, runtimeState.ntpServerSource, systemClock.getClockSkew(), elapsedTime(systemClock.getSecondsSinceSyncAttempt()).c_str(), elapsedTime(systemClock.getSecondsToSyncAttempt()).c_str(), elapsedTime((now - runtimeState.lastNtpCheck) - NTPCHECKTIME * 60).c_str(), elapsedTime(now - systemClock.getLastSyncTime()).c_str());
  printf("Loc - SavedLat:%.5f | SavedLon:%.5f | CurrentLat:%.5f | CurrentLon:%.5f | LocValid:%s\n", atof(savedlat.value()), atof(savedlon.value()), current.lat, current.lon, yesno[isCoordsValid()]);
  printf("IPGeo - Complete:%s | Lat:%.5f | Lon:%.5f | TZoffset:%d | Timezone:%s | ValidApi:%s | Retries:%d | LastAttempt:%s | LastSuccess:%s\n", yesno[checkipgeo.complete], ipgeo.lat, ipgeo.lon, ipgeo.tzoffset, ipgeo.timezone, yesno[isApiValid(ipgeoapi.value())], checkipgeo.retries, elapsedTime(now - checkipgeo.lastattempt).c_str(), elapsedTime(now - checkipgeo.lastsuccess).c_str());
  printf("GPS - Chars:%s | With-Fix:%s | Failed:%s | Passed:%s | Sats:%d | Hdop:%d | Elev:%d | Lat:%.5f | Lon:%.5f | FixAge:%s | LocAge:%s\n", formatLargeNumber(GPS.charsProcessed()).c_str(), formatLargeNumber(GPS.sentencesWithFix()).c_str(), formatLargeNumber(GPS.failedChecksum()).c_str(), formatLargeNumber(GPS.passedChecksum()).c_str(), gps.sats, gps.hdop, gps.elevation, gps.lat, gps.lon, elapsedTime(now - gps.timestamp).c_str(), elapsedTime(now - gps.lockage).c_str());
  printf("Weather Current - Icon:%s | Temp:%d%s | FeelsLike:%d%s | Humidity:%d%% | Clouds:%d%% | Wind:%d/%d%s | UVI:%d(%s) | Desc:%s | ValidApi:%s | Retries:%d/%d | LastAttempt:%s | LastSuccess:%s | NextShow:%s\n", weather.current.icon, weather.current.temp, tempunit, weather.current.feelsLike, tempunit, weather.current.humidity, weather.current.cloudcover, weather.current.windSpeed, weather.current.windGust, speedunit, weather.current.uvi, uv_index(weather.current.uvi).c_str(), weather.current.description, yesno[isApiValid(weatherapi.value())], checkweather.retries, HTTP_MAX_RETRIES, elapsedTime(now - checkweather.lastattempt).c_str(), elapsedTime(now - checkweather.lastsuccess).c_str(), nextShowDebug(now, lastshown.currentweather, static_cast<uint32_t>(current_weather_interval.value()) * T1H).c_str());
  printf("Weather Day - Icon:%s | LoTemp:%d%s | HiTemp:%d%s | Humidity:%d%% | Clouds:%d%% | Wind: %d/%d%s | UVI:%d(%s) | MoonPhase:%.2f | Desc: %s | NextShow:%s\n", weather.day.icon, weather.day.tempMin, tempunit, weather.day.tempMax, tempunit, weather.day.humidity, weather.day.cloudcover, weather.day.windSpeed, weather.day.windGust, speedunit, weather.day.uvi, uv_index(weather.day.uvi).c_str(), weather.day.moonPhase, weather.current.description, nextShowDebug(now, lastshown.dayweather, static_cast<uint32_t>(daily_weather_interval.value()) * T1H).c_str());
  printf("AQI Current - Index:%s | Co:%.2f | No:%.2f | No2:%.2f | Ozone:%.2f | So2:%.2f | Pm2.5:%.2f | Pm10:%.2f | Ammonia:%.2f | Desc: %s | Retries:%d/%d | LastAttempt:%s | LastSuccess:%s | NextShow:%s\n", air_quality[aqi.current.aqi], aqi.current.co, aqi.current.no, aqi.current.no2, aqi.current.o3, aqi.current.so2, aqi.current.pm25, aqi.current.pm10, aqi.current.nh3, (aqi.current.description).c_str(), checkaqi.retries, HTTP_MAX_RETRIES, elapsedTime(now - checkaqi.lastattempt).c_str(), elapsedTime(now - checkaqi.lastsuccess).c_str(), nextShowDebug(now, lastshown.aqi, static_cast<uint32_t>(aqi_interval.value()) * T1M).c_str());
  printf("AQI Day - Index:%s | Co:%.2f | No:%.2f | No2:%.2f | Ozone:%.2f | So2:%.2f | Pm2.5:%.2f | Pm10:%.2f | Ammonia:%.2f\n", air_quality[aqi.day.aqi], aqi.day.co, aqi.day.no, aqi.day.no2, aqi.day.o3, aqi.day.so2, aqi.day.pm25, aqi.day.pm10, aqi.day.nh3);
  printf("Alerts - Active:%s | Watch:%s | Warn:%s | Retries:%d | LastAttempt:%s | LastSuccess:%s | LastShown:%s\n", yesno[alerts.active], yesno[alerts.inWatch], yesno[alerts.inWarning], checkalerts.retries, elapsedTime(now - checkalerts.lastattempt).c_str(), elapsedTime(now - checkalerts.lastsuccess).c_str(), elapsedTime(now - lastshown.alerts).c_str());
  printf("[^----------------------------------------------------------------^]\n");
  printf("Main task stack size: %s bytes remaining\n", formatLargeNumber(uxTaskGetStackHighWaterMark(NULL)).c_str());
  printf("Current Display Tokens: %s\n", displaytoken.showTokens().c_str());
  printf("[^----------------------------------------------------------------^]\n");
  if (alerts.active)
    printf("*Alert - Status: %s | Severity: %s | Certainty: %s | Urgency: %s | Event: %s | Desc: %s %s\n", alerts.status1, alerts.severity1, alerts.certainty1, alerts.urgency1, alerts.event1, alerts.description1, alerts.instruction1);
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
  if (!hasVisibleText(scrolltext.message))
  {
    scrolltext.active = false;
    scrolltext.displayicon = false;
    scrolltext.position = mw - 1;
    scrolltext.size = 0;
    ESP_LOGW(TAG, "startScroll(): refusing to start an empty scroll message");
    return;
  }

  scrolltext.color = color;
  scrolltext.active = true;
  scrolltext.displayicon = displayicon;
  showClock.suspend();
}

// Process timezone to set current timezone offsets by taking into account user setting or ipgeolocation data
void processTimezone()
{
  uint32_t timer = millis();
  int16_t savedoffset = savedtzoffset.value();
  int16_t fixedoffset = fixed_offset.value();

  auto applyTimezoneOffset = [&](int16_t offset, const char *source) {
    current.tzoffset = offset;
    current.timezone = TimeZone::forHours(offset);
    ESP_LOGD(TAG, "Using %s timezone offset: %d", source, offset);
  };

  auto persistTimezoneOffset = [&](int16_t offset, const char *source) {
    if (offset == savedoffset)
      return;

    ESP_LOGI(TAG, "%s timezone [%d] is different than saved timezone [%d], saving new timezone", source, offset, savedoffset);
    savedtzoffset.value() = offset;
    savedoffset = offset;
    persistConfigurationState("timezone update");
  };

  if (enable_fixed_tz.isChecked())
  {
    applyTimezoneOffset(fixedoffset, "fixed");
    persistTimezoneOffset(fixedoffset, "Fixed");
  }
  else if (ipgeo.tzoffset != 127)
  {
    applyTimezoneOffset(ipgeo.tzoffset, "ip geolocation");
    persistTimezoneOffset(ipgeo.tzoffset, "IP Geo");
  }
  else if (savedoffset != 0 || fixedoffset == 0)
  {
    applyTimezoneOffset(savedoffset, "saved");
  }
  else
  {
    applyTimezoneOffset(fixedoffset, "configured fallback");
    persistTimezoneOffset(fixedoffset, "Configured fallback");
  }
  ESP_LOGV(TAG, "Process timezone complete: %lu ms", (millis() - timer));
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
  if ((String)savedcity.value() != current.city || (String)savedstate.value() != current.state || (String)savedcountry.value() != current.country)
  {
    ESP_LOGI(TAG, "Location not saved, saving new location: %s, %s, %s", current.city, current.state, current.country);
    // Convert city/state/country to char array and save to savedcity/state/country
    strcpy(savedcity.value(), current.city);
    strcpy(savedstate.value(), current.state);
    strcpy(savedcountry.value(), current.country);
    persistConfigurationState("location update");
  }
}

void updateCoords()
{
  auto coordsUnset = [](double lat, double lon) {
    return lat == 0.0 && lon == 0.0;
  };

  if (enable_fixed_loc.isChecked())
  {
    gps.lat = strtod(fixedLat.value(), NULL);
    gps.lon = strtod(fixedLon.value(), NULL);
    current.lat = strtod(fixedLat.value(), NULL);
    current.lon = strtod(fixedLon.value(), NULL);
    strcpy(current.locsource, "User Defined");
  }
  else if (coordsUnset(gps.lat, gps.lon) && coordsUnset(ipgeo.lat, ipgeo.lon) && coordsUnset(current.lat, current.lon))
  {
    // Set coordinates to saved location
    current.lat = strtod(savedlat.value(), NULL);
    current.lon = strtod(savedlon.value(), NULL);
    strcpy(current.locsource, "Previous Saved");
  }
  else if (coordsUnset(gps.lat, gps.lon) && !coordsUnset(ipgeo.lat, ipgeo.lon) && coordsUnset(current.lat, current.lon))
  {
    // Set coordinates to ip geolocation
    current.lat = ipgeo.lat;
    current.lon = ipgeo.lon;
    strcpy(current.locsource, "IP Geolocation");
    ESP_LOGI(TAG, "Using IPGeo location information: Lat: %lf Lon: %lf", (ipgeo.lat), (ipgeo.lon));
  }
  else if (!coordsUnset(gps.lat, gps.lon))
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
    persistConfigurationState("coordinate update");
    checkgeocode.ready = true;
  }
  else if (isCoordsValid() && !isLocationValid("geocode") && !isLocationValid("current"))
  {
    checkgeocode.ready = true;
  }
  // Rebuild API endpoints after coordinates change.
  rebuildApiUrls();
}

void display_setclockDigit(uint8_t bmp_num, uint8_t position, uint16_t color)
{
  // Each digit uses a fixed starting column on the 32x8 matrix.
  uint8_t pos = 0;
  switch (position)
  {
  case 0:
    pos = 0;
    break; // set to 0 when position is 0
  case 1:
    pos = 7;
    break; // set to 7 when position is 1
  case 2:
    pos = 16;
    break; // set to 16 when position is 2
  case 3:
    pos = 23;
    break; // set to 23 when position is 3
  default:
    return;
  }
  // draw the bitmap using column and color parameters
  matrix->drawBitmap((runtimeState.clockDisplayOffset) ? (pos - 4) : pos, 0, num[bmp_num], 8, 8, color);
}

void display_showStatus()
{
  uint16_t sclr = BLACK;
  uint16_t wclr = BLACK;
  uint16_t aclr = BLACK;
  uint16_t uclr = BLACK;
  uint16_t gclr = BLACK;
  if (green_status.isChecked())
    gclr = DARKGREEN;
  else
    gclr = BLACK;
  acetime_t now = systemClock.getNow();
  // Set the status color
  if (now - systemClock.getLastSyncTime() > (NTPCHECKTIME * 60) + 60)
    sclr = DARKRED;
  else if (runtimeState.timeSource[0] == 'g' && gps.fix && (now - systemClock.getLastSyncTime()) <= 600)
    sclr = BLACK;
  else if (runtimeState.timeSource[0] == 'n' && iotWebConf.getState() == 4)
    sclr = DARKGREEN;
  else if (iotWebConf.getState() <= 2) // boot, apmode, notconfigured
    sclr = DARKMAGENTA;
  else if (iotWebConf.getState() == 3 || iotWebConf.getState() == 4) // connecting, online
    sclr = DARKCYAN;
  else
    sclr = DARKRED;
  if (enable_system_status.isChecked() || sclr == DARKRED)
    matrix->drawPixel(0, 7, sclr);
  // Check the AQI and wether an alert is present
  if (enable_aqi_status.isChecked() && checkaqi.complete)
  {
    switch (aqi.current.aqi)
    {
    case 2:
      aclr = DARKYELLOW;
      break;
    case 3:
      aclr = DARKORANGE;
      break;
    case 4:
      aclr = DARKRED;
      break;
    case 5:
      aclr = DARKPURPLE;
      break;
    default:
      aclr = gclr;
      break;
    }
    matrix->drawPixel(0, 0, aclr);
  }
  if (enable_uvi_status.isChecked() && checkweather.complete)
  {
    switch (weather.current.uvi)
    {
    case 0:
      uclr = gclr;
      break;
    case 1:
      uclr = gclr;
      break;
    case 2:
      uclr = DARKYELLOW;
      break;
    case 3:
      uclr = DARKORANGE;
      break;
    case 4:
      uclr = DARKRED;
      break;
    case 5:
      uclr = DARKPURPLE;
      break;
    default:
      uclr = gclr;
      break;
    }
    matrix->drawPixel(mw - 1, 0, uclr);
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

void display_weatherIcon(char *icon)
{
  bool night;
  char dn = icon[2];
  night = (dn == 'n' ? true : false);
  char code1 = icon[0];
  char code2 = icon[1];
  // Clear
  if (code1 == '0' && code2 == '1')
  {
    matrix->drawRGBBitmap(mw - 8, 0, (night ? clear_night[cotimer.iconcycle] : clear_day[cotimer.iconcycle]), 8, 8);
  }
  // Partly cloudy
  else if ((code1 == '0' && code2 == '2') || (code1 == '0' && code2 == '3'))
  {
    matrix->drawRGBBitmap(mw - 8, 0, (night ? partly_cloudy_night[cotimer.iconcycle] : partly_cloudy_day[cotimer.iconcycle]), 8, 8);
  }
  // Cloudy
  else if ((code1 == '0') && (code2 == '4'))
  {
    matrix->drawRGBBitmap(mw - 8, 0, cloudy[cotimer.iconcycle], 8, 8);
  }
  // Fog/mist
  else if (code1 == '5' && code2 == '0')
  {
    matrix->drawRGBBitmap(mw - 8, 0, fog[cotimer.iconcycle], 8, 8);
  }
  // Snow
  else if (code1 == '1' && code2 == '3')
  {
    matrix->drawRGBBitmap(mw - 8, 0, snow[cotimer.iconcycle], 8, 8);
  }
  // Rain
  else if (code1 == '0' && code2 == '9')
  {
    matrix->drawRGBBitmap(mw - 8, 0, rain[cotimer.iconcycle], 8, 8);
  }
  // Heavy rain
  else if (code1 == '1' && code2 == '0')
  {
    matrix->drawRGBBitmap(mw - 8, 0, heavy_rain[cotimer.iconcycle], 8, 8);
  }
  // Thunderstorm
  else if (code1 == '1' && code2 == '1')
  {
    matrix->drawRGBBitmap(mw - 8, 0, thunderstorm[cotimer.iconcycle], 8, 8);
  }
  else
  {
    ESP_LOGE(TAG, "No Weather icon found to use");
    return;
  }
  // Animate
  if (millis() - cotimer.icontimer > ANI_SPEED)
  {
    cotimer.iconcycle = (cotimer.iconcycle == ANI_BITMAP_CYCLES - 1) ? 0 : cotimer.iconcycle + 1;
    cotimer.icontimer = millis();
  }
}

void display_temperature()
{
  int temp = weather.current.feelsLike;
  bool imperial_setting = imperial.isChecked();
  int tl = (imperial_setting) ? 32 : 0;
  int th = (imperial_setting) ? 104 : 40;
  uint32_t tc;
  if (enable_temp_color.isChecked())
    tc = hex2rgb(temp_color.value());
  else
    current.temphue = constrain(map(weather.current.feelsLike, tl, th, NIGHTHUE, 0), 0, NIGHTHUE);

  if (weather.current.feelsLike < tl)
  {
    if (imperial_setting && weather.current.feelsLike + abs(weather.current.feelsLike) == 0)
      current.temphue = NIGHTHUE + tl;
    else
      current.temphue = NIGHTHUE + (abs(weather.current.feelsLike) / 2);
  }
  tc = hsv2rgb(current.temphue);
  int xpos = 0;
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
  if (score == 0)
  {
    matrix->drawBitmap(xpos, 0, num[0], 8, 8, tc);
  }
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
  matrix->drawBitmap(xpos + (digits * 7) + 7, 0, sym[0], 8, 8, tc);
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

String getSystemZonedDateTimeString()
{
  ZonedDateTime ldt = getSystemZonedTime();
  char buf[100];

  if (twelve_clock.isChecked())
  {
    const char *ap = ldt.hour() > 11 ? "PM" : "AM";
    uint8_t hour = ldt.hour() % 12;
    if (hour == 0)
      hour = 12;

    sprintf(buf, "%d:%02d%s %s, %s %u%s %04d", hour, ldt.minute(), ap, DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()),
            ldt.day(), ordinal_suffix(ldt.day()), ldt.year());
  }
  else
  {
    sprintf(buf, "%02d:%02d %s, %s %d%s %04d", ldt.hour(), ldt.minute(), DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()),
            ldt.day(), ordinal_suffix(ldt.day()), ldt.year());
  }

  return String(buf);
}

uint16_t calcaqi(double c, double i_hi, double i_low, double c_hi, double c_low)
{
  return (i_hi - i_low) / (c_hi - c_low) * (c - c_low) + i_low;
}

// TODO: web interface cleanup
// TODO: advanced aqi calulations
// TODO: table titles
// TODO: remove tables if show is disabled
// TODO: weather daily in web
// TODO: timezone name in web
// TODO: full/new moon scroll and status icon?
// TODO: make very severe weather alerts (like tornado warnings) scroll continuosly
// FIXME: location change is adding scrolltext and color, needs to be only for changes

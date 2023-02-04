// Coroutines
#ifdef COROUTINE_STATES
COROUTINE(printStates) {
  COROUTINE_LOOP() {
    CoroutineScheduler::list(Serial);
    COROUTINE_DELAY(PROFILER_DELAY * 1000);
  }
}
#endif

#ifdef COROUTINE_PROFILER
COROUTINE(printProfiling) {
  COROUTINE_LOOP() {
    LogBinTableRenderer::printTo(Serial, 2 /*startBin*/, 13 /*endBin*/, false /*clear*/);
    COROUTINE_DELAY(PROFILER_DELAY * 1000);
  }
}
#endif

COROUTINE(sysClock) {
  COROUTINE_LOOP()
  {
    systemClock.loop();
    COROUTINE_YIELD();
  }
}

COROUTINE(showClock) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(displaytoken.isReady(0));
    ace_time::ZonedDateTime ldt = getSystemZonedTime();
    uint8_t myhours = ldt.hour();
    uint8_t myminute = ldt.minute();
    uint8_t mysecs = ldt.second();
    showclock.seconds = mysecs;
    uint8_t myhour;
    if (twelve_clock.isChecked())
      myhour = myhours % 12;
    else
      myhour = myhours;
    if (myhours * 60 >= 1320)
      current.clockhue = NIGHTHUE;
    else if (myhours * 60 < 360)
      current.clockhue = NIGHTHUE;
    else
      current.clockhue = constrain(map(myhours * 60 + myminute, 360, 1319, DAYHUE, NIGHTHUE), DAYHUE, NIGHTHUE);
    if (use_fixed_clockcolor.isChecked())
      showclock.color = hex2rgb(fixed_clockcolor.value());
    else
      showclock.color = hsv2rgb(current.clockhue);
    showclock.millis = millis();
    matrix->clear();
    if (myhour / 10 == 0 && myhour % 10 == 0)
    {
      display_setclockDigit(1, 0, showclock.color);
      display_setclockDigit(2, 1, showclock.color);
      clock_display_offset = false;
    }
    else
    {
      if (myhour / 10 != 0)
      {
        clock_display_offset = false;
        display_setclockDigit(myhour / 10, 0, showclock.color); // set first digit of hour
      }
      else
        clock_display_offset = true;
      display_setclockDigit(myhour % 10, 1, showclock.color); // set second digit of hour
    }
      display_setclockDigit(myminute / 10, 2, showclock.color); // set first digig of minute
      display_setclockDigit(myminute % 10, 3, showclock.color); // set second digit of minute
      if (showclock.colonflicker)
      {
        if (clock_display_offset)
        {
          matrix->drawPixel(12, 5, showclock.color); // draw colon
          matrix->drawPixel(12, 2, showclock.color); // draw colon
        }
        else
        {
          matrix->drawPixel(16, 5, showclock.color); // draw colon
          matrix->drawPixel(16, 2, showclock.color); // draw colon
        }
      display_showStatus();
      matrix->show();
      }
      else
      {
        display_showStatus();
        matrix->show();
        showclock.colonoff = true;
        COROUTINE_AWAIT(millis() - showclock.millis > showclock.fstop);
        if (clock_display_offset)
        {
          matrix->drawPixel(12, 5, showclock.color); // draw colon
          matrix->drawPixel(12, 2, showclock.color); // draw colon
        }
        else
        {
          matrix->drawPixel(16, 5, showclock.color); // draw colon
          matrix->drawPixel(16, 2, showclock.color); // draw colon
        }
      display_showStatus();
      matrix->show();
      }
    COROUTINE_AWAIT(showclock.seconds != getSystemZonedTime().second());
  }
}

COROUTINE(checkAirquality) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(abs(systemClock.getNow() - checkaqi.lastsuccess) > (airquality_interval.value() * T1M) && abs(systemClock.getNow() - checkaqi.lastattempt) > T1M && iotWebConf.getState() == 4 && (current.lat).length() > 1 && (weatherapi.value())[0] != '\0' && displaytoken.isReady(0) && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking air quality conditions...");
    checkaqi.active = true;
    checkaqi.retries = 1;
    checkaqi.jsonParsed = false;
    while (!checkaqi.jsonParsed && (checkaqi.retries <= 2))
    {
      COROUTINE_DELAY(1000);
      ESP_LOGD(TAG, "Connecting to %s", qurl);
      HTTPClient http;
      http.begin(qurl);
      int httpCode = http.GET();
      ESP_LOGD(TAG, "HTTP code : %d", httpCode);
      if (httpCode == 200)
      {
        aqiJson = JSON.parse(http.getString());
        if (JSON.typeof(aqiJson) == "undefined")
          ESP_LOGE(TAG, "Parsing aqiJson input failed!");
        else
          checkaqi.jsonParsed = true;
      } 
      else if (httpCode == 401) {
        alertflash.color = RED;
        alertflash.laps = 1;
        alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = "Invalid Openweathermap.org API Key";
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;
        COROUTINE_AWAIT(!scrolltext.active);
      } else {
        alertflash.color = RED;
        alertflash.laps = 1;
        alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = (String)"Error getting air quality conditions: " +httpCode;
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;       
        COROUTINE_AWAIT(!scrolltext.active);
      }
      http.end();
      checkaqi.retries++;
    }
    if (!checkaqi.jsonParsed)
      ESP_LOGE(TAG, "Error: JSON");
    else
    {
      fillAqiFromJson(&weather);
      checkaqi.lastsuccess = systemClock.getNow();
    }
    checkaqi.lastattempt = systemClock.getNow();
    checkaqi.active = false;
  }
}

COROUTINE(checkWeather) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(abs(systemClock.getNow() - checkweather.lastsuccess) > (show_weather_interval.value() * T1M) && abs(systemClock.getNow() - checkweather.lastattempt) > T1M && iotWebConf.getState() == 4 && (current.lat).length() > 1 && (weatherapi.value())[0] != '\0' && displaytoken.isReady(0) && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking weather forecast...");
    checkweather.retries = 1;
    checkweather.jsonParsed = false;
    checkweather.active = true;
    while (!checkweather.jsonParsed && (checkweather.retries <= 2))
    {
      COROUTINE_DELAY(1000);
      ESP_LOGD(TAG, "Connecting to %s", wurl);
      HTTPClient http;
      http.begin(wurl);
      int httpCode = http.GET();
      ESP_LOGD(TAG, "HTTP code : %d", httpCode);
      if (httpCode == 200)
      {
        weatherJson = JSON.parse(http.getString());
        if (JSON.typeof(weatherJson) == "undefined")
          ESP_LOGE(TAG, "Parsing weatherJson input failed!");
        else
          checkweather.jsonParsed = true;
      } 
      else if (httpCode == 401) {
        alertflash.color = RED;
        alertflash.laps = 1;
        alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = "Invalid Openweathermap.org API Key";
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;
        COROUTINE_AWAIT(!scrolltext.active);
      } else {
        alertflash.color = RED;
        alertflash.laps = 1;
        alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = (String)"Error getting weather forcast: " +httpCode;
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;       
        COROUTINE_AWAIT(!scrolltext.active);
      }
      http.end();
      checkweather.retries++;
    }
    if (!checkweather.jsonParsed)
      ESP_LOGE(TAG, "Error: JSON");
    else
    {
      fillWeatherFromJson(&weather);
      checkweather.lastsuccess = systemClock.getNow();
    }
    checkweather.lastattempt = systemClock.getNow();
    checkweather.active = false;
  }
}


#ifndef DISABLE_ALERTCHECK
COROUTINE(checkAlerts) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(!httpbusy && abs(systemClock.getNow() - checkalerts.lastsuccess) > (5 * T1M) && (systemClock.getNow() - checkalerts.lastattempt) > T1M && iotWebConf.getState() == 4 && (current.lat).length() > 1 && displaytoken.isReady(0));
    ESP_LOGI(TAG, "Checking weather alerts...");
    httpbusy = true;
    checkalerts.active;
    checkalerts.retries = 1;
    checkalerts.jsonParsed = false;
    while (!checkalerts.jsonParsed && (checkalerts.retries <= 2))
    {
      COROUTINE_DELAY(1000);
      ESP_LOGD(TAG, "Connecting to %s", aurl);
      HTTPClient http;
      http.begin(aurl);
      int httpCode = http.GET();
      ESP_LOGD(TAG, "HTTP code : %d", httpCode);
      if (httpCode == 200)
      {
        alertsJson = JSON.parse(http.getString());
        if (JSON.typeof(alertsJson) == "undefined")
          ESP_LOGE(TAG, "Parsing alertJson input failed!");
        else
          checkalerts.jsonParsed = true;
      } else 
      {
          alertflash.color = RED;
          alertflash.laps = 1;
          alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = (String) "Error getting weather alerts: " + httpCode;
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;
        COROUTINE_AWAIT(!scrolltext.active);
      }
      http.end();
      checkalerts.retries++;
    }
    if (!checkalerts.jsonParsed)
      ESP_LOGE(TAG, "Error: JSON");
    else
    {
      fillAlertsFromJson(&alerts);
      checkalerts.lastsuccess = systemClock.getNow();
    }
    checkalerts.lastattempt = systemClock.getNow();
    checkipgeo.active = false;
    httpbusy = false;
  }
}
#endif

COROUTINE(showDate) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(cotimer.show_date_ready && show_date.isChecked() && iotWebConf.getState() != 1 && displaytoken.isReady(1));
  displaytoken.setToken(1);
  scrolltext.message = getSystemZonedDateString();
  scrolltext.color = hex2rgb(datecolor.value());
  scrolltext.active = true;
  COROUTINE_AWAIT(!scrolltext.active);
  showclock.datelastshown = systemClock.getNow();
  cotimer.show_date_ready = false;
  displaytoken.resetToken(1);
  }
}

COROUTINE(showWeather) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(cotimer.show_weather_ready && show_weather.isChecked() && displaytoken.isReady(2));
  displaytoken.setToken(2);
  alertflash.color = hex2rgb(weather_color.value());
  alertflash.laps = 1;
  alertflash.active = true;
  COROUTINE_AWAIT(!alertflash.active);
  scrolltext.message = capString((String)"Currently: " + weather.currentDescription + " Humidity " + weather.currentHumidity + "% Wind " + int(weather.currentWindSpeed) + "/" + int(weather.currentWindGust) + " Air: " + air_quality[weather.currentaqi]);
  scrolltext.color = hex2rgb(weather_color.value());
  scrolltext.active = true;
  scrolltext.displayicon = true;
  strncpy(scrolltext.icon, weather.currentIcon, 4);
  COROUTINE_AWAIT(!scrolltext.active);
  scrolltext.displayicon = false;
  cotimer.millis = millis();
  while (millis() - cotimer.millis < 10000) {
    matrix->clear();
    display_temperature();
    display_showStatus();
    display_weatherIcon(weather.currentIcon);
    matrix->show();
    COROUTINE_DELAY(50);
  }
  weather.lastshown = systemClock.getNow();
  cotimer.show_weather_ready = false;
  displaytoken.resetToken(2);
  }
}

COROUTINE(showWeatherDaily) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(cotimer.show_weather_daily_ready && show_weather_daily.isChecked() && displaytoken.isReady(8));
  displaytoken.setToken(8);
  alertflash.color = hex2rgb(weather_daily_color.value());
  alertflash.laps = 1;
  alertflash.active = true;
  strncpy(scrolltext.icon, weather.dayIcon, 4);
  COROUTINE_AWAIT(!alertflash.active);
  scrolltext.message = capString((String)"Today: " + weather.dayDescription + " Humidity " + weather.dayHumidity + "% Wind " + int(weather.dayWindSpeed) + "/" + int(weather.dayWindGust));
  scrolltext.color = hex2rgb(weather_daily_color.value());
  scrolltext.active = true;
  scrolltext.displayicon = true;
  COROUTINE_AWAIT(!scrolltext.active);
  scrolltext.displayicon = false;
  cotimer.millis = millis();
  weather.lastdailyshown = systemClock.getNow();
  cotimer.show_weather_daily_ready = false;
  displaytoken.resetToken(8);
  }
}

COROUTINE(showAirquality) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(cotimer.show_airquality_ready && show_airquality.isChecked() && displaytoken.isReady(9));
  displaytoken.setToken(9);
  uint16_t color;
  if (use_fixed_aqicolor.isChecked())
    color = hex2rgb(airquality_color.value());
  else {
    if (weather.currentaqi == 1)
        color = GREEN;
    else if (weather.currentaqi == 2)
        color = YELLOW;
    else if (weather.currentaqi == 3)
        color = ORANGE;
    else if (weather.currentaqi == 4)
        color = RED;
    else if (weather.currentaqi == 5)
        color = PURPLE;
    else
        color = WHITE;
  }
  alertflash.color = color;
  scrolltext.color = color;
  alertflash.laps = 1;
  alertflash.active = true;
  COROUTINE_AWAIT(!alertflash.active);
  scrolltext.message = (String) "Air Quality: " + air_quality[weather.currentaqi];
  scrolltext.displayicon = false;
  scrolltext.active = true;
  COROUTINE_AWAIT(!scrolltext.active);
  cotimer.millis = millis();
  weather.lastaqishown = systemClock.getNow();
  cotimer.show_airquality_ready = false;
  displaytoken.resetToken(9);
  }
}

COROUTINE(showAlerts) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(cotimer.show_alert_ready && displaytoken.isReady(3));
  displaytoken.setToken(3);
  uint16_t acolor;
  if (alerts.inWarning)
    acolor = RED;
  else if (alerts.inWatch)
    acolor = YELLOW;
  alertflash.color = RED;
  alertflash.laps = 3;
  alertflash.active = true;
  COROUTINE_AWAIT(!alertflash.active);
  scrolltext.message = (String)alerts.description1;
  scrolltext.color = RED;
  scrolltext.active = true;
  scrolltext.displayicon = false;
  COROUTINE_AWAIT(!scrolltext.active);
  alerts.lastshown = systemClock.getNow();
  cotimer.show_alert_ready = false;
  displaytoken.resetToken(3);
  }
}

COROUTINE(print_debugData) {
  COROUTINE_LOOP() {
    COROUTINE_DELAY_SECONDS(15);
    COROUTINE_AWAIT(serialdebug.isChecked());
    debug_print("----------------------------------------------------------------", true);
    printSystemZonedTime();
    acetime_t now = systemClock.getNow();
    String gage = elapsedTime(now - gps.timestamp);
    String loca = elapsedTime(now - gps.lockage);
    String uptime = elapsedTime(now - bootTime);
    String igt = elapsedTime(now - checkipgeo.lastattempt);
    String igp = elapsedTime(now - checkipgeo.lastsuccess);
    String wage = elapsedTime(now - weather.timestamp);
    String wlt = elapsedTime(now - checkweather.lastattempt);
    String alt = elapsedTime(now - checkalerts.lastattempt);
    String wls = elapsedTime((now - weather.lastshown) - (show_weather_interval.value()*60));
    String als = elapsedTime(now - alerts.lastshown);
    String alg = elapsedTime(now - checkalerts.lastsuccess);
    String wlg = elapsedTime(now - checkweather.lastsuccess);
    String alq = elapsedTime(now - checkaqi.lastsuccess);
    String alh = elapsedTime((now - weather.lastaqishown) - (airquality_interval.value() * 60));
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
    debug_print((String) "Version - Firmware:v" + VERSION_MAJOR + "." + VERSION_MINOR + "." + VERSION_PATCH + " | Config:v" + CONFIGVER, true);
    debug_print((String) "System - RawLux:" + current.rawlux + " | Lux:" + current.lux + " | UsrBright:+" + userbrightness + " | Brightness:" + current.brightness + " | ClockHue:" + current.clockhue + " | temphue:" + current.temphue + " | WifiState:" + connection_state[iotWebConf.getState()] + " | IP:" + lip + " | HTTPBusy: " + yesno[httpbusy] + " | Uptime:" + uptime, true);
    debug_print((String) "Clock - Status:" + clock_status[systemClock.getSyncStatusCode()] + " | TimeSource:" + timesource + " | CurrentTZ:" + current.tzoffset +  " | NtpReady:" + gpsClock.ntpIsReady + " | LastTry:" + elapsedTime(systemClock.getSecondsSinceSyncAttempt()) + " | NextTry:" + elapsedTime(systemClock.getSecondsToSyncAttempt()) + " | Skew:" + systemClock.getClockSkew() + " sec | NextNtp:" + npt + " | LastSync:" + lst, true);
    debug_print((String) "Loc - SavedLat:" + savedlat.value() + " | SavedLon:" + savedlon.value() + " | CurrentLat:" + current.lat + " | CurrentLon:" + current.lon, true);
    debug_print((String) "IPGeo - Complete:" + yesno[checkipgeo.complete] + " | Lat:" + ipgeo.lat + " | Lon:" + ipgeo.lon + " | TZoffset:" + ipgeo.tzoffset + " | Timezone:" + ipgeo.timezone + " | LastAttempt:" + igt + " | LastSuccess:" + igp, true);
    debug_print((String) "GPS - Chars:" + GPS.charsProcessed() + " | With-Fix:" + GPS.sentencesWithFix() + " | Failed:" + GPS.failedChecksum() + " | Passed:" + GPS.passedChecksum() + " | Sats:" + gps.sats + " | Hdop:" + gps.hdop + " | Elev:" + gps.elevation + " | Lat:" + gps.lat + " | Lon:" + gps.lon + " | FixAge:" + gage + " | LocAge:" + loca, true);
    debug_print((String) "Weather - Icon:" + weather.currentIcon + " | Temp:" + weather.currentTemp + tempunit + " | FeelsLike:" + weather.currentFeelsLike + tempunit + " | Humidity:" + weather.currentHumidity + "% | Wind:" + weather.currentWindSpeed + "/" + weather.currentWindGust + speedunit + " | Desc:" + weather.currentDescription + " | LastTry:" + wlt + " | LastSuccess:" + wlg + " | NextShow:" + wage, true);
    debug_print((String) "Air Quality - Aqi:" + air_quality[weather.currentaqi] + " | Co:" + weather.carbon_monoxide + " | No:" + weather.nitrogen_monoxide + " | No2:" + weather.nitrogen_dioxide + " | Ozone:" + weather.ozone + " | So2:" + weather.sulfer_dioxide + " | Pm2.5:" + weather.particulates_small + " | Pm10:" + weather.particulates_medium + " | Ammonia:" + weather.ammonia + " | LastSuccess:" + alq + " | NextShow:" + alh, true);
    debug_print((String) "Alerts - Active:" + yesno[alerts.active] + " | Watch:" + yesno[alerts.inWatch] + " | Warn:" + yesno[alerts.inWarning] + " | LastTry:" + alt + " | LastSuccess:" + alg + " | LastShown:" + als, true);
    debug_print((String) "Location: " + geocode.city + ", " + geocode.state + ", " + geocode.country, true);
    if (alerts.active)
      debug_print((String) "*Alert1 - Status:" + alerts.status1 + " | Severity:" + alerts.severity1 + " | Certainty:" + alerts.certainty1 + " | Urgency:" + alerts.urgency1 + " | Event:" + alerts.event1 + " | Desc:" + alerts.description1, true);
    debug_print(displaytoken.showTokens(), true);
  }
}

COROUTINE(checkGeocode) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(!httpbusy && checkgeocode.active && checkipgeo.complete && abs(systemClock.getNow() - checkgeocode.lastattempt) > T1M && iotWebConf.getState() == 4 && (current.lat).length() > 1 && (weatherapi.value())[0] != '\0' && displaytoken.isReady(0) && !firsttimefailsafe && curl.length() > 7);
  ESP_LOGI(TAG, "Checking Geocode Location...");
  httpbusy = true;
  Serial.println((String) "CURL: " + curl);
  checkgeocode.retries = 1;
  checkgeocode.jsonParsed = false;
  while (!checkgeocode.jsonParsed && (checkgeocode.retries <= 1))
  {
    COROUTINE_DELAY(1000);
    ESP_LOGD(TAG, "Connecting to %s", curl);
    HTTPClient http;
    http.begin(curl);
    int httpCode = http.GET();
    ESP_LOGD(TAG, "HTTP code : %d", httpCode);
    if (httpCode == 200)
    {
      geocodeJson = JSON.parse(http.getString());
      if (JSON.typeof(geocodeJson) == "undefined")
        ESP_LOGE(TAG, "Parsing geocodeJson input failed!");
      else
        checkgeocode.jsonParsed = true;
    }
    else if (httpCode == 401)
    {
      alertflash.color = RED;
      alertflash.laps = 1;
      alertflash.active = true;
      COROUTINE_AWAIT(!alertflash.active);
      scrolltext.message = "Invalid Openweather API Key";
      scrolltext.color = RED;
      scrolltext.active = true;
      scrolltext.displayicon = false;
      COROUTINE_AWAIT(!scrolltext.active);
    }
    else
    {
      alertflash.color = RED;
      alertflash.laps = 1;
      alertflash.active = true;
      COROUTINE_AWAIT(!alertflash.active);
      scrolltext.message = (String) "Error getting geocode location: " + httpCode;
      scrolltext.color = RED;
      scrolltext.active = true;
      scrolltext.displayicon = false;
      COROUTINE_AWAIT(!scrolltext.active);
    }
    http.end();
    checkgeocode.retries++;
  }
  if (!checkgeocode.jsonParsed)
    ESP_LOGE(TAG, "Error: JSON");
  else
  {
    fillGeocodeFromJson(&geocode);
    checkgeocode.lastsuccess = systemClock.getNow();
  }
  checkgeocode.lastattempt = systemClock.getNow();
  checkgeocode.active = false;
  httpbusy = false;
  }
}

#ifndef DISABLE_IPGEOCHECK
COROUTINE(checkIpgeo) {
  COROUTINE_BEGIN();
  COROUTINE_AWAIT(!httpbusy && iotWebConf.getState() == 4 && (ipgeoapi.value())[0] != '\0' && displaytoken.isReady(0) && !firsttimefailsafe && gurl.length() > 7);
  while (!checkipgeo.complete)
  {
    COROUTINE_AWAIT(abs(systemClock.getNow() - checkipgeo.lastattempt) > T1M);
    ESP_LOGI(TAG, "Checking IP Geolocation...");
    httpbusy = true;
    Serial.println((String) "GURL: " + gurl);
    checkipgeo.active = true;
    checkipgeo.retries = 1;
    checkipgeo.jsonParsed = false;
    while (!checkipgeo.jsonParsed && (checkipgeo.retries <= 1))
    {
      COROUTINE_DELAY(1000);
      ESP_LOGD(TAG, "Connecting to %s", gurl);
      HTTPClient http;
      http.begin(gurl);
      int httpCode = http.GET();
      ESP_LOGD(TAG, "HTTP code : %d", httpCode);
      if (httpCode == 200)
      {
        ipgeoJson = JSON.parse(http.getString());
        Serial.println(ipgeoJson);
        Serial.flush();
        if (JSON.typeof(ipgeoJson) == "undefined")
        ESP_LOGE(TAG, "Parsing weatherJson input failed!");
        else
          checkipgeo.jsonParsed = true;
      }
      else if (httpCode == 401)
      {
        alertflash.color = RED;
        alertflash.laps = 1;
        alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = "Invalid IPGeolocation API Key";
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;
        COROUTINE_AWAIT(!scrolltext.active);
      }
      else
      {
        alertflash.color = RED;
        alertflash.laps = 1;
        alertflash.active = true;
        COROUTINE_AWAIT(!alertflash.active);
        scrolltext.message = (String) "Error getting ip geolocation: " + httpCode;
        scrolltext.color = RED;
        scrolltext.active = true;
        scrolltext.displayicon = false;
        COROUTINE_AWAIT(!scrolltext.active);
      }
      http.end();
      checkipgeo.retries++;
    }
    if (!checkipgeo.jsonParsed)
      ESP_LOGE(TAG, "Error: JSON");
    else
    {
      fillIpgeoFromJson(&ipgeo);
      checkipgeo.lastsuccess = systemClock.getNow();
      checkipgeo.complete = true;
      checkipgeo.active = false;
      processLoc();
      processTimezone();
    }
    checkipgeo.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
  COROUTINE_END();
}
#endif

COROUTINE(miscScrollers) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(iotWebConf.getState() == 1 || resetme || scrolltext.showingip);
    COROUTINE_AWAIT((millis() - scrolltext.resetmsgtime) > T1M*1000 && displaytoken.isReady(4));
    if (scrolltext.showingcfg) 
    {
      displaytoken.setToken(4);
      scrolltext.message = "Settings Updated";
      scrolltext.color = GREEN;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      scrolltext.showingcfg = false;
      displaytoken.resetToken(4);
    }
    if (scrolltext.showingip) 
    {
      displaytoken.setToken(4);
      scrolltext.message = (String)"IP: " + (WiFi.localIP()).toString();
      scrolltext.color = GREEN;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      scrolltext.showingip = false;
      displaytoken.resetToken(4);
    }
    if (resetme)
    {
      displaytoken.setToken(4);
      scrolltext.showingreset = true;
      alertflash.color = RED;
      alertflash.laps = 5;
      alertflash.active = true;
      COROUTINE_AWAIT(!alertflash.active);
      scrolltext.message = "Resetting to Defaults, Connect to LEDSMARTCLOCK wifi to setup";
      scrolltext.color = RED;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      nvs_flash_erase(); // erase the NVS partition and...
      nvs_flash_init();  // initialize the NVS partition.
      ESP.restart();
     }
     else if (iotWebConf.getState() == 1 && (millis() - scrolltext.resetmsgtime) > T1M*1000) {
      displaytoken.setToken(4);
      scrolltext.showingreset = true;
      alertflash.color = RED;
      alertflash.laps = 5;
      alertflash.active = true;
      COROUTINE_AWAIT(!alertflash.active);
      scrolltext.message = (String)"Connect to LEDSMARTCLOCK wifi to setup, version:" + VERSION_MAJOR + "." + VERSION_MINOR + "." + VERSION_PATCH;
      scrolltext.color = RED;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      scrolltext.resetmsgtime = millis();
      scrolltext.showingreset = false;
      displaytoken.resetToken(4);
     }
  }
}

COROUTINE(coroutineManager) {
  COROUTINE_LOOP() 
  { 
  acetime_t now = systemClock.getNow();
  if (iotWebConf.getState() == 1)
      firsttimefailsafe = true;
  if (!displaytoken.isReady(0))
      showClock.suspend();
  if (showClock.isSuspended() && displaytoken.isReady(0))
    showClock.resume();
  if (serialdebug.isChecked() && print_debugData.isSuspended() && displaytoken.isReady(0))
    print_debugData.resume();
  else if (!serialdebug.isChecked() && !print_debugData.isSuspended())
    print_debugData.suspend();
  if (show_date.isChecked() && showDate.isSuspended() && iotWebConf.getState() != 1)
    showDate.resume();
  else if ((!show_date.isChecked() && !showDate.isSuspended()) || iotWebConf.getState() == 1)
    showDate.suspend();
  if (show_weather.isChecked() && showWeather.isSuspended() && iotWebConf.getState() != 1 && displaytoken.isReady(0))
    showWeather.resume();
  else if ((!show_weather.isChecked() && !showWeather.isSuspended()) || iotWebConf.getState() == 1)
    showWeather.suspend();
  if (show_weather_daily.isChecked() && showWeatherDaily.isSuspended() && iotWebConf.getState() != 1 && displaytoken.isReady(0))
    showWeatherDaily.resume();
  else if ((!show_weather_daily.isChecked() && !showWeatherDaily.isSuspended()) || iotWebConf.getState() == 1)
    showWeatherDaily.suspend();
  if (show_airquality.isChecked() && checkAirquality.isSuspended() && iotWebConf.getState() != 1 && displaytoken.isReady(0))
    checkAirquality.resume();
  else if ((!show_airquality.isChecked() && !checkAirquality.isSuspended()) || iotWebConf.getState() == 1)
    checkAirquality.suspend();
  if ((show_weather.isChecked() || show_weather_daily.isChecked()) && checkWeather.isSuspended() && iotWebConf.getState() != 1 && iotWebConf.getState() == 4 && displaytoken.isReady(0)) {
    checkWeather.reset();
    checkWeather.resume();
  }
  else if ((!show_weather.isChecked() && !show_weather_daily.isChecked() && !checkWeather.isSuspended()) || iotWebConf.getState() == 1 || iotWebConf.getState() != 4)
    checkWeather.suspend();
  #ifndef DISABLE_ALERTCHECK
  if (checkAlerts.isSuspended() && iotWebConf.getState() != 1 && iotWebConf.getState() == 4 && displaytoken.isReady(0)) {
    checkAlerts.reset();
    checkAlerts.resume();
  }
  else if (!checkAlerts.isSuspended() && iotWebConf.getState() < 4)
    checkAlerts.suspend();
  #endif
  if ((abs(now - alerts.lastshown) > (show_alert_interval.value()*T1M)) && show_alert_interval.value() != 0 && displaytoken.isReady(0) && alerts.active)
    cotimer.show_alert_ready = true;
  if ((abs(now - weather.lastshown) > (show_weather_interval.value() * T1M)) && (abs(now - checkweather.lastsuccess)) < T1H + 60 && displaytoken.isReady(0))
    cotimer.show_weather_ready = true;
  if ((abs(now - showclock.datelastshown) > (show_date_interval.value() * T1H)) && displaytoken.isReady(0))
    cotimer.show_date_ready = true;
  if ((abs(now - weather.lastdailyshown) > (show_weather_daily_interval.value() * T1H)) && (abs(now - checkweather.lastsuccess)) < T1D && displaytoken.isReady(0))
    cotimer.show_weather_daily_ready = true;  
  if ((abs(now - weather.lastaqishown) > (airquality_interval.value() * T1M)) && (abs(now - checkaqi.lastsuccess)) < T1H + 60 && displaytoken.isReady(0))
    cotimer.show_airquality_ready = true;
  COROUTINE_YIELD();
  }
}

COROUTINE(IotWebConf) {
  COROUTINE_LOOP() 
  {
    //COROUTINE_AWAIT(millis() - cotimer.iotloop > 5);
    iotWebConf.doLoop();
    //cotimer.iotloop = millis();
    COROUTINE_YIELD();
  }
}

COROUTINE(AlertFlash) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(alertflash.active && showClock.isSuspended() && !disable_alertflash.isChecked());
      displaytoken.setToken(6);
      alertflash.lap = 0;
      matrix->clear();
      matrix->show();
      COROUTINE_DELAY(250);
      while (alertflash.lap < alertflash.laps)
      {
        matrix->clear();
        matrix->fillScreen(alertflash.color);
        matrix->show();
        COROUTINE_DELAY(250);
        matrix->clear();
        matrix->show();
        COROUTINE_DELAY(250);
        alertflash.lap++;
      }
      alertflash.active = false;
      displaytoken.resetToken(6);
  }
}

COROUTINE(scrollText) {
  COROUTINE_LOOP() 
  {
      COROUTINE_AWAIT(scrolltext.active && showClock.isSuspended());
      COROUTINE_AWAIT(millis() - scrolltext.millis >= cotimer.scrollspeed-1);

      if (!displaytoken.getToken(10)) 
      {
        displaytoken.setToken(10);
        ESP_LOGI(TAG, "Scrolltext Message: %s",scrolltext.message);
        ESP_LOGI(TAG, "Scrolltext Color: %llu",scrolltext.color);
      }
      uint16_t size = (scrolltext.message).length() * 6;
      if (scrolltext.position >= size - size - size) {
        matrix->clear();
        matrix->setCursor(scrolltext.position, 0);
        matrix->setTextColor(scrolltext.color);
        matrix->print(scrolltext.message);
        //display_showStatus();
        if (scrolltext.displayicon)
          display_weatherIcon(scrolltext.icon);
        matrix->show();
        scrolltext.position--;
        scrolltext.millis = millis();
      } else {
      scrolltext.active = false;
      scrolltext.position = mw;
      scrolltext.displayicon = false;
      ESP_LOGD(TAG, "Scrolltext [%s] complete",scrolltext.message);
      displaytoken.resetToken(10);
      
      }
  }
}

COROUTINE(gps_checkData) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(Serial1.available() > 0);
    while (Serial1.available() > 0)
      GPS.encode(Serial1.read());
    if (GPS.satellites.isUpdated())
    {
      gps.sats = GPS.satellites.value();
      ESP_LOGV(TAG, "GPS Satellites updated: %d", gps.sats);
      if (gps.sats > 0 && gps.fix == false)
      {
        gps.fix = true;
        gps.timestamp = systemClock.getNow();
        ESP_LOGI(TAG, "GPS fix aquired with %d satellites", gps.sats);
      }
      if (gps.sats == 0 && gps.fix == true)
      {
        gps.fix = false;
        ESP_LOGI(TAG, "GPS fix lost, no satellites");
      }
    }
    if (GPS.location.isUpdated()) 
    {
      if (!use_fixed_loc.isChecked()) {
        gps.lat = String(GPS.location.lat(), 5);
        gps.lon = String(GPS.location.lng(), 5);
        gps.lockage = systemClock.getNow();
        processLoc();
      }
      else {
        gps.lat = fixedLat.value();
        gps.lon = fixedLon.value();
        processLoc();
      }
      ESP_LOGV(TAG, "GPS Location updated: Lat: %s Lon: %s", gps.lat, gps.lon);
    }
    if (GPS.altitude.isUpdated()) 
    {
      gps.elevation = GPS.altitude.feet();
      ESP_LOGV(TAG, "GPS Elevation updated: %f feet", gps.elevation);
    }
    if (GPS.hdop.isUpdated()) 
    {
      gps.hdop = GPS.hdop.hdop();
      ESP_LOGV(TAG, "GPS HDOP updated: %f m",gps.hdop);
    }
    if (GPS.charsProcessed() < 10) ESP_LOGE(TAG, "No GPS data. Check wiring.");
  COROUTINE_DELAY(1000);
  }
}

COROUTINE(setBrightness) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(Tsl.available());
    Tsl.fullLuminosity(current.rawlux);
    current.lux = constrain(map(current.rawlux, 0, 500, LUXMIN, LUXMAX), LUXMIN, LUXMAX);
    current.brightavg = ((current.brightness) + (current.lux + userbrightness)) / 2;
    if (current.brightavg != current.brightness) 
    {
      current.brightness = current.brightavg;
      matrix->setBrightness(current.brightness);
      matrix->show();
    }
    #ifdef DEBUG_LIGHT
    Serial.println((String) "Lux: " + full + " brightness: " + cb + " avg: " + current.brightness);
    #endif
    COROUTINE_DELAY(LIGHT_CHECK_DELAY);
  }
}
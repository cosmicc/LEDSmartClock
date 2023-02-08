// Coroutines
COROUTINE(sysClock) {
  COROUTINE_LOOP()
  {
    systemClock.loop();
    COROUTINE_YIELD();
  }
}

COROUTINE(IotWebConf) {
  COROUTINE_LOOP() 
  {
    iotWebConf.doLoop();
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
    COROUTINE_AWAIT(isHttpReady() && isNextShowReady(checkaqi.lastsuccess, airquality_interval.value(), T1M) && isNextAttemptReady(checkaqi.lastattempt) && isApiValid(weatherapi.value()) && isCoordsValid() && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking air quality conditions...");
    httpbusy = true;
    checkaqi.retries++;
    checkaqi.jsonParsed = false;
    IotWebConf.suspend();
    if (httpRequest(3)) {
    int httpCode = request.GET();
    IotWebConf.resume();
    String payload;
    if (httpCode > 0)
          payload = request.getString();
    if (httpCode == 200)
      {
        ESP_LOGI(TAG, "AQI response code: %d", httpCode);
        Json = JSON.parse(payload);
        if (JSON.typeof(Json) == "undefined")
          ESP_LOGE(TAG, "Parsing AQIJson input failed!");
        else 
        {
            checkaqi.jsonParsed = true;
            fillAqiFromJson(&weather);
            checkaqi.lastsuccess = systemClock.getNow();
            checkaqi.retries = 0;
            showready.aqi = true;
        }
      }
      else
        ESP_LOGE(TAG, "AQI error code: %d", httpCode);
    }
    request.end();
    checkaqi.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}

COROUTINE(checkWeather) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(isHttpReady() && isNextShowReady(checkweather.lastsuccess, show_weather_interval.value(), T1M) && isNextAttemptReady(checkweather.lastattempt)  && isApiValid(weatherapi.value()) && isCoordsValid() && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking weather forecast...");
    httpbusy = true;
    checkweather.retries++;
    checkweather.jsonParsed = false;
    IotWebConf.suspend();
    if (httpRequest(0)) 
    {
      int httpCode = request.GET();
      IotWebConf.resume();
      String payload;
      if (httpCode == 200)
      {
        payload = request.getString();
        ESP_LOGI(TAG, "Weather response code: %d", httpCode);
        Json = JSON.parse(payload);
        if (JSON.typeof(Json) == "undefined")
          ESP_LOGE(TAG, "Parsing WeatherJson input failed!");
        else 
        {
            checkweather.jsonParsed = true;
            fillWeatherFromJson(&weather);
            checkweather.lastsuccess = systemClock.getNow();
            checkweather.retries = 0;
            showready.currentweather = true;
            showready.dayweather = true;
        }
      }
      else
        ESP_LOGE(TAG, "Weather error code: %d", httpCode);
      
    }
    request.end();
    checkweather.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}

#ifndef DISABLE_ALERTCHECK
COROUTINE(checkAlerts) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(isHttpReady() && isNextShowReady(checkalerts.lastsuccess, 5, T1M) && isNextAttemptReady(checkalerts.lastattempt) && isCoordsValid());
    ESP_LOGI(TAG, "Checking weather alerts...");
    httpbusy = true;
    checkalerts.retries++;
    checkalerts.jsonParsed = false;
    IotWebConf.suspend();
    if (httpRequest(1)) 
    {
      int httpCode = request.GET();
      IotWebConf.resume();
      String payload; 
      if (httpCode == 200)
      {
        payload = request.getString();
        ESP_LOGI(TAG, "Alerts response code: %d", httpCode);
        Json = JSON.parse(payload);
        if (JSON.typeof(Json) == "undefined")
          ESP_LOGE(TAG, "Parsing AlertsJson input failed!");
        else 
        {
            checkalerts.jsonParsed = true;
            fillAlertsFromJson(&alerts);
            checkalerts.lastsuccess = systemClock.getNow();
            showready.alerts;
        }
      }
      else
        ESP_LOGE(TAG, "Alerts error code: %d", httpCode);
    }
    request.end();
    checkalerts.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}
#endif

COROUTINE(showDate) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(isNextShowReady(lastshown.date, show_date_interval.value(), T1H) && show_date.isChecked() && iotWebConf.getState() != 1 && displaytoken.isReady(1));
  displaytoken.setToken(1);
  scrolltext.message = getSystemZonedDateString();
  scrolltext.color = hex2rgb(datecolor.value());
  scrolltext.active = true;
  COROUTINE_AWAIT(!scrolltext.active);
  lastshown.date = systemClock.getNow();
  showready.date = false;
  displaytoken.resetToken(1);
  }
}

COROUTINE(showWeather) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(showready.currentweather && show_weather.isChecked() && displaytoken.isReady(2));
  displaytoken.setToken(2);
  alertflash.color = hex2rgb(weather_color.value());
  alertflash.laps = 1;
  alertflash.active = true;
  COROUTINE_AWAIT(!alertflash.active);
  scrolltext.message = capString((String)"Current " + weather.currentDescription + " Humidity " + weather.currentHumidity + "% Wind " + int(weather.currentWindSpeed) + "/" + int(weather.currentWindGust) + " Air: " + air_quality[weather.currentaqi]);
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
  lastshown.currentweather = systemClock.getNow();
  showready.currentweather = false;
  displaytoken.resetToken(2);
  }
}

COROUTINE(showWeatherDaily) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(showready.dayweather && show_weather_daily.isChecked() && displaytoken.isReady(8));
  displaytoken.setToken(8);
  alertflash.color = hex2rgb(weather_daily_color.value());
  alertflash.laps = 1;
  alertflash.active = true;
  strncpy(scrolltext.icon, weather.dayIcon, 4);
  COROUTINE_AWAIT(!alertflash.active);
  scrolltext.message = capString((String)"Today " + weather.dayDescription + " Humidity " + weather.dayHumidity + "% Wind " + int(weather.dayWindSpeed) + "/" + int(weather.dayWindGust));
  scrolltext.color = hex2rgb(weather_daily_color.value());
  scrolltext.active = true;
  scrolltext.displayicon = true;
  COROUTINE_AWAIT(!scrolltext.active);
  scrolltext.displayicon = false;
  cotimer.millis = millis();
  lastshown.dayweather = systemClock.getNow();
  showready.dayweather = false;
  displaytoken.resetToken(8);
  }
}

COROUTINE(showAirquality) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(showready.aqi && show_airquality.isChecked() && displaytoken.isReady(9));
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
  scrolltext.message = (String) "AQI: " + air_quality[weather.currentaqi];
  scrolltext.displayicon = false;
  scrolltext.active = true;
  COROUTINE_AWAIT(!scrolltext.active);
  cotimer.millis = millis();
  lastshown.aqi = systemClock.getNow();
  showready.aqi = false;
  displaytoken.resetToken(9);
  }
}

COROUTINE(showAlerts) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(showready.alerts && displaytoken.isReady(3));
  if (alerts.active) {
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
    lastshown.alerts = systemClock.getNow();
    displaytoken.resetToken(3);
  }
  showready.alerts = false;
  }
}

COROUTINE(serialInput) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(500);
  if (Serial.available())
  {
    char input;
    input = Serial.read();
    switch (input)
    {
    case 'd':
        print_debugData();
        break;
    case 's':
        CoroutineScheduler::list(Serial);
        break;
    case 'r':
        showready.aqi = true;
        break;
    case 'w':
        showready.currentweather = true;
        break;
    case 'e':
        showready.date = true;
        break;
    case 'q':
        showready.dayweather = true;
        break;
    case 'c':
        //iotWebConf._allParameters.debugTo(&Serial);
        Serial.println();
        break;
    #ifdef COROUTINE_PROFILER    
    case 'f':
      LogBinTableRenderer::printTo(Serial, 2 /*startBin*/, 13 /*endBin*/, false /*clear*/);
      break;
    #endif
    }
  }
  }
}

COROUTINE(checkGeocode) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(isHttpReady() && isNextAttemptReady(checkgeocode.lastattempt) && isCoordsValid() && isApiValid(weatherapi.value()) && checkgeocode.ready && checkipgeo.complete && !firsttimefailsafe);
  ESP_LOGI(TAG, "Checking Geocode Location...");
  httpbusy = true;
  checkgeocode.jsonParsed = false;
  IotWebConf.suspend();
  if (httpRequest(2)) {
    int httpCode = request.GET();
    IotWebConf.resume();
    String payload;
    checkgeocode.retries++;
    if (httpCode == 200)
    {
      payload = request.getString();
      ESP_LOGI(TAG, "Geocode response code: %d", httpCode);
      Json = JSON.parse(payload);
      if (JSON.typeof(Json) == "undefined")
        ESP_LOGE(TAG, "Parsing GEOCodeJson input failed!");
      else 
      {
          checkgeocode.jsonParsed = true;
          fillGeocodeFromJson(&geocode);
          checkgeocode.lastsuccess = systemClock.getNow();
          checkgeocode.retries = 0;
          checkgeocode.ready = false;
          updateLocation();
      }
    }
    else if (httpCode == 401)
    {
      ESP_LOGE(TAG, "Geocode Invalid API, error code: %d", httpCode);
      //TODO: scroll invalid api
    }
    else
      ESP_LOGE(TAG, "Geocode error code: %d", httpCode);
  }
  request.end();
  checkgeocode.lastattempt = systemClock.getNow();
  httpbusy = false;
  }
}

#ifndef DISABLE_IPGEOCHECK
COROUTINE(checkIpgeo) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(!checkipgeo.complete && isHttpReady() && isNextAttemptReady(checkipgeo.lastattempt) && isApiValid(ipgeoapi.value()) && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking IP Geolocation...");
    httpbusy = true;
    checkipgeo.jsonParsed = false;
    IotWebConf.suspend();
    if (httpRequest(4))
    {
    int httpCode = request.GET();
    IotWebConf.resume();
    String payload;
    checkipgeo.retries++;
    if (httpCode == 200)
      {
      payload = request.getString();
      ESP_LOGI(TAG, "IPGeo response code: %d", httpCode);
      Json = JSON.parse(payload);
      if (JSON.typeof(Json) == "undefined")
          ESP_LOGE(TAG, "Parsing IPGeoJson input failed!");
        else 
        {
          checkipgeo.jsonParsed = true;
          fillIpgeoFromJson(&ipgeo);
          checkipgeo.lastsuccess = systemClock.getNow();
          checkipgeo.complete = true;
          checkipgeo.retries = 0;
          updateCoords();
          checkgeocode.ready = true;
          processTimezone();
        }
      }
      else if (httpCode == 401)
    {
      ESP_LOGE(TAG, "IPGeo Invalid API, error code: %d", httpCode);
      //TODO: scroll invalid api
    }
      else
        ESP_LOGE(TAG, "IPGeo response code: %d", httpCode);
    }
    request.end();
    checkipgeo.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}
#endif

COROUTINE(systemMessages) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT((iotWebConf.getState() == 1 || showready.reset || showready.ip || showready.loc) && displaytoken.isReady(4));
    if (showready.loc) 
    {
      displaytoken.setToken(4);
      scrolltext.message = (String)"New Location: " + current.city + ", " + current.state + ", " + current.country;
      scrolltext.color = GREEN;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      showready.loc = false;
      displaytoken.resetToken(4);
    }
    if (showready.ip) 
    {
      displaytoken.setToken(4);
      scrolltext.message = (String)"IP: " + (WiFi.localIP()).toString();
      scrolltext.color = GREEN;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      showready.ip = false;
      displaytoken.resetToken(4);
    }
    if (showready.reset && (millis() - scrolltext.resetmsgtime) > T1M*1000)
    {
      displaytoken.setToken(4);
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
      alertflash.color = RED;
      alertflash.laps = 5;
      alertflash.active = true;
      COROUTINE_AWAIT(!alertflash.active);
      scrolltext.message = (String)"Connect to LEDSMARTCLOCK wifi to setup, version:" + VERSION_MAJOR + "." + VERSION_MINOR + "." + VERSION_PATCH;
      scrolltext.color = RED;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      scrolltext.resetmsgtime = millis();
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
  if (!showClock.isSuspended() && !displaytoken.isReady(0))
  {
      showClock.suspend();
      ESP_LOGD(TAG, "Show Clock Coroutine Suspended");
  }
  if (showClock.isSuspended() && displaytoken.isReady(0))
  {
    showClock.resume();
    ESP_LOGD(TAG, "Show Clock Coroutine Resumed");
  }
    
  if (serialdebug.isChecked() && serialInput.isSuspended())
  {
    serialInput.resume();
    ESP_LOGD(TAG, "Serial Input Coroutine Resumed");
  }
  else if (!serialdebug.isChecked() && !serialInput.isSuspended())
  {
    serialInput.suspend();
   ESP_LOGD(TAG, "Serial Input Coroutine Suspended"); 
  }
    
  if (show_date.isChecked() && showDate.isSuspended())
  {
    showDate.resume();
   ESP_LOGD(TAG, "Show Date Coroutine Resumed"); 
  }
  else if (!show_date.isChecked() && !showDate.isSuspended())
  {
    showDate.suspend();
    ESP_LOGD(TAG, "Show Date Coroutine Suspended");
  }
    
  if (show_weather.isChecked() && showWeather.isSuspended())
  {
    showWeather.resume();
    ESP_LOGD(TAG, "Show Current Weather Coroutine Resumed");
  }
  else if ((!show_weather.isChecked() && !showWeather.isSuspended()))
  {
    showWeather.suspend();
    ESP_LOGD(TAG, "Show Current Weather Coroutine Suspended");
  }
    
  if (show_weather_daily.isChecked() && showWeatherDaily.isSuspended())
  {
    showWeatherDaily.resume();
   ESP_LOGD(TAG, "Show Day Weather Coroutine Resumed"); 
  }
  else if (!show_weather_daily.isChecked() && !showWeatherDaily.isSuspended())
  {
    showWeatherDaily.suspend();
    ESP_LOGD(TAG, "Show Day Weather Coroutine Suspended");
  }

  if (show_airquality.isChecked() && showAirquality.isSuspended())
  {
    showAirquality.resume();
    ESP_LOGD(TAG, "Show Air Quality Coroutine Resumed");
  }
  else if (!show_airquality.isChecked() && !showAirquality.isSuspended())
  {
    showAirquality.suspend();
    ESP_LOGD(TAG, "Show Air Quality Coroutine Suspended");
  }
  #ifndef DISABLE_WEATHERCHECK         
  // Weather couroutine management
  if ((show_weather.isChecked() || show_weather_daily.isChecked()) && checkWeather.isSuspended() && isCoordsValid() && isApiValid(weatherapi.value()) && iotWebConf.getState() == 4) 
  {
    checkWeather.reset();
    checkWeather.resume();
    ESP_LOGD(TAG, "Check Weather Coroutine Resumed");
  }
  else if ((!show_weather.isChecked() && !show_weather_daily.isChecked() && !checkWeather.isSuspended()) && (iotWebConf.getState() != 4 || !isCoordsValid() || !isApiValid(weatherapi.value())))
  {
    checkWeather.suspend();
    ESP_LOGD(TAG, "Check Weather Coroutine Suspended"); 
  }
  #endif
  // Alerts coroutine management
  #ifndef DISABLE_ALERTCHECK
  if (checkAlerts.isSuspended() && iotWebConf.getState() == 4) 
  {
    checkAlerts.reset();
    checkAlerts.resume();
    ESP_LOGD(TAG, "Check Alerts Coroutine Resumed");
  }
  else if (!checkAlerts.isSuspended() && iotWebConf.getState() != 4)
  {
    checkAlerts.suspend();
    ESP_LOGD(TAG, "Check Alerts Coroutine Suspended");
  }
  #endif
  //AQI coroutine management
  #ifndef DISABLE_AQICHECK         
  if ((checkAirquality.isSuspended() && iotWebConf.getState() == 4 && isCoordsValid() && isApiValid(weatherapi.value())) && (show_weather.isChecked() || show_weather_daily.isChecked() || show_airquality.isChecked())) 
  {
    checkAirquality.reset();
    checkAirquality.resume();
    ESP_LOGD(TAG, "Check Air Quality Coroutine Resumed");
  }
  else if (!checkAirquality.isSuspended() && (!isCoordsValid() || !isApiValid(weatherapi.value())) && !show_weather.isChecked() && !show_weather_daily.isChecked() && !show_airquality.isChecked() && iotWebConf.getState() != 4)
           {           
    checkAirquality.suspend();    
    ESP_LOGD(TAG, "Check Air Quality Coroutine Suspended");
           }
  #endif
  if (checkipgeo.complete && !checkIpgeo.isSuspended())
  {
    checkIpgeo.suspend();    
    ESP_LOGD(TAG, "Check IPGeolocation Coroutine Suspended");
  }
  COROUTINE_YIELD();
  }
}

COROUTINE(AlertFlash) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(alertflash.active && showClock.isSuspended());
    if (!disable_alertflash.isChecked()) {
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
      }
      uint16_t size = (scrolltext.message).length() * 6;
      if (scrolltext.position >= size - size - size) {
        matrix->clear();
        matrix->setCursor(scrolltext.position, 0);
        matrix->setTextColor(scrolltext.color);
        matrix->print(scrolltext.message);
        if (scrolltext.displayicon)
          display_weatherIcon(scrolltext.icon);
        matrix->show();
        scrolltext.position--;
        scrolltext.millis = millis();
      } else {
      scrolltext.active = false;
      scrolltext.position = mw;
      scrolltext.displayicon = false;
      ESP_LOGD(TAG, "Scrolltext complete",scrolltext.message);
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
        updateCoords();
      }
      else {
        gps.lat = fixedLat.value();
        gps.lon = fixedLon.value();
        updateCoords();
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

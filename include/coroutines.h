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
    COROUTINE_AWAIT(isHttpReady() && (isNextShowReady(checkaqi.lastsuccess, airquality_interval.value(), T1M) || isNextShowReady(checkweather.lastsuccess, show_weather_interval.value(), T1M) ) && isNextAttemptReady(checkaqi.lastattempt) && isApiValid(weatherapi.value()) && checkaqi.retries < 10 && isCoordsValid() && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking air quality conditions...");
    httpbusy = true;
    checkaqi.retries++;
    checkaqi.jsonParsed = false;
    IotWebConf.suspend();
    int httpCode;
    String reqdata;
    if (httpRequest(3))
    {
      httpCode = request.GET();
      IotWebConf.resume();
      if (httpCode == 200)
      {
        reqdata = request.getString();
        ESP_LOGD(TAG, "AQI http response code: [%d], payload length: [%d]", httpCode, reqdata.length());
        fillAqiFromJson(reqdata);
        checkaqi.lastsuccess = systemClock.getNow();
        checkaqi.retries = 0;
        ESP_LOGI(TAG, "New air quality data received");
        if (isNextShowReady(checkaqi.lastsuccess, airquality_interval.value(), T1M))
          showready.aqi = true;
      }
      else if (httpCode == 401)
      {
      ESP_LOGE(TAG, "Openweather Invalid API, error code: [%d]", httpCode);
      showready.apierror = true;
      showready.apierrorname = "openweather.org";
      }
      else
        ESP_LOGE(TAG, "AQI ERROR code: [%d]", httpCode);
    }
    request.end();
    checkaqi.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}

COROUTINE(checkWeather) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(isHttpReady() && isNextShowReady(checkweather.lastsuccess, show_weather_interval.value(), T1M) && isNextAttemptReady(checkweather.lastattempt)  && isApiValid(weatherapi.value()) && checkweather.retries < 10 && isCoordsValid() && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking weather forecast...");
    httpbusy = true;
    checkweather.retries++;
    checkweather.jsonParsed = false;
    IotWebConf.suspend();
    int httpCode;
    String reqdata;
    if (httpRequest(0))
    {
      httpCode = request.GET();
      IotWebConf.resume();
      if (httpCode == 200)
      {
        reqdata = request.getString();
        ESP_LOGD(TAG, "Weather http response code: [%d], payload length: [%d]", httpCode, reqdata.length());
        fillWeatherFromJson(reqdata);
        checkweather.lastsuccess = systemClock.getNow();
        checkweather.retries = 0;
        showready.currentweather = true;
        showready.dayweather = true;
        ESP_LOGI(TAG, "New weather data received");
      }
      else if (httpCode == 401)
      {
      ESP_LOGE(TAG, "Openweather Invalid API, error code: [%d]", httpCode);
      showready.apierror = true;
      showready.apierrorname = "openweather.org";
      }
      else
        ESP_LOGE(TAG, "Weather ERROR code: [%d]", httpCode);
    }
    request.end();
    checkweather.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}

#ifndef DISABLE_ALERTCHECK
COROUTINE(checkAlerts) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(isHttpReady() && isNextShowReady(checkalerts.lastsuccess, 5, T1M) && isNextAttemptReady(checkalerts.lastattempt) && checkalerts.retries < 10 && isCoordsValid());
    ESP_LOGI(TAG, "Checking weather alerts...");
    httpbusy = true;
    checkalerts.retries++;
    IotWebConf.suspend();
    int httpCode;
    String reqdata;
    if (httpRequest(1))
    {
      httpCode = request.GET();
      IotWebConf.resume();
      if (httpCode == 200)
      {
        reqdata = request.getString();
        ESP_LOGD(TAG, "Alerts http response code: [%d], payload length: [%d]", httpCode, reqdata.length());
        checkalerts.retries = 0;
        fillAlertsFromJson(reqdata);
        cleanString(alerts.description1);
        cleanString(alerts.instruction1);
        checkalerts.lastsuccess = systemClock.getNow();
        showready.alerts = true;
      }
      else
        ESP_LOGE(TAG, "Alerts ERROR code: [%d]", httpCode);
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
  scrolltext.message = capString((String)"Current " + weather.currentDescription + " Humidity " + weather.currentHumidity + "% Wind " + int(weather.currentWindSpeed) + "/" + int(weather.currentWindGust) + " Air: " + air_quality[aqi.currentaqi]);
  scrolltext.color = hex2rgb(weather_color.value());
  scrolltext.active = true;
  scrolltext.displayicon = true;
  scrolltext.icon = weather.currentIcon;
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
  scrolltext.icon = weather.dayIcon;
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
    if (aqi.currentaqi == 1)
        color = GREEN;
    else if (aqi.currentaqi == 2)
        color = YELLOW;
    else if (aqi.currentaqi == 3)
        color = ORANGE;
    else if (aqi.currentaqi == 4)
        color = RED;
    else if (aqi.currentaqi == 5)
        color = PURPLE;
    else
        color = WHITE;
  }
  alertflash.color = color;
  scrolltext.color = color;
  alertflash.laps = 1;
  alertflash.active = true;
  COROUTINE_AWAIT(!alertflash.active);
  scrolltext.message = (String) "AQI: " + air_quality[aqi.currentaqi];
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
  if (alerts.active)
  {
    displaytoken.setToken(3);
    if (alerts.inWarning) 
    {
      alertflash.color = RED;
      scrolltext.color = RED;
    }
    else if (alerts.inWatch)
    {
      alertflash.color = YELLOW;
      scrolltext.color = YELLOW;
    }
    alertflash.laps = 3;
    alertflash.active = true;
    COROUTINE_AWAIT(!alertflash.active);
    scrolltext.message = (String)alerts.description1 + " " + alerts.instruction1;
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
        checkaqi.lastsuccess = systemClock.getNow() - (airquality_interval.value() * 60);
        lastshown.aqi = systemClock.getNow() - (airquality_interval.value() * 60);
        COROUTINE_AWAIT(fromNow(checkaqi.lastsuccess) < 10);
        showready.aqi = true;
        break;
    case 'w':
        checkweather.lastsuccess = systemClock.getNow() - (show_weather_interval.value() * 60);
        COROUTINE_AWAIT(fromNow(checkweather.lastsuccess) < 10);
        lastshown.currentweather = systemClock.getNow() - (show_weather_interval.value() * 60);
        break;
    case 'e':
        lastshown.date = systemClock.getNow() - ((show_date_interval.value()) * 60);
        showready.date = true;
        break;
    case 'q':
        showready.dayweather = true;
        break;
    case 'c':
        //iotWebConf._allParameters.debugTo(&Serial);
        Serial.println();
        break;
    case 't':
      alertflash.color = hex2rgb(systemcolor.value());
      alertflash.laps = 5;
      alertflash.active = true;
      showClock.suspend();
      COROUTINE_AWAIT(!alertflash.active);
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
  COROUTINE_AWAIT(isHttpReady() && isNextAttemptReady(checkgeocode.lastattempt) && isCoordsValid() && isApiValid(weatherapi.value()) && checkgeocode.ready && checkipgeo.complete && checkgeocode.retries < 10 && !firsttimefailsafe);
  ESP_LOGI(TAG, "Checking Geocode Location...");
  httpbusy = true;
  checkgeocode.jsonParsed = false;
  IotWebConf.suspend();
  int httpCode;
  if (httpRequest(2))
  {
    httpCode = request.GET();
    IotWebConf.resume();
    String payload;
    checkgeocode.retries++;
    if (httpCode == 200)
    {
      payload = request.getString();
      ESP_LOGD(TAG, "GEOcode http response code: [%d], payload length: [%d]", httpCode, payload.length());
      fillGeocodeFromJson(payload);
      checkgeocode.lastsuccess = systemClock.getNow();
      checkgeocode.retries = 0;
      checkgeocode.ready = false;
      updateLocation();
      }
      else if (httpCode == 401)
      {
      ESP_LOGE(TAG, "Openweather.org Invalid API, error code: [%d]", httpCode);
      showready.apierrorname = "openweather.org";
      showready.apierror = true;
      }
      else
       ESP_LOGE(TAG, "Geocode ERROR code: [%d]", httpCode);
  }
  request.end();
  checkgeocode.lastattempt = systemClock.getNow();
  httpbusy = false;
  }
}

#ifndef DISABLE_IPGEOCHECK
COROUTINE(checkIpgeo) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(!checkipgeo.complete && isHttpReady() && isNextAttemptReady(checkipgeo.lastattempt) && isApiValid(ipgeoapi.value()) && checkipgeo.retries < 10 && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking IP Geolocation...");
    httpbusy = true;
    checkipgeo.jsonParsed = false;
    IotWebConf.suspend();
    int httpCode;
    if (httpRequest(4))
    {
    httpCode = request.GET();
    IotWebConf.resume();
    String payload;
    checkipgeo.retries++;
    if (httpCode == 200)
      {
      payload = request.getString();
      ESP_LOGD(TAG, "IPGeo http response code: [%d], payload length: [%d]", httpCode, payload.length());
      fillIpgeoFromJson(payload);
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
      ESP_LOGE(TAG, "IPGeolocation.io Invalid API, error code: [%d]", httpCode);
      showready.apierrorname = "ipgeolocation.io";
      showready.apierror = true;
    }
    else
        ESP_LOGE(TAG, "IPGeo ERROR code: [%d]", httpCode);
    request.end();
    checkipgeo.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}
#endif

COROUTINE(systemMessages) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT((iotWebConf.getState() == 1 || showready.reset || showready.ip || showready.apierror || showready.loc) && displaytoken.isReady(4));
    if (showready.apierror) 
    {
      displaytoken.setToken(4);
      alertflash.color = RED;
      alertflash.laps = 5;
      alertflash.active = true;
      COROUTINE_AWAIT(!alertflash.active);
      scrolltext.message = (String) showready.apierrorname + "API is invalid, please check settings!";
      scrolltext.color = RED;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      showready.apierror = false;
      displaytoken.resetToken(4);
    }
    if (showready.loc) 
    {
      alertflash.color = hex2rgb(systemcolor.value());
      alertflash.laps = 1;
      alertflash.active = true;
      COROUTINE_AWAIT(!alertflash.active);
      displaytoken.setToken(4);
      scrolltext.message = (String)"New Location: " + current.city + ", " + current.state + ", " + current.country;
      scrolltext.color = hex2rgb(systemcolor.value());
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      showready.loc = false;
      displaytoken.resetToken(4);
    }
    if (showready.ip) 
    {
      displaytoken.setToken(4);
      alertflash.color = hex2rgb(systemcolor.value());
      alertflash.laps = 1;
      alertflash.active = true;
      COROUTINE_AWAIT(!alertflash.active);
      scrolltext.message = (String)"IP: " + (WiFi.localIP()).toString();
      scrolltext.color = hex2rgb(systemcolor.value());
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

COROUTINE(AlertFlash) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(alertflash.active && showClock.isSuspended());
    if (enable_alertflash.isChecked()) 
    {
      displaytoken.setToken(6);
      alertflash.lap = 0;
      setBrightness.suspend();
      matrix->clear();
      matrix->show();
      alertflash.brightness = 0;
      alertflash.fadecycle = 0;
      alertflash.multiplier = 0;
      while (alertflash.lap < alertflash.laps)
      {
        COROUTINE_DELAY(100);
        while (alertflash.fadecycle <= 4)
        {
          alertflash.multiplier = alertflash.multiplier + 0.25;
          alertflash.brightness = current.brightness * alertflash.multiplier;
          matrix->clear();
          matrix->setBrightness(alertflash.brightness);
          matrix->fillScreen(alertflash.color);
          matrix->show();
          alertflash.fadecycle++;
          COROUTINE_DELAY(25);
        }
        alertflash.fadecycle = 0;
        while (alertflash.fadecycle <= 4)
        {
          alertflash.multiplier = alertflash.multiplier - 0.25;
          alertflash.brightness = current.brightness * alertflash.multiplier;
          matrix->clear();
          matrix->setBrightness(alertflash.brightness);
          matrix->fillScreen(alertflash.color);
          matrix->show();
          alertflash.fadecycle++;
          COROUTINE_DELAY(25);
        }
        alertflash.lap++;
        alertflash.fadecycle = 0;
        alertflash.multiplier = 0;
      }
      matrix->clear();
      matrix->show();
      setBrightness.resume();
      COROUTINE_DELAY(200);
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
        ESP_LOGI(TAG, "Scrolltext(): Scrolling message: [%s]", scrolltext.message.c_str());
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
      ESP_LOGD(TAG, "Scrolltext(): Scrolling complete: [%s]", scrolltext.message.c_str());
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
        gps.lat =GPS.location.lat();
        gps.lon = GPS.location.lng();
        gps.lockage = systemClock.getNow();
        updateCoords();
      }
      else {
        gps.lat = strtod(fixedLat.value(), NULL);
        gps.lon = strtod(fixedLon.value(), NULL);
        updateCoords();
      }
      ESP_LOGV(TAG, "GPS Location updated: Lat: %f Lon: %f", gps.lat, gps.lon);
    }
    if (GPS.altitude.isUpdated()) 
    {
      gps.elevation = GPS.altitude.feet();
      ESP_LOGV(TAG, "GPS Elevation updated: %d feet", gps.elevation);
    }
    if (GPS.hdop.isUpdated()) 
    {
      gps.hdop = GPS.hdop.hdop();
      ESP_LOGV(TAG, "GPS HDOP updated: %d m",gps.hdop);
    }
    if (GPS.charsProcessed() < 10) ESP_LOGE(TAG, "No GPS data. Check wiring.");
  COROUTINE_DELAY(1000);
  }
}
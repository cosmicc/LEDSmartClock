COROUTINE(sysClock) 
{
  COROUTINE_LOOP()
  {
    systemClock.loop();
    COROUTINE_YIELD();
  }
}

COROUTINE(IotWebConf) 
{
  COROUTINE_LOOP() 
  {
    iotWebConf.doLoop();
    COROUTINE_YIELD();
  }
}

COROUTINE(showClock) 
{
  COROUTINE_LOOP() 
  {
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
    if (enable_clock_color.isChecked())
      showclock.color = hex2rgb(clock_color.value());
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
    if (!colonflicker.isChecked()) {
      int16_t clock_x;
      clock_x = clock_display_offset ? 12 : 16;
      matrix->drawPixel(clock_x, 5, showclock.color); // draw colon
      matrix->drawPixel(clock_x, 2, showclock.color); // draw colon
    }
    display_showStatus();
    matrix->show();
    COROUTINE_AWAIT(millis() - showclock.millis > showclock.fstop);
    if (colonflicker.isChecked()) {
      int16_t clock_x;
      clock_x = clock_display_offset ? 12 : 16;
      matrix->drawPixel(clock_x, 5, showclock.color); // draw colon
      matrix->drawPixel(clock_x, 2, showclock.color); // draw colon
      display_showStatus();
      matrix->show();
    }
    COROUTINE_AWAIT(showclock.seconds != getSystemZonedTime().second());
  }
}

COROUTINE(checkAirquality) 
{
  COROUTINE_LOOP() 
  {
    //  || isNextShowReady(checkweather.lastsuccess, current_weather_interval.value(), T1M)
    COROUTINE_AWAIT(isHttpReady() && (isNextShowReady(checkaqi.lastsuccess, aqi_interval.value(), T1M)) && isNextAttemptReady(checkaqi.lastattempt) && isApiValid(weatherapi.value()) && checkaqi.retries < HTTP_MAX_RETRIES && isCoordsValid() && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking air quality conditions...");
    httpbusy = true;
    checkaqi.retries++;
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
        ESP_LOGD(TAG, "AQI http response code: [%d] %s, payload length: [%d]", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.length());
        if (fillAqiFromJson(reqdata))
        {
          checkaqi.retries = 0;
          checkaqi.complete = true;
          checkaqi.lastsuccess = systemClock.getNow();
          ESP_LOGI(TAG, "New air quality data received");
          if (!checkaqi.firsttime)
            showready.aqi = true;
          else
            checkaqi.firsttime = false;
        }
      }
      else if (httpCode == 401)
      {
      ESP_LOGE(TAG, "Openweather Invalid API, error: [%d] %s", httpCode, getHttpCodeName(httpCode).c_str());
      showready.apierror = true;
      strcpy(showready.apierrorname, "openweather.org");
      }
      else
        ESP_LOGE(TAG, "CHECKAQI ERROR: [%d] %s ", httpCode, getHttpCodeName(httpCode).c_str());
    }
    request.end();
    checkaqi.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}

COROUTINE(checkWeather) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(isHttpReady() && isNextShowReady(checkweather.lastsuccess, current_weather_interval.value(), T1M) && isNextAttemptReady(checkweather.lastattempt)  && isApiValid(weatherapi.value()) && checkweather.retries < HTTP_MAX_RETRIES && isCoordsValid() && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking weather forecast...");
    httpbusy = true;
    checkweather.retries++;
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
        ESP_LOGD(TAG, "Weather http response code: [%d] %s, payload length: [%d]", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.length());
        if (fillWeatherFromJson(reqdata))
        {
          checkweather.lastsuccess = systemClock.getNow();
          checkweather.retries = 0;
          checkweather.complete = true;
          if (!checkweather.firsttime)
          {
            showready.currentweather = true;
            showready.dayweather = true;
          }
          else
            checkweather.firsttime = false;
          ESP_LOGI(TAG, "New weather data received");
        }
      }
      else if (httpCode == 401)
      {
        ESP_LOGE(TAG, "Openweather Invalid API, error: [%d] %s", httpCode, getHttpCodeName(httpCode).c_str());
        showready.apierror = true;
        strcpy(showready.apierrorname, "openweather.org");
      }
      else
        ESP_LOGE(TAG, "CheckWeather ERROR: [%d] %s", httpCode, getHttpCodeName(httpCode).c_str());
    }
    request.end();
    checkweather.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}

#ifndef DISABLE_ALERTCHECK
COROUTINE(checkAlerts) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(isHttpReady() && isNextShowReady(checkalerts.lastsuccess, 5, T1M) && isNextAttemptReady(checkalerts.lastattempt) && checkalerts.retries < HTTP_MAX_RETRIES && isCoordsValid());
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
        ESP_LOGD(TAG, "Alerts http response code: [%d] %s, payload length: [%d]", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.length());
        checkalerts.retries = 0;
        if (fillAlertsFromJson(reqdata))
        {
          checkalerts.lastsuccess = systemClock.getNow();
          showready.alerts = true;
        }
      }
      else
        ESP_LOGE(TAG, "Alerts ERROR: [%d] %s", httpCode, getHttpCodeName(httpCode).c_str());
    }
    request.end();
    checkalerts.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}
#endif

COROUTINE(showDate) 
{
  COROUTINE_LOOP() 
  {
  COROUTINE_AWAIT(isNextShowReady(lastshown.date, date_interval.value(), T1H) && show_date.isChecked() && iotWebConf.getState() != 1 && displaytoken.isReady(1));
  displaytoken.setToken(1);
  getSystemZonedDateString(scrolltext.message);
  startScroll(hex2rgb(date_color.value()), false);
  COROUTINE_AWAIT(!scrolltext.active);
  lastshown.date = systemClock.getNow();
  showready.date = false;
  displaytoken.resetToken(1);
  }
}

COROUTINE(showWeather) 
{
  COROUTINE_LOOP() 
  {
  COROUTINE_AWAIT(showready.currentweather && show_current_weather.isChecked() && displaytoken.isReady(2));
  displaytoken.setToken(2);
  startFlash(hex2rgb(current_weather_color.value()), 1);
  COROUTINE_AWAIT(!alertflash.active);
  char speedunit[7];
  if (imperial.isChecked())
    memcpy(speedunit, imperial_units[1], 7);
  else
    memcpy(speedunit, metric_units[1], 7);
  snprintf(scrolltext.message, 512, "Current %s Humidity:%d%% Wind:%d/%d%s Clouds:%d%% AQi:%s UVi:%s", capString(weather.current.description), weather.current.humidity, weather.current.windSpeed, weather.current.windGust, speedunit, weather.current.cloudcover, air_quality[aqi.current.aqi], uv_index(weather.current.uvi).c_str());
  startScroll(hex2rgb(current_weather_color.value()), true);
  memcpy(scrolltext.icon, weather.current.icon, sizeof(weather.current.icon[0]) * 4);
  COROUTINE_AWAIT(!scrolltext.active);
  cotimer.millis = millis();
  while (millis() - cotimer.millis < 10000) 
  {
    matrix->clear();
    display_temperature();
    display_showStatus();
    display_weatherIcon(weather.current.icon);
    matrix->show();
    COROUTINE_DELAY(50);
  }
  lastshown.currentweather = systemClock.getNow();
  showready.currentweather = false;
  displaytoken.resetToken(2);
  }
}

COROUTINE(showWeatherDaily) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(showready.dayweather && show_daily_weather.isChecked() && displaytoken.isReady(8));
    displaytoken.setToken(8);
    startFlash(hex2rgb(daily_weather_color.value()), 1);
    memcpy(scrolltext.icon, weather.day.icon, sizeof(weather.day.icon[0]) * 4);
    COROUTINE_AWAIT(!alertflash.active);
    char speedunit[7];
    char tempunit[7];
    if (imperial.isChecked())
    {
        memcpy(speedunit, imperial_units[1], 7);
        memcpy(tempunit, imperial_units[3], 7);
    }
    else
    {
      memcpy(speedunit, metric_units[1], 7);
      memcpy(tempunit, metric_units[3], 7);
    }
    snprintf(scrolltext.message, 512, "Today %s Hi:%d Lo:%d Humidity:%d%% Wind:%d/%d%s Clouds:%d%% AQi:%s UVi:%s", capString(weather.day.description), weather.day.tempMax, weather.day.tempMin, weather.day.humidity, weather.day.windSpeed, weather.day.windGust, speedunit, weather.day.cloudcover, air_quality[aqi.day.aqi], uv_index(weather.day.uvi).c_str());
    startScroll(hex2rgb(daily_weather_color.value()), true);
    COROUTINE_AWAIT(!scrolltext.active);
    cotimer.millis = millis();
    lastshown.dayweather = systemClock.getNow();
    showready.dayweather = false;
    displaytoken.resetToken(8);
  }
}

COROUTINE(showAirquality) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(showready.aqi && show_aqi.isChecked() && displaytoken.isReady(9));
    displaytoken.setToken(9);
    // set color using lookup value and bit-shift if enable_aqi_color.isChecked() is false
    if (enable_aqi_color.isChecked())
      aqi.current.color = hex2rgb(aqi_color.value());
    else
      aqi.current.color = (AQIColorLookup[aqi.current.aqi]);
    startFlash(aqi.current.color, 1);
    COROUTINE_AWAIT(!alertflash.active);
    snprintf(scrolltext.message, 512, "Air Quality: %s", air_quality[aqi.current.aqi]);
    startScroll(aqi.current.color, false);
    COROUTINE_AWAIT(!scrolltext.active);
    cotimer.millis = millis();
    lastshown.aqi = systemClock.getNow();
    showready.aqi = false;
    displaytoken.resetToken(9);
  }
}

COROUTINE(showAlerts) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(showready.alerts && displaytoken.isReady(3));
    if (alerts.active)
    {
      displaytoken.setToken(3);
      startFlash(alertcolors[alerts.inWatch], 3);
      COROUTINE_AWAIT(!alertflash.active);
      char *inst = alerts.instruction1;
      capitalize(inst);
      strncpy(scrolltext.message, alerts.description1, sizeof(scrolltext.message) - 1);
      strncat(scrolltext.message, ". ", sizeof(scrolltext.message)-strlen(scrolltext.message)-1);
      strncat(scrolltext.message, inst, sizeof(scrolltext.message)-strlen(scrolltext.message)-1);
      startScroll(alertcolors[alerts.inWatch], false);
      COROUTINE_AWAIT(!scrolltext.active);
      lastshown.alerts = systemClock.getNow();
      displaytoken.resetToken(3);
    }
    showready.alerts = false;
  }
}

COROUTINE(serialInput) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(500);
    if (Serial.available())
    {
      char input;
      input = Serial.read();
      switch (input)
      {
        case 'd':  // Display debug data
            print_debugData();
            break;
        case 's':  // Display coroutines states
            CoroutineScheduler::list(Serial);
            break;
        case 'r':  // Force display of current air quality
            lastshown.aqi = systemClock.getNow() - (aqi_interval.value() * 60);
            showready.aqi = true;
            break;
        case 'w':  // Force display of current weather
            lastshown.currentweather = systemClock.getNow() - (current_weather_interval.value() * 60);
            showready.currentweather = true;
            break;
        case 'e':  // Display the current date
            lastshown.date = systemClock.getNow() - ((date_interval.value()) * 60 * 60);
            showready.date = true;
            break;
        case 'q':  // Force display of day weather
            lastshown.dayweather = systemClock.getNow() - (daily_weather_interval.value() * 60 * 60);
            showready.dayweather = true;
            break;
        case 'c':  // Display current main task stack size remaining (bytes)
            printf("Main task stack size: %s bytes remaining\n", formatLargeNumber(uxTaskGetStackHighWaterMark(NULL)).c_str());
            break;
        case 't':  // Test alertflash
            startFlash(hex2rgb(system_color.value()), 5);
            COROUTINE_AWAIT(!alertflash.active);
            break;
        #ifdef COROUTINE_PROFILER    
        case 'f':  // Display coroutine execution time table (Profiler)
          LogBinTableRenderer::printTo(Serial, 2 /*startBin*/, 13 /*endBin*/, false /*clear*/);
          break;
        #endif
        case 'a':
          printf("Log level changed to DEBUG\n");
          esp_log_level_set(TAG, ESP_LOG_DEBUG);
          break;
        default:
          printf("Unknown command: %c\n", input);
          break;
        }
    }
  }
}

COROUTINE(checkGeocode) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(isHttpReady() && isNextAttemptReady(checkgeocode.lastattempt) && isCoordsValid() && isApiValid(weatherapi.value()) && checkgeocode.ready && checkipgeo.complete && checkgeocode.retries < HTTP_MAX_RETRIES && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking Geocode Location...");
    httpbusy = true;
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
        ESP_LOGD(TAG, "GEOcode http response code: [%d] %s, payload length: [%d]", httpCode, getHttpCodeName(httpCode).c_str(), payload.length());
        if (fillGeocodeFromJson(payload))
        {
          checkgeocode.lastsuccess = systemClock.getNow();
          checkgeocode.retries = 0;
          checkgeocode.ready = false;
          updateLocation();
        }
      }
      else if (httpCode == 401)
      {
        ESP_LOGE(TAG, "Openweather.org Invalid API, error: [%d] %s", httpCode, getHttpCodeName(httpCode).c_str());
        strcpy(showready.apierrorname, "openweather.org");
        showready.apierror = true;
      }
      else
        ESP_LOGE(TAG, "CheckGeocode ERROR: [%d] %s", httpCode, getHttpCodeName(httpCode).c_str());
    }
    request.end();
    checkgeocode.lastattempt = systemClock.getNow();
    httpbusy = false;
  }
}

#ifndef DISABLE_IPGEOCHECK
COROUTINE(checkIpgeo) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(!checkipgeo.complete && isHttpReady() && isNextAttemptReady(checkipgeo.lastattempt) && isApiValid(ipgeoapi.value()) && checkipgeo.retries < HTTP_MAX_RETRIES && !firsttimefailsafe);
      ESP_LOGI(TAG, "Checking IP Geolocation...");
      httpbusy = true;
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
        ESP_LOGD(TAG, "IPGeo http response code: [%d] %s, payload length: [%d]", httpCode, getHttpCodeName(httpCode).c_str(), payload.length());
        if (fillIpgeoFromJson(payload))
        {
          checkipgeo.lastsuccess = systemClock.getNow();
          checkipgeo.complete = true;
          checkipgeo.retries = 0;
          updateCoords();
          checkgeocode.ready = true;
          processTimezone();
        }
        else
          ESP_LOGE(TAG, "fillIpgeoFromJson() Error parsing response");
        }
      }
      else if (httpCode == 401)
      {
        ESP_LOGE(TAG, "IPGeolocation.io Invalid API, error: [%d] %s", httpCode, getHttpCodeName(httpCode).c_str());
        strcpy(showready.apierrorname, "ipgeolocation.io");
        showready.apierror = true;
      }
      else
          ESP_LOGE(TAG, "CheckIPGeo ERROR: [%d] %s", httpCode, getHttpCodeName(httpCode).c_str());
      request.end();
      checkipgeo.lastattempt = systemClock.getNow();
      httpbusy = false;
  }
}
#endif

COROUTINE(systemMessages) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT((iotWebConf.getState() == 1 || showready.reset || showready.ip || showready.apierror || showready.loc || showready.sunrise || showready.sunset) && displaytoken.isReady(4));
    displaytoken.setToken(4);
    if (showready.sunrise) 
    {
      startFlash(hex2rgb(sunrise_color.value()), 2);
      COROUTINE_AWAIT(!alertflash.active);
      snprintf(scrolltext.message, sizeof(scrolltext.message), "%s", sunrise_message.value());
      startScroll(hex2rgb(sunrise_color.value()), false);
      COROUTINE_AWAIT(!scrolltext.active);
      showready.sunrise = false;
      showClock.resume();
    }
    else if (showready.sunset) 
    {
      startFlash(hex2rgb(sunset_color.value()), 2);
      COROUTINE_AWAIT(!alertflash.active);
      snprintf(scrolltext.message, sizeof(scrolltext.message), "%s", sunset_message.value());
      startScroll(hex2rgb(sunset_color.value()), false);
      COROUTINE_AWAIT(!scrolltext.active);
      showready.sunset = false;
      showClock.resume();
    }
    else if (showready.apierror) 
    {
      startFlash(RED, 5);
      COROUTINE_AWAIT(!alertflash.active);
      snprintf(scrolltext.message, sizeof(scrolltext.message), "%s API is invalid, please check settings!", showready.apierrorname);
      startScroll(RED, false);
      COROUTINE_AWAIT(!scrolltext.active);
      showready.apierror = false;
      showClock.resume();
    }
    else if (showready.loc) 
    {
      startFlash(hex2rgb(system_color.value()), 1);
      COROUTINE_AWAIT(!alertflash.active);
      snprintf(scrolltext.message, sizeof(scrolltext.message), "New Location: %s, %s, %s", current.city, current.state, current.country);
      startScroll(hex2rgb(system_color.value()), false);
      COROUTINE_AWAIT(!scrolltext.active);
      showready.loc = false;
      showClock.resume();
    }
    else if (showready.ip) 
    {
      startFlash(hex2rgb(system_color.value()), 1);
      COROUTINE_AWAIT(!alertflash.active);
      snprintf(scrolltext.message, sizeof(scrolltext.message), "IP: %s", WiFi.localIP().toString().c_str());
      startScroll(hex2rgb(system_color.value()), false);
      COROUTINE_AWAIT(!scrolltext.active);
      showready.ip = false;
      showClock.resume();
    }
    if (showready.reset && (millis() - scrolltext.resetmsgtime) > T1M*1000)
    {
      startFlash(RED, 5);
      COROUTINE_AWAIT(!alertflash.active);
      snprintf(scrolltext.message, sizeof(scrolltext.message), "Resetting to Defaults, Connect to LEDSMARTCLOCK wifi to setup");
      startScroll(RED, false);
      COROUTINE_AWAIT(!scrolltext.active);
      nvs_flash_erase(); // erase the NVS partition and...
      nvs_flash_init();  // initialize the NVS partition.
      ESP.restart();
     }
    else if (iotWebConf.getState() == 1 && (millis() - scrolltext.resetmsgtime) > T1M*1000) 
    {
      startFlash(RED, 5);
      COROUTINE_AWAIT(!alertflash.active);
      snprintf(scrolltext.message, sizeof(scrolltext.message), "Connect to LEDSMARTCLOCK wifi to setup, version: %d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
      startScroll(RED, false);
      COROUTINE_AWAIT(!scrolltext.active);
      scrolltext.resetmsgtime = millis();
     }
    displaytoken.resetToken(4);
  }
}

COROUTINE(coroutineManager) 
{
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
    
  if (show_current_weather.isChecked() && showWeather.isSuspended())
  {
    showWeather.resume();
    ESP_LOGD(TAG, "Show Current Weather Coroutine Resumed");
  }
  else if ((!show_current_weather.isChecked() && !showWeather.isSuspended()))
  {
    showWeather.suspend();
    ESP_LOGD(TAG, "Show Current Weather Coroutine Suspended");
  }
    
  if (show_daily_weather.isChecked() && showWeatherDaily.isSuspended())
  {
    showWeatherDaily.resume();
   ESP_LOGD(TAG, "Show Day Weather Coroutine Resumed"); 
  }
  else if (!show_daily_weather.isChecked() && !showWeatherDaily.isSuspended())
  {
    showWeatherDaily.suspend();
    ESP_LOGD(TAG, "Show Day Weather Coroutine Suspended");
  }

  if (show_aqi.isChecked() && showAirquality.isSuspended())
  {
    showAirquality.resume();
    ESP_LOGD(TAG, "Show Air Quality Coroutine Resumed");
  }
  else if (!show_aqi.isChecked() && !showAirquality.isSuspended())
  {
    showAirquality.suspend();
    ESP_LOGD(TAG, "Show Air Quality Coroutine Suspended");
  }
  #ifndef DISABLE_WEATHERCHECK         
  // Weather couroutine management
  if ((show_current_weather.isChecked() || show_daily_weather.isChecked()) && checkWeather.isSuspended() && isCoordsValid() && isApiValid(weatherapi.value()) && iotWebConf.getState() == 4 && checkweather.retries < 10) 
  {
    checkWeather.reset();
    checkWeather.resume();
    ESP_LOGD(TAG, "Check Weather Coroutine Resumed");
  }
  else if ((!show_current_weather.isChecked() && !show_daily_weather.isChecked() && !checkWeather.isSuspended()) && (iotWebConf.getState() != 4 || !isCoordsValid() || !isApiValid(weatherapi.value())))
  {
    checkWeather.suspend();
    ESP_LOGD(TAG, "Check Weather Coroutine Suspended"); 
  }
  #endif
  // Alerts coroutine management
  #ifndef DISABLE_ALERTCHECK
  if (checkAlerts.isSuspended() && iotWebConf.getState() == 4 && checkalerts.retries < HTTP_MAX_RETRIES) 
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
  if ((checkAirquality.isSuspended() && iotWebConf.getState() == 4 && isCoordsValid() && isApiValid(weatherapi.value())) && checkaqi.retries < HTTP_MAX_RETRIES && (show_current_weather.isChecked() || show_daily_weather.isChecked() || show_aqi.isChecked())) 
  {
    checkAirquality.reset();
    checkAirquality.resume();
    ESP_LOGD(TAG, "Check Air Quality Coroutine Resumed");
  }
  else if (!checkAirquality.isSuspended() && (!isCoordsValid() || !isApiValid(weatherapi.value())) && !show_current_weather.isChecked() && !show_daily_weather.isChecked() && !show_aqi.isChecked() && iotWebConf.getState() != 4)
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
  if (compareTimes(ZonedDateTime::forEpochSeconds(systemClock.getNow(), current.timezone), ZonedDateTime::forEpochSeconds(weather.day.sunrise, current.timezone)) && show_sunrise.isChecked())
  {
    showready.sunrise = true;
  }
  else if (compareTimes(ZonedDateTime::forEpochSeconds(systemClock.getNow(), current.timezone), ZonedDateTime::forEpochSeconds(weather.day.sunset, current.timezone)) && show_sunset.isChecked())
  {
    showready.sunset = true;
  }
  if (checkweather.retries == HTTP_MAX_RETRIES && !checkWeather.isSuspended())
  {
    ESP_LOGE(TAG, "Check weather retry limit [%d] exceeded, disabling service for protection", HTTP_MAX_RETRIES);
    checkWeather.suspend();
  }
  if (checkalerts.retries == HTTP_MAX_RETRIES && !checkAlerts.isSuspended())
  {
    ESP_LOGE(TAG, "Check alerts retry limit [%d] exceeded, disabling service for protection", HTTP_MAX_RETRIES);
    checkAlerts.suspend();
  }
  if (checkaqi.retries == HTTP_MAX_RETRIES && !checkAirquality.isSuspended())
  {
    ESP_LOGE(TAG, "Check aqi retry limit [%d] exceeded, disabling service for protection", HTTP_MAX_RETRIES);
    checkAirquality.suspend();
  }
  if (checkipgeo.retries == HTTP_MAX_RETRIES && !checkIpgeo.isSuspended())
  {
    ESP_LOGE(TAG, "Check IPGeo retry limit [%d] exceeded, disabling service for protection", HTTP_MAX_RETRIES);
    checkIpgeo.suspend();
  }
  if (checkgeocode.retries == HTTP_MAX_RETRIES && !checkGeocode.isSuspended())
  {
    ESP_LOGE(TAG, "Check Geocode retry limit [%d] exceeded, disabling service for protection", HTTP_MAX_RETRIES);
    checkGeocode.suspend();
  }
  COROUTINE_YIELD();
  }
}

COROUTINE(setBrightness) {
COROUTINE_LOOP() {
  COROUTINE_AWAIT(Tsl.available());
  Tsl.fullLuminosity(current.rawlux);
  // ensure limited range for lux value  
  current.lux = ((current.rawlux << 6) + (current.rawlux << 5) + (current.rawlux << 2) + LUXMIN * 257) >> 9;
  current.lux = current.lux < LUXMIN ? LUXMIN : current.lux > LUXMAX ? LUXMAX : current.lux;
  current.brightavg = (current.brightness + current.lux + userbrightness) >> 1;
  if (current.brightavg != current.brightness)
  {
    current.brightness = current.brightavg;
    matrix->setBrightness(current.brightness);
    matrix->show();
  }
  #ifdef DEBUG_LIGHT
  Serial.println("Lux: " + full + ", brightness: " + cb + ", avg: " + current.brightness);
  #endif
  COROUTINE_DELAY(LIGHT_CHECK_DELAY);
  }
}

COROUTINE(AlertFlash) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(alertflash.active && showClock.isSuspended() && showClock.isSuspended());
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
        while (alertflash.fadecycle <= 4) {
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
      matrix->setBrightness(current.brightness);
      matrix->show();
      setBrightness.reset();
      setBrightness.resume();
      COROUTINE_DELAY(200);
    }
      alertflash.active = false;
      displaytoken.resetToken(6);
  }
}

COROUTINE(scrollText) {
COROUTINE_LOOP() {
    COROUTINE_AWAIT(scrolltext.active && showClock.isSuspended());
    // Wait until the scroll speed has been met
    COROUTINE_AWAIT(millis() - scrolltext.millis >= cotimer.scrollspeed-1);
    // Get a token if not already set
    if (!displaytoken.getToken(10)) {
        displaytoken.setToken(10);
        scrolltext.size = strlen(scrolltext.message) * 6 - 1;
        ESP_LOGI(TAG, "Scrolltext(): Scrolling message - Chars:%d Width:%d \"%s\"", strlen(scrolltext.message), scrolltext.size, scrolltext.message);
    }
    // Scroll and display message if position within bounds
    if (scrolltext.position >= -scrolltext.size) {
        matrix->clear();
        matrix->setCursor(scrolltext.position, 0);
        matrix->setTextColor(scrolltext.color);
        matrix->print(scrolltext.message);
        // Display weather icon if true
        if (scrolltext.displayicon) {
            display_weatherIcon(scrolltext.icon);
        }
        matrix->show();
        scrolltext.position--;
        scrolltext.millis = millis();
    // Otherwise stop scrolling
    } else {
        scrolltext.active = false;
        scrolltext.position = mw-1;
        scrolltext.displayicon = false;
        ESP_LOGD(TAG, "Scrolltext(): Scrolling complete: Chars:%d Width:%d \"%s\"", strlen(scrolltext.message), scrolltext.size, scrolltext.message);
        displaytoken.resetToken(10);
    }
}
}

COROUTINE(gps_checkData) {
COROUTINE_LOOP() 
{
    COROUTINE_AWAIT(Serial1.available() > 0);
    while (Serial1.available() > 0)
    {
        // Read in the data from the serial port
        GPS.encode(Serial1.read());
    }
    if (GPS.satellites.isUpdated())
    {
        // Update the number of satellites all available satellites
        gps.sats = GPS.satellites.value();
        ESP_LOGV(TAG, "GPS Satellites updated: %d", gps.sats);
        // Check if a GPS fix is aquired and log the fix as true and record the timestamp
        if (gps.sats > 0 && (gps.fix & 0x1) == 0)
        {
            gps.fix |= 0x1; // Set the first bit to 1 to indicate a valid fix
            gps.timestamp = systemClock.getNow();
            ESP_LOGI(TAG, "GPS fix aquired with %d satellites", gps.sats);
        }
        // Check if the GPS fix is lost and set the fix variable to false
        else if (gps.sats == 0 && (gps.fix & 0x1) > 0)
        {
            gps.fix &= 0x0; // Set the first bit to 0 to indicate no valid fix
            ESP_LOGI(TAG, "GPS fix lost, no satellites");
        }
    }
    if (GPS.location.isUpdated()) 
    {
        // If 'enable_fixed_loc' is not checked, set the latitude and longitude values to the values stored in the location object
        if (!enable_fixed_loc.isChecked()) {
            gps.lat =GPS.location.lat();
            gps.lon = GPS.location.lng();
            // Set the lockage timestamp
            gps.lockage = systemClock.getNow();
            // Call the 'updateCoords()' function
            updateCoords();
        }
        // If 'enable_fixed_loc' is checked, set the latitude and longitude values to the values stored in the fixedLat and fixedLon values, which are strings
        else {
            gps.lat = strtod(fixedLat.value(), NULL);
            gps.lon = strtod(fixedLon.value(), NULL);
            // Call the 'updateCoords()' function
            updateCoords();
        }
        // Log the latitude and longitude
        ESP_LOGV(TAG, "GPS Location updated: Lat: %f Lon: %f", gps.lat, gps.lon);
    }
    if (GPS.altitude.isUpdated()) 
    {
        // Set the elevation in feet
        gps.elevation = GPS.altitude.feet();
        // Log the elevation
        ESP_LOGV(TAG, "GPS Elevation updated: %d feet", gps.elevation);
    }
    if (GPS.hdop.isUpdated()) 
    {
        // Set the HDOP
        gps.hdop = GPS.hdop.hdop();
        // Log the HDOP
        ESP_LOGV(TAG, "GPS HDOP updated: %d m",gps.hdop);
    }
    // Log a warning if there is no GPS data
    if (GPS.charsProcessed() < 10) ESP_LOGE(TAG, "No GPS data. Check wiring.");
    COROUTINE_DELAY(1000);
}
}
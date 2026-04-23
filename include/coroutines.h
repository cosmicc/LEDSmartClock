COROUTINE(sysClock) 
{
  COROUTINE_LOOP()
  {
    systemClock.loop();
    COROUTINE_YIELD();
  }
}

/** Returns the fastest enabled weather-driven refresh interval in minutes. */
static uint32_t weatherRefreshIntervalMinutes()
{
  uint32_t refreshMinutes = static_cast<uint32_t>(current_temp_interval.value());

  if (show_current_weather.isChecked())
    refreshMinutes = min(refreshMinutes, static_cast<uint32_t>(current_weather_interval.value()) * 60U);

  if (show_daily_weather.isChecked())
    refreshMinutes = min(refreshMinutes, static_cast<uint32_t>(daily_weather_interval.value()) * 60U);

  return refreshMinutes;
}

/** Returns the fastest enabled AQI-driven refresh interval in minutes. */
static uint32_t aqiRefreshIntervalMinutes()
{
  uint32_t refreshMinutes = static_cast<uint32_t>(aqi_interval.value());

  if (show_current_weather.isChecked())
    refreshMinutes = min(refreshMinutes, static_cast<uint32_t>(current_weather_interval.value()) * 60U);

  if (show_daily_weather.isChecked())
    refreshMinutes = min(refreshMinutes, static_cast<uint32_t>(daily_weather_interval.value()) * 60U);

  return refreshMinutes;
}

/** Returns true when runtime crosses a target epoch between scheduler passes. */
static bool crossedEpochBoundary(acetime_t previous, acetime_t current, acetime_t boundary)
{
  if (boundary <= 0 || previous <= 0 || current < previous)
    return false;

  return previous < boundary && current >= boundary;
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
      runtimeState.clockDisplayOffset = false;
    }
    else
    {
      if (myhour / 10 != 0)
      {
        runtimeState.clockDisplayOffset = false;
        display_setclockDigit(myhour / 10, 0, showclock.color); // set first digit of hour
      }
      else
        runtimeState.clockDisplayOffset = true;
      display_setclockDigit(myhour % 10, 1, showclock.color); // set second digit of hour
    }
    display_setclockDigit(myminute / 10, 2, showclock.color); // set first digig of minute
    display_setclockDigit(myminute % 10, 3, showclock.color); // set second digit of minute
    if (!colonflicker.isChecked()) {
      int16_t clock_x;
      clock_x = runtimeState.clockDisplayOffset ? 12 : 16;
      matrix->drawPixel(clock_x, 5, showclock.color); // draw colon
      matrix->drawPixel(clock_x, 2, showclock.color); // draw colon
    }
    display_showStatus();
    matrix->show();
    COROUTINE_AWAIT(millis() - showclock.millis > showclock.fstop);
    if (colonflicker.isChecked()) {
      int16_t clock_x;
      clock_x = runtimeState.clockDisplayOffset ? 12 : 16;
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
    COROUTINE_AWAIT(isHttpReady() && isNextShowReady(checkaqi.lastsuccess, aqiRefreshIntervalMinutes(), T1M) && isNextAttemptReady(checkaqi.lastattempt) && isApiValid(weatherapi.value()) && checkaqi.retries < HTTP_MAX_RETRIES && isCoordsValid() && !runtimeState.firstTimeFailsafe);
    ESP_LOGI(TAG, "Checking air quality conditions...");
    checkaqi.retries++;
    noteDiagnosticPending(DiagnosticService::AirQuality, true, "Refreshing",
                          F("Requesting the latest AQI forecast from OpenWeather."), checkaqi.retries);
    IotWebConf.suspend();
    int httpCode = 0;
    String reqdata;
    if (beginApiRequest(ApiEndpoint::AirQuality))
    {
      httpCode = networkService.client.GET();
      IotWebConf.resume();
      if (httpCode == 200)
      {
        reqdata = networkService.client.getString();
        ESP_LOGD(TAG, "AQI http response code: [%d] %s, payload length: [%d]", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.length());
        if (fillAqiFromJson(reqdata))
        {
          checkaqi.retries = 0;
          checkaqi.complete = true;
          checkaqi.lastsuccess = systemClock.getNow();
          ESP_LOGI(TAG, "New air quality data received");
          noteDiagnosticSuccess(DiagnosticService::AirQuality, true, "Fresh data",
                                String(air_quality[aqi.current.aqi]) + F(" AQI, ") + aqi.current.description,
                                checkaqi.retries, httpCode);
          if (checkaqi.firsttime)
            checkaqi.firsttime = false;
        }
        else
          noteDiagnosticFailure(DiagnosticService::AirQuality, true, "Parse failed",
                                F("OpenWeather returned AQI data, but the JSON payload could not be parsed cleanly."),
                                httpCode, checkaqi.retries);
      }
      else if (httpCode == 401 || httpCode == 403 || httpCode == 404 || httpCode == 429)
      {
        reqdata = networkService.client.getString();
        ESP_LOGE(TAG, "OpenWeather AQI request failed: [%d] %s | body: %s", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.c_str());
        noteDiagnosticFailure(DiagnosticService::AirQuality, true, "HTTP error",
                              String(F("OpenWeather AQI request failed with ")) + httpCode + F(" ") + getHttpCodeName(httpCode),
                              httpCode, checkaqi.retries);
        showready.apierror = true;
        strlcpy(showready.apierrorname, "openweather.org", sizeof(showready.apierrorname));
      }
      else
      {
        reqdata = networkService.client.getString();
        ESP_LOGE(TAG, "CHECKAQI ERROR: [%d] %s | body: %s", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.c_str());
        noteDiagnosticFailure(DiagnosticService::AirQuality, true, "Request failed",
                              String(F("AQI request failed with ")) + httpCode + F(" ") + getHttpCodeName(httpCode),
                              httpCode, checkaqi.retries);
      }
    }
    else
    {
      IotWebConf.resume();
      ESP_LOGE(TAG, "Failed to begin OpenWeather AQI request");
      noteDiagnosticFailure(DiagnosticService::AirQuality, true, "Start failed",
                            F("The shared HTTP client could not begin the AQI request."),
                            0, checkaqi.retries);
    }
    endApiRequest();
    checkaqi.lastattempt = systemClock.getNow();
  }
}

COROUTINE(checkWeather) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(isHttpReady() && isNextShowReady(checkweather.lastsuccess, weatherRefreshIntervalMinutes(), T1M) && isNextAttemptReady(checkweather.lastattempt)  && isApiValid(weatherapi.value()) && checkweather.retries < HTTP_MAX_RETRIES && isCoordsValid() && !runtimeState.firstTimeFailsafe);
    ESP_LOGI(TAG, "Checking weather forecast...");
    checkweather.retries++;
    noteDiagnosticPending(DiagnosticService::Weather, true, "Refreshing",
                          F("Requesting current and daily weather from OpenWeather."), checkweather.retries);
    IotWebConf.suspend();
    int httpCode = 0;
    String reqdata;
    if (beginApiRequest(ApiEndpoint::Weather))
    {
      httpCode = networkService.client.GET();
      IotWebConf.resume();
      if (httpCode == 200)
      {
        reqdata = networkService.client.getString();
        ESP_LOGD(TAG, "Weather http response code: [%d] %s, payload length: [%d]", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.length());
        if (fillWeatherFromJson(reqdata))
        {
          checkweather.retries = 0;
          checkweather.complete = true;
          if (checkweather.firsttime)
            checkweather.firsttime = false;
          checkweather.lastsuccess = systemClock.getNow();
          ESP_LOGI(TAG, "New weather data received");
          noteDiagnosticSuccess(DiagnosticService::Weather, true, "Fresh data",
                                String(weather.current.description) + F(", feels like ") + weather.current.feelsLike +
                                    (imperial.isChecked() ? F("F") : F("C")),
                                checkweather.retries, httpCode);
        }
        else
          noteDiagnosticFailure(DiagnosticService::Weather, true, "Parse failed",
                                F("OpenWeather returned weather data, but the JSON payload could not be parsed cleanly."),
                                httpCode, checkweather.retries);
      }
      else if (httpCode == 401 || httpCode == 403 || httpCode == 404 || httpCode == 429)
      {
        reqdata = networkService.client.getString();
        ESP_LOGE(TAG, "OpenWeather weather request failed: [%d] %s | body: %s", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.c_str());
        noteDiagnosticFailure(DiagnosticService::Weather, true, "HTTP error",
                              String(F("OpenWeather weather request failed with ")) + httpCode + F(" ") + getHttpCodeName(httpCode),
                              httpCode, checkweather.retries);
        showready.apierror = true;
        strlcpy(showready.apierrorname, "openweather.org", sizeof(showready.apierrorname));
      }
      else
      {
        reqdata = networkService.client.getString();
        ESP_LOGE(TAG, "CheckWeather ERROR: [%d] %s | body: %s", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.c_str());
        noteDiagnosticFailure(DiagnosticService::Weather, true, "Request failed",
                              String(F("Weather request failed with ")) + httpCode + F(" ") + getHttpCodeName(httpCode),
                              httpCode, checkweather.retries);
      }
    }
    else
    {
      IotWebConf.resume();
      ESP_LOGE(TAG, "Failed to begin OpenWeather weather request");
      noteDiagnosticFailure(DiagnosticService::Weather, true, "Start failed",
                            F("The shared HTTP client could not begin the weather request."),
                            0, checkweather.retries);
    }
    endApiRequest();
    checkweather.lastattempt = systemClock.getNow();
  }
}

#ifndef DISABLE_ALERTCHECK
COROUTINE(checkAlerts) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(isHttpReady() && isNextShowReady(checkalerts.lastsuccess, alert_interval.value(), T1M) && isNextAttemptReady(checkalerts.lastattempt) && checkalerts.retries < HTTP_MAX_RETRIES && isCoordsValid());
    ESP_LOGI(TAG, "Checking weather alerts...");
    checkalerts.retries++;
    noteDiagnosticPending(DiagnosticService::Alerts, true, "Refreshing",
                          F("Checking weather.gov for active local alerts."), checkalerts.retries);
    IotWebConf.suspend();
    int httpCode = 0;
    String reqdata;
    if (beginApiRequest(ApiEndpoint::Alerts))
    {
      httpCode = networkService.client.GET();
      IotWebConf.resume();
      if (httpCode == 200)
      {
        reqdata = networkService.client.getString();
        ESP_LOGD(TAG, "Alerts http response code: [%d] %s, payload length: [%d]", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.length());
        checkalerts.retries = 0;
        if (fillAlertsFromJson(reqdata))
        {
          checkalerts.lastsuccess = systemClock.getNow();
          showready.alerts = true;
          noteDiagnosticSuccess(DiagnosticService::Alerts, true,
                                alerts.active ? "Active alerts" : "Clear",
                                alerts.active
                                    ? String(alerts.count) + F(" alert(s): ") + summarizeActiveAlerts()
                                    : String(F("weather.gov reports no active alerts.")),
                                checkalerts.retries, httpCode);
        }
        else
          noteDiagnosticFailure(DiagnosticService::Alerts, true, "Parse failed",
                                F("weather.gov returned alerts data, but the JSON payload could not be parsed cleanly."),
                                httpCode, checkalerts.retries);
      }
      else
      {
        reqdata = networkService.client.getString();
        ESP_LOGE(TAG, "Alerts ERROR: [%d] %s | body: %s", httpCode, getHttpCodeName(httpCode).c_str(), reqdata.c_str());
        noteDiagnosticFailure(DiagnosticService::Alerts, true, "Request failed",
                              String(F("weather.gov alert request failed with ")) + httpCode + F(" ") + getHttpCodeName(httpCode),
                              httpCode, checkalerts.retries);
      }
    }
    else
    {
      IotWebConf.resume();
      ESP_LOGE(TAG, "Failed to begin weather.gov alerts request");
      noteDiagnosticFailure(DiagnosticService::Alerts, true, "Start failed",
                            F("The shared HTTP client could not begin the weather.gov alerts request."),
                            0, checkalerts.retries);
    }
    endApiRequest();
    checkalerts.lastattempt = systemClock.getNow();
  }
}
#endif

COROUTINE(showDate) 
{
  COROUTINE_LOOP() 
  {
  COROUTINE_AWAIT((showready.date || isNextShowReady(lastshown.date, date_interval.value(), T1H)) && show_date.isChecked() && iotWebConf.getState() != 1 && displaytoken.isReady(1));
  displaytoken.setToken(1);
  lastshown.date = systemClock.getNow();
  getSystemZonedDateString(scrolltext.message, sizeof(scrolltext.message));
  startScroll(hex2rgb(date_color.value()), false);
  COROUTINE_AWAIT(!scrolltext.active);
  showready.date = false;
  displaytoken.resetToken(1);
  }
}

COROUTINE(showWeather) 
{
  COROUTINE_LOOP() 
  {
  COROUTINE_AWAIT((showready.testcurrentweather || ((showready.currentweather || isNextShowReady(lastshown.currentweather, current_weather_interval.value(), T1H)) && show_current_weather.isChecked())) && checkweather.complete && displaytoken.isReady(2));
  const bool testMode = showready.testcurrentweather;
  displaytoken.setToken(2);
  if (!testMode)
    lastshown.currentweather = systemClock.getNow();
  startFlash(hex2rgb(current_weather_color.value()), 1);
  COROUTINE_AWAIT(!alertflash.active);
  buildCurrentWeatherScrollText(scrolltext.message, sizeof(scrolltext.message));
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
  showready.currentweather = false;
  showready.testcurrentweather = false;
  displaytoken.resetToken(2);
  }
}

COROUTINE(showTemp)
{
  COROUTINE_LOOP()
  {
  COROUTINE_AWAIT((showready.testcurrenttemp || ((showready.currenttemp || isNextShowReady(lastshown.currenttemp, current_temp_interval.value(), T1M)) && show_current_temp.isChecked())) && checkweather.complete && displaytoken.isReady(2));
  const bool testMode = showready.testcurrenttemp;
  displaytoken.setToken(2);
  // Suspend the clock immediately so the temperature panel cannot race the
  // next clock redraw while it owns the display token.
  showClock.suspend();
  if (!testMode)
    lastshown.currenttemp = systemClock.getNow();
  cotimer.millis = millis();
  ESP_LOGI(TAG, "Showing Temperature");
  while (millis() - cotimer.millis < static_cast<uint32_t>(current_temp_duration.value()) * 1000U)
  {
    matrix->clear();
    display_temperature();
    display_showStatus();
    display_weatherIcon(weather.current.icon);
    matrix->show();
    COROUTINE_DELAY(50);
  }
  showready.currenttemp = false;
  showready.testcurrenttemp = false;
  displaytoken.resetToken(2);
  }
}

COROUTINE(showWeatherDaily) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT((showready.testdayweather || ((showready.dayweather || isNextShowReady(lastshown.dayweather, daily_weather_interval.value(), T1H)) && show_daily_weather.isChecked())) && checkweather.complete && displaytoken.isReady(8));
    const bool testMode = showready.testdayweather;
    displaytoken.setToken(8);
    if (!testMode)
      lastshown.dayweather = systemClock.getNow();
    startFlash(hex2rgb(daily_weather_color.value()), 1);
    memcpy(scrolltext.icon, weather.day.icon, sizeof(weather.day.icon[0]) * 4);
    COROUTINE_AWAIT(!alertflash.active);
    buildDailyWeatherScrollText(scrolltext.message, sizeof(scrolltext.message));
    startScroll(hex2rgb(daily_weather_color.value()), true);
    COROUTINE_AWAIT(!scrolltext.active);
    cotimer.millis = millis();
    showready.dayweather = false;
    showready.testdayweather = false;
    displaytoken.resetToken(8);
  }
}

COROUTINE(showAirquality) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT((showready.testaqi || ((showready.aqi || isNextShowReady(lastshown.aqi, aqi_interval.value(), T1M)) && show_aqi.isChecked())) && checkaqi.complete && displaytoken.isReady(9));
    const bool testMode = showready.testaqi;
    displaytoken.setToken(9);
    if (!testMode)
      lastshown.aqi = systemClock.getNow();
    // set color using lookup value and bit-shift if enable_aqi_color.isChecked() is false
    if (enable_aqi_color.isChecked())
      aqi.current.color = hex2rgb(aqi_color.value());
    else
      aqi.current.color = (AQIColorLookup[aqi.current.aqi]);
    startFlash(aqi.current.color, 1);
    COROUTINE_AWAIT(!alertflash.active);
    snprintf(scrolltext.message, 512, "Air Quality: %s, %s", air_quality[aqi.current.aqi], (aqi.current.description).c_str());
    startScroll(aqi.current.color, false);
    COROUTINE_AWAIT(!scrolltext.active);
    cotimer.millis = millis();
    showready.aqi = false;
    showready.testaqi = false;
    displaytoken.resetToken(9);
  }
}

//This code is used to display the NOAA weather alert information.
//It is only called when the alerts are active and the display is ready.
COROUTINE(showAlerts) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT((showready.alerts || showready.testalerts) && displaytoken.isReady(3));
    if (alerts.active)
    {
      const AlertEntry *selectedEntry = displayAlert();
      if (selectedEntry == nullptr)
      {
        showready.alerts = false;
      }
      else
      {
        displaytoken.setToken(3);
        strlcpy(scrolltext.message,
                hasVisibleText(selectedEntry->displayText) ? selectedEntry->displayText : selectedEntry->event,
                sizeof(scrolltext.message));
        startFlash(alertcolors[selectedEntry->watch ? 1 : 0], 3);
        COROUTINE_AWAIT(!alertflash.active);
        startScroll(alertflash.color, false);
        COROUTINE_AWAIT(!scrolltext.active);
        if (!showready.testalerts)
        {
          lastshown.alerts = systemClock.getNow();
          advanceAlertRotation();
        }
        displaytoken.resetToken(3);
      }
    }
    else if (showready.testalerts)
    {
      constexpr char kSampleAlertMessage[] = "Test Alert Tornado Warning Seek shelter now";
      displaytoken.setToken(3);
      strlcpy(scrolltext.message, kSampleAlertMessage, sizeof(scrolltext.message));
      startFlash(alertcolors[0], 3);
      COROUTINE_AWAIT(!alertflash.active);
      startScroll(alertflash.color, false);
      COROUTINE_AWAIT(!scrolltext.active);
      displaytoken.resetToken(3);
    }
    showready.alerts = false;
    showready.testalerts = false;
  }
}

COROUTINE(serialInput) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(500);
    if (Serial.available())
    {
      ConsoleMirrorPrint out(true);
      handleDebugCommand(Serial.read(), out, true);
    }
  }
}

COROUTINE(checkGeocode) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(isHttpReady() && isNextAttemptReady(checkgeocode.lastattempt) && isCoordsValid() && isApiValid(weatherapi.value()) && checkgeocode.ready && checkgeocode.retries < HTTP_MAX_RETRIES && !runtimeState.firstTimeFailsafe);
    ESP_LOGI(TAG, "Checking Geocode Location...");
    noteDiagnosticPending(DiagnosticService::Geocode, true, "Refreshing",
                          F("Resolving city and region details from the current coordinates."), checkgeocode.retries);
    IotWebConf.suspend();
    int httpCode = 0;
    if (beginApiRequest(ApiEndpoint::Geocode))
    {
      httpCode = networkService.client.GET();
      IotWebConf.resume();
      String payload;
      checkgeocode.retries++;
      if (httpCode == 200)
      {
        payload = networkService.client.getString();
        ESP_LOGD(TAG, "GEOcode http response code: [%d] %s, payload length: [%d]", httpCode, getHttpCodeName(httpCode).c_str(), payload.length());
        if (fillGeocodeFromJson(payload))
        {
          checkgeocode.lastsuccess = systemClock.getNow();
          checkgeocode.retries = 0;
          checkgeocode.ready = false;
          updateLocation();
        }
        else
          noteDiagnosticFailure(DiagnosticService::Geocode, true, "Parse failed",
                                F("OpenWeather returned reverse-geocode data, but the JSON payload could not be parsed cleanly."),
                                httpCode, checkgeocode.retries);
      }
      else if (httpCode == 401 || httpCode == 403 || httpCode == 404 || httpCode == 429)
      {
        payload = networkService.client.getString();
        ESP_LOGE(TAG, "OpenWeather geocode request failed: [%d] %s | body: %s", httpCode, getHttpCodeName(httpCode).c_str(), payload.c_str());
        noteDiagnosticFailure(DiagnosticService::Geocode, true, "HTTP error",
                              String(F("OpenWeather geocode request failed with ")) + httpCode + F(" ") + getHttpCodeName(httpCode),
                              httpCode, checkgeocode.retries);
        strlcpy(showready.apierrorname, "openweather.org", sizeof(showready.apierrorname));
        showready.apierror = true;
      }
      else
      {
        payload = networkService.client.getString();
        ESP_LOGE(TAG, "CheckGeocode ERROR: [%d] %s | body: %s", httpCode, getHttpCodeName(httpCode).c_str(), payload.c_str());
        noteDiagnosticFailure(DiagnosticService::Geocode, true, "Request failed",
                              String(F("Geocode request failed with ")) + httpCode + F(" ") + getHttpCodeName(httpCode),
                              httpCode, checkgeocode.retries);
      }
    }
    else
    {
      IotWebConf.resume();
      ESP_LOGE(TAG, "Failed to begin OpenWeather geocode request");
      noteDiagnosticFailure(DiagnosticService::Geocode, true, "Start failed",
                            F("The shared HTTP client could not begin the reverse-geocode request."),
                            0, checkgeocode.retries);
    }
    endApiRequest();
    checkgeocode.lastattempt = systemClock.getNow();
  }
}

#ifndef DISABLE_IPGEOCHECK
COROUTINE(checkIpgeo) 
{
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(!checkipgeo.complete && isHttpReady() && isNextAttemptReady(checkipgeo.lastattempt) && isApiValid(ipgeoapi.value()) && checkipgeo.retries < HTTP_MAX_RETRIES && !runtimeState.firstTimeFailsafe);
      ESP_LOGI(TAG, "Checking IP Geolocation...");
      noteDiagnosticPending(DiagnosticService::IpGeo, true, "Refreshing",
                            F("Resolving timezone and coarse coordinates from the public IP address."), checkipgeo.retries);
      IotWebConf.suspend();
      int httpCode = 0;
      if (beginApiRequest(ApiEndpoint::IpGeo))
      {
        httpCode = networkService.client.GET();
        IotWebConf.resume();
        String payload;
        checkipgeo.retries++;
        if (httpCode == 200)
        {
          payload = networkService.client.getString();
          ESP_LOGD(TAG, "IPGeo http response code: [%d] %s, payload length: [%d]", httpCode, getHttpCodeName(httpCode).c_str(), payload.length());
          if (fillIpgeoFromJson(payload))
          {
            checkipgeo.lastsuccess = systemClock.getNow();
            checkipgeo.complete = true;
            checkipgeo.retries = 0;
            updateCoords();
            checkgeocode.ready = true;
            processTimezone();
            noteDiagnosticSuccess(DiagnosticService::IpGeo, true, "Resolved",
                                  String(ipgeo.timezone) + F(" at ") + String(ipgeo.lat, 5) + F(", ") + String(ipgeo.lon, 5),
                                  checkipgeo.retries, httpCode);
          }
          else
          {
            ESP_LOGE(TAG, "fillIpgeoFromJson() Error parsing response");
            noteDiagnosticFailure(DiagnosticService::IpGeo, true, "Parse failed",
                                  F("ipgeolocation.io returned data, but the JSON payload could not be parsed cleanly."),
                                  httpCode, checkipgeo.retries);
          }
        }
        else if (httpCode == 401)
        {
          ESP_LOGE(TAG, "IPGeolocation.io Invalid API, error: [%d] %s", httpCode, getHttpCodeName(httpCode).c_str());
          noteDiagnosticFailure(DiagnosticService::IpGeo, true, "Invalid API key",
                                String(F("ipgeolocation.io request failed with ")) + httpCode + F(" ") + getHttpCodeName(httpCode),
                                httpCode, checkipgeo.retries);
          strlcpy(showready.apierrorname, "ipgeolocation.io", sizeof(showready.apierrorname));
          showready.apierror = true;
        }
        else
        {
          ESP_LOGE(TAG, "CheckIPGeo ERROR: [%d] %s", httpCode, getHttpCodeName(httpCode).c_str());
          noteDiagnosticFailure(DiagnosticService::IpGeo, true, "Request failed",
                                String(F("IP geolocation request failed with ")) + httpCode + F(" ") + getHttpCodeName(httpCode),
                                httpCode, checkipgeo.retries);
        }
      }
      else
      {
        IotWebConf.resume();
        ESP_LOGE(TAG, "Failed to begin ipgeolocation.io request");
        noteDiagnosticFailure(DiagnosticService::IpGeo, true, "Start failed",
                              F("The shared HTTP client could not begin the IP geolocation request."),
                              0, checkipgeo.retries);
      }
      endApiRequest();
      checkipgeo.lastattempt = systemClock.getNow();
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
      snprintf(scrolltext.message, sizeof(scrolltext.message), "Connect to LEDSMARTCLOCK wifi to setup, version: %s", VERSION_SEMVER);
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
  if (runtimeState.rebootRequested && (millis() - runtimeState.rebootRequestMillis) > 1000)
  {
    ESP_LOGW(TAG, "Restarting clock due to user request.");
    ESP.restart();
  }
  runtimeState.firstTimeFailsafe = (iotWebConf.getState() == 1);
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
    #ifndef DISABLE_WEATHERCHECK         
  // Weather couroutine management
  if ((show_current_temp.isChecked() || show_current_weather.isChecked() || show_daily_weather.isChecked()) && checkWeather.isSuspended() && isCoordsValid() && isApiValid(weatherapi.value()) && iotWebConf.getState() == 4 && checkweather.retries < 10)
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
  if (show_current_temp.isChecked() && showTemp.isSuspended())
  {
    showTemp.resume();
    ESP_LOGD(TAG, "Show Current Temp Coroutine Resumed");
  }
  else if ((!show_current_temp.isChecked() && !showTemp.isSuspended()))
  {
    showTemp.suspend();
    ESP_LOGD(TAG, "Show Current Temp Coroutine Suspended");
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
  if (checkipgeo.complete && !checkIpgeo.isSuspended())
  {
    checkIpgeo.suspend();    
    ESP_LOGD(TAG, "Check IPGeolocation Coroutine Suspended");
  }
  const acetime_t nowEpoch = systemClock.getNow();
  acetime_t sunriseEpoch = 0;
  acetime_t sunsetEpoch = 0;
  if (getActiveSunEvents(nowEpoch, sunriseEpoch, sunsetEpoch))
  {
    if (show_sunrise.isChecked() &&
        crossedEpochBoundary(runtimeState.lastSolarCheck, nowEpoch, sunriseEpoch) &&
        runtimeState.sunriseNotifiedEpoch != sunriseEpoch)
    {
      showready.sunrise = true;
      runtimeState.sunriseNotifiedEpoch = sunriseEpoch;
    }
    if (show_sunset.isChecked() &&
        crossedEpochBoundary(runtimeState.lastSolarCheck, nowEpoch, sunsetEpoch) &&
        runtimeState.sunsetNotifiedEpoch != sunsetEpoch)
    {
      showready.sunset = true;
      runtimeState.sunsetNotifiedEpoch = sunsetEpoch;
    }
  }
  runtimeState.lastSolarCheck = nowEpoch;
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
  current.brightavg = (current.brightness + current.lux + runtimeState.userBrightness) >> 1;
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
        if (!hasVisibleText(scrolltext.message)) {
            scrolltext.active = false;
            scrolltext.displayicon = false;
            scrolltext.position = mw - 1;
            scrolltext.size = 0;
            ESP_LOGW(TAG, "Scrolltext(): skipping empty scroll message");
        } else {
            displaytoken.setToken(10);
            scrolltext.size = strlen(scrolltext.message) * 6 - 1;
            ESP_LOGI(TAG, "Scrolltext(): Scrolling message - Chars:%d Width:%d \"%s\"", strlen(scrolltext.message), scrolltext.size, scrolltext.message);
        }
    }
    // Scroll and display message if position within bounds
    if (scrolltext.active && scrolltext.position >= -scrolltext.size) {
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
    } else if (scrolltext.active) {
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
    constexpr uint32_t kGpsNoDataLogIntervalMs = 15000;
    constexpr uint32_t kGpsAcquiringLogIntervalMs = 30000;
    uint32_t nowMillis = millis();
    size_t bytesRead = 0;
    while (Serial1.available() > 0)
    {
        processGpsSerialByte(Serial1.read());
        bytesRead++;
    }
    if (bytesRead > 0)
    {
        if (!gps.moduleDetected)
        {
            gps.moduleDetected = true;
            gps.firstByteMillis = nowMillis;
            ESP_LOGI(TAG, "GPS UART traffic detected after %lu ms. Parser is receiving NMEA data.", static_cast<unsigned long>(nowMillis));
            noteDiagnosticPending(DiagnosticService::Gps, true, "UART detected",
                                  F("GPS UART traffic is active. Waiting for satellites and a valid fix."));
        }
        gps.lastByteMillis = nowMillis;
        gps.packetdelay = 0;
        gps.lastNoDataLogMillis = nowMillis;
    }
    else if (gps.moduleDetected && gps.lastByteMillis > 0)
    {
        gps.packetdelay = static_cast<int32_t>(nowMillis - gps.lastByteMillis);
    }

    if (!gps.moduleDetected)
    {
        if (gps.lastNoDataLogMillis == 0 || (nowMillis - gps.lastNoDataLogMillis) >= kGpsNoDataLogIntervalMs)
        {
            ESP_LOGW(TAG, "GPS has not produced any UART traffic yet on RX:%d TX:%d @ %lu baud. Check module power, wiring, and baud rate.",
                     GPS_RX_PIN, GPS_TX_PIN, static_cast<unsigned long>(gpsActiveBaud()));
            noteDiagnosticPending(DiagnosticService::Gps, true, "Waiting for UART",
                                  String(F("No GPS UART traffic has been seen yet on RX ")) + GPS_RX_PIN + F(" / TX ") + GPS_TX_PIN +
                                      F(" at ") + gpsActiveBaud() + F(" baud."));
            gps.lastNoDataLogMillis = nowMillis;
        }
    }
    else if (gps.packetdelay >= static_cast<int32_t>(kGpsNoDataLogIntervalMs)
             && (gps.lastNoDataLogMillis == 0 || (nowMillis - gps.lastNoDataLogMillis) >= kGpsNoDataLogIntervalMs))
    {
        ESP_LOGW(TAG, "GPS UART traffic stalled for %ld ms. Parsed chars:%lu passed:%lu failed:%lu",
                 static_cast<long>(gps.packetdelay),
                 static_cast<unsigned long>(GPS.charsProcessed()),
                 static_cast<unsigned long>(GPS.passedChecksum()),
                 static_cast<unsigned long>(GPS.failedChecksum()));
        noteDiagnosticFailure(DiagnosticService::Gps, true, "UART stalled",
                              String(F("GPS UART traffic stalled for ")) + gps.packetdelay + F(" ms."),
                              0, 0);
        gps.lastNoDataLogMillis = nowMillis;
    }

    if (GPS.satellites.isUpdated())
    {
        gps.sats = GPS.satellites.value();
        if (gps.sats != gps.lastReportedSats)
        {
            gps.lastReportedSats = gps.sats;
            if (gps.sats == 0)
                ESP_LOGI(TAG, "GPS reports no satellites in view.");
            else
                ESP_LOGI(TAG, "GPS satellites updated: %u visible.", gps.sats);
        }
    }
    if (GPS.location.isUpdated()) 
    {
        gps.lat = GPS.location.lat();
        gps.lon = GPS.location.lng();
        gps.lockage = systemClock.getNow();
        updateCoords();
        if (enable_fixed_loc.isChecked())
            ESP_LOGD(TAG, "GPS location updated: Lat: %.6f Lon: %.6f | custom coordinates remain active for services", gps.lat, gps.lon);
        else
            ESP_LOGI(TAG, "GPS location updated: Lat: %.6f Lon: %.6f | Sats:%u | HDOP:%d", gps.lat, gps.lon, gps.sats, gps.hdop);
    }
    if (GPS.altitude.isUpdated()) 
    {
        gps.elevation = GPS.altitude.feet();
        ESP_LOGD(TAG, "GPS elevation updated: %d feet", gps.elevation);
    }
    if (GPS.hdop.isUpdated()) 
    {
        gps.hdop = GPS.hdop.hdop();
        ESP_LOGD(TAG, "GPS HDOP updated: %d", gps.hdop);
    }

    bool hasFix = GPS.location.isValid() && gps.sats > 0;
    if (hasFix && !gps.fix)
    {
        gps.fix = true;
        gps.waitingForFix = false;
        gps.timestamp = systemClock.getNow();
        gps.lastProgressLogMillis = nowMillis;
        ESP_LOGI(TAG, "GPS fix acquired: Sats:%u | Lat:%.6f | Lon:%.6f | TimeValid:%s | LocationAge:%lu ms",
                 gps.sats, gps.lat, gps.lon, yesno[GPS.time.isValid()],
                 static_cast<unsigned long>(GPS.location.age()));
        noteDiagnosticSuccess(DiagnosticService::Gps, true, "Fix acquired",
                              String(gps.sats) + F(" sats, ") + String(gps.lat, 5) + F(", ") + String(gps.lon, 5));
        if (!enable_fixed_tz.isChecked())
          processTimezone();
    }
    else if (!hasFix && gps.fix)
    {
        gps.fix = false;
        gps.waitingForFix = gps.sats > 0;
        gps.lastProgressLogMillis = nowMillis;
        ESP_LOGW(TAG, "GPS fix lost. Sats:%u | LocationValid:%s | TimeValid:%s",
                 gps.sats, yesno[GPS.location.isValid()], yesno[GPS.time.isValid()]);
        noteDiagnosticFailure(DiagnosticService::Gps, true, "Fix lost",
                              String(F("GPS previously had a fix but lost it. Satellites visible: ")) + gps.sats,
                              0, 0);
    }
    else if (!hasFix && gps.moduleDetected
             && (gps.lastProgressLogMillis == 0 || (nowMillis - gps.lastProgressLogMillis) >= kGpsAcquiringLogIntervalMs))
    {
        gps.waitingForFix = true;
        gps.lastProgressLogMillis = nowMillis;
        if (gps.sats > 0)
        {
            ESP_LOGI(TAG, "GPS is receiving satellite data but has no valid lock yet. Sats:%u | LocationValid:%s | TimeValid:%s | Chars:%lu",
                     gps.sats, yesno[GPS.location.isValid()], yesno[GPS.time.isValid()],
                     static_cast<unsigned long>(GPS.charsProcessed()));
            noteDiagnosticPending(DiagnosticService::Gps, true, "Acquiring fix",
                                  String(F("Satellites visible: ")) + gps.sats + F(". The receiver is still waiting for a valid lock."));
        }
        else
        {
            ESP_LOGI(TAG, "GPS module detected and parser is active, but no satellites are locked yet. Chars:%lu | Passed:%lu | Failed:%lu",
                     static_cast<unsigned long>(GPS.charsProcessed()),
                     static_cast<unsigned long>(GPS.passedChecksum()),
                     static_cast<unsigned long>(GPS.failedChecksum()));
            noteDiagnosticPending(DiagnosticService::Gps, true, "Searching satellites",
                                  String(F("The GPS receiver is active, but no satellites are locked yet. Parsed chars: ")) +
                                      formatLargeNumber(GPS.charsProcessed()));
        }
    }
    COROUTINE_DELAY(1000);
}
}

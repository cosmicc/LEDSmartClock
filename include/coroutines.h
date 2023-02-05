// Coroutines

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
    httpRequest(3);
    checkaqi.lastattempt = systemClock.getNow();
    checkaqi.active = false;
  }
}

COROUTINE(checkWeather) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(abs(systemClock.getNow() - checkweather.lastsuccess) > (show_weather_interval.value() * T1M) && abs(systemClock.getNow() - checkweather.lastattempt) > T1M && iotWebConf.getState() == 4 && (current.lat).length() > 1 && (weatherapi.value())[0] != '\0' && displaytoken.isReady(0) && !firsttimefailsafe);
    ESP_LOGI(TAG, "Checking weather forecast...");
    checkweather.retries++;
    checkweather.jsonParsed = false;
    checkweather.success = false;
    httpRequest(0);
    checkweather.lastattempt = systemClock.getNow();
    checkweather.send = false;
  }
}

#ifndef DISABLE_ALERTCHECK
COROUTINE(checkAlerts) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(abs(systemClock.getNow() - checkalerts.lastsuccess) > (5 * T1M) && (systemClock.getNow() - checkalerts.lastattempt) > T1M && iotWebConf.getState() == 4 && (current.lat).length() > 1 && displaytoken.isReady(0));
    ESP_LOGI(TAG, "Checking weather alerts...");
    checkalerts.retries++;
    checkalerts.jsonParsed = false;
    httpRequest(1);
    checkalerts.lastattempt = systemClock.getNow();
    checkalerts.send = false;
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

COROUTINE(serialInput) {
  COROUTINE_LOOP() {
    COROUTINE_AWAIT(serialdebug.isChecked() && Serial.available());
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
          cotimer.show_airquality_ready = true;
          break;
        case 'w':
          cotimer.show_weather_ready = true;
          break;
        case 'e':
          cotimer.show_date_ready = true;
          break;
        case 'q':
          cotimer.show_weather_daily_ready = true;
          break;
        }
  }
}

COROUTINE(checkGeocode) {
  COROUTINE_LOOP() {
  COROUTINE_AWAIT(checkgeocode.active && checkipgeo.complete && abs(systemClock.getNow() - checkgeocode.lastattempt) > T1M && iotWebConf.getState() == 4 && (current.lat).length() > 1 && (weatherapi.value())[0] != '\0' && displaytoken.isReady(0) && !firsttimefailsafe && urls[3][0] != '\0');
  ESP_LOGI(TAG, "Checking Geocode Location...");
  checkgeocode.retries = 1;
  checkgeocode.jsonParsed = false;
  httpRequest(2);
  checkgeocode.retries++;
  checkgeocode.lastattempt = systemClock.getNow();
  checkgeocode.active = false;
  }
}

#ifndef DISABLE_IPGEOCHECK
COROUTINE(checkIpgeo) {
  COROUTINE_BEGIN();
  COROUTINE_AWAIT(iotWebConf.getState() == 4 && (ipgeoapi.value())[0] != '\0' && displaytoken.isReady(0) && !firsttimefailsafe && urls[4][0] != '\0');
    COROUTINE_AWAIT(abs(systemClock.getNow() - checkipgeo.lastattempt) > T1M);
    ESP_LOGI(TAG, "Checking IP Geolocation...");
    checkipgeo.active = true;
    checkipgeo.retries = 1;
    checkipgeo.jsonParsed = false;
    httpRequest(4);
    checkipgeo.lastattempt = systemClock.getNow();
    checkipgeo.active = false;
    COROUTINE_END();
}
#endif

COROUTINE(systemMessages) {
  COROUTINE_LOOP() 
  {
    COROUTINE_AWAIT(iotWebConf.getState() == 1 || resetme || scrolltext.showingip || scrolltext.showingloc);
    COROUTINE_AWAIT((millis() - scrolltext.resetmsgtime) > T1M*1000 && displaytoken.isReady(4));
    if (scrolltext.showingloc) 
    {
      displaytoken.setToken(4);
      scrolltext.message = (String)"";
      scrolltext.color = GREEN;
      scrolltext.active = true;
      COROUTINE_AWAIT(!scrolltext.active);
      scrolltext.showingcfg = false;
      displaytoken.resetToken(4);
    }
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
  if (serialdebug.isChecked() && serialInput.isSuspended() && displaytoken.isReady(0))
    serialInput.resume();
  else if (!serialdebug.isChecked() && !serialInput.isSuspended())
    serialInput.suspend();
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
    if (show_airquality.isChecked() && showAirquality.isSuspended() && iotWebConf.getState() != 1 && displaytoken.isReady(0))
    showAirquality.resume();
  else if ((!show_airquality.isChecked() && !showAirquality.isSuspended()) || iotWebConf.getState() == 1)
    showAirquality.suspend();
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
void handleRoot()
{
  if (iotWebConf.handleCaptivePortal())
    return;
  acetime_t now = systemClock.getNow();
  String s;
  if (iotWebConf.getState() == 1) 
  {
    s = F("<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>");
    s += F("<style>body { background-color: #222222; font-family: Verdana, sans-serif; }</style>");
    s += F("</head><body>");
    s += F("<style>a:link {color: #11ffff;background-color: transparent;text-decoration: none;} a:visited {color: #11ffff;background-color: transparent;text-decoration: none;} a:hover {color: #11ffff;background-color: transparent;text-decoration: underline;} a:active {color: #11ffff;background-color: transparent;text-decoration: underline;}</style>");
    s += F("<span style=\"color: #ffffff; background-color: #222222;\"><table style=\"margin: auto;\">");
    s += F("<tr><td style=\"text-align: center;\"><a title=\"Configure Settings\" href=\"config\">->[ Setup LEDSmartClock ]<-</a></td></tr>");
    s += F("<tr><td style=\"text-align: center;\"><br/></td></tr>");
    s += F("<tr><td style=\"text-align: center;\">Welcome to LEDSmartClock. This is the first-time setup process. You will need to provide the following information for the clock to connect to WiFi:</td></tr>");
    s += F("<tr><td style=\"text-align: center;\"><br/></td></tr>");
    s += F("<tr><td style=\"text-align: center;\"><ul><li>WiFi SSID and password</li><li>New password (for admin login)</li><li>Password for the WiFi when the clock is in AP mode</li></ul></td></tr>");
    s += F("<tr><td style=\"text-align: center;\"><br/></td></tr>");
    s += F("<tr><td style=\"text-align: center;\">Once you provide this information, the clock will connect to WiFi, and you can continue the configuration from the web interface using the IP that scrolls across the clock. Click 'Setup LEDSmartClock' to continue.</td></tr>");
    s += F("<tr><td style=\"text-align: center;\"><br/></td></tr>");
    s += F("<tr><td style=\"text-align: center;\"><a title=\"Configure Settings\" href=\"config\">->[ Setup LEDSmartClock ]<-</a></td></tr></table>");
    s += F("</body></html>\n");
  }
  else 
  {
    s = F("<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><body style=\"background-color:#222222; font-family: Verdana, sans-serif;\">");
    s += F("<style>a:link {color: #11ffff;background-color: transparent;text-decoration: none;} a:visited {color: #11ffff;background-color: transparent;text-decoration: none;} a:hover {color: #11ffff;background-color: transparent;text-decoration: underline;} a:active {color: #11ffff;background-color: transparent;text-decoration: underline;}</style>");
    s += F("<span style=\"color: #ffffff; background-color: #222222;\"><table style=\"font-size: 14px; width: 500px; margin-left: auto; margin-right: auto; height: 22px;\"><tbody><tr><td style=\"width: 500px;\"><blockquote><h2 style=\"text-align: center;\"><span style=\"color: #aa11ff;\">LED Smart Clock</span><p style=\"text-align: center;\"></h2><span style=\"color: #ffff99;\"><h3><em><strong>");
    s += getSystemZonedDateTimeString();
    s += F("</strong></h3></em></span></p></blockquote><p style=\"text-align: center;\"><a title=\"Configure Settings\" href=\"config\">->[ Configure Settings ]<-</a></p></td></tr></tbody></table>");
    if (!isApiValid(ipgeoapi.value()))
      s += F("<blockquote><h2 style=\"text-align: center;\"><span style=\"color: #aa11ff;\">*** You must enter a valid https://ipgeolocation.io API key on the configuration page! ***</span></h2></p></blockquote>");
    if (!isApiValid(weatherapi.value()))
      s += F("<blockquote><h2 style=\"text-align: center;\"><span style=\"color: #aa11ff;\">*** You must enter a valid https://openweathermap.org API key on the configuration page! ***</span></h2></p></blockquote>"); 
    s += F("<table style=\"width: 518px; height: 869px; margin-left: auto; margin-right: auto;\" border=\"0\"><tbody><tr style=\"height: 20px;\"><td style=\"height: 20px; text-align: right; width: 247px;\">Firmware Version:</td><td style=\"height: 20px; width: 255px;\">");
    s += (String)VERSION_MAJOR + "." + VERSION_MINOR + "." + VERSION_PATCH;
    s += F("</td></tr><tr style=\"height: 20px;\"><td style=\"height: 20px; text-align: right; width: 247px;\">Uptime:</td><td style=\"height: 20px; width: 255px;\">");
    s += elapsedTime(now - bootTime);
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Time Source:</td><td style=\"height: 2px; width: 255px;\">");
    toUpper(timesource);
    s += timesource;
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current LED Brightness:</td><td style=\"height: 2px; width: 255px;\">");
    s += map(current.brightness, (5+userbrightness), (LUXMAX + userbrightness), 1, 100);
    s += F("/100</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Timezone Offset:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)current.tzoffset;
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Last Time Sync:</td><td style=\"height: 2px; width: 255px;\">");
    s += elapsedTime(now - systemClock.getLastSyncTime());
    s += F(" ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Latitude:</td><td style=\"height: 2px; width: 255px;\">");
    s += current.lat;
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Longitude:</td><td style=\"height: 2px; width: 255px;\">");
    s += current.lon;
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Location Source:</td><td style=\"height: 2px; width: 255px;\">");
    s += current.locsource;
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Location:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)geocode.city + ", " + geocode.state + ", " + geocode.country;
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">&nbsp;</td><td style=\"height: 2px; width: 255px;\">&nbsp;</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Fix:</td><td style=\"height: 2px; width: 255px;\">");
    if (gps.fix)
      s += F("Yes");
    else
      s += F("No");
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Aquired Sattelites:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)gps.sats;
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Fix Age:</td><td style=\"height: 2px; width: 255px;\">");
    s += elapsedTime(now - gps.timestamp);
    s += F(" ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Location Age:</td><td style=\"height: 2px; width: 255px;\">");
    s += elapsedTime(now - gps.lockage);
    s += F(" ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Precision:</td><td style=\"height: 2px; width: 255px;\">");
    s += gps.hdop;
    s += F(" meters</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Elevation:</td><td style=\"height: 2px; width: 255px;\">");
    s += gps.elevation;
    s += F(" meters</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Total Events:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)GPS.charsProcessed();
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Passed Events:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)GPS.passedChecksum();
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Failed Events:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)GPS.failedChecksum();
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">GPS Fix Events:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)GPS.sentencesWithFix();
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">&nbsp;</td><td style=\"height: 2px; width: 255px;\">&nbsp;</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Temp:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)weather.current.temp;
    if (imperial.isChecked())
      s += F("&#8457;");
    else
      s += F("&#8451;");
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Feels Like Temp:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)weather.current.feelsLike;
      if (imperial.isChecked())
      s += F("&#8457;");
    else
      s += F("&#8451;");
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Humidity:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)weather.current.humidity;
    s += F("%</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Wind Speed:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)weather.current.windSpeed;
    s += F("&nbsp;");
    if (imperial.isChecked())
      s += imperial_units[1];
    else
      s += metric_units[1];
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Current Conditions:</td><td style=\"height: 2px; width: 255px;\">");
    s += capString(weather.current.description);
    
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Last Forcast Check Attempt:</td><td style=\"height: 2px; width: 255px;\">");
    s += elapsedTime(now - checkweather.lastattempt);
    s += F(" ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Last Forcast Check Success:</td><td style=\"height: 2px; width: 255px;\">");
    s += elapsedTime(now - checkweather.lastsuccess);
    s += F(" ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Next Forecast Display: in </td><td style=\"height: 2px; width: 255px;\">");
    s += elapsedTime((now - lastshown.currentweather) - (current_weather_interval.value() * 60));
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">&nbsp;</td><td style=\"height: 2px; width: 255px;\">&nbsp;</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Alert Active:</td><td style=\"height: 2px; width: 255px;\">");
    if (alerts.active)
      s += F("Yes");
    else
      s += F("No");
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Warnings:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)alerts.inWarning;
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Watches:</td><td style=\"height: 2px; width: 255px;\">");
    s += (String)alerts.inWatch;
    s += F("</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Alerts Last Attempt:</td><td style=\"height: 2px; width: 255px;\">");
    s += elapsedTime(now - checkalerts.lastattempt);
    s += F(" ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Weather Alerts Last Success:</td><td style=\"height: 2px; width: 255px;\">");
    s += elapsedTime(now - checkalerts.lastsuccess);
    s += F(" ago</td></tr><tr style=\"height: 2px;\"><td style=\"height: 2px; text-align: right; width: 247px;\">Last Weather Alert Shown:</td><td style=\"height: 2px; width: 255px;\">");
    s += elapsedTime(now - lastshown.alerts);
    s += F(" ago</td></tr></tbody></table>");
    s += F("</span></body></html>\n");
  }
  server.send(200, "text/html", s);
}

bool formValidator(iotwebconf::WebRequestWrapper* webRequestWrapper)
{
  ESP_LOGD(TAG, "Validating web form...");
  bool valid = true;
  //int l = webRequestWrapper->arg(ipgeoapi.getId()).length();
  //if (l != 32 || l != 0)
  //{
  //  ipgeoapi.errorMessage = "IPGeo API needs to be 32 characters long";
  //  valid = false;
 // }
  return valid;
}
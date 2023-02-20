// WARNING: Not advancing the config version in main.h after adding/deleting iotwebconf config options will result in system settings data corruption.  Iotwebconf will erase the config if it sees a different config version to avoid this corruption.

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, VERSION_CONFIG);
// SYSTEM GROUP
  iotwebconf::TextTParameter<12> savedlat =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("savedlat").label("Saved Latitude").defaultValue("0").build();
  iotwebconf::TextTParameter<12> savedlon =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("savedlon").label("Saved Longitude").defaultValue("0").build();
  iotwebconf::IntTParameter<int8_t> savedtzoffset =   
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("tzoffset").label("Saved TZ Offset").defaultValue(0).min(-12).max(12).step(1).placeholder("-12...12").build();
  iotwebconf::TextTParameter<32> savedcity =
    iotwebconf::Builder<iotwebconf::TextTParameter<32>>("savedcity").label("Saved City").defaultValue("0").build();
  iotwebconf::TextTParameter<32> savedstate =
    iotwebconf::Builder<iotwebconf::TextTParameter<32>>("savedstate").label("Saved State").defaultValue("0").build();
  iotwebconf::TextTParameter<32> savedcountry =
    iotwebconf::Builder<iotwebconf::TextTParameter<32>>("savedcountry").label("Saved Country").defaultValue("0").build();
  iotwebconf::CheckboxTParameter resetdefaults =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("resetdefaults").label("Reset to Defaults (AP mode)").defaultValue(false).build();
  iotwebconf::CheckboxTParameter serialdebug =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("serialdebug").label("Enable serial debug output (for debugging)").defaultValue(false).build();
  iotwebconf::TextTParameter<33> ipgeoapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("ipgeoapi").label("IPGeolocation.io API key").defaultValue("").build();
  iotwebconf::TextTParameter<33> weatherapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("weatherapi").label("Openweathermap.org API Key").defaultValue("").build();

// GROUP 1 DISPLAY CONFIG
iotwebconf::ParameterGroup group1 = iotwebconf::ParameterGroup("Display", "Display Settings");
  iotwebconf::CheckboxTParameter imperial =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("imperial").label("Use imperial units (Instead of metric)").defaultValue(true).build();
  iotwebconf::IntTParameter<int8_t> brightness_level =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("brightness_level").label("Brightness level (1-5)").defaultValue(DEF_BRIGHTNESS_LEVEL).min(1).max(5).step(1).placeholder("1(low)..5(high)").build();
  iotwebconf::IntTParameter<int8_t> text_scroll_speed =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("text_scroll_speed").label("Text scroll speed (1-10)").defaultValue(DEF_SCROLL_SPEED).min(1).max(10).step(1).placeholder("1(low)..10(high)").build();
  iotwebconf::ColorTParameter system_color =
    iotwebconf::Builder<iotwebconf::ColorTParameter>("system_color").label("Choose system messages text color").defaultValue(DEF_SYSTEM_COLOR).build();
  iotwebconf::CheckboxTParameter show_date =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_date").label("Display the current date").defaultValue(true).build();
  iotwebconf::ColorTParameter date_color =
    iotwebconf::Builder<iotwebconf::ColorTParameter>("date_color").label("Choose date color").defaultValue(DEF_DATE_COLOR).build();
  iotwebconf::IntTParameter<int8_t> date_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("date_interval").label("Display date interval in hours (1-24)").defaultValue(DEF_DATE_INTERVAL).min(1).max(24).step(1).placeholder("1..24(hours)").build();
  iotwebconf::CheckboxTParameter enable_alertflash =  
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_alertflash").label("Enable screen flashes before notifications").defaultValue(true).build();

//GROUP 2 CLOCK CONFIG
iotwebconf::ParameterGroup group2 = iotwebconf::ParameterGroup("Clock", "Clock Settings");
  iotwebconf::CheckboxTParameter twelve_clock=
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("twelve_clock").label("Use 12 Hour Clock").defaultValue(true).build();
  iotwebconf::CheckboxTParameter enable_fixed_tz =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_fixed_tz").label("Use custom timezone (Disables auto timezone)").defaultValue(false).build();
  iotwebconf::IntTParameter<int8_t> fixed_offset =   
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("fixed_offset").label("Custom timezone GMT offset hours").defaultValue(0).min(-12).max(12).step(1).placeholder("-12...12").build();
  iotwebconf::CheckboxTParameter colonflicker =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("colonflicker").label("Enable clock colon flash").defaultValue(true).build();
  iotwebconf::CheckboxTParameter flickerfast =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("flickerfast").label("Fast clock colon flash (Only works if \"enable colon flash\" is enabled above)").defaultValue(false).build();
  iotwebconf::CheckboxTParameter enable_clock_color=
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_clock_color").label("Use custom clock color (Disables auto color)").defaultValue(false).build();
  iotwebconf::ColorTParameter clock_color =
    iotwebconf::Builder<iotwebconf::ColorTParameter>("clock_color").label("Custom clock color").defaultValue(DEF_CLOCK_COLOR).build();

// GROUP 3 CURRENT WEATHER CONFIG
iotwebconf::ParameterGroup group3 = iotwebconf::ParameterGroup("CurrentWeather", "Current Weather");
  iotwebconf::CheckboxTParameter enable_temp_color =  
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_temp_color").label("Use custom temperature color (Disables auto color)").defaultValue(false).build();
  iotwebconf::ColorTParameter temp_color =
    iotwebconf::Builder<iotwebconf::ColorTParameter>("temp_color").label("Custom temperature color").defaultValue(DEF_TEMP_COLOR).build();
  iotwebconf::CheckboxTParameter show_current_weather =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_current_weather").label("Display current weather conditions").defaultValue(true).build();
  iotwebconf::ColorTParameter current_weather_color =
    iotwebconf::Builder<iotwebconf::ColorTParameter>("current_weather_color").label("Current conditions text color").defaultValue(DEF_WEATHER_COLOR).build();
  iotwebconf::IntTParameter<int8_t> current_weather_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("current_weather_interval").label("Current conditions display interval in minutes (1-120)").defaultValue(DEF_WEATHER_INTERVAL).min(1).max(120).step(1).placeholder("1(min)..60(min)").build();

// GROUP 4 DAY WEATHER CONFIG
iotwebconf::ParameterGroup group4 = iotwebconf::ParameterGroup("DailyWeather", "Daily Weather");
  iotwebconf::CheckboxTParameter show_daily_weather =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_daily_weather").label("Display daily weather conditions").defaultValue(true).build();
  iotwebconf::ColorTParameter daily_weather_color =
    iotwebconf::Builder<iotwebconf::ColorTParameter>("daily_weather_color").label("Daily conditions text color").defaultValue(DEF_DAILY_COLOR).build();
  iotwebconf::IntTParameter<int8_t> daily_weather_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("daily_weather_interval").label("Daily conditions display interval in hours (1-24)").defaultValue(DEF_DAILY_INTERVAL).min(1).max(24).step(1).placeholder("1(hour)..24(hours)").build();

// GROUP 5 AIR QUALITY CONFIG
iotwebconf::ParameterGroup group5 = iotwebconf::ParameterGroup("AQI", "Air Quality");
  iotwebconf::CheckboxTParameter show_aqi =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_aqi").label("Display air quality details").defaultValue(true).build();
  iotwebconf::ColorTParameter aqi_color =
    iotwebconf::Builder<iotwebconf::ColorTParameter>("aqi_color").label("Custom air quality color").defaultValue(DEF_AQI_COLOR).build();
  iotwebconf::CheckboxTParameter enable_aqi_color =  
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_aqi_color").label("Use custom air quality color (Disables auto color)").defaultValue(false).build();
  iotwebconf::IntTParameter<int8_t> aqi_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("aqi_interval").label("Air quality display interval in minutes (1-120)").defaultValue(DEF_AQI_INTERVAL).min(1).max(120).step(1).placeholder("1(min)..120(min)").build();
  iotwebconf::IntTParameter<int8_t> alert_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("alert_interval").label("Weather alert display interval in minutes (1-60)").defaultValue(DEF_ALERT_INTERVAL).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();

// GROUP 6 STATUS LEDS
iotwebconf::ParameterGroup group6 = iotwebconf::ParameterGroup("Status", "Status LEDs");
  iotwebconf::CheckboxTParameter enable_system_status =  
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_system_status").label("Enable system status LED (Bottom left)").defaultValue(true).build();
  iotwebconf::CheckboxTParameter enable_aqi_status =  
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_aqi_status").label("Enable Air Quality status LED (Top Left)").defaultValue(true).build();
  iotwebconf::CheckboxTParameter enable_uvi_status =  
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_uvi_status").label("Enable UV Index status LED (Top Right)").defaultValue(true).build();
  iotwebconf::CheckboxTParameter green_status =  
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("green_status").label("Use green instead of black/off for good/low status leds").defaultValue(false).build();

// GROUP 7 SUN CONFIG
iotwebconf::ParameterGroup group7 = iotwebconf::ParameterGroup("Sun", "Sunrise/Sunset");
  iotwebconf::CheckboxTParameter show_sunrise =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_sunrise").label("Display message on sunrise").defaultValue(true).build();
  iotwebconf::ColorTParameter sunrise_color =
    iotwebconf::Builder<iotwebconf::ColorTParameter>("sunrise_color").label("Sunrise message color").defaultValue(DEF_AQI_COLOR).build();
  iotwebconf::TextTParameter<128> sunrise_message =
    iotwebconf::Builder<iotwebconf::TextTParameter<128>>("sunrise_message").label("Message to display at sunrise").defaultValue("Good morning, the sun has risen").build();
  iotwebconf::CheckboxTParameter show_sunset =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_sunset").label("Display message on sunset").defaultValue(true).build();
  iotwebconf::ColorTParameter sunset_color =
    iotwebconf::Builder<iotwebconf::ColorTParameter>("sunset_color").label("Sunset message color").defaultValue(DEF_AQI_COLOR).build();
  iotwebconf::TextTParameter<128> sunset_message =
    iotwebconf::Builder<iotwebconf::TextTParameter<128>>("sunset_message").label("Message to display at sunset").defaultValue("Good evening, the sun has set").build();

// GROUP 8 LOCATION CONFIG
iotwebconf::ParameterGroup group8 = iotwebconf::ParameterGroup("Location", "Location Settings");
  iotwebconf::CheckboxTParameter show_loc_change =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_loc_change").label("Display new location on major location change").defaultValue(true).build();
  iotwebconf::CheckboxTParameter enable_fixed_loc =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_fixed_loc").label("Use custom location (Disables auto location)").defaultValue(false).build();
  iotwebconf::TextTParameter<12> fixedLat =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLat").label("Custom latitude").defaultValue("").build();
  iotwebconf::TextTParameter<12> fixedLon =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLon").label("Custom longitude").defaultValue("").build();

// IotWebConf User custom settings reference
// String savedlat;
// String savedlon;
// int8_t savedtzoffset;
// String savedcity;
// String savedstate;
// String savedcountry;
// bool debugserial
// String weatherapi;                  // OpenWeather API Key
// String ipgeoapi                     // IP Geolocation API

// GROUP 1 (Display)
// uint8_t brightness_level;           // 1 low - 3 high
// uint8_t text_scroll_speed;          // 1 slow - 10 fast
// color systemcolor;                  // system messages color
// bool imperial                       // use imperial units
// bool enable_alertflash              // disable alertflash
// bool show_date;                     // show date
// color datecolor                     // date color
// int8_t show_date_interval;          // show date interval in hours
// uint8_t alert_show_interval;        // Time between weather alert displays

// GROUP 2 (Clock)
// bool twelve_clock                   // enable 12 hour clock
// bool colonflicker;                  // flicker the colon ;)
// bool flickerfast;                   // flicker fast
// bool use_fixed_clockcolor           // use fixed clock color
// color fixed_clockcolor              // fixed clock color
// bool used_fixed_tz;                 // Use fixed Timezone
// String fixedTz;                     // Fixed Timezone

// GROUP 3 (Curent Weather)
// bool show_current_weather;          // Show current weather toggle
// color weather_color                 // Weather text color
// bool use_fixed_tempcolor            // used fixed temp color
// color fixed_tempcolor               // fixed temp color
// uint8_t weather_show_interval;      // Time between current weather displays

// GROUP 4 (Day Weather)
// bool show_weather_daily
// color weather_color_daily           // Weather text color
// uint8_t weather_show_daily_interval;// Time between day weather displays

// GROUP 5 (Air Quality)
// bool show_airquality                // display aqi details
// bool use_fixed_aqicolor
// color airquality_color              // aqi detail text color
// uint8_t airquality_show_interval;   // Time between aqi detail displays

// GROUP 6 (Status LEDS)
// bool enable_system_status            // enable status bottom left
// bool enable_aqi_status               // enable aqi top left
// bool enable_uvi_status               // enable uvi top right
// bool green_status                    // enable green for good/low status (istead of off/black)

// GROUP 7 (Sunrise/Sunset)
// bool enable_sunrise
// color sunrise_color
// char sunrise message
// bool enable_sunset
// color sunset_color
// char sunset message


// GROUP 8 (Location)
// bool display_loc_change;            // Display location on major change
// bool used_fixed_loc;                // Use fixed GPS coordinates
// String fixedLat;                    // Fixed GPS latitude
// String fixedLon;                    // Fixed GPS Longitude


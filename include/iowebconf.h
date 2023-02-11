// IotWebConf User custom settings reference
// String savedlat;
// String savedlon;
// int8_t savedtzoffset;
// String savedcity;
// String savedstate;
// String savedcountry;
// bool debugserial

// Group 1 (Display)
// uint8_t brightness_level;           // 1 low - 3 high
// uint8_t text_scroll_speed;          // 1 slow - 10 fast
// bool show_date;                     // show date
// color datecolor                     // date color
// int8_t show_date_interval;          // show date interval in hours
// bool enable_status                 // disable status corner led
// bool enable_alertflash             // disable alertflash

// Group 2 (Clock)
// bool twelve_clock                   // enable 12 hour clock
// bool used_fixed_tz;                 // Use fixed Timezone
// String fixedTz;                     // Fixed Timezone
// bool colonflicker;                  // flicker the colon ;)
// bool flickerfast;                   // flicker fast
// bool use_fixed_clockcolor           // use fixed clock color
// color fixed_clockcolor              // fixed clock color

// Group 3 (Weather)

// bool imperial                       // use imperail units
// bool use_fixed_tempcolor            // used fixed temp color
// color fixed_tempcolor               // fixed temp color

// bool show_weather;                  // Show weather toggle
// color weather_color                 // Weather text color
// uint8_t weather_show_interval;      // Time between current weather displays
// bool show_weather_daily
// color weather_color_daily           // Weather text color
// uint8_t weather_show_daily_interval;// Time between current weather displays
// bool show_airquality
// color airquality_color              // Weather text color
// uint8_t airquality_show_interval;   // Time between current weather displays

// uint8_t alert_check_interval;       // Time between alert checks
// uint8_t alert_show_interval;        // Time between alert displays
// String weatherapi;                  // OpenWeather API Key

// Group 4 (Location)
// bool used_fixed_loc;                // Use fixed GPS coordinates
// bool scoll_location;                // Scroll location on change
// String fixedLat;                    // Fixed GPS latitude
// String fixedLon;                    // Fixed GPS Longitude
// String ipgeoapi                     // IP Geolocation API

// WARNING: Not advancing the config version in main.h after adding/deleting iotwebconf config options will result in system settings data corruption.  Iotwebconf will erase the config if it sees a different config version to avoid this corruption.

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, VERSION_CONFIG);
iotwebconf::CheckboxTParameter resetdefaults =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("resetdefaults").label("Reset to Defaults (AP mode)").defaultValue(false).build();
iotwebconf::CheckboxTParameter serialdebug =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("serialdebug").label("Enable serial debug output (for debugging)").defaultValue(false).build();
iotwebconf::ParameterGroup group1 = iotwebconf::ParameterGroup("Display", "Display Settings");
  iotwebconf::CheckboxTParameter imperial =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("imperial").label("Use imperial units (Instead of metric)").defaultValue(true).build();
  iotwebconf::IntTParameter<int8_t> brightness_level =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("brightness_level").label("Brightness level (1-5)").defaultValue(DEF_BRIGHTNESS_LEVEL).min(1).max(5).step(1).placeholder("1(low)..5(high)").build();
  iotwebconf::IntTParameter<int8_t> text_scroll_speed =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("text_scroll_speed").label("Text scroll speed (1-10)").defaultValue(DEF_SCROLL_SPEED).min(1).max(10).step(1).placeholder("1(low)..10(high)").build();
    iotwebconf::ColorTParameter systemcolor =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("systemcolor").label("Choose system messages text color").defaultValue(DEF_SYSTEM_COLOR).build();
  iotwebconf::CheckboxTParameter show_date =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_date").label("Display the current date").defaultValue(true).build();
  iotwebconf::ColorTParameter datecolor =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("datecolor").label("Choose date color").defaultValue(DEF_DATE_COLOR).build();
 iotwebconf::IntTParameter<int8_t> show_date_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("show_date_interval").label("Display date interval in hours (1-24)").defaultValue(DEF_DATE_INTERVAL).min(1).max(24).step(1).placeholder("1..24(hours)").build();
  iotwebconf::CheckboxTParameter enable_status =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_status").label("Enable system status LED (Bottom left corner)").defaultValue(true).build();
iotwebconf::CheckboxTParameter enable_alertflash =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_alertflash").label("Enable screen flashes before notifications").defaultValue(true).build();
iotwebconf::ParameterGroup group3 = iotwebconf::ParameterGroup("Weather", "Weather Settings");
iotwebconf::CheckboxTParameter use_fixed_tempcolor =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_tempcolor").label("Use custom temperature color (Disables auto color)").defaultValue(false).build();
  iotwebconf::ColorTParameter fixed_tempcolor =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("fixed_tempcolor").label("Custom temperature color").defaultValue(DEF_TEMP_COLOR).build();
  iotwebconf::CheckboxTParameter show_weather =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_weather").label("Display current weather conditions").defaultValue(true).build();
  iotwebconf::ColorTParameter weather_color =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("weather_color").label("Current conditions text color").defaultValue(DEF_WEATHER_COLOR).build();
 iotwebconf::IntTParameter<int8_t> show_weather_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("weather_show_interval").label("Current conditions display interval in minutes (1-120)").defaultValue(DEF_WEATHER_INTERVAL).min(1).max(120).step(1).placeholder("1(min)..60(min)").build();
  iotwebconf::CheckboxTParameter show_weather_daily =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_weather_daily").label("Display daily weather conditions").defaultValue(true).build();
  iotwebconf::ColorTParameter weather_daily_color =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("weather_daily_color").label("Daily conditions text color").defaultValue(DEF_DAILY_COLOR).build();
 iotwebconf::IntTParameter<int8_t> show_weather_daily_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("weather_daily_show_interval").label("Daily conditions display interval in hours (1-24)").defaultValue(DEF_DAILY_INTERVAL).min(1).max(24).step(1).placeholder("1(hour)..24(hours)").build();
  iotwebconf::CheckboxTParameter show_airquality =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_airquality").label("Display current air quality").defaultValue(true).build();
iotwebconf::CheckboxTParameter use_fixed_aqicolor =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_aqicolor").label("Use custom air quality color (Disables auto color)").defaultValue(false).build();
  iotwebconf::ColorTParameter airquality_color =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("airquality_color").label("Custom air quality color").defaultValue(DEF_AQI_COLOR).build();
 iotwebconf::IntTParameter<int8_t> airquality_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("airquality_interval").label("Air quality display interval in minutes (1-120)").defaultValue(DEF_AQI_INTERVAL).min(1).max(120).step(1).placeholder("1(min)..120(min)").build();
  iotwebconf::IntTParameter<int8_t> show_alert_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("alert_show_interval").label("Weather alert display interval in minutes (1-60)").defaultValue(DEF_ALERT_INTERVAL).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
  iotwebconf::TextTParameter<33> weatherapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("weatherapi").label("Openweathermap.org API Key").defaultValue("").build();
iotwebconf::ParameterGroup group2 = iotwebconf::ParameterGroup("Clock", "Clock Settings");
  iotwebconf::CheckboxTParameter twelve_clock=
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("twelve_clock").label("Use 12 Hour Clock").defaultValue(true).build();
  iotwebconf::CheckboxTParameter use_fixed_tz =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_tz").label("Use custom timezone (Disables auto timezone)").defaultValue(false).build();
  iotwebconf::IntTParameter<int8_t> fixed_offset =   
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("fixed_offset").label("Custom timezone GMT offset hours").defaultValue(0).min(-12).max(12).step(1).placeholder("-12...12").build();
  iotwebconf::CheckboxTParameter colonflicker =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("colonflicker").label("Enable clock colon flash").defaultValue(true).build();
  iotwebconf::CheckboxTParameter flickerfast =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("flickerfast").label("Fast clock colon flash (Only works if \"enable colon flash\" is enabled above)").defaultValue(false).build();
  iotwebconf::CheckboxTParameter use_fixed_clockcolor=
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_clockcolor").label("Use custom clock color (Disables auto color)").defaultValue(false).build();
    iotwebconf::ColorTParameter fixed_clockcolor =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("fixed_clockcolor").label("Custom clock color").defaultValue(DEF_CLOCK_COLOR).build();
iotwebconf::ParameterGroup group4 = iotwebconf::ParameterGroup("Location", "Location Settings");
  iotwebconf::CheckboxTParameter use_fixed_loc =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_loc").label("Use custom location (Disables auto location)").defaultValue(false).build();
  iotwebconf::TextTParameter<12> fixedLat =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLat").label("Custom latitude").defaultValue("").build();
  iotwebconf::TextTParameter<12> fixedLon =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLon").label("Custom longitude").defaultValue("").build();
  iotwebconf::TextTParameter<33> ipgeoapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("ipgeoapi").label("IPGeolocation.io API key").defaultValue("").build();
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
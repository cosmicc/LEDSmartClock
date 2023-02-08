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
// bool disable_status                 // disable status corner led
// bool disable_alertflash             // disable alertflash

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

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, VERSION_CONFIG);
iotwebconf::CheckboxTParameter resetdefaults =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("resetdefaults").label("Reset to Defaults (AP mode)").defaultValue(false).build();
iotwebconf::CheckboxTParameter serialdebug =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("serialdebug").label("Serial Debug").defaultValue(false).build();
iotwebconf::ParameterGroup group1 = iotwebconf::ParameterGroup("Display", "Display Settings");
  iotwebconf::IntTParameter<int8_t> brightness_level =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("brightness_level").label("Brightness Level (1-5)").defaultValue(2).min(1).max(5).step(1).placeholder("1(low)..5(high)").build();
  iotwebconf::IntTParameter<int8_t> text_scroll_speed =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("text_scroll_speed").label("Text Scroll Speed (1-10)").defaultValue(5).min(1).max(10).step(1).placeholder("1(low)..10(high)").build();
  iotwebconf::CheckboxTParameter show_date =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_date").label("Show Date").defaultValue(true).build();
  iotwebconf::ColorTParameter datecolor =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("datecolor").label("Choose date color").defaultValue("#FF8800").build();
 iotwebconf::IntTParameter<int8_t> show_date_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("show_date_interval").label("Show Date Interval in Hours (1-24)").defaultValue(1).min(1).max(24).step(1).placeholder("1..24(hours)").build();
  iotwebconf::CheckboxTParameter disable_status =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("disable_status").label("Disable corner status LED").defaultValue(false).build();
iotwebconf::CheckboxTParameter disable_alertflash =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("disable_alertflash").label("Disable Notification Flashes").defaultValue(false).build();
iotwebconf::ParameterGroup group3 = iotwebconf::ParameterGroup("Weather", "Weather Settings");
  iotwebconf::CheckboxTParameter imperial =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("imperial").label("Imperial Units").defaultValue(true).build();
iotwebconf::CheckboxTParameter use_fixed_tempcolor =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_tempcolor").label("Use Fixed Temperature Color (Disables Auto Color)").defaultValue(false).build();
  iotwebconf::ColorTParameter fixed_tempcolor =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("fixed_tempcolor").label("Fixed Temperature Color").defaultValue("#FF8800").build();
  iotwebconf::CheckboxTParameter show_weather =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_weather").label("Show Current Weather Conditions").defaultValue(true).build();
  iotwebconf::ColorTParameter weather_color =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("weather_color").label("Current Conditions Text Color").defaultValue("#FF8800").build();
 iotwebconf::IntTParameter<int8_t> show_weather_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("weather_show_interval").label("Current Conditions Display Interval Min (1-60)").defaultValue(10).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
  iotwebconf::CheckboxTParameter show_weather_daily =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_weather_daily").label("Show Daily Weather Conditions").defaultValue(true).build();
  iotwebconf::ColorTParameter weather_daily_color =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("weather_daily_color").label("Daily Conditions Text Color").defaultValue("#FF8800").build();
 iotwebconf::IntTParameter<int8_t> show_weather_daily_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("weather_daily_show_interval").label("Daily Conditions Display Interval Hrs (1-24)").defaultValue(1).min(1).max(24).step(1).placeholder("1(hour)..24(hours)").build();
  iotwebconf::CheckboxTParameter show_airquality =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_airquality").label("Show Current Air Quality").defaultValue(true).build();
iotwebconf::CheckboxTParameter use_fixed_aqicolor =  
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_aqicolor").label("Use Fixed Air Quality Color (Disables Auto Color)").defaultValue(false).build();
  iotwebconf::ColorTParameter airquality_color =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("airquality_color").label("Air Quality Text Color").defaultValue("#FF8800").build();
 iotwebconf::IntTParameter<int8_t> airquality_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("airquality_interval").label("Air Quality Display Interval Min (1-120)").defaultValue(30).min(1).max(120).step(1).placeholder("1(min)..120(min)").build();
  iotwebconf::IntTParameter<int8_t> show_alert_interval =
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("alert_show_interval").label("Weather Alert Display Interval Min (1-60)").defaultValue(5).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();
  iotwebconf::TextTParameter<33> weatherapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("weatherapi").label("OpenWeather API Key").defaultValue("").build();
iotwebconf::ParameterGroup group2 = iotwebconf::ParameterGroup("Clock", "Clock Settings");
  iotwebconf::CheckboxTParameter twelve_clock=
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("twelve_clock").label("Use 12 Hour Clock").defaultValue(true).build();
  iotwebconf::CheckboxTParameter use_fixed_tz =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_tz").label("Use Fixed Timezone").defaultValue(false).build();
  iotwebconf::IntTParameter<int8_t> fixed_offset =   
    iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("fixed_offset").label("Fixed GMT Hours Offset").defaultValue(0).min(-12).max(12).step(1).placeholder("-12...12").build();
  iotwebconf::CheckboxTParameter colonflicker =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("colonflicker").label("Clock Colon Flash").defaultValue(true).build();
  iotwebconf::CheckboxTParameter flickerfast =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("flickerfast").label("Flash Colon Fast").defaultValue(true).build();
  iotwebconf::CheckboxTParameter use_fixed_clockcolor=
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_clockcolor").label("Use Fixed Clock Color (Disables Auto Color)").defaultValue(false).build();
    iotwebconf::ColorTParameter fixed_clockcolor =
   iotwebconf::Builder<iotwebconf::ColorTParameter>("fixed_clockcolor").label("Fixed Clock Color").defaultValue("#FF8800").build();
iotwebconf::ParameterGroup group4 = iotwebconf::ParameterGroup("Location", "Location Settings");
  iotwebconf::CheckboxTParameter use_fixed_loc =
    iotwebconf::Builder<iotwebconf::CheckboxTParameter>("use_fixed_loc").label("Use Fixed Location").defaultValue(false).build();
  iotwebconf::TextTParameter<12> fixedLat =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLat").label("Fixed latitude").defaultValue("").build();
  iotwebconf::TextTParameter<12> fixedLon =
    iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLon").label("Fixed Longitude").defaultValue("").build();
  iotwebconf::TextTParameter<33> ipgeoapi =
    iotwebconf::Builder<iotwebconf::TextTParameter<33>>("ipgeoapi").label("IPGeolocation.io API Key").defaultValue("").build();
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
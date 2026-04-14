#include "main.h"

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, CONFIG_VERSION);

iotwebconf::TextTParameter<12> savedlat =
  iotwebconf::Builder<iotwebconf::TextTParameter<12>>("savedlat").label("Saved Latitude").defaultValue("0").build();
iotwebconf::TextTParameter<12> savedlon =
  iotwebconf::Builder<iotwebconf::TextTParameter<12>>("savedlon").label("Saved Longitude").defaultValue("0").build();
iotwebconf::IntTParameter<int8_t> savedtzoffset =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("tzoffset").label("Saved TZ Offset").defaultValue(0).min(-12).max(12).step(1).placeholder("-12...12").build();
iotwebconf::TextTParameter<32> savedcity =
  iotwebconf::Builder<iotwebconf::TextTParameter<32>>("savedcity").label("Saved City").defaultValue("").build();
iotwebconf::TextTParameter<32> savedstate =
  iotwebconf::Builder<iotwebconf::TextTParameter<32>>("savedstate").label("Saved State").defaultValue("").build();
iotwebconf::TextTParameter<32> savedcountry =
  iotwebconf::Builder<iotwebconf::TextTParameter<32>>("savedcountry").label("Saved Country").defaultValue("").build();
iotwebconf::CheckboxTParameter resetdefaults =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("resetdefaults").label("Reset to Defaults (AP mode)").defaultValue(false).build();
iotwebconf::CheckboxTParameter serialdebug =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("serialdebug").label("Enable serial debug output (for debugging)").defaultValue(false).build();
iotwebconf::TextTParameter<33> ipgeoapi =
  iotwebconf::Builder<iotwebconf::TextTParameter<33>>("ipgeoapi").label("IPGeolocation.io API key").defaultValue("").build();
iotwebconf::TextTParameter<33> weatherapi =
  iotwebconf::Builder<iotwebconf::TextTParameter<33>>("weatherapi").label("OpenWeather API key (One Call 3.0)").defaultValue("").build();

iotwebconf::ParameterGroup group1 = iotwebconf::ParameterGroup("Display", "Display & Messages");
iotwebconf::CheckboxTParameter imperial =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("imperial").label("Use imperial units (Instead of metric)").defaultValue(true).build();
iotwebconf::IntTParameter<int8_t> brightness_level =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("brightness_level").label("Brightness level (1-10)").defaultValue(DEF_BRIGHTNESS_LEVEL).min(1).max(10).step(1).placeholder("1(low)..10(high)").build();
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

iotwebconf::ParameterGroup group2 = iotwebconf::ParameterGroup("Clock", "Clock & Time");
iotwebconf::CheckboxTParameter twelve_clock =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("twelve_clock").label("Use 12 Hour Clock").defaultValue(true).build();
iotwebconf::CheckboxTParameter enable_fixed_tz =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_fixed_tz").label("Use custom timezone (Disables auto timezone)").defaultValue(false).build();
iotwebconf::IntTParameter<int8_t> fixed_offset =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("fixed_offset").label("Custom timezone GMT offset hours").defaultValue(0).min(-12).max(12).step(1).placeholder("-12...12").build();
iotwebconf::CheckboxTParameter colonflicker =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("colonflicker").label("Enable clock colon flash").defaultValue(true).build();
iotwebconf::CheckboxTParameter flickerfast =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("flickerfast").label("Fast clock colon flash (Only works if \"enable colon flash\" is enabled above)").defaultValue(false).build();
iotwebconf::CheckboxTParameter enable_clock_color =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_clock_color").label("Use custom clock color (Disables auto color)").defaultValue(false).build();
iotwebconf::ColorTParameter clock_color =
  iotwebconf::Builder<iotwebconf::ColorTParameter>("clock_color").label("Custom clock color").defaultValue(DEF_CLOCK_COLOR).build();

iotwebconf::ParameterGroup group3 = iotwebconf::ParameterGroup("CurrentTemp", "Current Temp");
iotwebconf::CheckboxTParameter show_current_temp =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_current_temp").label("Display current temperature").defaultValue(true).build();
iotwebconf::CheckboxTParameter enable_temp_color =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_temp_color").label("Use custom temperature color (Disables auto color)").defaultValue(false).build();
iotwebconf::ColorTParameter temp_color =
  iotwebconf::Builder<iotwebconf::ColorTParameter>("temp_color").label("Custom temperature color").defaultValue(DEF_TEMP_COLOR).build();
iotwebconf::IntTParameter<int8_t> current_temp_interval =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("current_temp_interval").label("Current temperature display interval in minutes (1-120)").defaultValue(DEF_TEMP_INTERVAL).min(1).max(120).step(1).placeholder("1(min)..120(min)").build();
iotwebconf::IntTParameter<int8_t> current_temp_duration =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("current_temp_duration").label("Current temperature display duration in seconds (5-60)").defaultValue(DEF_TEMP_DISPLAY_DURATION).min(5).max(60).step(1).placeholder("5(sec)..60(sec)").build();

iotwebconf::ParameterGroup group4 = iotwebconf::ParameterGroup("CurrentWeather", "Current Weather");
iotwebconf::CheckboxTParameter show_current_weather =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_current_weather").label("Display current weather conditions").defaultValue(true).build();
iotwebconf::ColorTParameter current_weather_color =
  iotwebconf::Builder<iotwebconf::ColorTParameter>("current_weather_color").label("Current conditions text color").defaultValue(DEF_WEATHER_COLOR).build();
iotwebconf::IntTParameter<int8_t> current_weather_interval =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("current_weather_interval").label("Current conditions display interval in hours (1-24)").defaultValue(DEF_WEATHER_INTERVAL).min(1).max(24).step(1).placeholder("1..24(hours)").build();

iotwebconf::ParameterGroup group5 = iotwebconf::ParameterGroup("DailyWeather", "Forecast & Air Quality");
iotwebconf::CheckboxTParameter show_daily_weather =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_daily_weather").label("Display daily weather conditions").defaultValue(true).build();
iotwebconf::ColorTParameter daily_weather_color =
  iotwebconf::Builder<iotwebconf::ColorTParameter>("daily_weather_color").label("Daily conditions text color").defaultValue(DEF_DAILY_COLOR).build();
iotwebconf::IntTParameter<int8_t> daily_weather_interval =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("daily_weather_interval").label("Daily conditions display interval in hours (1-24)").defaultValue(DEF_DAILY_INTERVAL).min(1).max(24).step(1).placeholder("1(hour)..24(hours)").build();

iotwebconf::ParameterGroup group6 = iotwebconf::ParameterGroup("Alerts", "Alerts");
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

iotwebconf::ParameterGroup group7 = iotwebconf::ParameterGroup("Status", "Status LEDs");
iotwebconf::CheckboxTParameter enable_system_status =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_system_status").label("Enable system status LED (Bottom left)").defaultValue(true).build();
iotwebconf::CheckboxTParameter enable_aqi_status =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_aqi_status").label("Enable Air Quality status LED (Top Left)").defaultValue(true).build();
iotwebconf::CheckboxTParameter enable_uvi_status =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_uvi_status").label("Enable UV Index status LED (Top Right)").defaultValue(true).build();
iotwebconf::CheckboxTParameter green_status =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("green_status").label("Use green instead of black/off for good/low status leds").defaultValue(false).build();

iotwebconf::ParameterGroup group8 = iotwebconf::ParameterGroup("Sun", "Sunrise & Sunset");
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

iotwebconf::ParameterGroup group9 = iotwebconf::ParameterGroup("Location", "Location Settings");
iotwebconf::CheckboxTParameter show_loc_change =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_loc_change").label("Display new location on major location change").defaultValue(true).build();
iotwebconf::CheckboxTParameter enable_fixed_loc =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_fixed_loc").label("Use custom location (Disables auto location)").defaultValue(false).build();
iotwebconf::TextTParameter<12> fixedLat =
  iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLat").label("Custom latitude").defaultValue("").build();
iotwebconf::TextTParameter<12> fixedLon =
  iotwebconf::Builder<iotwebconf::TextTParameter<12>>("fixedLon").label("Custom longitude").defaultValue("").build();

namespace
{
/** Clamps an integer config parameter into its supported range in memory. */
template <typename T>
bool normalizeIntParameter(iotwebconf::IntTParameter<T> &parameter, T minValue, T maxValue, T fallbackValue)
{
  T currentValue = parameter.value();
  if (currentValue >= minValue && currentValue <= maxValue)
    return false;

  parameter.value() = fallbackValue;
  return true;
}

/** Registers top-level system parameters managed directly by IotWebConf. */
void addSystemParameters()
{
  iotWebConf.addSystemParameter(&serialdebug);
  iotWebConf.addSystemParameter(&resetdefaults);
  iotWebConf.addSystemParameter(&ipgeoapi);
  iotWebConf.addSystemParameter(&weatherapi);
}

/** Registers hidden persisted values that back startup fallbacks. */
void addHiddenParameters()
{
  iotWebConf.addHiddenParameter(&savedtzoffset);
  iotWebConf.addHiddenParameter(&savedlat);
  iotWebConf.addHiddenParameter(&savedlon);
  iotWebConf.addHiddenParameter(&savedcity);
  iotWebConf.addHiddenParameter(&savedstate);
  iotWebConf.addHiddenParameter(&savedcountry);
}

/** Repairs legacy or out-of-range config values after loading them from flash. */
void normalizeConfigValues()
{
  bool corrected = false;

  corrected |= normalizeIntParameter(brightness_level, static_cast<int8_t>(1), static_cast<int8_t>(10), static_cast<int8_t>(DEF_BRIGHTNESS_LEVEL));
  corrected |= normalizeIntParameter(text_scroll_speed, static_cast<int8_t>(1), static_cast<int8_t>(10), static_cast<int8_t>(DEF_SCROLL_SPEED));
  corrected |= normalizeIntParameter(date_interval, static_cast<int8_t>(1), static_cast<int8_t>(24), static_cast<int8_t>(DEF_DATE_INTERVAL));
  corrected |= normalizeIntParameter(current_temp_interval, static_cast<int8_t>(1), static_cast<int8_t>(120), static_cast<int8_t>(DEF_TEMP_INTERVAL));
  corrected |= normalizeIntParameter(current_temp_duration, static_cast<int8_t>(5), static_cast<int8_t>(60), static_cast<int8_t>(DEF_TEMP_DISPLAY_DURATION));
  corrected |= normalizeIntParameter(current_weather_interval, static_cast<int8_t>(1), static_cast<int8_t>(24), static_cast<int8_t>(DEF_WEATHER_INTERVAL));
  corrected |= normalizeIntParameter(daily_weather_interval, static_cast<int8_t>(1), static_cast<int8_t>(24), static_cast<int8_t>(DEF_DAILY_INTERVAL));
  corrected |= normalizeIntParameter(aqi_interval, static_cast<int8_t>(1), static_cast<int8_t>(120), static_cast<int8_t>(DEF_AQI_INTERVAL));
  corrected |= normalizeIntParameter(alert_interval, static_cast<int8_t>(1), static_cast<int8_t>(60), static_cast<int8_t>(DEF_ALERT_INTERVAL));
  corrected |= normalizeIntParameter(fixed_offset, static_cast<int8_t>(-12), static_cast<int8_t>(12), static_cast<int8_t>(0));
  corrected |= normalizeIntParameter(savedtzoffset, static_cast<int8_t>(-12), static_cast<int8_t>(12), static_cast<int8_t>(0));

  if (corrected)
    ESP_LOGW(TAG, "One or more out-of-range configuration values were normalized in memory.");
}

/** Populates the themed configuration sections with their owned parameters. */
void populateParameterGroups()
{
  group1.addItem(&brightness_level);
  group1.addItem(&text_scroll_speed);
  group1.addItem(&system_color);
  group1.addItem(&enable_alertflash);
  group1.addItem(&show_date);
  group1.addItem(&date_color);
  group1.addItem(&date_interval);

  group2.addItem(&twelve_clock);
  group2.addItem(&enable_fixed_tz);
  group2.addItem(&fixed_offset);
  group2.addItem(&colonflicker);
  group2.addItem(&flickerfast);
  group2.addItem(&enable_clock_color);
  group2.addItem(&clock_color);

  group3.addItem(&show_current_temp);
  group3.addItem(&enable_temp_color);
  group3.addItem(&temp_color);
  group3.addItem(&current_temp_interval);
  group3.addItem(&current_temp_duration);

  group4.addItem(&imperial);
  group4.addItem(&show_current_weather);
  group4.addItem(&current_weather_color);
  group4.addItem(&current_weather_interval);

  group5.addItem(&show_daily_weather);
  group5.addItem(&daily_weather_color);
  group5.addItem(&daily_weather_interval);
  group5.addItem(&show_aqi);
  group5.addItem(&enable_aqi_color);
  group5.addItem(&aqi_color);
  group5.addItem(&aqi_interval);

  group6.addItem(&alert_interval);

  group7.addItem(&enable_system_status);
  group7.addItem(&enable_aqi_status);
  group7.addItem(&enable_uvi_status);
  group7.addItem(&green_status);

  group8.addItem(&show_sunrise);
  group8.addItem(&sunrise_color);
  group8.addItem(&sunrise_message);
  group8.addItem(&show_sunset);
  group8.addItem(&sunset_color);
  group8.addItem(&sunset_message);

  group9.addItem(&show_loc_change);
  group9.addItem(&enable_fixed_loc);
  group9.addItem(&fixedLat);
  group9.addItem(&fixedLon);
}

/** Registers the custom configuration groups on the portal in display order. */
void addParameterGroups()
{
  iotWebConf.addParameterGroup(&group1);
  iotWebConf.addParameterGroup(&group2);
  iotWebConf.addParameterGroup(&group3);
  iotWebConf.addParameterGroup(&group4);
  iotWebConf.addParameterGroup(&group5);
  iotWebConf.addParameterGroup(&group6);
  iotWebConf.addParameterGroup(&group7);
  iotWebConf.addParameterGroup(&group8);
  iotWebConf.addParameterGroup(&group9);
}
} // namespace

void setupIotWebConf()
{
  addSystemParameters();
  addHiddenParameters();
  populateParameterGroups();
  addParameterGroups();

  iotWebConf.getApTimeoutParameter()->visible = true;
  iotWebConf.setWifiConnectionHandler(&connectWifi);
  iotWebConf.setWifiConnectionCallback(&wifiConnected);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.setStatusPin(STATUS_PIN);
  iotWebConf.setConfigPin(CONFIG_PIN);
  configureWebUi();
  iotWebConf.getSystemParameterGroup()->label = "Connectivity & Access";
  iotWebConf.setupUpdateServer(
      [](const char *updatePath)
      { (void)updatePath; },
      [](const char *userName, char *password)
      {
        (void)userName;
        (void)password;
      });
  iotWebConf.init();
  normalizeConfigValues();
}

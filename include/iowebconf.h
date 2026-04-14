#pragma once

/** Shared IotWebConf application instance that owns the configuration UI. */
extern IotWebConf iotWebConf;

/** Wires parameters, callbacks, OTA support, and the themed config portal. */
void setupIotWebConf();

/** Last persisted latitude used before live location services update it. */
extern iotwebconf::TextTParameter<12> savedlat;
/** Last persisted longitude used before live location services update it. */
extern iotwebconf::TextTParameter<12> savedlon;
/** Last persisted timezone offset used as a local fallback. */
extern iotwebconf::IntTParameter<int8_t> savedtzoffset;
/** Last persisted city name used during startup and offline operation. */
extern iotwebconf::TextTParameter<32> savedcity;
/** Last persisted state or province used during startup and offline operation. */
extern iotwebconf::TextTParameter<32> savedstate;
/** Last persisted country used during startup and offline operation. */
extern iotwebconf::TextTParameter<32> savedcountry;
/** Requests a factory reset when the next configuration save completes. */
extern iotwebconf::CheckboxTParameter resetdefaults;
/** Enables verbose serial logging intended for maintenance and debugging. */
extern iotwebconf::CheckboxTParameter serialdebug;
/** API key for the ipgeolocation.io location/timezone fallback service. */
extern iotwebconf::TextTParameter<33> ipgeoapi;
/** API key for OpenWeather weather, geocode, and AQI services. */
extern iotwebconf::TextTParameter<33> weatherapi;

/** Display-related configuration section shown on the web settings page. */
extern iotwebconf::ParameterGroup group1;
/** Selects imperial units instead of metric units. */
extern iotwebconf::CheckboxTParameter imperial;
/** User-selected brightness bias applied on top of ambient-light control. */
extern iotwebconf::IntTParameter<int8_t> brightness_level;
/** Base scroll speed for ticker-style text messages. */
extern iotwebconf::IntTParameter<int8_t> text_scroll_speed;
/** Default text color used by system and maintenance messages. */
extern iotwebconf::ColorTParameter system_color;
/** Enables rotating date messages in the display schedule. */
extern iotwebconf::CheckboxTParameter show_date;
/** Text color used for date messages. */
extern iotwebconf::ColorTParameter date_color;
/** Interval between date-message presentations. */
extern iotwebconf::IntTParameter<int8_t> date_interval;
/** Enables fullscreen flashes before selected notifications. */
extern iotwebconf::CheckboxTParameter enable_alertflash;

/** Clock-related configuration section shown on the web settings page. */
extern iotwebconf::ParameterGroup group2;
/** Selects 12-hour clock formatting instead of 24-hour time. */
extern iotwebconf::CheckboxTParameter twelve_clock;
/** Forces a fixed timezone offset instead of automatic timezone detection. */
extern iotwebconf::CheckboxTParameter enable_fixed_tz;
/** User-selected timezone offset used when fixed timezone mode is enabled. */
extern iotwebconf::IntTParameter<int8_t> fixed_offset;
/** Enables blinking of the colon between the clock digits. */
extern iotwebconf::CheckboxTParameter colonflicker;
/** Speeds up the colon blink cadence when blinking is enabled. */
extern iotwebconf::CheckboxTParameter flickerfast;
/** Locks the clock digits to a custom color instead of auto hue logic. */
extern iotwebconf::CheckboxTParameter enable_clock_color;
/** Custom color used by the clock digits when auto hue is disabled. */
extern iotwebconf::ColorTParameter clock_color;

/** Current-temperature configuration section. */
extern iotwebconf::ParameterGroup group3;
/** Enables the standalone current-temperature display block. */
extern iotwebconf::CheckboxTParameter show_current_temp;
/** Locks temperature rendering to a custom color instead of auto hue logic. */
extern iotwebconf::CheckboxTParameter enable_temp_color;
/** Custom color used by temperature rendering when auto hue is disabled. */
extern iotwebconf::ColorTParameter temp_color;
/** Interval between current-temperature displays. */
extern iotwebconf::IntTParameter<int8_t> current_temp_interval;
/** Duration that the current-temperature block remains visible once shown. */
extern iotwebconf::IntTParameter<int8_t> current_temp_duration;

/** Current-weather configuration section. */
extern iotwebconf::ParameterGroup group4;
/** Enables the current-conditions weather summary. */
extern iotwebconf::CheckboxTParameter show_current_weather;
/** Text color used by current-conditions weather messages. */
extern iotwebconf::ColorTParameter current_weather_color;
/** Interval between current-weather presentations. */
extern iotwebconf::IntTParameter<int8_t> current_weather_interval;

/** Daily-weather configuration section. */
extern iotwebconf::ParameterGroup group5;
/** Enables the daily forecast summary display. */
extern iotwebconf::CheckboxTParameter show_daily_weather;
/** Text color used by daily forecast messages. */
extern iotwebconf::ColorTParameter daily_weather_color;
/** Interval between daily forecast presentations. */
extern iotwebconf::IntTParameter<int8_t> daily_weather_interval;

/** Air-quality configuration section. */
extern iotwebconf::ParameterGroup group6;
/** Enables the air-quality summary display. */
extern iotwebconf::CheckboxTParameter show_aqi;
/** Custom text color used by AQI messages when auto color is disabled. */
extern iotwebconf::ColorTParameter aqi_color;
/** Locks AQI rendering to a custom color instead of AQI bucket colors. */
extern iotwebconf::CheckboxTParameter enable_aqi_color;
/** Interval between air-quality presentations. */
extern iotwebconf::IntTParameter<int8_t> aqi_interval;
/** Interval between alert-display opportunities. */
extern iotwebconf::IntTParameter<int8_t> alert_interval;

/** Status-LED configuration section. */
extern iotwebconf::ParameterGroup group7;
/** Enables the lower-left system-state status pixel. */
extern iotwebconf::CheckboxTParameter enable_system_status;
/** Enables the upper-left AQI status pixel. */
extern iotwebconf::CheckboxTParameter enable_aqi_status;
/** Enables the upper-right UV-index status pixel. */
extern iotwebconf::CheckboxTParameter enable_uvi_status;
/** Uses green instead of off for good/low status conditions. */
extern iotwebconf::CheckboxTParameter green_status;

/** Sunrise and sunset notification section. */
extern iotwebconf::ParameterGroup group8;
/** Enables a message when the local sunrise time is reached. */
extern iotwebconf::CheckboxTParameter show_sunrise;
/** Text color used by sunrise notifications. */
extern iotwebconf::ColorTParameter sunrise_color;
/** Message text shown when sunrise occurs. */
extern iotwebconf::TextTParameter<128> sunrise_message;
/** Enables a message when the local sunset time is reached. */
extern iotwebconf::CheckboxTParameter show_sunset;
/** Text color used by sunset notifications. */
extern iotwebconf::ColorTParameter sunset_color;
/** Message text shown when sunset occurs. */
extern iotwebconf::TextTParameter<128> sunset_message;

/** Location-override configuration section. */
extern iotwebconf::ParameterGroup group9;
/** Shows a notification when the detected location changes significantly. */
extern iotwebconf::CheckboxTParameter show_loc_change;
/** Forces a fixed latitude/longitude instead of automatic location updates. */
extern iotwebconf::CheckboxTParameter enable_fixed_loc;
/** User-entered latitude used when fixed-location mode is enabled. */
extern iotwebconf::TextTParameter<12> fixedLat;
/** User-entered longitude used when fixed-location mode is enabled. */
extern iotwebconf::TextTParameter<12> fixedLon;

#include "main.h"
#include <cstring>

namespace
{
// The application treats the key-based NVS store as authoritative. Bump this
// marker whenever IotWebConf's positional layout changes so an older blob is
// ignored instead of decoded into the wrong parameters during boot.
constexpr char kIotWebConfStorageMarker[] = "NVS4";
constexpr size_t kGpsBaudOptionLength = 7;
constexpr size_t kGpsBaudOptionCount = 10;
constexpr size_t kMatrixOptionLength = 12;
constexpr size_t kMatrixOptionCount = 2;
constexpr int16_t kMinHardwareGpioPin = 0;
constexpr int16_t kMaxHardwareGpioPin = LEDCLOCK_MAX_GPIO_PIN;
constexpr char kDefaultGpsBaudValue[] = "9600";
constexpr char kDefaultMatrixOriginValue[] = "bottom";
constexpr char kDefaultMatrixCornerValue[] = "right";
constexpr char kDefaultMatrixAxisValue[] = "columns";
constexpr char kDefaultMatrixOrderValue[] = "zigzag";
constexpr char kGpsBaudOptionValues[] =
  "1200\0\0\0"
  "2400\0\0\0"
  "4800\0\0\0"
  "9600\0\0\0"
  "14400\0\0"
  "19200\0\0"
  "28800\0\0"
  "38400\0\0"
  "57600\0\0"
  "115200\0";
constexpr char kMatrixOriginOptionValues[] =
  "bottom\0\0\0\0\0\0"
  "top\0\0\0\0\0\0\0\0\0";
constexpr char kMatrixOriginOptionNames[] =
  "Bottom\0\0\0\0\0\0"
  "Top\0\0\0\0\0\0\0\0\0";
constexpr char kMatrixCornerOptionValues[] =
  "right\0\0\0\0\0\0\0"
  "left\0\0\0\0\0\0\0\0";
constexpr char kMatrixCornerOptionNames[] =
  "Right\0\0\0\0\0\0\0"
  "Left\0\0\0\0\0\0\0\0";
constexpr char kMatrixAxisOptionValues[] =
  "columns\0\0\0\0\0"
  "rows\0\0\0\0\0\0\0\0";
constexpr char kMatrixAxisOptionNames[] =
  "Columns\0\0\0\0\0"
  "Rows\0\0\0\0\0\0\0\0";
constexpr char kMatrixOrderOptionValues[] =
  "zigzag\0\0\0\0\0\0"
  "progressive\0";
constexpr char kMatrixOrderOptionNames[] =
  "Zigzag\0\0\0\0\0\0"
  "Progressive\0";
static_assert(sizeof(kMatrixOriginOptionValues) == (kMatrixOptionLength * kMatrixOptionCount) + 1, "Matrix origin options must be fixed-width.");
static_assert(sizeof(kMatrixOriginOptionNames) == (kMatrixOptionLength * kMatrixOptionCount) + 1, "Matrix origin names must be fixed-width.");
static_assert(sizeof(kMatrixCornerOptionValues) == (kMatrixOptionLength * kMatrixOptionCount) + 1, "Matrix corner options must be fixed-width.");
static_assert(sizeof(kMatrixCornerOptionNames) == (kMatrixOptionLength * kMatrixOptionCount) + 1, "Matrix corner names must be fixed-width.");
static_assert(sizeof(kMatrixAxisOptionValues) == (kMatrixOptionLength * kMatrixOptionCount) + 1, "Matrix axis options must be fixed-width.");
static_assert(sizeof(kMatrixAxisOptionNames) == (kMatrixOptionLength * kMatrixOptionCount) + 1, "Matrix axis names must be fixed-width.");
static_assert(sizeof(kMatrixOrderOptionValues) == (kMatrixOptionLength * kMatrixOptionCount) + 1, "Matrix order options must be fixed-width.");
static_assert(sizeof(kMatrixOrderOptionNames) == (kMatrixOptionLength * kMatrixOptionCount) + 1, "Matrix order names must be fixed-width.");
bool hardwarePinParametersReady = false;
}

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, kIotWebConfStorageMarker);

iotwebconf::TextTParameter<12> savedlat =
  iotwebconf::Builder<iotwebconf::TextTParameter<12>>("savedlat").label("Saved Latitude").defaultValue("0").build();
iotwebconf::TextTParameter<12> savedlon =
  iotwebconf::Builder<iotwebconf::TextTParameter<12>>("savedlon").label("Saved Longitude").defaultValue("0").build();
iotwebconf::IntTParameter<int8_t> savedtzoffset =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("tzoffset").label("Saved TZ Offset").defaultValue(0).min(-12).max(12).step(1).placeholder("-12...12").build();
iotwebconf::TextTParameter<64> savedtimezone =
  iotwebconf::Builder<iotwebconf::TextTParameter<64>>("savedtimezone").label("Saved Timezone Name").defaultValue("").build();
iotwebconf::TextTParameter<32> savedcity =
  iotwebconf::Builder<iotwebconf::TextTParameter<32>>("savedcity").label("Saved City").defaultValue("").build();
iotwebconf::TextTParameter<32> savedstate =
  iotwebconf::Builder<iotwebconf::TextTParameter<32>>("savedstate").label("Saved State").defaultValue("").build();
iotwebconf::TextTParameter<32> savedcountry =
  iotwebconf::Builder<iotwebconf::TextTParameter<32>>("savedcountry").label("Saved Country").defaultValue("").build();
iotwebconf::CheckboxTParameter resetdefaults =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("resetdefaults").label("Restore AP mode (wipe all config)").defaultValue(false).build();
iotwebconf::CheckboxTParameter serialdebug =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("serialdebug").label("Enable serial debug output (for debugging)").defaultValue(false).build();
iotwebconf::CheckboxTParameter web_password_protection =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("web_password_protection").label("Require password for the full web interface").defaultValue(false).build();
iotwebconf::CheckboxTParameter web_dark_mode =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("web_dark_mode").label("Use dark mode for the web interface").defaultValue(false).build();
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
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_fixed_tz").label("Use fixed GMT offset (no DST)").defaultValue(false).build();
iotwebconf::IntTParameter<int8_t> fixed_offset =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("fixed_offset").label("Custom fixed GMT offset hours (no DST)").defaultValue(0).min(-12).max(12).step(1).placeholder("-12...12").build();
iotwebconf::CheckboxTParameter enable_manual_timezone =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_manual_timezone").label("Use manual timezone name (DST-aware)").defaultValue(false).build();
iotwebconf::TextTParameter<64> manual_timezone =
  iotwebconf::Builder<iotwebconf::TextTParameter<64>>("manual_timezone").label("Manual timezone name").defaultValue("").build();
iotwebconf::TextTParameter<64> ntp_server =
  iotwebconf::Builder<iotwebconf::TextTParameter<64>>("ntp_server").label("Preferred NTP server").defaultValue(DEFAULT_NTP_SERVER).build();
iotwebconf::CheckboxTParameter override_dhcp_ntp =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("override_dhcp_ntp").label("Always use preferred NTP server (ignore DHCP NTP)").defaultValue(false).build();
iotwebconf::SelectTParameter<7> gps_baud =
  iotwebconf::Builder<iotwebconf::SelectTParameter<7>>("gps_baud").label("GPS UART baud rate").defaultValue(kDefaultGpsBaudValue).optionValues(kGpsBaudOptionValues).optionNames(kGpsBaudOptionValues).optionCount(kGpsBaudOptionCount).nameLength(kGpsBaudOptionLength).build();
iotwebconf::CheckboxTParameter colonflicker =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("colonflicker").label("Enable clock colon flash").defaultValue(true).build();
iotwebconf::CheckboxTParameter flickerfast =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("flickerfast").label("Fast clock colon flash (Only works if \"enable colon flash\" is enabled above)").defaultValue(false).build();
iotwebconf::CheckboxTParameter enable_clock_color =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_clock_color").label("Use static clock color (overrides automatic day/night color)").defaultValue(false).build();
iotwebconf::ColorTParameter clock_color =
  iotwebconf::Builder<iotwebconf::ColorTParameter>("clock_color").label("Static clock color").defaultValue(DEF_CLOCK_COLOR).build();

iotwebconf::ParameterGroup group3 = iotwebconf::ParameterGroup("CurrentTemp", "Current Temp");
iotwebconf::CheckboxTParameter show_current_temp =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_current_temp").label("Display current temperature").defaultValue(true).build();
iotwebconf::CheckboxTParameter enable_temp_color =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_temp_color").label("Use static temperature color (overrides automatic temperature color)").defaultValue(false).build();
iotwebconf::ColorTParameter temp_color =
  iotwebconf::Builder<iotwebconf::ColorTParameter>("temp_color").label("Static temperature color").defaultValue(DEF_TEMP_COLOR).build();
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
iotwebconf::CheckboxTParameter current_weather_short_text =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("current_weather_short_text").label("Use short current weather text").defaultValue(true).build();

iotwebconf::ParameterGroup group5 = iotwebconf::ParameterGroup("DailyWeather", "Daily Weather");
iotwebconf::CheckboxTParameter show_daily_weather =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_daily_weather").label("Display daily weather conditions").defaultValue(true).build();
iotwebconf::ColorTParameter daily_weather_color =
  iotwebconf::Builder<iotwebconf::ColorTParameter>("daily_weather_color").label("Daily conditions text color").defaultValue(DEF_DAILY_COLOR).build();
iotwebconf::IntTParameter<int8_t> daily_weather_interval =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("daily_weather_interval").label("Daily conditions display interval in hours (1-24)").defaultValue(DEF_DAILY_INTERVAL).min(1).max(24).step(1).placeholder("1(hour)..24(hours)").build();
iotwebconf::CheckboxTParameter daily_weather_short_text =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("daily_weather_short_text").label("Use short daily forecast text").defaultValue(true).build();

iotwebconf::ParameterGroup group6 = iotwebconf::ParameterGroup("AirQuality", "Air Quality");
iotwebconf::CheckboxTParameter show_aqi =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("show_aqi").label("Display air quality details").defaultValue(true).build();
iotwebconf::ColorTParameter aqi_color =
  iotwebconf::Builder<iotwebconf::ColorTParameter>("aqi_color").label("Custom air quality color").defaultValue(DEF_AQI_COLOR).build();
iotwebconf::CheckboxTParameter enable_aqi_color =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_aqi_color").label("Use custom air quality color (Disables auto color)").defaultValue(false).build();
iotwebconf::IntTParameter<int8_t> aqi_interval =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("aqi_interval").label("Air quality display interval in minutes (1-120)").defaultValue(DEF_AQI_INTERVAL).min(1).max(120).step(1).placeholder("1(min)..120(min)").build();

iotwebconf::ParameterGroup group11 = iotwebconf::ParameterGroup("WeatherAlerts", "Weather Alerts");
iotwebconf::IntTParameter<int8_t> alert_interval =
  iotwebconf::Builder<iotwebconf::IntTParameter<int8_t>>("alert_interval").label("Weather alert display interval in minutes (1-60)").defaultValue(DEF_ALERT_INTERVAL).min(1).max(60).step(1).placeholder("1(min)..60(min)").build();

iotwebconf::ParameterGroup group7 = iotwebconf::ParameterGroup("Status", "Status LEDs");
iotwebconf::CheckboxTParameter enable_system_status =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_system_status").label("Enable system status LED (Bottom left)").defaultValue(false).build();
iotwebconf::CheckboxTParameter enable_aqi_status =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_aqi_status").label("Enable Air Quality status LED (Top Left)").defaultValue(false).build();
iotwebconf::CheckboxTParameter enable_uvi_status =
  iotwebconf::Builder<iotwebconf::CheckboxTParameter>("enable_uvi_status").label("Enable UV Index status LED (Top Right)").defaultValue(false).build();
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
iotwebconf::ParameterGroup group10 = iotwebconf::ParameterGroup("GPS", "GPS & Receiver");
iotwebconf::ParameterGroup group13 = iotwebconf::ParameterGroup("Hardware", "Hardware Pins & Matrix Layout");
iotwebconf::TextTParameter<24> hardware_profile =
  iotwebconf::Builder<iotwebconf::TextTParameter<24>>("hardware_profile").label("Hardware profile label").defaultValue(LEDCLOCK_BOARD_PROFILE).build();
iotwebconf::IntTParameter<int16_t> led_data_pin =
  iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("led_data_pin").label("LED matrix data GPIO").defaultValue(static_cast<int16_t>(LEDCLOCK_DEFAULT_LED_DATA_PIN)).min(kMinHardwareGpioPin).max(kMaxHardwareGpioPin).step(1).placeholder("GPIO").build();
iotwebconf::IntTParameter<int16_t> gps_rx_pin =
  iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("gps_rx_pin").label("GPS RX GPIO").defaultValue(static_cast<int16_t>(LEDCLOCK_DEFAULT_GPS_RX_PIN)).min(kMinHardwareGpioPin).max(kMaxHardwareGpioPin).step(1).placeholder("GPIO").build();
iotwebconf::IntTParameter<int16_t> gps_tx_pin =
  iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("gps_tx_pin").label("GPS TX GPIO").defaultValue(static_cast<int16_t>(LEDCLOCK_DEFAULT_GPS_TX_PIN)).min(kMinHardwareGpioPin).max(kMaxHardwareGpioPin).step(1).placeholder("GPIO").build();
iotwebconf::IntTParameter<int16_t> i2c_sda_pin =
  iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("i2c_sda_pin").label("I2C SDA GPIO").defaultValue(static_cast<int16_t>(LEDCLOCK_DEFAULT_I2C_SDA_PIN)).min(kMinHardwareGpioPin).max(kMaxHardwareGpioPin).step(1).placeholder("GPIO").build();
iotwebconf::IntTParameter<int16_t> i2c_scl_pin =
  iotwebconf::Builder<iotwebconf::IntTParameter<int16_t>>("i2c_scl_pin").label("I2C SCL GPIO").defaultValue(static_cast<int16_t>(LEDCLOCK_DEFAULT_I2C_SCL_PIN)).min(kMinHardwareGpioPin).max(kMaxHardwareGpioPin).step(1).placeholder("GPIO").build();
iotwebconf::SelectTParameter<12> matrix_origin =
  iotwebconf::Builder<iotwebconf::SelectTParameter<12>>("matrix_origin").label("Matrix LED #0 vertical edge").defaultValue(kDefaultMatrixOriginValue).optionValues(kMatrixOriginOptionValues).optionNames(kMatrixOriginOptionNames).optionCount(kMatrixOptionCount).nameLength(kMatrixOptionLength).build();
iotwebconf::SelectTParameter<12> matrix_corner =
  iotwebconf::Builder<iotwebconf::SelectTParameter<12>>("matrix_corner").label("Matrix LED #0 horizontal edge").defaultValue(kDefaultMatrixCornerValue).optionValues(kMatrixCornerOptionValues).optionNames(kMatrixCornerOptionNames).optionCount(kMatrixOptionCount).nameLength(kMatrixOptionLength).build();
iotwebconf::SelectTParameter<12> matrix_axis =
  iotwebconf::Builder<iotwebconf::SelectTParameter<12>>("matrix_axis").label("Matrix wiring direction").defaultValue(kDefaultMatrixAxisValue).optionValues(kMatrixAxisOptionValues).optionNames(kMatrixAxisOptionNames).optionCount(kMatrixOptionCount).nameLength(kMatrixOptionLength).build();
iotwebconf::SelectTParameter<12> matrix_order =
  iotwebconf::Builder<iotwebconf::SelectTParameter<12>>("matrix_order").label("Matrix wiring order").defaultValue(kDefaultMatrixOrderValue).optionValues(kMatrixOrderOptionValues).optionNames(kMatrixOrderOptionNames).optionCount(kMatrixOptionCount).nameLength(kMatrixOptionLength).build();
iotwebconf::ParameterGroup group12 = iotwebconf::ParameterGroup("Maintenance", "Maintenance");

bool hasConfiguredWebPassword(const char *password)
{
  return password != nullptr && strlen(password) >= 8;
}

namespace
{
/** Returns true when a GPIO is normally reserved for flash on the current ESP32 target. */
bool isReservedFlashGpio(int16_t pin)
{
#if defined(CONFIG_IDF_TARGET_ESP32S3)
  return pin >= 27 && pin <= 32;
#else
  return pin >= 6 && pin <= 11;
#endif
}

/** Returns true when the GPIO number is invalid for the current ESP32 target. */
bool isTargetInvalidGpio(int16_t pin)
{
#if defined(CONFIG_IDF_TARGET_ESP32)
  return pin == 20 || pin == 24 || (pin >= 28 && pin <= 31);
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
  return pin >= 22 && pin <= 25;
#else
  (void)pin;
  return false;
#endif
}

/** Returns true for classic ESP32 pins that are input-only and cannot drive UART TX, I2C, or LEDs. */
bool isClassicEsp32InputOnlyGpio(int16_t pin)
{
#ifdef CONFIG_IDF_TARGET_ESP32
  return pin >= 34 && pin <= 39;
#else
  (void)pin;
  return false;
#endif
}

/** Returns true when the GPIO number exists in the configured board profile range. */
bool isGpioInConfiguredRange(int16_t pin)
{
  return pin >= kMinHardwareGpioPin && pin <= kMaxHardwareGpioPin;
}

/** Validates one input-capable GPIO and writes a field-specific error on failure. */
bool validateInputPin(const char *label, int16_t pin, String &error)
{
  if (!isHardwareInputPinUsable(pin))
  {
    error = String(label) + F(" must be a usable GPIO from 0 to ") + kMaxHardwareGpioPin +
            F(" and cannot be reserved or invalid for this ESP32 target.");
    return false;
  }
  return true;
}

/** Validates one output-capable GPIO and writes a field-specific error on failure. */
bool validateOutputPin(const char *label, int16_t pin, String &error)
{
  if (!isHardwareOutputPinUsable(pin))
  {
    error = String(label) + F(" must be an output-capable GPIO from 0 to ") + kMaxHardwareGpioPin +
            F(" and cannot be reserved, invalid, or input-only for this ESP32 target.");
    return false;
  }
  return true;
}

/** Returns true when two runtime hardware signals were assigned to the same GPIO. */
bool hasDuplicateHardwarePins(const HardwarePinSettings &settings, String &error)
{
  struct NamedPin
  {
    const char *name;
    int16_t pin;
  };

  const NamedPin pins[] = {
      {"LED data", settings.ledDataPin},
      {"GPS RX", settings.gpsRxPin},
      {"GPS TX", settings.gpsTxPin},
      {"I2C SDA", settings.i2cSdaPin},
      {"I2C SCL", settings.i2cSclPin},
  };

  for (size_t left = 0; left < sizeof(pins) / sizeof(pins[0]); ++left)
  {
    for (size_t right = left + 1; right < sizeof(pins) / sizeof(pins[0]); ++right)
    {
      if (pins[left].pin == pins[right].pin)
      {
        error = String(F("GPIO ")) + pins[left].pin + F(" is assigned to both ") +
                pins[left].name + F(" and ") + pins[right].name + F(".");
        return true;
      }
    }
  }

  return false;
}

/** Returns true when a runtime signal conflicts with the compile-time config/status pins. */
bool hasCompileTimeControlPinConflict(const HardwarePinSettings &settings, String &error)
{
  struct NamedPin
  {
    const char *name;
    int16_t pin;
  };

  const NamedPin pins[] = {
      {"LED data", settings.ledDataPin},
      {"GPS RX", settings.gpsRxPin},
      {"GPS TX", settings.gpsTxPin},
      {"I2C SDA", settings.i2cSdaPin},
      {"I2C SCL", settings.i2cSclPin},
  };

  for (const NamedPin &pin : pins)
  {
    if (pin.pin == CONFIG_PIN)
    {
      error = String(pin.name) + F(" cannot use GPIO ") + CONFIG_PIN +
              F(" because that GPIO is reserved for the compile-time config button.");
      return true;
    }
    if (pin.pin == STATUS_PIN)
    {
      error = String(pin.name) + F(" cannot use GPIO ") + STATUS_PIN +
              F(" because that GPIO is reserved for the compile-time IotWebConf status LED.");
      return true;
    }
  }

  return false;
}

/** Restores all runtime wiring pins to the compile-time profile defaults. */
void applyDefaultHardwarePinSettings()
{
  const HardwarePinSettings defaults = defaultHardwarePinSettings();
  led_data_pin.value() = defaults.ledDataPin;
  gps_rx_pin.value() = defaults.gpsRxPin;
  gps_tx_pin.value() = defaults.gpsTxPin;
  i2c_sda_pin.value() = defaults.i2cSdaPin;
  i2c_scl_pin.value() = defaults.i2cSclPin;
}

/** Returns true when a dropdown value matches one of the two supported options. */
bool isSupportedMatrixOption(const char *value, const char *first, const char *second)
{
  return value != nullptr && (strcmp(value, first) == 0 || strcmp(value, second) == 0);
}

/** Returns true when the selected LED #0 vertical edge is supported. */
bool isSupportedMatrixOrigin(const char *value)
{
  return isSupportedMatrixOption(value, "bottom", "top");
}

/** Returns true when the selected LED #0 horizontal edge is supported. */
bool isSupportedMatrixCorner(const char *value)
{
  return isSupportedMatrixOption(value, "right", "left");
}

/** Returns true when the selected matrix wiring direction is supported. */
bool isSupportedMatrixAxis(const char *value)
{
  return isSupportedMatrixOption(value, "columns", "rows");
}

/** Returns true when the selected matrix wiring order is supported. */
bool isSupportedMatrixOrder(const char *value)
{
  return isSupportedMatrixOption(value, "zigzag", "progressive");
}

/** Restores the matrix layout dropdowns to the project defaults. */
void applyDefaultMatrixLayoutSettings()
{
  const MatrixLayoutSettings defaults = defaultMatrixLayoutSettings();
  strlcpy(matrix_origin.value(), defaults.origin, sizeof(matrix_origin.value()));
  strlcpy(matrix_corner.value(), defaults.corner, sizeof(matrix_corner.value()));
  strlcpy(matrix_axis.value(), defaults.axis, sizeof(matrix_axis.value()));
  strlcpy(matrix_order.value(), defaults.order, sizeof(matrix_order.value()));
}

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

/** Returns true when the supplied GPS UART baud is one of the supported receiver rates. */
bool isSupportedGpsBaudValue(int32_t baud)
{
  switch (baud)
  {
    case 1200:
    case 2400:
    case 4800:
    case 9600:
    case 14400:
    case 19200:
    case 28800:
    case 38400:
    case 57600:
    case 115200:
      return true;
    default:
      return false;
  }
}

/** Registers top-level system parameters managed directly by IotWebConf. */
void addSystemParameters()
{
  iotWebConf.addSystemParameter(&web_password_protection);
  iotWebConf.addSystemParameter(&web_dark_mode);
  iotWebConf.addSystemParameter(&ipgeoapi);
  iotWebConf.addSystemParameter(&weatherapi);
}

/** Registers hidden persisted values that back startup fallbacks. */
void addHiddenParameters()
{
  iotWebConf.addHiddenParameter(&savedtzoffset);
  iotWebConf.addHiddenParameter(&savedtimezone);
  iotWebConf.addHiddenParameter(&savedlat);
  iotWebConf.addHiddenParameter(&savedlon);
  iotWebConf.addHiddenParameter(&savedcity);
  iotWebConf.addHiddenParameter(&savedstate);
  iotWebConf.addHiddenParameter(&savedcountry);
}

/** Repairs legacy or out-of-range config values after loading them from flash. */
void normalizeLoadedConfigValuesImpl()
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
  if (!isSupportedGpsBaudValue(static_cast<int32_t>(strtol(gps_baud.value(), nullptr, 10))))
  {
    strlcpy(gps_baud.value(), kDefaultGpsBaudValue, sizeof(gps_baud.value()));
    corrected = true;
  }
  corrected |= normalizeHardwarePinSettings();
  corrected |= normalizeMatrixLayoutSettings();
  if (hardware_profile.value()[0] == '\0')
  {
    strlcpy(hardware_profile.value(), compiledHardwareProfile(), sizeof(hardware_profile.value()));
    corrected = true;
  }
  if (ntp_server.value()[0] == '\0')
  {
    strlcpy(ntp_server.value(), DEFAULT_NTP_SERVER, 64);
    corrected = true;
  }
  if (enable_manual_timezone.isChecked() && !isSupportedTimezoneName(manual_timezone.value()))
  {
    enable_manual_timezone.value() = false;
    corrected = true;
    ESP_LOGW(TAG, "Disabled manual timezone because the saved timezone name is blank or unsupported: %s", manual_timezone.value());
  }
  if (web_password_protection.isChecked() &&
      !hasConfiguredWebPassword(iotWebConf.getApPasswordParameter()->valueBuffer))
  {
    web_password_protection.value() = false;
    corrected = true;
    ESP_LOGW(TAG, "Disabled web password protection because no web password is configured.");
  }
  if (!web_password_protection.isChecked() &&
      !hasConfiguredWebPassword(iotWebConf.getApPasswordParameter()->valueBuffer))
  {
    strlcpy(iotWebConf.getApPasswordParameter()->valueBuffer, wifiInitialApPassword,
            static_cast<size_t>(iotWebConf.getApPasswordParameter()->getLength()));
    corrected = true;
  }

  if (corrected)
    ESP_LOGW(TAG, "One or more out-of-range configuration values were normalized in memory.");
}

/** Populates the themed configuration sections with their owned parameters. */
void populateParameterGroups()
{
  group1.addItem(&imperial);
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
  group2.addItem(&enable_manual_timezone);
  group2.addItem(&manual_timezone);
  group2.addItem(&ntp_server);
  group2.addItem(&override_dhcp_ntp);
  group2.addItem(&colonflicker);
  group2.addItem(&flickerfast);
  group2.addItem(&enable_clock_color);
  group2.addItem(&clock_color);

  group3.addItem(&show_current_temp);
  group3.addItem(&enable_temp_color);
  group3.addItem(&temp_color);
  group3.addItem(&current_temp_interval);
  group3.addItem(&current_temp_duration);

  group4.addItem(&show_current_weather);
  group4.addItem(&current_weather_color);
  group4.addItem(&current_weather_interval);
  group4.addItem(&current_weather_short_text);

  group5.addItem(&show_daily_weather);
  group5.addItem(&daily_weather_color);
  group5.addItem(&daily_weather_interval);
  group5.addItem(&daily_weather_short_text);

  group6.addItem(&show_aqi);
  group6.addItem(&enable_aqi_color);
  group6.addItem(&aqi_color);
  group6.addItem(&aqi_interval);

  group11.addItem(&alert_interval);

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

  group10.addItem(&gps_baud);

  group13.addItem(&hardware_profile);
  group13.addItem(&led_data_pin);
  group13.addItem(&gps_rx_pin);
  group13.addItem(&gps_tx_pin);
  group13.addItem(&i2c_sda_pin);
  group13.addItem(&i2c_scl_pin);
  group13.addItem(&matrix_origin);
  group13.addItem(&matrix_corner);
  group13.addItem(&matrix_axis);
  group13.addItem(&matrix_order);

  group12.addItem(&serialdebug);
  group12.addItem(&resetdefaults);
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
  iotWebConf.addParameterGroup(&group11);
  iotWebConf.addParameterGroup(&group7);
  iotWebConf.addParameterGroup(&group8);
  iotWebConf.addParameterGroup(&group9);
  iotWebConf.addParameterGroup(&group13);
  iotWebConf.addParameterGroup(&group10);
  iotWebConf.addParameterGroup(&group12);
}
} // namespace

const char *compiledHardwareProfile()
{
  return LEDCLOCK_BOARD_PROFILE;
}

HardwarePinSettings defaultHardwarePinSettings()
{
  return HardwarePinSettings{
      static_cast<int16_t>(LEDCLOCK_DEFAULT_LED_DATA_PIN),
      static_cast<int16_t>(LEDCLOCK_DEFAULT_GPS_RX_PIN),
      static_cast<int16_t>(LEDCLOCK_DEFAULT_GPS_TX_PIN),
      static_cast<int16_t>(LEDCLOCK_DEFAULT_I2C_SDA_PIN),
      static_cast<int16_t>(LEDCLOCK_DEFAULT_I2C_SCL_PIN),
  };
}

HardwarePinSettings rawConfiguredHardwarePinSettings()
{
  return HardwarePinSettings{
      led_data_pin.value(),
      gps_rx_pin.value(),
      gps_tx_pin.value(),
      i2c_sda_pin.value(),
      i2c_scl_pin.value(),
  };
}

HardwarePinSettings configuredHardwarePinSettings()
{
  return hardwarePinParametersReady ? rawConfiguredHardwarePinSettings() : defaultHardwarePinSettings();
}

MatrixLayoutSettings defaultMatrixLayoutSettings()
{
  return MatrixLayoutSettings{
      kDefaultMatrixOriginValue,
      kDefaultMatrixCornerValue,
      kDefaultMatrixAxisValue,
      kDefaultMatrixOrderValue,
  };
}

MatrixLayoutSettings rawConfiguredMatrixLayoutSettings()
{
  return MatrixLayoutSettings{
      matrix_origin.value(),
      matrix_corner.value(),
      matrix_axis.value(),
      matrix_order.value(),
  };
}

MatrixLayoutSettings configuredMatrixLayoutSettings()
{
  return hardwarePinParametersReady ? rawConfiguredMatrixLayoutSettings() : defaultMatrixLayoutSettings();
}

uint8_t matrixLayoutFlags(const MatrixLayoutSettings &settings)
{
  const char *originValue = settings.origin != nullptr ? settings.origin : "";
  const char *cornerValue = settings.corner != nullptr ? settings.corner : "";
  const char *axisValue = settings.axis != nullptr ? settings.axis : "";
  const char *orderValue = settings.order != nullptr ? settings.order : "";
  const uint8_t origin = strcmp(originValue, "top") == 0 ? NEO_MATRIX_TOP : NEO_MATRIX_BOTTOM;
  const uint8_t corner = strcmp(cornerValue, "left") == 0 ? NEO_MATRIX_LEFT : NEO_MATRIX_RIGHT;
  const uint8_t axis = strcmp(axisValue, "rows") == 0 ? NEO_MATRIX_ROWS : NEO_MATRIX_COLUMNS;
  const uint8_t order = strcmp(orderValue, "progressive") == 0 ? NEO_MATRIX_PROGRESSIVE : NEO_MATRIX_ZIGZAG;
  return origin + corner + axis + order;
}

uint8_t defaultMatrixLayoutFlags()
{
  return matrixLayoutFlags(defaultMatrixLayoutSettings());
}

uint8_t configuredMatrixLayoutFlags()
{
  return matrixLayoutFlags(configuredMatrixLayoutSettings());
}

bool isHardwareInputPinUsable(int16_t pin)
{
  return isGpioInConfiguredRange(pin) && !isReservedFlashGpio(pin) && !isTargetInvalidGpio(pin);
}

bool isHardwareOutputPinUsable(int16_t pin)
{
  return isHardwareInputPinUsable(pin) && !isClassicEsp32InputOnlyGpio(pin);
}

bool isSupportedLedDataPin(int16_t pin)
{
  switch (pin)
  {
#if defined(CONFIG_IDF_TARGET_ESP32)
    case 2:
    case 4:
    case 5:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 21:
    case 22:
    case 23:
    case 25:
    case 26:
    case 27:
    case 32:
    case 33:
      return true;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
    case 2:
    case 4:
    case 5:
    case 12:
    case 13:
    case 14:
    case 15:
    case 16:
    case 17:
    case 18:
    case 19:
    case 21:
    case 26:
    case 33:
    case 34:
    case 35:
    case 36:
    case 37:
    case 38:
    case 39:
#if LEDCLOCK_MAX_GPIO_PIN >= 40
    case 40:
    case 41:
    case 42:
    case 43:
    case 44:
    case 45:
    case 46:
    case 47:
    case 48:
#endif
      return true;
#else
    case 2:
    case 4:
    case 5:
      return true;
#endif
    default:
      return false;
  }
}

bool isHardwarePinConfigurationValid(const HardwarePinSettings &settings, String &error)
{
  error = "";
  if (!validateOutputPin("LED data pin", settings.ledDataPin, error))
    return false;
  if (!isSupportedLedDataPin(settings.ledDataPin))
  {
    error = F("LED data pin must use one of the GPIOs supported by this firmware build's FastLED switch table.");
    return false;
  }
  if (!validateInputPin("GPS RX pin", settings.gpsRxPin, error))
    return false;
  if (!validateOutputPin("GPS TX pin", settings.gpsTxPin, error))
    return false;
  if (!validateOutputPin("I2C SDA pin", settings.i2cSdaPin, error))
    return false;
  if (!validateOutputPin("I2C SCL pin", settings.i2cSclPin, error))
    return false;
  if (hasDuplicateHardwarePins(settings, error))
    return false;
  if (hasCompileTimeControlPinConflict(settings, error))
    return false;
  return true;
}

bool isMatrixLayoutConfigurationValid(const MatrixLayoutSettings &settings, String &error)
{
  error = "";
  if (!isSupportedMatrixOrigin(settings.origin))
  {
    error = F("Matrix LED #0 vertical edge must be bottom or top.");
    return false;
  }
  if (!isSupportedMatrixCorner(settings.corner))
  {
    error = F("Matrix LED #0 horizontal edge must be right or left.");
    return false;
  }
  if (!isSupportedMatrixAxis(settings.axis))
  {
    error = F("Matrix wiring direction must be columns or rows.");
    return false;
  }
  if (!isSupportedMatrixOrder(settings.order))
  {
    error = F("Matrix wiring order must be zigzag or progressive.");
    return false;
  }
  return true;
}

bool normalizeHardwarePinSettings()
{
  String error;
  if (isHardwarePinConfigurationValid(configuredHardwarePinSettings(), error))
    return false;

  applyDefaultHardwarePinSettings();
  ESP_LOGW(TAG, "Invalid hardware pin configuration reset to %s defaults: %s",
           compiledHardwareProfile(), error.c_str());
  return true;
}

bool normalizeMatrixLayoutSettings()
{
  String error;
  if (isMatrixLayoutConfigurationValid(configuredMatrixLayoutSettings(), error))
    return false;

  applyDefaultMatrixLayoutSettings();
  ESP_LOGW(TAG, "Invalid matrix layout reset to project defaults: %s", error.c_str());
  return true;
}

int16_t ledDataPin()
{
  return configuredHardwarePinSettings().ledDataPin;
}

int16_t gpsRxPin()
{
  return configuredHardwarePinSettings().gpsRxPin;
}

int16_t gpsTxPin()
{
  return configuredHardwarePinSettings().gpsTxPin;
}

int16_t i2cSdaPin()
{
  return configuredHardwarePinSettings().i2cSdaPin;
}

int16_t i2cSclPin()
{
  return configuredHardwarePinSettings().i2cSclPin;
}

void normalizeLoadedConfigValues()
{
  normalizeLoadedConfigValuesImpl();
}

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
  iotWebConf.getThingNameParameter()->label = "Clock network name";
  iotWebConf.getWifiSsidParameter()->label = "Wi-Fi SSID";
  iotWebConf.getWifiPasswordParameter()->label = "Wi-Fi password";
  iotWebConf.getApPasswordParameter()->label = "Clock web password (used when protection is enabled)";
  iotWebConf.getApTimeoutParameter()->label = "Setup portal timeout in seconds";
  iotWebConf.setupUpdateServer(
      [](const char *updatePath)
      { (void)updatePath; },
      [](const char *userName, char *password)
      {
        (void)userName;
        (void)password;
      });
  iotWebConf.init();
  // The key-based Preferences store owns persistence now, so start from
  // defaults here and let the JSON/NVS loader apply authoritative values.
  iotWebConf.getRootParameterGroup()->applyDefaultValue();
  hardwarePinParametersReady = true;
  normalizeLoadedConfigValues();
}

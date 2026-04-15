#include "main.h"
#include "config_backup.h"
#include <Preferences.h>

namespace
{
/** Describes one stable config field that can be exported and restored by key. */
struct ConfigBackupField
{
  /** Stable JSON key used in the backup file. */
  const char *id;
  /** Writes the current live value into the JSON backup document. */
  void (*exportValue)(JsonObject values, const char *key);
  /** Validates and applies one imported JSON value into live config memory. */
  bool (*importValue)(JsonVariantConst value);
};

/** Writes a classic IotWebConf text/password buffer into the JSON values object. */
void exportClassicBuffer(JsonObject values, const char *key, iotwebconf::Parameter &parameter)
{
  values[key] = parameter.valueBuffer;
}

/** Converts an imported JSON value into a String for text-like fields. */
bool parseImportedString(JsonVariantConst value, String &parsed)
{
  if (value.isNull())
    return false;

  if (value.is<const char *>())
  {
    parsed = String(value.as<const char *>());
    return true;
  }

  if (value.is<String>())
  {
    parsed = value.as<String>();
    return true;
  }

  if (value.is<bool>())
  {
    parsed = value.as<bool>() ? F("true") : F("false");
    return true;
  }

  if (value.is<long>() || value.is<int>() || value.is<unsigned long>() || value.is<unsigned int>())
  {
    parsed = String(value.as<long>());
    return true;
  }

  return false;
}

/** Parses a JSON value into a bool while accepting common legacy string forms. */
bool parseImportedBool(JsonVariantConst value, bool &parsed)
{
  if (value.is<bool>())
  {
    parsed = value.as<bool>();
    return true;
  }

  if (value.is<int>() || value.is<long>() || value.is<unsigned int>() || value.is<unsigned long>())
  {
    parsed = value.as<long>() != 0;
    return true;
  }

  String rendered;
  if (!parseImportedString(value, rendered))
    return false;

  rendered.trim();
  rendered.toLowerCase();
  if (rendered == "true" || rendered == "1" || rendered == "yes" || rendered == "on" || rendered == "selected")
  {
    parsed = true;
    return true;
  }
  if (rendered == "false" || rendered == "0" || rendered == "no" || rendered == "off" || rendered.length() == 0)
  {
    parsed = false;
    return true;
  }

  return false;
}

/** Parses a JSON value into a bounded signed integer. */
bool parseImportedInt(JsonVariantConst value, int32_t &parsed)
{
  if (value.is<int>() || value.is<long>() || value.is<unsigned int>() || value.is<unsigned long>())
  {
    parsed = value.as<long>();
    return true;
  }

  String rendered;
  if (!parseImportedString(value, rendered))
    return false;

  char *end = nullptr;
  long converted = strtol(rendered.c_str(), &end, 10);
  if (end == rendered.c_str() || (end != nullptr && *end != '\0'))
    return false;

  parsed = static_cast<int32_t>(converted);
  return true;
}

/** Validates a color-string backup field before restoring it. */
bool isColorValueValid(const String &value)
{
  if (value.length() != 7 || value.charAt(0) != '#')
    return false;

  for (size_t index = 1; index < value.length(); ++index)
  {
    if (!isxdigit(static_cast<unsigned char>(value.charAt(index))))
      return false;
  }
  return true;
}

/** Restores a classic IotWebConf text/password buffer from imported JSON. */
bool importClassicBuffer(JsonVariantConst value, iotwebconf::Parameter &parameter)
{
  String rendered;
  if (!parseImportedString(value, rendered))
    return false;

  if (rendered.length() + 1 > static_cast<size_t>(parameter.getLength()))
    return false;

  strlcpy(parameter.valueBuffer, rendered.c_str(), static_cast<size_t>(parameter.getLength()));
  return true;
}

/** Restores any fixed-size character-array parameter from imported JSON. */
template <size_t N>
bool importCharArrayValue(JsonVariantConst value, char (&buffer)[N])
{
  String rendered;
  if (!parseImportedString(value, rendered))
    return false;

  if (rendered.length() + 1 > N)
    return false;

  strlcpy(buffer, rendered.c_str(), N);
  return true;
}

/** Restores a validated HTML color input value from imported JSON. */
template <size_t N>
bool importColorValue(JsonVariantConst value, char (&buffer)[N])
{
  String rendered;
  if (!parseImportedString(value, rendered) || !isColorValueValid(rendered))
    return false;

  if (rendered.length() + 1 > N)
    return false;

  strlcpy(buffer, rendered.c_str(), N);
  return true;
}

/** Restores a boolean checkbox-style setting from imported JSON. */
bool importBoolValue(JsonVariantConst value, bool &destination)
{
  bool parsed = false;
  if (!parseImportedBool(value, parsed))
    return false;

  destination = parsed;
  return true;
}

/** Restores an integer setting while enforcing the parameter's supported range. */
template <typename T>
bool importIntValue(JsonVariantConst value, T &destination, int32_t minValue, int32_t maxValue)
{
  int32_t parsed = 0;
  if (!parseImportedInt(value, parsed))
    return false;

  if (parsed < minValue || parsed > maxValue)
    return false;

  destination = static_cast<T>(parsed);
  return true;
}

#define CLASSIC_FIELD(idLiteral, paramExpr)                                                                                  \
  {                                                                                                                          \
    idLiteral, [](JsonObject values, const char *key) { exportClassicBuffer(values, key, *(paramExpr)); },                  \
      [](JsonVariantConst value) { return importClassicBuffer(value, *(paramExpr)); }                                        \
  }

#define TEXT_FIELD(idLiteral, param)                                                                                         \
  {                                                                                                                          \
    idLiteral, [](JsonObject values, const char *key) { values[key] = param.value(); },                                     \
      [](JsonVariantConst value) { return importCharArrayValue(value, param.value()); }                                      \
  }

#define COLOR_FIELD(idLiteral, param)                                                                                        \
  {                                                                                                                          \
    idLiteral, [](JsonObject values, const char *key) { values[key] = param.value(); },                                     \
      [](JsonVariantConst value) { return importColorValue(value, param.value()); }                                          \
  }

#define BOOL_FIELD(idLiteral, param)                                                                                         \
  {                                                                                                                          \
    idLiteral, [](JsonObject values, const char *key) { values[key] = param.isChecked(); },                                 \
      [](JsonVariantConst value) { return importBoolValue(value, param.value()); }                                           \
  }

#define INT_FIELD(idLiteral, param, minValue, maxValue)                                                                      \
  {                                                                                                                          \
    idLiteral, [](JsonObject values, const char *key) { values[key] = param.value(); },                                     \
      [](JsonVariantConst value) { return importIntValue(value, param.value(), static_cast<int32_t>(minValue),              \
                                                         static_cast<int32_t>(maxValue)); }                                  \
  }

/** Stable backup schema used to export and import config by key instead of storage order. */
const ConfigBackupField kConfigBackupFields[] = {
    CLASSIC_FIELD("iwcThingName", iotWebConf.getThingNameParameter()),
    CLASSIC_FIELD("iwcApPassword", iotWebConf.getApPasswordParameter()),
    CLASSIC_FIELD("iwcWifiSsid", iotWebConf.getWifiSsidParameter()),
    CLASSIC_FIELD("iwcWifiPassword", iotWebConf.getWifiPasswordParameter()),
    CLASSIC_FIELD("iwcApTimeout", iotWebConf.getApTimeoutParameter()),
    BOOL_FIELD("serialdebug", serialdebug),
    TEXT_FIELD("ipgeoapi", ipgeoapi),
    TEXT_FIELD("weatherapi", weatherapi),
    TEXT_FIELD("savedlat", savedlat),
    TEXT_FIELD("savedlon", savedlon),
    INT_FIELD("tzoffset", savedtzoffset, -12, 12),
    TEXT_FIELD("savedcity", savedcity),
    TEXT_FIELD("savedstate", savedstate),
    TEXT_FIELD("savedcountry", savedcountry),
    BOOL_FIELD("imperial", imperial),
    INT_FIELD("brightness_level", brightness_level, 1, 10),
    INT_FIELD("text_scroll_speed", text_scroll_speed, 1, 10),
    COLOR_FIELD("system_color", system_color),
    BOOL_FIELD("show_date", show_date),
    COLOR_FIELD("date_color", date_color),
    INT_FIELD("date_interval", date_interval, 1, 24),
    BOOL_FIELD("enable_alertflash", enable_alertflash),
    BOOL_FIELD("twelve_clock", twelve_clock),
    BOOL_FIELD("enable_fixed_tz", enable_fixed_tz),
    INT_FIELD("fixed_offset", fixed_offset, -12, 12),
    TEXT_FIELD("ntp_server", ntp_server),
    BOOL_FIELD("override_dhcp_ntp", override_dhcp_ntp),
    BOOL_FIELD("colonflicker", colonflicker),
    BOOL_FIELD("flickerfast", flickerfast),
    BOOL_FIELD("enable_clock_color", enable_clock_color),
    COLOR_FIELD("clock_color", clock_color),
    BOOL_FIELD("show_current_temp", show_current_temp),
    BOOL_FIELD("enable_temp_color", enable_temp_color),
    COLOR_FIELD("temp_color", temp_color),
    INT_FIELD("current_temp_interval", current_temp_interval, 1, 120),
    INT_FIELD("current_temp_duration", current_temp_duration, 5, 60),
    BOOL_FIELD("show_current_weather", show_current_weather),
    COLOR_FIELD("current_weather_color", current_weather_color),
    INT_FIELD("current_weather_interval", current_weather_interval, 1, 24),
    BOOL_FIELD("show_daily_weather", show_daily_weather),
    COLOR_FIELD("daily_weather_color", daily_weather_color),
    INT_FIELD("daily_weather_interval", daily_weather_interval, 1, 24),
    BOOL_FIELD("show_aqi", show_aqi),
    COLOR_FIELD("aqi_color", aqi_color),
    BOOL_FIELD("enable_aqi_color", enable_aqi_color),
    INT_FIELD("aqi_interval", aqi_interval, 1, 120),
    INT_FIELD("alert_interval", alert_interval, 1, 60),
    BOOL_FIELD("enable_system_status", enable_system_status),
    BOOL_FIELD("enable_aqi_status", enable_aqi_status),
    BOOL_FIELD("enable_uvi_status", enable_uvi_status),
    BOOL_FIELD("green_status", green_status),
    BOOL_FIELD("show_sunrise", show_sunrise),
    COLOR_FIELD("sunrise_color", sunrise_color),
    TEXT_FIELD("sunrise_message", sunrise_message),
    BOOL_FIELD("show_sunset", show_sunset),
    COLOR_FIELD("sunset_color", sunset_color),
    TEXT_FIELD("sunset_message", sunset_message),
    BOOL_FIELD("show_loc_change", show_loc_change),
    BOOL_FIELD("enable_fixed_loc", enable_fixed_loc),
    TEXT_FIELD("fixedLat", fixedLat),
    TEXT_FIELD("fixedLon", fixedLon),
};

constexpr char kConfigStoreNamespace[] = "ledclock";
constexpr char kConfigStoreKey[] = "config_json";

/** Returns true when the given backup key matches a field known to this firmware. */
bool isKnownBackupField(const char *fieldId)
{
  for (const ConfigBackupField &field : kConfigBackupFields)
  {
    if (strcmp(field.id, fieldId) == 0)
      return true;
  }
  return false;
}

/** Returns the JSON object that actually contains key/value settings in a backup file. */
JsonObjectConst getImportedValuesObject(JsonDocument &document)
{
  if (document["values"].is<JsonObjectConst>())
    return document["values"].as<JsonObjectConst>();
  return document.as<JsonObjectConst>();
}

/** Populates a JSON document with the live configuration using stable field IDs. */
bool buildConfigurationDocument(JsonDocument &document, bool includeMetadata)
{
  document.clear();

  if (includeMetadata)
  {
    JsonObject metadata = document.createNestedObject("metadata");
    metadata["format"] = "ledsmartclock-config";
    metadata["format_version"] = 1;
    metadata["firmware_version"] = VERSION_SEMVER;
    metadata["exported_at"] = systemClock.isInit() ? getSystemZonedDateTimeString() : String(F("Unavailable"));
    metadata["time_source"] = runtimeState.timeSource;
  }

  JsonObject values = document.createNestedObject("values");
  for (const ConfigBackupField &field : kConfigBackupFields)
    field.exportValue(values, field.id);

  return !document.overflowed();
}

/** Serializes the live configuration into JSON for backup or persisted storage. */
bool serializeLiveConfiguration(String &json, String &error, bool includeMetadata, bool pretty)
{
  error = "";
  json = "";

  DynamicJsonDocument document(16384);
  if (!buildConfigurationDocument(document, includeMetadata))
  {
    error = F("Unable to allocate configuration document.");
    return false;
  }

  size_t bytesWritten = pretty ? serializeJsonPretty(document, json) : serializeJson(document, json);
  if (bytesWritten == 0)
  {
    error = F("Failed to serialize the configuration document.");
    return false;
  }

  return true;
}

/** Applies imported key/value settings into live memory without persisting them yet. */
bool applyConfigurationValues(JsonObjectConst values, ConfigImportResult &result)
{
  result = ConfigImportResult{};

  for (JsonPairConst entry : values)
  {
    if (!isKnownBackupField(entry.key().c_str()))
      ++result.ignoredCount;
  }

  for (const ConfigBackupField &field : kConfigBackupFields)
  {
    if (!values.containsKey(field.id))
      continue;

    if (field.importValue(values[field.id]))
      ++result.appliedCount;
    else
      ++result.rejectedCount;
  }

  if (result.appliedCount == 0)
  {
    result.error = F("The uploaded backup did not contain any recognized settings for this firmware.");
    return false;
  }

  resetdefaults.value() = false;
  normalizeLoadedConfigValues();

  result.success = true;
  result.hasWarnings = result.ignoredCount > 0 || result.rejectedCount > 0;
  result.summary = String(F("Imported ")) + result.appliedCount + F(" setting") +
                   (result.appliedCount == 1 ? F("") : F("s")) + F(".");

  if (result.ignoredCount > 0)
  {
    result.summary += F(" Ignored ");
    result.summary += result.ignoredCount;
    result.summary += F(" unknown field");
    result.summary += (result.ignoredCount == 1 ? F("") : F("s"));
    result.summary += F(".");
  }

  if (result.rejectedCount > 0)
  {
    result.summary += F(" Rejected ");
    result.summary += result.rejectedCount;
    result.summary += F(" invalid field");
    result.summary += (result.rejectedCount == 1 ? F("") : F("s"));
    result.summary += F(".");
  }

  return true;
}
} // namespace

bool exportConfigurationBackup(String &json, String &error)
{
  return serializeLiveConfiguration(json, error, true, true);
}

bool importConfigurationBackup(const String &json, ConfigImportResult &result)
{
  result = ConfigImportResult{};

  if (json.length() == 0)
  {
    result.error = F("The uploaded backup file was empty.");
    return false;
  }

  DynamicJsonDocument document(24576);
  DeserializationError parseError = deserializeJson(document, json);
  if (parseError)
  {
    result.error = String(F("Could not parse the uploaded JSON backup: ")) + parseError.c_str();
    return false;
  }

  JsonObjectConst values = getImportedValuesObject(document);
  if (values.isNull())
  {
    result.error = F("The uploaded backup did not contain a usable values object.");
    return false;
  }

  if (!applyConfigurationValues(values, result))
    return false;

  String saveError;
  if (!saveStoredConfiguration(saveError))
  {
    result.success = false;
    result.error = saveError;
    return false;
  }

  applyRuntimeConfiguration();

  return true;
}

String configBackupDownloadFileName()
{
  return String(F("ledsmartclock-config-")) + VERSION_SEMVER + F(".json");
}

bool hasStoredConfiguration()
{
  Preferences preferences;
  if (!preferences.begin(kConfigStoreNamespace, true))
    return false;

  bool present = preferences.isKey(kConfigStoreKey);
  preferences.end();
  return present;
}

bool loadStoredConfiguration(String &error)
{
  error = "";
  Preferences preferences;
  if (!preferences.begin(kConfigStoreNamespace, true))
  {
    error = F("Failed to open the persisted configuration store.");
    return false;
  }

  String json = preferences.getString(kConfigStoreKey, "");
  preferences.end();

  if (json.length() == 0)
  {
    error = F("The persisted configuration store was empty.");
    return false;
  }

  DynamicJsonDocument document(24576);
  DeserializationError parseError = deserializeJson(document, json);
  if (parseError)
  {
    error = String(F("Could not parse the persisted configuration JSON: ")) + parseError.c_str();
    return false;
  }

  JsonObjectConst values = getImportedValuesObject(document);
  if (values.isNull())
  {
    error = F("The persisted configuration JSON did not contain a usable values object.");
    return false;
  }

  iotWebConf.getRootParameterGroup()->applyDefaultValue();

  ConfigImportResult loadResult;
  if (!applyConfigurationValues(values, loadResult))
  {
    error = loadResult.error.length() > 0 ? loadResult.error : String(F("The persisted configuration did not contain any recognized settings."));
    return false;
  }

  return true;
}

bool saveStoredConfiguration(String &error)
{
  String json;
  if (!serializeLiveConfiguration(json, error, false, false))
    return false;

  Preferences preferences;
  if (!preferences.begin(kConfigStoreNamespace, false))
  {
    error = F("Failed to open the persisted configuration store for writing.");
    return false;
  }

  size_t bytesWritten = preferences.putString(kConfigStoreKey, json);
  preferences.end();

  if (bytesWritten == 0)
  {
    error = F("Failed to write the persisted configuration store.");
    return false;
  }

  return true;
}

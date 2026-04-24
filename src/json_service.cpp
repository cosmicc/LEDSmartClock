#include "main.h"

namespace
{
constexpr size_t kAlertFilterCapacity = 4096U;
constexpr size_t kAlertDocumentCapacity = 16384U;
constexpr size_t kAlertDisplayTarget = 220U;
constexpr size_t kAlertSentenceTarget = 140U;
constexpr size_t kAqiDocumentCapacity = 24U * 1024U;
constexpr uint8_t kAqiUnknown = 0U;
constexpr uint8_t kAqiMin = 1U;
constexpr uint8_t kAqiMax = 5U;

/**
 * Copies a JSON string field into a fixed-size destination, defaulting to an
 * empty string when the source field is absent.
 */
template <size_t N>
void copyJsonString(char (&dest)[N], JsonVariantConst value)
{
  const char *text = value.isNull() ? "" : value.as<const char *>();
  snprintf(dest, N, "%s", text == nullptr ? "" : text);
}

/** Clears one retained alert entry back to an empty state. */
void clearAlertEntry(AlertEntry &entry)
{
  entry.status[0] = '\0';
  entry.severity[0] = '\0';
  entry.certainty[0] = '\0';
  entry.urgency[0] = '\0';
  entry.event[0] = '\0';
  entry.headline[0] = '\0';
  entry.description[0] = '\0';
  entry.instruction[0] = '\0';
  entry.displayText[0] = '\0';
  entry.categoryRank = 0;
  entry.severityRank = 0;
  entry.urgencyRank = 0;
  entry.certaintyRank = 0;
  entry.warning = false;
  entry.watch = false;
}

/** Clears the shared active-alert state before loading a fresh payload. */
void clearAlertState()
{
  alerts.active = false;
  alerts.inWatch = false;
  alerts.inWarning = false;
  alerts.count = 0;
  alerts.watchCount = 0;
  alerts.warningCount = 0;
  alerts.displayIndex = 0;
  alerts.timestamp = 0;

  for (uint8_t index = 0; index < kMaxTrackedAlerts; ++index)
    clearAlertEntry(alerts.items[index]);
}

/** Copies, sanitizes, and reports whether a JSON string produced visible text. */
template <size_t N>
bool copyJsonStringClean(char (&dest)[N], JsonVariantConst value)
{
  copyJsonString(dest, value);
  cleanString(dest);
  return hasVisibleText(dest);
}

/** Returns true when haystack contains needle, ignoring ASCII case. */
bool containsIgnoreCase(const char *haystack, const char *needle)
{
  if (!hasVisibleText(haystack) || !hasVisibleText(needle))
    return false;

  String haystackText(haystack);
  String needleText(needle);
  haystackText.toLowerCase();
  needleText.toLowerCase();
  return haystackText.indexOf(needleText) >= 0;
}

/** Returns true when two alert strings are equivalent, ignoring case. */
bool equalsIgnoreCase(const char *left, const char *right)
{
  if (!hasVisibleText(left) || !hasVisibleText(right))
    return false;

  return strcasecmp(left, right) == 0;
}

/** Maps the alert event title to a coarse priority bucket. */
uint8_t alertCategoryRank(const char *event)
{
  if (containsIgnoreCase(event, "Emergency"))
    return 5;
  if (containsIgnoreCase(event, "Warning"))
    return 4;
  if (containsIgnoreCase(event, "Watch"))
    return 3;
  if (containsIgnoreCase(event, "Advisory"))
    return 2;
  if (containsIgnoreCase(event, "Statement") || containsIgnoreCase(event, "Outlook"))
    return 1;
  return 0;
}

/** Maps weather.gov severity text into a sort rank. */
uint8_t alertSeverityRank(const char *severity)
{
  if (equalsIgnoreCase(severity, "Extreme"))
    return 4;
  if (equalsIgnoreCase(severity, "Severe"))
    return 3;
  if (equalsIgnoreCase(severity, "Moderate"))
    return 2;
  if (equalsIgnoreCase(severity, "Minor"))
    return 1;
  return 0;
}

/** Maps weather.gov urgency text into a sort rank. */
uint8_t alertUrgencyRank(const char *urgency)
{
  if (equalsIgnoreCase(urgency, "Immediate"))
    return 4;
  if (equalsIgnoreCase(urgency, "Expected"))
    return 3;
  if (equalsIgnoreCase(urgency, "Future"))
    return 2;
  if (equalsIgnoreCase(urgency, "Past"))
    return 1;
  return 0;
}

/** Maps weather.gov certainty text into a sort rank. */
uint8_t alertCertaintyRank(const char *certainty)
{
  if (equalsIgnoreCase(certainty, "Observed"))
    return 4;
  if (equalsIgnoreCase(certainty, "Likely"))
    return 3;
  if (equalsIgnoreCase(certainty, "Possible"))
    return 2;
  if (equalsIgnoreCase(certainty, "Unlikely"))
    return 1;
  return 0;
}

/** Returns true when the retained alert should be treated as warning-level. */
bool isWarningAlert(const AlertEntry &entry)
{
  if (entry.categoryRank >= 4)
    return true;
  if (entry.categoryRank > 0)
    return false;
  return entry.certaintyRank >= 3;
}

/** Returns true when the retained alert should be treated as watch-level. */
bool isWatchAlert(const AlertEntry &entry)
{
  if (entry.categoryRank == 3)
    return true;
  if (entry.categoryRank >= 4)
    return false;
  return entry.certaintyRank == 2;
}

/** Copies only the first sentence or line of a longer alert paragraph. */
template <size_t N>
void copyFirstSentence(char (&dest)[N], const char *source, size_t targetLength = N - 1U)
{
  dest[0] = '\0';
  if (!hasVisibleText(source))
    return;

  const size_t maxLength = min(targetLength, N - 1U);
  size_t writeIndex = 0;
  bool truncated = false;
  for (size_t readIndex = 0; source[readIndex] != '\0' && writeIndex < maxLength; ++readIndex)
  {
    char ch = source[readIndex];
    if (ch == '\r' || ch == '\n')
      break;

    dest[writeIndex++] = ch;

    if (ch == '.' || ch == '!' || ch == '?')
      break;

    if (writeIndex == maxLength && source[readIndex + 1] != '\0')
      truncated = true;
  }

  while (writeIndex > 0 && isspace(static_cast<unsigned char>(dest[writeIndex - 1])))
    --writeIndex;

  if (truncated && writeIndex >= 3U)
  {
    dest[writeIndex - 3U] = '.';
    dest[writeIndex - 2U] = '.';
    dest[writeIndex - 1U] = '.';
  }

  dest[writeIndex] = '\0';
}

/** Appends one alert clause using sentence-style separation. */
template <size_t N>
void appendAlertClause(char (&dest)[N], const char *text)
{
  if (!hasVisibleText(text))
    return;

  if (dest[0] != '\0')
    strlcat(dest, ". ", N);
  strlcat(dest, text, N);
}

/** Trims a mutable alert string to the desired maximum length with ellipsis. */
template <size_t N>
void truncateWithEllipsis(char (&text)[N], size_t targetLength)
{
  if (targetLength >= N)
    targetLength = N - 1U;

  size_t length = strlen(text);
  if (length <= targetLength)
    return;

  if (targetLength < 4U)
  {
    text[targetLength] = '\0';
    return;
  }

  text[targetLength - 3U] = '.';
  text[targetLength - 2U] = '.';
  text[targetLength - 1U] = '.';
  text[targetLength] = '\0';
}

/** Builds the matrix-friendly scroll text for one retained alert. */
void buildAlertDisplayText(AlertEntry &entry, uint8_t index, uint8_t totalCount)
{
  entry.displayText[0] = '\0';

  char prefix[32]{};
  char shortHeadline[160]{};
  char shortDescription[160]{};
  char shortInstruction[128]{};
  copyFirstSentence(shortHeadline, entry.headline, kAlertSentenceTarget);
  copyFirstSentence(shortDescription, entry.description, kAlertSentenceTarget);
  copyFirstSentence(shortInstruction, entry.instruction, 96U);

  if (totalCount > 1U)
    snprintf(prefix, sizeof(prefix), "Alert %u/%u", index + 1U, totalCount);

  if (prefix[0] != '\0')
    appendAlertClause(entry.displayText, prefix);

  const char *title = hasVisibleText(entry.event) ? entry.event : (hasVisibleText(entry.headline) ? entry.headline : entry.description);
  appendAlertClause(entry.displayText, title);

  const char *summary = hasVisibleText(shortHeadline) ? shortHeadline : shortDescription;
  if (hasVisibleText(summary) && !equalsIgnoreCase(summary, title) && !containsIgnoreCase(summary, title))
    appendAlertClause(entry.displayText, summary);

  char candidate[sizeof(entry.displayText)]{};
  strlcpy(candidate, entry.displayText, sizeof(candidate));
  if (hasVisibleText(shortInstruction) && !containsIgnoreCase(candidate, shortInstruction))
  {
    appendAlertClause(candidate, shortInstruction);
    if (strlen(candidate) <= kAlertDisplayTarget)
      strlcpy(entry.displayText, candidate, sizeof(entry.displayText));
  }

  if (!hasVisibleText(entry.displayText))
    strlcpy(entry.displayText, title, sizeof(entry.displayText));

  truncateWithEllipsis(entry.displayText, kAlertDisplayTarget);
}

/** Returns true when left should sort ahead of right in the alert list. */
bool alertHigherPriority(const AlertEntry &left, const AlertEntry &right)
{
  if (left.categoryRank != right.categoryRank)
    return left.categoryRank > right.categoryRank;
  if (left.severityRank != right.severityRank)
    return left.severityRank > right.severityRank;
  if (left.urgencyRank != right.urgencyRank)
    return left.urgencyRank > right.urgencyRank;
  if (left.certaintyRank != right.certaintyRank)
    return left.certaintyRank > right.certaintyRank;
  return strcasecmp(left.event, right.event) < 0;
}

/** Sorts the retained alert list from highest to lowest priority. */
void sortRetainedAlerts()
{
  for (uint8_t left = 0; left + 1U < alerts.count; ++left)
  {
    uint8_t bestIndex = left;
    for (uint8_t right = left + 1U; right < alerts.count; ++right)
    {
      if (alertHigherPriority(alerts.items[right], alerts.items[bestIndex]))
        bestIndex = right;
    }

    if (bestIndex != left)
    {
      AlertEntry swap = alerts.items[left];
      alerts.items[left] = alerts.items[bestIndex];
      alerts.items[bestIndex] = swap;
    }
  }
}

/** Copies one weather.gov feature into a retained alert entry. */
void loadAlertEntry(JsonVariantConst feature, AlertEntry &entry)
{
  JsonVariantConst properties = feature["properties"];
  copyJsonStringClean(entry.status, properties["status"]);
  copyJsonStringClean(entry.severity, properties["severity"]);
  copyJsonStringClean(entry.certainty, properties["certainty"]);
  copyJsonStringClean(entry.urgency, properties["urgency"]);
  copyJsonStringClean(entry.event, properties["event"]);

  const char *headlineSource = "empty";
  if (copyJsonStringClean(entry.headline, properties["parameters"]["NWSheadline"][0]))
    headlineSource = "parameters.NWSheadline";
  else if (copyJsonStringClean(entry.headline, properties["headline"]))
    headlineSource = "headline";
  else if (copyJsonStringClean(entry.headline, properties["event"]))
    headlineSource = "event";
  else
    entry.headline[0] = '\0';

  const char *descriptionSource = "empty";
  if (copyJsonStringClean(entry.description, properties["description"]))
    descriptionSource = "description";
  else if (copyJsonStringClean(entry.description, properties["headline"]))
    descriptionSource = "headline";
  else if (copyJsonStringClean(entry.description, properties["event"]))
    descriptionSource = "event";
  else
    entry.description[0] = '\0';

  if (!copyJsonStringClean(entry.instruction, properties["instruction"]))
    entry.instruction[0] = '\0';

  entry.categoryRank = alertCategoryRank(entry.event);
  entry.severityRank = alertSeverityRank(entry.severity);
  entry.urgencyRank = alertUrgencyRank(entry.urgency);
  entry.certaintyRank = alertCertaintyRank(entry.certainty);
  entry.warning = isWarningAlert(entry);
  entry.watch = isWatchAlert(entry);

  ESP_LOGI(TAG,
           "Loaded alert: Event=%s | HeadlineSource=%s | DescriptionSource=%s | Category=%u | Severity=%u | Urgency=%u | Certainty=%u",
           hasVisibleText(entry.event) ? entry.event : "(unnamed)",
           headlineSource,
           descriptionSource,
           entry.categoryRank,
           entry.severityRank,
           entry.urgencyRank,
           entry.certaintyRank);
}

bool readAqiBucket(JsonVariantConst value, const char *label, uint8_t &bucket)
{
  if (!value.is<int>())
  {
    ESP_LOGE(TAG, "AQI payload is missing the %s AQI bucket", label);
    bucket = kAqiUnknown;
    return false;
  }

  const int parsed = value.as<int>();
  if (parsed < kAqiMin || parsed > kAqiMax)
  {
    ESP_LOGE(TAG, "AQI payload has invalid %s AQI bucket: %d", label, parsed);
    bucket = kAqiUnknown;
    return false;
  }

  bucket = static_cast<uint8_t>(parsed);
  return true;
}
} // namespace

bool fillAlertsFromJson(const String &payload)
{
  DynamicJsonDocument filter(kAlertFilterCapacity);
  for (uint8_t index = 0; index < kMaxTrackedAlerts; ++index)
  {
    JsonObject properties = filter["features"][index]["properties"];
    properties["status"] = true;
    properties["severity"] = true;
    properties["certainty"] = true;
    properties["urgency"] = true;
    properties["event"] = true;
    properties["headline"] = true;
    properties["description"] = true;
    properties["instruction"] = true;
    properties["parameters"]["NWSheadline"] = true;
  }

  DynamicJsonDocument doc(kAlertDocumentCapacity);
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error)
  {
    ESP_LOGE(TAG, "Alerts deserializeJson() failed: %s", error.c_str());
    return false;
  }

  JsonArrayConst features = doc["features"].as<JsonArrayConst>();
  clearAlertState();

  uint8_t featureIndex = 0;
  for (JsonVariantConst feature : features)
  {
    if (alerts.count >= kMaxTrackedAlerts)
      break;

    AlertEntry entry;
    clearAlertEntry(entry);
    loadAlertEntry(feature, entry);
    if (!hasVisibleText(entry.event) && !hasVisibleText(entry.headline) && !hasVisibleText(entry.description))
    {
      ESP_LOGD(TAG, "Skipping weather alert %u because it had no visible event or description text", featureIndex + 1U);
      ++featureIndex;
      continue;
    }

    alerts.items[alerts.count] = entry;
    ++alerts.count;
    ++featureIndex;
  }

  if (alerts.count == 0)
  {
    showready.alerts = false;
    ESP_LOGI(TAG, "No current active weather alerts");
    return true;
  }

  sortRetainedAlerts();

  alerts.active = true;
  alerts.timestamp = systemClock.getNow();
  alerts.displayIndex = 0;
  for (uint8_t index = 0; index < alerts.count; ++index)
  {
    AlertEntry &entry = alerts.items[index];
    if (entry.warning)
      ++alerts.warningCount;
    if (entry.watch)
      ++alerts.watchCount;
    buildAlertDisplayText(entry, index, alerts.count);
  }

  alerts.inWarning = alerts.warningCount > 0;
  alerts.inWatch = alerts.watchCount > 0;
  showready.alerts = true;

  ESP_LOGI(TAG, "Retained %u active weather alert(s): warnings=%u watches=%u primary=%s",
           alerts.count,
           alerts.warningCount,
           alerts.watchCount,
           hasVisibleText(alerts.items[0].event) ? alerts.items[0].event : "(unnamed)");
  for (uint8_t index = 0; index < alerts.count; ++index)
  {
    const AlertEntry &entry = alerts.items[index];
    ESP_LOGI(TAG, "Alert %u/%u ranked: %s | Display: %s",
             index + 1U,
             alerts.count,
             hasVisibleText(entry.event) ? entry.event : "(unnamed)",
             hasVisibleText(entry.displayText) ? entry.displayText : "(empty)");
  }

  return true;
}

bool fillWeatherFromJson(const String &payload)
{
  bool result = false;
  StaticJsonDocument<2048> filter;
  filter["current"]["weather"][0]["icon"] = true;
  filter["current"]["temp"] = true;
  filter["current"]["feels_like"] = true;
  filter["current"]["humidity"] = true;
  filter["current"]["wind_speed"] = true;
  filter["current"]["wind_gust"] = true;
  filter["current"]["uvi"] = true;
  filter["current"]["clouds"] = true;
  filter["current"]["weather"][0]["description"] = true;
  filter["hourly"][0]["wind_gust"] = true;
  filter["daily"][0]["temp"]["min"] = true;
  filter["daily"][0]["temp"]["max"] = true;
  filter["daily"][0]["sunrise"] = true;
  filter["daily"][0]["sunset"] = true;
  filter["daily"][0]["moonrise"] = true;
  filter["daily"][0]["moonset"] = true;
  filter["daily"][0]["uvi"] = true;
  filter["daily"][0]["clouds"] = true;
  filter["daily"][0]["humidity"] = true;
  filter["daily"][0]["wind_speed"] = true;
  filter["daily"][0]["wind_gust"] = true;
  filter["daily"][0]["moon_phase"] = true;
  filter["daily"][0]["weather"][0]["description"] = true;
  filter["daily"][0]["weather"][0]["icon"] = true;
  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error)
  {
    ESP_LOGE(TAG, "Weather deserializeJson() failed: %s", error.c_str());
    return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  if (!obj["current"]["weather"][0].isNull())
  {
    copyJsonString(weather.current.icon, obj["current"]["weather"][0]["icon"]);
    weather.current.temp = obj["current"]["temp"];
    weather.current.feelsLike = obj["current"]["feels_like"];
    weather.current.humidity = obj["current"]["humidity"];
    weather.current.windSpeed = obj["current"]["wind_speed"];
    weather.current.windGust = obj["current"]["wind_gust"] | obj["hourly"][0]["wind_gust"] | 0;
    weather.current.uvi = obj["current"]["uvi"];
    weather.current.cloudcover = obj["current"]["clouds"];
    copyJsonString(weather.current.description, obj["current"]["weather"][0]["description"]);
    result = true;
  }
  if (!obj["daily"][0].isNull())
  {
    weather.day.tempMin = obj["daily"][0]["temp"]["min"];
    weather.day.tempMax = obj["daily"][0]["temp"]["max"];
    weather.day.humidity = obj["daily"][0]["humidity"];
    weather.day.windSpeed = obj["daily"][0]["wind_speed"];
    weather.day.windGust = obj["daily"][0]["wind_gust"];
    weather.day.uvi = obj["daily"][0]["uvi"];
    weather.day.cloudcover = obj["daily"][0]["clouds"];
    weather.day.moonPhase = obj["daily"][0]["moon_phase"];
    weather.day.sunrise = convert1970Epoch(obj["daily"][0]["sunrise"]);
    weather.day.sunset = convert1970Epoch(obj["daily"][0]["sunset"]);
    weather.day.moonrise = convert1970Epoch(obj["daily"][0]["moonrise"]);
    weather.day.moonset = convert1970Epoch(obj["daily"][0]["moonset"]);
    copyJsonString(weather.day.description, obj["daily"][0]["weather"][0]["description"]);
    copyJsonString(weather.day.icon, obj["daily"][0]["weather"][0]["icon"]);
    result = true;
  }
  return result;
}

bool fillIpgeoFromJson(const String &payload)
{
  StaticJsonDocument<128> filter;
  filter["time_zone"]["name"] = true;
  filter["time_zone"]["offset"] = true;
  filter["latitude"] = true;
  filter["longitude"] = true;

  StaticJsonDocument<768> doc;
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error)
  {
    ESP_LOGE(TAG, "IPGeo deserializeJson() failed: %s", error.c_str());
    return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  if (!obj["time_zone"].isNull())
  {
    copyJsonString(ipgeo.timezone, obj["time_zone"]["name"]);
    ipgeo.tzoffset = obj["time_zone"]["offset"] | 127;
    ipgeo.lat = obj["latitude"] | 0.0;
    ipgeo.lon = obj["longitude"] | 0.0;
    return true;
  }
  return false;
}

bool fillGeocodeFromJson(const String &payload)
{
  StaticJsonDocument<96> filter;
  filter[0]["name"] = true;
  filter[0]["state"] = true;
  filter[0]["country"] = true;

  StaticJsonDocument<768> doc;
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error)
  {
    ESP_LOGE(TAG, "GEOcode deserializeJson() failed: %s", error.c_str());
    return false;
  }

  JsonArray array = doc.as<JsonArray>();
  if (!array.isNull() && !array[0].isNull())
  {
    copyJsonString(geocode.city, array[0]["name"]);
    copyJsonString(geocode.state, array[0]["state"]);
    copyJsonString(geocode.country, array[0]["country"]);
    return true;
  }
  return false;
}

bool fillAqiFromJson(const String &payload)
{
  bool ready = false;
  StaticJsonDocument<768> filter;
  filter["list"][0]["main"]["aqi"] = true;
  filter["list"][0]["components"]["co"] = true;
  filter["list"][0]["components"]["no"] = true;
  filter["list"][0]["components"]["no2"] = true;
  filter["list"][0]["components"]["o3"] = true;
  filter["list"][0]["components"]["so2"] = true;
  filter["list"][0]["components"]["pm2_5"] = true;
  filter["list"][0]["components"]["pm10"] = true;
  filter["list"][0]["components"]["nh3"] = true;

  DynamicJsonDocument doc(kAqiDocumentCapacity);
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error)
  {
    ESP_LOGE(TAG, "AQI deserializeJson() failed: %s", error.c_str());
    return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  JsonArray list = obj["list"].as<JsonArray>();
  if (list.size() <= 7 || list[1].isNull() || list[7].isNull())
  {
    ESP_LOGE(TAG, "AQI payload did not contain the expected forecast entries. Entries parsed: %u",
             static_cast<unsigned>(list.size()));
    return false;
  }
  ESP_LOGV(TAG, "AQI forecast payload parsed successfully.");
  uint8_t currentBucket = kAqiUnknown;
  uint8_t dayBucket = kAqiUnknown;
  if (!readAqiBucket(list[1]["main"]["aqi"], "current", currentBucket) ||
      !readAqiBucket(list[7]["main"]["aqi"], "daily", dayBucket))
  {
    return false;
  }

  aqi.current.aqi = currentBucket;
  aqi.current.co = obj["list"][1]["components"]["co"];
  aqi.current.no = obj["list"][1]["components"]["no"];
  aqi.current.no2 = obj["list"][1]["components"]["no2"];
  aqi.current.o3 = obj["list"][1]["components"]["o3"];
  aqi.current.so2 = obj["list"][1]["components"]["so2"];
  aqi.current.pm25 = obj["list"][1]["components"]["pm2_5"];
  aqi.current.pm10 = obj["list"][1]["components"]["pm10"];
  aqi.current.nh3 = obj["list"][1]["components"]["nh3"];
  ready = true;
  ESP_LOGD(TAG, "New current air quality data received");
  processAqiDescription();
  aqi.day.aqi = dayBucket;
  aqi.day.co = obj["list"][7]["components"]["co"];
  aqi.day.no = obj["list"][7]["components"]["no"];
  aqi.day.no2 = obj["list"][7]["components"]["no2"];
  aqi.day.o3 = obj["list"][7]["components"]["o3"];
  aqi.day.so2 = obj["list"][7]["components"]["so2"];
  aqi.day.pm25 = obj["list"][7]["components"]["pm2_5"];
  aqi.day.pm10 = obj["list"][7]["components"]["pm10"];
  aqi.day.nh3 = obj["list"][7]["components"]["nh3"];
  ready = true;
  ESP_LOGD(TAG, "New daily air quality data received");
  return ready;
}

uint8_t safeAqiIndex(int value)
{
  return value >= kAqiMin && value <= kAqiMax ? static_cast<uint8_t>(value) : kAqiUnknown;
}

void processAqiDescription()
{
  aqi.current.description = "";
  if (aqi.current.co > 4400)
    aqi.current.description = "Elevated Carbon Monoxide";
  if (aqi.current.o3 > 180)
  {
    if (aqi.current.description == "")
      aqi.current.description = "Elevated ";
    else
      aqi.current.description += " & ";
    aqi.current.description += "Ozone";
  }
  if (aqi.current.no2 > 40)
  {
    if (aqi.current.description == "")
      aqi.current.description = "Elevated ";
    else
      aqi.current.description += " & ";
    aqi.current.description += "Nitrogen Dioxode";
  }
  if (aqi.current.so2 > 20)
  {
    if (aqi.current.description == "")
      aqi.current.description = "Elevated ";
    else
      aqi.current.description += " & ";
    aqi.current.description += "Sulfer Dioxode";
  }
  if (aqi.current.pm25 > 10)
  {
    if (aqi.current.description == "")
      aqi.current.description = "Elevated ";
    else
      aqi.current.description += " & ";
    aqi.current.description += "Small Particulates";
  }
  if (aqi.current.pm10 > 20)
  {
    if (aqi.current.description == "")
      aqi.current.description = "Elevated ";
    else
      aqi.current.description += " & ";
    aqi.current.description += "Medium Particulates";
  }
}

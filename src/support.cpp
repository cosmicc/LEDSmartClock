#include "main.h"

namespace
{
/** Copies a string into a diagnostics buffer without overrunning it. */
template <size_t N>
void copyDiagnosticText(char (&dest)[N], const char *value)
{
  snprintf(dest, N, "%s", value == nullptr ? "" : value);
}

/** Applies a diagnostics update to the selected subsystem record. */
void updateDiagnosticRecord(DiagnosticService service, bool enabled, bool healthy, const char *summary,
                            const String &detail, uint8_t retries, int16_t lastCode,
                            bool markSuccess, bool markFailure)
{
  ServiceDiagnostic &record = diagnosticState(service);
  record.enabled = enabled;
  record.healthy = healthy;
  record.retries = retries;
  record.lastCode = lastCode;
  copyDiagnosticText(record.summary, summary);
  copyDiagnosticText(record.detail, detail.c_str());

  const acetime_t now = systemClock.getNow();
  if (markSuccess && now != LocalTime::kInvalidSeconds)
    record.lastSuccess = now;
  if (markFailure && now != LocalTime::kInvalidSeconds)
    record.lastFailure = now;
}
} // namespace

String elapsedTime(uint32_t seconds)
{
  String result;
  if (seconds < 60)
  {
    result = String(seconds) + " seconds";
  }
  else
  {
    uint32_t minutes = seconds / 60;
    if (minutes < 60)
    {
      result = String(minutes) + " minutes, " + String(seconds % 60) + " seconds";
    }
    else
    {
      uint32_t hours = minutes / 60;
      if (hours < 24)
      {
        result = String(hours) + " hours, " + String(minutes % 60) + " minutes";
      }
      else
      {
        uint32_t days = hours / 24;
        if (days < 30)
        {
          result = String(days) + " days, " + String(hours % 24) + " hours";
        }
        else
        {
          uint32_t months = days / 30;
          if (months < 12)
          {
            result = String(months) + " months, " + String(days % 30) + " days";
          }
          else
          {
            result = "Never";
          }
        }
      }
    }
  }
  return result;
}

char *capString(char *str)
{
  bool capNext = true;
  for (size_t i = 0; str[i] != '\0'; i++)
  {
    if (str[i] >= 'a' && str[i] <= 'z')
    {
      if (capNext)
      {
        str[i] = str[i] - ('a' - 'A');
        capNext = false;
      }
    }
    else if (str[i] == ' ')
    {
      capNext = true;
    }
    else
    {
      capNext = false;
    }
  }
  return str;
}

ace_time::acetime_t convertUnixEpochToAceTime(uint32_t ntpSeconds)
{
  constexpr int32_t kDaysToConverterEpochFromNtpEpoch = 36524;
  int32_t daysToCurrentEpochFromNtpEpoch = Epoch::daysToCurrentEpochFromInternalEpoch();
  daysToCurrentEpochFromNtpEpoch += kDaysToConverterEpochFromNtpEpoch;
  uint32_t secondsToCurrentEpochFromNtpEpoch = 86400uL * daysToCurrentEpochFromNtpEpoch;
  uint32_t epochSeconds = ntpSeconds - secondsToCurrentEpochFromNtpEpoch;
  return epochSeconds;
}

ace_time::acetime_t convert1970Epoch(ace_time::acetime_t epoch1970)
{
  ace_time::acetime_t epoch2050 = epoch1970 - Epoch::secondsToCurrentEpochFromUnixEpoch64();
  return epoch2050;
}

long aceEpochToUnixSeconds(ace_time::acetime_t epochSeconds)
{
  return static_cast<long>(static_cast<int64_t>(epochSeconds) +
                           Epoch::secondsToCurrentEpochFromUnixEpoch64());
}

String formatDebugTimestamp(ace_time::acetime_t epochSeconds)
{
  if (epochSeconds == LocalTime::kInvalidSeconds)
    return F("invalid");

  String formatted = getCustomZonedTimestamp(epochSeconds);
  formatted += F(" | unix=");
  formatted += String(aceEpochToUnixSeconds(epochSeconds));
  formatted += F(" | ace=");
  formatted += String(epochSeconds);
  return formatted;
}

void setTimeSource(const String &inputString)
{
  const size_t capacity = sizeof(runtimeState.timeSource);
  for (size_t i = 0; i < capacity; i++)
  {
    if (i + 1 < capacity && i < static_cast<size_t>(inputString.length()))
    {
      runtimeState.timeSource[i] = inputString.charAt(static_cast<unsigned int>(i));
    }
    else
    {
      runtimeState.timeSource[i] = '\0';
      break;
    }
  }

  String source = inputString;
  source.toLowerCase();
  if (source == "gps")
  {
    noteDiagnosticSuccess(DiagnosticService::Timekeeping, true, "GPS locked",
                          F("Clock time is currently coming from the GPS receiver."), 0, 0);
  }
  else if (source == "ntp")
  {
    noteDiagnosticSuccess(DiagnosticService::Timekeeping, true, "Network synced",
                          F("Clock time is currently coming from the SNTP client."), 0, 0);
  }
  else if (source == "rtc")
  {
    noteDiagnosticPending(DiagnosticService::Timekeeping, true, "RTC fallback",
                          F("Clock time is currently being held by the DS3231 RTC chip."));
  }
  else
  {
    noteDiagnosticPending(DiagnosticService::Timekeeping, true, "Waiting",
                          F("The clock is still waiting for GPS, NTP, or RTC time."));
  }
}

ace_time::acetime_t Now()
{
  return systemClock.getNow();
}

void resetLastNtpCheck()
{
  resetLastNtpCheck(systemClock.getNow());
}

void resetLastNtpCheck(ace_time::acetime_t syncedTime)
{
  runtimeState.lastNtpCheck = syncedTime;
}

bool compareTimes(ace_time::ZonedDateTime t1, ace_time::ZonedDateTime t2)
{
  return t1.hour() == t2.hour() &&
         t1.minute() == t2.minute() &&
         t1.second() == t2.second();
}

ServiceDiagnostic &diagnosticState(DiagnosticService service)
{
  return runtimeState.diagnostics[static_cast<size_t>(service)];
}

const char *diagnosticServiceLabel(DiagnosticService service)
{
  switch (service)
  {
  case DiagnosticService::Wifi:
    return "Wi-Fi";
  case DiagnosticService::Timekeeping:
    return "Timekeeping";
  case DiagnosticService::Ntp:
    return "NTP";
  case DiagnosticService::Gps:
    return "GPS";
  case DiagnosticService::Weather:
    return "Weather";
  case DiagnosticService::AirQuality:
    return "Air Quality";
  case DiagnosticService::Alerts:
    return "Alerts";
  case DiagnosticService::Geocode:
    return "Geocode";
  case DiagnosticService::IpGeo:
    return "IP Geo";
  case DiagnosticService::Count:
  default:
    return "Unknown";
  }
}

void resetServiceDiagnostics()
{
  for (size_t index = 0; index < kDiagnosticServiceCount; ++index)
  {
    ServiceDiagnostic &record = runtimeState.diagnostics[index];
    record.enabled = true;
    record.healthy = false;
    record.retries = 0;
    record.lastCode = 0;
    record.lastSuccess = 0;
    record.lastFailure = 0;
    copyDiagnosticText(record.summary, "Pending");
    copyDiagnosticText(record.detail, "Waiting for runtime data.");
  }
}

void noteDiagnosticPending(DiagnosticService service, bool enabled, const char *summary, const String &detail,
                           uint8_t retries, int16_t lastCode)
{
  updateDiagnosticRecord(service, enabled, false, summary, detail, retries, lastCode, false, false);
}

void noteDiagnosticSuccess(DiagnosticService service, bool enabled, const char *summary, const String &detail,
                           uint8_t retries, int16_t lastCode)
{
  updateDiagnosticRecord(service, enabled, true, summary, detail, retries, lastCode, true, false);
}

void noteDiagnosticFailure(DiagnosticService service, bool enabled, const char *summary, const String &detail,
                           int16_t lastCode, uint8_t retries)
{
  updateDiagnosticRecord(service, enabled, false, summary, detail, retries, lastCode, false, true);
}

void toUpper(char *input)
{
  int count = strlen(input);
  for (int i = 0; i < count; i++)
    input[i] = toupper(static_cast<unsigned char>(input[i]));
}

uint16_t calcbright(uint16_t bl)
{
  uint16_t retVal = 0;
  switch (bl)
  {
  case 1:
    retVal = 0;
    break;
  case 2:
    retVal = 1;
    break;
  case 3:
    retVal = 2;
    break;
  case 4:
    retVal = 4;
    break;
  case 5:
    retVal = 8;
    break;
  case 6:
    retVal = 10;
    break;
  case 7:
    retVal = 15;
    break;
  case 8:
    retVal = 20;
    break;
  case 9:
    retVal = 25;
    break;
  case 10:
    retVal = 30;
    break;
  default:
    retVal = 0;
    break;
  }
  return retVal;
}

const char *ordinal_suffix(int n)
{
  int mod100 = n % 100;
  if (mod100 >= 11 && mod100 <= 13)
  {
    return "th";
  }

  switch (n % 10)
  {
  case 1:
    return "st";
  case 2:
    return "nd";
  case 3:
    return "rd";
  default:
    return "th";
  }
}

void cleanString(char *str)
{
  char *src = str;
  char *dst = str;
  while (*src)
  {
    if (*src != '\n' && *src != '"' && *src != '[' && *src != ']')
    {
      *dst++ = *src;
    }
    src++;
  }
  *dst = '\0';
}

void capitalize(char *str)
{
  while (*str)
  {
    *str = toupper(static_cast<unsigned char>(*str));
    str++;
  }
}

bool hasVisibleText(const char *text)
{
  if (text == nullptr)
    return false;

  while (*text != '\0')
  {
    if (!isspace(static_cast<unsigned char>(*text)))
      return true;
    ++text;
  }

  return false;
}

const AlertEntry *primaryAlert()
{
  if (!alerts.active || alerts.count == 0)
    return nullptr;

  return &alerts.items[0];
}

const AlertEntry *displayAlert()
{
  if (!alerts.active || alerts.count == 0)
    return nullptr;

  if (alerts.displayIndex >= alerts.count)
    alerts.displayIndex = 0;

  return &alerts.items[alerts.displayIndex];
}

void advanceAlertRotation()
{
  if (alerts.count == 0)
  {
    alerts.displayIndex = 0;
    return;
  }

  alerts.displayIndex = (alerts.displayIndex + 1U) % alerts.count;
}

String summarizeActiveAlerts(uint8_t maxItems)
{
  if (!alerts.active || alerts.count == 0)
    return F("No active alert");

  String summary;
  uint8_t rendered = min(maxItems, alerts.count);
  for (uint8_t index = 0; index < rendered; ++index)
  {
    const AlertEntry &entry = alerts.items[index];
    const char *title = hasVisibleText(entry.event) ? entry.event : entry.headline;
    if (!hasVisibleText(title))
      title = "Unnamed alert";

    if (summary.length() > 0)
      summary += F(", ");
    summary += title;
  }

  if (alerts.count > rendered)
  {
    summary += F(" (+");
    summary += (alerts.count - rendered);
    summary += F(" more)");
  }

  return summary;
}

namespace
{
/** Copies a weather description into a local buffer and capitalizes it for scroll text. */
void copyWeatherDescription(char *dest, size_t length, const char *source, const char *fallback)
{
  if (length == 0)
    return;

  const char *selected = hasVisibleText(source) ? source : fallback;
  strlcpy(dest, selected, length);
  capString(dest);
}

/** Returns the compact temperature unit used in scrolling weather summaries. */
const char *weatherTempUnitLabel()
{
  return imperial.isChecked() ? "F" : "C";
}

/** Returns the compact wind-speed unit used in scrolling weather summaries. */
const char *weatherSpeedUnitLabel()
{
  return imperial.isChecked() ? "mph" : "kph";
}
} // namespace

void buildCurrentWeatherScrollText(char *buffer, size_t length)
{
  if (buffer == nullptr || length == 0)
    return;

  char description[sizeof(weather.current.description)]{};
  copyWeatherDescription(description, sizeof(description), weather.current.description, "Weather");
  const uint8_t gust = weather.current.windGust > weather.current.windSpeed ? weather.current.windGust : weather.current.windSpeed;

  if (current_weather_short_text.isChecked())
  {
    snprintf(buffer, length, "Now %s %d%s Feels:%d%s Wind:%u%s",
             description,
             weather.current.temp,
             weatherTempUnitLabel(),
             weather.current.feelsLike,
             weatherTempUnitLabel(),
             weather.current.windSpeed,
             weatherSpeedUnitLabel());
    return;
  }

  snprintf(buffer, length, "Current %s Temp:%d%s Feels:%d%s Humidity:%u%% Wind:%u/%u%s Clouds:%u%% AQI:%s UV:%s",
           description,
           weather.current.temp,
           weatherTempUnitLabel(),
           weather.current.feelsLike,
           weatherTempUnitLabel(),
           weather.current.humidity,
           weather.current.windSpeed,
           gust,
           weatherSpeedUnitLabel(),
           weather.current.cloudcover,
           air_quality[safeAqiIndex(aqi.current.aqi)],
           uv_index(weather.current.uvi).c_str());
}

void buildDailyWeatherScrollText(char *buffer, size_t length)
{
  if (buffer == nullptr || length == 0)
    return;

  char description[sizeof(weather.day.description)]{};
  copyWeatherDescription(description, sizeof(description), weather.day.description, "Forecast");
  const uint8_t gust = weather.day.windGust > weather.day.windSpeed ? weather.day.windGust : weather.day.windSpeed;

  if (daily_weather_short_text.isChecked())
  {
    snprintf(buffer, length, "Today %s Hi:%d%s Lo:%d%s",
             description,
             weather.day.tempMax,
             weatherTempUnitLabel(),
             weather.day.tempMin,
             weatherTempUnitLabel());
    return;
  }

  snprintf(buffer, length, "Today %s Hi:%d%s Lo:%d%s Humidity:%u%% Wind:%u/%u%s Clouds:%u%% AQI:%s UV:%s",
           description,
           weather.day.tempMax,
           weatherTempUnitLabel(),
           weather.day.tempMin,
           weatherTempUnitLabel(),
           weather.day.humidity,
           weather.day.windSpeed,
           gust,
           weatherSpeedUnitLabel(),
           weather.day.cloudcover,
           air_quality[safeAqiIndex(aqi.day.aqi)],
           uv_index(weather.day.uvi).c_str());
}

bool cmpLocs(const char a1[32], const char a2[32])
{
  return memcmp(a1, a2, 32) == 0;
}

String uv_index(uint8_t index)
{
  if (index <= 2)
    return "Low";
  if (index <= 5)
    return "Moderate";
  if (index <= 7)
    return "High";
  if (index <= 10)
    return "Very High";
  return "Extreme";
}

int32_t fromNow(ace_time::acetime_t ctime)
{
  return (systemClock.getNow() - ctime);
}

String formatLargeNumber(int number)
{
  char str[20];
  String formattedNumber;
  int i = 0;
  if (number < 0)
  {
    str[i++] = '-';
    number = -number;
  }
  do
  {
    str[i++] = number % 10 + '0';
    if (i % 4 == 3 && number / 10 != 0)
    {
      str[i++] = ',';
    }
    number /= 10;
  } while (number);
  str[i] = '\0';
  for (int j = i - 1; j >= 0; j--)
  {
    formattedNumber += str[j];
  }
  return formattedNumber;
}

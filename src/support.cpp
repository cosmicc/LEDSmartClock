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
  if (markSuccess && now > 0)
    record.lastSuccess = now;
  if (markFailure && now > 0)
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

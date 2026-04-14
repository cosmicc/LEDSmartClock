#include "main.h"

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
  for (int i = 0; i < 4; i++)
  {
    if (i < inputString.length())
    {
      runtimeState.timeSource[i] = inputString.charAt(i);
    }
    else
    {
      runtimeState.timeSource[i] = '\0';
    }
  }
}

ace_time::acetime_t Now()
{
  return systemClock.getNow();
}

void resetLastNtpCheck()
{
  runtimeState.lastNtpCheck = systemClock.getNow();
}

bool compareTimes(ace_time::ZonedDateTime t1, ace_time::ZonedDateTime t2)
{
  return t1.hour() == t2.hour() &&
         t1.minute() == t2.minute() &&
         t1.second() == t2.second();
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

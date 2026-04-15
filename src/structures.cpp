#include "main.h"

String DisplayToken::showTokens() const
{
  String activeTokens;

  if (token1)
    activeTokens += "Date(1),";
  if (token2)
    activeTokens += "CurWeather(2),";
  if (token3)
    activeTokens += "Alerts(3),";
  if (token4)
    activeTokens += "Misc(4),";
  if (token5)
    activeTokens += "5,";
  if (token6)
    activeTokens += "AlertFlash(6),";
  if (token7)
    activeTokens += "7,";
  if (token8)
    activeTokens += "DayWeather(8),";
  if (token9)
    activeTokens += "AirQual(9),";
  if (token10)
    activeTokens += "ScrollText(10),";

  if (activeTokens.isEmpty())
  {
    return "0";
  }

  if (activeTokens.endsWith(","))
  {
    activeTokens.remove(activeTokens.length() - 1);
  }

  return activeTokens;
}

bool DisplayToken::getToken(uint8_t position) const
{
  switch (position)
  {
  case 1:
    return token1;
  case 2:
    return token2;
  case 3:
    return token3;
  case 4:
    return token4;
  case 5:
    return token5;
  case 6:
    return token6;
  case 7:
    return token7;
  case 8:
    return token8;
  case 9:
    return token9;
  case 10:
    return token10;
  default:
    return false;
  }
}

void DisplayToken::setToken(uint8_t position)
{
  if (position == 0 || position > 10)
  {
    ESP_LOGW(TAG, "Ignoring invalid display token request: %d", position);
    return;
  }

  if (getToken(position))
  {
    ESP_LOGW(TAG, "Display token already held: %s(%d)", name[position], position);
    return;
  }

  ESP_LOGD(TAG, "Setting display token: %s(%d)", name[position], position);

  switch (position)
  {
  case 1:
    token1 = true;
    break;
  case 2:
    token2 = true;
    break;
  case 3:
    token3 = true;
    break;
  case 4:
    token4 = true;
    break;
  case 5:
    token5 = true;
    break;
  case 6:
    token6 = true;
    break;
  case 7:
    token7 = true;
    break;
  case 8:
    token8 = true;
    break;
  case 9:
    token9 = true;
    break;
  case 10:
    token10 = true;
    break;
  default:
    break;
  }
}

void DisplayToken::resetToken(uint8_t position)
{
  if (position == 0 || position > 10)
  {
    ESP_LOGW(TAG, "Ignoring invalid display token release: %d", position);
    return;
  }

  ESP_LOGD(TAG, "Releasing display token: %s(%d)", name[position], position);

  switch (position)
  {
  case 1:
    token1 = false;
    break;
  case 2:
    token2 = false;
    break;
  case 3:
    token3 = false;
    break;
  case 4:
    token4 = false;
    break;
  case 5:
    token5 = false;
    break;
  case 6:
    token6 = false;
    break;
  case 7:
    token7 = false;
    break;
  case 8:
    token8 = false;
    break;
  case 9:
    token9 = false;
    break;
  case 10:
    token10 = false;
    break;
  default:
    break;
  }
}

void DisplayToken::resetAllTokens()
{
  token1 = false;
  token2 = false;
  token3 = false;
  token4 = false;
  token5 = false;
  token6 = false;
  token7 = false;
  token8 = false;
  token9 = false;
  token10 = false;
}

bool DisplayToken::isReady(uint8_t position) const
{
  switch (position)
  {
  case 0:
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
  case 8:
  case 9:
  case 10:
    return !(token1 || token2 || token3 || token4 || token5 || token6 || token7 || token8 || token9 || token10);
  default:
    return false;
  }
}

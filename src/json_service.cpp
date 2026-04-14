#include "main.h"

namespace
{
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
} // namespace

bool fillAlertsFromJson(const String &payload)
{
  StaticJsonDocument<2048> filter;
  filter["features"][0]["properties"]["status"] = true;
  filter["features"][0]["properties"]["severity"] = true;
  filter["features"][0]["properties"]["certainty"] = true;
  filter["features"][0]["properties"]["urgency"] = true;
  filter["features"][0]["properties"]["event"] = true;
  filter["features"][0]["properties"]["instruction"] = true;
  filter["features"][0]["properties"]["parameters"]["NWSheadline"] = true;
  filter["features"][1]["properties"]["status"] = true;
  filter["features"][1]["properties"]["severity"] = true;
  filter["features"][1]["properties"]["certainty"] = true;
  filter["features"][1]["properties"]["urgency"] = true;
  filter["features"][1]["properties"]["event"] = true;
  filter["features"][1]["properties"]["instruction"] = true;
  filter["features"][1]["properties"]["parameters"]["NWSheadline"] = true;
  filter["features"][2]["properties"]["status"] = true;
  filter["features"][2]["properties"]["severity"] = true;
  filter["features"][2]["properties"]["certainty"] = true;
  filter["features"][2]["properties"]["urgency"] = true;
  filter["features"][2]["properties"]["event"] = true;
  filter["features"][2]["properties"]["instruction"] = true;
  filter["features"][2]["properties"]["parameters"]["NWSheadline"] = true;
  StaticJsonDocument<8192> doc;
  DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));
  if (error)
  {
    ESP_LOGE(TAG, "Alerts deserializeJson() failed: %s", error.c_str());
    return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  uint8_t actwatch = 0;
  uint8_t actwarn = 0;
  bool result = false;
  if (!obj["features"][0].isNull())
  {
    if ((obj["features"][0]["properties"]["severity"].as<String>()) != "Unknown")
    {
      if ((obj["features"][0]["properties"]["certainty"].as<String>()) == "Observed" || (obj["features"][0]["properties"]["certainty"].as<String>()) == "Likely")
        actwarn = 1;
      else if ((obj["features"][0]["properties"]["certainty"].as<String>()) == "Possible")
        actwatch = 1;
    }
    else
      ESP_LOGD(TAG, "Weather alert 1 received but is of status Unknown, skipping");
    if (!obj["features"][1].isNull())
    {
      if ((obj["features"][1]["properties"]["severity"].as<String>()) != "Unknown")
      {
        if ((obj["features"][1]["properties"]["certainty"].as<String>()) == "Observed" || (obj["features"][1]["properties"]["certainty"].as<String>()) == "Likely")
        {
          if (actwarn == 0)
            actwarn = 2;
        }
        else if ((obj["features"][1]["properties"]["certainty"].as<String>()) == "Possible")
          if (actwatch == 0)
            actwatch = 2;
      }
      else
        ESP_LOGD(TAG, "Weather alert 2 received but is of status Unknown, skipping");
      if (!obj["features"][2].isNull())
      {
        if ((obj["features"][2]["properties"]["severity"].as<String>()) != "Unknown")
        {
          if ((obj["features"][2]["properties"]["certainty"].as<String>()) == "Observed" || (obj["features"][2]["properties"]["certainty"].as<String>()) == "Likely")
          {
            if (actwarn == 0)
              actwarn = 3;
          }
          else if ((obj["features"][2]["properties"]["certainty"].as<String>()) == "Possible")
          {
            if (actwatch == 0)
              actwatch = 3;
          }
        }
        else
          ESP_LOGD(TAG, "Weather alert 3 received but is of status Unknown, skipping");
      }
    }
  }
  if (actwarn > 0)
  {
    alerts.inWarning = true;
    alerts.inWatch = false;
    alerts.active = true;
    showready.alerts = true;
    alerts.timestamp = systemClock.getNow();
    ESP_LOGI(TAG, "Active weather WARNING alert received");
  }
  else if (actwatch > 0)
  {
    alerts.inWarning = false;
    alerts.inWatch = true;
    alerts.active = true;
    showready.alerts = true;
    alerts.timestamp = systemClock.getNow();
    ESP_LOGI(TAG, "Active weather WATCH alert received");
  }
  if (actwarn == 1 || actwatch == 1)
  {
    if (!obj["features"][0]["properties"]["status"].isNull())
    {
      copyJsonString(alerts.status1, obj["features"][0]["properties"]["status"]);
      copyJsonString(alerts.severity1, obj["features"][0]["properties"]["severity"]);
      copyJsonString(alerts.certainty1, obj["features"][0]["properties"]["certainty"]);
      copyJsonString(alerts.urgency1, obj["features"][0]["properties"]["urgency"]);
      copyJsonString(alerts.event1, obj["features"][0]["properties"]["event"]);
    }
    if (!obj["features"][0]["properties"]["parameters"]["NWSheadline"].isNull())
    {
      copyJsonString(alerts.description1, obj["features"][0]["properties"]["parameters"]["NWSheadline"][0]);
      cleanString(alerts.description1);
      result = true;
    }
    if (!obj["features"][0]["properties"]["instruction"].isNull())
    {
      copyJsonString(alerts.instruction1, obj["features"][0]["properties"]["instruction"]);
      cleanString(alerts.instruction1);
    }
    else
    {
      alerts.instruction1[0] = '\0';
    }
  }
  else if (actwarn == 2 || actwatch == 2)
  {
    copyJsonString(alerts.status1, obj["features"][1]["properties"]["status"]);
    copyJsonString(alerts.severity1, obj["features"][1]["properties"]["severity"]);
    copyJsonString(alerts.certainty1, obj["features"][1]["properties"]["certainty"]);
    copyJsonString(alerts.urgency1, obj["features"][1]["properties"]["urgency"]);
    copyJsonString(alerts.event1, obj["features"][1]["properties"]["event"]);
    if (!obj["features"][1]["properties"]["parameters"]["NWSheadline"].isNull())
    {
      copyJsonString(alerts.description1, obj["features"][1]["properties"]["parameters"]["NWSheadline"][0]);
      cleanString(alerts.description1);
      result = true;
    }
    if (!obj["features"][1]["properties"]["instruction"].isNull())
    {
      copyJsonString(alerts.instruction1, obj["features"][1]["properties"]["instruction"]);
      cleanString(alerts.instruction1);
    }
    else
      alerts.instruction1[0] = '\0';
  }
  else if (actwarn == 3 || actwatch == 3)
  {
    copyJsonString(alerts.status1, obj["features"][2]["properties"]["status"]);
    copyJsonString(alerts.severity1, obj["features"][2]["properties"]["severity"]);
    copyJsonString(alerts.certainty1, obj["features"][2]["properties"]["certainty"]);
    copyJsonString(alerts.urgency1, obj["features"][2]["properties"]["urgency"]);
    copyJsonString(alerts.event1, obj["features"][2]["properties"]["event"]);
    if (!obj["features"][2]["properties"]["parameters"]["NWSheadline"].isNull())
    {
      copyJsonString(alerts.description1, obj["features"][2]["properties"]["parameters"]["NWSheadline"][0]);
      cleanString(alerts.description1);
      result = true;
    }
    if (!obj["features"][2]["properties"]["instruction"].isNull())
    {
      copyJsonString(alerts.instruction1, obj["features"][2]["properties"]["instruction"]);
      cleanString(alerts.instruction1);
    }
    else
      alerts.instruction1[0] = '\0';
  }
  else
  {
    alerts.active = false;
    alerts.inWarning = false;
    alerts.inWatch = false;
    ESP_LOGI(TAG, "No current active weather alerts");
    result = true;
  }
  return result;
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
  StaticJsonDocument<8192> doc;
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
  StaticJsonDocument<2048> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    ESP_LOGE(TAG, "IPGeo deserializeJson() failed: %s", error.c_str());
    return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  if (!obj["time_zone"].isNull())
  {
    copyJsonString(ipgeo.timezone, obj["time_zone"]["name"]);
    ipgeo.tzoffset = obj["time_zone"]["offset"];
    ipgeo.lat = obj["latitude"];
    ipgeo.lon = obj["longitude"];
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
  StaticJsonDocument<30720> doc;
  DeserializationError error = deserializeJson(doc, payload);
  if (error)
  {
    ESP_LOGE(TAG, "AQI deserializeJson() failed: %s", error.c_str());
    return false;
  }
  JsonObject obj = doc.as<JsonObject>();
  Serial.println(obj);
  aqi.current.aqi = obj["list"][1]["main"]["aqi"];
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
  aqi.day.aqi = obj["list"][7]["main"]["aqi"];
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

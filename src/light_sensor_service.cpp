#include "main.h"

namespace
{
constexpr uint16_t kI2cTransactionTimeoutMs = 25;
constexpr uint8_t kLightSensorFailureLimit = 3;
constexpr uint32_t kLightSensorRecoveryDelayMs = 30000UL;

bool lightSensorHealthy = false;
uint8_t lightSensorFailures = 0;
uint32_t nextLightSensorRecoveryMillis = 0;

void setFixedBrightnessFallback()
{
  current.rawlux = LUXMIN;
  current.lux = LUXMIN;
  current.brightavg = current.brightness;
}

void markLightSensorFailure(const char *operation)
{
  if (lightSensorFailures < 255)
    ++lightSensorFailures;

  ESP_LOGW(TAG, "TSL2561 %s failed with status %u (%u/%u)",
           operation,
           static_cast<unsigned>(Tsl.status()),
           static_cast<unsigned>(lightSensorFailures),
           static_cast<unsigned>(kLightSensorFailureLimit));

  if (lightSensorFailures >= kLightSensorFailureLimit)
  {
    lightSensorHealthy = false;
    nextLightSensorRecoveryMillis = millis() + kLightSensorRecoveryDelayMs;
    setFixedBrightnessFallback();
    Tsl.off();
    ESP_LOGW(TAG, "TSL2561 disabled after repeated failures. Retrying in %lu ms.",
             static_cast<unsigned long>(kLightSensorRecoveryDelayMs));
  }
}

bool bringUpLightSensor()
{
  Tsl.begin();
  if (!Tsl.available())
    return false;
  if (!Tsl.on())
    return false;
  if (!Tsl.setSensitivity(true, Tsl2561::EXP_14))
    return false;

  lightSensorHealthy = true;
  lightSensorFailures = 0;
  nextLightSensorRecoveryMillis = 0;
  ESP_LOGI(TAG, "TSL2561 light sensor is available at I2C address 0x%02x",
           static_cast<unsigned>(Tsl.address()));
  return true;
}
} // namespace

void initializeI2cBus(const char *reason)
{
  Wire.end();
  delay(2);
  Wire.begin(TSL2561_SDA, TSL2561_SCL);
  Wire.setTimeOut(kI2cTransactionTimeoutMs);
  wireInterface.begin();
  ESP_LOGI(TAG, "I2C bus initialized on SDA:%d SCL:%d with %u ms transaction timeout (%s)",
           TSL2561_SDA,
           TSL2561_SCL,
           static_cast<unsigned>(kI2cTransactionTimeoutMs),
           reason == nullptr ? "startup" : reason);
}

bool initializeLightSensor(uint32_t timeoutMs)
{
  const uint32_t started = millis();
  while ((millis() - started) < timeoutMs)
  {
    if (bringUpLightSensor())
      return true;

    systemClock.loop();
    esp_task_wdt_reset();
    delay(10);
  }

  lightSensorHealthy = false;
  lightSensorFailures = kLightSensorFailureLimit;
  nextLightSensorRecoveryMillis = millis() + kLightSensorRecoveryDelayMs;
  setFixedBrightnessFallback();
  ESP_LOGW(TAG, "TSL2561 did not respond within %lu ms. Continuing with fixed brightness.",
           static_cast<unsigned long>(timeoutMs));
  return false;
}

bool isLightSensorHealthy()
{
  return lightSensorHealthy;
}

void recoverLightSensorIfNeeded()
{
  if (lightSensorHealthy || nextLightSensorRecoveryMillis == 0)
    return;

  if (static_cast<int32_t>(millis() - nextLightSensorRecoveryMillis) < 0)
    return;

  ESP_LOGW(TAG, "Attempting TSL2561/I2C recovery after repeated light-sensor failures.");
  initializeI2cBus("TSL2561 recovery");
  if (!bringUpLightSensor())
  {
    lightSensorFailures = kLightSensorFailureLimit;
    nextLightSensorRecoveryMillis = millis() + kLightSensorRecoveryDelayMs;
    setFixedBrightnessFallback();
    ESP_LOGW(TAG, "TSL2561 recovery failed. Retrying in %lu ms.",
             static_cast<unsigned long>(kLightSensorRecoveryDelayMs));
  }
}

bool readLightSensorLuminosity(uint16_t &rawLux)
{
  if (!lightSensorHealthy)
  {
    recoverLightSensorIfNeeded();
    return false;
  }

  if (!Tsl.fullLuminosity(rawLux))
  {
    markLightSensorFailure("read");
    return false;
  }

  lightSensorFailures = 0;
  return true;
}

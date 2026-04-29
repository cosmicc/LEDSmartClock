// LedSmartClock for a 8x32 LED neopixel display
//
// Created by: Ian Perry (ianperry99@gmail.com)
// https://github.com/cosmicc/LEDSmartClock

// DO NOT USE DELAYS OR SLEEPS EVER! This breaks systemclock (Everything is coroutines now)

#include "main.h"
#include "bitmaps.h"
#include "config_backup.h"
#include "coroutines.h"
#include <cctype>
#include <cmath>

namespace
{
constexpr uint8_t kTimezoneCacheSize = 2;
constexpr uint32_t kAutoPersistCooldownMs = 10UL * 60UL * 1000UL;
ExtendedZoneProcessorCache<kTimezoneCacheSize> timezoneProcessorCache;
ExtendedZoneManager timezoneManager(
    zonedbx::kZoneAndLinkRegistrySize,
    zonedbx::kZoneAndLinkRegistry,
    timezoneProcessorCache);

uint32_t lastAutoPersistMillis = 0;
bool pendingAutoPersist = false;
char pendingAutoPersistReason[48]{};

/** Returns a readable label for the ESP reset cause captured at boot. */
const char *espResetReasonLabel(esp_reset_reason_t reason)
{
  switch (reason)
  {
  case ESP_RST_POWERON:
    return "Power-on reset";
  case ESP_RST_EXT:
    return "External reset";
  case ESP_RST_SW:
    return "Software reset";
  case ESP_RST_PANIC:
    return "Exception or panic";
  case ESP_RST_INT_WDT:
    return "Interrupt watchdog";
  case ESP_RST_TASK_WDT:
    return "Task watchdog";
  case ESP_RST_WDT:
    return "Other watchdog";
  case ESP_RST_DEEPSLEEP:
    return "Deep-sleep wake";
  case ESP_RST_BROWNOUT:
    return "Brownout";
  case ESP_RST_SDIO:
    return "SDIO reset";
  case ESP_RST_UNKNOWN:
  default:
    return "Unknown reset";
  }
}

/** Captures the reset cause early so later debug output can explain this boot. */
void captureLastRebootReason()
{
  const esp_reset_reason_t reason = esp_reset_reason();
  snprintf(runtimeState.lastRebootReason, sizeof(runtimeState.lastRebootReason),
           "%s (%d)", espResetReasonLabel(reason), static_cast<int>(reason));
}

/** Formats the remaining time until the next scheduled display for debug output. */
String nextShowDebug(acetime_t now, acetime_t lastShown, uint32_t intervalSeconds)
{
  acetime_t nextDue = lastShown + static_cast<acetime_t>(intervalSeconds);
  if (nextDue <= now)
    return F("Ready now");

  return String(F("in ")) + elapsedTime(static_cast<uint32_t>(nextDue - now));
}

/** Returns a bounded random startup offset so hourly jobs do not align exactly. */
uint32_t getStartupShowStaggerSeconds(uint32_t intervalSeconds)
{
  if (intervalSeconds < T1M)
    return 0;

  uint32_t staggerCap = intervalSeconds / 4U;
  staggerCap = min(staggerCap, 15U * 60U);
  staggerCap = max(staggerCap, 30U);
  return esp_random() % (staggerCap + 1U);
}

/** Seeds a display schedule so the first run waits a full interval plus a small stagger. */
acetime_t seedLastShown(acetime_t bootTime, uint32_t intervalSeconds)
{
  return bootTime + static_cast<acetime_t>(getStartupShowStaggerSeconds(intervalSeconds));
}

/** Initializes the non-clock display schedules after boot. */
void seedDisplaySchedules()
{
  lastshown.date = seedLastShown(runtimeState.bootTime, static_cast<uint32_t>(date_interval.value()) * T1H);
  lastshown.currenttemp = seedLastShown(runtimeState.bootTime, static_cast<uint32_t>(current_temp_interval.value()) * T1M);
  lastshown.currentweather = seedLastShown(runtimeState.bootTime, static_cast<uint32_t>(current_weather_interval.value()) * T1H);
  lastshown.dayweather = seedLastShown(runtimeState.bootTime, static_cast<uint32_t>(daily_weather_interval.value()) * T1H);
  lastshown.aqi = seedLastShown(runtimeState.bootTime, static_cast<uint32_t>(aqi_interval.value()) * T1M);
}

void applyEspLogLevel(bool enabled)
{
  const esp_log_level_t level = enabled ? ESP_LOG_DEBUG : ESP_LOG_INFO;
  esp_log_level_set("*", level);
  esp_log_level_set(TAG, level);
}

bool isAutoPersistReason(const char *reason)
{
  return reason != nullptr &&
         (strcmp(reason, "timezone update") == 0 ||
          strcmp(reason, "location update") == 0 ||
          strcmp(reason, "coordinate update") == 0);
}

bool autoPersistCooldownElapsed(uint32_t now)
{
  return lastAutoPersistMillis == 0 || (now - lastAutoPersistMillis) >= kAutoPersistCooldownMs;
}

bool writeConfigurationState(const char *reason)
{
  String error;
  if (!saveStoredConfiguration(error))
  {
    ESP_LOGE(TAG, "Failed to persist configuration after %s: %s", reason, error.c_str());
    return false;
  }

  ESP_LOGI(TAG, "Persisted configuration after %s.", reason);
  return true;
}

/** Persists the live configuration into the key-based store and logs failures. */
bool persistConfigurationState(const char *reason)
{
  const char *saveReason = reason == nullptr ? "unspecified update" : reason;
  if (!isAutoPersistReason(saveReason))
  {
    const bool saved = writeConfigurationState(saveReason);
    if (saved)
      pendingAutoPersist = false;
    return saved;
  }

  const uint32_t now = millis();
  if (autoPersistCooldownElapsed(now))
  {
    const bool saved = writeConfigurationState(saveReason);
    if (saved)
    {
      lastAutoPersistMillis = now;
      pendingAutoPersist = false;
    }
    return saved;
  }

  strlcpy(pendingAutoPersistReason, saveReason, sizeof(pendingAutoPersistReason));
  pendingAutoPersist = true;
  ESP_LOGW(TAG, "Deferred configuration persist after %s to reduce flash write churn.", saveReason);
  return true;
}

/** Applies the checkbox-backed serial debug setting to the live ESP log pipeline. */
void applySerialDebugRuntimeConfiguration()
{
  const bool enabled = serialdebug.isChecked();
  setConsoleSerialMirrorEnabled(enabled);
  applyEspLogLevel(enabled);
}

/** Updates the serial debug checkbox from a console command and persists it. */
bool setSerialDebugConfiguration(bool enabled, const char *reason)
{
  serialdebug.value() = enabled;
  applySerialDebugRuntimeConfiguration();
  return persistConfigurationState(reason);
}

/** Returns true when a timezone-name buffer contains a usable identifier. */
bool hasTimezoneName(const char *zoneName)
{
  return zoneName != nullptr && zoneName[0] != '\0';
}

/** Formats an AceTime offset into a signed +HH:MM string. */
String formatTimeOffset(const TimeOffset &offset)
{
  int8_t hour = 0;
  int8_t minute = 0;
  offset.toHourMinute(hour, minute);

  char buffer[8];
  snprintf(buffer, sizeof(buffer), "%+03d:%02d", hour, abs(minute));
  return String(buffer);
}

/** Returns the currently active offset for the selected timezone at the given epoch. */
TimeOffset getTimezoneOffsetAt(acetime_t epochSeconds)
{
  return ZonedDateTime::forEpochSeconds(epochSeconds, current.timezone).timeOffset();
}

/** Updates the cached whole-hour offset used by legacy code and debug output. */
void refreshCachedTimezoneOffset(acetime_t epochSeconds)
{
  current.tzoffset = getTimezoneOffsetAt(epochSeconds).toMinutes() / 60;
}

/** Applies an IANA timezone name through AceTime so DST changes remain automatic. */
bool applyNamedTimezone(const char *zoneName, const char *source)
{
  if (!hasTimezoneName(zoneName))
    return false;

  TimeZone resolved = timezoneManager.createForZoneName(zoneName);
  if (resolved.isError())
  {
    ESP_LOGW(TAG, "Unable to resolve timezone name from %s: %s", source, zoneName);
    return false;
  }

  current.timezone = resolved;
  refreshCachedTimezoneOffset(systemClock.getNow());
  ESP_LOGD(TAG, "Using %s timezone: %s (%s)", source, zoneName, getSystemTimezoneOffsetString().c_str());
  return true;
}

/** Returns true when a coordinate pair is usable for location-based calculations. */
bool hasValidCoordinatePair(double lat, double lon)
{
  return !(lat == 0.0 && lon == 0.0) && lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
}

/** Derives a whole-hour timezone offset from longitude when named zones are unavailable. */
bool deriveTimezoneOffsetFromLongitude(double longitude, int16_t &offsetHours)
{
  if (longitude < -180.0 || longitude > 180.0)
    return false;

  int rounded = static_cast<int>((longitude / 15.0) + (longitude >= 0.0 ? 0.5 : -0.5));
  rounded = constrain(rounded, -12, 14);
  offsetHours = static_cast<int16_t>(rounded);
  return true;
}

/** Converts degrees to radians for solar-angle calculations. */
double degreesToRadians(double degrees)
{
  constexpr double kPi = 3.14159265358979323846;
  return degrees * (kPi / 180.0);
}

/** Converts radians to degrees for solar-angle calculations. */
double radiansToDegrees(double radians)
{
  constexpr double kPi = 3.14159265358979323846;
  return radians * (180.0 / kPi);
}

/** Normalizes an angle to the [0, 360) degree range. */
double normalizeDegrees(double degrees)
{
  while (degrees < 0.0)
    degrees += 360.0;
  while (degrees >= 360.0)
    degrees -= 360.0;
  return degrees;
}

/** Normalizes an hour value to the [0, 24) range. */
double normalizeHours(double hours)
{
  while (hours < 0.0)
    hours += 24.0;
  while (hours >= 24.0)
    hours -= 24.0;
  return hours;
}

/** Returns true when the supplied Gregorian year is a leap year. */
bool isLeapYear(int16_t year)
{
  return ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
}

/** Converts a calendar date into a 1-based day-of-year index. */
uint16_t dayOfYearFromDate(int16_t year, uint8_t month, uint8_t day)
{
  static const uint16_t kDaysBeforeMonth[] = {
      0,   31,  59,  90,  120, 151,
      181, 212, 243, 273, 304, 334};

  if (month < 1 || month > 12 || day < 1 || day > 31)
    return 0;

  uint16_t dayOfYear = kDaysBeforeMonth[month - 1] + day;
  if (month > 2 && isLeapYear(year))
    ++dayOfYear;
  return dayOfYear;
}

/** Calculates one UTC solar event hour using the NOAA sunrise/sunset approximation. */
bool computeSolarUtcHour(uint16_t dayOfYear, double latitudeDeg, double longitudeDeg, bool sunrise, double &utcHour)
{
  if (dayOfYear == 0)
    return false;

  const double lngHour = longitudeDeg / 15.0;
  const double approximateTime = static_cast<double>(dayOfYear) +
                                 ((sunrise ? 6.0 : 18.0) - lngHour) / 24.0;
  const double meanAnomaly = (0.9856 * approximateTime) - 3.289;
  double trueLongitude = meanAnomaly +
                         (1.916 * sin(degreesToRadians(meanAnomaly))) +
                         (0.020 * sin(2.0 * degreesToRadians(meanAnomaly))) +
                         282.634;
  trueLongitude = normalizeDegrees(trueLongitude);

  double rightAscension = radiansToDegrees(atan(0.91764 * tan(degreesToRadians(trueLongitude))));
  rightAscension = normalizeDegrees(rightAscension);

  const double longitudeQuadrant = floor(trueLongitude / 90.0) * 90.0;
  const double rightAscensionQuadrant = floor(rightAscension / 90.0) * 90.0;
  rightAscension = (rightAscension + (longitudeQuadrant - rightAscensionQuadrant)) / 15.0;

  const double sinDeclination = 0.39782 * sin(degreesToRadians(trueLongitude));
  const double cosDeclination = cos(asin(sinDeclination));
  const double cosLocalHour = (cos(degreesToRadians(90.833)) -
                               (sinDeclination * sin(degreesToRadians(latitudeDeg)))) /
                              (cosDeclination * cos(degreesToRadians(latitudeDeg)));

  // Polar day/night where the sun never rises/sets for this date.
  if (cosLocalHour > 1.0 || cosLocalHour < -1.0)
    return false;

  double localHourAngle = sunrise
                              ? (360.0 - radiansToDegrees(acos(cosLocalHour)))
                              : radiansToDegrees(acos(cosLocalHour));
  localHourAngle /= 15.0;

  const double localMeanTime = localHourAngle + rightAscension - (0.06571 * approximateTime) - 6.622;
  utcHour = normalizeHours(localMeanTime - lngHour);
  return true;
}

/** Calculates cached sunrise and sunset epochs from location/timezone when API data is unavailable. */
bool computeFallbackSunEvents(acetime_t nowEpoch, acetime_t &sunriseEpoch, acetime_t &sunsetEpoch, bool &usingGpsFix)
{
  const bool gpsCoordsAvailable = gps.fix && hasValidCoordinatePair(gps.lat, gps.lon);
  const double latitude = gpsCoordsAvailable ? gps.lat : current.lat;
  const double longitude = gpsCoordsAvailable ? gps.lon : current.lon;
  usingGpsFix = gpsCoordsAvailable;

  if (!hasValidCoordinatePair(latitude, longitude))
    return false;

  const ZonedDateTime localNow = ZonedDateTime::forEpochSeconds(nowEpoch, current.timezone);
  const int16_t year = localNow.year();
  const uint8_t month = localNow.month();
  const uint8_t day = localNow.day();
  const uint16_t dayOfYear = dayOfYearFromDate(year, month, day);
  if (dayOfYear == 0)
    return false;

  static int16_t cachedYear = INT16_MIN;
  static uint16_t cachedDayOfYear = 0;
  static int32_t cachedLatE5 = INT32_MIN;
  static int32_t cachedLonE5 = INT32_MIN;
  static int16_t cachedOffsetMinutes = INT16_MIN;
  static acetime_t cachedSunrise = 0;
  static acetime_t cachedSunset = 0;
  static bool cachedValid = false;

  const int32_t latE5 = static_cast<int32_t>(latitude * 100000.0);
  const int32_t lonE5 = static_cast<int32_t>(longitude * 100000.0);
  const int16_t offsetMinutes = getTimezoneOffsetAt(nowEpoch).toMinutes();

  if (cachedValid &&
      cachedYear == year &&
      cachedDayOfYear == dayOfYear &&
      cachedLatE5 == latE5 &&
      cachedLonE5 == lonE5 &&
      cachedOffsetMinutes == offsetMinutes)
  {
    sunriseEpoch = cachedSunrise;
    sunsetEpoch = cachedSunset;
    return true;
  }

  double sunriseUtcHour = 0.0;
  double sunsetUtcHour = 0.0;
  if (!computeSolarUtcHour(dayOfYear, latitude, longitude, true, sunriseUtcHour) ||
      !computeSolarUtcHour(dayOfYear, latitude, longitude, false, sunsetUtcHour))
  {
    cachedValid = false;
    return false;
  }

  auto localEpochForHour = [&](double utcHour) -> acetime_t {
    double localHour = utcHour + (static_cast<double>(offsetMinutes) / 60.0);
    int16_t dayAdjustment = 0;
    while (localHour < 0.0)
    {
      localHour += 24.0;
      --dayAdjustment;
    }
    while (localHour >= 24.0)
    {
      localHour -= 24.0;
      ++dayAdjustment;
    }

    const LocalDateTime localMidnight = LocalDateTime::forComponents(year, month, day, 0, 0, 0);
    const acetime_t midnightEpochUtc = localMidnight.toEpochSeconds();
    const acetime_t localMidnightEpoch = midnightEpochUtc - static_cast<acetime_t>(offsetMinutes) * 60;
    const acetime_t secondsIntoDay = static_cast<acetime_t>(lround(localHour * 3600.0));
    return localMidnightEpoch + static_cast<acetime_t>(dayAdjustment) * 86400 + secondsIntoDay;
  };

  sunriseEpoch = localEpochForHour(sunriseUtcHour);
  sunsetEpoch = localEpochForHour(sunsetUtcHour);

  cachedYear = year;
  cachedDayOfYear = dayOfYear;
  cachedLatE5 = latE5;
  cachedLonE5 = lonE5;
  cachedOffsetMinutes = offsetMinutes;
  cachedSunrise = sunriseEpoch;
  cachedSunset = sunsetEpoch;
  cachedValid = true;
  return true;
}

/** Formats a TinyGPS++ age field into a readable string. */
String formatGpsDataAge(unsigned long ageMillis)
{
  if (ageMillis == ULONG_MAX)
    return F("Unavailable");

  uint32_t roundedSeconds = static_cast<uint32_t>((ageMillis + 999UL) / 1000UL);
  return elapsedTime(roundedSeconds);
}

/** Formats a GPS event timestamp into a readable age label with an unset fallback. */
String formatGpsEventAge(acetime_t timestamp)
{
  if (timestamp == 0 || timestamp == LocalTime::kInvalidSeconds)
    return F("Pending");

  const acetime_t now = systemClock.getNow();
  if (now == LocalTime::kInvalidSeconds || now <= timestamp)
    return F("Pending");

  return elapsedTime(static_cast<uint32_t>(now - timestamp));
}

constexpr size_t kGpsRawNmeaCapacity = 6144U;
char sGpsRawNmeaBuffer[kGpsRawNmeaCapacity]{};
size_t sGpsRawNmeaHead = 0;
size_t sGpsRawNmeaLength = 0;

/** Returns true when the supplied GPS UART baud is one of the supported receiver rates. */
bool isSupportedGpsBaudValue(uint32_t baud)
{
  switch (baud)
  {
    case 4800UL:
    case 9600UL:
    case 19200UL:
    case 38400UL:
    case 57600UL:
    case 115200UL:
      return true;
    default:
      return false;
  }
}

/** Appends one incoming GPS UART byte into the retained raw NMEA troubleshooting buffer. */
void appendGpsRawNmeaByte(uint8_t byte)
{
  if (byte == '\0')
    return;

  sGpsRawNmeaBuffer[sGpsRawNmeaHead] = static_cast<char>(byte);
  sGpsRawNmeaHead = (sGpsRawNmeaHead + 1U) % kGpsRawNmeaCapacity;
  if (sGpsRawNmeaLength < kGpsRawNmeaCapacity)
    ++sGpsRawNmeaLength;

  ++gps.rawBytesCaptured;
  if (byte == '\n')
    ++gps.rawSentenceCount;
}

/** Resets the live GPS-fix bookkeeping without clearing maintenance counters. */
void clearGpsRuntimeState()
{
  gps.fix = false;
  gps.sats = 0;
  gps.lat = 0.0;
  gps.lon = 0.0;
  gps.elevation = 0;
  gps.hdop = 0;
  gps.timestamp = 0;
  gps.lastcheck = 0;
  gps.lockage = 0;
  gps.packetdelay = 0;
  gps.moduleDetected = false;
  gps.waitingForFix = false;
  gps.lastReportedSats = 255;
  gps.firstByteMillis = 0;
  gps.lastByteMillis = 0;
  gps.lastNoDataLogMillis = 0;
  gps.lastProgressLogMillis = 0;
}

/** Opens the GPS UART using the selected baud and records the live receiver state. */
void startGpsUart(const char *reason, bool logToSerial)
{
  const uint32_t baud = gpsConfiguredBaud();
  Serial1.flush();
  Serial1.end();
  Serial1.begin(baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  while (Serial1.available() > 0)
    Serial1.read();

  gps.activeBaud = baud;
  ++gps.uartRestartCount;

  if (logToSerial)
  {
    ESP_LOGI(TAG, "GPS UART started on RX:%d TX:%d at %lu baud (%s). Waiting for module traffic and satellite lock.",
             GPS_RX_PIN, GPS_TX_PIN, static_cast<unsigned long>(baud), reason == nullptr ? "startup" : reason);
  }
}

struct RuntimeHealthSnapshot
{
  uint32_t nowMs = 0;
  unsigned long heap = 0;
  unsigned long minHeap = 0;
  unsigned long stack = 0;
  uint8_t wifiState = 0;
  int wifiStatus = 0;
  String ipAddress;
  bool httpReady = false;
  bool httpBusy = false;
  const char *endpoint = "none";
  unsigned long busyMs = 0;
  String tokens;
  bool showClockSuspended = false;
  bool alertFlashActive = false;
  bool scrollActive = false;
  int scrollPosition = 0;
  int scrollSize = 0;
  unsigned long displayBlockedMs = 0;
  unsigned long lastHealthLogAgeMs = 0;
};

RuntimeHealthSnapshot captureRuntimeHealthSnapshot()
{
  RuntimeHealthSnapshot snapshot;
  snapshot.nowMs = millis();
  snapshot.heap = static_cast<unsigned long>(ESP.getFreeHeap());
  snapshot.minHeap = static_cast<unsigned long>(ESP.getMinFreeHeap());
  snapshot.stack = static_cast<unsigned long>(uxTaskGetStackHighWaterMark(NULL));
  snapshot.wifiState = iotWebConf.getState();
  snapshot.wifiStatus = static_cast<int>(WiFi.status());
  snapshot.ipAddress = WiFi.localIP().toString();
  snapshot.httpReady = isHttpReady();
  snapshot.httpBusy = networkService.busy;
  snapshot.endpoint = runtimeState.networkBusyEndpoint[0] == '\0' ? "none" : runtimeState.networkBusyEndpoint;
  snapshot.busyMs = networkService.busy && runtimeState.networkBusySinceMillis != 0
                        ? static_cast<unsigned long>(snapshot.nowMs - runtimeState.networkBusySinceMillis)
                        : 0UL;
  snapshot.tokens = displaytoken.showTokens();
  snapshot.showClockSuspended = showClock.isSuspended();
  snapshot.alertFlashActive = alertflash.active;
  snapshot.scrollActive = scrolltext.active;
  snapshot.scrollPosition = scrolltext.position;
  snapshot.scrollSize = scrolltext.size;
  snapshot.displayBlockedMs = runtimeState.displayBlockedSinceMillis == 0
                                  ? 0UL
                                  : static_cast<unsigned long>(snapshot.nowMs - runtimeState.displayBlockedSinceMillis);
  snapshot.lastHealthLogAgeMs = runtimeState.lastHealthLogMillis == 0
                                    ? 0UL
                                    : static_cast<unsigned long>(snapshot.nowMs - runtimeState.lastHealthLogMillis);
  return snapshot;
}
} // namespace

void flushDeferredConfigurationState()
{
  if (!pendingAutoPersist)
    return;

  const uint32_t now = millis();
  if (!autoPersistCooldownElapsed(now))
    return;

  char reason[64]{};
  snprintf(reason, sizeof(reason), "deferred %s",
           pendingAutoPersistReason[0] == '\0' ? "runtime update" : pendingAutoPersistReason);

  if (writeConfigurationState(reason))
    pendingAutoPersist = false;

  lastAutoPersistMillis = now;
}

extern "C" void app_main()
{
  Serial.begin(115200);
  initConsoleLog();
  applyEspLogLevel(false);
  captureLastRebootReason();
  ESP_LOGI(TAG, "Boot reset reason: %s", runtimeState.lastRebootReason);
  uint32_t init_timer = millis();
  nvs_flash_init();
  ESP_LOGD(TAG, "Initializing Hardware Watchdog...");
  esp_task_wdt_init(WDT_TIMEOUT, true); // enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);               // add current thread to WDT watch
  ESP_LOGD(TAG, "Initializing the system clock...");
  initializeI2cBus("startup");
  systemClock.setup();
  resetServiceDiagnostics();
  setTimeSource(systemClock.isInit() ? F("rtc") : F("none"));
  noteDiagnosticPending(DiagnosticService::Wifi, true, "Booting",
                        F("Wi-Fi services are still starting."));
  noteDiagnosticPending(DiagnosticService::Ntp, true, "Waiting",
                        F("The SNTP client is waiting for Wi-Fi before it can sync time."));
  noteDiagnosticPending(DiagnosticService::Gps, true, "Waiting for UART",
                        F("The GPS diagnostics are waiting for UART traffic from the receiver."));
  noteDiagnosticPending(DiagnosticService::Weather, false, "Waiting",
                        F("Weather checks will start after configuration, Wi-Fi, and coordinates are ready."));
  noteDiagnosticPending(DiagnosticService::AirQuality, false, "Waiting",
                        F("Air-quality checks will start after configuration, Wi-Fi, and coordinates are ready."));
  noteDiagnosticPending(DiagnosticService::Alerts, true, "Waiting",
                        F("Alert polling will start after Wi-Fi and coordinates are ready."));
  noteDiagnosticPending(DiagnosticService::Geocode, false, "Waiting",
                        F("Reverse geocoding will run once coordinates are known."));
  noteDiagnosticPending(DiagnosticService::IpGeo, false, "Waiting",
                        F("IP geolocation will run after Wi-Fi is connected if an API key is configured."));
  dsClock.setup();
  gpsClock.setup();
  ESP_LOGD(TAG, "Initializing IotWebConf...");
  setupIotWebConf();
  bool loadedStoredConfiguration = false;
  if (hasStoredConfiguration())
  {
    String configStoreError;
    if (loadStoredConfiguration(configStoreError))
    {
      ESP_LOGI(TAG, "Loaded key-based configuration store.");
      loadedStoredConfiguration = true;
    }
    else
    {
      ESP_LOGE(TAG, "Failed to load key-based configuration store: %s", configStoreError.c_str());
      ESP_LOGW(TAG, "Ignoring the persisted key-based store and continuing with defaults.");
      iotWebConf.getRootParameterGroup()->applyDefaultValue();
      normalizeLoadedConfigValues();
    }
  }
  else
  {
    ESP_LOGI(TAG, "No persisted configuration store found. Using defaults until first save.");
  }
  if (loadedStoredConfiguration && iotWebConf.getWifiSsidParameter()->valueBuffer[0] != '\0')
  {
    iotWebConf.skipApStartup();
    ESP_LOGI(TAG, "Skipping setup AP startup because a stored Wi-Fi configuration is available.");
  }
  applySerialDebugRuntimeConfiguration();
  printSystemZonedTime();
  noteDiagnosticPending(DiagnosticService::Weather, isApiValid(weatherapi.value()),
                        isApiValid(weatherapi.value()) ? "Waiting" : "Missing API key",
                        isApiValid(weatherapi.value())
                            ? String(F("Weather checks are waiting for Wi-Fi, coordinates, and their first refresh window."))
                            : String(F("OpenWeather One Call 3.0 API key is not configured.")));
  noteDiagnosticPending(DiagnosticService::AirQuality, isApiValid(weatherapi.value()),
                        isApiValid(weatherapi.value()) ? "Waiting" : "Missing API key",
                        isApiValid(weatherapi.value())
                            ? String(F("AQI checks are waiting for Wi-Fi, coordinates, and their first refresh window."))
                            : String(F("OpenWeather API key is not configured for AQI requests.")));
  noteDiagnosticPending(DiagnosticService::IpGeo, isApiValid(ipgeoapi.value()),
                        isApiValid(ipgeoapi.value()) ? "Waiting" : "Missing API key",
                        isApiValid(ipgeoapi.value())
                            ? String(F("IP geolocation is waiting for Wi-Fi before its first request."))
                            : String(F("ipgeolocation.io API key is not configured.")));
  noteDiagnosticPending(DiagnosticService::Geocode, isApiValid(weatherapi.value()),
                        isApiValid(weatherapi.value()) ? "Waiting" : "Missing API key",
                        isApiValid(weatherapi.value())
                            ? String(F("Reverse geocoding is waiting for valid coordinates."))
                            : String(F("OpenWeather API key is not configured for reverse geocoding.")));
  gpsClock.refreshNtpServer();
  ESP_LOGD(TAG, "Initializing Light Sensor...");
  runtimeState.userBrightness = calcbright(brightness_level.value());
  current.brightness = runtimeState.userBrightness;
  constexpr uint32_t kLightSensorStartupTimeoutMs = 5000UL;
  initializeLightSensor(kLightSensorStartupTimeoutMs);
  ESP_LOGD(TAG, "Initializing GPS Module...");
  startGpsUart("startup", false);
  gps.uartRestartCount = 0;
  ESP_LOGI(TAG, "GPS UART started on RX:%d TX:%d at %lu baud. Waiting for module traffic and satellite lock.",
                 GPS_RX_PIN, GPS_TX_PIN, static_cast<unsigned long>(gpsActiveBaud()));
  ESP_LOGD(TAG, "Initializing coroutine scheduler...");
  sysClock.setName("sysclock");
  coroutineManager.setName("manager");
  showClock.setName("show_clock");
  showDate.setName("show_date");
  systemMessages.setName("systemMessages");
  setBrightness.setName("brightness");
  gps_checkData.setName("gpsdata");
  serialInput.setName("serialInput");
  IotWebConf.setName("iotwebconf");
  scrollText.setName("scrolltext");
  AlertFlash.setName("alertflash");
  showWeather.setName("show_weather");
  showAlerts.setName("show_alerts");
  checkWeather.setName("check_weather");
  showAirquality.setName("show_air");
  showWeatherDaily.setName("show_weatherday");
  checkAirquality.setName("check_air");
  checkGeocode.setName("check_geocode");
#ifndef DISABLE_ALERTCHECK
  checkAlerts.setName("check_alerts");
#endif
#ifndef DISABLE_IPGEOCHECK
  checkIpgeo.setName("check_ipgeo");
#endif
#ifdef COROUTINE_PROFILER
  LogBinProfiler::createProfilers();
#endif
  ipgeo.tzoffset = 127;
  processTimezone();
  registerWebRoutes();
  cotimer.scrollspeed = map(text_scroll_speed.value(), 1, 10, 100, 10);
  ESP_LOGD(TAG, "IotWebConf initilization is complete. Web server is ready.");
  ESP_LOGD(TAG, "Setting class timestamps...");
  runtimeState.bootTime = systemClock.getNow();
  runtimeState.lastNtpCheck = systemClock.getNow() - 3601;
  cotimer.icontimer = millis(); // reset all delay timers
  gps.timestamp = runtimeState.bootTime - T1Y + 60;
  gps.lastcheck = runtimeState.bootTime - T1Y + 60;
  gps.lockage = runtimeState.bootTime - T1Y + 60;
  checkweather.lastsuccess = runtimeState.bootTime - T1Y + 60;
  checkalerts.lastsuccess = runtimeState.bootTime - T1Y + 60;
  checkipgeo.lastsuccess = runtimeState.bootTime - T1Y + 60;
  checkaqi.lastsuccess = runtimeState.bootTime - T1Y + 60;
  checkgeocode.lastsuccess = runtimeState.bootTime - T1Y + 60;
  checkalerts.lastattempt = runtimeState.bootTime - T1Y + 60;
  checkweather.lastattempt = runtimeState.bootTime - T1Y + 60;
  checkipgeo.lastattempt = runtimeState.bootTime - T1Y + 60;
  checkaqi.lastattempt = runtimeState.bootTime - T1Y + 60;
  checkgeocode.lastattempt = runtimeState.bootTime - T1Y + 60;
  lastshown.alerts = runtimeState.bootTime - T1Y + 60;
  seedDisplaySchedules();
  scrolltext.position = mw;
  if (enable_fixed_loc.isChecked())
    ESP_LOGI(TAG, "Setting Fixed GPS Location Lat: %s Lon: %s", fixedLat.value(), fixedLon.value());
  updateCoords();
  updateLocation();
  showclock.fstop = 250;
  if (flickerfast.isChecked())
    showclock.fstop = 20;
  ESP_LOGD(TAG, "Initializing the display...");
  FastLED.addLeds<NEOPIXEL, HSPI_MOSI>(leds, NUMMATRIX).setCorrection(TypicalLEDStrip);
  matrix->begin();
  matrix->setBrightness(runtimeState.userBrightness);
  matrix->setTextWrap(false);
  ESP_LOGD(TAG, "Display initalization complete.");
  ESP_LOGD(TAG, "Setup initialization complete: %lu ms", static_cast<unsigned long>(millis() - init_timer));
  scrolltext.resetmsgtime = millis() - 60000;
  displaytoken.resetAllTokens();
  CoroutineScheduler::setup();

  for (;;)
  {
    esp_task_wdt_reset();
#ifdef COROUTINE_PROFILER
    CoroutineScheduler::loopWithProfiler();
#else
    CoroutineScheduler::loop();
#endif
  }
}

// callbacks
void configSaved()
{
  normalizeLoadedConfigValues();
  if (!web_password_protection.isChecked())
    strlcpy(iotWebConf.getApPasswordParameter()->valueBuffer, wifiInitialApPassword,
            static_cast<size_t>(iotWebConf.getApPasswordParameter()->getLength()));
  applySerialDebugRuntimeConfiguration();
  persistConfigurationState("config save");
  applyRuntimeConfiguration();
}

void applyRuntimeConfiguration()
{
  applySerialDebugRuntimeConfiguration();
  updateCoords();
  processTimezone();
  if (gps.activeBaud != gpsConfiguredBaud())
    restartGpsUart("config update");
  gpsClock.refreshNtpServer();
  rebuildApiUrls();
  noteDiagnosticPending(DiagnosticService::Weather, isApiValid(weatherapi.value()),
                        isApiValid(weatherapi.value()) ? "Configured" : "Missing API key",
                        isApiValid(weatherapi.value())
                            ? String(F("Weather refresh will run when Wi-Fi, coordinates, and schedules allow it."))
                            : String(F("OpenWeather One Call 3.0 API key is not configured.")),
                        checkweather.retries);
  noteDiagnosticPending(DiagnosticService::AirQuality, isApiValid(weatherapi.value()),
                        isApiValid(weatherapi.value()) ? "Configured" : "Missing API key",
                        isApiValid(weatherapi.value())
                            ? String(F("AQI refresh will run when Wi-Fi, coordinates, and schedules allow it."))
                            : String(F("OpenWeather API key is not configured for AQI requests.")),
                        checkaqi.retries);
  noteDiagnosticPending(DiagnosticService::IpGeo, isApiValid(ipgeoapi.value()),
                        isApiValid(ipgeoapi.value()) ? "Configured" : "Missing API key",
                        isApiValid(ipgeoapi.value())
                            ? String(F("IP geolocation will refresh after Wi-Fi is online."))
                            : String(F("ipgeolocation.io API key is not configured.")),
                        checkipgeo.retries);
  showclock.fstop = 250;
  if (flickerfast.isChecked())
    showclock.fstop = 20;
  cotimer.scrollspeed = map(text_scroll_speed.value(), 1, 10, 100, 10);
  runtimeState.userBrightness = calcbright(brightness_level.value());
  ESP_LOGI(TAG, "Configuration was updated.");
  showready.reset = resetdefaults.isChecked();
  runtimeState.firstTimeFailsafe = false;
}

// This function is called when the WiFi module is connected to the network.
void wifiConnected()
{
  ESP_LOGI(TAG, "WiFi is now connected.");
  noteDiagnosticSuccess(DiagnosticService::Wifi, true, "Connected",
                        String(F("Wi-Fi connected at ")) + WiFi.localIP().toString());
  gpsClock.refreshNtpServer();
  gpsClock.ntpReady();
  display_showStatus();
  matrix->show();
  systemClock.forceSync();
  showready.ip = true;
}

bool connectAp(const char *apName, const char *password)
{
  noteDiagnosticPending(DiagnosticService::Wifi, true, "Captive portal",
                        String(F("Setup access point ")) + apName + F(" is active for configuration."));
  return WiFi.softAP(apName, password, 4);
}

void connectWifi(const char *ssid, const char *password)
{
  gpsClock.prepareServerSelection();
  noteDiagnosticPending(DiagnosticService::Wifi, true, "Connecting",
                        String(F("Attempting to join SSID ")) + ssid + F("."));
  WiFi.begin(ssid, password);
}

bool isSupportedGpsBaud(uint32_t baud)
{
  return isSupportedGpsBaudValue(baud);
}

uint32_t gpsConfiguredBaud()
{
  const int32_t configured = gps_baud.value();
  return isSupportedGpsBaudValue(static_cast<uint32_t>(configured)) ? static_cast<uint32_t>(configured) : DEFAULT_GPS_BAUD;
}

uint32_t gpsActiveBaud()
{
  return gps.activeBaud != 0 ? gps.activeBaud : gpsConfiguredBaud();
}

void clearGpsRawNmea()
{
  sGpsRawNmeaHead = 0;
  sGpsRawNmeaLength = 0;
  memset(sGpsRawNmeaBuffer, 0, sizeof(sGpsRawNmeaBuffer));
}

String getGpsRawNmeaSnapshot()
{
  if (sGpsRawNmeaLength == 0)
    return String();

  String snapshot;
  snapshot.reserve(sGpsRawNmeaLength + 1U);
  const size_t start = (sGpsRawNmeaHead + kGpsRawNmeaCapacity - sGpsRawNmeaLength) % kGpsRawNmeaCapacity;
  size_t index = start;
  for (size_t copied = 0; copied < sGpsRawNmeaLength; ++copied)
  {
    snapshot += sGpsRawNmeaBuffer[index];
    index = (index + 1U) % kGpsRawNmeaCapacity;
  }
  return snapshot;
}

size_t gpsRawNmeaLength()
{
  return sGpsRawNmeaLength;
}

void restartGpsUart(const char *reason)
{
  startGpsUart(reason == nullptr ? "manual restart" : reason, true);
  noteDiagnosticPending(DiagnosticService::Gps, true, "UART restarted",
                        String(F("GPS UART restarted at ")) + gpsActiveBaud() + F(" baud.") +
                            (reason == nullptr ? String() : String(F(" Reason: ")) + reason));
}

void resetGpsParser(const char *reason, bool restartUart)
{
  GPS = TinyGPSPlus();
  clearGpsRuntimeState();
  ++gps.parserResetCount;
  gps.lastResetMillis = millis();
  strlcpy(gps.lastResetReason, reason == nullptr ? "manual reset" : reason, sizeof(gps.lastResetReason));
  clearGpsRawNmea();
  updateCoords();
  updateLocation();
  ESP_LOGW(TAG, "GPS parser reset requested (%s).", gps.lastResetReason);
  if (restartUart)
    startGpsUart(gps.lastResetReason, true);

  noteDiagnosticPending(DiagnosticService::Gps, true, restartUart ? "Parser reset" : "Parser cleared",
                        String(F("GPS parser state was reset. Listening on RX ")) + GPS_RX_PIN +
                            F(" / TX ") + GPS_TX_PIN + F(" at ") + gpsActiveBaud() + F(" baud."));
}

void processGpsSerialByte(int incomingByte)
{
  if (incomingByte < 0)
    return;

  appendGpsRawNmeaByte(static_cast<uint8_t>(incomingByte));
  GPS.encode(static_cast<char>(incomingByte));
}

// Regular Functions
bool isNextShowReady(acetime_t lastshown, uint32_t interval, uint32_t multiplier)
{
  // Calculate current time in seconds
  acetime_t now = systemClock.getNow();
  // Check if the elapsed time since the last show has reached the configured interval.
  bool showReady = (now - lastshown) >= static_cast<acetime_t>(interval * multiplier);
  return showReady;
}

bool isNextAttemptReady(acetime_t lastattempt)
{
  // wait 1 minute between attempts
  return (systemClock.getNow() - lastattempt) > T1M;
}

// Returns true if the current coordinates are valid
bool isCoordsValid()
{
  return !(current.lat == 0.0 && current.lon == 0.0);
}

bool isLocationValid(String source)
{
  auto hasLocationText = [](const char *value) {
    return value[0] != '\0' && !(value[0] == '0' && value[1] == '\0');
  };

  bool result = false;
  if (source == "geocode")
  {
    if (hasLocationText(geocode.city))
      result = true;
  }
  else if (source == "current")
  {
    if (hasLocationText(current.city))
      result = true;
  }
  else if (source == "saved")
  {
    if (hasLocationText(savedcity.value()))
      result = true;
  }
  return result;
}

void print_debugData(ConsoleMirrorPrint &out)
{
  acetime_t now = systemClock.getNow();
  char tempunit[7];
  char speedunit[7];
  char distanceunit[7];
  if (imperial.isChecked())
  {
    memcpy(tempunit, imperial_units[3], 7);
    memcpy(speedunit, imperial_units[1], 7);
    memcpy(distanceunit, imperial_units[2], 7);
  }
  else
  {
    memcpy(tempunit, metric_units[3], 7);
    memcpy(speedunit, metric_units[1], 7);
    memcpy(distanceunit, metric_units[2], 7);
  }
  out.printf("[^----------------------------------------------------------------^]\n");
  out.printf("Current Time: %s\n", getSystemZonedTimestamp().c_str());
  out.printf("Boot Time: %s\n", getCustomZonedTimestamp(runtimeState.bootTime).c_str());
  out.printf("Sunrise Time: %s\n", getCustomZonedTimestamp(weather.day.sunrise).c_str());
  out.printf("Sunset Time: %s\n", getCustomZonedTimestamp(weather.day.sunset).c_str());
  out.printf("Moonrise Time: %s\n", getCustomZonedTimestamp(weather.day.moonrise).c_str());
  out.printf("Moonset Time: %s\n", getCustomZonedTimestamp(weather.day.moonset).c_str());
  out.printf("Location: ");
  if (isLocationValid("current"))
    out.printf("%s, %s, %s\n", current.city, current.state, current.country);
  else
    out.printf("Currently not known\n");
  out.printf("[^----------------------------------------------------------------^]\n");
  out.printf("Firmware - Version:v%s\n", VERSION_SEMVER);
  out.printf("Boot - LastRebootReason:%s\n",
             runtimeState.lastRebootReason[0] == '\0' ? "Unknown" : runtimeState.lastRebootReason);
  out.printf("System - RawLux:%d | Lux:%d | UsrBright:+%d | Brightness:%d | ClockHue:%d | TempHue:%d | WifiState:%s | HttpReady:%s | IP:%s | Uptime:%s | Heap:%lu | MinHeap:%lu\n",
             current.rawlux,
             current.lux,
             runtimeState.userBrightness,
             current.brightness,
             current.clockhue,
             current.temphue,
             (connection_state[iotWebConf.getState()]),
             (yesno[isHttpReady()]),
             ((WiFi.localIP()).toString()).c_str(),
             elapsedTime(now - runtimeState.bootTime).c_str(),
             static_cast<unsigned long>(ESP.getFreeHeap()),
             static_cast<unsigned long>(ESP.getMinFreeHeap()));
  out.printf("Clock - Status:%s | TimeSource:%s | CurrentTZ:%s | Timezone:%s | DstActive:%s | NtpReady:%s | NtpServer:%s(%s) | Skew:%d Seconds | LastAttempt:%s | NextAttempt:%s | NextNtp:%s | LastSync:%s\n", clock_status[systemClock.getSyncStatusCode()], runtimeState.timeSource, getSystemTimezoneOffsetString().c_str(), getSystemTimezoneName().c_str(), yesno[isSystemTimezoneDstActive()], yesno[gpsClock.ntpIsReady], runtimeState.ntpServer, runtimeState.ntpServerSource, systemClock.getClockSkew(), elapsedTime(systemClock.getSecondsSinceSyncAttempt()).c_str(), elapsedTime(systemClock.getSecondsToSyncAttempt()).c_str(), elapsedTime((now - runtimeState.lastNtpCheck) - NTPCHECKTIME * 60).c_str(), elapsedTime(now - systemClock.getLastSyncTime()).c_str());
  out.printf("Loc - SavedLat:%.5f | SavedLon:%.5f | CurrentLat:%.5f | CurrentLon:%.5f | LocValid:%s\n", atof(savedlat.value()), atof(savedlon.value()), current.lat, current.lon, yesno[isCoordsValid()]);
  out.printf("IPGeo - Complete:%s | Lat:%.5f | Lon:%.5f | TZoffset:%d | Timezone:%s | ValidApi:%s | Retries:%d | LastAttempt:%s | LastSuccess:%s\n", yesno[checkipgeo.complete], ipgeo.lat, ipgeo.lon, ipgeo.tzoffset, ipgeo.timezone, yesno[isApiValid(ipgeoapi.value())], checkipgeo.retries, elapsedTime(now - checkipgeo.lastattempt).c_str(), elapsedTime(now - checkipgeo.lastsuccess).c_str());
  out.printf("GPS - Baud:%lu | Chars:%s | With-Fix:%s | Failed:%s | Passed:%s | Sats:%d | Hdop:%d | Elev:%d | Lat:%.5f | Lon:%.5f | FixAge:%s | LocAge:%s\n",
             static_cast<unsigned long>(gpsActiveBaud()),
             formatLargeNumber(GPS.charsProcessed()).c_str(),
             formatLargeNumber(GPS.sentencesWithFix()).c_str(),
             formatLargeNumber(GPS.failedChecksum()).c_str(),
             formatLargeNumber(GPS.passedChecksum()).c_str(),
             gps.sats,
             gps.hdop,
             gps.elevation,
             gps.lat,
             gps.lon,
             formatGpsEventAge(gps.timestamp).c_str(),
             formatGpsEventAge(gps.lockage).c_str());
  out.printf("Weather Current - Icon:%s | Temp:%d%s | FeelsLike:%d%s | Humidity:%d%% | Clouds:%d%% | Wind:%d/%d%s | UVI:%d(%s) | Desc:%s | ValidApi:%s | Retries:%d/%d | LastAttempt:%s | LastSuccess:%s | NextShow:%s\n", weather.current.icon, weather.current.temp, tempunit, weather.current.feelsLike, tempunit, weather.current.humidity, weather.current.cloudcover, weather.current.windSpeed, weather.current.windGust, speedunit, weather.current.uvi, uv_index(weather.current.uvi).c_str(), weather.current.description, yesno[isApiValid(weatherapi.value())], checkweather.retries, HTTP_MAX_RETRIES, elapsedTime(now - checkweather.lastattempt).c_str(), elapsedTime(now - checkweather.lastsuccess).c_str(), nextShowDebug(now, lastshown.currentweather, static_cast<uint32_t>(current_weather_interval.value()) * T1H).c_str());
  out.printf("Weather Day - Icon:%s | LoTemp:%d%s | HiTemp:%d%s | Humidity:%d%% | Clouds:%d%% | Wind: %d/%d%s | UVI:%d(%s) | MoonPhase:%.2f | Desc: %s | NextShow:%s\n", weather.day.icon, weather.day.tempMin, tempunit, weather.day.tempMax, tempunit, weather.day.humidity, weather.day.cloudcover, weather.day.windSpeed, weather.day.windGust, speedunit, weather.day.uvi, uv_index(weather.day.uvi).c_str(), weather.day.moonPhase, weather.current.description, nextShowDebug(now, lastshown.dayweather, static_cast<uint32_t>(daily_weather_interval.value()) * T1H).c_str());
  out.printf("AQI Current - Index:%s | Co:%.2f | No:%.2f | No2:%.2f | Ozone:%.2f | So2:%.2f | Pm2.5:%.2f | Pm10:%.2f | Ammonia:%.2f | Desc: %s | Retries:%d/%d | LastAttempt:%s | LastSuccess:%s | NextShow:%s\n", air_quality[safeAqiIndex(aqi.current.aqi)], aqi.current.co, aqi.current.no, aqi.current.no2, aqi.current.o3, aqi.current.so2, aqi.current.pm25, aqi.current.pm10, aqi.current.nh3, (aqi.current.description).c_str(), checkaqi.retries, HTTP_MAX_RETRIES, elapsedTime(now - checkaqi.lastattempt).c_str(), elapsedTime(now - checkaqi.lastsuccess).c_str(), nextShowDebug(now, lastshown.aqi, static_cast<uint32_t>(aqi_interval.value()) * T1M).c_str());
  out.printf("AQI Day - Index:%s | Co:%.2f | No:%.2f | No2:%.2f | Ozone:%.2f | So2:%.2f | Pm2.5:%.2f | Pm10:%.2f | Ammonia:%.2f\n", air_quality[safeAqiIndex(aqi.day.aqi)], aqi.day.co, aqi.day.no, aqi.day.no2, aqi.day.o3, aqi.day.so2, aqi.day.pm25, aqi.day.pm10, aqi.day.nh3);
  out.printf("Alerts - Active:%s | Count:%u | Watch:%u | Warn:%u | Retries:%d | LastAttempt:%s | LastSuccess:%s | LastShown:%s\n",
             yesno[alerts.active],
             alerts.count,
             alerts.watchCount,
             alerts.warningCount,
             checkalerts.retries,
             elapsedTime(now - checkalerts.lastattempt).c_str(),
             elapsedTime(now - checkalerts.lastsuccess).c_str(),
             elapsedTime(now - lastshown.alerts).c_str());
  out.printf("[^----------------------------------------------------------------^]\n");
  out.printf("Main task stack size: %s bytes remaining\n", formatLargeNumber(uxTaskGetStackHighWaterMark(NULL)).c_str());
  out.printf("HTTP Client - Busy:%s | Endpoint:%s | BusyFor:%lu ms\n",
             yesno[networkService.busy],
             runtimeState.networkBusyEndpoint[0] == '\0' ? "none" : runtimeState.networkBusyEndpoint,
             networkService.busy && runtimeState.networkBusySinceMillis != 0
                 ? static_cast<unsigned long>(millis() - runtimeState.networkBusySinceMillis)
                 : 0UL);
  out.printf("Current Display Tokens: %s\n", displaytoken.showTokens().c_str());
  out.printf("Display Blocked For: %lu ms\n",
             runtimeState.displayBlockedSinceMillis == 0
                 ? 0UL
                 : static_cast<unsigned long>(millis() - runtimeState.displayBlockedSinceMillis));
  out.printf("[^----------------------------------------------------------------^]\n");
  if (alerts.active)
  {
    for (uint8_t index = 0; index < alerts.count; ++index)
    {
      const AlertEntry &entry = alerts.items[index];
      out.printf("*Alert %u/%u - Status:%s | Severity:%s | Certainty:%s | Urgency:%s | Event:%s | Headline:%s | Desc:%s | Inst:%s | Display:%s\n",
                 index + 1U,
                 alerts.count,
                 entry.status,
                 entry.severity,
                 entry.certainty,
                 entry.urgency,
                 entry.event,
                 entry.headline,
                 entry.description,
                 entry.instruction,
                 entry.displayText);
    }
  }
}

void print_debugData()
{
  ConsoleMirrorPrint out(true);
  print_debugData(out);
}

void print_gpsStatus(ConsoleMirrorPrint &out)
{
  uint32_t nowMillis = millis();
  String lastByteAge = gps.moduleDetected ? elapsedTime((nowMillis - gps.lastByteMillis + 999UL) / 1000UL) : String(F("Never"));
  String locationAge = formatGpsDataAge(GPS.location.age());
  String timeAge = formatGpsDataAge(GPS.time.age());
  String satsAge = formatGpsDataAge(GPS.satellites.age());
  String altitudeAge = formatGpsDataAge(GPS.altitude.age());
  String hdopAge = formatGpsDataAge(GPS.hdop.age());
  String lastResetAge = gps.lastResetMillis == 0 ? String(F("Never"))
                                                 : elapsedTime((nowMillis - gps.lastResetMillis + 999UL) / 1000UL);

  out.printf("[^------------------------- GPS Status --------------------------^]\n");
  out.printf("GPS UART - RX:%d | TX:%d | Baud:%lu | ModuleDetected:%s | LastByte:%s | SerialAvail:%d | Restarts:%lu\n",
         GPS_RX_PIN, GPS_TX_PIN, static_cast<unsigned long>(gpsActiveBaud()), yesno[gps.moduleDetected], lastByteAge.c_str(), Serial1.available(),
         static_cast<unsigned long>(gps.uartRestartCount));
  out.printf("GPS Parser - Chars:%s | Passed:%s | Failed:%s | SentencesWithFix:%s\n",
         formatLargeNumber(GPS.charsProcessed()).c_str(),
         formatLargeNumber(GPS.passedChecksum()).c_str(),
         formatLargeNumber(GPS.failedChecksum()).c_str(),
         formatLargeNumber(GPS.sentencesWithFix()).c_str());
  out.printf("GPS Recovery - ParserResets:%lu | LastReset:%s | Reason:%s | RawBytes:%s | RawSentences:%s\n",
         static_cast<unsigned long>(gps.parserResetCount),
         lastResetAge.c_str(),
         gps.lastResetReason[0] == '\0' ? "n/a" : gps.lastResetReason,
         formatLargeNumber(static_cast<int>(gps.rawBytesCaptured)).c_str(),
         formatLargeNumber(static_cast<int>(gps.rawSentenceCount)).c_str());
  out.printf("GPS State - Fix:%s | WaitingForFix:%s | Sats:%u | LocationValid:%s | TimeValid:%s | DateValid:%s\n",
         yesno[gps.fix], yesno[gps.waitingForFix], gps.sats,
         yesno[GPS.location.isValid()], yesno[GPS.time.isValid()], yesno[GPS.date.isValid()]);
  out.printf("GPS Ages - Location:%s | Time:%s | Satellites:%s | Altitude:%s | HDOP:%s\n",
         locationAge.c_str(), timeAge.c_str(), satsAge.c_str(), altitudeAge.c_str(), hdopAge.c_str());
  out.printf("GPS Live - Lat:%.6f | Lon:%.6f | Elev:%dft | HDOP:%d | LastFix:%s | LastLocation:%s\n",
         gps.lat, gps.lon, gps.elevation, gps.hdop,
         formatGpsEventAge(gps.timestamp).c_str(),
         formatGpsEventAge(gps.lockage).c_str());
  out.printf("GPS Override - FixedLocation:%s | CurrentCoords:%.6f,%.6f | CurrentSource:%s\n",
         yesno[enable_fixed_loc.isChecked()], current.lat, current.lon, current.locsource);
  out.printf("[^----------------------------------------------------------------^]\n");
}

void print_gpsStatus()
{
  ConsoleMirrorPrint out(true);
  print_gpsStatus(out);
}

void print_gpsRawNmea(ConsoleMirrorPrint &out)
{
  out.printf("[^------------------------ GPS Raw NMEA -------------------------^]\n");
  String snapshot = getGpsRawNmeaSnapshot();
  out.printf("%s\n", snapshot.length() > 0 ? snapshot.c_str() : "No raw NMEA captured yet.");
  out.printf("[^----------------------------------------------------------------^]\n");
}

void print_gpsRawNmea()
{
  ConsoleMirrorPrint out(true);
  print_gpsRawNmea(out);
}

void logRuntimeHealth()
{
  const RuntimeHealthSnapshot snapshot = captureRuntimeHealthSnapshot();
  ESP_LOGI(TAG,
           "Runtime health: heap:%lu minHeap:%lu stack:%lu wifi:%s wifiStatus:%d ip:%s httpBusy:%s endpoint:%s busyMs:%lu tokens:%s showClockSusp:%s alertflash:%s scroll:%s scrollPos:%d",
           snapshot.heap,
           snapshot.minHeap,
           snapshot.stack,
           connection_state[snapshot.wifiState],
           snapshot.wifiStatus,
           snapshot.ipAddress.c_str(),
           yesno[snapshot.httpBusy],
           snapshot.endpoint,
           snapshot.busyMs,
           snapshot.tokens.c_str(),
           yesno[snapshot.showClockSuspended],
           yesno[snapshot.alertFlashActive],
           yesno[snapshot.scrollActive],
           snapshot.scrollPosition);
}

void print_runtimeHealth(ConsoleMirrorPrint &out)
{
  const RuntimeHealthSnapshot snapshot = captureRuntimeHealthSnapshot();
  out.printf("[^---------------------- Runtime Health ----------------------^]\n");
  out.printf("Snapshot - UptimeMs:%lu | LastAutoLog:%lu ms ago\n",
             static_cast<unsigned long>(snapshot.nowMs),
             snapshot.lastHealthLogAgeMs);
  out.printf("Memory - Heap:%lu | MinHeap:%lu | Stack:%lu bytes remaining\n",
             snapshot.heap,
             snapshot.minHeap,
             snapshot.stack);
  out.printf("Network - WifiState:%s | WiFiStatus:%d | IP:%s\n",
             connection_state[snapshot.wifiState],
             snapshot.wifiStatus,
             snapshot.ipAddress.c_str());
  out.printf("HTTP - Ready:%s | Busy:%s | Endpoint:%s | BusyFor:%lu ms\n",
             yesno[snapshot.httpReady],
             yesno[snapshot.httpBusy],
             snapshot.endpoint,
             snapshot.busyMs);
  out.printf("Display - Tokens:%s\n", snapshot.tokens.c_str());
  out.printf("Display - ShowClockSuspended:%s | AlertFlash:%s | Scroll:%s\n",
             yesno[snapshot.showClockSuspended],
             yesno[snapshot.alertFlashActive],
             yesno[snapshot.scrollActive]);
  out.printf("Display - ScrollPos:%d | ScrollSize:%d | BlockedFor:%lu ms\n",
             snapshot.scrollPosition,
             snapshot.scrollSize,
             snapshot.displayBlockedMs);
  out.printf("[^------------------------------------------------------------^]\n");
}

void print_runtimeHealth()
{
  ConsoleMirrorPrint out(true);
  print_runtimeHealth(out);
}

bool handleDebugCommand(char input, ConsoleMirrorPrint &out, bool allowImmediateRestart)
{
  const char command = static_cast<char>(tolower(static_cast<unsigned char>(input)));
  switch (command)
  {
  case 'd':
    print_debugData(out);
    return true;
  case 'g':
    print_gpsStatus(out);
    return true;
  case 'n':
    print_gpsRawNmea(out);
    return true;
  case 'p':
    resetGpsParser("debug command", true);
    out.printf("GPS parser reset and UART restarted at %lu baud.\n", static_cast<unsigned long>(gpsActiveBaud()));
    return true;
  case 'u':
    restartGpsUart("debug command");
    out.printf("GPS UART restarted at %lu baud.\n", static_cast<unsigned long>(gpsActiveBaud()));
    return true;
  case 's':
    CoroutineScheduler::list(out);
    return true;
  case 'm':
    print_runtimeHealth(out);
    return true;
  case 'a':
    if (!checkaqi.complete)
    {
      out.printf("AQI test unavailable. No air quality data has been loaded yet.\n");
      return true;
    }
    showready.testaqi = true;
    out.printf("Queued AQI scroller test without changing the next scheduled run.\n");
    return true;
  case 'w':
    if (!checkweather.complete)
    {
      out.printf("Current weather test unavailable. No weather data has been loaded yet.\n");
      return true;
    }
    showready.testcurrentweather = true;
    out.printf("Queued current weather scroller test without changing the next scheduled run.\n");
    return true;
  case 'e':
    lastshown.date = systemClock.getNow() - ((date_interval.value()) * 60 * 60);
    showready.date = true;
    out.printf("Triggered date display.\n");
    return true;
  case 'q':
    if (!checkweather.complete)
    {
      out.printf("Daily weather test unavailable. No weather data has been loaded yet.\n");
      return true;
    }
    showready.testdayweather = true;
    out.printf("Queued daily weather scroller test without changing the next scheduled run.\n");
    return true;
  case 'x':
    showready.testalerts = true;
    out.printf("Queued alert scroller test%s.\n", alerts.active ? "" : " using a sample alert because none are active");
    return true;
  case 'y':
    if (!checkweather.complete)
    {
      out.printf("Temperature/icon test unavailable. No weather data has been loaded yet.\n");
      return true;
    }
    showready.testcurrenttemp = true;
    out.printf("Queued temperature and icon display test without changing the next scheduled run.\n");
    return true;
  case 'c':
    out.printf("Main task stack size: %s bytes remaining\n", formatLargeNumber(uxTaskGetStackHighWaterMark(NULL)).c_str());
    return true;
  case 't':
    startFlash(hex2rgb(system_color.value()), 5);
    out.printf("Triggered alert flash test.\n");
    return true;
#ifdef COROUTINE_PROFILER
  case 'f':
    LogBinTableRenderer::printTo(out, 2 /*startBin*/, 13 /*endBin*/, false /*clear*/);
    return true;
#endif
  case 'l':
  {
    const bool enabled = !serialdebug.isChecked();
    const bool saved = setSerialDebugConfiguration(enabled, "serial debug toggle");
    out.printf("Serial debug output %s. Log level is %s.\n",
               enabled ? "enabled" : "disabled",
               enabled ? "DEBUG" : "INFO");
    if (!saved)
      out.printf("Warning: serial debug setting could not be saved to configuration.\n");
    return true;
  }
  case 'r':
    out.printf("Rebooting...\n");
    if (allowImmediateRestart)
      ESP.restart();
    else
    {
      runtimeState.rebootRequested = true;
      runtimeState.rebootRequestMillis = millis();
    }
    return true;
  case 'h':
    out.printf("Help:\n");
    out.printf("  d: Display debug data\n");
    out.printf("  g: Display GPS status\n");
    out.printf("  n: Display retained raw GPS NMEA tail\n");
    out.printf("  p: Reset GPS parser and restart GPS UART\n");
    out.printf("  u: Restart GPS UART using the configured baud\n");
    out.printf("  s: Display coroutines states\n");
    out.printf("  m: Display runtime health snapshot\n");
    out.printf("  a: Test AQI scroller without changing schedule\n");
    out.printf("  w: Test current weather scroller without changing schedule\n");
    out.printf("  e: Display the current date\n");
    out.printf("  q: Test daily weather scroller without changing schedule\n");
    out.printf("  x: Test alert scroller (uses a sample alert if none are active)\n");
    out.printf("  y: Test temperature and icon display without changing schedule\n");
    out.printf("  c: Display current main task stack size remaining (bytes)\n");
    out.printf("  t: Test alertflash\n");
#ifdef COROUTINE_PROFILER
    out.printf("  f: Display coroutine execution time table (Profiler)\n");
#endif
    out.printf("  l: Toggle serial debug output and DEBUG log level\n");
    out.printf("  r: Reboot clock\n");
    out.printf("  h: Display this help\n");
    return true;
  default:
    out.printf("Unknown command: %c\n", input);
    return false;
  }
}

void startFlash(uint16_t color, uint8_t laps)
{
  alertflash.color = color;
  alertflash.laps = laps;
  alertflash.active = true;
  showClock.suspend();
}

void startScroll(uint16_t color, bool displayicon)
{
  if (!hasVisibleText(scrolltext.message))
  {
    scrolltext.active = false;
    scrolltext.displayicon = false;
    scrolltext.position = mw - 1;
    scrolltext.size = 0;
    ESP_LOGW(TAG, "startScroll(): refusing to start an empty scroll message");
    return;
  }

  scrolltext.color = color;
  scrolltext.active = true;
  scrolltext.displayicon = displayicon;
  showClock.suspend();
}

// Process timezone to set current timezone offsets by taking into account user setting or ipgeolocation data
void processTimezone()
{
  uint32_t timer = millis();
  int16_t savedoffset = savedtzoffset.value();
  int16_t fixedoffset = fixed_offset.value();
  const char *savedZoneName = savedtimezone.value();
  const acetime_t nowEpoch = systemClock.getNow();

  auto applyTimezoneOffset = [&](int16_t offset, const char *source) {
    current.tzoffset = offset;
    current.timezone = TimeZone::forHours(offset);
    ESP_LOGD(TAG, "Using %s fixed timezone offset: %s", source,
             formatTimeOffset(TimeOffset::forHours(offset)).c_str());
  };

  auto setTimezoneSource = [&](const char *source) {
    strlcpy(runtimeState.timezoneSource, source, sizeof(runtimeState.timezoneSource));
  };

  auto persistTimezoneSelection = [&](int16_t offset, const char *zoneName, const char *source) {
    bool changed = false;
    if (offset != savedoffset)
    {
      ESP_LOGI(TAG, "%s timezone offset [%s] is different than saved timezone offset [%s], saving new timezone",
               source,
               formatTimeOffset(TimeOffset::forHours(offset)).c_str(),
               formatTimeOffset(TimeOffset::forHours(savedoffset)).c_str());
      savedtzoffset.value() = offset;
      savedoffset = offset;
      changed = true;
    }

    if (zoneName != nullptr && strcmp(savedtimezone.value(), zoneName) != 0)
    {
      ESP_LOGI(TAG, "%s timezone name [%s] is different than saved timezone name [%s], saving new timezone", source, zoneName, savedtimezone.value());
      strlcpy(savedtimezone.value(), zoneName, 64);
      savedZoneName = savedtimezone.value();
      changed = true;
    }

    if (!changed)
      return;
    persistConfigurationState("timezone update");
  };

  if (enable_fixed_tz.isChecked())
  {
    applyTimezoneOffset(fixedoffset, "fixed");
    persistTimezoneSelection(fixedoffset, nullptr, "Fixed");
    setTimezoneSource("Fixed override");
    noteDiagnosticPending(DiagnosticService::Timekeeping, true, "Fixed timezone",
                          String(F("Using a manual GMT offset of ")) + getSystemTimezoneOffsetString() + F(" with no automatic DST rules."));
  }
  else if (applyNamedTimezone(ipgeo.timezone, "ip geolocation"))
  {
    persistTimezoneSelection(getTimezoneOffsetAt(nowEpoch).toMinutes() / 60, ipgeo.timezone, "IP Geo");
    setTimezoneSource("IP geolocation name");
  }
  else if (applyNamedTimezone(savedZoneName, "saved"))
  {
    persistTimezoneSelection(getTimezoneOffsetAt(nowEpoch).toMinutes() / 60, savedZoneName, "Saved");
    setTimezoneSource("Saved timezone name");
  }
  else
  {
    int16_t gpsDerivedOffset = 0;
    if (gps.fix && hasValidCoordinatePair(gps.lat, gps.lon) &&
        deriveTimezoneOffsetFromLongitude(gps.lon, gpsDerivedOffset))
    {
      applyTimezoneOffset(gpsDerivedOffset, "gps longitude");
      persistTimezoneSelection(gpsDerivedOffset, nullptr, "GPS");
      setTimezoneSource("GPS longitude fallback");
      noteDiagnosticPending(DiagnosticService::Timekeeping, true, "GPS-derived timezone",
                            String(F("Using GPS longitude ")) + String(gps.lon, 5) +
                                F(" for timezone offset fallback: GMT") + getSystemTimezoneOffsetString() +
                                F(" (DST rules unavailable without a named timezone)."));
    }
    else if (ipgeo.tzoffset != 127)
    {
      applyTimezoneOffset(ipgeo.tzoffset, "ip geolocation");
      persistTimezoneSelection(ipgeo.tzoffset, nullptr, "IP Geo");
      setTimezoneSource("IP geolocation offset");
    }
    else if (savedoffset != 0 || fixedoffset == 0)
    {
      applyTimezoneOffset(savedoffset, "saved");
      setTimezoneSource("Saved offset");
    }
    else
    {
      applyTimezoneOffset(fixedoffset, "configured fallback");
      persistTimezoneSelection(fixedoffset, nullptr, "Configured fallback");
      setTimezoneSource("Configured fallback");
    }
  }
  ESP_LOGV(TAG, "Process timezone complete: %lu ms", (millis() - timer));
}

bool getActiveSunEvents(acetime_t nowEpoch, acetime_t &sunriseEpoch, acetime_t &sunsetEpoch)
{
  const bool hasWeatherSunrise = weather.day.sunrise > 0;
  const bool hasWeatherSunset = weather.day.sunset > 0;
  bool usingGpsFix = false;
  acetime_t fallbackSunrise = 0;
  acetime_t fallbackSunset = 0;
  const bool hasFallbackSunEvents = computeFallbackSunEvents(nowEpoch, fallbackSunrise, fallbackSunset, usingGpsFix);

  if (hasWeatherSunrise && hasWeatherSunset)
  {
    sunriseEpoch = weather.day.sunrise;
    sunsetEpoch = weather.day.sunset;
    runtimeState.activeSunriseEpoch = sunriseEpoch;
    runtimeState.activeSunsetEpoch = sunsetEpoch;
    strlcpy(runtimeState.solarTimesSource, "Weather API", sizeof(runtimeState.solarTimesSource));
    return true;
  }

  if ((hasWeatherSunrise || hasWeatherSunset) && hasFallbackSunEvents)
  {
    sunriseEpoch = hasWeatherSunrise ? weather.day.sunrise : fallbackSunrise;
    sunsetEpoch = hasWeatherSunset ? weather.day.sunset : fallbackSunset;
    runtimeState.activeSunriseEpoch = sunriseEpoch;
    runtimeState.activeSunsetEpoch = sunsetEpoch;
    strlcpy(runtimeState.solarTimesSource,
            usingGpsFix ? "Weather+GPS fallback" : "Weather+Coord fallback",
            sizeof(runtimeState.solarTimesSource));
    return true;
  }

  if (hasFallbackSunEvents)
  {
    sunriseEpoch = fallbackSunrise;
    sunsetEpoch = fallbackSunset;
    runtimeState.activeSunriseEpoch = fallbackSunrise;
    runtimeState.activeSunsetEpoch = fallbackSunset;
    strlcpy(runtimeState.solarTimesSource,
            usingGpsFix ? "GPS fallback" : "Coordinate fallback",
            sizeof(runtimeState.solarTimesSource));
    return true;
  }

  sunriseEpoch = 0;
  sunsetEpoch = 0;
  runtimeState.activeSunriseEpoch = 0;
  runtimeState.activeSunsetEpoch = 0;
  strlcpy(runtimeState.solarTimesSource, "Unavailable", sizeof(runtimeState.solarTimesSource));
  return false;
}

void updateLocation()
{
  // Update location information
  if (isLocationValid("geocode"))
  {
    const bool locationChanged = !cmpLocs(geocode.city, current.city) ||
                                 !cmpLocs(geocode.state, current.state) ||
                                 !cmpLocs(geocode.country, current.country);
    if (show_loc_change.isChecked() && locationChanged)
    {
      showClock.suspend();
      showready.loc = true;
    }
    // Set current location fields
    strlcpy(current.city, geocode.city, sizeof(current.city));
    strlcpy(current.state, geocode.state, sizeof(current.state));
    strlcpy(current.country, geocode.country, sizeof(current.country));
    ESP_LOGI(TAG, "Using Geocode location: %s, %s, %s", current.city, current.state, current.country);
    noteDiagnosticSuccess(DiagnosticService::Geocode, isApiValid(weatherapi.value()), "Resolved",
                          String(current.city) + F(", ") + current.state + F(", ") + current.country,
                          checkgeocode.retries, 200);
  }
  else if (isLocationValid("saved"))
  {
    // Set current location fields to saved location
    strlcpy(current.city, savedcity.value(), sizeof(current.city));
    strlcpy(current.state, savedstate.value(), sizeof(current.state));
    strlcpy(current.country, savedcountry.value(), sizeof(current.country));
    ESP_LOGD(TAG, "Using saved location: %s, %s, %s", current.city, current.state, current.country);
    noteDiagnosticPending(DiagnosticService::Geocode, isApiValid(weatherapi.value()), "Saved fallback",
                          String(current.city) + F(", ") + current.state + F(", ") + current.country,
                          checkgeocode.retries);
  }
  else if (isLocationValid("current"))
  {
    ESP_LOGD(TAG, "No new location update, using current location: %s, %s, %s", current.city, current.state, current.country);
    noteDiagnosticPending(DiagnosticService::Geocode, isApiValid(weatherapi.value()), "Using current",
                          String(current.city) + F(", ") + current.state + F(", ") + current.country,
                          checkgeocode.retries);
  }
  else
  {
    ESP_LOGD(TAG, "No valid locations found");
    noteDiagnosticPending(DiagnosticService::Geocode, isApiValid(weatherapi.value()), "Waiting",
                          F("A city or region has not been resolved yet."),
                          checkgeocode.retries);
  }
  // Save the new location if needed
  if ((String)savedcity.value() != current.city || (String)savedstate.value() != current.state || (String)savedcountry.value() != current.country)
  {
    ESP_LOGI(TAG, "Location not saved, saving new location: %s, %s, %s", current.city, current.state, current.country);
    // Convert city/state/country to char array and save to savedcity/state/country
    strlcpy(savedcity.value(), current.city, sizeof(current.city));
    strlcpy(savedstate.value(), current.state, sizeof(current.state));
    strlcpy(savedcountry.value(), current.country, sizeof(current.country));
    persistConfigurationState("location update");
  }
}

void updateCoords()
{
  auto coordsUnset = [](double lat, double lon) {
    return lat == 0.0 && lon == 0.0;
  };

  auto absoluteDelta = [](double left, double right) {
    double delta = left - right;
    return delta < 0.0 ? -delta : delta;
  };

  if (enable_fixed_loc.isChecked())
  {
    current.lat = strtod(fixedLat.value(), NULL);
    current.lon = strtod(fixedLon.value(), NULL);
    strlcpy(current.locsource, "User Defined", sizeof(current.locsource));
    noteDiagnosticPending(DiagnosticService::Geocode, isApiValid(weatherapi.value()), "Fixed coordinates",
                          String(F("Services are using fixed coordinates ")) + String(current.lat, 5) + F(", ") + String(current.lon, 5),
                          checkgeocode.retries);
  }
  else if (coordsUnset(gps.lat, gps.lon) && coordsUnset(ipgeo.lat, ipgeo.lon) && coordsUnset(current.lat, current.lon))
  {
    // Set coordinates to saved location
    current.lat = strtod(savedlat.value(), NULL);
    current.lon = strtod(savedlon.value(), NULL);
    strlcpy(current.locsource, "Previous Saved", sizeof(current.locsource));
    noteDiagnosticPending(DiagnosticService::Geocode, isApiValid(weatherapi.value()), "Saved coordinates",
                          String(F("Services are using saved coordinates ")) + String(current.lat, 5) + F(", ") + String(current.lon, 5),
                          checkgeocode.retries);
  }
  else if (coordsUnset(gps.lat, gps.lon) && !coordsUnset(ipgeo.lat, ipgeo.lon) && coordsUnset(current.lat, current.lon))
  {
    // Set coordinates to ip geolocation
    current.lat = ipgeo.lat;
    current.lon = ipgeo.lon;
    strlcpy(current.locsource, "IP Geolocation", sizeof(current.locsource));
    ESP_LOGI(TAG, "Using IPGeo location information: Lat: %lf Lon: %lf", (ipgeo.lat), (ipgeo.lon));
    noteDiagnosticPending(DiagnosticService::Geocode, isApiValid(weatherapi.value()), "IP coordinates",
                          String(F("Services are using IP-derived coordinates ")) + String(current.lat, 5) + F(", ") + String(current.lon, 5),
                          checkgeocode.retries);
  }
  else if (!coordsUnset(gps.lat, gps.lon))
  {
    // Set coordinates to gps location
    current.lat = gps.lat;
    current.lon = gps.lon;
    strlcpy(current.locsource, "GPS", sizeof(current.locsource));
    noteDiagnosticPending(DiagnosticService::Geocode, isApiValid(weatherapi.value()), "GPS coordinates",
                          String(F("Services are using live GPS coordinates ")) + String(current.lat, 5) + F(", ") + String(current.lon, 5),
                          checkgeocode.retries);
  }
  // Calculate if major location shift has taken place
  double olat = strtod(savedlat.value(), NULL);
  double olon = strtod(savedlon.value(), NULL);
  if (absoluteDelta(current.lat, olat) > 0.02 || absoluteDelta(current.lon, olon) > 0.02)
  {
    // Log warning and save new coordinates
    ESP_LOGW(TAG, "Major location shift, saving values");
    ESP_LOGW(TAG, "Lat:[%0.6lf]->[%0.6lf] Lon:[%0.6lf]->[%0.6lf]", olat, current.lat, olon, current.lon);
    dtostrf(current.lat, 9, 5, savedlat.value());
    dtostrf(current.lon, 9, 5, savedlon.value());
    persistConfigurationState("coordinate update");
    checkgeocode.ready = true;
    if (!enable_fixed_tz.isChecked() && gps.fix && hasValidCoordinatePair(gps.lat, gps.lon))
      processTimezone();
  }
  else if (isCoordsValid() && !isLocationValid("geocode") && !isLocationValid("current"))
  {
    checkgeocode.ready = true;
  }
  // Rebuild API endpoints after coordinates change.
  rebuildApiUrls();
}

void display_setclockDigit(uint8_t bmp_num, uint8_t position, uint16_t color)
{
  // Each digit uses a fixed starting column on the 32x8 matrix.
  uint8_t pos = 0;
  switch (position)
  {
  case 0:
    pos = 0;
    break; // set to 0 when position is 0
  case 1:
    pos = 7;
    break; // set to 7 when position is 1
  case 2:
    pos = 16;
    break; // set to 16 when position is 2
  case 3:
    pos = 23;
    break; // set to 23 when position is 3
  default:
    return;
  }
  // draw the bitmap using column and color parameters
  matrix->drawBitmap((runtimeState.clockDisplayOffset) ? (pos - 4) : pos, 0, num[bmp_num], 8, 8, color);
}

void display_showStatus()
{
  uint16_t sclr = BLACK;
  uint16_t wclr = BLACK;
  uint16_t aclr = BLACK;
  uint16_t uclr = BLACK;
  uint16_t gclr = BLACK;
  if (green_status.isChecked())
    gclr = DARKGREEN;
  else
    gclr = BLACK;
  acetime_t now = systemClock.getNow();
  // Set the status color
  if (now - systemClock.getLastSyncTime() > (NTPCHECKTIME * 60) + 60)
    sclr = DARKRED;
  else if (runtimeState.timeSource[0] == 'g' && gps.fix && (now - systemClock.getLastSyncTime()) <= 600)
    sclr = BLACK;
  else if (runtimeState.timeSource[0] == 'n' && iotWebConf.getState() == 4)
    sclr = DARKGREEN;
  else if (iotWebConf.getState() <= 2) // boot, apmode, notconfigured
    sclr = DARKMAGENTA;
  else if (iotWebConf.getState() == 3 || iotWebConf.getState() == 4) // connecting, online
    sclr = DARKCYAN;
  else
    sclr = DARKRED;
  if (enable_system_status.isChecked())
    matrix->drawPixel(0, 7, sclr);
  // Check the AQI and wether an alert is present
  if (enable_aqi_status.isChecked() && checkaqi.complete)
  {
    switch (aqi.current.aqi)
    {
    case 2:
      aclr = DARKYELLOW;
      break;
    case 3:
      aclr = DARKORANGE;
      break;
    case 4:
      aclr = DARKRED;
      break;
    case 5:
      aclr = DARKPURPLE;
      break;
    default:
      aclr = gclr;
      break;
    }
    matrix->drawPixel(0, 0, aclr);
  }
  if (enable_uvi_status.isChecked() && checkweather.complete)
  {
    switch (weather.current.uvi)
    {
    case 0:
      uclr = gclr;
      break;
    case 1:
      uclr = gclr;
      break;
    case 2:
      uclr = DARKYELLOW;
      break;
    case 3:
      uclr = DARKORANGE;
      break;
    case 4:
      uclr = DARKRED;
      break;
    case 5:
      uclr = DARKPURPLE;
      break;
    default:
      uclr = gclr;
      break;
    }
    matrix->drawPixel(mw - 1, 0, uclr);
  }
  if (alerts.inWarning)
    wclr = RED;
  else if (alerts.inWatch)
    wclr = YELLOW;
  else
    wclr = BLACK;
  if (wclr != BLACK)
  {
    matrix->drawPixel(0, 3, wclr);
    matrix->drawPixel(0, 4, wclr);
    matrix->drawPixel(mw - 1, 3, wclr);
    matrix->drawPixel(mw - 1, 4, wclr);
  }
  // If colors are different from before show matrix again
  if (current.oldaqiclr != aclr || current.oldstatusclr != sclr || current.oldstatuswclr != wclr || current.olduvicolor != uclr)
  {
    matrix->show();
    current.oldstatusclr = sclr;
    current.oldstatuswclr = wclr;
    current.oldaqiclr = aclr;
    current.olduvicolor = uclr;
  }
}

void display_weatherIcon(char *icon)
{
  bool night;
  char dn = icon[2];
  night = (dn == 'n' ? true : false);
  char code1 = icon[0];
  char code2 = icon[1];
  // Clear
  if (code1 == '0' && code2 == '1')
  {
    matrix->drawRGBBitmap(mw - 8, 0, (night ? clear_night[cotimer.iconcycle] : clear_day[cotimer.iconcycle]), 8, 8);
  }
  // Partly cloudy
  else if ((code1 == '0' && code2 == '2') || (code1 == '0' && code2 == '3'))
  {
    matrix->drawRGBBitmap(mw - 8, 0, (night ? partly_cloudy_night[cotimer.iconcycle] : partly_cloudy_day[cotimer.iconcycle]), 8, 8);
  }
  // Cloudy
  else if ((code1 == '0') && (code2 == '4'))
  {
    matrix->drawRGBBitmap(mw - 8, 0, cloudy[cotimer.iconcycle], 8, 8);
  }
  // Fog/mist
  else if (code1 == '5' && code2 == '0')
  {
    matrix->drawRGBBitmap(mw - 8, 0, fog[cotimer.iconcycle], 8, 8);
  }
  // Snow
  else if (code1 == '1' && code2 == '3')
  {
    matrix->drawRGBBitmap(mw - 8, 0, snow[cotimer.iconcycle], 8, 8);
  }
  // Rain
  else if (code1 == '0' && code2 == '9')
  {
    matrix->drawRGBBitmap(mw - 8, 0, rain[cotimer.iconcycle], 8, 8);
  }
  // Heavy rain
  else if (code1 == '1' && code2 == '0')
  {
    matrix->drawRGBBitmap(mw - 8, 0, heavy_rain[cotimer.iconcycle], 8, 8);
  }
  // Thunderstorm
  else if (code1 == '1' && code2 == '1')
  {
    matrix->drawRGBBitmap(mw - 8, 0, thunderstorm[cotimer.iconcycle], 8, 8);
  }
  else
  {
    ESP_LOGE(TAG, "No Weather icon found to use");
    return;
  }
  // Animate
  if (millis() - cotimer.icontimer > ANI_SPEED)
  {
    cotimer.iconcycle = (cotimer.iconcycle == ANI_BITMAP_CYCLES - 1) ? 0 : cotimer.iconcycle + 1;
    cotimer.icontimer = millis();
  }
}

void display_temperature()
{
  int temp = weather.current.feelsLike;
  bool imperial_setting = imperial.isChecked();
  int tl = (imperial_setting) ? 32 : 0;
  int th = (imperial_setting) ? 104 : 40;
  uint32_t tc;
  if (enable_temp_color.isChecked())
  {
    tc = hex2rgb(temp_color.value());
  }
  else
  {
    current.temphue = constrain(map(weather.current.feelsLike, tl, th, NIGHTHUE, 0), 0, NIGHTHUE);

    if (weather.current.feelsLike < tl)
    {
      if (imperial_setting && weather.current.feelsLike == 0)
        current.temphue = NIGHTHUE + tl;
      else
        current.temphue = NIGHTHUE + (abs(weather.current.feelsLike) / 2);
    }
    tc = hsv2rgb(current.temphue);
  }

  int xpos = 0;
  bool isneg = false;
  int score = temp < 0 ? -temp : temp;
  int digits = 1;
  for (int value = score; value >= 10; value /= 10)
    ++digits;
  if (temp < 0)
  {
    isneg = true;
    digits++;
  }
  if (digits == 3)
    xpos = 1;
  else if (digits == 2)
    xpos = 5;
  else if (digits == 1)
    xpos = 9;
  xpos = xpos * (digits);
  if (score == 0)
  {
    matrix->drawBitmap(xpos, 0, num[0], 8, 8, tc);
  }
  while (score)
  {
    matrix->drawBitmap(xpos, 0, num[score % 10], 8, 8, tc);
    score /= 10;
    xpos = xpos - 7;
  }
  if (isneg == true)
  {
    matrix->drawBitmap(xpos, 0, sym[1], 8, 8, tc);
    xpos = xpos - 7;
  }
  matrix->drawBitmap(xpos + (digits * 7) + 7, 0, sym[0], 8, 8, tc);
}

void printSystemZonedTime()
{
  ConsoleMirrorPrint out(isConsoleSerialMirrorEnabled());
  getSystemZonedTime().printTo(out);
  out.println();
}

ZonedDateTime getSystemZonedTime()
{
  acetime_t now = systemClock.getNow();
  refreshCachedTimezoneOffset(now);
  return ZonedDateTime::forEpochSeconds(now, current.timezone);
}

String getSystemTimezoneOffsetString()
{
  return formatTimeOffset(getTimezoneOffsetAt(systemClock.getNow()));
}

String getSystemTimezoneName()
{
  if (enable_fixed_tz.isChecked())
    return String(F("Fixed GMT")) + getSystemTimezoneOffsetString();

  if (hasTimezoneName(ipgeo.timezone))
    return String(ipgeo.timezone);

  if (hasTimezoneName(savedtimezone.value()))
    return String(savedtimezone.value());

  return String(F("Fixed GMT")) + getSystemTimezoneOffsetString();
}

bool isSystemTimezoneDstActive()
{
  return !current.timezone.getZonedExtra(systemClock.getNow()).dstOffset().isZero();
}

String getSystemZonedTimestamp()
{
  acetime_t now = systemClock.getNow();
  ZonedDateTime TimeWZ = ZonedDateTime::forEpochSeconds(now, current.timezone);
  char str[64];
  // Create string with year, month, day, hour, minute, second and time offset values
  snprintf(str, sizeof(str), "%02d/%02d/%02d %02d:%02d:%02d [GMT%s]", TimeWZ.month(), TimeWZ.day(), TimeWZ.year(),
           TimeWZ.hour(), TimeWZ.minute(), TimeWZ.second(), getSystemTimezoneOffsetString().c_str());
  return (String)str;
}

String getCustomZonedTimestamp(acetime_t now)
{
  ZonedDateTime TimeWZ = ZonedDateTime::forEpochSeconds(now, current.timezone);
  String offsetString = formatTimeOffset(TimeWZ.timeOffset());
  char str[64];
  // Create string with year, month, day, hour, minute, second and time offset values
  snprintf(str, sizeof(str), "%02d/%02d/%02d %02d:%02d:%02d [GMT%s]", TimeWZ.month(), TimeWZ.day(), TimeWZ.year(),
           TimeWZ.hour(), TimeWZ.minute(), TimeWZ.second(), offsetString.c_str());
  return (String)str;
}

void getSystemZonedDateString(char *str, size_t length)
{
  if (str == nullptr || length == 0)
    return;

  // Get zoned date time object
  ZonedDateTime ldt = getSystemZonedTime();
  // Format date string
  snprintf(str, length, "%s, %s %d%s %d",
          DateStrings().dayOfWeekLongString(ldt.dayOfWeek()),
          DateStrings().monthLongString(ldt.month()),
          ldt.day(), ordinal_suffix(ldt.day()), ldt.year());
}

String getSystemZonedDateTimeString()
{
  ZonedDateTime ldt = getSystemZonedTime();
  char buf[100];

  if (twelve_clock.isChecked())
  {
    const char *ap = ldt.hour() > 11 ? "PM" : "AM";
    uint8_t hour = ldt.hour() % 12;
    if (hour == 0)
      hour = 12;

    snprintf(buf, sizeof(buf), "%d:%02d%s %s, %s %u%s %04d", hour, ldt.minute(), ap,
             DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()),
             ldt.day(), ordinal_suffix(ldt.day()), ldt.year());
  }
  else
  {
    snprintf(buf, sizeof(buf), "%02d:%02d %s, %s %d%s %04d", ldt.hour(), ldt.minute(),
             DateStrings().dayOfWeekLongString(ldt.dayOfWeek()), DateStrings().monthLongString(ldt.month()),
             ldt.day(), ordinal_suffix(ldt.day()), ldt.year());
  }

  return String(buf);
}

uint16_t calcaqi(double c, double i_hi, double i_low, double c_hi, double c_low)
{
  return (i_hi - i_low) / (c_hi - c_low) * (c - c_low) + i_low;
}

// TODO: web interface cleanup
// TODO: advanced aqi calulations
// TODO: table titles
// TODO: remove tables if show is disabled
// TODO: weather daily in web
// TODO: timezone name in web
// TODO: full/new moon scroll and status icon?
// TODO: make very severe weather alerts (like tornado warnings) scroll continuosly
// FIXME: location change is adding scrolltext and color, needs to be only for changes

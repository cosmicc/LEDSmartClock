#include "main.h"
#include <esp_sntp.h>
#include <cstring>
#include <lwip/ip_addr.h>
#include <time.h>

namespace ace_time
{
namespace clock
{
namespace
{
/** Small fixed-size record for one NTP server candidate in the retry order. */
struct NtpServerCandidate
{
  char server[64]{};
  char source[32]{};
  int8_t slot = -1;
};

/** Monotonic count of completed SNTP sync callbacks since boot. */
volatile uint32_t sSntpSyncCount = 0;

/** Unix timestamp captured by the latest completed SNTP sync callback. */
volatile time_t sLastSntpUnixTime = 0;

/** Copies a bounded C string into runtime state without overrunning the buffer. */
void setRuntimeNtpValue(char *dest, size_t capacity, const char *value)
{
  snprintf(dest, capacity, "%s", value == nullptr ? "" : value);
}

/** Formats an SNTP server address into a printable buffer when one is set. */
bool formatSntpAddress(const ip_addr_t *serverAddress, char *address, size_t capacity)
{
  if (serverAddress == nullptr || ip_addr_isany(serverAddress))
    return false;
  return ipaddr_ntoa_r(serverAddress, address, capacity) != nullptr;
}

/** Dumps the raw SNTP server slots so DHCP-provided NTP state is visible in logs. */
void logSntpServerSlots(const char *context)
{
  char address[IPADDR_STRLEN_MAX];
  for (uint8_t index = 0; index < SNTP_MAX_SERVERS; ++index)
  {
    const char *serverName = esp_sntp_getservername(index);
    const ip_addr_t *serverAddress = esp_sntp_getserver(index);
    const bool hasAddress = formatSntpAddress(serverAddress, address, sizeof(address));
    ESP_LOGD(TAG, "GPSClock: %s slot[%u] name=%s addr=%s", context, index,
             (serverName != nullptr && serverName[0] != '\0') ? serverName : "(none)",
             hasAddress ? address : "(none)");
  }
}

/** Logs the active runtime NTP selection whenever the effective server changes. */
void logActiveNtpSelection()
{
  static char lastServer[sizeof(runtimeState.ntpServer)] = "";
  static char lastSource[sizeof(runtimeState.ntpServerSource)] = "";

  if (strcmp(lastServer, runtimeState.ntpServer) == 0 && strcmp(lastSource, runtimeState.ntpServerSource) == 0)
    return;

  setRuntimeNtpValue(lastServer, sizeof(lastServer), runtimeState.ntpServer);
  setRuntimeNtpValue(lastSource, sizeof(lastSource), runtimeState.ntpServerSource);
  ESP_LOGI(TAG, "GPSClock: active NTP server is %s (%s)", runtimeState.ntpServer, runtimeState.ntpServerSource);
}

/** Returns the preferred manual NTP server or the built-in default fallback. */
const char *preferredNtpServer()
{
  return ntp_server.value()[0] != '\0' ? ntp_server.value() : DEFAULT_NTP_SERVER;
}

/** Returns the configured SNTP sync interval in milliseconds. */
uint32_t getSntpSyncIntervalMillis()
{
  const uint32_t intervalMillis = static_cast<uint32_t>(NTPCHECKTIME) * 60UL * 1000UL;
  return intervalMillis < 15000UL ? 15000UL : intervalMillis;
}

/** Returns the label used when no DHCP NTP server is currently in use. */
const char *preferredNtpSourceLabel()
{
  return ntp_server.value()[0] != '\0'
             ? (override_dhcp_ntp.isChecked() ? "Manual override" : "Configured fallback")
             : "Default fallback";
}

/** Copies the selected NTP candidate into runtime state for logging and UI display. */
void applyRuntimeNtpSelection(const NtpServerCandidate &candidate)
{
  setRuntimeNtpValue(runtimeState.ntpServer, sizeof(runtimeState.ntpServer), candidate.server);
  setRuntimeNtpValue(runtimeState.ntpServerSource, sizeof(runtimeState.ntpServerSource), candidate.source);
}

/** Collects all DHCP-provided NTP servers in slot order. */
uint8_t collectDhcpNtpServers(NtpServerCandidate *candidates, uint8_t capacity)
{
#if LWIP_DHCP_GET_NTP_SRV
  char address[IPADDR_STRLEN_MAX];
  uint8_t count = 0;
  for (uint8_t index = 0; index < SNTP_MAX_SERVERS; ++index)
  {
    const char *serverName = esp_sntp_getservername(index);
    const ip_addr_t *serverAddress = esp_sntp_getserver(index);
    if ((serverName == nullptr || serverName[0] == '\0') && formatSntpAddress(serverAddress, address, sizeof(address)))
    {
      ESP_LOGD(TAG, "GPSClock: detected DHCP NTP server in slot[%u]: %s", index, address);
      if (count < capacity)
      {
        setRuntimeNtpValue(candidates[count].server, sizeof(candidates[count].server), address);
        snprintf(candidates[count].source, sizeof(candidates[count].source), "DHCP slot %u", index + 1);
        candidates[count].slot = index;
        ++count;
      }
    }
  }
  if (count == 0)
    ESP_LOGD(TAG, "GPSClock: no DHCP-provided NTP server currently available.");
  else
    ESP_LOGD(TAG, "GPSClock: %u DHCP NTP server(s) available for failover.", count);
  return count;
#else
  ESP_LOGD(TAG, "GPSClock: DHCP NTP support is not enabled in this build.");
#endif
  return 0;
}

/** Builds the ordered list of NTP servers to try for the next sync attempt. */
uint8_t buildNtpServerCandidates(NtpServerCandidate *candidates, uint8_t capacity)
{
  uint8_t count = 0;
  if (!override_dhcp_ntp.isChecked())
    count = collectDhcpNtpServers(candidates, capacity);

  if (count > 0)
    return count;

  if (capacity == 0)
    return 0;

  setRuntimeNtpValue(candidates[0].server, sizeof(candidates[0].server), preferredNtpServer());
  setRuntimeNtpValue(candidates[0].source, sizeof(candidates[0].source), preferredNtpSourceLabel());
  candidates[0].slot = -1;
  return 1;
}

/** Updates the effective NTP server labels shown in the dashboard and config UI. */
void updateRuntimeNtpSelection()
{
  NtpServerCandidate candidates[SNTP_MAX_SERVERS + 1];
  const uint8_t count = buildNtpServerCandidates(candidates, SNTP_MAX_SERVERS + 1);
  if (count > 0)
  {
    applyRuntimeNtpSelection(candidates[0]);
    return;
  }

  setRuntimeNtpValue(runtimeState.ntpServer, sizeof(runtimeState.ntpServer), preferredNtpServer());
  setRuntimeNtpValue(runtimeState.ntpServerSource, sizeof(runtimeState.ntpServerSource), preferredNtpSourceLabel());
}

/** Applies the current DHCP/manual SNTP selection policy before or after Wi-Fi connects. */
void configureSntpServerSelection()
{
#if LWIP_DHCP_GET_NTP_SRV
  esp_sntp_servermode_dhcp(!override_dhcp_ntp.isChecked());
  ESP_LOGD(TAG, "GPSClock: DHCP NTP server mode is %s.", override_dhcp_ntp.isChecked() ? "disabled" : "enabled");
#endif
  if (override_dhcp_ntp.isChecked())
  {
    esp_sntp_setservername(0, preferredNtpServer());
    ESP_LOGD(TAG, "GPSClock: forcing preferred NTP server: %s", preferredNtpServer());
  }
  else
  {
    esp_sntp_setservername(0, nullptr);
    ESP_LOGD(TAG, "GPSClock: cleared manual SNTP hostname so DHCP NTP can populate the active slot.");
  }
}

/** Applies runtime SNTP settings that are independent of the selected server list. */
void configureSntpRuntime()
{
  esp_sntp_set_time_sync_notification_cb([](struct timeval *tv) {
    sLastSntpUnixTime = (tv == nullptr) ? 0 : tv->tv_sec;
    ++sSntpSyncCount;
    ESP_LOGI(TAG, "GPSClock: SNTP sync callback received time %ld using %s (%s).",
             static_cast<long>(sLastSntpUnixTime), runtimeState.ntpServer, runtimeState.ntpServerSource);
  });
  esp_sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
  esp_sntp_set_sync_interval(getSntpSyncIntervalMillis());
}

/** Returns true when the libc system clock currently holds a real SNTP time. */
bool hasUsableSystemTime()
{
  return time(nullptr) >= 1451606400;
}

/** Returns the current libc system clock converted into the AceTime epoch. */
acetime_t getSystemSntpTime()
{
  if (!hasUsableSystemTime())
    return Clock::kInvalidSeconds;

  return convert1970Epoch(static_cast<acetime_t>(time(nullptr)));
}

/** Returns true when a fresh SNTP callback has completed after the request started. */
bool hasFreshSntpSync(uint32_t baselineCount)
{
  return sSntpSyncCount != baselineCount;
}

/** Returns true when the currently displayed time is coming from the DS3231 RTC. */
bool isRtcBackedTimeActive()
{
  return strcmp(runtimeState.timeSource, "rtc") == 0;
}

/** Returns true when the currently displayed time is backed by the libc SNTP clock. */
bool isCachedNtpTimeActive()
{
  return strcmp(runtimeState.timeSource, "ntp") == 0 && hasUsableSystemTime();
}
} // namespace

bool GPSClock::isSetup() const
{
  return ntpIsReady;
}

void GPSClock::setup()
{
  setupNTP();
}

void GPSClock::ntpReady()
{
  setupNTP();
  refreshNtpServer();
  noteDiagnosticPending(DiagnosticService::Ntp, true, "Ready",
                        String(F("SNTP transport is ready. Selected server: ")) + runtimeState.ntpServer +
                            F(" (") + runtimeState.ntpServerSource + F(")."));
}

void GPSClock::prepareServerSelection()
{
  ESP_LOGD(TAG, "GPSClock: prepareServerSelection(): configuring SNTP selection before Wi-Fi connect.");
  configureSntpServerSelection();
  noteDiagnosticPending(DiagnosticService::Ntp, true, "Preparing",
                        String(F("Preparing NTP server selection using ")) + preferredNtpServer() + F("."));
}

void GPSClock::refreshNtpServer()
{
  ESP_LOGD(TAG, "GPSClock: refreshNtpServer(): preferred=%s overrideDhcp=%s sntpEnabled=%s",
           preferredNtpServer(), override_dhcp_ntp.isChecked() ? "true" : "false",
           esp_sntp_enabled() ? "true" : "false");
  logSntpServerSlots("before refresh");
  updateRuntimeNtpSelection();

  if (iotWebConf.getState() != 4)
  {
    noteDiagnosticPending(DiagnosticService::Ntp, true, "Waiting for Wi-Fi",
                          String(F("Selected server is ")) + runtimeState.ntpServer + F(" (") +
                              runtimeState.ntpServerSource + F("), but Wi-Fi is not online yet."));
    logActiveNtpSelection();
    return;
  }

  const bool sntpWasEnabled = esp_sntp_enabled();
  if (!sntpWasEnabled)
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
  configureSntpRuntime();
  configureSntpServerSelection();

  if (!sntpWasEnabled)
    esp_sntp_init();

  logSntpServerSlots("after refresh");
  updateRuntimeNtpSelection();
  logActiveNtpSelection();
}

void GPSClock::setupNTP()
{
  if (!ntpIsReady && iotWebConf.getState() == 4)
  {
    configureSntpRuntime();
    ESP_LOGD(TAG, "GPSClock: SNTP transport ready");
    noteDiagnosticPending(DiagnosticService::Ntp, true, "Transport ready",
                          String(F("SNTP transport is ready. Selected server: ")) + runtimeState.ntpServer +
                              F(" (") + runtimeState.ntpServerSource + F(")."));
    ntpIsReady = true;
  }
}

acetime_t GPSClock::getNow() const
{
  if (GPS.time.isUpdated())
    return readResponse();

  sendRequest();
  if (isResponseReady())
    return readResponse();

  if (!mWaitingForNtpSync)
    return kInvalidSeconds;

  const uint32_t waitStart = millis();
  while ((millis() - waitStart) < mRequestTimeout)
  {
    while (Serial1.available())
      GPS.encode(Serial1.read());
    if (isResponseReady())
      return readResponse();
    delay(1);
  }

  const_cast<GPSClock *>(this)->mWaitingForNtpSync = false;
  if (isRtcBackedTimeActive() || isCachedNtpTimeActive())
    return readResponse();

  return kInvalidSeconds;
}

void GPSClock::sendRequest() const
{
  while (Serial1.available())
    GPS.encode(Serial1.read());
  if (GPS.time.isUpdated() || !ntpIsReady || iotWebConf.getState() != 4)
  {
    return;
  }

  if (mWaitingForNtpSync)
  {
    return;
  }

  if (abs(Now() - runtimeState.lastNtpCheck) < NTPCHECKTIME * 60)
  {
    return;
  }

  const_cast<GPSClock *>(this)->refreshNtpServer();
  NtpServerCandidate candidates[SNTP_MAX_SERVERS + 1];
  const uint8_t candidateCount = buildNtpServerCandidates(candidates, SNTP_MAX_SERVERS + 1);
  if (candidateCount == 0)
  {
    ESP_LOGE(TAG, "GPSClock: sendRequest(): no NTP servers are available to query.");
    noteDiagnosticFailure(DiagnosticService::Ntp, true, "No servers",
                          F("No NTP servers are available from DHCP or the configured fallback."),
                          0, 0);
    return;
  }

  applyRuntimeNtpSelection(candidates[0]);
  logActiveNtpSelection();
  const_cast<GPSClock *>(this)->mPendingSntpSyncCount = sSntpSyncCount;
  const_cast<GPSClock *>(this)->mRequestStartMillis = millis();
  const_cast<GPSClock *>(this)->mWaitingForNtpSync = true;
  ESP_LOGD(TAG, "GPSClock: sendRequest(): requesting fresh SNTP sync from %s (%s)%s",
           runtimeState.ntpServer, runtimeState.ntpServerSource,
           candidates[0].slot >= 0 ? " [DHCP]" : "");
  noteDiagnosticPending(DiagnosticService::Ntp, true, "Syncing",
                        String(F("Requesting fresh SNTP sync from ")) + runtimeState.ntpServer +
                            F(" (") + runtimeState.ntpServerSource + F(")."));

  if (!esp_sntp_restart())
  {
    ESP_LOGE(TAG, "GPSClock: sendRequest(): failed to restart SNTP for %s (%s).",
             runtimeState.ntpServer, runtimeState.ntpServerSource);
    const_cast<GPSClock *>(this)->mWaitingForNtpSync = false;
    noteDiagnosticFailure(DiagnosticService::Ntp, true, "Restart failed",
                          String(F("Failed to restart SNTP for ")) + runtimeState.ntpServer +
                              F(" (") + runtimeState.ntpServerSource + F(")."),
                          0, 0);
  }
}

bool GPSClock::isResponseReady() const
{
  if (GPS.time.isUpdated())
    return true;

  if (mWaitingForNtpSync)
  {
    if (hasFreshSntpSync(mPendingSntpSyncCount))
      return true;

    if ((millis() - mRequestStartMillis) >= mRequestTimeout)
    {
      ESP_LOGW(TAG, "GPSClock: isResponseReady(): timed out waiting for fresh SNTP sync from %s (%s).",
               runtimeState.ntpServer, runtimeState.ntpServerSource);
      const_cast<GPSClock *>(this)->mWaitingForNtpSync = false;
      noteDiagnosticFailure(DiagnosticService::Ntp, true, "Timed out",
                            String(F("Timed out waiting for SNTP sync from ")) + runtimeState.ntpServer +
                                F(" (") + runtimeState.ntpServerSource + F(")."),
                            -11, 0);
    }
    return false;
  }

  return isCachedNtpTimeActive() || isRtcBackedTimeActive();
}

acetime_t GPSClock::readResponse() const
{
  if (GPS.time.isUpdated())
  {
    auto localDateTime = LocalDateTime::forComponents(GPS.date.year(), GPS.date.month(), GPS.date.day(), GPS.time.hour(), GPS.time.minute(), GPS.time.second());
    acetime_t gpsSeconds = localDateTime.toEpochSeconds();
    if (GPS.time.age() < 100 && GPS.satellites.value() != 0)
    {
      setTimeSource("gps");
      resetLastNtpCheck(gpsSeconds);
      ESP_LOGI(TAG, "GPSClock: readResponse(): gpsSeconds: %d | skew: %dsec | age: %dms", gpsSeconds, abs(Now()) - abs(gpsSeconds), GPS.time.age());
      return gpsSeconds;
    }
    else
    {
      ESP_LOGW(TAG, "GPSClock: readResponse(): Stale GPS time skipped: gpsSeconds: %d | skew: %dsec | age: %dms", gpsSeconds, abs(Now()) - abs(gpsSeconds), GPS.time.age());
      return kInvalidSeconds;
    }
  }

  const bool freshSntpSync = mWaitingForNtpSync && hasFreshSntpSync(mPendingSntpSyncCount);
  if (freshSntpSync || isCachedNtpTimeActive())
  {
    acetime_t epochSeconds = getSystemSntpTime();
    const_cast<GPSClock *>(this)->mWaitingForNtpSync = false;
    if (epochSeconds == kInvalidSeconds)
    {
      ESP_LOGE(TAG, "GPSClock: readResponse(): SNTP callback fired but the system clock is still invalid.");
      noteDiagnosticFailure(DiagnosticService::Ntp, true, "Invalid time",
                            F("SNTP reported success, but the libc system clock still looks invalid."),
                            0, 0);
      return kInvalidSeconds;
    }

    setTimeSource("ntp");
    if (freshSntpSync)
    {
      resetLastNtpCheck(epochSeconds);
      ESP_LOGI(TAG, "GPSClock: readResponse(): fresh SNTP sync unixSeconds: %ld | epochSeconds: %d | skew: %dsec",
               static_cast<long>(sLastSntpUnixTime), epochSeconds, abs(Now()) - abs(epochSeconds));
      noteDiagnosticSuccess(DiagnosticService::Ntp, true, "Fresh sync",
                            String(runtimeState.ntpServer) + F(" (") + runtimeState.ntpServerSource + F(")"),
                            0, 0);
    }
    else
    {
      ESP_LOGD(TAG, "GPSClock: readResponse(): using cached SNTP-backed system time: %d", epochSeconds);
      noteDiagnosticPending(DiagnosticService::Ntp, true, "Using cached sync",
                            String(runtimeState.ntpServer) + F(" (") + runtimeState.ntpServerSource + F(")"));
    }
    return epochSeconds;
  }

  if (isRtcBackedTimeActive())
  {
    acetime_t rtcSeconds = dsClock.getNow();
    if (rtcSeconds != kInvalidSeconds)
    {
      setTimeSource("rtc");
      noteDiagnosticPending(DiagnosticService::Ntp, true, "RTC fallback",
                            F("No fresh NTP sync is active. The clock is currently using the RTC chip."));
      return rtcSeconds;
    }
  }

  setTimeSource("none");
  noteDiagnosticFailure(DiagnosticService::Ntp, true, "No valid time",
                        F("Neither GPS, NTP, nor the RTC currently has a valid time source."),
                        0, 0);
  return kInvalidSeconds;
}
} // namespace clock
} // namespace ace_time

GPSClock gpsClock;
SystemClockLoop systemClock(&gpsClock, &dsClock, 60, 5, 5000);

#pragma once

#include <AceTime.h>
#include <HTTPClient.h>
#include <cstddef>
#include <cstdint>

/**
 * Enumerates the external APIs used by the firmware.
 *
 * Replacing raw integer indices with a scoped enum makes call sites easier to
 * read and removes the risk of mixing up URLs accidentally.
 */
enum class ApiEndpoint : uint8_t
{
  Weather,
  Alerts,
  Geocode,
  AirQuality,
  IpGeo,
  Reserved,
};

/**
 * Stores the concrete URL for each remote API endpoint.
 *
 * The URLs are rebuilt whenever configuration or coordinates change.
 */
struct EndpointUrls
{
  /** Cached OpenWeather One Call endpoint for current and daily weather data. */
  char weather[256]{};
  /** Cached weather.gov endpoint for active alerts at the current coordinates. */
  char alerts[256]{};
  /** Cached OpenWeather reverse-geocode endpoint for city/state resolution. */
  char geocode[256]{};
  /** Cached OpenWeather air-quality endpoint for AQI and pollutant data. */
  char airQuality[256]{};
  /** Cached ipgeolocation.io endpoint used for timezone and coarse fallback coordinates. */
  char ipGeo[256]{};
  /** Reserved URL slot kept for future API expansion without reshaping the struct. */
  char reserved[256]{};
};

/**
 * Owns the shared HTTP client and the currently built endpoint URLs.
 */
struct NetworkService
{
  /** Shared HTTP client reused across all remote API requests. */
  HTTPClient client;
  /** Last rebuilt set of concrete endpoint URLs derived from config and coordinates. */
  EndpointUrls urls;
  /** True while an in-flight request currently owns the shared HTTP client. */
  bool busy = false;
};

/**
 * Enumerates the runtime subsystems surfaced on the diagnostics page.
 */
enum class DiagnosticService : uint8_t
{
  Wifi,
  Timekeeping,
  Ntp,
  Gps,
  Weather,
  AirQuality,
  Alerts,
  Geocode,
  IpGeo,
  Count,
};

/** Number of diagnostics records carried inside runtime state. */
constexpr size_t kDiagnosticServiceCount = static_cast<size_t>(DiagnosticService::Count);

/**
 * Stores the latest health snapshot for one subsystem.
 *
 * The web diagnostics page uses these records to show the current summary,
 * last success, last failure, retry counters, and the most recent error code
 * without having to inspect each service implementation directly.
 */
struct ServiceDiagnostic
{
  /** False when the subsystem is intentionally disabled by config or setup mode. */
  bool enabled = true;
  /** True when the subsystem is currently healthy enough to serve live data. */
  bool healthy = false;
  /** Most recent retry counter for the subsystem, when applicable. */
  uint8_t retries = 0;
  /** Latest HTTP or transport code associated with the current summary. */
  int16_t lastCode = 0;
  /** Time when this subsystem last completed a successful update. */
  ace_time::acetime_t lastSuccess = 0;
  /** Time when this subsystem last recorded a failure or hard warning. */
  ace_time::acetime_t lastFailure = 0;
  /** Short status text such as Connected, Waiting, Fresh Data, or Timed Out. */
  char summary[32] = "Pending";
  /** Longer human-readable context shown on the diagnostics page. */
  char detail[160] = "Waiting for runtime data.";
};

/**
 * Collects the cross-cutting runtime values that were previously individual
 * primitive globals.
 */
struct RuntimeState
{
  /** Timestamp of the last NTP check scheduling decision. */
  ace_time::acetime_t lastNtpCheck = 0;
  /** Hostname or IP address of the NTP server currently selected for sync. */
  char ntpServer[64] = DEFAULT_NTP_SERVER;
  /** Short label describing where the active NTP server came from, such as DHCP slot 2. */
  char ntpServerSource[32] = "Default fallback";
  /** Shifts the clock left when temperature or status pixels need the extra columns. */
  bool clockDisplayOffset = false;
  /** Timestamp captured during boot and reused for uptime reporting. */
  ace_time::acetime_t bootTime = 0;
  /** Short tag describing the active time source, such as gps, ntp, rtc, or none. */
  char timeSource[8] = "none";
  /** User-selected brightness bias added on top of ambient-light control. */
  uint8_t userBrightness = 0;
  /** Prevents early network jobs from running while the first-time portal is active. */
  bool firstTimeFailsafe = false;
  /** Human-readable label describing how the active timezone was selected. */
  char timezoneSource[32] = "Configured fallback";
  /** Epoch time for the currently selected sunrise event source. */
  ace_time::acetime_t activeSunriseEpoch = 0;
  /** Epoch time for the currently selected sunset event source. */
  ace_time::acetime_t activeSunsetEpoch = 0;
  /** Human-readable label describing where sunrise/sunset values came from. */
  char solarTimesSource[24] = "Unavailable";
  /** Previous epoch sample used for sunrise/sunset edge detection. */
  ace_time::acetime_t lastSolarCheck = 0;
  /** Sunrise epoch already announced, used to prevent duplicate messages. */
  ace_time::acetime_t sunriseNotifiedEpoch = 0;
  /** Sunset epoch already announced, used to prevent duplicate messages. */
  ace_time::acetime_t sunsetNotifiedEpoch = 0;
  /** Set when the web UI or serial console has requested a controlled reboot. */
  bool rebootRequested = false;
  /** Millisecond timestamp used to delay restart until the HTTP response is sent. */
  uint32_t rebootRequestMillis = 0;
  /** Latest diagnostics snapshot for each major subsystem shown in the web UI. */
  ServiceDiagnostic diagnostics[kDiagnosticServiceCount]{};
};

/** Shared runtime timestamps and flags that coordinate major subsystems. */
extern RuntimeState runtimeState;
/** Shared HTTP client and cached API endpoint URLs. */
extern NetworkService networkService;

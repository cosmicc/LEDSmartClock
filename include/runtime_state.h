#pragma once

#include <AceTime.h>
#include <HTTPClient.h>
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
 * Collects the cross-cutting runtime values that were previously individual
 * primitive globals.
 */
struct RuntimeState
{
  /** Timestamp of the last NTP check scheduling decision. */
  ace_time::acetime_t lastNtpCheck = 0;
  /** Shifts the clock left when temperature or status pixels need the extra columns. */
  bool clockDisplayOffset = false;
  /** Timestamp captured during boot and reused for uptime reporting. */
  ace_time::acetime_t bootTime = 0;
  /** Short tag describing the active time source, such as gps or ntp. */
  char timeSource[4] = "n/a";
  /** User-selected brightness bias added on top of ambient-light control. */
  uint8_t userBrightness = 0;
  /** Prevents early network jobs from running while the first-time portal is active. */
  bool firstTimeFailsafe = false;
  /** Set when the web UI or serial console has requested a controlled reboot. */
  bool rebootRequested = false;
  /** Millisecond timestamp used to delay restart until the HTTP response is sent. */
  uint32_t rebootRequestMillis = 0;
};

/** Shared runtime timestamps and flags that coordinate major subsystems. */
extern RuntimeState runtimeState;
/** Shared HTTP client and cached API endpoint URLs. */
extern NetworkService networkService;

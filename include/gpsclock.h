#pragma once

namespace ace_time
{
namespace clock
{
/**
 * Provides time from either the GPS receiver or an NTP request routed through
 * the Wi-Fi connection.
 *
 * The class is intentionally stateful because it owns the UDP socket used for
 * NTP polling and tracks when that transport becomes available.
 */
class GPSClock : public Clock
{
private:
  /** Maximum time to wait for a fresh SNTP sync event during a forced sync. */
  uint16_t const mRequestTimeout = 5000;
  /** SNTP sync generation visible at the start of the active request. */
  mutable uint32_t mPendingSntpSyncCount = 0;
  /** Millisecond timestamp when the current SNTP request was started. */
  mutable uint32_t mRequestStartMillis = 0;
  /** True while the clock is waiting for a fresh SNTP sync callback. */
  mutable bool mWaitingForNtpSync = false;

public:
  mutable bool ntpIsReady = false;
  static const acetime_t kInvalidSeconds = LocalTime::kInvalidSeconds;
  static const uint16_t kConnectTimeoutMillis = 10000;

  /** Returns true once the NTP transport has been prepared. */
  bool isSetup() const;
  /** Prepares the NTP transport during startup. */
  void setup();
  /** Re-enables the NTP transport after Wi-Fi comes online. */
  void ntpReady();
  /** Configures SNTP server selection before Wi-Fi starts DHCP negotiation. */
  void prepareServerSelection();
  /** Re-evaluates the preferred NTP server and any DHCP-provided override. */
  void refreshNtpServer();
  /** Marks the SNTP transport as available after Wi-Fi comes online. */
  void setupNTP();
  /** Returns the current time from GPS, SNTP, or RTC fallback. */
  acetime_t getNow() const override;
  /** Starts a fresh SNTP sync when one is due. */
  void sendRequest() const;
  /** Returns true when GPS, fresh SNTP, or a usable fallback time is available. */
  bool isResponseReady() const;
  /** Consumes GPS, SNTP, or RTC fallback time data. */
  acetime_t readResponse() const;
};
} // namespace clock
} // namespace ace_time

using ace_time::clock::GPSClock;

/** Hybrid GPS/NTP clock used as the external time source for syncing. */
extern GPSClock gpsClock;
/** Main AceTime loop that keeps logical time and manages sync decisions. */
extern SystemClockLoop systemClock;

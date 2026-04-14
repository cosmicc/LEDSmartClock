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
  static const uint8_t kNtpPacketSize = 48;
  const char *const mServer = "172.25.150.1";
  uint16_t const mLocalPort = 2390;
  uint16_t const mRequestTimeout = 1000;
  mutable WiFiUDP mUdp;
  mutable uint8_t mPacketBuffer[kNtpPacketSize];

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
  /** Initializes the UDP socket used for NTP polling. */
  void setupNTP();
  /** Returns the current time from GPS or the latest NTP response. */
  acetime_t getNow() const override;
  /** Sends an NTP request when the clock is due for resynchronization. */
  void sendRequest() const;
  /** Returns true when either GPS data or an NTP response is available. */
  bool isResponseReady() const;
  /** Consumes either GPS time data or an NTP response packet. */
  acetime_t readResponse() const;
  /** Formats and sends a raw NTP packet. */
  void sendNtpPacket(const IPAddress &address) const;
};
} // namespace clock
} // namespace ace_time

using ace_time::clock::GPSClock;

/** Hybrid GPS/NTP clock used as the external time source for syncing. */
extern GPSClock gpsClock;
/** Main AceTime loop that keeps logical time and manages sync decisions. */
extern SystemClockLoop systemClock;

// AceTimeClock GPSClock custom class
namespace ace_time {
namespace clock {
class GPSClock: public Clock {
  private:
    static const uint8_t kNtpPacketSize = 48;
    const char* const mServer = "us.pool.ntp.org";
    uint16_t const mLocalPort = 2390;
    uint16_t const mRequestTimeout = 1000;
    mutable WiFiUDP mUdp;
    mutable uint8_t mPacketBuffer[kNtpPacketSize];

  public:
    mutable bool ntpIsReady = false;
    static const acetime_t kInvalidSeconds = LocalTime::kInvalidSeconds;
    /** Number of millis to wait during connect before timing out. */
    static const uint16_t kConnectTimeoutMillis = 10000;

    bool isSetup() const { return ntpIsReady; }

    void setup() {
      if (iotWebConf.getState() == 4) {
        mUdp.begin(mLocalPort);
        ESP_LOGD(TAG, "GPSClock: NTP ready");
        ntpIsReady = true;
      }
    }

    void ntpReady() {
      if (iotWebConf.getState() == 4 || !ntpIsReady) {
        mUdp.begin(mLocalPort);
        ESP_LOGD(TAG, "GPSClock: NTP ready");
        ntpIsReady = true;
      }
    }

  acetime_t getNow() const {
    if (GPS.time.isUpdated())
        readResponse();
    if (!ntpIsReady || iotWebConf.getState() != 4 || abs(Now() - lastntpcheck) > NTPCHECKTIME*60)
      return kInvalidSeconds;
    sendRequest();
    uint16_t startTime = millis();
    while ((uint16_t)(millis() - startTime) < mRequestTimeout)
      if (isResponseReady())
           return readResponse();
    return kInvalidSeconds;
    }

  void sendRequest() const {
    while (Serial1.available() > 0)
        GPS.encode(Serial1.read());
    if (GPS.time.isUpdated())
        return;
    if (!ntpIsReady)
        return;
    if (iotWebConf.getState() != 4) {
    ESP_LOGW(TAG, "GPSClock: sendRequest(): not connected");
    return;
    }
    if (abs(Now() - lastntpcheck) > NTPCHECKTIME*60) {
      while (mUdp.parsePacket() > 0) {}
      ESP_LOGD(TAG, "GPSClock: sendRequest(): sending request");
      IPAddress ntpServerIP;
      WiFi.hostByName(mServer, ntpServerIP);
      sendNtpPacket(ntpServerIP);  
    }
  }

  bool isResponseReady() const {
    if (GPS.time.isUpdated()) return true;
    if (!ntpIsReady || iotWebConf.getState() != 4)
      return false;
    return mUdp.parsePacket() >= kNtpPacketSize;
  }

  acetime_t readResponse() const {
    if (GPS.time.isUpdated()) {
      auto localDateTime = LocalDateTime::forComponents(GPS.date.year(), GPS.date.month(), GPS.date.day(), GPS.time.hour(), GPS.time.minute(), GPS.time.second());
      acetime_t gpsSeconds = localDateTime.toEpochSeconds();
      if (GPS.time.age() < 100 && GPS.satellites.value() != 0) {
        setTimeSource("gps");
        resetLastNtpCheck();
        ESP_LOGI(TAG, "GPSClock: readResponse(): gpsSeconds: %d | skew: %dsec | age: %dms", gpsSeconds, abs(Now()) - abs(gpsSeconds), GPS.time.age());
        return gpsSeconds;
      } 
      else 
      {
        ESP_LOGW(TAG, "GPSClock: readResponse(): Stale GPS time skipped: gpsSeconds: %d | skew: %dsec | age: %dms", gpsSeconds, abs(Now()) - abs(gpsSeconds), GPS.time.age());
        return kInvalidSeconds;
      }
    }
    if (!ntpIsReady) return kInvalidSeconds;
    if (iotWebConf.getState() != 4) {
      ESP_LOGW(TAG, "GPSClock: readResponse(): not connected");
      return kInvalidSeconds;
    }
    mUdp.read(mPacketBuffer, kNtpPacketSize);
    uint32_t ntpSeconds =  (uint32_t) mPacketBuffer[40] << 24;
    ntpSeconds |= (uint32_t) mPacketBuffer[41] << 16;
    ntpSeconds |= (uint32_t) mPacketBuffer[42] << 8;
    ntpSeconds |= (uint32_t) mPacketBuffer[43];
    if (ntpSeconds != 0) {
      acetime_t epochSeconds = convertUnixEpochToAceTime(ntpSeconds);
      resetLastNtpCheck();
      setTimeSource("ntp");
      ESP_LOGI(TAG, "GPSClock: readResponse(): ntpSeconds: %u | epochSeconds: %d | skew: %dsec | age: %lums", ntpSeconds, epochSeconds, abs(Now()) - abs(epochSeconds), millis()-gps.packetdelay);
      return epochSeconds;
    } else {
        ESP_LOGE(TAG, "GPSClock: Error: 0 NTP seconds recieved");
        return kInvalidSeconds;
    }
  }

  void sendNtpPacket(const IPAddress& address) const {
    memset(mPacketBuffer, 0, kNtpPacketSize);
    mPacketBuffer[0] = 0b11100011;   // LI, Version, Mode
    mPacketBuffer[1] = 0;     // Stratum, or type of clock
    mPacketBuffer[2] = 6;     // Polling Interval
    mPacketBuffer[3] = 0xEC;  // Peer Clock Precision
    mPacketBuffer[12] = 49;
    mPacketBuffer[13] = 0x4E;
    mPacketBuffer[14] = 49;
    mPacketBuffer[15] = 52;
    gps.packetdelay = millis();
    mUdp.beginPacket(address, 123); // NTP requests are to port 123
    mUdp.write(mPacketBuffer, kNtpPacketSize);
    mUdp.endPacket();
  }
};
}
}

using ace_time::clock::GPSClock;
static GPSClock gpsClock;
static SystemClockLoop systemClock(&gpsClock /*reference*/, &dsClock /*backup*/, 60, 5, 1000);  // reference & backup clock
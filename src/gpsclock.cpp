#include "main.h"

namespace ace_time
{
namespace clock
{
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
}

void GPSClock::setupNTP()
{
  if (!ntpIsReady && iotWebConf.getState() == 4)
  {
    mUdp.begin(mLocalPort);
    ESP_LOGD(TAG, "GPSClock: NTP ready");
    ntpIsReady = true;
  }
}

acetime_t GPSClock::getNow() const
{
  if (GPS.time.isUpdated())
    readResponse();
  if (!ntpIsReady || iotWebConf.getState() != 4 || abs(Now() - runtimeState.lastNtpCheck) > (NTPCHECKTIME * 60))
    return kInvalidSeconds;
  sendRequest();
  uint32_t startTime = millis();
  while ((millis() - startTime) < mRequestTimeout)
    if (isResponseReady())
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
  if (abs(Now() - runtimeState.lastNtpCheck) < NTPCHECKTIME * 60)
  {
    return;
  }
  while (mUdp.parsePacket() > 0)
  {
  }
  ESP_LOGD(TAG, "GPSClock: sendRequest(): sending request");
  IPAddress ntpServerIP;
  WiFi.hostByName(mServer, ntpServerIP);
  sendNtpPacket(ntpServerIP);
}

bool GPSClock::isResponseReady() const
{
  if (GPS.time.isUpdated())
    return true;
  if (!ntpIsReady || iotWebConf.getState() != 4)
    return false;
  return mUdp.parsePacket() >= kNtpPacketSize;
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
  if (!ntpIsReady)
    return kInvalidSeconds;
  if (iotWebConf.getState() != 4)
  {
    ESP_LOGW(TAG, "GPSClock: readResponse(): not connected");
    return kInvalidSeconds;
  }
  mUdp.read(mPacketBuffer, kNtpPacketSize);
  uint32_t ntpSeconds = (uint32_t)mPacketBuffer[40] << 24;
  ntpSeconds |= (uint32_t)mPacketBuffer[41] << 16;
  ntpSeconds |= (uint32_t)mPacketBuffer[42] << 8;
  ntpSeconds |= (uint32_t)mPacketBuffer[43];
  if (ntpSeconds != 0)
  {
    acetime_t epochSeconds = convertUnixEpochToAceTime(ntpSeconds);
    resetLastNtpCheck();
    setTimeSource("ntp");
    ESP_LOGI(TAG, "GPSClock: readResponse(): ntpSeconds: %u | epochSeconds: %d | skew: %dsec | age: %lums", ntpSeconds, epochSeconds, abs(Now()) - abs(epochSeconds), millis() - gps.packetdelay);
    return epochSeconds;
  }
  else
  {
    ESP_LOGE(TAG, "GPSClock: Error: 0 NTP seconds recieved");
    return kInvalidSeconds;
  }
}

void GPSClock::sendNtpPacket(const IPAddress &address) const
{
  memset(mPacketBuffer, 0, kNtpPacketSize);
  mPacketBuffer[0] = 0b11100011;
  mPacketBuffer[1] = 0;
  mPacketBuffer[2] = 6;
  mPacketBuffer[3] = 0xEC;
  mPacketBuffer[12] = 49;
  mPacketBuffer[13] = 0x4E;
  mPacketBuffer[14] = 49;
  mPacketBuffer[15] = 52;
  gps.packetdelay = millis();
  mUdp.beginPacket(address, 123);
  mUdp.write(mPacketBuffer, kNtpPacketSize);
  mUdp.endPacket();
}
} // namespace clock
} // namespace ace_time

GPSClock gpsClock;
SystemClockLoop systemClock(&gpsClock, &dsClock, 60, 5, 1000);

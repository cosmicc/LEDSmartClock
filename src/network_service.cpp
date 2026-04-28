#include "main.h"

namespace
{
constexpr int32_t kHttpConnectTimeoutMs = 5000;
constexpr uint16_t kHttpReadTimeoutMs = 5000;
constexpr uint32_t kHttpBodyIdleTimeoutMs = 3000;
constexpr uint32_t kHttpBodyTotalTimeoutMs = 15000;
constexpr size_t kHttpBodyChunkBytes = 256;
constexpr size_t kHttpInitialReserveBytes = 1024;
constexpr size_t kHttpWeatherMaxBytes = 64U * 1024U;
constexpr size_t kHttpAlertsMaxBytes = 64U * 1024U;
constexpr size_t kHttpAqiMaxBytes = 48U * 1024U;
constexpr size_t kHttpSmallResponseMaxBytes = 16U * 1024U;
constexpr const char *kSensitiveQueryKeys[] = {"apiKey=", "appid="};

class BoundedStringWriteStream : public Stream
{
public:
  BoundedStringWriteStream(String &target, size_t limitBytes)
      : target(target), limitBytes(limitBytes)
  {
  }

  int available() override
  {
    return 0;
  }

  int read() override
  {
    return -1;
  }

  int peek() override
  {
    return -1;
  }

  void flush() override
  {
  }

  size_t write(uint8_t value) override
  {
    return write(&value, 1);
  }

  size_t write(const uint8_t *buffer, size_t size) override
  {
    if (buffer == nullptr || size == 0)
      return 0;

    if (target.length() + size > limitBytes)
    {
      exceeded = true;
      setWriteError();
      return 0;
    }

    if (!target.concat(reinterpret_cast<const char *>(buffer), static_cast<unsigned int>(size)))
    {
      outOfMemory = true;
      setWriteError();
      return 0;
    }

    esp_task_wdt_reset();
    return size;
  }

  using Print::write;

  bool exceededLimit() const
  {
    return exceeded;
  }

  bool failedAllocation() const
  {
    return outOfMemory;
  }

private:
  String &target;
  size_t limitBytes;
  bool exceeded = false;
  bool outOfMemory = false;
};

size_t boundedMin(size_t left, size_t right)
{
  return left < right ? left : right;
}

size_t defaultResponseLimit(ApiEndpoint endpoint)
{
  switch (endpoint)
  {
  case ApiEndpoint::Weather:
    return kHttpWeatherMaxBytes;
  case ApiEndpoint::Alerts:
    return kHttpAlertsMaxBytes;
  case ApiEndpoint::AirQuality:
    return kHttpAqiMaxBytes;
  case ApiEndpoint::Geocode:
  case ApiEndpoint::IpGeo:
    return kHttpSmallResponseMaxBytes;
  case ApiEndpoint::Reserved:
  default:
    return kHttpSmallResponseMaxBytes;
  }
}

char *mutableApiUrl(ApiEndpoint endpoint)
{
  switch (endpoint)
  {
  case ApiEndpoint::Weather:
    return networkService.urls.weather;
  case ApiEndpoint::Alerts:
    return networkService.urls.alerts;
  case ApiEndpoint::Geocode:
    return networkService.urls.geocode;
  case ApiEndpoint::AirQuality:
    return networkService.urls.airQuality;
  case ApiEndpoint::IpGeo:
    return networkService.urls.ipGeo;
  case ApiEndpoint::Reserved:
  default:
    return networkService.urls.reserved;
  }
}

const char *endpointName(ApiEndpoint endpoint)
{
  switch (endpoint)
  {
  case ApiEndpoint::Weather:
    return "weather";
  case ApiEndpoint::Alerts:
    return "alerts";
  case ApiEndpoint::Geocode:
    return "geocode";
  case ApiEndpoint::AirQuality:
    return "airquality";
  case ApiEndpoint::IpGeo:
    return "ipgeo";
  case ApiEndpoint::Reserved:
  default:
    return "reserved";
  }
}

String redactedUrl(const char *url)
{
  String sanitized(url == nullptr ? "" : url);
  for (const char *key : kSensitiveQueryKeys)
  {
    int keyIndex = sanitized.indexOf(key);
    while (keyIndex >= 0)
    {
      const int valueStart = keyIndex + strlen(key);
      int valueEnd = sanitized.indexOf('&', valueStart);
      if (valueEnd < 0)
        valueEnd = sanitized.length();
      sanitized = sanitized.substring(0, valueStart) + F("<redacted>") + sanitized.substring(valueEnd);
      keyIndex = sanitized.indexOf(key, valueStart + 10);
    }
  }
  return sanitized;
}
} // namespace

bool isApiValid(char *apikey)
{
  return apikey != nullptr && apikey[0] != '\0';
}

bool isHttpReady()
{
  return !networkService.busy && iotWebConf.getState() == 4 && displaytoken.isReady(0);
}

bool beginApiRequest(ApiEndpoint endpoint)
{
  const char *url = getApiUrl(endpoint);
  if (url == nullptr || url[0] == '\0')
  {
    return false;
  }

  networkService.busy = true;
  runtimeState.networkBusySinceMillis = millis();
  strlcpy(runtimeState.networkBusyEndpoint, endpointName(endpoint), sizeof(runtimeState.networkBusyEndpoint));
  networkService.client.setReuse(false);
  networkService.client.setConnectTimeout(kHttpConnectTimeoutMs);
  networkService.client.setTimeout(kHttpReadTimeoutMs);
  ESP_LOGD(TAG, "Sending %s request: %s", endpointName(endpoint), redactedUrl(url).c_str());
  if (!networkService.client.begin(url))
    return false;

  networkService.client.addHeader(F("Accept"), F("application/json"));
  return true;
}

void endApiRequest()
{
  networkService.client.end();
  networkService.busy = false;
  runtimeState.networkBusySinceMillis = 0;
  runtimeState.networkBusyEndpoint[0] = '\0';
}

bool readHttpResponseBody(ApiEndpoint endpoint, String &payload, size_t maxBytes)
{
  payload = "";
  const size_t limit = maxBytes == 0 ? defaultResponseLimit(endpoint) : maxBytes;
  const int contentLength = networkService.client.getSize();
  const bool unknownLength = contentLength < 0;

  if (contentLength == 0)
    return true;

  if (contentLength > 0 && static_cast<size_t>(contentLength) > limit)
  {
    ESP_LOGE(TAG, "%s response body length %d exceeds the %u byte safety limit",
             endpointName(endpoint), contentLength, static_cast<unsigned>(limit));
    return false;
  }

  WiFiClient *stream = networkService.client.getStreamPtr();
  if (stream == nullptr)
  {
    ESP_LOGE(TAG, "Unable to read %s response body because the HTTP stream is not connected",
             endpointName(endpoint));
    return false;
  }

  const size_t reserveBytes = contentLength > 0
                                  ? boundedMin(static_cast<size_t>(contentLength), limit)
                                  : boundedMin(kHttpInitialReserveBytes, limit);
  if (reserveBytes > 0 && !payload.reserve(reserveBytes))
  {
    ESP_LOGW(TAG, "Unable to reserve %u bytes for %s response; reading incrementally",
             static_cast<unsigned>(reserveBytes), endpointName(endpoint));
  }

  if (unknownLength)
  {
    BoundedStringWriteStream decodedBody(payload, limit);
    const int bytesWritten = networkService.client.writeToStream(&decodedBody);
    if (decodedBody.exceededLimit())
    {
      ESP_LOGE(TAG, "%s response body exceeded the %u byte safety limit while decoding transfer body",
               endpointName(endpoint), static_cast<unsigned>(limit));
      return false;
    }
    if (decodedBody.failedAllocation())
    {
      ESP_LOGE(TAG, "Out of memory while decoding %s response body", endpointName(endpoint));
      return false;
    }
    if (bytesWritten < 0)
    {
      ESP_LOGE(TAG, "Unable to decode %s response body: %s",
               endpointName(endpoint), HTTPClient::errorToString(bytesWritten).c_str());
      return false;
    }
    if (payload.length() == 0)
    {
      ESP_LOGE(TAG, "%s response body decoded to an empty payload without Content-Length",
               endpointName(endpoint));
      return false;
    }

    ESP_LOGD(TAG, "%s response body decoded without Content-Length, payload length: [%u]",
             endpointName(endpoint), static_cast<unsigned>(payload.length()));
    return true;
  }

  uint8_t buffer[kHttpBodyChunkBytes];
  int remaining = contentLength;
  const uint32_t started = millis();
  uint32_t lastProgress = started;

  while (networkService.client.connected() || stream->available() > 0)
  {
    if ((millis() - started) >= kHttpBodyTotalTimeoutMs)
    {
      ESP_LOGE(TAG, "Timed out reading %s response body after %lu ms",
               endpointName(endpoint), static_cast<unsigned long>(kHttpBodyTotalTimeoutMs));
      return false;
    }

    const int available = stream->available();
    if (available > 0)
    {
      size_t toRead = boundedMin(static_cast<size_t>(available), sizeof(buffer));
      if (remaining > 0)
        toRead = boundedMin(toRead, static_cast<size_t>(remaining));

      if (payload.length() + toRead > limit)
      {
        ESP_LOGE(TAG, "%s response body exceeded the %u byte safety limit while reading",
                 endpointName(endpoint), static_cast<unsigned>(limit));
        return false;
      }

      const int bytesRead = stream->readBytes(buffer, toRead);
      if (bytesRead <= 0)
      {
        if ((millis() - lastProgress) >= kHttpBodyIdleTimeoutMs)
        {
          ESP_LOGE(TAG, "Timed out waiting for %s response body data", endpointName(endpoint));
          return false;
        }
        delay(1);
        esp_task_wdt_reset();
        continue;
      }

      if (!payload.concat(buffer, static_cast<unsigned int>(bytesRead)))
      {
        ESP_LOGE(TAG, "Out of memory while appending %s response body", endpointName(endpoint));
        return false;
      }

      if (remaining > 0)
        remaining -= bytesRead;
      if (remaining == 0)
        return true;

      lastProgress = millis();
      esp_task_wdt_reset();
      delay(0);
      continue;
    }

    if (remaining == 0)
      return true;
    if ((millis() - lastProgress) >= kHttpBodyIdleTimeoutMs)
    {
      ESP_LOGE(TAG, "Timed out waiting for %s response body data", endpointName(endpoint));
      return false;
    }
    esp_task_wdt_reset();
    delay(1);
  }

  if (remaining > 0)
  {
    ESP_LOGE(TAG, "%s response body ended with %d byte(s) unread",
             endpointName(endpoint), remaining);
    return false;
  }

  return true;
}

void rebuildApiUrls()
{
  const char *units = imperial.isChecked() ? "imperial" : "metric";

  snprintf(networkService.urls.weather, sizeof(networkService.urls.weather), "%s?units=%s&exclude=minutely,alerts&appid=%s&lat=%f&lon=%f&lang=en",
           OPENWEATHER_ONECALL_ENDPOINT, units, weatherapi.value(), current.lat, current.lon);
  snprintf(networkService.urls.alerts, sizeof(networkService.urls.alerts), "%s?status=actual&point=%f,%f",
           WEATHER_GOV_ALERTS_ENDPOINT, current.lat, current.lon);
  snprintf(networkService.urls.geocode, sizeof(networkService.urls.geocode), "%s?lat=%f&lon=%f&limit=5&appid=%s",
           OPENWEATHER_GEOCODE_ENDPOINT, current.lat, current.lon, weatherapi.value());
  snprintf(networkService.urls.airQuality, sizeof(networkService.urls.airQuality), "%s?lat=%f&lon=%f&appid=%s",
           OPENWEATHER_AQI_ENDPOINT, current.lat, current.lon, weatherapi.value());
  snprintf(networkService.urls.ipGeo, sizeof(networkService.urls.ipGeo), "%s?apiKey=%s",
           IPGEOLOCATION_ENDPOINT, ipgeoapi.value());
  snprintf(networkService.urls.reserved, sizeof(networkService.urls.reserved), "notusedyet");
}

const char *getApiUrl(ApiEndpoint endpoint)
{
  return mutableApiUrl(endpoint);
}

String getHttpCodeName(int code)
{
  switch (code)
  {
  case HTTP_CODE_CONTINUE:
    return "Continue";
  case HTTP_CODE_SWITCHING_PROTOCOLS:
    return "Switching Protocols";
  case HTTP_CODE_PROCESSING:
    return "Processing";
  case HTTP_CODE_OK:
    return "OK";
  case HTTP_CODE_CREATED:
    return "Created";
  case HTTP_CODE_ACCEPTED:
    return "Accepted";
  case HTTP_CODE_NON_AUTHORITATIVE_INFORMATION:
    return "Non-Authoritative Information";
  case HTTP_CODE_NO_CONTENT:
    return "No Content";
  case HTTP_CODE_RESET_CONTENT:
    return "Reset Content";
  case HTTP_CODE_PARTIAL_CONTENT:
    return "Partial Content";
  case HTTP_CODE_MULTI_STATUS:
    return "Multi-Status";
  case HTTP_CODE_ALREADY_REPORTED:
    return "Already Reported";
  case HTTP_CODE_IM_USED:
    return "IM Used";
  case HTTP_CODE_MULTIPLE_CHOICES:
    return "Multiple Choices";
  case HTTP_CODE_MOVED_PERMANENTLY:
    return "Moved Permanently";
  case HTTP_CODE_FOUND:
    return "Found";
  case HTTP_CODE_SEE_OTHER:
    return "See Other";
  case HTTP_CODE_NOT_MODIFIED:
    return "Not Modified";
  case HTTP_CODE_USE_PROXY:
    return "Use Proxy";
  case HTTP_CODE_TEMPORARY_REDIRECT:
    return "Temporary Redirect";
  case HTTP_CODE_PERMANENT_REDIRECT:
    return "Permanent Redirect";
  case HTTP_CODE_BAD_REQUEST:
    return "Bad Request";
  case HTTP_CODE_UNAUTHORIZED:
    return "Unauthorized";
  case HTTP_CODE_PAYMENT_REQUIRED:
    return "Payment Required";
  case HTTP_CODE_FORBIDDEN:
    return "Forbidden";
  case HTTP_CODE_NOT_FOUND:
    return "Not Found";
  case HTTP_CODE_METHOD_NOT_ALLOWED:
    return "Method Not Allowed";
  case HTTP_CODE_NOT_ACCEPTABLE:
    return "Not Acceptable";
  case HTTP_CODE_PROXY_AUTHENTICATION_REQUIRED:
    return "Proxy Authentication Required";
  case HTTP_CODE_REQUEST_TIMEOUT:
    return "Request Timeout";
  case HTTP_CODE_CONFLICT:
    return "Conflict";
  case HTTP_CODE_GONE:
    return "Gone";
  case HTTP_CODE_LENGTH_REQUIRED:
    return "Length Required";
  case HTTP_CODE_PRECONDITION_FAILED:
    return "Precondition Failed";
  case HTTP_CODE_PAYLOAD_TOO_LARGE:
    return "Payload Too Large";
  case HTTP_CODE_URI_TOO_LONG:
    return "URI Too Long";
  case HTTP_CODE_UNSUPPORTED_MEDIA_TYPE:
    return "Unsupported Media Type";
  case HTTP_CODE_RANGE_NOT_SATISFIABLE:
    return "Range Not Satisfiable";
  case HTTP_CODE_EXPECTATION_FAILED:
    return "Expectation Failed";
  case HTTP_CODE_MISDIRECTED_REQUEST:
    return "Misdirected Request";
  case HTTP_CODE_UNPROCESSABLE_ENTITY:
    return "Unprocessable Entity";
  case HTTP_CODE_LOCKED:
    return "Locked";
  case HTTP_CODE_FAILED_DEPENDENCY:
    return "Failed Dependency";
  case HTTP_CODE_UPGRADE_REQUIRED:
    return "Upgrade Required";
  case HTTP_CODE_PRECONDITION_REQUIRED:
    return "Precondition Required";
  case HTTP_CODE_TOO_MANY_REQUESTS:
    return "Too Many Requests";
  case HTTP_CODE_REQUEST_HEADER_FIELDS_TOO_LARGE:
    return "Request Header Fields Too Large";
  case HTTP_CODE_INTERNAL_SERVER_ERROR:
    return "Internal Server Error";
  case HTTP_CODE_NOT_IMPLEMENTED:
    return "Not Implemented";
  case HTTP_CODE_BAD_GATEWAY:
    return "Bad Gateway";
  case HTTP_CODE_SERVICE_UNAVAILABLE:
    return "Service Unavailable";
  case HTTP_CODE_GATEWAY_TIMEOUT:
    return "Gateway Timeout";
  case HTTP_CODE_HTTP_VERSION_NOT_SUPPORTED:
    return "HTTP Version Not Supported";
  case HTTP_CODE_VARIANT_ALSO_NEGOTIATES:
    return "Variant Also Negotiates";
  case HTTP_CODE_INSUFFICIENT_STORAGE:
    return "Insufficient Storage";
  case HTTP_CODE_LOOP_DETECTED:
    return "Loop Detected";
  case HTTP_CODE_NOT_EXTENDED:
    return "Not Extended";
  case HTTP_CODE_NETWORK_AUTHENTICATION_REQUIRED:
    return "Network Authentication Required";
  case -1:
    return "Connection refused";
  case -2:
    return "Send Header Failed";
  case -3:
    return "Send Payload Failed";
  case -4:
    return "Not Connected";
  case -5:
    return "Connection Lost";
  case -6:
    return "No Stream";
  case -7:
    return "No HTTP Server";
  case -8:
    return "Too Less RAM";
  case -9:
    return "Encoding Error";
  case -10:
    return "Stream Write Error";
  case -11:
    return "Read Timeout";
  default:
    return "Unknown Error";
  }
}

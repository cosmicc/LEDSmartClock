#include "main.h"

#include <cstdarg>

namespace
{
constexpr size_t kConsoleLogCapacity = 24U * 1024U;
// Keep formatting scratch buffers intentionally small so ESP-IDF tasks with
// shallow stacks, such as sys_evt during Wi-Fi startup, do not overflow just
// because logging is mirrored into the web console.
constexpr size_t kConsolePrintfBuffer = 192U;

char sConsoleLogBuffer[kConsoleLogCapacity]{};
size_t sConsoleLogHead = 0;
size_t sConsoleLogLength = 0;
uint32_t sConsoleLogCursor = 0;
portMUX_TYPE sConsoleLogMux = portMUX_INITIALIZER_UNLOCKED;
vprintf_like_t sOriginalLogWriter = nullptr;
bool sConsoleLogInstalled = false;

/** Appends raw bytes into the circular RAM console buffer. */
void appendConsoleBytesLocked(const char *data, size_t length)
{
  if (data == nullptr || length == 0)
    return;

  sConsoleLogCursor += static_cast<uint32_t>(length);

  if (length >= kConsoleLogCapacity)
  {
    const char *tail = data + (length - kConsoleLogCapacity);
    memcpy(sConsoleLogBuffer, tail, kConsoleLogCapacity);
    sConsoleLogHead = 0;
    sConsoleLogLength = kConsoleLogCapacity;
    return;
  }

  for (size_t index = 0; index < length; ++index)
  {
    sConsoleLogBuffer[sConsoleLogHead] = data[index];
    sConsoleLogHead = (sConsoleLogHead + 1U) % kConsoleLogCapacity;
    if (sConsoleLogLength < kConsoleLogCapacity)
      ++sConsoleLogLength;
  }
}

/** Copies a console window into a String caller buffer. */
bool copyConsoleWindow(uint32_t since, String &out, uint32_t &cursor, bool &truncated)
{
  portENTER_CRITICAL(&sConsoleLogMux);

  cursor = sConsoleLogCursor;
  const uint32_t earliestCursor = (sConsoleLogCursor > sConsoleLogLength)
                                      ? (sConsoleLogCursor - static_cast<uint32_t>(sConsoleLogLength))
                                      : 0;
  truncated = since < earliestCursor;
  if (since < earliestCursor)
    since = earliestCursor;
  if (since > sConsoleLogCursor)
    since = sConsoleLogCursor;

  const size_t available = static_cast<size_t>(sConsoleLogCursor - since);
  const size_t oldestIndex = (sConsoleLogHead + kConsoleLogCapacity - sConsoleLogLength) % kConsoleLogCapacity;
  const size_t logicalOffset = static_cast<size_t>(since - earliestCursor);
  size_t readIndex = (oldestIndex + logicalOffset) % kConsoleLogCapacity;

  out = "";
  out.reserve(available + 1U);
  for (size_t copied = 0; copied < available; ++copied)
  {
    out += sConsoleLogBuffer[readIndex];
    readIndex = (readIndex + 1U) % kConsoleLogCapacity;
  }

  portEXIT_CRITICAL(&sConsoleLogMux);
  return true;
}

/** Captures ESP-IDF log lines into the RAM console buffer while preserving Serial output. */
int consoleLogVprintf(const char *format, va_list args)
{
  char line[kConsolePrintfBuffer];
  va_list copy;
  va_copy(copy, args);
  const int written = vsnprintf(line, sizeof(line), format, copy);
  va_end(copy);

  if (written > 0)
  {
    size_t used = min(static_cast<size_t>(written), sizeof(line) - 1U);
    if (static_cast<size_t>(written) >= sizeof(line) && used >= 4U)
    {
      memcpy(line + used - 4U, "...", 4U);
      used = sizeof(line) - 1U;
    }
    portENTER_CRITICAL(&sConsoleLogMux);
    appendConsoleBytesLocked(line, used);
    portEXIT_CRITICAL(&sConsoleLogMux);
  }

  if (sOriginalLogWriter != nullptr)
    return sOriginalLogWriter(format, args);

  return vprintf(format, args);
}
} // namespace

ConsoleMirrorPrint::ConsoleMirrorPrint(bool mirrorToSerial)
    : mMirrorToSerial(mirrorToSerial)
{
}

size_t ConsoleMirrorPrint::write(uint8_t ch)
{
  return write(&ch, 1U);
}

size_t ConsoleMirrorPrint::write(const uint8_t *buffer, size_t size)
{
  if (buffer == nullptr || size == 0)
    return 0;

  appendConsoleBytes(reinterpret_cast<const char *>(buffer), size, mMirrorToSerial);
  return size;
}

int ConsoleMirrorPrint::printf(const char *format, ...)
{
  char line[kConsolePrintfBuffer];
  va_list args;
  va_start(args, format);
  const int written = vsnprintf(line, sizeof(line), format, args);
  va_end(args);

  if (written <= 0)
    return written;

  size_t used = min(static_cast<size_t>(written), sizeof(line) - 1U);
  if (static_cast<size_t>(written) >= sizeof(line) && used >= 4U)
  {
    memcpy(line + used - 4U, "...", 4U);
    used = sizeof(line) - 1U;
  }
  write(reinterpret_cast<const uint8_t *>(line), used);
  return written;
}

void initConsoleLog()
{
  if (sConsoleLogInstalled)
    return;

  sOriginalLogWriter = esp_log_set_vprintf(consoleLogVprintf);
  sConsoleLogInstalled = true;
}

void appendConsoleBytes(const char *data, size_t length, bool mirrorToSerial)
{
  if (data == nullptr || length == 0)
    return;

  portENTER_CRITICAL(&sConsoleLogMux);
  appendConsoleBytesLocked(data, length);
  portEXIT_CRITICAL(&sConsoleLogMux);

  if (mirrorToSerial)
    Serial.write(reinterpret_cast<const uint8_t *>(data), length);
}

uint32_t getConsoleLogCursor()
{
  portENTER_CRITICAL(&sConsoleLogMux);
  const uint32_t cursor = sConsoleLogCursor;
  portEXIT_CRITICAL(&sConsoleLogMux);
  return cursor;
}

bool readConsoleLogSnapshot(String &out, uint32_t &cursor, bool &truncated)
{
  return copyConsoleWindow(0, out, cursor, truncated);
}

bool readConsoleLogSince(uint32_t since, String &out, uint32_t &cursor, bool &truncated)
{
  return copyConsoleWindow(since, out, cursor, truncated);
}

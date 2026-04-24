#pragma once

#include <Arduino.h>
#include <Print.h>
#include <cstddef>
#include <cstdint>

/**
 * Mirrors text into the in-memory console buffer and optionally to USB serial.
 *
 * This lets debug helpers, the web console, and command handlers share one
 * output path while still satisfying APIs that expect an Arduino Print target.
 */
class ConsoleMirrorPrint : public Print
{
public:
  /** Creates a console output stream and optionally mirrors writes to Serial. */
  explicit ConsoleMirrorPrint(bool mirrorToSerial = true);

  /** Writes one byte into the console buffer and optional Serial mirror. */
  size_t write(uint8_t ch) override;

  /** Writes a byte span into the console buffer and optional Serial mirror. */
  size_t write(const uint8_t *buffer, size_t size) override;

  /** Formats and writes text into the console buffer and optional Serial mirror. */
  int printf(const char *format, ...);

private:
  bool mMirrorToSerial = true;
};

/** Installs the ESP-IDF log hook so runtime logs are captured in the RAM buffer. */
void initConsoleLog();

/** Appends text directly into the RAM console buffer and optional Serial mirror. */
void appendConsoleBytes(const char *data, size_t length, bool mirrorToSerial);

/** Returns the current end cursor of the RAM console buffer. */
uint32_t getConsoleLogCursor();

/** Returns the number of retained RAM console bytes currently available. */
size_t getConsoleLogRetainedLength();

/** Reads a tail snapshot of the RAM console buffer. */
bool readConsoleLogSnapshot(String &out, uint32_t &cursor, bool &truncated);

/** Reads console output newer than the supplied cursor from the RAM buffer. */
bool readConsoleLogSince(uint32_t since, String &out, uint32_t &cursor, bool &truncated);

/** Clears the retained RAM console buffer and resets its cursor. */
void clearConsoleLog();

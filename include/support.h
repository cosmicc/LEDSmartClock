#pragma once

#include <Arduino.h>
#include <AceTime.h>
#include "runtime_state.h"

/** Formats an elapsed duration into a human-readable string. */
String elapsedTime(uint32_t seconds);
/** Capitalizes the first letter of each word in-place. */
char *capString(char *str);
/** Converts an NTP epoch into the AceTime epoch. */
ace_time::acetime_t convertUnixEpochToAceTime(uint32_t ntpSeconds);
/** Converts a Unix epoch into the currently configured AceTime epoch. */
ace_time::acetime_t convert1970Epoch(ace_time::acetime_t epoch1970);
/** Records the latest active time source string in runtime state. */
void setTimeSource(const String &inputString);
/** Convenience wrapper around the system clock. */
ace_time::acetime_t Now();
/** Stores the timestamp of the latest successful external time sync using the current system clock. */
void resetLastNtpCheck();
/** Stores the timestamp of the latest successful external time sync using a known synced time value. */
void resetLastNtpCheck(ace_time::acetime_t syncedTime);
/** Returns the ordinal suffix for a day number. */
const char *ordinal_suffix(int n);
/** Uppercases a mutable C string in-place. */
void toUpper(char *input);
/** Maps a user brightness level into the LED matrix brightness curve. */
uint16_t calcbright(uint16_t bl);
/** Removes characters that do not render well in scroll text. */
void cleanString(char *str);
/** Uppercases an entire mutable C string in-place. */
void capitalize(char *str);
/** Returns true when a mutable C string contains at least one non-space character. */
bool hasVisibleText(const char *text);
/** Compares two fixed-width location strings for exact equality. */
bool cmpLocs(const char a1[32], const char a2[32]);
/** Converts a numeric UV index into a descriptive label. */
String uv_index(uint8_t index);
/** Returns the delta between now and the supplied timestamp. */
int32_t fromNow(ace_time::acetime_t ctime);
/** Inserts grouping separators into large integers for debug output. */
String formatLargeNumber(int number);
/** Compares two zoned date-times using only hour, minute, and second. */
bool compareTimes(ace_time::ZonedDateTime t1, ace_time::ZonedDateTime t2);
/** Returns the mutable diagnostics record for the selected subsystem. */
ServiceDiagnostic &diagnosticState(DiagnosticService service);
/** Returns the shared label used for the selected diagnostics subsystem. */
const char *diagnosticServiceLabel(DiagnosticService service);
/** Resets all diagnostics records to their boot-time defaults. */
void resetServiceDiagnostics();
/** Records a non-fatal or still-waiting subsystem state for the diagnostics page. */
void noteDiagnosticPending(DiagnosticService service, bool enabled, const char *summary, const String &detail, uint8_t retries = 0, int16_t lastCode = 0);
/** Records a successful subsystem update for the diagnostics page. */
void noteDiagnosticSuccess(DiagnosticService service, bool enabled, const char *summary, const String &detail, uint8_t retries = 0, int16_t lastCode = 200);
/** Records a failed subsystem update for the diagnostics page. */
void noteDiagnosticFailure(DiagnosticService service, bool enabled, const char *summary, const String &detail, int16_t lastCode = 0, uint8_t retries = 0);

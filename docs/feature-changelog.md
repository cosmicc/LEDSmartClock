# LEDSmartClock Feature Changelog

This file tracks completed work and the ranked roadmap for the v2 firmware line.
It is updated through firmware v2.7.2.

## Current Priorities

1. [ ] Add search and filtering on the configuration page.
   Basic and Advanced modes already reduce clutter, but a text filter would make specific settings faster to find.
2. [ ] Add automated tests around config import/export, DST boundaries, invalid backup handling, alert ranking, and release packaging metadata.
3. [ ] Add a hardware self-test mode for RTC, light sensor, GPS UART, matrix output, and network reachability.
4. [ ] Add configuration sanity warnings when update intervals are set aggressively enough to overload APIs or overwhelm the display.

## Recently Completed

- [x] Add manual timezone selection by IANA zone name, with form validation, onboarding support, backup/restore fields, and DST-aware runtime priority ahead of automatic fallbacks.
- [x] Add detailed per-option help text to the configuration page and reorganize the settings into clearer sections: Current Weather, Daily Weather, Air Quality, Weather Alerts, GPS & Receiver, and bottom-of-page Maintenance.
- [x] Change GPS UART baud configuration from a freeform number to a dropdown of common rates through 115200, defaulting to 9600.
- [x] Add a dedicated web `Diagnostics` page with Wi-Fi, timekeeping, NTP, GPS, weather, AQI, alerts, IP geolocation, reverse geocoding, retry state, last success, and last error context.
- [x] Add a dashboard service-health summary so the landing page surfaces failing, pending, disabled, and healthy subsystems without opening diagnostics first.
- [x] Add a downloadable in-memory log buffer and live web `Console` with incremental polling, clear-console support, and web-triggered debug commands.
- [x] Add GPS recovery and troubleshooting controls, including raw NMEA, parser reset, UART restart, baud selection, and visible parser statistics.
- [x] Add one-shot display test actions for AQI, current weather, daily weather, alerts, date, temp/icon, and alert flash without changing the normal display schedule.
- [x] Add `Basic` and `Advanced` configuration modes, grouped configuration sections, section notes, dark mode, password visibility toggles, and factory-default reset that preserves Wi-Fi and API credentials.
- [x] Add first-boot onboarding that walks through Wi-Fi, API keys, timezone behavior, optional web password protection, and a final matrix self-test.
- [x] Add firmware upload progress, `update.bin` validation, success/failure feedback, and backup reminders to the OTA flow.
- [x] Add browser-based first-time install packaging with `firmware.bin`, `update.bin`, release metadata, and a local web installer inside `firmware.zip`.
- [x] Add release/build metadata, including version, git commit, board target, and build date, to the dashboard and release package.
- [x] Add GPS-longitude timezone offset fallback when named timezone data is unavailable.
- [x] Add GPS/coordinate sunrise and sunset fallback when OpenWeather sun-event data is missing or partial.
- [x] Add richer weather-alert handling for multiple active alerts, severity ordering, warning/watch counts, condensed matrix text, and dashboard/diagnostics summaries.
- [x] Update weather fetching to OpenWeather One Call 3.0 and keep weather.gov alerts on the supported active-alert endpoint.
- [x] Harden external API reads and large web pages with bounded HTTP body reads, no-store cache headers, and guarded HTML allocation.
- [x] Fix AQI pollutant parsing and descriptions so PM2.5, PM10, ozone, nitrogen dioxide, and AQI buckets map to the correct display values.

## Completed Foundation

- [x] Rewrite the firmware into a cleaner v2 codebase with typed service/state modules.
- [x] Rebuild the web UI into a cohesive dashboard, config portal, diagnostics page, console, backup/restore flow, and OTA maintenance flow.
- [x] Fix display-token overlap so temp, current weather, forecast, AQI, alerts, and clock rendering no longer fight for the matrix.
- [x] Add configurable NTP server selection with `pool.ntp.org` fallback, DHCP-aware effective-server reporting, and a manual override switch.
- [x] Add configuration export and import so the full clock setup can be downloaded, restored, and migrated across firmware versions even when the option set changes.
- [x] Add an export-config prompt in the firmware update flow so users are reminded to back up settings before flashing new firmware.
- [x] Add a first-run import prompt so a brand-new or reset clock can restore a saved configuration instead of being set up from scratch.
- [x] Rework config storage so future firmware can add, remove, and reorder settings without depending on fragile positional mapping.
- [x] Add a custom password-only web login with session-based protection for the dashboard, diagnostics, console, firmware tools, and advanced configuration portal, plus password recovery through setup mode.

## Later Ideas

- [ ] Add quiet hours or bedtime mode for brightness and noisy scroll suppression overnight.
- [ ] Add multiple display profiles such as home, travel, debug, and minimal clock-only.
- [ ] Add a visual display preview for time, weather, AQI, alerts, colors, and brightness.
- [ ] Derive a real named timezone from GPS coordinates instead of only deriving a fixed offset from longitude.
- [ ] Add a small developer test harness for JSON parsers and config backup validation outside the ESP32 runtime.

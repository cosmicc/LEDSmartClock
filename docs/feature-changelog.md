# LEDSmartClock Feature Changelog

This file tracks completed work and the ranked roadmap for the v2 firmware line.
The `Current Priorities` section is the active implementation order.

## Current Priorities

1. [x] Add a dedicated web `Diagnostics` page.
   Show GPS, NTP, weather, AQI, alerts, Wi-Fi, and API state in one place with last success, last error, and retry counters.
2. [x] Add a downloadable in-memory log buffer.
   Keep the most recent serial/debug lines in RAM and expose them in the web UI so field debugging does not require a live USB console.
3. [x] Add a dashboard service-health summary.
   Surface concise badges such as `GPS: no UART`, `NTP: timeout`, or `Weather: auth failed` so the landing page makes failures obvious.
4. [ ] Add manual timezone selection by zone name.
   Allow a user to select or enter a real timezone such as `America/New_York` instead of relying only on IP geolocation or a fixed GMT offset.
5. [ ] Add GPS recovery and troubleshooting controls.
   Extend the current diagnostics with actions such as parser reset, raw NMEA view, and configurable GPS baud selection.

## Next Up

- [ ] Add `Basic` and `Advanced` configuration modes so common settings are easier to find.
- [ ] Add search and filtering on the configuration page.
- [ ] Add display preview and test actions for time, weather, AQI, alerts, colors, and brightness.
- [ ] Add first-boot onboarding that walks through Wi-Fi, API keys, timezone, and a final self-test.
- [ ] Remove the remaining legacy IotWebConf migration shim after enough devices have upgraded to key-based storage.
- [ ] Add automated tests around config import/export, DST boundaries, and invalid backup handling.
- [ ] Add a hardware self-test mode for RTC, light sensor, GPS UART, matrix output, and network reachability.
- [ ] Add release metadata such as version, git commit, board target, and build date to the status page and release package.
- [ ] Add configuration sanity warnings when update intervals are set aggressively enough to overload APIs or overwhelm the display.

## Later Ideas

- [ ] Add quiet hours or bedtime mode for brightness and noisy scroll suppression overnight.
- [ ] Add multiple display profiles such as home, travel, debug, and minimal clock-only.
- [x] Add richer weather-alert handling for multiple active alerts, severity ordering, and condensed long-text mode.
- [ ] Derive more local behavior directly from GPS location, including timezone selection and sunrise or sunset fallback.

## Completed

- [x] Rewrite the firmware into a cleaner v2 codebase with typed service/state modules.
- [x] Rebuild the web UI into a cohesive dashboard, config portal, and OTA maintenance flow.
- [x] Add firmware upload progress, success/failure feedback, and release packaging guidance.
- [x] Fix display-token overlap so temp, current weather, and clock rendering no longer fight for the matrix.
- [x] Fix the weather.gov alerts URL so alert polling no longer sends the unsupported `limit` query parameter.
- [x] Add configurable NTP server selection with `pool.ntp.org` fallback, DHCP-aware effective-server reporting, and a manual override switch.
- [x] Add configuration export and import so the full clock setup can be downloaded, restored, and migrated across firmware versions even when the option set changes.
- [x] Add an export-config prompt in the firmware update flow so users are reminded to back up settings before flashing new firmware.
- [x] Add a first-run import prompt so a brand-new or reset clock can restore a saved configuration instead of being set up from scratch.
- [x] Rework config storage so future firmware can add, remove, and reorder settings without depending on fragile positional mapping.

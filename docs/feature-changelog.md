# LEDSmartClock Feature Changelog

This file tracks user-requested feature work and larger cleanup items for the v2 firmware line. Check items when they are complete in released firmware.

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

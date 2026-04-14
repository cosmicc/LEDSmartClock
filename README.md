# LED SmartClock

LED SmartClock is an ESP32-based wall clock built around an 8x32 WS2812B LED matrix. It combines a large-format clock display with GPS, NTP, weather, air-quality, and alert data, then exposes everything through a web dashboard, a configuration portal, and OTA firmware updates.

Version `2.2.3` is the current rewrite baseline. It introduces a cleaner internal structure, a redesigned web interface, and a themed OTA update flow with progress feedback.

## What It Does

- Displays a large 12-hour or 24-hour clock on an 8x32 LED matrix.
- Keeps time from GPS, NTP, and the onboard DS3231 RTC.
- Resolves timezone automatically from IP geolocation or uses a fixed manual offset.
- Resolves location from GPS, reverse geocoding, or fixed coordinates.
- Shows rotating data blocks for date, current temperature, current weather, daily forecast, AQI, and weather alerts.
- Adjusts brightness automatically from ambient light, with user bias controls.
- Exposes a status dashboard and a full configuration portal over Wi-Fi.
- Supports OTA updates from the web interface using `firmware.bin`.

## Web Interface

The firmware serves two main pages:

- `Status page`
  - Current time source, timezone source, uptime, location, weather, AQI, and alert state.
  - Reboot action.
  - Link to the firmware update page.
- `Configuration page`
  - Grouped settings for connectivity, display, clock behavior, weather, AQI, sun events, and location.
  - Cancel and reboot actions.
  - Firmware update entry point.

The OTA update page now matches the rest of the UI and shows upload progress while the browser transfers the image.

## Required Services

Some features depend on external services:

- `OpenWeather`
  - Used for current weather, forecast, AQI, and reverse geocoding.
  - Requires an API key with access to `One Call 3.0`.
- `ipgeolocation.io`
  - Used for automatic timezone and coarse location fallback.
  - Requires an API key.
- `weather.gov`
  - Used for alert data.
  - No API key required.

Without API keys, the clock still works as a clock, but weather, AQI, reverse geocoding, and automatic timezone/location features will be limited.

## Hardware

The project currently assumes these core parts:

- ESP32 development board
- 8x32 WS2812B LED matrix
- DS3231 RTC module
- TSL2561 light sensor
- NEO-6M GPS module
- 5V power supply sized for the LED matrix

Important:

- The original hardware notes referenced an `ESP32-S3` board.
- The current PlatformIO project is configured for `esp32dev` in [platformio.ini](platformio.ini).
- Confirm the board target for your hardware before flashing or changing pin assignments.

## First-Time Installation

For a first install, use USB flashing.

The safest path is to build and flash from source with PlatformIO so the bootloader, partitions, and firmware stay aligned with the current project configuration.

### Build From Source

1. Install PlatformIO.
2. Clone this repository.
3. Review [platformio.ini](platformio.ini) and confirm the selected board matches your hardware.
4. Build the firmware:

```bash
platformio run
```

5. Flash over USB using your normal PlatformIO upload workflow or your preferred ESP32 flashing tool.

### Prebuilt Release Notes

If you use release artifacts instead of building locally:

- `firmware.bin` is suitable for OTA updates after the clock is already installed.
- First-time USB flashing may also require `bootloader.bin` and `partitions.bin`, depending on your flashing workflow.
- If you are not sure, prefer a full PlatformIO flash from source for the first install.

## First Boot And Setup

On first boot, or whenever the device is forced back into AP mode, the clock exposes a Wi-Fi access point:

- SSID: `LEDSMARTCLOCK`
- Password: `ledsmartclock`

Initial setup flow:

1. Connect to the `LEDSMARTCLOCK` access point.
2. Open the configuration portal.
3. Set the following first:
   - `AP Password`
   - `Wi-Fi SSID`
   - `Wi-Fi Password`
   - `OpenWeather API key`
   - `ipgeolocation.io API key`
4. Save the configuration.
5. Let the clock connect to your normal Wi-Fi network.
6. Open the status page on the device IP address to verify time sync, location, weather, and AQI.

If GPS is unavailable, the clock can still work with fixed coordinates and a manual timezone offset.

## OTA Updates

After the first USB install, updates can be applied from the browser.

OTA behavior in the current firmware:

- Upload `firmware.bin` only.
- Typical firmware size is about `2 MB`.
- The update page now shows upload progress and final success or failure feedback.
- The device reboots automatically after a successful OTA update.

Limitations:

- OTA does not replace `bootloader.bin`.
- OTA does not replace `partitions.bin`.
- If a future release changes the bootloader or partition layout, update over USB instead of OTA.

## Configuration Notes

The firmware stores settings through IotWebConf. Firmware version and configuration schema are tracked separately:

- Firmware version: `2.2.3`
- Config schema: `16`

The config schema must increase whenever the stored IotWebConf field mapping changes. When that happens, older saved settings may be reset instead of being reused.

## Status Indicators

The display includes configurable status LEDs that summarize runtime state:

- System status
- Air quality status
- UV / upper-corner status
- Alert indicators for active watches and warnings

The status page in the web UI is the best source of detail when diagnosing why a service is not updating.

## Repository Layout

- `src/`
  - Application code
- `include/`
  - Public headers, data models, and shared interfaces
- `docs/rewrite-roadmap.md`
  - Notes on the version 2 rewrite direction

## Development Notes

- `main` is the current release branch.
- `rewrite-foundation` contains the main version 2 rewrite work that has now been merged into `main`.
- `legacy` and `pre-coroutine` are older archive branches.

Useful development commands:

```bash
platformio run
```

```bash
platformio run -t upload
```

If serial debug output is enabled in the configuration, runtime diagnostics are emitted over USB serial.

## Known Caveats

- The current PlatformIO target is `esp32dev`, while earlier project notes referenced `ESP32-S3` hardware. Verify your board choice before flashing.
- OTA updates only cover `firmware.bin`.
- Weather and geolocation features depend on valid external API keys and network access.

## Roadmap

The rewrite is still in progress. The architecture and migration goals are tracked in [docs/rewrite-roadmap.md](docs/rewrite-roadmap.md).

## Contributing

Bug reports, hardware notes, cleanup patches, and UI improvements are welcome. If you open an issue or pull request, include:

- The hardware variant you are using
- Whether GPS is connected
- Whether API keys are configured
- Whether the problem occurs in AP mode, normal Wi-Fi mode, or both
- Any relevant serial debug output

## Credits

- Project author: Ian Perry ([cosmicc](https://github.com/cosmicc))
- AceTime, AceTimeClock, AceRoutine: Brian Parks ([bxparks](https://github.com/bxparks))
- IotWebConf: Balazs Kelemen ([prampec](https://github.com/prampec))
- FastLED NeoMatrix: Marc Merlin ([marcmerlin](https://github.com/marcmerlin))

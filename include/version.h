#pragma once

// Firmware semantic version.
//
// Keeping the numeric parts separate makes it easy to bump major/minor/patch
// independently while still exposing a single derived string for logs, OTA UI,
// and the status page.
#define STRINGIFY_DETAIL(x) #x
#define STRINGIFY(x) STRINGIFY_DETAIL(x)

#define VERSION_MAJOR 2
#define VERSION_MINOR 2
#define VERSION_PATCH 3

#define VERSION_SEMVER STRINGIFY(VERSION_MAJOR) "." STRINGIFY(VERSION_MINOR) "." STRINGIFY(VERSION_PATCH)
#define VERSION "LED SmartClock v" VERSION_SEMVER

// Persisted configuration schema version used by IotWebConf.
//
// This is intentionally separate from the firmware version. Advance this any
// time the IotWebConf parameter set, storage order, or field mapping changes,
// otherwise previously saved settings can be read back into the wrong fields.
#define CONFIG_VERSION "16"

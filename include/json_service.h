#pragma once

#include <Arduino.h>

/**
 * Parses a weather.gov alert payload into the ranked shared alert state.
 */
bool fillAlertsFromJson(const String &payload);

/**
 * Parses an OpenWeather weather payload into the shared weather state.
 */
bool fillWeatherFromJson(const String &payload);

/**
 * Parses an ipgeolocation.io payload into the shared timezone/location state.
 */
bool fillIpgeoFromJson(const String &payload);

/**
 * Parses an OpenWeather reverse-geocoding payload into the shared location
 * state.
 */
bool fillGeocodeFromJson(const String &payload);

/**
 * Parses an OpenWeather air-quality payload into the shared AQI state.
 */
bool fillAqiFromJson(const String &payload);

/**
 * Returns a lookup-table-safe AQI bucket. OpenWeather buckets are 1..5; 0 is
 * used locally for unknown or invalid data.
 */
uint8_t safeAqiIndex(int value);

/**
 * Recomputes the human-readable AQI description based on parsed pollutant
 * values.
 */
void processAqiDescription();

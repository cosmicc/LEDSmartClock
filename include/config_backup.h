#pragma once

#include <Arduino.h>

/**
 * Summarizes the outcome of importing a configuration backup file.
 *
 * Import can still succeed when unknown or obsolete fields are present, so the
 * warning counters help the UI explain what happened without failing the whole
 * restore.
 */
struct ConfigImportResult
{
  /** True when at least one recognized field was restored successfully. */
  bool success = false;
  /** True when some fields were ignored or rejected during import. */
  bool hasWarnings = false;
  /** Number of recognized settings applied from the backup file. */
  uint16_t appliedCount = 0;
  /** Number of unknown keys ignored because this firmware does not use them. */
  uint16_t ignoredCount = 0;
  /** Number of recognized keys rejected because their values were invalid. */
  uint16_t rejectedCount = 0;
  /** Human-readable import summary shown in the web UI after restore. */
  String summary;
  /** Human-readable fatal error shown when the backup file could not be parsed. */
  String error;
};

/** Builds a downloadable JSON backup containing the current configuration. */
bool exportConfigurationBackup(String &json, String &error);

/** Imports a JSON backup, applies any recognized fields, and persists them. */
bool importConfigurationBackup(const String &json, ConfigImportResult &result);

/** Returns the default attachment filename used for exported config backups. */
String configBackupDownloadFileName();

/** Returns true when a key-based persisted configuration already exists. */
bool hasStoredConfiguration();

/** Loads the key-based persisted configuration into the live config fields. */
bool loadStoredConfiguration(String &error);

/** Saves the live configuration into the key-based persisted store. */
bool saveStoredConfiguration(String &error);

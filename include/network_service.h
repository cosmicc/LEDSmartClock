#pragma once

#include <Arduino.h>

#include "runtime_state.h"

/**
 * Returns true when an API key has been configured.
 *
 * Providers validate the actual key contents; locally we only need to know
 * whether a value is present.
 */
bool isApiValid(char *apikey);

/**
 * Returns true when the shared HTTP client is idle and safe to reuse.
 */
bool isHttpReady();

/**
 * Starts a request against the selected API endpoint and marks the network
 * transport as busy.
 */
bool beginApiRequest(ApiEndpoint endpoint);

/**
 * Releases the shared HTTP client after a request completes.
 */
void endApiRequest();

/**
 * Rebuilds all API URLs using the current configuration and coordinates.
 */
void rebuildApiUrls();

/**
 * Returns the resolved URL for the selected endpoint.
 */
const char *getApiUrl(ApiEndpoint endpoint);

/**
 * Converts HTTP status codes into readable names for logs and diagnostics.
 */
String getHttpCodeName(int code);

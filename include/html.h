#pragma once

/** Applies the shared LED Smart Clock visual theme to the config portal. */
void configureWebUi();

/** Registers the root, config, and fallback web handlers on the shared server. */
void registerWebRoutes();

/** Renders the root web status page. */
void handleRoot();

/** Queues a controlled reboot after serving the confirmation page. */
void handleReboot();

/** Validates the configuration form before settings are persisted. */
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper);

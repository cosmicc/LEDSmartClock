#pragma once

#include <Arduino.h>

/** Packs an RGB color into the 16-bit format used by the display library. */
uint16_t RGB16(uint8_t r, uint8_t g, uint8_t b);

/** Shared named colors used by display rendering, alerts, and the web theme. */
extern const uint16_t BLACK;
extern const uint16_t RED;
extern const uint16_t GREEN;
extern const uint16_t BLUE;
extern const uint16_t YELLOW;
extern const uint16_t CYAN;
extern const uint16_t PINK;
extern const uint16_t WHITE;
extern const uint16_t ORANGE;
extern const uint16_t PURPLE;
extern const uint16_t DAYBLUE;
extern const uint16_t LIME;
extern const uint16_t DARKGREEN;
extern const uint16_t DARKRED;
extern const uint16_t DARKBLUE;
extern const uint16_t DARKYELLOW;
extern const uint16_t DARKPURPLE;
extern const uint16_t DARKMAGENTA;
extern const uint16_t DARKCYAN;
extern const uint16_t DARKORANGE;

/** Converts two hexadecimal characters into their byte value. */
int hexcolorToInt(char upper, char lower);

/** Converts an HTML-style `#RRGGBB` string into a 16-bit display color. */
uint16_t hex2rgb(const char *str);

/** Converts a hue value into a full-saturation RGB color. */
uint16_t hsv2rgb(uint8_t hsvr);

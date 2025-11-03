// Simple placeholder XBM icons (30x30) for anomaly indicators.
// Replace with proper artwork when available. Bit order: XBM (LSB first).

#pragma once

#include <stdint.h>

namespace asap::display::assets {

// Placeholder: 30x30 transparent (all zeros). Replace with real art.
// Each row is 30 bits => 4 bytes, 30 rows => 120 bytes per icon.
static const uint8_t kIconRadiation30x30[120] = { 0 };
static const uint8_t kIconFire30x30[120]      = { 0 };
static const uint8_t kIconBiohazard30x30[120] = { 0 };
static const uint8_t kIconPsy30x30[120]       = { 0 };

constexpr uint8_t kIconW = 30;
constexpr uint8_t kIconH = 30;

} // namespace asap::display::assets

#pragma once

#include <stdint.h>
#include <U8g2lib.h>

#include <asap/display/DisplayTypes.h>

namespace asap::display
{

// Shared U8g2-based rendering helpers used by both hardware and native paths.
// These routines translate a DisplayFrame into draw calls on a provided U8G2.

void renderFrameU8g2(::U8G2& u8g2, const DisplayFrame& frame);

void drawAnomalyIndicatorsU8g2(::U8G2& u8g2,
                               uint8_t radPercent, uint8_t thermPercent,
                               uint8_t chemPercent, uint8_t psyPercent,
                               uint8_t radStage, uint8_t thermStage,
                               uint8_t chemStage, uint8_t psyStage);

}  // namespace asap::display

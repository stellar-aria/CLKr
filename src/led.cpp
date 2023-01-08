// Copyright 2023 Katherine Whitlock
//
// Author: Katherine Whitlock (kate@skylinesynths.nyc)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// -----------------------------------------------------------------------------
//
// LED helper function implementations, primarily patterns

#include "led.h"
#include "resources.h"
#include <avrlib/time.h>

namespace clkr {
void LedDance() {
  LedSetBrightness(LED_CLOCK, BRIGHTNESS_FULL);
  LedSetBrightness(LED_PAUSE, BRIGHTNESS_NONE);
  for (int i = 0; i < 3; i++) {
    ConstantDelay(150);
    LedSetBrightness(LED_CLOCK, BRIGHTNESS_NONE);
    LedSetBrightness(LED_PAUSE, BRIGHTNESS_FULL);
    ConstantDelay(150);
    LedSetBrightness(LED_CLOCK, BRIGHTNESS_FULL);
    LedSetBrightness(LED_PAUSE, BRIGHTNESS_NONE);
  }
  LedSetBrightness(LED_CLOCK, BRIGHTNESS_NONE);
  LedSetBrightness(LED_PAUSE, BRIGHTNESS_NONE);
  LedSetBrightness(LED_CLOCK, BRIGHTNESS_FULL);
  LedSetBrightness(LED_PAUSE, BRIGHTNESS_FULL);
}

void FadeLeds() {
  static uint16_t fader_idx;
  static uint32_t faderTimer;
  uint8_t fader = pgm_read_byte(lut_res_gauss_curve + fader_idx);
  LedSetBrightness(LED_CLOCK, fader);
  LedSetBrightness(LED_PAUSE, fader);

  if (faderTimer == 0) {
    fader_idx += 1;
    if (fader_idx >= LUT_RES_GAUSS_CURVE_SIZE) {
      fader_idx = 0;
    }
    faderTimer = 45;
  } else {
    faderTimer--;
  }
}
} // namespace clkr
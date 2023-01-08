// Copyright 2012 Emilie Gillet, 2023 Katherine Whitlock
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
//         Katherine Whitlock (kate@skylinesynths.nyc)
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
// Resources definitions.
//
// Automatically generated with:
// make resources
#pragma once

#include "avrlib/base.h"
#include <avr/pgmspace.h>

#include "avrlib/resources_manager.h"

namespace clkr {

extern const uint32_t lut_res_tempo_phase_increment[] PROGMEM;
#define LUT_RES_TEMPO_PHASE_INCREMENT 1
#define LUT_RES_TEMPO_PHASE_INCREMENT_SIZE 512

extern const uint8_t lut_res_gauss_curve[] PROGMEM;
#define LUT_RES_GAUSS_CURVE_SIZE 500

extern const uint32_t lut_res_legacy_timer_lin[] PROGMEM;
extern const uint32_t lut_res_legacy_timer_log[] PROGMEM;
#define LUT_RES_LEGACY_TIMER_SCALER_SIZE 255
}
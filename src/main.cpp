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

#include "avrlib/adc.h"
#include "avrlib/boot.h"
#include "avrlib/gpio.h"
#include "avrlib/op.h"
#include "avrlib/time.h"
#include "avrlib/watchdog_timer.h"
#include "clock.h"
#include "hardware_config.h"
#include "led.h"
#include "resources.h"
#include "running_average.h"

using namespace avrlib;
using namespace clkr;

// 625khz (1.6us) / 8000 = 78.125hz (12.8ms)
constexpr uint8_t kUpdatePeriod = 20000000 / 64 / 8000;
constexpr uint8_t kTimer2Period = 25;

Gpio<PortB, 5> clockOut;
DigitalInput<Gpio<PortB, 4>> button;
DigitalInput<Gpio<PortC, 2>> selector;
AdcInputScanner adc;

enum Parameter {
  PARAMETER_NONE,       // In main mode
  PARAMETER_TRANSITION, // Entering/Leaving settings editor
  PARAMETER_WAITING,    // In settings editor mode
  PARAMETER_CLOCK_RESOLUTION,
  PARAMETER_TAP_TEMPO, // or pause
};

enum SpeedMode {
  MODE_FAST, // 0v corresponds to right position, and 5v to left position
  MODE_SLOW, // ordered like this for ease of translation
};

volatile bool legacy_clock = LOW;

// The overall state of the clock system,
// in reality, whether the clock outputs are enabled or not
enum RunState { STATE_RUNNING, STATE_PAUSED };

uint32_t tap_duration = 0;

volatile Parameter parameter = PARAMETER_NONE;
volatile SpeedMode speed_mode = MODE_FAST;
volatile RunState run_state = STATE_RUNNING;
volatile bool long_press_detected = false;

// This is how we count for the legacy system:
// very fast, very frequent
// Why do this instead of modifying the comparator?
// Timer is only 16bits wide maximum (Timer1)
// Maximum comparison for legacy is 6250000 (10 seconds)
// which exceeds the 16bit width
volatile uint32_t legacy_comparator;
volatile uint32_t legacy_counter = 0;
ISR(TIMER2_COMPA_vect) { legacy_counter += kTimer2Period; }

inline void HandleClockInternalLegacy() {
  // Legacy clock system
  if (clock.legacy_mode() && legacy_counter >= legacy_comparator) {
    legacy_counter = 0;
    legacy_clock = !legacy_clock;
  }
}

uint8_t led_pattern[2] = {0, 0};

/* Update the LEDS to reflect the current state of the system. */
inline void UpdateLeds() {
  uint8_t clock_pwm = led_pattern[LED_CLOCK];
  uint8_t pause_pwm = led_pattern[LED_PAUSE];

  // We're not editing any parameters...
  if (parameter == PARAMETER_NONE) {
    clock_pwm = 0x00; // led_pattern[LED_CLOCK];
    pause_pwm = 0x00; // led_pattern[LED_PAUSE];

    // Clock LED
    if (clock.legacy_mode()) {
      clock_pwm = legacy_clock ? BRIGHTNESS_FULL : BRIGHTNESS_NONE;
    } else { // Grids Mode
      if (clock.on_first_half()) {
        clock_pwm = BRIGHTNESS_FULL;

        // If the clock is locked, (such as by tap tempo),
        // also flash the pause light at the same time to
        // give a "synchronized" appearance.
        if (clock.locked()) {
          pause_pwm = BRIGHTNESS_FULL;
        }
      }
    }

    // If we're paused, display such
    if (run_state == STATE_PAUSED) {
      pause_pwm = BRIGHTNESS_FULL;
    }

    LedSetBrightness(LED_CLOCK, clock_pwm);
    LedSetBrightness(LED_PAUSE, pause_pwm);
    led_pattern[LED_CLOCK] = clock_pwm;
    led_pattern[LED_PAUSE] = pause_pwm;
  } else if (parameter == PARAMETER_TRANSITION) {
    // Swapping between modes
  } else if (parameter == PARAMETER_WAITING) {
    // WAITING for parameter to edit...
    FadeLeds();
  } else { // EDITing a parameter
    clock_pwm = BRIGHTNESS_NONE;
    pause_pwm = BRIGHTNESS_NONE;
    switch (parameter) {
    case PARAMETER_CLOCK_RESOLUTION: {
      if (!clock.legacy_mode()) { // legacy mode is just all dark
        auto clock_resolution = clock.clock_resolution();
        if (clock_resolution == CLOCK_RESOLUTION_4_PPQN) {
          clock_pwm = BRIGHTNESS_FULL;
        } else if (clock_resolution == CLOCK_RESOLUTION_8_PPQN) {
          pause_pwm = BRIGHTNESS_FULL;
        } else if (clock_resolution == CLOCK_RESOLUTION_24_PPQN) {
          clock_pwm = BRIGHTNESS_FULL;
          pause_pwm = BRIGHTNESS_FULL;
        }
      }
      break;
    }

    case PARAMETER_TAP_TEMPO:
      if (clock.tap_tempo()) {
        pause_pwm = BRIGHTNESS_FULL;
      } else {
        clock_pwm = BRIGHTNESS_FULL;
      }
      break;

    default:
      break;
    }
    LedSetBrightness(LED_CLOCK, clock_pwm);
    LedSetBrightness(LED_PAUSE, pause_pwm);
  }
}

/* Clock output signal function*/
inline void UpdateClockOut() {
  if (run_state == STATE_PAUSED) {
    clockOut.set_value(LOW);
    return;
  }

  // Legacy Mode
  // No differentiation between FAST and SLOW here, it's
  // handled by the changing timer prescaler in ScanPots()
  if (clock.legacy_mode()) {
    clockOut.set_value(legacy_clock);
    return;
  }

  // Grids Mode
  switch (speed_mode) {
  // In the FAST mode, past_falling_edge and new_pulse
  // determine the bounds of our square wave output
  case MODE_FAST:
    if (clock.past_falling_edge()) {
      clockOut.set_value(LOW);
    } else if (clock.new_pulse()) {
      clockOut.set_value(HIGH);
    }
    break;

  // But SLOW mode just follows the LED (50% duty cycle)
  case MODE_SLOW:
    if (clock.on_first_half()) {
      clockOut.set_value(HIGH);
    } else {
      clockOut.set_value(LOW);
    }
    break;
  }
}

uint8_t ticks_granularity[] = {6, 3, 1, 0};

// This function is what actually pushes the system
// forwards, called from the Grids timer's interrupt
inline void HandleClockInternalGrids() {
  uint8_t num_ticks = 0;
  uint8_t increment = ticks_granularity[clock.clock_resolution()];

  clock.Tick();
  clock.Wrap(0); // No clock swing
  if (clock.raising_edge()) {
    num_ticks = increment;
  }

  if (num_ticks) {
    clock.TickClock(num_ticks);
  }
}

enum SwitchState {
  SWITCH_STATE_JUST_PRESSED = 0x01,
  SWITCH_STATE_PRESSED = 0xff,
  SWITCH_STATE_JUST_RELEASED = 0xfe,
  SWITCH_STATE_RELEASED = 0x00
};

inline void HandleTapButton() {
  static uint8_t switch_state = 0x00; // default state is LOW
  static uint16_t switch_hold_time = 0;

  switch_state = switch_state << 1;
  if (button.Read()) {
    switch_state |= 1;
  }

  if (switch_state == SWITCH_STATE_JUST_PRESSED) {
    if (parameter == PARAMETER_NONE) {
      if (!clock.tap_tempo() || clock.legacy_mode()) {
        // Act as a pause button
        run_state = static_cast<RunState>(!run_state);
        clock.Reset();
      } else {
        // Tap Tempo system
        uint32_t new_bpm = (F_CPU * 60L) / (64 * kUpdatePeriod * tap_duration);
        if (new_bpm >= 30 && new_bpm <= 480) {
          clock.Update(new_bpm, clock.clock_resolution());
          clock.Reset();
          clock.Lock();
          clock.SaveSettings();
        } else {
          clock.Unlock();
          clock.SaveSettings();
        }
        tap_duration = 0;
      }
    }
    switch_hold_time = 0;
  } else if (switch_state == SWITCH_STATE_PRESSED) {
    ++switch_hold_time;
    if (switch_hold_time == 1000) {
      long_press_detected = true;
    }
  }
}

// Interrupt for Timer1 (GRIDS MODE)
ISR(TIMER1_COMPA_vect, ISR_NOBLOCK) {
  static uint8_t switch_debounce_prescaler;

  ++tap_duration;
  ++switch_debounce_prescaler;
  if (switch_debounce_prescaler >= 10) {
    // Debounce RESET/TAP switch and perform switch action.
    HandleTapButton();
    switch_debounce_prescaler = 0;
  }

  if (clock.legacy_mode()) {
    HandleClockInternalLegacy();
  } else {
    HandleClockInternalGrids();
  }

  adc.Scan();
  UpdateClockOut();
  UpdateLeds();
}

// Pin Change Interrupt for Pause CV input (Port C, Pin 3)
ISR(PCINT1_vect) {
  // read the port and mask with desired pin
  bool pin_state = (PINC & _BV(PINC3));

  // invert because inverting op-amp input (high CV is low MCU input)
  bool cv_input = !pin_state;
  if (cv_input == HIGH) {
    run_state = STATE_PAUSED;
  } else if (cv_input == LOW) {
    run_state = STATE_RUNNING;
  }
}

RunningAverage<10> smooth_rate;
static uint8_t pot_values[8];
static uint32_t parameter_timeout = 0;

/**
 * @brief ScanPots deals with constantly checking the inputs, both CV and UI.
 * It's called from our main() loop, so it handles things for both the
 * Grids-based system, and the Legacy system.
 */
void ScanPots() {
  // This handles switching to the settings menu
  if (long_press_detected) {
    if (parameter == PARAMETER_NONE) {
      // Freeze pot values, enter settings mode
      for (uint8_t i = 1; i < 3; ++i) {
        pot_values[i] = adc.Read8(i);
      }
      parameter = PARAMETER_TRANSITION;
      LedDance();
      parameter = PARAMETER_WAITING;
    } else { // Save the settings, exit settings menu to main functionality
      parameter = PARAMETER_TRANSITION;
      LedDance();
      clock.SaveSettings();
      parameter = PARAMETER_NONE;

      // if the pause function is disabled, make sure we're running
      if (clock.tap_tempo() && !clock.legacy_mode()) {
        run_state = STATE_RUNNING;
      }
    }
    long_press_detected = false;
  }

  if (parameter == PARAMETER_NONE) {                // In normal operation...
    uint8_t pot_val = adc.Read8(ADC_CHANNEL_TEMPO); // Fetch the pot value
    pot_val = smooth_rate.push_and_get(pot_val);    // Smooth it out
    uint8_t cv_val = ~adc.Read8(ADC_CHANNEL_TEMPO_CV);

    // Legacy Mode update
    uint16_t combo_val = pot_val + cv_val; // plain add the values
    // clamp to maximum index
    if (combo_val > LUT_RES_LEGACY_TIMER_SCALER_SIZE) {
      combo_val = LUT_RES_LEGACY_TIMER_SCALER_SIZE;
    }
    // Tap Tempo mode setting doubles as lin/log setting for legacy mode
    if (clock.tap_tempo()) {
      legacy_comparator = pgm_read_dword(lut_res_legacy_timer_log + pot_val);
    } else {
      legacy_comparator = pgm_read_dword(lut_res_legacy_timer_lin + pot_val);
    }

    // Grids BPM update
    uint8_t pot_bpm = U8U8MulShift8(pot_val, 220) + 20;
    uint16_t bpm = pot_bpm + U8U8MulShift8(cv_val, 240);
    if (bpm != clock.bpm() && !clock.locked()) {
      clock.Update(bpm, clock.clock_resolution());
    }

    // Fetch the switch value
    bool mode = adc.Read8(ADC_CHANNEL_SELECTOR) & 0x80;
    if (mode) { // switch to left
      speed_mode = MODE_SLOW;
      TCCR2B = _BV(CS21) | _BV(CS20); // legacy mode clk/32 prescaler
    } else {
      speed_mode = MODE_FAST;
      TCCR2B = _BV(CS21); // legacy mode clk/8 prescaler
    }

  } else { // In Settings menu, editing parameters...
    // There's only two inputs we care about,
    for (uint8_t i = ADC_CHANNEL_TEMPO; i <= ADC_CHANNEL_SELECTOR; ++i) {
      int16_t value = adc.Read8(i);
      int16_t delta = value - pot_values[i]; // calculate the change
      if (delta < 0) {
        delta = -delta; // absolute value of delta
      }

      if (delta > 32) {
        // value is from Read8, guranteed uint8_t
        pot_values[i] = static_cast<uint8_t>(value);

        switch (i) {
        // Editing the clock resolution/mode
        case ADC_CHANNEL_TEMPO: {
          parameter = PARAMETER_CLOCK_RESOLUTION;

          // take only the two most significant bits of the value
          uint8_t truncated_value = (value >> 6);
          if (truncated_value == 0x00) { // Enable legacy mode
            clock.set_legacy_mode(true);
            TIMSK2 = _BV(OCIE2A); // enable our Timer 2 interrupt
          } else {
            TIMSK2 = 0x00; // clear Timer 2 interrupt enable
            clock.set_legacy_mode(false);
            // clock resolutions are 0 indexed and don't include the legacy mode
            clock.set_clock_resolution(truncated_value - 1);
            clock.Update(clock.bpm(), clock.clock_resolution());
          }
          break;
        }

        // Editing the Tap Tempo settings
        case ADC_CHANNEL_SELECTOR:
          parameter = PARAMETER_TAP_TEMPO;
          clock.set_tap_tempo(!(value & 0x80));
          if (!clock.tap_tempo()) {
            clock.Unlock();
          }
          break;
        }
        parameter_timeout = 400000;
      }
    }
    if (parameter != PARAMETER_WAITING) {
      parameter_timeout -= 1;
      if (parameter_timeout <= 0) {
        parameter = PARAMETER_WAITING;
      }
    }
  }
}

/**
 * @brief Initialize the microcontroller.
 * This handles setup of all the required pins, timers, and interrupts
 */
void Init() {
  sei();
  UCSR0B = 0;

  clockOut.set_mode(DIGITAL_OUTPUT);
  clock.Init();

  button.Init();
  button.DisablePullUpResistor();

  adc.Init();
  adc.set_num_inputs(ADC_CHANNEL_LAST);
  Adc::set_reference(ADC_DEFAULT);
  Adc::set_alignment(ADC_LEFT_ALIGNED);

  // Pin Change Interrupt for Pause CV (port C, pin 3)
  PCICR |= _BV(PCIE1);
  PCMSK1 |= _BV(PCINT11);

  // Setup LED outputs
  DDRD |= _BV(PD6) | _BV(PD5);
  OCR0A = 0;
  OCR0B = 0;
  TCCR0A = _BV(COM0A1) | _BV(COM0B1) | _BV(WGM01) |
           _BV(WGM00); // Non-Inverting Fast PWM mode 3 using OCR A & B unit
  TCCR0B = _BV(CS00);  // No-Prescalar

  // Setup GRIDS MODE timer
  TCCR1A = 0x00;
  TCCR1B = _BV(WGM12) // Clear Timer on Compare Match (CTC), TOP is set by OCR1A
           | _BV(CS11) | _BV(CS10); // Set the prescaler to clk/64
  OCR1A = kUpdatePeriod - 1;        // every 12.8ms
  TIMSK1 = _BV(OCIE1A);             // Output Compare Match A Interrupt Enable

  // Setup internal clock timer (LEGACY)
  TCCR2A = _BV(WGM21);   // CTC, TOP is set by OCR2A
  TCCR2B = _BV(CS21);    // divide clock source by 8
  OCR2A = kTimer2Period; // update every interval

  // We'll only enable the interrupt when we switch to
  // legacy mode to save clock cycles in Grids mode.
  if (clock.legacy_mode()) {
    TIMSK2 = _BV(OCIE2A); // enable our Timer 2 interrupt
  }
}

/**
 * @brief The main loop
 * Very simple main loop, most of the major timekeeping and output updating is
 * done via interrupts ()
 */
int main(void) {
  ResetWatchdog();
  Init();
  clock.Update(clock.bpm(), clock.clock_resolution());
  while (1) {
    // Use any spare cycles to read the CVs and update the potentiometers
    ScanPots();
  }
}
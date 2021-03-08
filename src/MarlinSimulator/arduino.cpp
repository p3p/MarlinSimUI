/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include <iostream>
#include <src/inc/MarlinConfig.h>
#include <MarlinSimulator/execution_control.h>
#include <src/HAL/shared/Delay.h>

// Interrupts
void cli() { Kernel::disableInterrupts(); } // Disable
void sei() { Kernel::enableInterrupts(); } // Enable

void noInterrupts() {cli();}
void interrupts() {sei();}

// Time functions
void _delay_ms(const int delay_ms) {
  delay(delay_ms);
}

uint32_t millis() {
  return (uint32_t)Kernel::TimeControl::millis();
}

uint64_t micros() {
  return (uint32_t)Kernel::TimeControl::micros();
}

// This is required for some Arduino libraries we are using
void delayMicroseconds(unsigned long us) {
  Kernel::delayMicros(us);
}

extern "C" void delay(const int msec) {
  Kernel::delayMillis(msec);
}

// IO functions
// As defined by Arduino INPUT(0x0), OUTPUT(0x1), INPUT_PULLUP(0x2)
void pinMode(const pin_t pin, const uint8_t mode) {
  if (!VALID_PIN(pin)) return;
  Gpio::setMode(pin, mode);
}

void digitalWrite(pin_t pin, uint8_t pin_status) {
  if (!VALID_PIN(pin)) return;
  Gpio::set(pin, pin_status);
}

bool digitalRead(pin_t pin) {
  if (!VALID_PIN(pin)) return false;
  return Gpio::get(pin);
}

void analogWrite(pin_t pin, int pwm_value) {  // 1 - 254: pwm_value, 0: LOW, 255: HIGH
  if (!VALID_PIN(pin)) return;
  Gpio::set(pin, pwm_value);
}

uint16_t analogRead(pin_t adc_pin) {
  if (!VALID_PIN(DIGITAL_PIN_TO_ANALOG_PIN(adc_pin))) return 0;
  return Gpio::get(DIGITAL_PIN_TO_ANALOG_PIN(adc_pin));
}

char *dtostrf(double __val, signed char __width, unsigned char __prec, char *__s) {
  char format_string[20];
  snprintf(format_string, 20, "%%%d.%df", __width, __prec);
  sprintf(__s, format_string, __val);
  return __s;
}

int32_t random(int32_t max) {
  return rand() % max;
}

int32_t random(int32_t min, int32_t max) {
  return min + rand() % (max - min);
}

void randomSeed(uint32_t value) {
  srand(value);
}

int map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


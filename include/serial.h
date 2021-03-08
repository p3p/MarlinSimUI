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
#pragma once

#include <src/inc/MarlinConfigPre.h>
#if ENABLED(EMERGENCY_PARSER)
  #include <src/feature/e_parser.h>
#endif
#include "src/core/serial_hook.h"

#include <mutex>
#include <atomic>
#include <cstring>
#include <stdarg.h>
#include <stdio.h>

/**
 * Generic RingBuffer
 * T type of the buffer array
 * S size of the buffer (must be power of 2)
 */
template <typename T, std::size_t S> class RingBuffer {
public:
  RingBuffer() {index_read = index_write = 0;}

  std::size_t available() const {return mask(index_write - index_read);}
  std::size_t free() const {return size() - available();}
  bool empty() const {return index_read == index_write;}
  bool full() const {return next(index_write) == index_read;}
  void clear() {index_read = index_write = 0;}

  bool peek(T *const value) const {
    if (value == nullptr || empty()) return false;
    *value = buffer[index_read];
    return true;
  }

  inline std::size_t read(T* dst, std::size_t length) {
    std::scoped_lock lock(m);
    length = std::min(length, available());
    const std::size_t length1 = std::min(length, buffer_size - index_read);
    assert(index_read < buffer_size);
    assert(index_read < buffer_size + length1);
    memcpy(dst, (char*)buffer + index_read, length1);
    memcpy(dst + length1, (char*)buffer, length - length1);
    index_read = mask(index_read + length);
    return length;
  }

  inline std::size_t write(T* src, std::size_t length) {
    std::scoped_lock lock(m);
    length = std::min(length, free());
    const std::size_t length1 = std::min(length, buffer_size - index_write);
    assert(index_write < buffer_size);
    assert(index_write < buffer_size + length1);
    memcpy((char*)buffer + index_write, src, length1);
    memcpy((char*)buffer, src + length1, length - length1);
    index_write = mask(index_write + length);
    return length;
  }

  std::size_t read(T *const value) {
    std::scoped_lock lock(m);
    if (value == nullptr || empty()) return 0;
    *value = buffer[index_read];
    index_read = next(index_read);
    return 1;
  }

  std::size_t write(const T value) {
    std::scoped_lock lock(m);
    std::size_t next_head = next(index_write);
    if (next_head == index_read) return 0;     // buffer full
    buffer[index_write] = value;
    index_write = next_head;
    return 1;
  }

  constexpr std::size_t size() const {
    return buffer_size - 1;
  }

private:
  inline std::size_t mask(std::size_t val) const {
    return val & buffer_mask;
  }

  inline std::size_t next(std::size_t val) const {
    return mask(val + 1);
  }
  std::mutex m;
  static const std::size_t buffer_size = S;
  static const std::size_t buffer_mask = buffer_size - 1;
  volatile T buffer[buffer_size];
  std::atomic_size_t index_write;
  std::atomic_size_t index_read;
};


struct HalSerial {
  HalSerial() { host_connected = true; }

  void begin(int32_t) {}
  void end()          {}

  int peek() {
    uint8_t value;
    return receive_buffer.peek(&value) ? value : -1;
  }

  int16_t read() {
    uint8_t value = 0;
    uint32_t ret = receive_buffer.read(&value);
    return (ret ? value : -1);
  }

  size_t write(char c) {
    if (!host_connected) return 0;
    while (!transmit_buffer.free());
    return transmit_buffer.write(c);
  }

  bool connected() { return host_connected; }

  uint16_t available() {
    return (uint16_t)receive_buffer.available();
  }

  void flush() { receive_buffer.clear(); }

  uint8_t availableForWrite() {
    return transmit_buffer.free() > 255 ? 255 : (uint8_t)transmit_buffer.free();
  }

  void flushTX() {
    if (host_connected) {
      while (transmit_buffer.available()) {
        Kernel::yield();
      }
    }
  }

  static constexpr std::size_t receive_buffer_size = 32768;
  static constexpr std::size_t transmit_buffer_size = 32768;
  RingBuffer<uint8_t, receive_buffer_size> receive_buffer;
  RingBuffer<uint8_t, transmit_buffer_size> transmit_buffer;
  volatile bool host_connected;
};

typedef Serial0Type<HalSerial> MSerialT;
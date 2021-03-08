#pragma once

#include <SDL2/SDL.h>
#include "../user_interface.h"

#include <list>
#include <deque>
#include "SPISlavePeripheral.h"

class XPT2046Device: public SPISlavePeripheral {
public:
  XPT2046Device(pin_type clk, pin_type mosi, pin_type miso, pin_type cs) : SPISlavePeripheral(clk, mosi, miso, cs) {}
  virtual ~XPT2046Device() {};

  void update() {}
  void ui_widget();
  void ui_callback();

  void onByteReceived(uint8_t _byte) override;
  void onEndTransaction() override;

  uint16_t lastClickX = 0;
  uint16_t lastClickY = 0;
  bool dirty = false;
  uint64_t touch_time;
};

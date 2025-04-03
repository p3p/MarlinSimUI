#pragma once

class Bltouch {
public:
  void enable() {
    this->disabled = false;
  }
  void disable() {
    this->disabled = true;
  }
  bool disabled = false;
};

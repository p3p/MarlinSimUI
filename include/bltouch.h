#pragma once

class Bltouch {
public:
  void enable() {
    this->disabled = false;
  }
  void disable() {
    this->disabled = true;
  }

  bool isDisabled() const {
    return this->disabled;
  }

private:
  bool disabled = false;
};

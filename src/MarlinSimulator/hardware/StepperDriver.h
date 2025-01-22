#pragma once

#include <atomic>

#include <imgui.h>

#include "Gpio.h"
#include "../virtual_printer.h"

class StepperDriver : public VirtualPrinter::Component {
public:
  struct step_event_t {
    uint64_t timestamp;
    int64_t count;
  };
  StepperDriver(pin_type enable, pin_type dir, pin_type step, std::function<void()> step_callback = [](){} ) : VirtualPrinter::Component("StepperDriver"), enable(enable), dir(dir), step(step), step_callback(step_callback) {
    Gpio::attach(step, [this](GpioEvent& ev){ this->interrupt(ev); });
  }
  ~StepperDriver() {}

  void update() override {
    // this is needed to add a stopped moving event, so the velocity can reset to zero after moves
    //TODO: minimum speed?
    if (!stopped && (Kernel::TimeControl::getTicks() - history.back().timestamp > (Kernel::TimeControl::ONE_BILLION / 100))) {
      history.push_back(history.back());
      stopped = true;
    }
  };

  void ui_widget() override {
    ImGui::Text("Steps: %ld", step_count.load());
    if (history.size() > 1) {
      int64_t steps_timedelta = (history.end() - 2)->timestamp - (history.end() - 1)->timestamp;
      int64_t steps_stepdelta = (history.end() - 2)->count - (history.end() - 1)->count;
      ImGui::Text("Steps/s: %f", steps_timedelta != 0 ? (double)(steps_stepdelta * 1000000000) / steps_timedelta : 0);
    }
  }

  void interrupt(GpioEvent& ev) {
    if (ev.pin_id == step && ev.event == ev.RISE && Gpio::get_pin_value(enable) == 0) {
      step_count += ((Gpio::get_pin_value(dir) * 2) - 1);
      history.push_back({ev.timestamp, step_count});
      if (stopped) { history.push_back({history.back().timestamp, step_count}); stopped = false; }
      if (history.back().timestamp -  history.front().timestamp > Kernel::TimeControl::nanosToTicks(Kernel::TimeControl::ONE_BILLION * 5)) history.pop_front();
      step_callback();
    }
  }

  int64_t steps() {
    return step_count;
  }

  bool stopped = true;
  std::deque<step_event_t> history {};
  std::atomic_int64_t step_count = 0;
  const pin_type enable, dir, step;
  std::function<void()> step_callback;
};

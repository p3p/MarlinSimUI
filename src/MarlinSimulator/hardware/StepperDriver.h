#pragma once

#include <atomic>

#include <imgui.h>

#include "Gpio.h"
#include "../virtual_printer.h"

// Display Marlin's stepper counts instead of the simulator's stepper counts
//#define SHOW_MARLIN_STEPPER_COUNTS

class StepperDriver : public VirtualPrinter::Component {
public:
  StepperDriver(pin_type enable, pin_type dir, pin_type step, std::function<void()> step_callback = [](){} ) : VirtualPrinter::Component("StepperDriver"), enable(enable), dir(dir), step(step), step_callback(step_callback) {
    Gpio::attach(step, [this](GpioEvent& ev){ this->interrupt(ev); });
  }
  ~StepperDriver() {}

  void ui_widget() {
    ImGui::Text("Steps: %ld",
      #ifdef SHOW_MARLIN_STEPPER_COUNTS
        m_step_count
      #else
        step_count
      #endif
      .load()
    );
  }

  void interrupt(GpioEvent& ev) {
    if (ev.pin_id == step && ev.event == ev.RISE && Gpio::get_pin_value(enable) == 0) {
      step_count += ((Gpio::get_pin_value(dir) * 2) - 1);
      step_callback();
    }
  }

  std::atomic_int64_t step_count = 0;
  int64_t steps() { return step_count; }
  void steps(int64_t s) { step_count = s; }

  #ifdef SHOW_MARLIN_STEPPER_COUNTS
    std::atomic_int64_t m_step_count = 0;
    int64_t m_steps() { return m_step_count; }
    void m_steps(int64_t s) { m_step_count = s; }
  #endif

  const pin_type enable, dir, step;
  std::function<void()> step_callback;
};

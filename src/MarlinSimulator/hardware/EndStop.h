#pragma once

#include <atomic>

#include <imgui.h>

#include "Gpio.h"
#include "../virtual_printer.h"

class EndStop : public VirtualPrinter::Component {
public:
  EndStop(pin_type endstop, bool invert_logic, std::function<bool()> triggered) : VirtualPrinter::Component("EndStop"), endstop(endstop), invert_logic(invert_logic), triggered(triggered) {
    Gpio::attach(endstop, [this](GpioEvent& ev){ this->interrupt(ev); });
  }
  ~EndStop() {}

  void ui_widget() {
    // Display the logical trigger state (not the electrical pin level)
    bool logical_triggered = triggered();
    bool display_value = manual_override.load() ? manual_trigger_state.load() : (logical_triggered && enabled.load());

    if (ImGui::Checkbox("Triggered", &display_value)) {
      manual_override = true;
      manual_trigger_state = display_value;
    }
    ImGui::SameLine();

    bool is_auto = !manual_override.load();
    if (ImGui::Checkbox("Auto", &is_auto)) {
      manual_override = !is_auto;
    }

    bool enabled_value = enabled.load();
    if (ImGui::Checkbox("Enabled", &enabled_value)) {
      enabled = enabled_value;
    }
  }

  void interrupt(GpioEvent& ev) {
    if (ev.pin_id == endstop && ev.event == ev.GET_VALUE) {
      bool logical_state;
      if (manual_override.load()) {
        logical_state = manual_trigger_state.load();
      } else {
        logical_state = triggered() && enabled.load();
      }
      // Convert logical trigger state to electrical pin level based on HIT_STATE
      bool pin_level = invert_logic ? !logical_state : logical_state;
      Gpio::set_pin_value(ev.pin_id, pin_level);
    }
  }

private:
  const pin_type endstop;
  std::atomic_bool enabled = true;
  std::atomic_bool manual_override = false;
  std::atomic_bool manual_trigger_state = false;
  bool invert_logic;
  std::function<bool()> triggered;
};
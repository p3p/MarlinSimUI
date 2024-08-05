#pragma once

#include <imgui.h>

#include "../virtual_printer.h"

class ScaraArm : public VirtualPrinter::Component {
public:
  ScaraArm() : VirtualPrinter::Component("ScaraArm") {

  }
  ~ScaraArm() {}

  void ui_widget() {
    ImGui::Checkbox("Enabled", &enabled);
  }

  bool enabled = true;
};
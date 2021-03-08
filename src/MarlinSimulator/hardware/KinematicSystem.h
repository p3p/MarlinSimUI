#pragma once

#include <memory>
#include <atomic>
#include <glm/glm.hpp>

#include <imgui.h>

#include "Gpio.h"
#include "../virtual_printer.h"
#include "StepperDriver.h"

class KinematicSystem : public VirtualPrinter::Component {
public:
  KinematicSystem(std::function<void(glm::vec4)> on_kinematic_update);
  ~KinematicSystem() {}

  void ui_widget();
  void kinematic_update();

  glm::vec4 effector_position{}, stepper_position{};
  glm::vec3 origin{};
  std::vector<std::shared_ptr<VirtualPrinter::Component>> steppers;
  std::function<void(glm::vec4)> on_kinematic_update;

};
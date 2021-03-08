#include <imgui.h>

#include "KinematicSystem.h"

#include <src/inc/MarlinConfig.h>

constexpr float steps_per_unit[] = DEFAULT_AXIS_STEPS_PER_UNIT;

KinematicSystem::KinematicSystem(std::function<void(glm::vec4)> on_kinematic_update) : VirtualPrinter::Component("Kinematic System"), on_kinematic_update(on_kinematic_update) {

  steppers.push_back(add_component<StepperDriver>("Stepper0", X_ENABLE_PIN, X_DIR_PIN, X_STEP_PIN, [this](){ this->kinematic_update(); }));
  steppers.push_back(add_component<StepperDriver>("Stepper1", Y_ENABLE_PIN, Y_DIR_PIN, Y_STEP_PIN, [this](){ this->kinematic_update(); }));
  steppers.push_back(add_component<StepperDriver>("Stepper2", Z_ENABLE_PIN, Z_DIR_PIN, Z_STEP_PIN, [this](){ this->kinematic_update(); }));
  steppers.push_back(add_component<StepperDriver>("Stepper3", E0_ENABLE_PIN, E0_DIR_PIN, E0_STEP_PIN, [this](){ this->kinematic_update(); }));

  srand(time(0));
  origin.x = (rand() % (X_MAX_POS - X_MIN_POS)) + X_MIN_POS;
  origin.y = (rand() % (Y_MAX_POS - Y_MIN_POS)) + Y_MIN_POS;
  origin.z = (rand() % (Z_MAX_POS - Z_MIN_POS)) + Z_MIN_POS;
}

void KinematicSystem::kinematic_update() {
  stepper_position = glm::vec4{
    std::static_pointer_cast<StepperDriver>(steppers[0])->steps() / steps_per_unit[0] * (((INVERT_X_DIR * 2) - 1) * -1.0),
    std::static_pointer_cast<StepperDriver>(steppers[1])->steps() / steps_per_unit[1] * (((INVERT_Y_DIR * 2) - 1) * -1.0),
    std::static_pointer_cast<StepperDriver>(steppers[2])->steps() / steps_per_unit[2] * (((INVERT_Z_DIR * 2) - 1) * -1.0),
    std::static_pointer_cast<StepperDriver>(steppers[3])->steps() / steps_per_unit[3] * (((INVERT_E_DIR * 2) - 1) * -1.0)
  };

  effector_position = glm::vec4(origin, 0.0f) + stepper_position;
  on_kinematic_update(effector_position);
}

void KinematicSystem::ui_widget() {
  auto pos = (glm::vec4(origin, 0) + stepper_position);
  auto value = pos.x;
  if (ImGui::SliderFloat("X Position(mm)", &value, X_MIN_POS, X_MAX_POS)) {
    origin.x = value - stepper_position.x;
    kinematic_update();
  }
  value = pos.y;
  if (ImGui::SliderFloat("Y Position(mm)",  &value, Y_MIN_POS, Y_MAX_POS)) {
    origin.y = value - stepper_position.y;
    kinematic_update();
  }
  value = pos.z;
  if (ImGui::SliderFloat("Z Position(mm)",  &value, Z_MIN_POS, Z_MAX_POS)) {
    origin.z = value - stepper_position.z;
    kinematic_update();
  }
}
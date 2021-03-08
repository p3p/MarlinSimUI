#include <imgui.h>


#include "hardware/StepperDriver.h"
#include "hardware/EndStop.h"
#include "hardware/Heater.h"
#include "hardware/print_bed.h"
#include "hardware/print_bed.h"
#include "hardware/bed_probe.h"
#include "hardware/ST7796Device.h"
#include "hardware/HD44780Device.h"
#include "hardware/ST7920Device.h"
#include "hardware/SDCard.h"
#include "hardware/W25QxxDevice.h"
#include "hardware/FilamentRunoutSensor.h"
#include "hardware/KinematicSystem.h"

#include "virtual_printer.h"

#include "src/inc/MarlinConfig.h"

std::function<void(glm::vec4)> VirtualPrinter::on_kinematic_update;
std::map<std::string, std::shared_ptr<VirtualPrinter::Component>> VirtualPrinter::component_map;
std::vector<std::shared_ptr<VirtualPrinter::Component>> VirtualPrinter::components;
std::shared_ptr<VirtualPrinter::Component> VirtualPrinter::root;

void VirtualPrinter::Component::ui_widgets() {
  ui_widget();
  for(auto const& it : children) {
    if (ImGui::CollapsingHeader(it->name.c_str())) {
      ImGui::Indent(6.0f);
      ImGui::PushID(it->name.c_str());
      it->ui_widgets();
      ImGui::PopID();
      ImGui::Unindent(6.0f);
    }
  }
}

void VirtualPrinter::build() {
  root = add_component<Component>("root");

  auto kinematics = root->add_component<KinematicSystem>("Kinematic System", on_kinematic_update);

  root->add_component<EndStop>("Endstop(X min)", X_MIN_PIN, X_MIN_ENDSTOP_INVERTING, [kinematics](){ return kinematics->effector_position.x <= 0; });
  root->add_component<EndStop>("Endstop(Y min)", Y_MIN_PIN, Y_MIN_ENDSTOP_INVERTING, [kinematics](){ return kinematics->effector_position.y <= 0; });
  root->add_component<EndStop>("Endstop(Z min)", Z_MIN_PIN, Z_MIN_ENDSTOP_INVERTING, [kinematics](){ return kinematics->effector_position.z <= 0; });

  auto print_bed = root->add_component<PrintBed>("Print Bed", glm::vec2{X_BED_SIZE, Y_BED_SIZE});

  #if HAS_BED_PROBE
    root->add_component<BedProbe>("Probe", Z_MIN_PROBE_PIN, glm::vec3 NOZZLE_TO_PROBE_OFFSET, kinematics->effector_position, *print_bed);
  #endif

  root->add_component<Heater>("Hotend Heater", HEATER_0_PIN, TEMP_0_PIN, heater_data{12, 3.6}, hotend_data{13, 20, 0.897}, adc_data{4700, 12});
  root->add_component<Heater>("Bed Heater", HEATER_BED_PIN, TEMP_BED_PIN, heater_data{12, 1.2}, hotend_data{325, 824, 0.897}, adc_data{4700, 12});
  #if HAS_SPI_FLASH
    root->add_component<W25QxxDevice>("SPI Flash", SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, W25QXX_CS_PIN, SPI_FLASH_SIZE);
  #endif
  #ifdef SDSUPPORT
    root->add_component<SDCard>("SD Card", SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SDSS, SD_DETECT_PIN, SD_DETECT_STATE);
  #endif
  #if ENABLED(FILAMENT_RUNOUT_SENSOR)
    root->add_component<FilamentRunoutSensor>("Filament Runout Sensor", FIL_RUNOUT1_PIN, FIL_RUNOUT_STATE);
  #endif

  #if ANY(TFT_COLOR_UI, TFT_CLASSIC_UI, TFT_LVGL_UI)
    root->add_component<ST7796Device>("ST7796Device Display", SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, TFT_CS_PIN, TOUCH_CS_PIN, TFT_DC_PIN, BEEPER_PIN, BTN_EN1, BTN_EN2, BTN_ENC, BTN_BACK, KILL_PIN);
  #elif defined(HAS_MARLINUI_HD44780)
    root->add_component<HD44780Device>("HD44780Device Display", LCD_PINS_RS, LCD_PINS_ENABLE, LCD_PINS_D4, LCD_PINS_D5, LCD_PINS_D6, LCD_PINS_D7, BEEPER_PIN, BTN_EN1, BTN_EN2, BTN_ENC, BTN_BACK, KILL_PIN);
  #elif defined(HAS_MARLINUI_U8GLIB)
    root->add_component<ST7920Device>("ST7920Device Display", LCD_PINS_D4, LCD_PINS_ENABLE, LCD_PINS_RS, BEEPER_PIN, BTN_EN1, BTN_EN2, BTN_ENC, BTN_BACK, KILL_PIN);
  #endif

  for(auto const& component : components) component->ui_init();

  kinematics->kinematic_update();
}

void VirtualPrinter::ui_widgets() {
  if (root) root->ui_widgets();
}
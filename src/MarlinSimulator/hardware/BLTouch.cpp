#include <src/inc/MarlinConfigPre.h>
#include <src/core/millis_t.h>
#include <src/feature/bltouch.h>

#include "print_bed.h"
#include "bed_probe.h"
#include "pwm_reader.h"

#include "BLTouch.h"

void BLTouchProbe::interrupt(GpioEvent& ev) {
  if (ev.pin_id == m_servo_pin) {
    auto angle = dutycycle_to_degrees(m_servo->pwm_duty);
    if (is_close_enough(angle, BLTOUCH_DEPLOY)) {
      m_probe->enabled = true;
      m_deployed = true;
    } else if (is_close_enough(angle, BLTOUCH_STOW)) {
      m_probe->enabled = false;
      m_deployed = false;
    }
  }
}

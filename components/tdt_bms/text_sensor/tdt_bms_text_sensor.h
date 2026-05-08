#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "../tdt_bms.h"

namespace esphome {
namespace tdt_bms {

// Listener that publishes one pack's human-readable strings: battery operating
// mode, cell chemistry, and comma-separated lists of currently-active
// protections / warnings / faults.
class TdtBmsPackTextSensor : public TdtBmsListener {
 public:
  explicit TdtBmsPackTextSensor(uint8_t pack) : pack_(pack) {}

  void on_status_data(uint8_t pack, const StatusFrame &data) override;
  void on_pack_offline(uint8_t pack) override;
  void dump_config() override;

  void set_battery_mode_text_sensor(text_sensor::TextSensor *s) { battery_mode_ = s; }
  void set_chemistry_text_sensor(text_sensor::TextSensor *s) { chemistry_ = s; }
  void set_active_protections_text_sensor(text_sensor::TextSensor *s) { active_protections_ = s; }
  void set_active_warnings_text_sensor(text_sensor::TextSensor *s) { active_warnings_ = s; }
  void set_active_faults_text_sensor(text_sensor::TextSensor *s) { active_faults_ = s; }

 protected:
  uint8_t pack_;
  text_sensor::TextSensor *battery_mode_{nullptr};
  text_sensor::TextSensor *chemistry_{nullptr};
  text_sensor::TextSensor *active_protections_{nullptr};
  text_sensor::TextSensor *active_warnings_{nullptr};
  text_sensor::TextSensor *active_faults_{nullptr};
};

}  // namespace tdt_bms
}  // namespace esphome

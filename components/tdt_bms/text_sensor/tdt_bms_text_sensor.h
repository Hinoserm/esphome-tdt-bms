#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "../tdt_bms.h"

namespace esphome {
namespace tdt_bms {

// Listener that publishes one pack's human-readable strings: battery operating
// mode, cell chemistry, comma-separated lists of currently-active
// protections / warnings / faults, and firmware version.
class TdtBmsPackTextSensor : public TdtBmsListener {
 public:
  explicit TdtBmsPackTextSensor(uint8_t pack) : pack_(pack) {}

  uint8_t get_pack() const override { return this->pack_; }
  bool wants_analog() const override { return this->balancing_cells_ != nullptr; }
  bool wants_status() const override {
    return this->battery_mode_ || this->chemistry_ ||
           this->active_protections_ || this->active_warnings_ ||
           this->active_faults_;
  }

  void on_analog_data(uint8_t pack, const AnalogFrame &data) override;
  void on_status_data(uint8_t pack, const StatusFrame &data) override;
  void on_info_data(uint8_t pack, const InfoFrame &data) override;
  void on_pack_offline(uint8_t pack) override;
  void dump_config() override;

  void set_battery_mode_text_sensor(text_sensor::TextSensor *s) { battery_mode_ = s; }
  void set_chemistry_text_sensor(text_sensor::TextSensor *s) { chemistry_ = s; }
  void set_active_protections_text_sensor(text_sensor::TextSensor *s) { active_protections_ = s; }
  void set_active_warnings_text_sensor(text_sensor::TextSensor *s) { active_warnings_ = s; }
  void set_active_faults_text_sensor(text_sensor::TextSensor *s) { active_faults_ = s; }
  void set_firmware_version_text_sensor(text_sensor::TextSensor *s) { firmware_version_ = s; }
  void set_balancing_cells_text_sensor(text_sensor::TextSensor *s) { balancing_cells_ = s; }
  void set_bms_mode_flags_text_sensor(text_sensor::TextSensor *s) { bms_mode_flags_ = s; }

 protected:
  uint8_t pack_;
  text_sensor::TextSensor *battery_mode_{nullptr};
  text_sensor::TextSensor *chemistry_{nullptr};
  text_sensor::TextSensor *active_protections_{nullptr};
  text_sensor::TextSensor *active_warnings_{nullptr};
  text_sensor::TextSensor *active_faults_{nullptr};
  text_sensor::TextSensor *firmware_version_{nullptr};
  text_sensor::TextSensor *balancing_cells_{nullptr};
  text_sensor::TextSensor *bms_mode_flags_{nullptr};
};

}  // namespace tdt_bms
}  // namespace esphome

#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../tdt_bms.h"

namespace esphome {
namespace tdt_bms {

// Listener that publishes one pack's binary state (MOSFET on/off, fault
// rolled-up flags, balancer activity, online/offline).
class TdtBmsPackBinarySensor : public TdtBmsListener {
 public:
  explicit TdtBmsPackBinarySensor(uint8_t pack) : pack_(pack) {}

  uint8_t get_pack() const override { return this->pack_; }
  bool wants_analog() const override;
  bool wants_status() const override { return this->any_status_sensor_set_(); }

  void on_analog_data(uint8_t pack, const AnalogFrame &data) override;
  void on_status_data(uint8_t pack, const StatusFrame &data) override;
  void on_pack_online(uint8_t pack) override;
  void on_pack_offline(uint8_t pack) override;
  void dump_config() override;

  void set_charging_mosfet_binary_sensor(binary_sensor::BinarySensor *s) { charging_mosfet_ = s; }
  void set_discharging_mosfet_binary_sensor(binary_sensor::BinarySensor *s) { discharging_mosfet_ = s; }
  void set_heater_binary_sensor(binary_sensor::BinarySensor *s) { heater_ = s; }
  void set_bms_sleeping_binary_sensor(binary_sensor::BinarySensor *s) { bms_sleeping_ = s; }
  void set_protection_active_binary_sensor(binary_sensor::BinarySensor *s) { protection_active_ = s; }
  void set_warning_active_binary_sensor(binary_sensor::BinarySensor *s) { warning_active_ = s; }
  void set_fault_active_binary_sensor(binary_sensor::BinarySensor *s) { fault_active_ = s; }
  void set_low_soc_warning_binary_sensor(binary_sensor::BinarySensor *s) { low_soc_warning_ = s; }
  void set_balancing_active_binary_sensor(binary_sensor::BinarySensor *s) { balancing_active_ = s; }
  void set_online_binary_sensor(binary_sensor::BinarySensor *s) { online_ = s; }

  // Per-cell balancing state: true when the corresponding bit is set in the
  // BMS's balance_bitmap (cell index is 0-based here, 1-based in YAML keys).
  void set_cell_balancing_binary_sensor(uint8_t i, binary_sensor::BinarySensor *s) {
    if (i < MAX_CELLS) cell_balancing_sensors_[i] = s;
  }

 protected:
  bool any_status_sensor_set_() const;
  bool any_cell_balancing_set_() const;

  uint8_t pack_;
  binary_sensor::BinarySensor *charging_mosfet_{nullptr};
  binary_sensor::BinarySensor *discharging_mosfet_{nullptr};
  binary_sensor::BinarySensor *heater_{nullptr};
  binary_sensor::BinarySensor *bms_sleeping_{nullptr};
  binary_sensor::BinarySensor *protection_active_{nullptr};
  binary_sensor::BinarySensor *warning_active_{nullptr};
  binary_sensor::BinarySensor *fault_active_{nullptr};
  binary_sensor::BinarySensor *low_soc_warning_{nullptr};
  binary_sensor::BinarySensor *balancing_active_{nullptr};
  binary_sensor::BinarySensor *online_{nullptr};
  binary_sensor::BinarySensor *cell_balancing_sensors_[MAX_CELLS]{nullptr};
};

}  // namespace tdt_bms
}  // namespace esphome

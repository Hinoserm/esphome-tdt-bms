#pragma once

#include "esphome/components/sensor/sensor.h"
#include "../tdt_bms.h"

namespace esphome {
namespace tdt_bms {

// Listener that publishes one pack's analog measurements to a configurable set
// of `sensor::Sensor` outputs. Constructed with a 1-based pack address; ignores
// broadcasts intended for other packs.
class TdtBmsPackSensor : public TdtBmsListener {
 public:
  explicit TdtBmsPackSensor(uint8_t pack) : pack_(pack) {}

  uint8_t get_pack() const override { return this->pack_; }
  bool wants_analog() const override { return this->any_sensor_set_(); }

  void on_analog_data(uint8_t pack, const AnalogFrame &data) override;
  void on_pack_offline(uint8_t pack) override;
  void dump_config() override;

  void set_cell_voltage_sensor(uint8_t i, sensor::Sensor *s) {
    if (i < MAX_CELLS) cell_voltage_sensors_[i] = s;
  }
  void set_temperature_sensor(uint8_t i, sensor::Sensor *s) {
    if (i < MAX_TEMPS) temperature_sensors_[i] = s;
  }

  SUB_SENSOR(pack_voltage)
  SUB_SENSOR(pack_current)
  SUB_SENSOR(pack_power)
  SUB_SENSOR(cpu_voltage)
  SUB_SENSOR(remaining_capacity)
  SUB_SENSOR(full_capacity)
  SUB_SENSOR(design_capacity)
  SUB_SENSOR(cycle_count)
  SUB_SENSOR(state_of_charge)
  SUB_SENSOR(state_of_health)
  SUB_SENSOR(insulation_resistance)
  SUB_SENSOR(bms_self_consumption)
  SUB_SENSOR(min_cell_voltage)
  SUB_SENSOR(max_cell_voltage)
  SUB_SENSOR(cell_voltage_delta)
  SUB_SENSOR(avg_cell_voltage)
  SUB_SENSOR(temperature_high)
  SUB_SENSOR(temperature_low)
  SUB_SENSOR(active_balance_current)
  SUB_SENSOR(active_balance_target_voltage)
  SUB_SENSOR(emergency_mode_timer)
  SUB_SENSOR(equipment_voltage)

 protected:
  bool any_sensor_set_() const;

  uint8_t pack_;
  sensor::Sensor *cell_voltage_sensors_[MAX_CELLS]{nullptr};
  sensor::Sensor *temperature_sensors_[MAX_TEMPS]{nullptr};
};

}  // namespace tdt_bms
}  // namespace esphome

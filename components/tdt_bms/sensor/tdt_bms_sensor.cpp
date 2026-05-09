#include "tdt_bms_sensor.h"

#include "esphome/core/log.h"

#include <cmath>

namespace esphome {
namespace tdt_bms {

static const char *const TAG = "tdt_bms.sensor";

static inline void publish(sensor::Sensor *s, float v) {
  if (s != nullptr) s->publish_state(v);
}

static inline void publish_nan(sensor::Sensor *s) {
  if (s != nullptr) s->publish_state(NAN);
}

bool TdtBmsPackSensor::any_sensor_set_() const {
  if (this->pack_voltage_sensor_ || this->pack_current_sensor_ ||
      this->pack_power_sensor_ || this->cpu_voltage_sensor_ ||
      this->remaining_capacity_sensor_ || this->full_capacity_sensor_ ||
      this->design_capacity_sensor_ || this->cycle_count_sensor_ ||
      this->state_of_charge_sensor_ || this->state_of_health_sensor_ ||
      this->insulation_resistance_sensor_ || this->bms_self_consumption_sensor_ ||
      this->min_cell_voltage_sensor_ || this->max_cell_voltage_sensor_ ||
      this->cell_voltage_delta_sensor_ || this->avg_cell_voltage_sensor_ ||
      this->temperature_high_sensor_ || this->temperature_low_sensor_ ||
      this->active_balance_current_sensor_ || this->active_balance_target_voltage_sensor_ ||
      this->emergency_mode_timer_sensor_ || this->equipment_voltage_sensor_) {
    return true;
  }
  for (auto *s : this->cell_voltage_sensors_) if (s) return true;
  for (auto *s : this->temperature_sensors_) if (s) return true;
  return false;
}

void TdtBmsPackSensor::on_analog_data(uint8_t pack, const AnalogFrame &data) {
  if (pack != this->pack_) return;

  publish(this->pack_voltage_sensor_, data.pack_voltage_V);
  publish(this->pack_current_sensor_, data.pack_current_A);
  publish(this->pack_power_sensor_, data.pack_voltage_V * data.pack_current_A);
  publish(this->cpu_voltage_sensor_, data.cpu_voltage_V);

  publish(this->remaining_capacity_sensor_, data.remaining_capacity_Ah);
  publish(this->full_capacity_sensor_, data.full_capacity_Ah);
  publish(this->design_capacity_sensor_, data.design_capacity_Ah);

  publish(this->cycle_count_sensor_, float(data.cycle_count));
  publish(this->state_of_charge_sensor_, float(data.soc_pct));
  publish(this->state_of_health_sensor_, float(data.soh_pct));

  publish(this->insulation_resistance_sensor_, float(data.insulation_kohm));
  publish(this->bms_self_consumption_sensor_, float(data.bms_self_consumption_mA));

  publish(this->min_cell_voltage_sensor_, float(data.min_cell_mV) * 0.001f);
  publish(this->max_cell_voltage_sensor_, float(data.max_cell_mV) * 0.001f);
  publish(this->cell_voltage_delta_sensor_, float(data.delta_cell_mV) * 0.001f);
  publish(this->avg_cell_voltage_sensor_, float(data.avg_cell_mV) * 0.001f);

  for (uint8_t i = 0; i < data.bat_num && i < MAX_CELLS; i++) {
    publish(this->cell_voltage_sensors_[i], float(data.cell_mV[i]) * 0.001f);
  }
  for (uint8_t i = 0; i < data.temp_num && i < MAX_TEMPS; i++) {
    publish(this->temperature_sensors_[i], data.temp_C[i]);
  }
  if (data.temp_num > 0) {
    publish(this->temperature_high_sensor_, data.temperature_high_C);
    publish(this->temperature_low_sensor_, data.temperature_low_C);
  }

  // Active-balance fields are only meaningful when BMS_MODE_F bit 1 is set.
  // When invalid (firmware doesn't expose them) we leave the sensors at NaN
  // so HA shows them as unavailable rather than as a stale 0.
  if (data.active_balance_valid) {
    publish(this->active_balance_current_sensor_, data.active_balance_current_A);
    publish(this->active_balance_target_voltage_sensor_, data.active_balance_target_V);
    publish(this->emergency_mode_timer_sensor_, float(data.emergency_mode_minutes));
    publish(this->equipment_voltage_sensor_, data.equipment_voltage_V);
  } else {
    publish_nan(this->active_balance_current_sensor_);
    publish_nan(this->active_balance_target_voltage_sensor_);
    publish_nan(this->emergency_mode_timer_sensor_);
    publish_nan(this->equipment_voltage_sensor_);
  }
}

void TdtBmsPackSensor::on_pack_offline(uint8_t pack) {
  if (pack != this->pack_) return;

  publish_nan(this->pack_voltage_sensor_);
  publish_nan(this->pack_current_sensor_);
  publish_nan(this->pack_power_sensor_);
  publish_nan(this->cpu_voltage_sensor_);
  publish_nan(this->remaining_capacity_sensor_);
  publish_nan(this->full_capacity_sensor_);
  publish_nan(this->design_capacity_sensor_);
  publish_nan(this->cycle_count_sensor_);
  publish_nan(this->state_of_charge_sensor_);
  publish_nan(this->state_of_health_sensor_);
  publish_nan(this->insulation_resistance_sensor_);
  publish_nan(this->bms_self_consumption_sensor_);
  publish_nan(this->min_cell_voltage_sensor_);
  publish_nan(this->max_cell_voltage_sensor_);
  publish_nan(this->cell_voltage_delta_sensor_);
  publish_nan(this->avg_cell_voltage_sensor_);
  publish_nan(this->temperature_high_sensor_);
  publish_nan(this->temperature_low_sensor_);
  publish_nan(this->active_balance_current_sensor_);
  publish_nan(this->active_balance_target_voltage_sensor_);
  publish_nan(this->emergency_mode_timer_sensor_);
  publish_nan(this->equipment_voltage_sensor_);
  for (auto *s : this->cell_voltage_sensors_) publish_nan(s);
  for (auto *s : this->temperature_sensors_) publish_nan(s);
}

void TdtBmsPackSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Pack %u sensors:", this->pack_);
  LOG_SENSOR("  ", "Pack Voltage", this->pack_voltage_sensor_);
  LOG_SENSOR("  ", "Pack Current", this->pack_current_sensor_);
  LOG_SENSOR("  ", "Pack Power", this->pack_power_sensor_);
  LOG_SENSOR("  ", "CPU Voltage", this->cpu_voltage_sensor_);
  LOG_SENSOR("  ", "Remaining Capacity", this->remaining_capacity_sensor_);
  LOG_SENSOR("  ", "Full Capacity", this->full_capacity_sensor_);
  LOG_SENSOR("  ", "Design Capacity", this->design_capacity_sensor_);
  LOG_SENSOR("  ", "Cycle Count", this->cycle_count_sensor_);
  LOG_SENSOR("  ", "State of Charge", this->state_of_charge_sensor_);
  LOG_SENSOR("  ", "State of Health", this->state_of_health_sensor_);
  LOG_SENSOR("  ", "Insulation Resistance", this->insulation_resistance_sensor_);
  LOG_SENSOR("  ", "BMS Self-Consumption", this->bms_self_consumption_sensor_);
  LOG_SENSOR("  ", "Min Cell Voltage", this->min_cell_voltage_sensor_);
  LOG_SENSOR("  ", "Max Cell Voltage", this->max_cell_voltage_sensor_);
  LOG_SENSOR("  ", "Cell Voltage Delta", this->cell_voltage_delta_sensor_);
  LOG_SENSOR("  ", "Average Cell Voltage", this->avg_cell_voltage_sensor_);
  LOG_SENSOR("  ", "Temperature High", this->temperature_high_sensor_);
  LOG_SENSOR("  ", "Temperature Low", this->temperature_low_sensor_);
  LOG_SENSOR("  ", "Active Balance Current", this->active_balance_current_sensor_);
  LOG_SENSOR("  ", "Active Balance Target Voltage", this->active_balance_target_voltage_sensor_);
  LOG_SENSOR("  ", "Emergency Mode Timer", this->emergency_mode_timer_sensor_);
  LOG_SENSOR("  ", "Equipment Voltage", this->equipment_voltage_sensor_);
  for (uint8_t i = 0; i < MAX_CELLS; i++) {
    if (this->cell_voltage_sensors_[i] != nullptr) {
      LOG_SENSOR("  ", "Cell Voltage", this->cell_voltage_sensors_[i]);
    }
  }
  for (uint8_t i = 0; i < MAX_TEMPS; i++) {
    if (this->temperature_sensors_[i] != nullptr) {
      LOG_SENSOR("  ", "Temperature", this->temperature_sensors_[i]);
    }
  }
}

}  // namespace tdt_bms
}  // namespace esphome

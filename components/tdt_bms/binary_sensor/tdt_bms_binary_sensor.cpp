#include "tdt_bms_binary_sensor.h"

#include "esphome/core/log.h"

namespace esphome {
namespace tdt_bms {

static const char *const TAG = "tdt_bms.binary_sensor";

static inline void publish(binary_sensor::BinarySensor *s, bool state) {
  if (s != nullptr) s->publish_state(state);
}

bool TdtBmsPackBinarySensor::any_status_sensor_set_() const {
  return this->charging_mosfet_ || this->discharging_mosfet_ ||
         this->heater_ || this->bms_sleeping_ ||
         this->protection_active_ || this->warning_active_ ||
         this->fault_active_ || this->low_soc_warning_;
}

bool TdtBmsPackBinarySensor::wants_analog() const {
  // balancing_active is derived from analog data.
  if (this->balancing_active_ != nullptr) return true;
  // Online tracking needs at least one polled command for the pack. If the
  // user only wired `online:` on this listener and nothing status-fed, fall
  // back to wanting analog so the pack still gets polled.
  if (this->online_ != nullptr && !this->any_status_sensor_set_()) return true;
  return false;
}

void TdtBmsPackBinarySensor::on_analog_data(uint8_t pack, const AnalogFrame &data) {
  if (pack != this->pack_) return;
  publish(this->balancing_active_, data.balance_bitmap != 0);
}

void TdtBmsPackBinarySensor::on_status_data(uint8_t pack, const StatusFrame &data) {
  if (pack != this->pack_) return;

  // Status3: bit 1 = charge MOSFET on, bit 2 = discharge MOSFET on, bit 7 = heater
  publish(this->charging_mosfet_, (data.status3 & 0x02) != 0);
  publish(this->discharging_mosfet_, (data.status3 & 0x04) != 0);
  publish(this->heater_, (data.status3 & 0x80) != 0);

  publish(this->bms_sleeping_, data.sleep_flag != 0);
  publish(this->low_soc_warning_, data.soc_low_warn != 0);

  // Status1/Status2: any tripped protection (OVP, UVP, OCP, OTP, UTP, SCP, soft-start).
  publish(this->protection_active_, (data.status1 != 0) || (data.status2 != 0));

  // Warning1/Warning2: lower-threshold alarms.
  publish(this->warning_active_, (data.warning1 != 0) || (data.warning2 != 0));

  // Status5: hardware faults (MOSFET fault, sample errors).
  publish(this->fault_active_, data.status5 != 0);
}

void TdtBmsPackBinarySensor::on_pack_online(uint8_t pack) {
  if (pack != this->pack_) return;
  publish(this->online_, true);
}

void TdtBmsPackBinarySensor::on_pack_offline(uint8_t pack) {
  if (pack != this->pack_) return;
  publish(this->online_, false);
  // Conservative: clear any "active" indicators since we can't trust stale state.
  publish(this->protection_active_, false);
  publish(this->warning_active_, false);
  publish(this->fault_active_, false);
  publish(this->low_soc_warning_, false);
  publish(this->balancing_active_, false);
}

void TdtBmsPackBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Pack %u binary sensors:", this->pack_);
  LOG_BINARY_SENSOR("  ", "Charging MOSFET", this->charging_mosfet_);
  LOG_BINARY_SENSOR("  ", "Discharging MOSFET", this->discharging_mosfet_);
  LOG_BINARY_SENSOR("  ", "Heater", this->heater_);
  LOG_BINARY_SENSOR("  ", "BMS Sleeping", this->bms_sleeping_);
  LOG_BINARY_SENSOR("  ", "Protection Active", this->protection_active_);
  LOG_BINARY_SENSOR("  ", "Warning Active", this->warning_active_);
  LOG_BINARY_SENSOR("  ", "Fault Active", this->fault_active_);
  LOG_BINARY_SENSOR("  ", "Low SOC Warning", this->low_soc_warning_);
  LOG_BINARY_SENSOR("  ", "Balancing Active", this->balancing_active_);
  LOG_BINARY_SENSOR("  ", "Online", this->online_);
}

}  // namespace tdt_bms
}  // namespace esphome

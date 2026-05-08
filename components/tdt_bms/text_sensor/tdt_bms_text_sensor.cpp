#include "tdt_bms_text_sensor.h"

#include "esphome/core/log.h"

#include <string>

namespace esphome {
namespace tdt_bms {

static const char *const TAG = "tdt_bms.text_sensor";

static inline void publish(text_sensor::TextSensor *s, const std::string &v) {
  if (s != nullptr) s->publish_state(v);
}

// Bit-name tables for active_protections / active_warnings / active_faults.
// Each row is a tuple of (mask byte index 0/1, bit, name); we assemble a
// comma-separated list of all active flags.
struct FlagBit {
  uint8_t byte_idx;
  uint8_t bit;
  const char *name;
};

static const FlagBit PROTECTION_BITS[] = {
    // Status1 — primary protections
    {0, 0, "cell_OVP"},   {0, 1, "cell_UVP"},
    {0, 2, "pack_OVP"},   {0, 3, "pack_UVP"},
    {0, 4, "chg_OCP"},    {0, 5, "dsg_OCP"},
    {0, 6, "SCP"},
    // Status2 — temperature / environment protections
    {1, 0, "chg_OTP"},    {1, 1, "dsg_OTP"},
    {1, 2, "chg_UTP"},    {1, 3, "dsg_UTP"},
    {1, 4, "MOS_OTP"},    {1, 5, "ENV_OTP"},
    {1, 6, "ENV_UTP"},    {1, 7, "softstart_fail"},
};

static const FlagBit WARNING_BITS[] = {
    // Warning1 — lower-threshold cell/pack alarms
    {0, 0, "cell_OV"},    {0, 1, "cell_UV"},
    {0, 2, "pack_OV"},    {0, 3, "pack_UV"},
    {0, 4, "chg_OC"},     {0, 5, "dsg_OC"},
    // Warning2 — temperature alarms
    {1, 0, "chg_OT"},     {1, 1, "dsg_OT"},
    {1, 2, "chg_UT"},     {1, 3, "dsg_UT"},
    {1, 4, "ENV_OT"},     {1, 5, "ENV_UT"},
    {1, 6, "MOS_OT"},     {1, 7, "softstart_warn"},
};

static const FlagBit FAULT_BITS[] = {
    {0, 0, "chg_mos_fault"},
    {0, 1, "dsg_mos_fault"},
    {0, 2, "temp_sensor_fault"},
    {0, 4, "voltage_sample_fault"},
};

template<size_t N>
static std::string join_flags(const FlagBit (&table)[N], uint8_t b0, uint8_t b1) {
  std::string out;
  for (const auto &f : table) {
    uint8_t v = (f.byte_idx == 0) ? b0 : b1;
    if (v & (1u << f.bit)) {
      if (!out.empty()) out += ",";
      out += f.name;
    }
  }
  return out;
}

static const char *battery_mode_text(uint8_t v) {
  switch (v) {
    case 1: return "Charging";
    case 2: return "Discharging";
    default: return "Idle";
  }
}

static const char *chemistry_text(uint8_t v) {
  switch (v) {
    case 1: return "Li-ion NMC";
    case 2: return "LiFePO4";
    case 3: return "LTO";
    case 4: return "Other";
    default: return "Unknown";
  }
}

void TdtBmsPackTextSensor::on_status_data(uint8_t pack, const StatusFrame &data) {
  if (pack != this->pack_) return;

  publish(this->battery_mode_, battery_mode_text(data.battery_mode));
  publish(this->chemistry_, chemistry_text(data.bat_type));
  publish(this->active_protections_, join_flags(PROTECTION_BITS, data.status1, data.status2));
  publish(this->active_warnings_, join_flags(WARNING_BITS, data.warning1, data.warning2));
  publish(this->active_faults_, join_flags(FAULT_BITS, data.status5, 0));
}

void TdtBmsPackTextSensor::on_pack_offline(uint8_t pack) {
  if (pack != this->pack_) return;
  publish(this->battery_mode_, "Offline");
  publish(this->chemistry_, "");
  publish(this->active_protections_, "");
  publish(this->active_warnings_, "");
  publish(this->active_faults_, "");
}

void TdtBmsPackTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "Pack %u text sensors:", this->pack_);
  LOG_TEXT_SENSOR("  ", "Battery Mode", this->battery_mode_);
  LOG_TEXT_SENSOR("  ", "Chemistry", this->chemistry_);
  LOG_TEXT_SENSOR("  ", "Active Protections", this->active_protections_);
  LOG_TEXT_SENSOR("  ", "Active Warnings", this->active_warnings_);
  LOG_TEXT_SENSOR("  ", "Active Faults", this->active_faults_);
}

}  // namespace tdt_bms
}  // namespace esphome

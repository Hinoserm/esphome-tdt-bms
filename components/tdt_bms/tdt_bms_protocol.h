#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace esphome {
namespace tdt_bms {

// Wire-protocol constants.
static constexpr uint8_t SOI_REQUEST = 0xBC;
static constexpr uint8_t SOI_RESPONSE = 0x7E;
static constexpr uint8_t EOI = 0x0D;

static constexpr uint8_t LOCAL_ADDR = 0x01;  // we always speak as the master/originator

static constexpr uint8_t CID1 = 0x46;        // LiFePO4 family

static constexpr uint8_t CID2_ANALOG = 0x42;
static constexpr uint8_t CID2_STATUS = 0x44;
static constexpr uint8_t CID2_INFO = 0xC1;

static constexpr uint8_t RTN_OK_DATA = 0x4F;
static constexpr uint8_t RTN_OK_EMPTY = 0x00;
static constexpr uint8_t RTN_CHKSUM_ERR = 0x02;
static constexpr uint8_t RTN_LCHKSUM_ERR = 0x03;

// Hard caps for parser output. The wire protocol can carry more, but these match
// the documented hardware limits and bound stack-allocated arrays.
static constexpr uint8_t MAX_CELLS = 16;
static constexpr uint8_t MAX_TEMPS = 6;

// Maximum response size we accept on the wire (info length is capped to 12 bits ≈ 4095 chars,
// but real frames are well under 256 bytes).
static constexpr size_t MAX_FRAME_SIZE = 512;

// LCHKSUM nibble check on a 12-bit info-length value, returns the 4-bit checksum.
uint8_t lchksum(uint16_t info_chars);

// Encode a 12-bit info length into a 16-bit ASCII-hex word with the LCHKSUM in the upper nibble.
uint16_t encode_length(uint16_t info_chars);

// Two's-complement 16-bit checksum over the body bytes (everything between SOI and CHKSUM).
uint16_t frame_checksum(const uint8_t *body, size_t len);

// Build a multi-pack request frame addressed to `target_addr`. Output is appended to `out`,
// which is cleared first. `info` is the raw ASCII info field (e.g. the bytes "01" for the
// analog/status/probe subcommand) and may be empty.
void build_request(std::vector<uint8_t> &out, uint8_t target_addr, uint8_t cid2,
                   const uint8_t *info, size_t info_len);

// Decoded view over a complete response frame. `info` points into the original frame buffer
// and is valid only as long as that buffer lives.
struct ParsedHeader {
  uint8_t target_addr;   // pack that responded (echo of request target_addr)
  uint8_t master_addr;
  uint8_t rtn;
  uint8_t cid2;
  uint16_t info_chars;   // info length in ASCII chars (lower 12 bits of LENGTH word)
  const uint8_t *info;   // pointer into source frame at the start of the info field
};

// Parse header + checksum of a response frame. Returns true on a structurally valid frame.
bool parse_header(const uint8_t *frame, size_t frame_len, ParsedHeader &out);

// Decoded analog (CID2=0x42) data for one pack.
struct AnalogFrame {
  uint8_t bat_num;
  uint16_t cell_mV[MAX_CELLS];
  uint8_t temp_num;
  float temp_C[MAX_TEMPS];
  float pack_current_A;
  float pack_voltage_V;
  float remaining_capacity_Ah;
  float full_capacity_Ah;
  float design_capacity_Ah;
  uint16_t cycle_count;
  float cpu_voltage_V;
  uint8_t soc_pct;
  uint8_t soh_pct;
  uint32_t insulation_kohm;   // up to ~655 MΩ on open-circuit reads — uint16_t overflows
  int16_t bms_self_consumption_mA;
  uint32_t balance_bitmap;

  // Derived cell statistics, populated by the parser for convenience.
  uint16_t min_cell_mV;
  uint16_t max_cell_mV;
  uint16_t avg_cell_mV;
  uint16_t delta_cell_mV;
};

// Decoded alarm/status (CID2=0x44) data for one pack.
struct StatusFrame {
  uint8_t bat_num;

  // Raw protection / warning / fault byte registers.
  uint8_t status1;     // Status1: cell/pack OVP/UVP, charge/discharge OCP, SCP
  uint8_t status2;     // Status2: charge/discharge OTP/UTP, MOS_OTP, ENV_OTP/UTP, soft-start
  uint8_t status3;     // Status3: MOSFET on/off, heater
  uint8_t status4;     // Status4: switches, SOC LED config
  uint8_t status5;     // Status5: hardware faults
  uint8_t warning1;    // Warning1: lower-threshold cell/pack OV/UV/OC
  uint8_t warning2;    // Warning2: lower-threshold temperature alarms
  uint8_t battery_mode;
  uint8_t sleep_flag;
  uint8_t bat_type;
  uint8_t soc_low_warn;
};

// Parse the analog info field. `bms_mode_f` (read separately via CID2=0xC1) gates the
// current scale: bit 0 clear → 0.01 A, bit 0 set → 0.1 A. Pass 0 if unknown (defaults to 0.01).
bool parse_analog(const uint8_t *info, size_t info_chars, uint16_t bms_mode_f, AnalogFrame &out);

// Parse the alarm/status info field.
bool parse_status(const uint8_t *info, size_t info_chars, StatusFrame &out);

}  // namespace tdt_bms
}  // namespace esphome

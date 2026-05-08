#include "tdt_bms_protocol.h"

namespace esphome {
namespace tdt_bms {

// ASCII-hex helpers. The wire format encodes every protocol byte as two
// uppercase ASCII hex characters; these helpers convert between that on-wire
// representation and host-side integers.

static inline uint8_t hex_nibble(uint8_t c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return 0xFF;
}

static inline char nibble_hex(uint8_t v) {
  return v < 10 ? char('0' + v) : char('A' + (v - 10));
}

static inline uint8_t read_byte(const uint8_t *info, size_t off) {
  return uint8_t((hex_nibble(info[off]) << 4) | hex_nibble(info[off + 1]));
}

static inline uint16_t read_word(const uint8_t *info, size_t off) {
  return uint16_t((uint16_t(read_byte(info, off)) << 8) | read_byte(info, off + 2));
}

static inline int16_t read_sword(const uint8_t *info, size_t off) {
  uint16_t v = read_word(info, off);
  return (v & 0x8000) ? int16_t(int32_t(v) - 0x10000) : int16_t(v);
}

static inline void append_hex_byte(std::vector<uint8_t> &out, uint8_t b) {
  out.push_back(uint8_t(nibble_hex(b >> 4)));
  out.push_back(uint8_t(nibble_hex(b & 0xF)));
}

static inline void append_hex_word(std::vector<uint8_t> &out, uint16_t w) {
  append_hex_byte(out, uint8_t(w >> 8));
  append_hex_byte(out, uint8_t(w & 0xFF));
}

uint8_t lchksum(uint16_t info_chars) {
  uint8_t s = uint8_t((info_chars & 0xF) + ((info_chars >> 4) & 0xF) + ((info_chars >> 8) & 0xF));
  return uint8_t(((~s) + 1) & 0xF);
}

uint16_t encode_length(uint16_t info_chars) {
  return uint16_t((uint16_t(lchksum(info_chars)) << 12) | (info_chars & 0xFFF));
}

uint16_t frame_checksum(const uint8_t *body, size_t len) {
  uint32_t s = 0;
  for (size_t i = 0; i < len; i++) s += body[i];
  s &= 0xFFFF;
  return uint16_t(((~s) + 1) & 0xFFFF);
}

void build_request(std::vector<uint8_t> &out, uint8_t target_addr, uint8_t cid2,
                   const uint8_t *info, size_t info_len) {
  out.clear();
  out.reserve(1 + 12 + info_len + 4 + 1);

  out.push_back(SOI_REQUEST);

  const size_t body_start = out.size();
  append_hex_byte(out, target_addr);
  append_hex_byte(out, LOCAL_ADDR);
  append_hex_byte(out, CID1);
  append_hex_byte(out, cid2);
  append_hex_word(out, encode_length(uint16_t(info_len)));
  for (size_t i = 0; i < info_len; i++) out.push_back(info[i]);

  const uint16_t chk = frame_checksum(out.data() + body_start, out.size() - body_start);
  append_hex_word(out, chk);

  out.push_back(EOI);
}

bool parse_header(const uint8_t *frame, size_t frame_len, ParsedHeader &out) {
  // Minimum frame: SOI + 12 header chars + 4 chk chars + EOI = 18 bytes.
  if (frame_len < 18) return false;
  if (frame[0] != SOI_RESPONSE) return false;
  if (frame[frame_len - 1] != EOI) return false;

  // Body excludes SOI, the last 4 chksum chars, and the EOI.
  const uint8_t *body = frame + 1;
  size_t body_len = frame_len - 1 - 4 - 1;

  uint16_t computed = frame_checksum(body, body_len);
  uint16_t received = uint16_t((uint16_t(read_byte(frame, frame_len - 5)) << 8) |
                               read_byte(frame, frame_len - 3));
  if (computed != received) return false;

  out.target_addr = read_byte(frame, 1);
  out.master_addr = read_byte(frame, 3);
  out.rtn         = read_byte(frame, 5);
  out.cid2        = read_byte(frame, 7);
  uint16_t length_word = read_word(frame, 9);
  out.info_chars = uint16_t(length_word & 0xFFF);
  out.info       = frame + 13;

  // Trust the wire frame size if the reported length disagrees. The body
  // minus its 12-char header is the actual info field length.
  if (size_t(13) + out.info_chars + 4 + 1 > frame_len) {
    out.info_chars = body_len > 12 ? uint16_t(body_len - 12) : uint16_t(0);
  }
  return true;
}

bool parse_analog(const uint8_t *info, size_t info_chars, uint16_t bms_mode_f, AnalogFrame &out) {
  if (info_chars < 6) return false;

  size_t o = 0;
  // INFOFLAG and pack_info_byte are not consumed; advance past them.
  o += 2;  // INFOFLAG
  o += 2;  // pack_info_byte

  uint8_t bat_num = read_byte(info, o); o += 2;
  if (bat_num > MAX_CELLS) return false;
  out.bat_num = bat_num;

  if (o + size_t(bat_num) * 4 > info_chars) return false;
  uint16_t mn = 0xFFFF, mx = 0;
  uint32_t avg_sum = 0;
  for (uint8_t i = 0; i < bat_num; i++) {
    uint16_t v = read_word(info, o); o += 4;
    out.cell_mV[i] = v;
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    avg_sum += v;
  }
  if (bat_num == 0) {
    out.min_cell_mV = out.max_cell_mV = out.avg_cell_mV = out.delta_cell_mV = 0;
  } else {
    out.min_cell_mV = mn;
    out.max_cell_mV = mx;
    out.avg_cell_mV = uint16_t(avg_sum / bat_num);
    out.delta_cell_mV = uint16_t(mx - mn);
  }

  if (o + 2 > info_chars) return false;
  uint8_t temp_num = read_byte(info, o); o += 2;
  if (temp_num > MAX_TEMPS) return false;
  out.temp_num = temp_num;

  if (o + size_t(temp_num) * 4 > info_chars) return false;
  float hottest = 0.0f;
  float coldest = 0.0f;
  bool have_temp = false;
  for (uint8_t i = 0; i < temp_num; i++) {
    uint16_t v = read_word(info, o); o += 4;
    // Kelvin offset 2730 (= 273.0 K). Some firmware variants use 2731 or 2731.5;
    // sensors will read ~0.5°C low compared to a calibrated reference.
    float c = float(int(v) - 2730) * 0.1f;
    out.temp_C[i] = c;
    if (!have_temp) {
      hottest = coldest = c;
      have_temp = true;
    } else {
      if (c > hottest) hottest = c;
      if (c < coldest) coldest = c;
    }
  }
  out.temperature_high_C = have_temp ? hottest : 0.0f;
  out.temperature_low_C = have_temp ? coldest : 0.0f;

  if (o + 4 > info_chars) return false;
  // Current scale is gated on BMS_MODE_F bit 0. Default firmware reports cA (0.01 A).
  float cur_scale = (bms_mode_f & 0x01) ? 0.1f : 0.01f;
  out.pack_current_A = read_sword(info, o) * cur_scale; o += 4;

  if (o + 4 > info_chars) return false;
  out.pack_voltage_V = read_word(info, o) * 0.01f; o += 4;

  if (o + 4 > info_chars) return false;
  out.remaining_capacity_Ah = read_word(info, o) * 0.01f; o += 4;

  // Two reserved chars between Rm and Fcc.
  o += 2;

  if (o + 4 > info_chars) return false;
  out.full_capacity_Ah = read_word(info, o) * 0.01f; o += 4;

  if (o + 4 > info_chars) return false;
  out.cycle_count = read_word(info, o); o += 4;

  if (o + 4 > info_chars) return false;
  out.design_capacity_Ah = read_word(info, o) * 0.01f; o += 4;

  if (o + 4 > info_chars) return false;
  out.cpu_voltage_V = read_word(info, o) * 0.01f; o += 4;

  // unknown_field (always 0 in observed firmware) — skipped.
  o += 4;

  // SOC is 1 byte (2 chars), not a uint16.
  if (o + 2 > info_chars) return false;
  out.soc_pct = read_byte(info, o); o += 2;

  // Gain_Chg_R — internal calibration register, not surfaced.
  o += 4;

  if (o + 8 > info_chars) return false;
  uint16_t bal_hi = read_word(info, o); o += 4;
  uint16_t bal_lo = read_word(info, o); o += 4;
  out.balance_bitmap = (uint32_t(bal_hi) << 16) | bal_lo;

  // Trailing fields are read backwards from the end of the info field. Some firmware
  // emits raw binary bytes mid-info around the active-balance slot, so sequential parsing
  // past this point can land on non-ASCII bytes; reverse-indexed reads avoid that.
  out.soh_pct = (info_chars >= 2) ? read_byte(info, info_chars - 2) : 0;
  // Open-circuit insulation reads sit near 0xFFFF raw (~655 MΩ); compute in 32-bit
  // to avoid the wrap that uint16_t * 10 produces above ~6.5 MΩ.
  out.insulation_kohm =
      (info_chars >= 22) ? uint32_t(read_word(info, info_chars - 22)) * 10u : 0u;
  out.bms_self_consumption_mA =
      (info_chars >= 10) ? read_sword(info, info_chars - 10) : 0;

  return true;
}

bool parse_status(const uint8_t *info, size_t info_chars, StatusFrame &out) {
  if (info_chars < 6) return false;

  size_t o = 0;
  o += 2;  // INFOFLAG
  o += 2;  // pack_info_byte

  uint8_t bat_num = read_byte(info, o); o += 2;
  out.bat_num = bat_num;

  // Per-cell warning bytes (not surfaced; rolled-up flags carry the actionable signal).
  if (o + size_t(bat_num) * 2 > info_chars) return false;
  o += size_t(bat_num) * 2;

  if (o + 2 > info_chars) return false;
  uint8_t temp_num = read_byte(info, o); o += 2;
  if (o + size_t(temp_num) * 2 > info_chars) return false;
  o += size_t(temp_num) * 2;

  // Three reserved warning bytes (charge current, pack voltage, discharge current).
  if (o + 6 > info_chars) return false;
  o += 6;

  // Status1..Status7. Status6 (lock) and Status7 (reserved) are skipped.
  if (o + 14 > info_chars) return false;
  out.status1 = read_byte(info, o); o += 2;
  out.status2 = read_byte(info, o); o += 2;
  out.status3 = read_byte(info, o); o += 2;
  out.status4 = read_byte(info, o); o += 2;
  out.status5 = read_byte(info, o); o += 2;
  o += 2;
  o += 2;

  if (o + 4 > info_chars) return false;
  out.warning1 = read_byte(info, o); o += 2;
  out.warning2 = read_byte(info, o); o += 2;

  // Trailing fields are optional; older firmware truncates the info field here.
  out.battery_mode = 0;
  out.sleep_flag = 0;
  out.bat_type = 0;
  out.soc_low_warn = 0;
  if (o + 2 <= info_chars) { out.battery_mode = read_byte(info, o); o += 2; }
  if (o + 2 <= info_chars) { out.sleep_flag   = read_byte(info, o); o += 2; }
  if (o + 2 <= info_chars) { out.bat_type     = read_byte(info, o); o += 2; }
  if (o + 2 <= info_chars) { out.soc_low_warn = read_byte(info, o); o += 2; }

  return true;
}

}  // namespace tdt_bms
}  // namespace esphome

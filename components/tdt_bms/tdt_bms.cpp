#include "tdt_bms.h"

#include "esphome/core/log.h"

namespace esphome {
namespace tdt_bms {

static const char *const TAG = "tdt_bms";

// Subcommand info field used by all multi-pack analog/status/probe queries.
static const uint8_t INFO_SUBCMD_01[2] = {'0', '1'};

void TdtBms::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TDT BMS hub for %u pack(s)", this->pack_count_);
  this->rx_buffer_.reserve(MAX_FRAME_SIZE);
}

void TdtBms::dump_config() {
  ESP_LOGCONFIG(TAG, "TDT BMS:");
  ESP_LOGCONFIG(TAG, "  Pack count: %u", this->pack_count_);
  ESP_LOGCONFIG(TAG, "  Update interval: %u ms", unsigned(this->get_update_interval()));
  ESP_LOGCONFIG(TAG, "  Reply timeout: %u ms", unsigned(REPLY_TIMEOUT_MS));
  ESP_LOGCONFIG(TAG, "  Inter-command gap: %u ms", unsigned(INTER_COMMAND_MS));
  this->check_uart_settings(9600);
  for (auto *l : this->listeners_) l->dump_config();
}

void TdtBms::update() {
  // The previous burst must finish before the next one starts. If it didn't, we
  // skip this tick and let the queue drain — losing a tick is preferable to
  // overlapping requests on the same UART.
  if (this->awaiting_reply_ || this->tx_queue_head_ < this->tx_queue_.size()) {
    const size_t remaining = this->tx_queue_.size() - this->tx_queue_head_;
    ESP_LOGW(TAG, "Polling tick fired before previous burst drained (%u request(s) left)",
             unsigned(remaining));
    return;
  }

  this->enqueue_round_();
}

void TdtBms::enqueue_round_() {
  this->tx_queue_.clear();
  this->tx_queue_head_ = 0;
  // Analog data first for all packs, then status for all packs. Cells/voltage/SOC
  // change faster than fault flags so analog gets priority within each round.
  for (uint8_t p = 1; p <= this->pack_count_; p++) {
    this->tx_queue_.push_back({p, CID2_ANALOG});
  }
  for (uint8_t p = 1; p <= this->pack_count_; p++) {
    this->tx_queue_.push_back({p, CID2_STATUS});
  }
}

void TdtBms::try_send_next_() {
  if (this->awaiting_reply_) return;
  if (this->tx_queue_head_ >= this->tx_queue_.size()) return;

  const uint32_t now = millis();
  if (now - this->last_tx_complete_at_ < INTER_COMMAND_MS) return;

  const TxRequest req = this->tx_queue_[this->tx_queue_head_];

  build_request(this->tx_buffer_, req.pack, req.cid2, INFO_SUBCMD_01, sizeof(INFO_SUBCMD_01));

  this->write_array(this->tx_buffer_.data(), this->tx_buffer_.size());
  this->flush();

  this->pending_pack_ = req.pack;
  this->pending_cid2_ = req.cid2;
  this->awaiting_reply_ = true;
  this->request_sent_at_ = now;

  ESP_LOGV(TAG, "Sent CID2=0x%02X to pack %u (%u bytes on the wire)", req.cid2, req.pack,
           unsigned(this->tx_buffer_.size()));
}

void TdtBms::loop() {
  const uint32_t now = millis();

  // Drop stale partial frames.
  if (!this->rx_buffer_.empty() && now - this->last_rx_byte_at_ > RX_INACTIVITY_MS) {
    ESP_LOGV(TAG, "RX buffer cleared due to inactivity (%u byte(s) discarded)",
             unsigned(this->rx_buffer_.size()));
    this->rx_buffer_.clear();
  }

  // Time out a request that never got a reply.
  if (this->awaiting_reply_ && now - this->request_sent_at_ > REPLY_TIMEOUT_MS) {
    ESP_LOGW(TAG, "Reply timeout for pack %u CID2=0x%02X", this->pending_pack_,
             this->pending_cid2_);
    this->mark_response_missed_(this->pending_pack_);
    this->advance_after_reply_(false);
    this->rx_buffer_.clear();
  }

  // Drain any pending RX bytes.
  while (this->available()) {
    uint8_t b;
    if (!this->read_byte(&b)) break;
    if (this->parse_byte_(b)) {
      this->last_rx_byte_at_ = now;
    } else {
      this->rx_buffer_.clear();
    }
  }

  // Send the next pending request, subject to the inter-command gap.
  this->try_send_next_();
}

bool TdtBms::parse_byte_(uint8_t b) {
  const size_t at = this->rx_buffer_.size();
  if (at >= MAX_FRAME_SIZE) return false;

  this->rx_buffer_.push_back(b);

  if (at == 0) {
    // Searching for SOI; drop noise until we see one.
    return b == SOI_RESPONSE;
  }
  if (b != EOI) return true;  // not yet end-of-frame

  this->handle_complete_frame_();
  return false;  // dispatched (or rejected) — clear the buffer either way
}

void TdtBms::handle_complete_frame_() {
  ParsedHeader hdr;
  if (!parse_header(this->rx_buffer_.data(), this->rx_buffer_.size(), hdr)) {
    ESP_LOGW(TAG, "Discarded malformed response (%u bytes)",
             unsigned(this->rx_buffer_.size()));
    if (this->awaiting_reply_) {
      this->mark_response_missed_(this->pending_pack_);
      this->advance_after_reply_(false);
    }
    return;
  }

  if (hdr.rtn != RTN_OK_DATA) {
    ESP_LOGW(TAG, "Pack %u CID2=0x%02X returned RTN=0x%02X", hdr.target_addr, hdr.cid2,
             hdr.rtn);
    if (this->awaiting_reply_) {
      this->mark_response_missed_(this->pending_pack_);
      this->advance_after_reply_(false);
    }
    return;
  }

  bool parsed = false;
  if (hdr.cid2 == CID2_ANALOG) {
    AnalogFrame data{};
    if (parse_analog(hdr.info, hdr.info_chars, this->bms_mode_f_, data)) {
      this->mark_response_received_(hdr.target_addr);
      for (auto *l : this->listeners_) l->on_analog_data(hdr.target_addr, data);
      parsed = true;
    } else {
      ESP_LOGW(TAG, "Pack %u: analog parse failed", hdr.target_addr);
    }
  } else if (hdr.cid2 == CID2_STATUS) {
    StatusFrame data{};
    if (parse_status(hdr.info, hdr.info_chars, data)) {
      this->mark_response_received_(hdr.target_addr);
      for (auto *l : this->listeners_) l->on_status_data(hdr.target_addr, data);
      parsed = true;
    } else {
      ESP_LOGW(TAG, "Pack %u: status parse failed", hdr.target_addr);
    }
  } else {
    ESP_LOGV(TAG, "Pack %u: unhandled CID2=0x%02X", hdr.target_addr, hdr.cid2);
  }

  // Only advance the queue if we were actually awaiting this frame. A stray or
  // duplicate frame (BMS retransmission, late reply after timeout) must not
  // skip a still-pending request.
  if (this->awaiting_reply_) {
    this->advance_after_reply_(parsed);
  }
}

void TdtBms::advance_after_reply_(bool /*success*/) {
  this->awaiting_reply_ = false;
  if (this->tx_queue_head_ < this->tx_queue_.size()) {
    this->tx_queue_head_++;
  }
  this->last_tx_complete_at_ = millis();
}

void TdtBms::mark_response_received_(uint8_t pack) {
  if (pack == 0 || pack > MAX_PACKS) return;
  const uint8_t i = pack - 1;
  this->no_response_count_[i] = 0;
  if (!this->online_[i]) {
    this->online_[i] = true;
    for (auto *l : this->listeners_) l->on_pack_online(pack);
  }
}

void TdtBms::mark_response_missed_(uint8_t pack) {
  if (pack == 0 || pack > MAX_PACKS) return;
  const uint8_t i = pack - 1;
  if (this->no_response_count_[i] >= MAX_NO_RESPONSE_COUNT) return;
  this->no_response_count_[i]++;
  if (this->no_response_count_[i] >= MAX_NO_RESPONSE_COUNT && this->online_[i]) {
    this->online_[i] = false;
    for (auto *l : this->listeners_) l->on_pack_offline(pack);
  }
}

}  // namespace tdt_bms
}  // namespace esphome

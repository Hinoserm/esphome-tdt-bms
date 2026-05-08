#include "tdt_bms.h"

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <algorithm>

namespace esphome {
namespace tdt_bms {

static const char *const TAG = "tdt_bms";

// Subcommand info field used by all multi-pack analog/status/probe queries.
static const uint8_t INFO_SUBCMD_01[2] = {'0', '1'};

void TdtBms::setup() {
  this->rx_buffer_.reserve(MAX_FRAME_SIZE);
  this->build_active_polls_();
}

void TdtBms::build_active_polls_() {
  this->active_polls_.clear();
  this->analog_listeners_.clear();
  this->status_listeners_.clear();

  // Pre-filter listeners by which command they consume, so frame dispatch
  // doesn't iterate everyone on every reply.
  for (auto *l : this->listeners_) {
    if (l->wants_analog()) this->analog_listeners_.push_back(l);
    if (l->wants_status()) this->status_listeners_.push_back(l);
  }

  if (this->pack_count_ > 0) {
    // Explicit override: poll every pack 1..N with both commands regardless
    // of which listeners consume the responses.
    for (uint8_t p = 1; p <= this->pack_count_; p++) {
      this->active_polls_.push_back({p, CID2_ANALOG});
    }
    for (uint8_t p = 1; p <= this->pack_count_; p++) {
      this->active_polls_.push_back({p, CID2_STATUS});
    }
    ESP_LOGCONFIG(TAG, "  Polling 1..%u (explicit), %u command(s) per round",
                  this->pack_count_, unsigned(this->active_polls_.size()));
    return;
  }

  // Auto-detect from listeners: only poll packs that have at least one
  // configured entity, and only send commands that have a consumer.
  std::vector<uint8_t> packs;
  for (auto *l : this->listeners_) {
    uint8_t p = l->get_pack();
    if (std::find(packs.begin(), packs.end(), p) == packs.end()) {
      packs.push_back(p);
    }
  }
  std::sort(packs.begin(), packs.end());

  // Interleave by pack across commands so the same pack isn't queried
  // twice in immediate succession — back-to-back queries to one pack can
  // race the BMS's reply path, especially when relayed through the chain.
  for (uint8_t pack : packs) {
    for (auto *l : this->listeners_) {
      if (l->get_pack() == pack && l->wants_analog()) {
        this->active_polls_.push_back({pack, CID2_ANALOG});
        break;
      }
    }
  }
  for (uint8_t pack : packs) {
    for (auto *l : this->listeners_) {
      if (l->get_pack() == pack && l->wants_status()) {
        this->active_polls_.push_back({pack, CID2_STATUS});
        break;
      }
    }
  }

  ESP_LOGCONFIG(TAG, "  Polling %u pack(s) auto-detected, %u command(s) per round",
                unsigned(packs.size()), unsigned(this->active_polls_.size()));
}

void TdtBms::dump_config() {
  ESP_LOGCONFIG(TAG, "TDT BMS:");
  if (this->pack_count_ > 0) {
    ESP_LOGCONFIG(TAG, "  Pack count (explicit): %u", this->pack_count_);
  } else {
    ESP_LOGCONFIG(TAG, "  Pack count: auto-detect from listeners");
  }
  ESP_LOGCONFIG(TAG, "  Update interval: %u ms", unsigned(this->get_update_interval()));
  ESP_LOGCONFIG(TAG, "  Reply timeout: %u ms", unsigned(REPLY_TIMEOUT_MS));
  ESP_LOGCONFIG(TAG, "  Inter-command gap: %u ms", unsigned(INTER_COMMAND_MS));
  ESP_LOGCONFIG(TAG, "  Commands per round: %u", unsigned(this->active_polls_.size()));
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
  this->tx_queue_ = this->active_polls_;
  this->tx_queue_head_ = 0;
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

  ESP_LOGD(TAG, "TX pack=%u cid2=0x%02X (%u bytes): %s", req.pack, req.cid2,
           unsigned(this->tx_buffer_.size()),
           format_hex_pretty(this->tx_buffer_.data(), this->tx_buffer_.size()).c_str());
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
      for (auto *l : this->analog_listeners_) l->on_analog_data(hdr.target_addr, data);
      parsed = true;
    } else {
      ESP_LOGW(TAG, "Pack %u: analog parse failed", hdr.target_addr);
    }
  } else if (hdr.cid2 == CID2_STATUS) {
    StatusFrame data{};
    if (parse_status(hdr.info, hdr.info_chars, data)) {
      this->mark_response_received_(hdr.target_addr);
      for (auto *l : this->status_listeners_) l->on_status_data(hdr.target_addr, data);
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

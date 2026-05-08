#pragma once

#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"
#include "tdt_bms_protocol.h"

#include <vector>

namespace esphome {
namespace tdt_bms {

// Listeners attach to the hub to receive parsed frames. Each platform entry
// (sensor / binary_sensor / text_sensor) constructs its own listener bound to
// a specific pack number; the listener filters broadcasts internally.
class TdtBmsListener {
 public:
  // Identifies which pack address this listener is bound to.
  virtual uint8_t get_pack() const = 0;

  // Whether this listener consumes analog (CID2=0x42) or status (CID2=0x44)
  // responses. The hub uses these to decide which commands are worth sending
  // for this listener's pack — if no listener for a given pack wants a given
  // command, that command is skipped each polling round.
  virtual bool wants_analog() const { return false; }
  virtual bool wants_status() const { return false; }

  virtual void on_analog_data(uint8_t pack, const AnalogFrame &data) {}
  virtual void on_status_data(uint8_t pack, const StatusFrame &data) {}
  virtual void on_pack_online(uint8_t pack) {}
  virtual void on_pack_offline(uint8_t pack) {}
  virtual void dump_config() {}
};

class TdtBms : public PollingComponent, public uart::UARTDevice {
 public:
  void setup() override;
  void loop() override;
  void update() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }

  void set_pack_count(uint8_t n) { this->pack_count_ = n; }
  void register_listener(TdtBmsListener *l) { this->listeners_.push_back(l); }

 protected:
  // Maximum addressable packs in a single chain. The wire protocol allows up to 16,
  // matching the BatteryOnline_bit field width.
  static constexpr uint8_t MAX_PACKS = 16;

  // After this many consecutive missed replies a pack is considered offline.
  static constexpr uint8_t MAX_NO_RESPONSE_COUNT = 3;

  // Per-frame reply timeout. The BMS itself is not particularly fast at 9600 baud
  // and back-to-back queries occasionally lose packets; 1.5s is a comfortable margin.
  static constexpr uint32_t REPLY_TIMEOUT_MS = 1500;

  // Minimum gap between consecutive requests on the bus. Lower values can trigger
  // BMS rejection during high-rate polling.
  static constexpr uint32_t INTER_COMMAND_MS = 200;

  // RX inactivity threshold; a partial frame older than this is dropped.
  static constexpr uint32_t RX_INACTIVITY_MS = 200;

  struct TxRequest {
    uint8_t pack;
    uint8_t cid2;
  };

  void build_active_polls_();
  void enqueue_round_();
  void try_send_next_();
  bool parse_byte_(uint8_t b);
  void handle_complete_frame_();
  void advance_after_reply_(bool success);
  void mark_response_received_(uint8_t pack);
  void mark_response_missed_(uint8_t pack);

  // Explicit override: when 0, the active pack list is derived from the set
  // of pack numbers registered by listeners. When non-zero, packs 1..pack_count_
  // are polled with both analog and status commands regardless of listeners.
  uint8_t pack_count_{0};
  uint16_t bms_mode_f_{0};   // Read from CID2=0xC1 once at boot; gates current scale.

  std::vector<TdtBmsListener *> listeners_;

  // Pre-filtered subsets of listeners_ populated at setup() time so dispatch
  // doesn't iterate every listener on every frame.
  std::vector<TdtBmsListener *> analog_listeners_;
  std::vector<TdtBmsListener *> status_listeners_;

  // Static list of (pack, cid2) tuples to poll each round, computed once at
  // setup() from the listener registrations.
  std::vector<TxRequest> active_polls_;

  // TX queue: a flat list of pending (pack, cid2) pairs for the current burst.
  // Refilled from active_polls_ at the start of each round.
  std::vector<TxRequest> tx_queue_;
  size_t tx_queue_head_{0};

  bool awaiting_reply_{false};
  uint8_t pending_pack_{0};
  uint8_t pending_cid2_{0};
  uint32_t request_sent_at_{0};
  uint32_t last_tx_complete_at_{0};

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_rx_byte_at_{0};

  // Reused TX scratch buffer; capacity grows once and stays after clear().
  std::vector<uint8_t> tx_buffer_;

  uint8_t no_response_count_[MAX_PACKS] = {};
  bool online_[MAX_PACKS] = {};
};

}  // namespace tdt_bms
}  // namespace esphome

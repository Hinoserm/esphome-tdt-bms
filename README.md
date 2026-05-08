# esphome-tdt-bms

ESPHome external component for **Humsienk 100 Ah rack-mount LiFePO4 batteries** and other battery packs that use a TDT-1001 BMS (or compatible variant). Reads cell voltages, pack voltage, current, capacity, SOC, SOH, temperatures, MOSFET state, protections, and active alarms over the BMS console-port serial connection, and exposes them as Home Assistant sensors via ESPHome.

Multi-pack chains are supported: when several packs are linked through their inter-pack chain ports, a single ESPHome device wired to the master pack's console port can poll every pack in the chain.

## Tested with

- 2× Humsienk 16S 100 Ah LiFePO4 rack-mount packs (master/slave chain)
- ESP32-S3 with W5500 ethernet, ESP-IDF framework
- ESPHome 2026.4.5

The component is structurally complete and validated against live hardware. Optional firmware features gated on the `BMS_MODE_F` flag word (active-balance current/target, MCU current sensor) are not yet exercised; defaults match the most common firmware behavior.

## Hardware requirements

- A TDT-1001 BMS (or compatible variant) with an RS-232 console port. The Humsienk 100 Ah rack-mount LiFePO4 is the validated reference.
- An ESP32 (or ESP32-S3) with a hardware UART connected to the BMS console port through an RS-232 level shifter (e.g. MAX3232).
- Serial settings: 9600 baud, 8N1, no flow control.

For multi-pack configurations, the master pack holds the RS-232 connection. Slave packs are reached via the inter-pack chain link. The master must be left in its default protocol mode for chain transparency to work.

## ESP32-S3 + ESP-IDF: required sdkconfig

If you're targeting an ESP32-S3 with the ESP-IDF framework, **you must release UART0 from the IDF console subsystem before assigning it to the BMS**, or the BMS won't respond. The IDF bootloader pre-configures UART0 for console output before user code runs; when ESPHome later reroutes UART0 to your BMS pins, leftover IOMUX state corrupts the first serial frame the BMS sees and the BMS goes silent.

```yaml
esp32:
  board: esp32-s3-devkitc-1
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_ESP_CONSOLE_NONE: y
      CONFIG_ESP_CONSOLE_SECONDARY_NONE: y

logger:
  hardware_uart: USB_SERIAL_JTAG
```

Both `CONFIG_ESP_CONSOLE_NONE` **and** `CONFIG_ESP_CONSOLE_SECONDARY_NONE` are required — on S3 the secondary console default is still UART0 even after redirecting the primary. You only need this if the BMS UART is the first `uart:` entry (i.e. allocated to Bus 0). If you have another UART declared first (e.g. an inverter component on a different UART), the BMS UART naturally lands on Bus 1 and these flags aren't needed.

## Installation

Add the component to your ESPHome configuration:

```yaml
external_components:
  - source: github://hinoserm/esphome-tdt-bms
    components: [tdt_bms]
```

## Configuration

```yaml
uart:
  id: bms_uart
  tx_pin: GPIO16
  rx_pin: GPIO17
  baud_rate: 9600
  rx_buffer_size: 384

tdt_bms:
  id: bms
  uart_id: bms_uart
  update_interval: 5s
  # packs: 2            # optional override; otherwise auto-detected from
  #                       the pack numbers referenced in your platform entries

sensor:
  - platform: tdt_bms
    tdt_bms_id: bms
    pack: 1
    pack_voltage: { name: "Pack 1 Voltage" }
    pack_current: { name: "Pack 1 Current" }
    state_of_charge: { name: "Pack 1 SOC" }
    state_of_health: { name: "Pack 1 SOH" }
    cell_voltage_1: { name: "Pack 1 Cell 1" }
    # ... cell_voltage_2 through cell_voltage_16
    temperature_1: { name: "Pack 1 Temp 1" }
    # ... temperature_2 through temperature_6

binary_sensor:
  - platform: tdt_bms
    tdt_bms_id: bms
    pack: 1
    online: { name: "Pack 1 Online" }
    charging_mosfet: { name: "Pack 1 Charge MOSFET" }
    discharging_mosfet: { name: "Pack 1 Discharge MOSFET" }

text_sensor:
  - platform: tdt_bms
    tdt_bms_id: bms
    pack: 1
    battery_mode: { name: "Pack 1 Mode" }
    chemistry: { name: "Pack 1 Chemistry" }
```

See [`example.yaml`](example.yaml) for a full reference covering both packs and every available sensor.

## Available sensors per pack

### `sensor:` (numeric)

**Pack-level**: `pack_voltage`, `pack_current`, `pack_power`, `cpu_voltage`, `remaining_capacity`, `full_capacity`, `design_capacity`, `cycle_count`, `state_of_charge`, `state_of_health`, `insulation_resistance`, `bms_self_consumption`, `min_cell_voltage`, `max_cell_voltage`, `cell_voltage_delta`, `avg_cell_voltage`, `temperature_high`, `temperature_low`

The per-pack current scale is auto-detected at boot from the BMS firmware-info word (CID2=0xC1). On firmware variants with the high-current scale flag set, readings are correctly interpreted as 0.1 A per LSB; on default firmware they are interpreted as 0.01 A per LSB.

**Per-cell**: `cell_voltage_1` … `cell_voltage_16`

**Per-temperature-sensor**: `temperature_1` … `temperature_6`

### `binary_sensor:`

**Pack-level**: `online`, `charging_mosfet`, `discharging_mosfet`, `heater`, `bms_sleeping`, `protection_active`, `warning_active`, `fault_active`, `low_soc_warning`, `balancing_active`

**Per-cell**: `cell_balancing_1` … `cell_balancing_16` (true while the BMS is actively balancing that cell)

### `text_sensor:`

- `battery_mode` — `Charging` / `Discharging` / `Idle`
- `chemistry` — `LiFePO4` / `Li-ion NMC` / `LTO` / etc.
- `active_protections`, `active_warnings`, `active_faults` — comma-separated lists of currently-tripped flag names
- `balancing_cells` — comma-separated 1-based list of cells currently balancing (empty when none)
- `firmware_version` — pack firmware/board version string (read once at boot)

## Notes

- **Auto-detect packs**: if `packs:` isn't set on the hub, the active pack list is derived from the `pack:` numbers used in your `sensor:` / `binary_sensor:` / `text_sensor:` entries. Only those packs are polled, and only commands with a consumer are sent — e.g. if no `binary_sensor` or `text_sensor` is configured for a pack, the alarm/status query for that pack is skipped entirely.
- **Polling cadence**: each `update_interval` tick enqueues the analog query (for sensor consumers) and the status query (for binary_sensor / text_sensor consumers) for every active pack. With 2 packs at the default 5 s interval, each pack refreshes roughly every 5 s. For larger chains, raise the interval so the bus has time to drain between rounds.
- **Temperature offset**: the BMS firmware reports temperatures as `(K × 10 − 2730)`. Readings sit ~0.5 °C below a calibrated reference; subtract 0.5 in your dashboard if precision matters.
- **Online status**: a pack that fails to respond for 3 consecutive update cycles is marked offline; its sensors publish `NaN` (numeric) or `false` (binary) until it returns.
- **Insulation resistance**: open-circuit reads sit near 655 MΩ. Values below ~100 kΩ indicate a developing insulation fault and are worth alerting on.

## License

MIT — see [`LICENSE`](LICENSE).

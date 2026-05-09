"""Numeric sensor platform for the tdt_bms component.

A single platform entry binds to one battery pack via `tdt_bms_id` + `pack`
and exposes any subset of the available numeric measurements as Home Assistant
sensors. Per-cell and per-temperature sensors are addressed by 1-based index.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_VOLTAGE,
    DEVICE_CLASS_CURRENT,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_TEMPERATURE,
    DEVICE_CLASS_BATTERY,
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,
    UNIT_VOLT,
    UNIT_AMPERE,
    UNIT_WATT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
    UNIT_EMPTY,
    ENTITY_CATEGORY_DIAGNOSTIC,
)

from .. import (
    CONF_PACK,
    CONF_TDT_BMS_ID,
    TDT_BMS_PLATFORM_SCHEMA,
    TdtBmsListener,
    tdt_bms_ns,
)

DEPENDENCIES = ["tdt_bms"]
CODEOWNERS = ["@hinoserm"]

TdtBmsPackSensor = tdt_bms_ns.class_("TdtBmsPackSensor", TdtBmsListener)

UNIT_AMPERE_HOUR = "Ah"
UNIT_KILOOHM = "kΩ"
UNIT_MILLIAMP = "mA"

# Pack-level scalar sensors. The dict key is both the YAML key and the C++
# setter stem (`set_<key>_sensor`); the values are kwargs forwarded to
# `sensor.sensor_schema(...)`.
SENSORS = {
    "pack_voltage": dict(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "pack_current": dict(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "pack_power": dict(
        unit_of_measurement=UNIT_WATT,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_POWER,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "cpu_voltage": dict(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "remaining_capacity": dict(
        unit_of_measurement=UNIT_AMPERE_HOUR,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "full_capacity": dict(
        unit_of_measurement=UNIT_AMPERE_HOUR,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "design_capacity": dict(
        unit_of_measurement=UNIT_AMPERE_HOUR,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "cycle_count": dict(
        unit_of_measurement=UNIT_EMPTY,
        accuracy_decimals=0,
        state_class=STATE_CLASS_TOTAL_INCREASING,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "state_of_charge": dict(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_BATTERY,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "state_of_health": dict(
        unit_of_measurement=UNIT_PERCENT,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "insulation_resistance": dict(
        unit_of_measurement=UNIT_KILOOHM,
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "bms_self_consumption": dict(
        unit_of_measurement=UNIT_MILLIAMP,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "min_cell_voltage": dict(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "max_cell_voltage": dict(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "cell_voltage_delta": dict(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "avg_cell_voltage": dict(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # Highest and lowest of the per-sensor temperature readings currently
    # observed in the pack. The high reading is the hot-spot for thermal
    # alerting; the low reading flags cells that may be running cold.
    "temperature_high": dict(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "temperature_low": dict(
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    # Active-balance fields, populated only when BMS_MODE_F bit 1 is set on
    # the target firmware. Sensors stay unavailable on firmware that doesn't
    # expose them.
    "active_balance_current": dict(
        unit_of_measurement=UNIT_AMPERE,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_CURRENT,
        state_class=STATE_CLASS_MEASUREMENT,
    ),
    "active_balance_target_voltage": dict(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "emergency_mode_timer": dict(
        unit_of_measurement="min",
        accuracy_decimals=0,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
    "equipment_voltage": dict(
        unit_of_measurement=UNIT_VOLT,
        accuracy_decimals=3,
        device_class=DEVICE_CLASS_VOLTAGE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CELLS = [f"cell_voltage_{i}" for i in range(1, 17)]
TEMPS = [f"temperature_{i}" for i in range(1, 7)]

_CELL_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_VOLT,
    accuracy_decimals=3,
    device_class=DEVICE_CLASS_VOLTAGE,
    state_class=STATE_CLASS_MEASUREMENT,
)
_TEMP_SCHEMA = sensor.sensor_schema(
    unit_of_measurement=UNIT_CELSIUS,
    accuracy_decimals=1,
    device_class=DEVICE_CLASS_TEMPERATURE,
    state_class=STATE_CLASS_MEASUREMENT,
)

CONFIG_SCHEMA = (
    TDT_BMS_PLATFORM_SCHEMA.extend(
        {cv.GenerateID(): cv.declare_id(TdtBmsPackSensor)}
    )
    .extend(
        {cv.Optional(k): sensor.sensor_schema(**v) for k, v in SENSORS.items()}
    )
    .extend({cv.Optional(k): _CELL_SCHEMA for k in CELLS})
    .extend({cv.Optional(k): _TEMP_SCHEMA for k in TEMPS})
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TDT_BMS_ID])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_PACK])
    cg.add(parent.register_listener(var))

    for i, key in enumerate(CELLS):
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(var.set_cell_voltage_sensor(i, sens))
    for i, key in enumerate(TEMPS):
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(var.set_temperature_sensor(i, sens))
    for key in SENSORS:
        if key in config:
            sens = await sensor.new_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_sensor")(sens))

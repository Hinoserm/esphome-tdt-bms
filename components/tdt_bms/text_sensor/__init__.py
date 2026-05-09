"""Text sensor platform for the tdt_bms component.

Exposes human-readable strings (battery operating mode, cell chemistry, and
comma-separated lists of currently-active protections / warnings / faults) as
Home Assistant text sensors.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import (
    CONF_ID,
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

TdtBmsPackTextSensor = tdt_bms_ns.class_("TdtBmsPackTextSensor", TdtBmsListener)

# YAML key → text_sensor.text_sensor_schema kwargs.
TEXT_SENSORS = {
    "battery_mode": dict(),
    "chemistry": dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    "active_protections": dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    "active_warnings": dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    "active_faults": dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    "firmware_version": dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    "balancing_cells": dict(),
    "bms_mode_flags": dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
}

CONFIG_SCHEMA = TDT_BMS_PLATFORM_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(TdtBmsPackTextSensor)}
).extend(
    {cv.Optional(k): text_sensor.text_sensor_schema(**v) for k, v in TEXT_SENSORS.items()}
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TDT_BMS_ID])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_PACK])
    cg.add(parent.register_listener(var))

    for key in TEXT_SENSORS:
        if key in config:
            sens = await text_sensor.new_text_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_text_sensor")(sens))

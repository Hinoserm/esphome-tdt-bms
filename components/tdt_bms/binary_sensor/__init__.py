"""Binary sensor platform for the tdt_bms component.

Exposes MOSFET states, balancer activity, online status, and rolled-up
protection / warning / fault flags as Home Assistant binary sensors.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_CONNECTIVITY,
    DEVICE_CLASS_PROBLEM,
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

TdtBmsPackBinarySensor = tdt_bms_ns.class_("TdtBmsPackBinarySensor", TdtBmsListener)

# YAML key → binary_sensor.binary_sensor_schema(...) kwargs.
BINARY_SENSORS = {
    "charging_mosfet": dict(),
    "discharging_mosfet": dict(),
    "heater": dict(),
    "bms_sleeping": dict(entity_category=ENTITY_CATEGORY_DIAGNOSTIC),
    "protection_active": dict(device_class=DEVICE_CLASS_PROBLEM),
    "warning_active": dict(device_class=DEVICE_CLASS_PROBLEM),
    "fault_active": dict(device_class=DEVICE_CLASS_PROBLEM),
    "low_soc_warning": dict(device_class=DEVICE_CLASS_BATTERY),
    "balancing_active": dict(),
    "online": dict(
        device_class=DEVICE_CLASS_CONNECTIVITY,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    ),
}

CONFIG_SCHEMA = TDT_BMS_PLATFORM_SCHEMA.extend(
    {cv.GenerateID(): cv.declare_id(TdtBmsPackBinarySensor)}
).extend(
    {cv.Optional(k): binary_sensor.binary_sensor_schema(**v) for k, v in BINARY_SENSORS.items()}
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_TDT_BMS_ID])
    var = cg.new_Pvariable(config[CONF_ID], config[CONF_PACK])
    cg.add(parent.register_listener(var))

    for key in BINARY_SENSORS:
        if key in config:
            sens = await binary_sensor.new_binary_sensor(config[key])
            cg.add(getattr(var, f"set_{key}_binary_sensor")(sens))

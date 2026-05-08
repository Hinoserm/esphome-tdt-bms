"""TDT-1001 BMS hub component.

Owns the UART connection and runs the polling state machine that queries each
configured battery pack for analog data and status. Sensor / binary_sensor /
text_sensor platforms attach to this hub and consume the parsed values.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = ["@hinoserm"]
DEPENDENCIES = ["uart"]

CONF_TDT_BMS_ID = "tdt_bms_id"
CONF_PACK = "pack"
CONF_PACKS = "packs"

tdt_bms_ns = cg.esphome_ns.namespace("tdt_bms")
TdtBms = tdt_bms_ns.class_("TdtBms", cg.PollingComponent, uart.UARTDevice)
TdtBmsListener = tdt_bms_ns.class_("TdtBmsListener")

# Schema fragment shared by sensor / binary_sensor / text_sensor platforms.
# Each platform entry binds to one TdtBms hub and one pack address.
TDT_BMS_PLATFORM_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_TDT_BMS_ID): cv.use_id(TdtBms),
        cv.Required(CONF_PACK): cv.int_range(min=1, max=16),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(TdtBms),
            cv.Optional(CONF_PACKS, default=1): cv.int_range(min=1, max=16),
        }
    )
    .extend(cv.polling_component_schema("5s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_pack_count(config[CONF_PACKS]))

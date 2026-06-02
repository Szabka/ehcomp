from esphome.components import time, binary_sensor, switch, sensor
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_DEVICE_CLASS, CONF_NAME, CONF_PASSWORD
from esphome.core import CORE

CODEOWNERS = ["@RoganDawes"]

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor", "sensor", "switch", "text_sensor"]

CONF_PORT = "tcpport"

wintex_ns = cg.esphome_ns.namespace("wintexss")
Wintexss = wintex_ns.class_("StreamServerComponent", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Wintexss),
            cv.Required(CONF_PORT): cv.positive_int,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    if CORE.is_esp8266:
        cg.add_build_flag("-DHARDWARE_SERIAL")
        cg.add_library("ESPAsyncTCP", None)

    cg.add(var.set_port(config[CONF_PORT]))

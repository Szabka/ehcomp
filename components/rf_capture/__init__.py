from typing import Any

from esphome import pins
import esphome.codegen as cg
from esphome.components import remote_base
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_DUMP,
    CONF_FILTER,
    CONF_ID,
    CONF_IDLE,
    CONF_PIN,
    CONF_TOLERANCE,
    CONF_TYPE,
    CONF_VALUE,
)
from esphome.core import CORE
from esphome.types import ConfigType

AUTO_LOAD = ["remote_base"]

rf_capture_ns = cg.esphome_ns.namespace("rf_capture")
remote_base_ns = cg.esphome_ns.namespace("remote_base")

ToleranceMode = remote_base_ns.enum("ToleranceMode")

TYPE_PERCENTAGE = "percentage"
TYPE_TIME = "time"

TOLERANCE_MODE = {
    TYPE_PERCENTAGE: ToleranceMode.TOLERANCE_MODE_PERCENTAGE,
    TYPE_TIME: ToleranceMode.TOLERANCE_MODE_TIME,
}

TOLERANCE_SCHEMA = cv.typed_schema(
    {
        TYPE_PERCENTAGE: cv.Schema(
            {cv.Required(CONF_VALUE): cv.All(cv.percentage_int, cv.uint32_t)}
        ),
        TYPE_TIME: cv.Schema(
            {cv.Required(CONF_VALUE): cv.positive_time_period_microseconds}
        ),
    },
    lower=True,
    enum=TOLERANCE_MODE,
)

RfCapture = rf_capture_ns.class_(
    "RfCapture", remote_base.RemoteReceiverBase, cg.Component
)


def validate_tolerance(value: Any) -> ConfigType:
    if isinstance(value, dict):
        return TOLERANCE_SCHEMA(value)

    if "%" in str(value):
        type_ = TYPE_PERCENTAGE
    else:
        try:
            cv.positive_time_period_microseconds(value)
            type_ = TYPE_TIME
        except cv.Invalid as exc:
            raise cv.Invalid("Tolerance must be a percentage or time") from exc

    return TOLERANCE_SCHEMA({CONF_VALUE: value, CONF_TYPE: type_})


def validate_esp8266(config: ConfigType) -> ConfigType:
    if not CORE.is_esp8266:
        raise cv.Invalid("rf_capture supports only ESP8266")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(RfCapture),
            cv.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
            cv.Optional(CONF_DUMP, default=[]): remote_base.validate_dumpers,
            cv.Optional(CONF_TOLERANCE, default="25%"): validate_tolerance,
            cv.Optional(CONF_BUFFER_SIZE, default="4000b"): cv.All(
                cv.validate_bytes, cv.int_range(min=64, max=65535)
            ),
            cv.Optional(CONF_FILTER, default="50us"): cv.positive_time_period_microseconds,
            cv.Optional(CONF_IDLE, default="10ms"): cv.positive_time_period_microseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    validate_esp8266,
)


async def to_code(config: ConfigType) -> None:
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    var = cg.new_Pvariable(config[CONF_ID], pin)
    await cg.register_component(var, config)

    dumpers = await remote_base.build_dumpers(config[CONF_DUMP])
    for dumper in dumpers:
        remote_base.add_dumper(var, dumper)

    cg.add(
        var.set_tolerance(
            config[CONF_TOLERANCE][CONF_VALUE], config[CONF_TOLERANCE][CONF_TYPE]
        )
    )
    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_filter_us(config[CONF_FILTER]))
    cg.add(var.set_idle_us(config[CONF_IDLE]))

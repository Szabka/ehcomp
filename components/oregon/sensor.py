import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_CHANNEL,
    CONF_HUMIDITY,
    CONF_TEMPERATURE,
    CONF_TYPE,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)
from . import Oregon

CONF_OREGON_ID = "oregon_id"


def _sensor(conf):
    if conf is None:
        return cg.RawExpression("nullptr")
    return conf


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_OREGON_ID): cv.use_id(Oregon),
            cv.Optional(CONF_CHANNEL): cv.int_range(min=1, max=15),
            cv.Optional(CONF_TYPE): cv.hex_uint16_t,
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=1,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_TEMPERATURE, CONF_HUMIDITY),
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_OREGON_ID])
    temperature = None
    humidity = None
    if CONF_TEMPERATURE in config:
        temperature = await sensor.new_sensor(config[CONF_TEMPERATURE])
    if CONF_HUMIDITY in config:
        humidity = await sensor.new_sensor(config[CONF_HUMIDITY])
    channel = config.get(CONF_CHANNEL, 0)
    match_type = CONF_TYPE in config
    sensor_type = config.get(CONF_TYPE, 0)
    cg.add(hub.add_sensor(channel, sensor_type, match_type, _sensor(temperature), _sensor(humidity)))

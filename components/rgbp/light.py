import esphome.codegen as cg
from esphome.components import light, output, power_supply
import esphome.config_validation as cv
from esphome.const import CONF_BLUE, CONF_GREEN, CONF_OUTPUT_ID, CONF_RED, CONF_POWER_SUPPLY

rgb_ns = cg.esphome_ns.namespace("rgbp")
RGBLightOutput = rgb_ns.class_("RGBLightOutput", light.LightOutput)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(RGBLightOutput),
        cv.Required(CONF_RED): cv.use_id(output.FloatOutput),
        cv.Required(CONF_GREEN): cv.use_id(output.FloatOutput),
        cv.Required(CONF_BLUE): cv.use_id(output.FloatOutput),
        cv.Optional(CONF_POWER_SUPPLY): cv.use_id(power_supply.PowerSupply), 
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)

    red = await cg.get_variable(config[CONF_RED])
    cg.add(var.set_red(red))
    green = await cg.get_variable(config[CONF_GREEN])
    cg.add(var.set_green(green))
    blue = await cg.get_variable(config[CONF_BLUE])
    cg.add(var.set_blue(blue))

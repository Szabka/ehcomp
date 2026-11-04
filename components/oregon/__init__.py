import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import remote_base
from esphome.const import CONF_ID

DEPENDENCIES = ["remote_base"]
AUTO_LOAD = ["sensor"]
MULTI_CONF = True

CONF_RECEIVER_ID = "receiver_id"

oregon_ns = cg.esphome_ns.namespace("oregon")
Oregon = oregon_ns.class_("Oregon", cg.Component, remote_base.RemoteReceiverListener)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Oregon),
        cv.GenerateID(CONF_RECEIVER_ID): cv.use_id(remote_base.RemoteReceiverBase),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    receiver = await cg.get_variable(config[CONF_RECEIVER_ID])
    remote_base.add_listener(receiver, var)

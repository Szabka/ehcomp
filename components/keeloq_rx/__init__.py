import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import remote_base
from esphome.const import CONF_ID

DEPENDENCIES = ["remote_base"]

CONF_RECEIVER_ID = "receiver_id"

keeloq_rx_ns = cg.esphome_ns.namespace("keeloq_rx")
KeeloqRx = keeloq_rx_ns.class_("KeeloqRx", cg.Component, remote_base.RemoteReceiverListener)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(KeeloqRx),
        cv.GenerateID(CONF_RECEIVER_ID): cv.use_id(remote_base.RemoteReceiverBase),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    receiver = await cg.get_variable(config[CONF_RECEIVER_ID])
    remote_base.add_listener(receiver, var)

"""Timed Memories: a screensaver step that shows a camera/image entity full-screen with a date caption."""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import smart_display
from esphome.const import CONF_ID

DEPENDENCIES = ["smart_display"]

timed_memories_ns = cg.esphome_ns.namespace("timed_memories")
TimedMemories = timed_memories_ns.class_("TimedMemories", cg.Component)

CONFIG_SCHEMA = cv.Schema({cv.GenerateID(): cv.declare_id(TimedMemories)}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await smart_display.register_plugin(var, __file__)
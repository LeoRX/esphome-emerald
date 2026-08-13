import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import ble_client, sensor
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_ID,
    CONF_POWER,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_POWER,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    UNIT_WATT,
)

DEPENDENCIES = ["ble_client"]

emerald_ble_ns = cg.esphome_ns.namespace("emerald_ble")
Emerald = emerald_ble_ns.class_("Emerald", ble_client.BLEClientNode, cg.Component)

CONF_PULSES_PER_KWH = "pulses_per_kwh"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Emerald),
            cv.Required(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Required(CONF_PULSES_PER_KWH): cv.float_range(min=0.001),
            cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                device_class=DEVICE_CLASS_BATTERY,
                accuracy_decimals=0,
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(ble_client.BLE_CLIENT_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await ble_client.register_ble_node(var, config)

    power = await sensor.new_sensor(config[CONF_POWER])
    cg.add(var.set_power_sensor(power))
    cg.add(var.set_pulses_per_kwh(config[CONF_PULSES_PER_KWH]))

    if CONF_BATTERY_LEVEL in config:
        battery = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(var.set_battery(battery))

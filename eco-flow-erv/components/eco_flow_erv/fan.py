import esphome.codegen as cg
from esphome.components import binary_sensor, fan, select, switch, text_sensor, uart
import esphome.config_validation as cv
from esphome.const import CONF_RESTORE_MODE, ENTITY_CATEGORY_DIAGNOSTIC

DEPENDENCIES = ["uart", "esp32"]
AUTO_LOAD = ["binary_sensor", "select", "switch", "text_sensor"]

ns = cg.esphome_ns.namespace("eco_flow_erv")
EcoFlowFan = ns.class_("EcoFlowFan", cg.Component, fan.Fan, uart.UARTDevice)
ControlSwitch = ns.class_("ControlSwitch", switch.Switch)
ModeSelect = ns.class_("ModeSelect", select.Select)

CONFIG_SCHEMA = (
    fan.fan_schema(EcoFlowFan)
    .extend({
        # The prototype never restores commands or arms itself after a reboot.
        cv.Optional(CONF_RESTORE_MODE, default="NO_RESTORE"): cv.enum(
            {"NO_RESTORE": fan.FanRestoreMode.NO_RESTORE}, upper=True
        ),
        cv.Required("control_enabled"): switch.switch_schema(
            ControlSwitch, block_inverted=True,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ).extend({
            cv.Optional(CONF_RESTORE_MODE, default="ALWAYS_OFF"): cv.enum(
                {"ALWAYS_OFF": switch.SwitchRestoreMode.SWITCH_ALWAYS_OFF}, upper=True
            ),
        }),
        cv.Required("airflow_mode"): select.select_schema(ModeSelect),
        cv.Optional("enable_recovery", default=False): cv.boolean,
        cv.Optional("frame_interval", default="97ms"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(milliseconds=90), max=cv.TimePeriod(seconds=1)),
        ),
        cv.Optional("recovery_phase_packets", default=700): cv.int_range(min=100, max=5000),
        cv.Optional("out_timeout", default="2s"): cv.All(
            cv.positive_time_period_milliseconds,
            cv.Range(min=cv.TimePeriod(seconds=1), max=cv.TimePeriod(seconds=60)),
        ),
        cv.Optional("observed_frame"): text_sensor.text_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
        cv.Optional("out_active"): binary_sensor.binary_sensor_schema(
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC
        ),
    })
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "eco_flow_erv", require_tx=True, require_rx=False,
    data_bits=8, parity="EVEN", stop_bits=2,
)


async def to_code(config):
    var = await fan.new_fan(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_frame_interval(config["frame_interval"]))
    cg.add(var.set_phase_packets(config["recovery_phase_packets"]))
    cg.add(var.set_out_timeout(config["out_timeout"]))

    gate = await switch.new_switch(config["control_enabled"], var)
    cg.add(var.set_control_switch(gate))
    options = ["Supply", "Exhaust"]
    if config["enable_recovery"]:
        options.append("Recovery (experimental)")
    mode = await select.new_select(config["airflow_mode"], var, options=options)
    cg.add(var.set_mode_select(mode))
    if "observed_frame" in config:
        sensor = await text_sensor.new_text_sensor(config["observed_frame"])
        cg.add(var.set_observed_frame(sensor))
    if "out_active" in config:
        sensor = await binary_sensor.new_binary_sensor(config["out_active"])
        cg.add(var.set_out_active(sensor))

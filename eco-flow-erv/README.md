# Eco-Flo bedroom ERV

ESPHome UART control for Alex's CFM Eco-Flo / Vents TwinFresh Comfo RA1-50-2,
using an M5Stack NanoC6. [Tracking issue #7](https://github.com/alexlmiller/esphome-projects/issues/7).

On 2026-09-10 Alex physically verified **on/off and speeds 1–3 in both Supply
and Exhaust** through ERV IN. Recovery remains disabled. Permanent electrical
installation and controller-loss behavior are not yet validated.

## Home Assistant controls

1. Enable **Enable control frames**. This starts repeating OFF, not the fan.
2. Select **Requested airflow mode**: Supply or Exhaust.
3. Open **Requested fan** for on/off and three-speed control.
4. To stop, turn the fan OFF before disabling control frames.

The fan reports requested state, not measured airflow. Mode or speed-only
changes while off do not power on; an explicit ON request is required.
Every reboot starts OFF, speed 1, Supply, with control frames disabled.
Wi-Fi/API loss does not deliberately reboot the Nano; an armed local stream
continues while it runs.

**Disabling control frames is not a stop command or electrical disconnect.**
The ERV may retain its last state after transmission loss; this is untested.
TX still drives idle HIGH while disarmed. There is no proven controller-loss
failsafe.

The deployed `lev-haos` dashboard entry is **eco-flo-bedroom.yaml**.
Choose **Logs → On the network** to view DEBUG logs; no Install/flash is needed.
Its thin wrapper is maintained in
[infra](https://github.com/alexlmiller/infra/blob/main/roles/esphome_devices/files/devices/eco-flo-bedroom.yaml).
Standard Wi-Fi, API encryption, OTA and fallback-AP secrets are inherited.

## Hardware

| NanoC6 pin | Role | Control-board terminal |
| --- | --- | --- |
| GPIO1 / Grove white | TX | IN |
| GPIO2 / Grove yellow | Optional protected RX | OUT |
| Grove black | Signal reference | GND, subject to grounding/isolation review |

This is the logical pin map, not a validated direct-wire installation. The
successful bench test used TX and GND only, with separate USB power; RX and
Grove red were disconnected. No XP1 relay or IR tap is needed for core control.

ESP32 GPIOs are 3.3 V and are not 5 V-tolerant; the ERV controller was measured
at 5 V. OUT needs a suitable protection/level-conversion interface before RX
connection. Permanent IN drive, power supply, isolation and grounding still
need verification. Disconnect mains before changing wiring. Use the control
board's labeled IN/OUT/GND, not terminal numbers alone; never join two outputs.
No internal 5 V tap is approved, and USB and ERV-derived power must not be
paralleled without reviewing backfeed protection. Meter averages and continuity
checks do not establish pulse peaks or mains-rated isolation.

References: [NanoC6 pin map](https://docs.m5stack.com/en/core/M5NanoC6),
[ESP GPIO voltage limits](https://docs.espressif.com/projects/esp-faq/en/latest/hardware-related/hardware-design.html#what-is-the-voltage-tolerance-of-gpios-of-esp-chips).

## Protocol and configuration

Packets are `05 STATE CHECK`, where `CHECK = 05 + STATE` (hex). OFF is
`05 00 05`; Supply uses states `19/1A/1B`, Exhaust `09/0A/0B` for speeds 1–3.
The working transmitter uses **618 baud, 8E2, non-inverted TX, 97 ms cadence**.
618 baud is bench-derived, not a manufacturer's specification.

Original OUT captures decode inverted; RX retains that independent candidate
setting. The polarity discrepancy is unresolved, and working TX does not
validate RX. OUT diagnostics are observation only, never acknowledgement or
motor feedback. They become stale after two seconds without a valid frame.
No RX connection is needed for control.

| Substitution | Default |
| --- | --- |
| `tech_name`, `display_name` | `eco-flo-bedroom`, `Bedroom ERV` |
| `erv_board`, `erv_variant` | `esp32-c6-devkitc-1`, `esp32c6` |
| `erv_tx_pin`, `erv_rx_pin` | `GPIO1`, `GPIO2` |
| `erv_tx_inverted`, `erv_rx_inverted` | `false`, `true` |
| `erv_baud`, `erv_frame_interval` | `618`, `97ms` |
| `erv_enable_recovery` | `false` |
| `erv_log_level`, `erv_diagnostics_interval` | `DEBUG`, `5s` |
| `erv_log_uart`, `erv_log_baud` | `USB_SERIAL_JTAG`, `115200` |

The deadline scheduler avoids the original 112 ms component-loop rounding;
late callbacks send once without catch-up bursts. Blocking work can still
delay packets. Timing overrides are experimental: framing must remain 8E2,
baud 550–700, with at least 48 bit cells per frame interval.

Experimental recovery alternates every 700 transmitted frames. Phase B at
speeds 2/3 is inferred, not captured or physically verified. Phase restarts on
power/mode changes are implementation policy, not established factory behavior.
Keep recovery disabled until separately tested. Night/humidity policy, passive
ventilation, filter status and simultaneous remote control are out of scope.

## Package import

The remote wrapper must override the local component source. Keep both refs
aligned and use a single-source mapping so it replaces the local source list:

```yaml
packages:
  erv: github://alexlmiller/esphome-projects/eco-flow-erv/eco-flow-erv.yaml@main
external_components:
  source:
    type: git
    url: https://github.com/alexlmiller/esphome-projects
    ref: main
    path: eco-flow-erv/components
  components: [eco_flow_erv]
```

## Development and logs

Use the pinned ESPHome 2026.8.2 environment from the repository root. For a
fresh checkout, copy `secrets.example.yaml` to `eco-flow-erv/secrets.yaml` for
validation only; do not overwrite existing secrets or flash placeholder keys.

```sh
esphome config eco-flow-erv/eco-flow-erv.yaml
python eco-flow-erv/tests/test_config.py
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined \
  eco-flow-erv/tests/test_protocol.cpp -o /tmp/erv-protocol-test
/tmp/erv-protocol-test
esphome compile eco-flow-erv/eco-flow-erv.yaml
```

Compile does not flash. Tests cover protocol fixtures, parser resynchronization,
independent power/mode, arming, polarity overrides, cadence and timer wrap.

DEBUG logs include state changes and five-second TX/RX/cadence summaries.
VERBOSE adds packets; VERY_VERBOSE adds raw RX bytes and may disturb timing.
Counters describe software queues, not measured wire timing or command success.
USB logging uses the native USB peripheral, not the ERV UART; use network logs
to avoid USB serial-open resets.

## Remaining checks

- Validate the permanent electrical interface/power arrangement and wire levels.
- Test controller reset, power loss and disconnection before unattended use.
- If wanted, validate protected OUT monitoring and experimental recovery.

Capture tables, board photos and the dated test history are in
[BENCH-NOTES.md](BENCH-NOTES.md) and [research-pics/](research-pics/).
[DESIGN.md](DESIGN.md) preserves superseded proposals, not build instructions.

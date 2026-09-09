# Eco-Flo ERV — experimental UART prototype

For Alex's CFM Eco-Flo / Vents TwinFresh Comfo RA1-50-2 bedroom ERV.
Related: [issue #7](https://github.com/alexlmiller/esphome-projects/issues/7).

**OUT decoding is established; ESP32 control through IN is not yet proven.**
This is bench firmware, not a validated unattended installation. Nothing here
automatically flashes, deploys, or connects to the ERV.

## V1 scope

- Requested fan: independent on/off and speeds 1–3.
- Requested airflow mode: Supply or Exhaust.
- Optional experimental Recovery: alternate phases every 700 transmitted frames.
- Separate RX observation: last decoded OUT frame and whether traffic is fresh.
- Explicit control-frame enable switch; disabled at every boot.

Night mode, native humidity thresholds/readings, passive ventilation, filter
status, and simultaneous physical-remote control are out of scope.

The fan entity is **requested state**, not measured motor feedback. Mode changes
and speed-only commands while off do not power on. A Home Assistant action that
explicitly includes `state: on` does power on when armed. Settings are not
restored across reboots; boot defaults are OFF, speed 1, Supply, transmission
disabled. Wi-Fi/API loss does not deliberately reboot the controller; the local
stream continues while the ESP32 is running and armed.

## Board and signal allocation

The NanoC6 has enough pins for control **and** monitoring:

| Proposed pin | Logical role | ERV connection, through an appropriate interface |
| --- | --- | --- |
| GPIO1 / Grove white | UART TX | IN |
| GPIO2 / Grove yellow | UART RX | OUT |

These are independent directions of one full-duplex UART. Monitoring adds one
signal GPIO, not another pair. The M5Stack ATOM and Seeed options can also be
considered, but their exact models must be identified before choosing pins.
Source: [official NanoC6 pin map](https://docs.m5stack.com/en/core/M5NanoC6).

This table is a **logical allocation, not authorization for a direct hookup**.
ESP32 GPIOs are 3.3 V signals; the ERV controller was measured at 5 V. An input
protection/level-conversion interface is required for RX, and the electrical
drive requirements of IN still need establishing. Isolation, power source,
interface polarity, and grounding must be settled before connecting the two.
The Grove red wire is 5 V power; it is not another signal or an approved ERV
power input. No internal ERV power tap is selected in this prototype.

The recordings are inverted **as captured**. TX and RX inversion are separately
configurable because a level-conversion/isolation stage can change polarity.
The multimeter averages do not establish waveform peaks or resolve the apparent
polarity discrepancy with the trace. Verify both sides of the chosen interface.

Use the **control board's labeled IN/OUT/GND terminals**. The mains PCB also has
an XT1 marking; terminal-block numbers alone are not sufficient identification.
Never connect ESP TX to ERV OUT, and do not loop ERV OUT directly back to IN.

## How the prototype behaves

1. Boot: receive monitoring runs; no control frames are sent.
2. Enable control frames: begins repeating OFF. It does not start the fan.
3. Select airflow mode/speed, then explicitly turn Requested fan on.
4. Turn Requested fan off: repeats `05 00 05` while control remains enabled.
5. Disable control frames: stops sending and clears requested power.

**Muting TX is not a stop command.** The ERV may retain its last state when
frames stop, the ESP32 resets, or the cable fails; that behavior is untested.
Use the fan OFF command and confirm the actual response before muting during
bench tests. There is no proven loss-of-controller failsafe yet.

The enable switch gates packets, **not the electrical connection**: UART setup
still drives an idle voltage on TX. Start monitor-only testing with IN physically
disconnected. Disabling the switch does not make the TX pin high impedance.

Packets are sent once per scheduler interval without catch-up bursts. State
changes take effect at the next interval (normally within about 97 ms plus loop
latency); an already transmitting packet is not interrupted. Repeated frames
continue even in OFF. RX never changes the requested fan, starts transmission,
or acts as an acknowledgement. A valid OUT frame only demonstrates bus traffic;
its relationship to accepted IN commands and physical airflow is unverified.
After two seconds without a checksum-valid OUT frame, diagnostics become stale.
No separate RX connection is required to transmit; diagnostics remain stale if
OUT is absent. RX can run alone with the control gate off.

## Configuration

Canonical local entrypoint: `eco-flow-erv.yaml`. Pinned repo ESPHome version:
2026.8.2. The package defaults to NanoC6 / ESP-IDF / 4 MB flash.

| Substitution | Default | Purpose |
| --- | --- | --- |
| `tech_name`, `display_name` | `eco-flo-bedroom`, `Bedroom ERV` | Identity |
| `erv_board`, `erv_variant` | `esp32-c6-devkitc-1`, `esp32c6` | Board overrides |
| `erv_tx_pin`, `erv_rx_pin` | `GPIO1`, `GPIO2` | Signal pins |
| `erv_tx_inverted`, `erv_rx_inverted` | `true`, `true` | As-captured candidate polarity |
| `erv_baud` | `618` | Measured timing candidate; not proven nominal baud |
| `erv_frame_interval` | `97ms` | Frame start cadence |
| `erv_enable_recovery` | `false` | Expose experimental recovery option |

UART uses 8 data bits, even parity, 2 stop bits. The component rejects other
framing at validation; runtime setup rejects baud outside 550–700 or intervals
shorter than 48 bit times. Tuning these values is a bench experiment, not a
claim that every combination is accepted by the ERV. Nested fan options include
`out_timeout` (default `2s`) and `recovery_phase_packets` (default `700`).

Recovery starts in phase A on power-on or a mode change. Speed-only changes
preserve its phase/count. Those restart rules are **our implementation policy**,
not measured factory behavior. Delayed loops extend a phase rather than sending
queued catch-up packets. Phase B at speeds 2 and 3 is inferred, not captured;
this is why recovery requires explicit opt-in. Absolute phase direction may
depend on the ERV's jumper configuration; do not label it verified intake/exhaust.

### Local validation and compile

From the repository root, using its isolated Python environment:

```sh
python -m pip install -r requirements.txt
cp secrets.example.yaml eco-flow-erv/secrets.yaml
esphome config eco-flow-erv/eco-flow-erv.yaml
esphome compile eco-flow-erv/eco-flow-erv.yaml
python eco-flow-erv/tests/test_config.py
c++ -std=c++17 -Wall -Wextra -Werror eco-flow-erv/tests/test_protocol.cpp -o /tmp/erv-protocol-test
/tmp/erv-protocol-test
```

The example secrets are placeholders for validation, **not flash-ready Wi-Fi/API
credentials**. The generated build is not ready to install in the ERV. Compile
does not flash. Tests cover captured/inferred fixtures separately, parser
resynchronization, off-state independence, arming, recovery scheduling, delayed
loops, and timer wrap.

Local validation on 2026-09-09: all 10 configuration tests and the sanitized C++
protocol/state-machine suite passed; the NanoC6 ESP-IDF firmware compiled with
ESPHome 2026.8.2. This verifies software/build behavior, not IN acceptance or
electrical compatibility. No flash, OTA, or ERV transmission was performed.

### Remote package wrapper (after the component is published)

Local component paths resolve relative to the consuming YAML, so a remote
wrapper must override the source as well as import the package. Use the same
reviewed ref for both; `main` is appropriate only once this package is merged:

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

Keep the single-source mapping form shown above: it replaces the package's
local-source list during merging rather than appending another source. This
merge shape is exercised by the wrapper configuration tests. No infra wrapper
or lev-haos deployment has been added yet.

## Next hardware gate

1. Select/verify the electrical interface and power/ground arrangement.
2. With IN disconnected, verify ESP RX reproduces known OUT packets and stale
   detection. This also checks RX polarity and clock compatibility.
3. With the ESP TX isolated from the ERV, capture its OFF/active frames and verify
   actual wire timing, parity, stop bits, idle level, and interface output levels.
4. With the interface approved, test IN with repeated OFF, then explicit ON at
   speed 1. Observe motor response separately from OUT.
5. Test all six Supply/Exhaust speed combinations and OFF; establish jumper
   dependence, physical direction, and whether OUT follows IN while slaved.
6. Test controller loss/reset/disconnection and recovery behavior before any
   unattended installation. Night/humidity settings do not block core testing.

See [BENCH-NOTES.md](BENCH-NOTES.md) for raw evidence and remaining uncertainty.
The original [DESIGN.md](DESIGN.md) is historical and contains superseded wiring
assumptions; do not use it as a build procedure.

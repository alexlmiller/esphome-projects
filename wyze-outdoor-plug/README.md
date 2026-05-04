# Wyze Outdoor Plug (WLPPO1)

ESPHome firmware for the Wyze Outdoor dual-outlet smart plug. Reverse-engineered from scratch — there is no upstream package or component.

> **Status**: ✅ Stable. In service on 5+ outdoor plugs.

## What this gives you

Per dual-outlet unit:

- Two GPIO-driven relays with independent control
- Per-relay restore mode: **Off / On / Last / Opposite** — the "Opposite" mode flips state from whatever the unit was in before reboot, useful for "fail open" or "fail closed" wiring
- HLW8012 power monitoring: amps, volts, watts, daily energy
- Two physical button toggles (one per outlet)
- LDR-based daylight binary sensor (good for "turn on at dusk" automations)
- Per-relay status LEDs that mirror relay state
- Top-of-unit status LED for ESPHome heartbeat

## Hardware

The Wyze Outdoor Plug uses an internal ESP32 (esp-wrover-kit class) with an HLW8012 power-monitoring IC. The two outlets share one HLW8012 and one LDR; they are controlled by independent relays and read by independent push buttons.

> **Flashing the stock unit**: requires opening the housing and connecting to the ESP32 via UART jumper pads. The first flash is over wires; OTA after that. Hardware teardown and pinout reverse-engineering details are out of scope here — search "Wyze Outdoor Plug ESPHome flash" for community guides.

## Usage

### As a `github://` package import (recommended)

```yaml
substitutions:
  display_name: "Wyze Outdoor 1 - Hay Barn"
  tech_name: wyze-outdoor-1-hay-barn
  current_res: "0.001"
  voltage_div: "770"

packages:
  base: github://alexlmiller/esphome-projects/wyze-outdoor-plug/wyze-outdoor-plug.yaml@main
```

### Required substitutions

| Name | Description |
|---|---|
| `display_name` | Human-readable friendly name |
| `tech_name` | Hostname (lowercase, hyphen-separated, ≤31 chars) |
| `current_res` | HLW8012 current-resistor calibration. Higher → lower watt reading. Default `0.001`. |
| `voltage_div` | HLW8012 voltage-divider calibration. Lower → lower voltage reading. Default `770`. |

### Optional substitutions

| Name | Default | Notes |
|---|---|---|
| `syslog_host` | `127.0.0.1` | Override + add a `syslog:` block in your wrapper to enable remote UDP logging |

## Calibration

The default `current_res: "0.001"` and `voltage_div: "770"` work reasonably well out of the box. To dial in:

1. Plug a known load (e.g. a 60 W incandescent bulb on a Kill-A-Watt) into one outlet
2. Compare the Kill-A-Watt reading to the ESPHome `Watts` sensor
3. Adjust `current_res` (lower = higher watts) until they match
4. Repeat for voltage with a multimeter on the line side

The package includes a baseline 2-point linear calibration (`0 W → 0 W`, `134 W → 58 W`) to handle non-linearity at low loads. Override the entire `power.filters.calibrate_linear` block in your wrapper if you need a different curve.

## Adding remote syslog

```yaml
substitutions:
  syslog_host: "10.0.0.5"

packages:
  base: github://alexlmiller/esphome-projects/wyze-outdoor-plug/wyze-outdoor-plug.yaml@main

udp:
  id: syslog_udp
  addresses: ${syslog_host}

syslog:
  udp_id: syslog_udp
  port: 5515
  level: INFO
  strip: true
```

## Pin reference

| Function | GPIO |
|---|---|
| Relay 1 | 15 |
| Relay 2 | 32 |
| Button 1 | 18 |
| Button 2 | 17 |
| Relay 1 LED | 19 |
| Relay 2 LED | 16 |
| HLW8012 SEL | 25 |
| HLW8012 CF | 27 |
| HLW8012 CF1 | 26 |
| LDR ADC | 34 |
| Status LED | 5 |

# Mitsubishi CN105 Mini-Split Controller

Native ESPHome control of Mitsubishi indoor units via the CN105 service port.
Wraps the [echavet/MitsubishiCN105ESPHome](https://github.com/echavet/MitsubishiCN105ESPHome)
external component with production wiring for the M5Stack NanoC6.

> **Status**: ✅ Stable. In service on three MSZ-EF mini-splits.

## Hardware

- **M5Stack NanoC6** — ESP32-C6, 4 MB flash, single-core RISC-V
- **CN105 cable** — wired into the indoor unit's CN105 service connector
- Powered from the indoor unit's 5 V rail via the CN105 connector — no external power needed

### Wiring (Grove → CN105)

| M5 Grove pin | CN105 pin | Function |
|---|---|---|
| 5V (red) | 1 | Power |
| GND (black) | 4 | Ground |
| G1 / GPIO1 | 5 | NanoC6 TX → indoor RX |
| G2 / GPIO2 | 2 | NanoC6 RX ← indoor TX |
| – | 3 | Unused (12V) |

Pin 3 (12V) on the indoor unit is **not** connected. CN105 idles high; G2 is also a C6 boot-strap pin but a high-idle UART line does not block boot.

## What this package gives you

- Climate entity with full mode + fan-speed + swing support
- ISEE motion sensor exposure (when supported by the indoor unit)
- Sub-mode, stage, compressor frequency diagnostics
- `set_remote_temperature` / `use_internal_temperature` services for using a separate room temperature sensor (e.g. an Apollo R Pro mmWave) — the unit regulates against the remote temp for 30 min after each update, then falls back to its own air-return sensor
- WiFi/uptime diagnostics, restart button, captive-portal AP fallback
- Optional remote syslog (UDP) when the consuming wrapper sets `syslog_host`

## Usage

### As a `github://` package import (recommended)

```yaml
substitutions:
  tech_name: mitsubishi-living-room
  display_name: "Living Room Mini Split"

packages:
  base: github://alexlmiller/esphome-projects/mitsubishi-cn105/mitsubishi-cn105.yaml@main
```

### Required substitutions

| Name | Description |
|---|---|
| `tech_name` | Hostname (lowercase, hyphen-separated, ≤31 chars) |
| `display_name` | Human-readable friendly name |

### Optional substitutions

| Name | Default | Notes |
|---|---|---|
| `syslog_host` | `127.0.0.1` | Override + add a `syslog:` block in your wrapper to enable remote UDP logging |

## Adding remote syslog

The package declares a `time:` source already, so adding syslog in your wrapper is a one-block addition:

```yaml
substitutions:
  syslog_host: "10.0.0.5"

packages:
  base: github://alexlmiller/esphome-projects/mitsubishi-cn105/mitsubishi-cn105.yaml@main

udp:
  id: syslog_udp
  addresses: ${syslog_host}

syslog:
  udp_id: syslog_udp
  port: 5515
  level: INFO
  strip: true
```

## External component pinning

The package pins `echavet/MitsubishiCN105ESPHome` to its latest stable release,
`@2026.5.1`. This release includes the sensor API and uptime implementation
changes required by ESPHome 2026.8.

## Compatible indoor units

Tested on Mitsubishi MSZ-EF wall-mount units. Most modern Mitsubishi indoor units with a CN105 service port should work, but `outside_air_temperature_sensor`, `input_power_sensor`, and `kwh_sensor` may or may not report depending on the outdoor unit. The package leaves those off by default; add them in your wrapper if your hardware supports them.

## References

- [echavet/MitsubishiCN105ESPHome](https://github.com/echavet/MitsubishiCN105ESPHome) — upstream component
- [CN105 protocol writeup](https://github.com/SwiCago/HeatPump) — original reverse-engineering

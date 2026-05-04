# Emporia Vue (16-channel whole-home energy monitor)

ESPHome config for the Emporia Vue Energy Monitor — a 16-channel whole-home power monitor that clamps onto your service entrance and individual circuit breakers. Wraps [emporia-vue-local/esphome](https://github.com/emporia-vue-local/esphome) with production wiring.

> **Status**: ✅ Stable. In service on two panels (garage subpanel, house external).

## What this package gives you

- Full ESP32 boilerplate (esphome, esp32, framework, OTA, API, WiFi)
- I²C bus configuration for the energy-monitor IC
- Phase A and Phase B voltage / frequency / phase-angle sensors
- Main panel CT clamp power readings (the two big ones around your service mains)
- RTTTL buzzer + Two Beeps button
- Status LED
- WiFi/uptime diagnostics, restart button, captive-portal AP fallback
- Shared filter chains (`*throttle_avg`, `*throttle_time`, `*positive_only`, `*invert_positive`) for use in wrapper-defined ct_clamps
- Optional remote syslog (when wrapper sets `syslog_host` and adds the syslog block)

## What you provide in your wrapper

The per-instance circuit list. The package leaves this to wrappers because every Emporia Vue install has a different set of circuits wired — there's no useful default.

For each circuit you want to monitor:

1. Add a `ct_clamps:` entry under the existing `emporia_vue` sensor in your wrapper
2. Add a `copy` sensor for the smoothed power output
3. Add a `total_daily_energy` sensor for cumulative kWh
4. (Optional) Add a `template` sensor to compute the panel balance after subtracting your monitored circuits

See the [Wrapper template](#wrapper-template) section for copy-paste blocks.

## Usage

### As a `github://` package import (recommended)

```yaml
substitutions:
  tech_name: emporia-vue-house
  display_name: "Emporia Vue - House"

packages:
  base: github://alexlmiller/esphome-projects/emporia-vue/emporia-vue.yaml@main

# Wrapper extends the sensor: list with per-circuit ct_clamps + copy + energy.
# See "Wrapper template" below.
sensor:
  - platform: emporia_vue   # extends the existing emporia_vue sensor
    i2c_id: i2c_a
    ct_clamps:
      - phase_id: phase_a
        input: "1"
        power:
          id: cir1
          device_class: power
          state_class: measurement
          unit_of_measurement: "W"
          filters:
            - *positive_only
            - multiply: "2"   # 240V circuit; use "1" for 120V
  # … copy sensors and total_daily_energy below
```

### Required substitutions

| Name | Description |
|---|---|
| `tech_name` | Hostname (lowercase, hyphen-separated, ≤31 chars) |
| `display_name` | Human-readable friendly name |

### Optional substitutions

| Name | Default | Notes |
|---|---|---|
| `syslog_host` | `127.0.0.1` | Override + add a `syslog:` block in wrapper to enable remote UDP logging |
| `timezone` | `America/Denver` | IANA TZ string for the SNTP time component |

## Wrapper template

Per circuit, add three blocks. Copy + paste, adjust circuit number / id / name / phase / multiply.

### 1. Raw CT clamp reading (under `sensor: - platform: emporia_vue: ct_clamps:`)

```yaml
- phase_id: phase_a            # phase_a or phase_b
  input: "1"                   # 1..16 (CT clamp port)
  power:
    id: cir1                   # used by the copy + energy sensors below
    device_class: power
    state_class: measurement
    unit_of_measurement: "W"
    filters:
      - *positive_only
      - multiply: "2"          # 240V → 2; 120V → 1
```

### 2. Smoothed power copy (top-level under `sensor:`)

```yaml
- platform: copy
  name: "Sauna Power"          # human-readable
  source_id: cir1              # matches the id above
  device_class: power
  state_class: measurement
  unit_of_measurement: "W"
  accuracy_decimals: 1
  filters: [*throttle_avg]
```

### 3. Daily energy (top-level under `sensor:`)

```yaml
- platform: total_daily_energy
  name: "Sauna Energy"
  power_id: cir1
  device_class: energy
  state_class: total_increasing
  unit_of_measurement: "kWh"
  accuracy_decimals: 3
  restore: false
  filters:
    - multiply: 0.001          # Wh → kWh
    - *throttle_time
```

### Optional: panel total + balance

If you want a `Panel Total Power` (sum of phase A and B) and `Panel Balance Power` (total minus your monitored circuits, i.e. "everything else"), add these template sensors plus their copies:

```yaml
sensor:
  - platform: template
    name: "Panel Total Power Internal"
    id: total_power
    lambda: 'return id(phase_a_power).state + id(phase_b_power).state;'
    update_interval: never
    device_class: power
    state_class: measurement
    unit_of_measurement: "W"
    accuracy_decimals: 1
    internal: true

  - platform: template
    name: "Panel Balance Power Internal"
    id: balance_power
    lambda: |-
      return max(0.0f, id(total_power).state -
        id(cir1).state -
        id(cir2).state);
        // … add subtractions for every monitored circuit
    update_interval: never
    device_class: power
    state_class: measurement
    unit_of_measurement: "W"
    accuracy_decimals: 1
    internal: true

  - platform: copy
    name: "Panel Total Power"
    source_id: total_power
    device_class: power
    state_class: measurement
    unit_of_measurement: "W"
    accuracy_decimals: 1
    filters: [*throttle_avg]

  - platform: copy
    name: "Panel Balance Power"
    source_id: balance_power
    device_class: power
    state_class: measurement
    unit_of_measurement: "W"
    accuracy_decimals: 1
    filters: [*throttle_avg]

  - platform: total_daily_energy
    name: "Panel Total Daily Energy"
    power_id: total_power
    device_class: energy
    state_class: total_increasing
    unit_of_measurement: "kWh"
    accuracy_decimals: 3
    restore: false
    filters:
      - multiply: 0.001
      - *throttle_time

  - platform: total_daily_energy
    name: "Panel Balance Daily Energy"
    power_id: balance_power
    device_class: energy
    state_class: total_increasing
    unit_of_measurement: "kWh"
    accuracy_decimals: 3
    restore: false
    filters:
      - multiply: 0.001
      - *throttle_time

  - platform: emporia_vue
    i2c_id: i2c_a
    on_update:
      then:
        - component.update: total_power
        - component.update: balance_power
```

The `on_update` trigger ensures the template sensors recompute every time the underlying CT clamps refresh.

## Calibration

The default `calibration: 0.022` for both phases works for a typical 200 A North American panel. For 100 A or 400 A services, or non-NA voltage standards, follow the [emporia-vue-local calibration guide](https://github.com/emporia-vue-local/esphome#calibration).

## External component pinning

The package tracks `emporia-vue-local/esphome@dev` with a 1-day refresh. The `dev` branch is the canonical default per the upstream maintainer's recommendation. If you need stability over latest features, override in your wrapper:

```yaml
external_components:
  - source: github://emporia-vue-local/esphome@<commit-hash-or-tag>
    components:
      - emporia_vue
    refresh: never
```

## Hardware reference

- **MCU**: ESP32 (board `esp32dev`)
- **I²C SDA / SCL**: GPIO 21 / 22
- **Buzzer**: GPIO 12 (LEDC) + GPIO 27 (GND)
- **Status LED**: GPIO 23

The Emporia Vue PCB exposes UART headers internally for first flash. After that, OTA is reliable. Search "Emporia Vue ESPHome flash" for community teardown guides.

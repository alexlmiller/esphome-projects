# Emporia Vue (16-channel whole-home energy monitor)

ESPHome config for the Emporia Vue Energy Monitor — a 16-channel whole-home power monitor that clamps onto your service entrance and individual circuit breakers. Wraps [emporia-vue-local/esphome](https://github.com/emporia-vue-local/esphome) with production wiring.

> **Status**: ✅ Stable. In service on two panels.

## What this package gives you

The base package handles ESP32 boilerplate so wrappers don't have to:

- esphome / esp32 / framework / OTA / API / WiFi / captive portal / web server
- External component reference for `emporia_vue`
- I²C bus configuration on GPIO 21/22 (id `i2c_a` — wrapper's emporia_vue sensor references this)
- Time source (SNTP, override timezone via `${timezone}` substitution)
- Persistent energy storage tuning (6h flash-write interval)
- RTTTL buzzer + Two Beeps button
- Status LED on GPIO 23
- WiFi/uptime diagnostics, restart button, captive-portal AP fallback

## What you provide in your wrapper

The full `emporia_vue` sensor block — phases + CT clamps for the main panel and every monitored circuit, plus copy sensors for smoothing and `total_daily_energy` for kWh tracking.

The package intentionally does **not** declare the emporia_vue sensor: ESPHome's package merging would create two conflicting sensor instances if both the package and the wrapper tried to declare one. Keeping the sensor entirely in the wrapper avoids that.

This means each wrapper is ~150–200 LOC, but the boilerplate (~150 LOC) is no longer duplicated across instances.

## Usage

```yaml
substitutions:
  tech_name: emporia-vue-house
  display_name: "Emporia Vue - House"

packages:
  base: github://alexlmiller/esphome-projects/emporia-vue/emporia-vue.yaml@main

# YAML anchors for shared filters — defined here, used below
.default_filters:
  - &throttle_avg
    throttle_average: 5s
  - &throttle_time
    throttle: 60s
  - &positive_only
    lambda: 'return max(x, 0.0f);'
  - &invert_positive
    lambda: 'return max(-x, 0.0f);'

sensor:
  # The actual emporia_vue sensor — phases + main panel CTs + per-circuit CTs
  - platform: emporia_vue
    i2c_id: i2c_a
    phases:
      - id: phase_a
        input: BLACK
        calibration: 0.022
        voltage:
          name: "Panel Phase A Voltage"
          device_class: voltage
          state_class: measurement
          unit_of_measurement: "V"
          accuracy_decimals: 1
          filters: [*throttle_avg, *positive_only]
      - id: phase_b
        input: RED
        calibration: 0.022
        voltage:
          name: "Panel Phase B Voltage"
          device_class: voltage
          state_class: measurement
          unit_of_measurement: "V"
          accuracy_decimals: 1
          filters: [*throttle_avg, *positive_only]
    ct_clamps:
      # Main panel A
      - phase_id: phase_a
        input: "A"
        power:
          id: phase_a_power
          device_class: power
          state_class: measurement
          unit_of_measurement: "W"
          filters: [*invert_positive]
      # Main panel B
      - phase_id: phase_b
        input: "B"
        power:
          id: phase_b_power
          device_class: power
          state_class: measurement
          unit_of_measurement: "W"
          filters: [*invert_positive]
      # Per-circuit clamps — repeat for each connected circuit (1..16)
      - phase_id: phase_a
        input: "1"
        power:
          id: cir1
          device_class: power
          state_class: measurement
          unit_of_measurement: "W"
          filters:
            - *positive_only
            - multiply: 2.0   # 240V circuit; use 1.0 for 120V

  # Smoothed copy sensors for HA dashboards
  - platform: copy
    name: "Panel Phase A Power"
    source_id: phase_a_power
    device_class: power
    state_class: measurement
    unit_of_measurement: "W"
    accuracy_decimals: 1
    filters: [*throttle_avg]

  - platform: copy
    name: "House Heat Pump Power"
    source_id: cir1
    device_class: power
    state_class: measurement
    unit_of_measurement: "W"
    accuracy_decimals: 1
    filters: [*throttle_avg]

  # Daily energy
  - platform: total_daily_energy
    name: "House Heat Pump Energy"
    power_id: cir1
    device_class: energy
    state_class: total_increasing
    unit_of_measurement: "kWh"
    accuracy_decimals: 3
    restore: false
    filters:
      - multiply: 0.001   # Wh → kWh
      - *throttle_time
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

## Per-circuit template

Copy this triplet of blocks for each circuit you have wired (replace circuit number, name, phase, multiplier):

```yaml
# Under sensor: - platform: emporia_vue: ct_clamps:
- phase_id: phase_a            # phase_a or phase_b
  input: "1"                   # 1..16 (CT clamp port)
  power:
    id: cir1                   # used by the copy + energy sensors below
    device_class: power
    state_class: measurement
    unit_of_measurement: "W"
    filters:
      - *positive_only
      - multiply: 2.0          # 240V → 2.0; 120V → 1.0

# Top-level under sensor:
- platform: copy
  name: "Sauna Power"          # human-readable
  source_id: cir1              # matches the id above
  device_class: power
  state_class: measurement
  unit_of_measurement: "W"
  accuracy_decimals: 1
  filters: [*throttle_avg]

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

## Optional: panel total + balance

If you want a `Panel Total Power` (sum of phase A and B) and `Panel Balance Power` (total minus your monitored circuits, i.e. "everything else"), add this on top of the per-circuit blocks. The `on_update` trigger is needed so the template sensors recompute every time the underlying CT clamps refresh — and it has to be on the same `emporia_vue` sensor instance:

```yaml
sensor:
  - platform: emporia_vue
    i2c_id: i2c_a
    on_update:
      then:
        - component.update: total_power
        - component.update: balance_power
    phases: …    # as before
    ct_clamps: … # as before

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
```

## Calibration

The default `calibration: 0.022` works for a typical 200 A North American panel. For 100 A or 400 A services, or non-NA voltage standards, follow the [emporia-vue-local calibration guide](https://github.com/emporia-vue-local/esphome#calibration).

## External component pinning

The package tracks `emporia-vue-local/esphome@dev` with a 1-day refresh — this is the canonical default per the upstream maintainer's recommendation. If you need stability over latest features, override in your wrapper:

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

The Emporia Vue PCB exposes UART headers internally for first flash. After that, OTA is reliable.

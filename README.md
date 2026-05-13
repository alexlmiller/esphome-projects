# ESPHome Projects

A collection of custom ESPHome device configurations and build notes.

These configs are designed to be importable as packages from anywhere — point a
device YAML at `github://alexlmiller/esphome-projects/<device>/<device>.yaml`
and override substitutions to suit your setup. Each subfolder is
self-contained: a build README, the canonical YAML, and any helper assets
(images, iOS Shortcuts, calibration data) that belong with that device.

## Devices

| Device | Hardware | Status | Link |
|---|---|---|---|
| Smart Scale | Waveshare ESP32-S3-Mini + HX711 + 4× load cells + SSD1309 OLED + LIS3DH + TPS61023 | 🚧 In progress | [smart-scale/](smart-scale/) |
| Mitsubishi CN105 mini-splits | M5Stack Atom + CN105 cable | 📦 Planned migration | [mitsubishi-cn105/](mitsubishi-cn105/) |
| Standing Desk Controller | ESP32-S3 Mini or M5Stack NanoC6 + DeskUp Pro RJ12 board | ✅ Stable | [desk-controller/](desk-controller/) |
| Sauna Controller | Custom ESP32 + thermostat + RS-485 panel transport | 🔗 External repo | [sauna-controller/](sauna-controller/) |

Status legend: 🚧 in progress · ✅ stable · 📦 planned migration · 🔗 external repo

## Using these configs

### As a `github://` package import (recommended)

The cleanest way to consume these is via ESPHome's `packages:` import — the
upstream YAML carries the device logic, your local wrapper carries identity
and secrets:

```yaml
# your-fleet/devices/scale-bathroom.yaml
substitutions:
  device_id: scale-bathroom
  user_1_name: "Alex"
  user_2_name: "Sam"
  user_1_target_kg: "81.6"
  user_2_target_kg: "70.3"

packages:
  base: github://alexlmiller/esphome-projects/smart-scale/smart-scale.yaml@main

api:
  encryption:
    key: !secret api_key

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

ESPHome merges your overrides on top of the upstream config, so you can pin
to a specific tag/commit (`@v1.2.0` instead of `@main`) for stability and bump
when you're ready.

### As a clone-and-go starting point

Each subfolder also works standalone:

```bash
git clone https://github.com/alexlmiller/esphome-projects
cd esphome-projects/smart-scale
cp ../secrets.example.yaml secrets.yaml
# edit secrets.yaml with your wifi/api credentials
esphome run smart-scale.yaml
```

## Conventions

- **Substitutions**: every device YAML exposes its tunable values
  (hostname, user names, calibration constants, thresholds) at the top of the
  file. Override them from your wrapper rather than forking.
- **Units**: internal math is metric (kg, °C). Display layers and HA dashboards
  can render in any unit.
- **Secrets**: `!secret` references everything. Never inline a key.
- **Logging**: devices that need centralized logs have a syslog block; replace
  the IP with your collector's address.
- **Adapted upstreams**: packages derived from community firmware record their
  reviewed upstream baseline in [`upstreams.json`](upstreams.json). The
  scheduled upstream watcher opens an issue when a source project publishes a
  newer release, so the porting review happens next to the package code.
- **Dependency updates**: Renovate tracks GitHub Actions, the pinned ESPHome
  validation toolchain in [`requirements.txt`](requirements.txt), and
  version-tagged ESPHome `external_components` declared as
  `source: github://owner/repo@v1.2.3`. Branch refs such as `@main` or `@dev` are
  intentionally manual because they do not represent a reviewable release bump.
- **License**: MIT — see [LICENSE](LICENSE).

## Contributing

These are personal builds; PRs welcome but priorities follow my own use cases.
Issues are good for typos, broken links, or compatibility breaks against newer
ESPHome versions. For bigger feature ideas, open a discussion before a PR.

# Mitsubishi CN105 Mini-Split Controller

> **Status**: 📦 Planned migration — currently maintained in
> [alexlmiller/infra](https://github.com/alexlmiller/infra) under
> `roles/esphome_devices/files/devices/mitsubishi-*.yaml` +
> `roles/esphome_devices/files/packages/mitsubishi-cn105-base.yaml`.

Native ESPHome control of Mitsubishi MSZ-EF mini-splits via the CN105 service
port — replaces Sensibo with direct local control. Three units in service:
Living Room, Master Bedroom, Alex Office.

Migration to this repo is pending. Once landed, the canonical YAML will be
importable as:

```yaml
packages:
  base: github://alexlmiller/esphome-projects/mitsubishi-cn105/mitsubishi-cn105-base.yaml@main

substitutions:
  device_id: mitsubishi-living-room
  friendly_name: "Living Room AC"
```

## Reference

- Cutover PR in infra: [alexlmiller/infra#710](https://github.com/alexlmiller/infra/pull/710)
- Upstream ESPHome CN105 component: <https://github.com/echavet/MitsubishiCN105ESPHome>

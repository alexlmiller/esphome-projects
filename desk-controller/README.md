# Standing Desk Controller

> **Status**: 📦 Planned migration — design tracked in
> [alexlmiller/infra#717](https://github.com/alexlmiller/infra/issues/717).

Wires a Maidesite-protocol standing desk into Home Assistant via the desk's
RJ12 control port. Exposes the desk as a cover entity (proportional height
slider) plus M1–M4 memory buttons.

## Hardware

- M5Stack NanoC6 (ESP32-C6, 4 MB flash)
- Powered from the desk's 5 V rail via the RJ12 cable — no separate USB-C power
- Grove 4-pin → flying-leads cable + 6P6C RJ12 cable, spliced once

## Approach

Imports [smarthomeguys/DeskUp-Pro-Controller-RJ12](https://github.com/smarthomeguys/DeskUp-Pro-Controller-RJ12)
as the upstream package. Their UART encode/decode logic (height-byte unpacking,
M1–M4 preset round-trip) is mature; this subfolder will only carry local
substitutions and any fleet-specific customizations.

Once landed, the canonical YAML will be importable as:

```yaml
packages:
  upstream: github://smarthomeguys/DeskUp-Pro-Controller-RJ12/desk-controller.yaml
  local: github://alexlmiller/esphome-projects/desk-controller/desk-controller.yaml@main
```

## Reference

- Tracking issue: [alexlmiller/infra#717](https://github.com/alexlmiller/infra/issues/717)
- Upstream project: <https://github.com/smarthomeguys/DeskUp-Pro-Controller-RJ12>

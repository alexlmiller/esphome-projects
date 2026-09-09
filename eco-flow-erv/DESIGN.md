# Eco-Flo ERV — Historical Preliminary Design (Superseded)

> **Historical proposal, not current wiring/build instructions.** The OUT bus
> was decoded on 2026-09-09. Use [README.md](README.md) for the narrowed UART
> prototype and [BENCH-NOTES.md](BENCH-NOTES.md) for evidence and unknowns.
> The text below is preserved to explain the original investigation, but contains
> disproven assumptions: XP1 is six pins/two jumpers, not a three-pin relay tap;
> XP5 is a motor connector, not an approved power supply tap; the NanoC6 exposes
> two Grove signal GPIOs, not 6–8 general expansion pins. No XP1 relay, IR tap,
> native sensor taps, or mains-power modification is part of the current build.

This document is the full upfront design for retrofitting an ESP32 inside a
CFM Eco-Flo / Vents TwinFresh Comfo **RA1-50-2** ductless ERV so it can be
controlled and observed from Home Assistant. It is reviewed against the
[user's manual](comfo-manual.pdf) and the photos in [`research-pics/`](research-pics/).

The companion `mitsubishi-cn105` package in this repo is the closest reference
for shape (M5 NanoC6, climate-like entity, lambdas for services).

---

## 1. Goals

The user controls and signals we want exposed to HA:

| Capability | Manual states | Direction |
|---|---|---|
| **Power** | On / Off | bi-directional |
| **Mode** | Off / Passive / In (supply) / Out (exhaust) / Mixed (regeneration) | bi-directional |
| **Speed** | 1 / 2 / 3 | bi-directional |
| **Night mode** | On / Off | bi-directional |
| **Humidity threshold** | 45 / 55 / 65 % | bi-directional |
| **Indoor humidity** | live reading | read-only |
| **Ambient light** | live reading | read-only |
| **Filter-replace flag** | needs cleaning / OK | read-only |
| **Filter timer reset** | one-shot | write-only |

Stretch: power consumption (CT clamp), shutter-open state, current "phase" of
the regeneration cycle (supply leg vs exhaust leg).

---

## 2. Hardware findings (from photos)

Two PCBs, screwed to a mounting plate behind the wall-mounted air duct.

### 2.1 Mains PCB — `003-…100BB-01`

Visible in [`IMG_1485`](research-pics/IMG_1485.jpeg):

| Marking | What it is |
|---|---|
| `XT1` / `XT2` (2-screw) | AC mains input — `L`, `N` (120 V 60 Hz) |
| `F1` (T1.5A 150V WMT) | Line fuse |
| `F2` | Second fuse position |
| Florten brick | AC → low-voltage DC PSU (12 V suspected, TBD) |
| `XP2` (white JST) | DC + signal wires to main control PCB |

### 2.2 Main control PCB — `004-650120088` (silkscreen "RB 50")

Visible in [`IMG_1483`](research-pics/IMG_1483.jpeg), [`IMG_1486`](research-pics/IMG_1486.jpeg), [`IMG_1488`](research-pics/IMG_1488.jpeg):

| Marking | What it is |
|---|---|
| `XP1` (3-pin header + jumper) | **Direction select** for Ventilation mode — silkscreen `Air inflow` / `Air outflow`. *Currently set to `Air inflow`.* This is the "CN7" jumper from the manual. |
| `XT1/XT2` (blue 4-screw) | Master-slave signal bus: `Out / GND / In / GND`. *Unused.* |
| `XP2` (multi-pin JST) | Fan motor harness |
| `XP3` `XP4` `XP5` (small JST) | Various peripherals: IR receiver + light sensor, humidity sensor, shutter actuator, indicator LEDs. *Not yet individually mapped — TBD in Phase 0.* |
| Silkscreen `PWR ON +12` near `XP5` | **Likely a 12 V rail tap** — TBD verify |
| `D1` | LED on a stalk through the front panel (filter-replace or power) |
| Round white speaker grille | Onboard buzzer |

### 2.3 Front-panel peripherals (per manual)

- IR receiver + ambient light sensor (combined assembly, behind the small dark
  port at the bottom of the front grille)
- Humidity sensor (behind the small vent openings on the front)
- Power-status LED
- Filter-replacement LED

### 2.4 No WiFi expansion option

The Comfo product line has **never** offered a WiFi module — confirmed by
reading the current Vents-US Comfo catalog/datasheet, and by the absence of
any unpopulated WiFi-shaped header on the PCBs. The WiFi-enabled Vents units
(TwinFresh Expert WiFi, TwinFresh Atmo Mini, TwinFresh Style WiFi) use
different mainboards and a Modbus-over-UDP stack — not portable to this unit.

This rules out a "drop-in WiFi accessory" approach. We're soldering.

---

## 3. Control-surface analysis

Three plausible ways into the unit, ranked by preference:

| Approach | Pros | Cons |
|---|---|---|
| **A. Master-slave terminal (`In`/`Out`/`GND`)** | Officially-supported wired control input. **Zero PCB modification** — wires land on the existing `XT2` screw block. Pure electrical, no air-gap. Listening on the same line gives free state-mirroring of remote presses. | Protocol is **not publicly documented**. Need to capture this unit's `Out` line while pressing IR-remote buttons, then decode + replay. Feasible from one unit assuming the master broadcasts even without a slave attached (per manual wording, it does). |
| **B. IR-receiver-line tap** | One wire to sniff + (maybe) inject. Reuses everything the IR remote already does. | Requires identifying the IR receiver's output pin on the PCB. Injecting depends on whether the line is open-collector with a pull-up (likely, but TBD). |
| **C. IR LED transmit** | No PCB modification needed for command path. | Air-gap reliability inside a closed housing. Doesn't sniff physical-remote presses. |

**Tapping onboard slide switches** was considered and ruled out — the manual
explicitly says onboard switches can only do speeds 2/3 and three of four
modes; Speed 1, Passive mode, and Night mode are remote-only and the user
wants all three.

**Decision**: pursue **(A)** first. If Phase 0 decode succeeds, the install
becomes radically simpler — two terminal-block wires plus the XP1 relay,
no PCB soldering. Fall back to **(B)** if the protocol can't be cracked
from a single-unit capture. **(C)** is a last-resort fallback if both fail.

### 3.1 Mapping the user's three modes to commands

With the **XP1 jumper relay** controlling direction:

| HA state | IR command | XP1 relay |
|---|---|---|
| Off | `Power Off` | don't care |
| In (supply) | `Air supply` | don't care (Air supply ignores XP1) |
| Out (exhaust) | `Ventilation` | `Air outflow` |
| Mixed (regeneration) | `Heat recovery` | don't care |
| Passive (shutters open, fan idle) | `Passive air supply` | don't care |

The IR `Ventilation` button is the only mode that respects XP1; everything
else is direction-fixed. So driving XP1 from a relay only matters when we
want Out mode without surrendering Mixed mode (which we do).

---

## 4. Architecture

```
                    ┌──────────────────────────────────────────────┐
                    │  Eco-Flo unit (existing)                     │
                    │                                              │
   120V AC ─────────┼─▶ Mains PCB ──── 12V ────┐                  │
                    │                          │                  │
                    │            Main control PCB                 │
                    │  ┌────────────────────────────────────┐     │
                    │  │ XP1 jumper ──┐                     │     │
                    │  │              ▼ SPDT relay         │     │
                    │  │           [ESP32]──── 3.3V buck ◀─┘     │
                    │  │              ▲                          │
                    │  │              │ GPIO                     │
                    │  │ XT2 In/Out ──┤ (primary: master-slave)  │
                    │  │ IR rx out ───┤ (fallback: sniff/inject) │
                    │  │ Humidity ────┤ (analog or I²C)          │
                    │  │ Light sense ─┤ (analog)                 │
                    │  │ Filter LED ──┤ (digital input)          │
                    │  │ Power LED ───┤ (digital input)          │
                    │  └──────────────┴────────── WiFi ──────────┼─▶ HA
                    │                                            │
                    │  Optional: CT clamp on AC feed ────────────┼─▶ HA
                    └────────────────────────────────────────────┘
```

### 4.1 MCU choice

**M5Stack NanoC6** (ESP32-C6). Rationale:

- Already in the fleet (used by `mitsubishi-cn105`), so we have a working
  flash workflow and known-good board profile.
- Compact (about 30 × 30 mm) — fits inside the unit housing.
- WiFi 6 + BLE; native USB-C for first flash.
- Plenty of GPIOs for the wire count we need (~6–8 signals).

Alternates considered:

- ESP32-C3 Super Mini (~$2, smaller still) — viable backup if we want a
  cheaper bake-once-and-forget board. Less mature with ESP-IDF + ESPHome
  combo in our fleet today.
- ESP32-S3 Mini — used by `desk-controller` and `smart-scale`. Overkill on
  flash + PSRAM for this job; physically larger.

### 4.2 Power

Preferred path: tap **12 V from the unit's internal rail** (likely on or near
`XP5` per silkscreen) → onboard buck/LDO to **3.3 V** for the NanoC6.

Fallback: add a **Hi-Link HLK-PM03 (3.3V, 3W)** AC/DC module wired in
parallel with the mains terminal block. Independent of the unit's PSU.
Trades complexity for safety isolation — also useful if the unit's 12V rail
turns out to be poorly regulated or current-limited.

### 4.3 XP1 relay

Single **SPDT signal relay**, coil driven by an ESP GPIO via NPN/MOSFET
driver. Wiring:

- Cut/remove physical jumper from `XP1`.
- Solder three flying leads to the three header pins.
- Relay COM ← middle pin.
- Relay NC ← `Air inflow` pin (so power-loss safe state defaults to In).
- Relay NO ← `Air outflow` pin.
- ESP coil drive = HIGH for Out mode, else LOW.

Alternative: **CMOS analog switch (74HC4053 or similar)** if the XP1 signal
turns out to be quiet logic-level. Smaller, no mechanical wear. Decision in
Phase 0 once we measure XP1 voltage and current.

### 4.4 IR receiver tap

If the IR receiver is a standard TSOP-style 3-pin module (VCC / GND / OUT),
its output is an open-collector / push-pull logic line that idles HIGH and
pulls LOW during IR bursts. Tap with one wire to a GPIO configured as input
with internal pull-up disabled (the receiver provides it).

Inject by adding an open-drain MOSFET or 1N4148 from a GPIO to the OUT line
— pulling it low fakes IR pulses. Carrier-modulation is moot at this point
because the receiver already demodulated. We just replay timing.

If injection turns out to be unsafe or unreliable on the shared line, fall
back to a front-facing **IR LED** wired through a series resistor and NPN,
aimed at the receiver across a few mm of air.

### 4.5 Sensor taps

Identify in Phase 0:

- **Humidity sensor** — likely on a daughter board or short flying lead.
  If it's a 4-pin I²C device (SHT-style), tap SDA/SCL and add an ESP I²C
  master. If 3-pin analog (HIH-4030-style), tap the OUT pin to an ESP ADC.
- **Light sensor** — likely a photoresistor or photodiode on a 2-wire lead
  near the IR receiver. Read as ADC, or as digital with a threshold
  comparator depending on the conditioning circuit.
- **Filter LED** + **Power LED** — tap the cathode side, read as digital
  inputs (LED on/off ↔ logic level).

---

## 5. Firmware design (ESPHome)

The package will follow the same shape as `mitsubishi-cn105`: base YAML in
this repo with substitutions, wrapper in `infra` pinning identity and
secrets.

### 5.1 Entities exposed to HA

```
fan.eco_flo_<room>                  — speed (Off/Low/Med/High) and direction
select.eco_flo_<room>_mode          — Off / Passive / In / Out / Mixed
switch.eco_flo_<room>_night_mode    — on/off
select.eco_flo_<room>_humidity_set  — 45% / 55% / 65%
sensor.eco_flo_<room>_humidity      — % RH
sensor.eco_flo_<room>_ambient_light — lux (or raw)
binary_sensor.eco_flo_<room>_filter_dirty
button.eco_flo_<room>_filter_reset
sensor.eco_flo_<room>_power         — W (if CT clamp added)
```

Internal helpers (not exposed):

- `script_send_ir` — takes a command enum, emits the correct pulse train.
- `globals.last_known_mode` — restore-after-reboot bookkeeping.
- `text_sensor.last_command_source` — `wifi` / `physical_remote`, useful for
  debugging.

### 5.2 IR transmit strategy in ESPHome

Use the `remote_transmitter:` component if it's a known protocol (NEC).
If the captured codes don't map to a built-in protocol, use a `raw:`
transmission with the pulse train hard-coded — the same pattern Tasmota uses
for proprietary HVAC remotes.

For receive: `remote_receiver:` with a `lambda:` decoder that matches the
exact codes captured in Phase 0 and updates the matching entity. Bonus: a
`text_sensor` that publishes the last command source so we can tell whether
HA or the physical remote triggered a state change.

### 5.3 Substitutions (planned)

```yaml
substitutions:
  tech_name: eco-flo-bedroom            # hostname
  display_name: "Bedroom ERV"
  board: m5stack-nanoc6
  ir_tx_pin: GPIO1
  ir_rx_pin: GPIO2
  xp1_relay_pin: GPIO3
  humidity_pin: GPIO4                   # or i²c, TBD Phase 0
  light_pin: GPIO5
  filter_led_pin: GPIO6
  power_led_pin: GPIO7
  syslog_host: "127.0.0.1"
```

---

## 6. Phase 0 — Bench investigation (mandatory before final build)

Done with the unit on the bench, cover off, mains energized for parts of it.
Capture results in `BENCH-NOTES.md` (created during this phase).

1. **🟢 Master-slave protocol capture (blocking — gates approach choice)**
   - Connect scope/logic-analyzer to `XT2 Out` (with respect to `XT2 GND`).
     Press every button on the IR remote in known sequence (Off, Speed 1/2/3,
     Air supply, Ventilation, Heat recovery, Passive, Night, Humidity
     L/M/H).
   - Capture each transmission. Look for: a recognizable bit pattern, frame
     boundaries, repetition behavior, idle level, voltage swing.
   - **Goal**: determine whether the master emits a decodable signal at all
     (manual implies yes; not yet confirmed) and whether the framing is
     simple enough to replay from an ESP GPIO.
   - **Decision gate**: if the signal looks like a UART, NEC-style pulse
     train, or other tractable encoding → adopt approach (A), drop most of
     steps 3 and 4 below. If the signal looks rolling/CRC/complex → fall back
     to approach (B).
2. **Verify 12 V tap**
   - Multimeter from `XP5`-area pads to GND. Look for a steady 12 V (or
     whatever the actual rail is). Document pinout.
3. **Capture IR remote codes** *(only if step 1 fails)*
   - Aim the supplied remote at a generic 38 kHz IR receiver wired to a
     scratch ESP32 running `remote_receiver:` with `dump: all`. Record codes
     for every button. Identify protocol family.
4. **Identify IR receiver output line on the PCB** *(only if step 1 fails)*
   - Locate the IR receiver module on the main PCB (likely a small TSOP-style
     part near the front-panel side). Probe its output pin while pressing
     remote buttons. Confirm logic level (3.3 V vs 5 V) and idle-high
     behavior. Pick the tap point.
5. **Inject test pulse on IR output line** *(only if step 1 fails)*
   - With unit powered, drive the line low briefly through a 470 Ω resistor
     from a 3.3 V scratch GPIO. Confirm the unit responds (e.g., beeps or
     changes mode) — proves we can co-drive the line. If not, fall back to
     external IR LED.
6. **Measure XP1 jumper signal**
   - Voltage on each pin with respect to GND, in both jumper positions.
     Confirm whether it's a high-impedance digital sense line (relay is
     overkill but safe) or carries any current (relay required).
7. **Identify humidity sensor + light sensor**
   - Visual inspection + probe wires from the small front-panel JST(s) on
     the main PCB. Determine 2-wire/3-wire/4-wire and likely interface.
8. **Map Power LED + Filter LED drive lines**
   - Probe the LED cathodes / current-set resistors. Determine drive
     polarity and idle level.

Each step has a binary outcome — either the design proceeds as drafted, or
the bench notes flag a deviation (e.g., "12 V tap not viable → use HLK-PM03",
"IR injection not safe → add IR LED").

---

## 7. Build sequence (post-Phase 0)

1. Order BOM (NanoC6, signal relay, MOSFET, passives, optional HLK-PM03,
   small perfboard, 26-AWG flying leads, heat-shrink).
2. Build the daughter board on a 20 × 30 mm perfboard.
3. Flash NanoC6 with the Phase-1 ESPHome YAML over USB-C.
4. Bench-verify with mains energized: IR sniff, IR inject, XP1 relay.
5. Stage humidity + light sensor reads.
6. Final fitment inside the unit casing; route wires; close up.
7. Add HA dashboard (mirror the `desk-controller`/`mitsubishi-cn105`
   pattern).

---

## 8. Open risks / TBDs

| # | Risk | Mitigation |
|---|---|---|
| 1 | IR-line injection may double-drive against the unit's own IR receiver and cause a brown-out spike | Series resistor + open-drain inject; bench-verify before permanent install |
| 2 | XP1 might carry mains-adjacent voltage (unlikely but unverified) | Phase 0 step 5 — measure first |
| 3 | Humidity sensor may be on a non-standard interface | Worst case: drop humidity read from v1, use HA's room sensor instead |
| 4 | The unit may not have a clean 12 V rail accessible | Fallback: HLK-PM03 module on the mains feed |
| 5 | "Out" mode requires the relay path; if we screw up XP1 wiring, default-NC = inflow keeps the unit safely useful | Wire NC to Air inflow as the safe default |
| 6 | Onboard buzzer beeps audibly on IR commands — could be noisy if HA sends many commands | If annoying, find buzzer line and add an ESP-controlled mute switch |

---

## 9. Sources

- [Vents TwinFresh Comfo RA1-50-2 user manual](comfo-manual.pdf) (saved locally)
- [Vents TwinFresh Expert RA1-50-2 install guide](expert-manual.pdf) (saved locally — useful for terminal-block conventions across the line)
- [HVAC Direct — Comfo RA1-50-2 product listing](https://hvacdirect.com/vents-us-twinfresh-comfo-ra1-50-2.html)
- [Vents-US — Comfo RA1-50-2 product page](https://vents-us.com/product/twinfresh-comfo-ra1-50-2-120v-60hz/)
- [Continental Fan Manufacturing](https://www.continentalfan.com) — the CFM Eco-Flo brand

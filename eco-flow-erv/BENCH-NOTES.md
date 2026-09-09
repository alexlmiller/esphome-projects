# OUT protocol findings — 2026-09-09

Six recordings from Alex's bedroom CFM Eco-Flo / Vents TwinFresh Comfo RA1-50-2.
Physical analyzer channel 1 is **D0** in the sigrok session, sampled at 1 MHz.
D1's short correlated glitches were treated as pickup, not another protocol.

## Established from captures

All **3,424 complete packets** passed direct raw start/parity/stop-bit checks,
the candidate additive checksum, and agreement with sigrok UART decoding.
Partial packets at recording boundaries were excluded. They contain eight
distinct complete packet values; no rolling counter appears in these captures.

```text
05 STATE CHECK
CHECK = 05 + STATE
```

All values are hexadecimal. First-byte meaning is unknown. No checksum overflow
case was observed. Bit 08 appears in all active examples but is not independently
proven to be a generic power bit. OFF is the entire state byte 00; do not invent
zero-speed active states such as 08/18 or label them passive ventilation.

| Requested state / phase | Speed 1 | Speed 2 | Speed 3 |
| --- | --- | --- | --- |
| Supply | `05 19 1E` | `05 1A 1F` | `05 1B 20` |
| User-selected Exhaust / recovery A | `05 09 0E` | `05 0A 0F` | `05 0B 10` |
| Recovery B | `05 0D 12` | **`05 0E 13` inferred** | **`05 0F 14` inferred** |

OFF: `05 00 05`, captured after both supply and recovery operation.

- Inverted **as recorded**, idle low, eight data bits LSB-first, even parity.
- Two stop-bit intervals between characters: consistent with 8E2 or 8E1 plus
  one extra idle bit. A 618-baud inverted 8E1 sigrok decoder reads the stream.
- Measured bit time approximately 1.618 ms (~618 baud).
- Frame starts approximately every 97.0–97.2 ms (60 bit cells).
- Three 12-bit characters occupy about 58 ms; approximately 39 ms idle follows.
- Recovery alternated 09 / 0D at speed 1, with **700 packets per full phase**.
  Measured full phases: 67.947375 and 67.937413 seconds.
- A nominal 600 baud / 100 ms / 70 seconds with a common clock offset is
  plausible, not established. The capture does not assign the offset to the
  ERV versus analyzer. Do not treat 618 baud as a manufacturer's specification.

No OFF packet appears between recovery phase flips. That does not establish
motor braking/coasting behavior. Alex confirmed physical reversal during the
long recording, but phase-to-physical-direction mapping was not timestamped.
The supply flag 10 is not a universal direction bit: recovery reverses without it.

## Capture index and transitions

Times are starts of complete packets, not button presses or physical responses.
Original files remain in Alex's Downloads; they are not duplicated in this repo.

| File relative to Downloads | Duration | Packets | Significant transitions (seconds: state) |
| --- | ---: | ---: | --- |
| `test 1/t.sr` | 10 s | 102 | Supply 19 → 1B at 2.999325 |
| `test 3/2.sr` | 50 s | 514 | 1B → 1A 1.730230 → 19 17.376897 → 00 34.188667 → 19 42.743730 |
| `test 4/3.sr` | 24.32 s | 250 | 19 → 09 6.543047 → 19 21.111686; recovery selection in between is indistinguishable |
| `recovery test.sr` | 159.70304 s | 1,645 | 19 → 09 3.035556 → 0D 70.982931 → 09 138.920344 |
| `test 5.sr` | 51.89632 s | 534 | 09 → 0A 4.618674 → 0B 15.200130 → 09 27.144375 → 0A 41.515883 → 0B 45.885755 → 00 49.668800 |
| `test 6.sr` | 36.864 s | 379 | 00 → 0B 2.209916 → 09 17.346871 |

SHA-256, in the same order:

```text
73121b916dac137eeeb22a4c85f9cc51f3c3237bcc08c4af5c9a0e56a8d3c39b
ead73d473c78e86d524c304b360655208e4588ffebfae7b5b6498c88f1f8f449
caa3887b98e98e0847d60666d7e4d1b979e812299e170df9b1cd8a3921ab70ce
61c0717dd500d1816dc2270727c8e567eec13a2c73acfac2ee0f9ce0f80b66ca
7013224a8610b2618ac2db95dc4fb44cf3d7391c06c6e3d3fa2daa324ab1f320
3b905c612b56fde94725cec5670309bcb64ac7d91d8e0d9d1fc2f382228a8364
```

For independent UART decoding of an original recording:

```sh
sigrok-cli --input-file 'test 6.sr' \
  --protocol-decoders uart:rx=D0:baudrate=618:invert_rx=yes:parity=even:data_bits=8 \
  --protocol-decoder-annotations uart=rx-data:rx-warnings:rx-parity-err \
  --protocol-decoder-samplenum
```

The firmware's checksum parser is not equivalent to the offline raw parity/stop
validation; framing is delegated to the hardware UART.

## Power, humidity, and night behavior

Alex observed that a remote mode selection does not wake an off unit: explicit
power-on is needed. OUT transitions directly from OFF to the active state; no
additional ON handshake was observed. **This is not evidence that IN requires
a separate ON command.** The prototype keeps desired power independent from mode.

Test 6 pressed power, night/day, humidity low/medium/high, then night/day twice.
Only the already-known speed-3 to speed-1 change appeared on OUT. Alex noticed a
physical slowdown but could not attribute it to night versus humidity. Exact
button timing and light conditions were not established; long/short beep meaning
is unassigned. These features probably affect the master's selected output
state, but native policy flags or sensor values have not been decoded.

## Hardware observations and remaining unknowns

- ATmega88PA controller; user measured 5 V at the 1117 regulator.
- FAVOTEK PM30X10-S120A is labeled 12 V. A related PSU family's isolation spec
  is not independent certification of this exact assembled board/setup.
- OUT/GND DC averages: 2.8 V off, 3.15 V high, 3.45 V low. AC readings on the
  same pair varied. Neither establishes waveform peak voltage or safe input
  levels for an ESP32. The voltage averages and inverted trace are not yet
  reconciled into a verified terminal polarity model.
- An unplugged continuity check found no connection between mains neutral/
  ground and OUT ground. This is not a mains-rated isolation test.
- XP1 is six pins/two jumpers. XP5 is motor PWM/DIR/+12, not an approved power
  supply tap. No relay or jumper modification is required by this prototype.

**Not tested:** IN acceptance/wake-up, input electrical requirements, synthetic
master timing tolerance, slave switch/jumper requirements, OUT behavior while
slaved, actual response latency/direction, and loss-of-transmission behavior.
Monitoring OUT must not be advertised as an acknowledgement or fan tachometer.

The user only needs power, speed, and direction, and accepts ESP32-only control.
The remaining gate is a protected hardware bench test, not more broad remote
button decoding. See README for the ordered test plan.

## USB-only NanoC6 flash — 2026-09-09

User confirmed USB connection to the development Mac with Grove disconnected,
and approved reuse of existing infra ESPHome credentials. Credentials were
rendered into ignored, mode-0600 local secrets; neither secrets nor generated
firmware images are committed.

- Target identified over USB as ESP32-C6FH4 revision 0.2 with 4 MB embedded flash.
- Source: `559d3d0` on `codex/eco-flow-erv-prototype`, ESPHome 2026.8.2 / ESP-IDF
  5.5.5, DEBUG USB logging. Build completed with the approved credentials.
- Optional 4 MB pre-flash backup failed twice with "Serial data stream stopped"
  at requested 460800 and 115200 baud. **No rollback image was obtained.**
- Standard USB upload at requested 115200 baud succeeded; esptool verified the
  written data hash and reset the device. No separate full-chip erase was used.
- Serial setup completed successfully with GPIO1 TX / GPIO2 RX, 618 baud, 8E2.
  Both observed boots logged an initial Wi-Fi authentication failure followed
  by a successful retry. Reopening the USB serial port produced a fresh boot;
  use network logs for follow-up observation without further serial opens.
- Encrypted native API access succeeded with expected device name and MAC
  checks. All five exposed entities returned their expected boot states:
  Requested fan OFF, speed 1; Requested airflow mode Supply; Enable control
  frames OFF; OUT traffic present false; Observed OUT frame "No OUT data".
- Initial five-second diagnostics consistently reported control OFF, TX=0,
  RX=0 and no checksum candidates. This is software counter evidence, not a
  logic-analyzer measurement of GPIO voltage or idle level.

No fan, mode or arming commands were sent, and the Nano was not connected to
ERV IN, OUT or power. USB-only bring-up is verified; the protected interface,
isolated TX waveform check and actual IN-control test remain outstanding.

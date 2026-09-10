# NanoC6 standalone timing verification — 2026-09-09

User-approved analyzer-only test. Nano GPIO1/white connected to analyzer D1;
analyzer GND to Nano GND/black. Both disconnected from the unplugged ERV.
Nano powered separately; analyzer attached to the development Mac.
All recordings use 1 MHz sampling. Red and yellow Grove wires unused.
These files contain logic samples, **not** credentials or firmware images.

`before-supply-2-1mhz.sr`: all eight channels recorded, D1 active; 3 seconds,
27 complete `05 1A 1F` packets, median frame period 112.003 ms.

Each active `fixed-*.sr` recording is 3 seconds with D1 selected. Measurements
exclude incomplete packets at recording boundaries. Decoding used inverted
UART at 618 baud, 8 data bits, even parity, 2 stop bits. All complete frames
have the expected additive checksum and no parity/framing warnings. Initial
partial-packet synchronization warnings occur in some files.

| Capture suffix | Packet | Complete frames | Min / median / max frame interval (ms) |
| --- | --- | ---: | --- |
| off | `05 00 05` | 30 | 96.956 / 97.004 / 97.052 |
| supply-1 | `05 19 1E` | 30 | 96.957 / 97.003 / 97.056 |
| supply-2 | `05 1A 1F` | 30 | 96.904 / 97.002 / 97.105 |
| supply-3 | `05 1B 20` | 31 | 96.942 / 97.005 / 97.047 |
| exhaust-1 | `05 09 0E` | 30 | 96.955 / 97.001 / 97.053 |
| exhaust-2 | `05 0A 0F` | 30 | 96.893 / 97.003 / 97.123 |
| exhaust-3 | `05 0B 10` | 31 | 96.835 / 96.9995 / 97.242 |
| off-final | `05 00 05` | 30 | 96.957 / 97.002 / 97.059 |

`fixed-boot-disarmed-1mhz.sr` and `fixed-final-disarmed-1mhz.sr` each contain
2 seconds / 2,000,000 samples with D1 continuously low and no transitions.
This establishes idle output, not a high-impedance pin.

Example reproduction from this directory:

```sh
sigrok-cli --input-file fixed-supply-2-1mhz.sr \
  --protocol-decoders uart:rx=D1:baudrate=618:invert_rx=yes:parity=even:data_bits=8:stop_bits=2 \
  --protocol-decoder-annotations uart=rx-data:rx-warnings:rx-parity-err \
  --protocol-decoder-samplenum
```

The fixed firmware was compiled with ESPHome 2026.8.2 at
`2026-09-09 20:00:39 -0600` and installed by successful OTA. Final API state:
fan OFF, speed 1, Supply, control frames disabled. No controls were restored
to the pre-test ON state. The captures do not establish ERV IN voltage
requirements, reception, motor response, or physical airflow direction.

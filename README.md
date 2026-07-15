# ASkompu LilyGO application shell

This branch contains the first hardware-independent ASkompu application shell
around the working LilyGO T-Display S3 prototype. The normative product and
interaction specification is the [ASkompu GitHub Wiki](https://github.com/Mikky100/ASkompu/wiki).

## Architecture

- `src/core/` owns accepted distance pulses, speed state, Trip 1, Trip 2,
  diagnostic total pulse count, calibration editing, navigation state and the
  semantic `DisplayModel`. It has no Arduino, GPIO, TFT, Preferences or display
  resolution dependency.
- `src/domain/` contains reusable calculation and calibration primitives.
- `src/input/` adapts active-low GPIO buttons and the interrupt-driven pulse
  input. `ButtonInterpreter` separates raw electrical state, debounce,
  short/long classification and repeat events and is native-testable.
- `src/ports/ArduinoClock.h` adapts Arduino monotonic time.
- `src/settings/` adapts the calibration setting to Arduino Preferences/NVS.
- `src/ui/` renders the semantic model on the 320x170 LilyGO display. Pixel
  layout remains entirely in this adapter.
- `src/main.cpp` wires the adapters to the core. It contains no application
  calculation or navigation state.

The current drive view preserves the speed and diagnostic prototype. It shows
`TOTAL`, `TRIP 1` and `TRIP 2`; the two trips are independently owned by the
core. The calibration editor remains the only editing view: Up or Down opens
it with a one-step change, Up/Down edit, Left cancels, and Right accepts.

## Build and native tests

Run all deterministic hardware-independent tests:

```powershell
platformio test -e native
```

The tests use injected timestamps and raw button states. They need no real
clock, sleeps, display, NVS or connected board. They cover motion math at 36,
60 and 120 km/h, 1000 and 100 mm/pulse, zero-speed timeout, `micros()` rollover,
the complete generator cycle, distance saturation, both trips, independent
resets, event debounce/short/long handling, calibration navigation, save
requests, validation and semantic display values.

Build the firmware without uploading it:

```powershell
platformio run -e lilygo-t-display-s3
```

## Pulse generator test cycle

With `1000 mm/pulse`, the ESP32-C3 generator's 120-second cycle is:

| Stage | Pulse period | Pulses | Expected speed | Distance |
|---|---:|---:|---:|---:|
| 30 s pulses | 100000 us | 300 | 36 km/h | 300 m |
| 10 s pause | - | 0 | 0 km/h after timeout | 0 m |
| 30 s pulses | 60000 us | 500 | 60 km/h | 500 m |
| 10 s pause | - | 0 | 0 km/h after timeout | 0 m |
| 30 s pulses | 30000 us | 1000 | 120 km/h | 1000 m |
| 10 s pause | - | 0 | 0 km/h after timeout | 0 m |

One cycle produces 1800 accepted pulses and 1800 metres in both trips unless a
trip is reset. Pauses do not increase distance. Speed returns to zero one
second after the last pulse.

## Calibration and NVS

Calibration is displayed and stored in millimetres per pulse. The default is
`1000`, the editing step is `1`, and the validated technical range is
`1...100000`. An accepted change affects only future pulses. Cancel never
writes, and accepting the unchanged value does not request a write.

The existing Preferences namespace and keys are unchanged: namespace
`askompu`, keys `schemaVersion` and `mmPerPulseFixed`. A missing schema, an
unsupported schema or an out-of-range value falls back to `1000 mm/pulse`
without writing during boot.

## Confirmed LilyGO wiring

All listed inputs are active LOW with the existing configuration except the
pulse signal:

| Function | GPIO |
|---|---:|
| Left | 1 |
| Up | 2 |
| Down | 3 |
| Right | 10 |
| Trip 1 reset | 14 |
| Speed/distance pulse | 16 |

No GPIO has been assigned for Point, AT, reverse, Trip 2 reset or foot reset.
Their wiki-defined semantic identifiers may exist in the hardware-independent
event API, but they are not connected by the LilyGO adapter.

## Physical verification still required

Native tests and compilation cannot verify the electrical or visual path. On a
physical LilyGO test board, verify:

1. GPIO1/2/3/10 active-low arrows debounce and calibration navigation.
2. Holding Up/Down performs one initial step and then steady repeat.
3. GPIO14 holds Trip 1 at zero while pressed and never resets Trip 2 or TOTAL.
4. GPIO16 counts every generator pulse without duplicates or loss.
5. Each generator stage shows 36, 60 and 120 km/h, each pause reaches zero, and
   the full cycle ends at 1800 pulses and 1.800 km in both unreset trips.
6. Trip 1 and Trip 2 labels and values are readable in the compact diagnostic
   area without clipping.
7. Calibration cancel leaves NVS unchanged; accept survives a power cycle; an
   accepted change affects only subsequent pulses.
8. Display rotation, backlight, contrast and refresh remain stable for the
   complete 120-second run.

Firmware upload and serial-port testing are intentionally not part of the
automated workflow.

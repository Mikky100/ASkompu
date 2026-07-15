# ASkompu LilyGO application shell

This branch contains a hardware-independent ASkompu application shell and a
320x170 LilyGO T-Display S3 adapter. The normative product and interaction
specification is the [ASkompu GitHub Wiki](https://github.com/Mikky100/ASkompu/wiki).

## Startup and software clock

Every boot starts in an unskippable time-entry view. Time is entered as `H:MM`:
Up/Down changes the active field, Right advances from hours to minutes and
accepts from minutes, and Left returns from minutes to hours. Hours wrap within
`0...23` and minutes within `0...59`, so an invalid value cannot be accepted.

Acceptance sets the clock to exactly `H:MM:00`; elapsed time before acceptance
is ignored. Time is never read from or written to Preferences/NVS, so it must be
entered again after every restart. The current clock is software-only and is
not battery-backed. `core::Clock` separates the application from the clock
implementation; the current `SoftwareClock` uses an injected monotonic
`TimeSource`, and a later RTC adapter can implement the same interface without
changing the core, DisplayModel, or navigation semantics. Unsigned elapsed-time
tracking handles Arduino `millis()` rollover when the clock is serviced by the
normal non-blocking loop.

## Non-competition basic view

After time acceptance the basic view shows:

- clock as `HH:MM:SS`, following the wiki's clock format;
- Trip 1 as the visually dominant value;
- Trip 2 at the same time, with a smaller presentation;
- rounded prototype speed with `km/h`.

It contains no `MENU` or `DEV` text, menu hint, GPIO numbers, button states,
total pulse count, pulse age, calibration data, or debug text. The speed value
is a prototype validation feature and is not a permanent requirement of the
final competition display.

The wiki defines Up/Down, not Right, as the way to open the main menu from the
drive/basic view. Down opens at the first item and Up at the last item. The main
menu wraps; submenus clamp at their ends. Left returns, Right opens or accepts,
and a long Left abandons the current non-startup UI operation and returns to the
basic view.

## Menu tree

The current wiki's complete top-level skeleton is represented. `Unavailable`
items are visible but dimmed and cannot be activated; no success response is
shown for them.

| Main group | Items | Status |
|---|---|---|
| Ajomääräys ja pistevälit | Ajomääräys, Pistevälit | Unavailable: order/competition domain is not implemented yet |
| Kilpailutyyppi ja JAT | Kilpailutyyppi, JAT-tyypit | Unavailable: competition and JAT domain is not implemented yet |
| Kellonaika ja lähtöaika | Kellonaika; Lähtöajan korjaus | Clock editing works; start-time correction needs competition history |
| Mittarikerroin ja mittis | Mittarikerroin; Mittis | Existing mm/pulse calibration works; measurement-course workflow is not implemented |
| Pisteet ja tapahtumat | Jaksojen pisteet, Kokonaispisteet, Tapahtumat | Unavailable: scoring and event repository do not exist yet |
| Näyttöasetukset | Näyttöprofiili, Näyttöselitteet, Aikaeron muoto, Trip-tarkkuus | Unavailable: these persistent settings are not in the current core/store |
| Tripit | Nollaa Trip 1, Nollaa Trip 2, Ulkoinen trip | Both resets work; external display source needs its hardware/protocol adapter |
| Järjestelmä | Diagnostiikka, Painikeasetukset, Muut asetukset | Diagnostics works; persistent button/system settings are not implemented |

Calibration editing uses a draft value. Left cancels without changing the
active value or requesting a write. Right accepts; an unchanged value requests
no NVS write, and one changed accepted value requests one controlled write.
Time editing uses the startup editor semantics, but cancel preserves the old
clock and acceptance resets seconds to `00` without touching NVS.

Trip resets have no confirmation, as required by the wiki. GPIO14 holds only
Trip 1 at zero while pressed. The menu can reset either trip independently.
Neither reset changes total pulse count, and navigation never stops pulse
processing.

## Diagnostics

`Järjestelmä > Diagnostiikka` is an explicitly developmental view. It shows the
debounced Left/Up/Down/Right/GPIO14 states, latest semantic button event,
total/Trip 1/Trip 2 pulse counts, both trip distances, mm/pulse calibration,
speed, last-pulse age and zero-timeout state, current UI state, software-clock
state/time, and elapsed time since clock acceptance. Left closes it. Opening or
viewing diagnostics does not reset trips, change settings, stop the clock, or
stop pulse processing.

## Architecture

- `src/core/` owns the UI state machine, menu selection/scrolling, trips,
  accepted pulses, speed, clock-facing semantics, calibration draft, save
  requests, diagnostics, and semantic `DisplayModel`. It has no Arduino, GPIO,
  TFT, Preferences, or display-resolution dependency.
- `src/core/Clock.*` defines `TimeSource`/`Clock` and the hardware-independent
  rollover-safe software clock.
- `src/domain/` contains calculation, saturation, trip, speed, and calibration
  primitives.
- `src/input/` adapts active-low buttons and interrupt-driven pulses.
  `ButtonInterpreter` is native-testable.
- `src/ports/ArduinoClock.h` is the `millis()`/`micros()` adapter.
- `src/settings/` is the only Preferences/NVS adapter. It stores calibration,
  never wall-clock time.
- `src/ui/` maps DisplayModel values to the LilyGO pixel layout. A screen-sized
  sprite prevents visible full-screen clearing; rendering remains on the
  controlled application refresh cadence.
- `src/main.cpp` wires the adapters and keeps pulse, button, speed, clock, save,
  and display work non-blocking.

## Build and native tests

Run deterministic hardware-independent tests:

```powershell
platformio test -e native
```

Compile the hardware-independent core as a standalone smoke build:

```powershell
platformio run -e native
```

Build firmware without uploading it:

```powershell
platformio run -e lilygo-t-display-s3
```

The native tests use a controlled time source and no wall clock, sleeps,
display, NVS, or board. They cover clock validation/entry/acceptance, restart,
minute/hour/day transitions, multi-day running and rollover; basic DisplayModel
contents; menu opening, wrapping, clamping, scrolling, disabled items and
return paths; clock editing; calibration cancel/accept/failure/write requests;
diagnostics; independent trip resets; pulses in every relevant UI state;
motion math, saturation, debounce, speed timeout, and the generator cycle.

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

One cycle produces 1800 accepted pulses and 1800 metres in both unreset trips.
Pauses do not increase distance and speed returns to zero one second after the
last pulse.

## Calibration and NVS

Calibration is stored in millimetres per pulse. The default is `1000`, the step
is `1`, and the validated technical range is `1...100000`. A change affects
only future pulses. The existing Preferences namespace and keys remain
`askompu`, `schemaVersion`, and `mmPerPulseFixed`. Missing, incompatible, or
out-of-range storage falls back safely to `1000 mm/pulse`.

## Confirmed LilyGO wiring and missing hardware

| Function | GPIO |
|---|---:|
| Left | 1 |
| Up | 2 |
| Down | 3 |
| Right | 10 |
| Trip 1 reset | 14 |
| Speed/distance pulse | 16 |

Arrow and Trip 1 inputs are active LOW. No GPIO has been invented for Point,
AT, reverse, Trip 2 reset, or foot reset. Trip 2 is currently reset from the
menu or a semantic core event. Competition inputs, RTC hardware, external trip
display link, reverse input, scoring/event storage, and order/segment domain
remain physically or logically unavailable.

## Physical verification checklist

1. On every power-up, confirm the device remains in H:MM entry until Right is
   pressed on the minute field; accept `0:00`, `7:05`, `12:30`, and `23:59`.
2. Confirm acceptance starts at exactly `H:MM:00`, the clock advances, and a
   power cycle asks again rather than restoring time from NVS.
3. Inspect the basic view for clock, dominant Trip 1, Trip 2, and speed; confirm
   no MENU, DEV, GPIO, button, total-pulse, calibration, or debug text appears.
4. Confirm Down opens the first wiki menu item, Up opens the last, the main menu
   wraps, long lists scroll, disabled rows cannot be activated, and Left returns.
5. Edit time from the menu: cancel must preserve it; accept must reset seconds
   to `00` and continue from the acceptance instant.
6. Open calibration, test cancel, unchanged accept, changed successful save,
   failed-save indication, and persistence after a power cycle.
7. Reset each trip from the menu; verify the other trip and total pulse count do
   not change. Hold GPIO14 and verify only Trip 1 stays at zero until release.
8. Open diagnostics and exercise all five wired buttons. Verify live states,
   pulse/trip/calibration/speed/clock data, clean Left exit, and no state reset.
9. Run the full generator cycle: observe 36/60/120 km/h, zero in each pause,
   no pause distance, and exactly 1800 pulses / 1.800 km when trips are unreset.
10. While startup entry, menu, calibration, and diagnostics are visible, verify
    pulses continue accumulating and speed/clock continue updating.
11. Inspect display rotation, clipping, font readability, sprite refresh,
    backlight, contrast, and flicker for the full 120-second run.
12. Verify the two boards share GND and 3.3 V logic only; with separate USB
    supplies, do not connect their 5 V pins.

Firmware upload and serial-port testing are intentionally outside the automated
workflow.

# ASkompu LilyGO application shell

This branch contains a hardware-independent ASkompu application shell and a
320x170 LilyGO T-Display S3 adapter. The normative product and interaction
specification is the [ASkompu GitHub Wiki](https://github.com/Mikky100/ASkompu/wiki).

## Competition runtime

TIME, SPEED and MITTIS calculation, start waiting, normal points, immediate
point undo, JAT stages, finish, scoring, AT, additional orders and road breaks
run in RAM. JAT supports `MANNED_JAT`, `EMIT_JAT_OFFSET`, `EMIT_MLA` and
`EMIT_ULA`, including their start-time proposals and physical stage-distance
zero points. MITTIS calibration proposals use integer arithmetic and a changed
factor affects only future pulses.

Events are written in order to a hardware-independent RAM repository. The log
contains point/undo, AT/cancel, JAT, finish, start-time, MITTIS, override,
reverse and trip-reset records. JAT and finish records carry their `StageResult`;
totals use saturating arithmetic. Runtime events and in-progress competition
state are intentionally not restored after power loss yet, and rapidly
changing state is not written to NVS.

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

The current wiki's top-level skeleton is represented. `AJOMAARAYS` is a single
workflow; unavailable unrelated items remain visible but dimmed.

| Main group | Items | Status |
|---|---|---|
| Ajomääräys | Unified creation, browsing, editing, and confirmed replacement | Implemented |
| Kello | Direct clock editing | Implemented |
| Kerroin | Direct mm/pulse calibration editing | Implemented |
| Pisteet ja tapahtumat | Jaksojen pisteet, Kokonaispisteet, Tapahtumat | Implemented from the current RAM event/result data |
| Näyttöasetukset | Näyttöprofiili, Näyttöselitteet, Aikaeron muoto, Trip-tarkkuus, Tekstin väri | Persistent white/red/green text color works; other rows remain unavailable |
| Tripit | Nollaa Trip 1, Nollaa Trip 2, Ulkoinen trip | Both resets work; external display source needs its hardware/protocol adapter |
| Järjestelmä | Diagnostiikka, Painikeasetukset, Muut asetukset | Diagnostics works; persistent button/system settings are not implemented |

Calibration editing uses a draft value. Left cancels without changing the
active value or requesting a write. Right accepts; an unchanged value requests
no NVS write, and one changed accepted value requests one controlled write.
Time editing uses the startup editor semantics, but cancel preserves the old
clock and acceptance resets seconds to `00` without touching NVS.

Trip resets have no confirmation. In the LilyGO test profile GPIO11 resets only
Trip 2; Trip 1 and foot reset remain core operations but have no physical GPIO.
GPIO14 is exclusively the Point button. Neither reset changes total pulse
count, competition distance, or speed calculation.

## Diagnostics

`Järjestelmä > Diagnostiikka` is an explicitly developmental view. It shows the
debounced Left/Up/Down/Right/Point/AT/Trip2 states, latest Point and AT events,
the filtered reverse state,
total/Trip 1/Trip 2 pulse counts, both trip distances, mm/pulse calibration,
speed, last-pulse age and zero-timeout state, current UI state, software-clock
state/time, and elapsed time since clock acceptance. Left closes it. Opening or
viewing diagnostics does not reset trips, change settings, stop the clock, or
stop pulse processing.

## Route-order creation, editing, and storage

`AJOMAARAYS` is now one top-level workflow. With no stored order it first asks
for `EMIT` or `NON-EMIT`, locks that choice, asks for the competition start time,
and then enters ordered TIME, SPEED, or MITTIS segments. Each accepted TIME or
SPEED value continues with `SEURAAVA`, `JAT`, or `MAALI`. TIME accepts 1...3599
seconds and SPEED a two-digit 1...99 km/h value. MITTIS accepts 1000...9999
metres and then a TIME value for that same interval; SPEED is not available as
the MITTIS interval's time rule. A finish is mandatory. Segment browsing starts
with the competition start time and shows both distance and time for MITTIS.
Segment
browsing labels the start point as `L` and the finish point as `M`, for example
`L-1` and `3-M`.

With an existing order, Up/Down browses segments, Right edits the selected
segment, and Left returns or abandons the in-progress edit. A long Right opens
the `UUSI AJOMAARAYS?` confirmation; Left declines and Right starts a separate
replacement draft. Competition type is not part of ordinary editing.

In `WAIT_START` and `RUNNING`, opening `AJOMAARAYS` first shows `MUOKKAA
AJOMAARAYS`. Up/Down toggles to `KORVAA AJOMAARAYS`, Right confirms the selected
workflow, and Left returns to the menu.

The domain model and validator are in `src/domain/RouteOrder.*`. The editor,
binary codec, and UI-independent storage port are in `src/route/`. The ESP32
adapter stores a checksum-protected blob in alternating Preferences slots. It
writes and verifies the inactive payload before advancing its generation, so a
failed write leaves the previous valid generation loadable. Unsupported schema
versions, corrupt payloads, invalid values, invalid point order, incompatible
JAT types, and missing/early finishes are rejected before replacement.

The active order and the editor draft are distinct objects. Creation or editing
does not mutate the active order; only a validated and successfully persisted
draft becomes current. The codec and validator have no UI, Bluetooth, Arduino,
or Preferences dependency and can therefore be reused by a future Android
import adapter.

## Architecture

- `src/core/` owns the UI state machine, menu selection/scrolling, trips,
  accepted pulses, speed, clock-facing semantics, calibration draft, save
  requests, diagnostics, and semantic `DisplayModel`. It has no Arduino, GPIO,
  TFT, Preferences, or display-resolution dependency.
- `src/core/Clock.*` defines `TimeSource`/`Clock` and the hardware-independent
  rollover-safe software clock.
- `src/domain/` contains calculation, saturation, trip, speed and calibration
  primitives plus scoring, event/result models and the RAM event repository.
- `src/input/` adapts active-low buttons, the reverse level, and
  interrupt-driven pulses. `ButtonInterpreter` and the 20 ms continuous-level
  `StableSignalFilter` are native-testable.
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
diagnostics; independent trip resets; Point/AT timing and cancellation;
signed forward/reverse distance; MITTIS calibration; every JAT type and start
proposal; scoring; runtime overrides; result/event browsing; reverse-level
filtering; pulses in every relevant UI state; motion math, saturation, debounce,
speed timeout, and the generator cycle.

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

The same settings repository validates `lateFactor` (default 1), `earlyFactor`
(default 3), `jatResultSeconds` (0...20, default 5) and
`atDisplayDistanceM` (1...20, default 10). Writes compare stored values and
only update changed accepted fields.

## Confirmed LilyGO wiring and missing hardware

| Function | GPIO |
|---|---:|
| Left | 1 |
| Up | 2 |
| Down | 3 |
| Right | 10 |
| Trip 2 reset | 11 |
| AT | 12 |
| Reverse level, active LOW | 13 |
| Point, LilyGO board button | 14 |
| Speed/distance pulse | 16 |

All buttons use active LOW with internal pull-ups. Reverse is a continuous
active-LOW input with a 20 ms stability filter; its stable value is sampled
before each pulse batch. Point long-press starts at 1200 ms without repeat, and
its release cannot also create a short point. Trip 1 reset and foot reset are
not physically connected in this profile. RTC hardware, external trip display
and persistent crash-safe runtime/event storage remain unavailable. Trip 1 is
reset automatically at every JAT and again at the accepted MLA/ULA physical
start point. Trip 2 is never reset by JAT automation.

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
7. Press GPIO11 and verify only Trip 2 resets. Confirm GPIO14 never resets or
   pauses Trip 1 accumulation.
8. Open diagnostics and exercise Point, AT, Trip 2, and reverse. Verify live states,
   pulse/trip/calibration/speed/clock data, clean Left exit, and no state reset.
9. Run the full generator cycle: observe 36/60/120 km/h, zero in each pause,
   no pause distance, and exactly 1800 pulses / 1.800 km when trips are unreset.
10. While startup entry, menu, calibration, and diagnostics are visible, verify
    pulses continue accumulating and speed/clock continue updating.
11. Inspect display rotation, clipping, font readability, sprite refresh,
    backlight, contrast, and flicker for the full 120-second run.
12. Verify the two boards share GND and 3.3 V logic only; with separate USB
    supplies, do not connect their 5 V pins.
13. During a pulse run, pull GPIO13 LOW and verify trip and competition distance
    decrease while displayed speed remains positive; release it and verify
    distance increases without losing a pulse batch.
14. Verify a short GPIO14 press advances one point on release and a press longer
    than 1200 ms opens `LISAMAARAYS` / `TIEKATKO` without creating a point.
15. Drive a MITTIS interval, verify the old/new factor prompt, reject once,
    accept once, power-cycle, and confirm only the accepted factor persists.
16. Verify AT shows the press time, a second AT within three seconds cancels it,
    and the overlay disappears only after one second at at most 3.6 km/h plus
    the configured absolute travel distance in either direction.
17. Exercise every JAT type, including result timeout/Right skip, minute editing,
    EMIT offset acceptance with Point, MLA/ULA distance exclusion and a start
    time crossing midnight.
18. Open stage points, total points and event history; verify cancelled events
    are marked and finish keeps the current frozen clock/delta presentation.

Firmware upload and serial-port testing are intentionally outside the automated
workflow.

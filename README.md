# ASkompu LilyGO test prototype

This branch contains the LilyGO T-Display S3 hardware prototype for the
ASkompu speed pulse, calibrated Trip 1 distance and button inputs.

## Build and test

Run the hardware-independent domain tests:

```powershell
platformio test -e native
```

Build the LilyGO firmware:

```powershell
platformio run -e lilygo-t-display-s3
```

Upload to a connected LilyGO only when hardware testing is intended:

```powershell
platformio run -e lilygo-t-display-s3 -t upload --upload-port COM4
```

## Pulse generator test cycle

With `1000 mm/pulse`, the ESP32-C3 generator cycle is:

| Stage | Pulse period | Pulses | Expected speed | Distance |
|---|---:|---:|---:|---:|
| 30 s pulses | 100000 us | 300 | 36 km/h | 300 m |
| 10 s pause | - | 0 | 0 km/h after timeout | 0 m |
| 30 s pulses | 60000 us | 500 | 60 km/h | 500 m |
| 10 s pause | - | 0 | 0 km/h after timeout | 0 m |
| 30 s pulses | 30000 us | 1000 | 120 km/h | 1000 m |
| 10 s pause | - | 0 | 0 km/h after timeout | 0 m |

One complete cycle produces 1800 pulses and 1800 metres. The pauses do not
increase either Trip 1 distance or its pulse count.

## Calibration

The calibration is stored and displayed in millimetres per pulse. The default
is `1000 mm/pulse`, the editing step is `1 mm/pulse`, and the validated
technical range is `1...100000 mm/pulse`.

Press Up or Down on the drive screen to open the calibration editor. Up
increases and Down decreases the value. Holding either button starts continuous
editing after 500 ms at 10 steps per second. Right accepts the value and writes
it to NVS; Left cancels without writing. An accepted calibration affects only
future pulses and does not recalculate the existing Trip 1 distance.

The Arduino Preferences namespace is `askompu`. The stored keys are
`schemaVersion` and `mmPerPulseFixed`. A missing, unsupported or out-of-range
value falls back to `1000 mm/pulse` without writing to NVS during boot.

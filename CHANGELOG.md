# Changelog

## Unreleased

- Added three persistent menu/label font sizes and removed automatic shrinking
  of individual menu rows and the stray small Trip 1 marker.
- Added persistent Trip 1, Trip 2 and side-by-side Trip 1+2 display selection,
  plus configurable internal and external reset targets.
- Restored the post-JAT countdown relative to the accepted next-stage start
  while preserving the completed-stage reset.
- Moved the ILI9488 profile's four GPIO18-21 inputs to adjacent GPIO39-42 and
  re-enabled native USB CDC/JTAG on GPIO19/20.
- Shortened the drive-view time-adjustment label to `AIKA+-`.
- Added the GPIO8 active-low light switch for the ILI9488 profile. It disables
  GPIO9 display/keyboard lighting without stopping the computer.
- Added a short-Left manual subtraction mode for running SPEED segments, with
  signed reverse-equivalent distance calculation, event logging and a large
  `MIINUSTUS` drive-view warning.
- Added a calibration-dependent 200 km/h pulse-edge filter to reject fast
  electrical glitches before they increase distance.
- Changed the ILI9488 profile's active-low GPIO40 speed input to count the
  fast falling edge instead of the RC network's slower rising edge.
- Added a running-stage time adjustment editor: Right opens, Up/Down changes
  the cumulative ideal time by 10 seconds, Right accepts and Left cancels.
  Accepted adjustments affect stage scoring and are written to the event log.

## 0.1.0 — Prototype

ASkompu 0.1.0 is the first versioned prototype release for hardware and
in-car evaluation. It is not a production-ready or safety-certified navigation
instrument.

### Included

- ESP32-S3 N16R8 + 480×320 ILI9488 and LilyGO T-Display S3 profiles
- TIME, SPEED and single-MITTIS route-order entry and runtime calculation
- JAT variants, AT, finish, scoring, point undo, additional orders and road
  breaks
- persistent route order, calibration and display settings
- independent Trip 1 and Trip 2 calculation and reset inputs
- configurable brightness, text colour, display labels and debug speed
- native regression tests and reproducible PlatformIO firmware builds

### Prototype limitations

- runtime competition state and event history are held in RAM and are not
  restored after power loss
- external trip display transport and protocol are not complete
- dedicated RTC hardware is not integrated
- enclosure, vehicle power supply, EMC, temperature and long-duration road
  testing remain prototype validation work
- the firmware has not been safety certified

# Changelog

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

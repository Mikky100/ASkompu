# Repo map (v0.0.1)

## Juuri
- `platformio.ini` – PlatformIO-ympäristö LilyGo T-Display S3:lle
- `README.md` – käyttö- ja arkkitehtuuriohje (suomi)
- `repo-map.md` – tämä tiedosto

## include
- `include/BoardConfig.h`
  - Keskitetty pinni- ja aktiivisuustasomääritys

## src/app
- `src/app/Application.h/.cpp`
  - Sovelluksen setup/loop
  - Näyttötilat ja navigointi
  - Asetusten muokkauslogiikka

## src/hal
- `src/hal/Display.h/.cpp`
  - TFT_eSPI-käärö
- `src/hal/Buttons.h/.cpp`
  - Debounce + event API (Pressed/Released/Repeat)
- `src/hal/SpeedInput.h/.cpp`
  - Keskeytyspohjainen pulssilaskenta + 200 ms ikkuna
- `src/hal/Storage.h/.cpp`
  - Preferences/NVS-lataus ja -tallennus

## src/domain
- `src/domain/Settings.h`
  - Käyttäjäasetukset
- `src/domain/Theme.h`
  - Teemat ja paletit
- `src/domain/TimeModel.h`
  - Yksinkertainen HH:MM-aikamalli millis()-päivityksellä
- `src/domain/TripModel.h`
  - Trip-matkan laskenta ja reset-offset
- `src/domain/MenuState.h`
  - Ruututila + valikkovalinnat
- `src/domain/Diagnostics.h`
  - Debug-näkymän data

## src/ui
- `src/ui/Layout.h/.cpp`
  - Keskitetyt rectit + fonttikoot
- `src/ui/Renderer.h/.cpp`
  - Kaikkien ruutujen piirto

## src
- `src/ArduinoSketch.cpp`
  - Arduino setup()/loop()-entrypoint

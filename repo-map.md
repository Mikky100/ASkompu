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
  - Suora ruutunavigointi ilman erillistä valikkolistaa
  - Käynnistyksessä avaa aina kelloruudun

## src/hal
- `src/hal/Display.h/.cpp`
  - TFT_eSPI-käärö
- `src/hal/Buttons.h/.cpp`
  - Debounce + event API
- `src/hal/SpeedInput.h/.cpp`
  - Keskeytyspohjainen pulssilaskenta + 200 ms ikkuna
- `src/hal/Storage.h/.cpp`
  - Preferences/NVS tallentaa vain kertoimen ja teeman

## src/domain
- `src/domain/Settings.h`
  - Käyttäjäasetukset
- `src/domain/Theme.h`
  - Teemat (punainen/vihreä/sininen)
- `src/domain/TimeModel.h`
  - HH:MM-aikamalli millis()-päivityksellä
- `src/domain/TripModel.h`
  - Trip-laskenta ja reset-offset
- `src/domain/PointsModel.h`
  - Pisteiden placeholder-malli (v0.0.1 minimi)
- `src/domain/MenuState.h`
  - Aktiivinen ruutu
- `src/domain/Diagnostics.h`
  - Debug-näkymän data

## src/ui
- `src/ui/Layout.h/.cpp`
  - Keskitetyt rectit + fonttikoot
- `src/ui/Renderer.h/.cpp`
  - Pääruutu + asetusruudut + debug

## src
- `src/main.ino`
  - Arduino setup()/loop()-entrypoint

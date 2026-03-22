# AS-kompu v0.0.1 (firmware skeleton)

AS-kompu v0.0.1 on PlatformIO-pohja LilyGo T-Display S3 -laitteelle.
Tavoite on testirepon kaltainen perustoiminta ilman kosketusta:
nuolinäppäimillä suoraan ruudusta toiseen, ilman erillistä listavalikkoa.

## Käytös nyt (päivitetty)
- Käynnistyksessä avataan aina **Kello**-ruutu.
- Erillistä valikkolistaa ei piirretä.
- LEFT/RIGHT siirtää seuraavaan/edelliseen ruutuun:
  1. Kello
  2. Kerroin
  3. Teema
  4. Päänäkymä
  5. Debug
- Päänäkymässä näkyy:
  - pieni kello
  - mahdollisimman suuri pistenäyttö (placeholder)
  - mahdollisimman suuri trip
  - pieni pulssirivi alhaalla
- km/h näkyy vain Debug-ruudussa.

## Tallennuslogiikka (NVS)
- Tallennetaan NVS:ään:
  - kerroin
  - teema
- **Kelloa ei tallenneta NVS:ään**

## Teemat
- Oletus: punainen
- Vaihtoehdot: punainen / vihreä / sininen

## Arkkitehtuuri

```text
src/
  app/      Sovellusohjaus, navigointi, loop
  hal/      Laitetason ajurit (Display, Buttons, SpeedInput, Storage)
  domain/   Tietomallit (asetukset, aika, trip, diagnostiikka, piste-placeholder)
  ui/       Layout-tokenit + renderöinti
include/
  BoardConfig.h   GPIO-määrittelyt
```

## GPIO-kartoitus
Määritykset löytyvät tiedostosta `include/BoardConfig.h`.

- Trip reset: GPIO14 (aktiivinen LOW)
- Nopeuspulssi: GPIO16 (oletus)
- Nuolinäppäimet (UP/DOWN/LEFT/RIGHT): GPIO2/GPIO3/GPIO1/GPIO10

## Layoutin säätö
- `src/ui/Layout.cpp`: rectit (`rectClock`, `rectPoints`, `rectTrip`, `rectStatus`)
- `src/ui/Layout.h`: fonttikoot

## Build ja flash
```bash
pio run -e lilygo-t-display-s3
pio run -e lilygo-t-display-s3 -t upload
pio device monitor -b 115200
```

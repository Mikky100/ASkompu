# AS-kompu v0.0.1 (firmware skeleton)

AS-kompu v0.0.1 on rakennettava PlatformIO-pohja LilyGo T-Display S3 -laitteelle.
Tämä versio sisältää toimivan valikkonavigoinnin nuolinäppäimillä, perusasetukset
(kello, matkakerroin, teema), keskitetyn UI-layoutin sekä debug-näkymän,
jonne km/h on rajattu.

## Tavoite v0.0.1
- Käynnistyy päänäkymään ("Näyttö")
- Valikko: **Näyttö / Asetukset / Debug**
- Asetukset tallentuvat NVS:ään (Preferences)
- Trip-reset toimii GPIO14:llä
- km/h näkyy vain Debug-näytössä

## Arkkitehtuuri

```text
src/
  app/      Sovellusohjaus, navigointi, loop
  hal/      Laitetason ajurit (Display, Buttons, SpeedInput, Storage)
  domain/   Tietomallit (asetukset, aika, trip, diagnostiikka, valikkotila)
  ui/       Layout-tokenit + renderöinti
include/
  BoardConfig.h   GPIO-määrittelyt
```

## GPIO-kartoitus
Määritykset löytyvät tiedostosta `include/BoardConfig.h`.

- Trip reset: GPIO14 (aktiivinen LOW)
- Nopeuspulssi: GPIO21 (oletus)
- Nuolinäppäimet (UP/DOWN/LEFT/RIGHT): **väliaikaiset placeholderit**

> Huom: ESP32testCount-repon GPIO-mappia ei voitu hakea tässä ympäristössä
> (verkko-eston vuoksi), joten nuolinäppäinten arvot on merkattu TODO:lla.
> Korvaa ne suoraan prototyypin arvoilla, kun yhteys on käytettävissä.

## Valikon käyttö
- **LEFT**: takaisin / avaa valikon päänäytöstä
- **UP/DOWN**: siirry valinnassa tai muuta arvoa
- **RIGHT**: avaa / vahvista

Asetukset:
1. **Kello**: HH:MM (24 h), tunnit ja minuutit säädettävissä
2. **Kerroin**: oletus 1000 (pulssia per metri)
3. **Teema**: 3 yötä varten sopivaa tummaa teemaa

### Kerroin-selitys
Kerroin tulkitaan tässä muodossa: `pulssia per metri`.
- suurempi luku => sama matka vaatii enemmän pulsseja => laskettu matka kasvaa hitaammin
- pienempi luku => sama pulssimäärä vastaa pidempää matkaa

## Layoutin säätö eri näytölle
Muokkaa tiedostoa `src/ui/Layout.cpp`:
- `rectClock`, `rectPoints`, `rectTrip`, `rectStatus`, `rectMenu`
- fonttikoot `src/ui/Layout.h` (`fontHuge`, `fontLarge`, `fontSmall`)

Renderöinti käyttää vain näitä nimettyjä alueita, joten elementtien siirto/koon muutos
onnistuu ilman hajautettuja "magic number" -arvoja.

## Build ja flash
```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

## Debug-näkymä
Debug-näyttö näyttää:
- km/h (vain täällä)
- kokonaispulssit
- 200 ms ikkunan pulssit
- kerroin
- loopin taajuus (Hz)

# AS-kompu v0.0.1 (firmware skeleton)

AS-kompu v0.0.1 on PlatformIO-pohja LilyGo T-Display S3 -laitteelle.
Tässä versiossa on toimiva valikkonavigointi nuolinäppäimillä,
perusasetukset (kello, matkakerroin, teema), keskitetty UI-layout,
sekä debug-näkymä johon km/h on rajattu.

## Tavoite v0.0.1
- Käynnistyy päänäkymään ("Näyttö")
- Valikko: **Näyttö / Asetukset / Debug**
- Asetukset tallentuvat NVS:ään (Preferences)
- Trip-reset toimii GPIO14:llä
- km/h näkyy vain Debug-näytössä

## Päivitys käyttäjäpalautteen perusteella
- Näytön välkkymistä vähennetty:
  - renderöintiä ei tehdä enää jokaisessa loopissa,
  - päänäkymä päivittyy jaksollisesti (~250 ms) ja valikot tapahtumaperusteisesti,
  - päänäkymässä tyhjennetään vain osiot (rectit), ei koko ruutua.
- Ensikäynnistyksellä kellon asetus avataan automaattisesti,
  jos HH:MM-arvoa ei ole vielä tallennettu NVS:ään.
- Oletustekstiväri on punainen.
- Teemavaihtoehdot: **Punainen / Vihreä / Sininen**.

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
> verkko-/policy-rajoitusten vuoksi. Korvaa nuolinäppäinten arvot suoraan
> prototyypin arvoilla, kun yhteys on käytettävissä.

## Valikon käyttö
- **LEFT**: takaisin / avaa valikon päänäytöstä
- **UP/DOWN**: siirry valinnassa tai muuta arvoa
- **RIGHT**: avaa / vahvista

Asetukset:
1. **Kello**: HH:MM (24 h), tunnit ja minuutit säädettävissä
2. **Kerroin**: oletus 1000 (pulssia per metri)
3. **Teema**: Punainen / Vihreä / Sininen

### Kerroin-selitys
Kerroin tulkitaan muodossa: `pulssia per metri`.
- suurempi luku => sama matka vaatii enemmän pulsseja => laskettu matka kasvaa hitaammin
- pienempi luku => sama pulssimäärä vastaa pidempää matkaa

## Layoutin säätö eri näytölle
Muokkaa tiedostoja:
- `src/ui/Layout.cpp`: `rectClock`, `rectPoints`, `rectTrip`, `rectStatus`, `rectMenu`
- `src/ui/Layout.h`: fonttikoot `fontHuge`, `fontLarge`, `fontSmall`

Renderöinti käyttää näitä nimettyjä alueita, joten elementtien paikka/koon muutos
onnistuu ilman hajautettuja "magic number" -arvoja.

## Build ja flash
```bash
pio run -e lilygo-t-display-s3
pio run -e lilygo-t-display-s3 -t upload
pio device monitor -b 115200
```

## Debug-näkymä
Debug-näyttö näyttää:
- km/h (vain täällä)
- kokonaispulssit
- 200 ms ikkunan pulssit
- kerroin
- loopin taajuus (Hz)

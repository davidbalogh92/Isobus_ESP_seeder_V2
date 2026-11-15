# Vetőgép V2 – fejlesztési szabályok

* A kommunikáció magyarul megy (kód-megjegyzések, dokumentáció, commit üzenetek is).
* Hardvert nem égetünk / nem flashelünk: a felhasználó tölti fel (USB vagy OTA) és teszteli.
* A VT-felület maradjon VT2- és VT3-kompatibilis.

## Két panel, párhuzamosan

Minden fejlesztés **mindkét panelre** készül, és mindkettőre fordul (CI: `waveshare_8di8ro`, `rcv17_16mb`):

| | Waveshare ESP32-S3-ETH-8DI-8RO-C | ESP32S3 RCv1.7 (Rate Controller) |
|---|---|---|
| Fordítási környezet | `waveshare_8di8ro` | `rcv17_16mb` (`SEEDER_BOARD_RCV17`, PSRAM nélkül) |
| Kiadás | `VETOGEP_WAVESHARE_16MB_…` | `VETOGEP_RCV17_16MB_…` |
| Leírás | README_HU.md | docs/HARDVER_RCV17_HU.md |

A panelfüggő rész csak a `src/esp/board.hpp` (lábak, képességek) és az `src/esp/io.cpp` (meghajtók);
a mag (`src/core`, `include/seeder`) és az alkalmazásréteg (`src/app`) közös.

**Ha egy új funkció a két panelen nem egyformán valósítható meg, megvalósítás előtt meg kell beszélni
a felhasználóval** (nem találgatunk, nem döntünk helyette). Ismert eltérések, amelyekre figyelni kell:

* RCv1.7-en nincs: hangjelző, szabad relé helyett H-híd kimenet. HX711 mérleg (GPIO11/12) és DS18B20
  (GPIO10) a H26 nyers 3,3 V-os lábain van.
* RCv1.7-en minden szabad láb foglalt (GPIO1 kerék, 2 turbina, 9 leforgató gomb, 10 hőmérő, 11/12 mérleg;
  a 3 és 46 strapping) – új bemenethez / kimenethez előbb meg kell beszélni, mi kerüljön ki vagy hová.
* Motorvezérlő: Waveshare – külső vezérlő (PWM / PWM+DIR / PWM+PWM); RCv1.7 – saját TB67H451 (PWM+PWM)
  vagy külső Cytron a H2-n (PWM+DIR).
* CAN-lábak: Waveshare 17/18, RCv1.7 17/16. Kimenetek: Waveshare TCA9554 relék, RCv1.7 PCA9685 H-hidak.
* Új bemenet / kimenet felvételekor mindkét panelen ki kell jelölni a helyét (vagy megbeszélni, hogy az
  egyiken nincs), és frissíteni mindkét PINMAP-et és leírást.

## Ellenőrzés

* `python tools/gen_params.py` → `tools/pool/generate_pool.py --preview` (paraméter / VT változásnál)
* `build-host`: `cmake ../host && make -j8 && ctest` (test_core, test_app, iop_check, isobus_sim)
* `python -m unittest discover -s tests/python`
* VT-pool változásnál a Windows-program mintái: `build-host/web_demo 8099` + `tools/capture_vt_fixtures.py`
* Push után a CI-nek mindkét panelre zöldnek kell lennie.

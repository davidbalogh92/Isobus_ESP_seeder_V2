# Vetőgép V2 az ESP32S3 RCv1.7 panelen (második, kisebb tudású panel)

Az AgOpenGPS Rate Controller panel (Vági F. / Gunics B., ESP32-S3-WROOM-1U, 16 MB flash, **PSRAM nincs**)
ugyanazt a vetőgép-firmware-t futtatja, mint a Waveshare panel – a VT, a web, a Windows-program, az
AgOpenGPS, a Keya CAN-motor és a **külső kijelző** (CAN, Proprietary A) ugyanúgy működik.
A panel adott lábkészlete miatt kevesebbet tud:

| Van | Nincs |
|---|---|
| 2 adagolómotor enkóderrel (a panel TB67H451 H-hídjain, vagy külső Cytronnal), vagy Keya CAN-motor | HX711 mérleg (leforgatási mérlegelés kézzel) |
| 2 tartályszenzor, munkakapcsoló | DS18B20 hőmérők (olajhűtés) |
| kerékjeladó, turbina-jeladó (szabad lábakon, illesztéssel), leforgató gomb | hangjelző |
| 8 kimenet a PCA9685-ön (kuplungok, terelőszelep, 2 munkalámpa, tartalék) | |
| nyomásmérés (ADS1115 a panelen), Ethernet (W5500) | |

Kiadási fájlok: `VETOGEP_RCV17_16MB_vXXX_FULL_0x0.bin` (USB) és `VETOGEP_RCV17_16MB_vXXX_OTA.bin` (OTA).
**A Waveshare fájlját ne töltsd erre a panelre, és fordítva** – más lábak, más CAN-láb.

## Első feltöltés új, nyers panelre (eFuse 3,3 V)

A GPIO45 strapping láb a W5500 INT-re kötött: az INT induláskor magas, ezért a nyers panel a 3,3 V-os
flash-t 1,8 V-ról próbálja járatni – nem indul, és a böngészős feltöltő sem tud írni rá. A flash
feszültségét egyszer, véglegesen eFuse-zal 3,3 V-ra kell égetni. Ehhez kész Windows-szkript van:

* a GitHub kiadásban: **`RCV17_ELSO_FELTOLTES.zip`** (forrás: `tools/rcv17_elso_feltoltes/`);
* kicsomagolás után tedd mellé a `…RCV17_16MB_vXXX_FULL_0x0.bin` fájlt, és indítsd az
  **`ELSO_FELTOLTES_RCV17.bat`**-ot (dupla katt);
* a szkript letölti az Espressif esptool / espefuse programot, kiválasztod a COM portot, letöltő módba
  teszed a panelt (BOOT nyomva → RESET → BOOT elenged), kiolvassa az eFuse-t, és ha kell, kétszeri
  megerősítés után beégeti (`set_flash_voltage 3.3V`); táp ki / be után feltölti a FULL firmware-t 0x0-ra;
* ha az eFuse már be van égetve (pl. a másik firmware-rel), csak feltölt. A Waveshare-fájlt nem engedi.

Kézzel: `python -m espefuse --chip esp32s3 --port COMx set_flash_voltage 3.3V`, táp ki / be, majd a
FULL fájl 0x0-ra (esptool vagy https://espressif.github.io/esptool-js/). Részletek: `tools/rcv17_elso_feltoltes/OLVASS_EL.txt`.

## Lábkiosztás

Forrás: a panel kapcsolási rajza (Rate_control_ESPRC_01_V1.4, 6. lap) és az RC_ESP32 firmware
(ESP32RC_refactor26_june). A két szabályzó kimenet a panelen **TB67H451 H-híd**: IN1 („Dir”) és IN2 („PWM”)
mindkettő PWM-jel, az egyik irányhoz az IN1-et, a másikhoz az IN2-t kell hajtani.

| Jel | Láb | Megjegyzés |
|---|---|---|
| ISOBUS CAN TX / RX | GPIO17 / GPIO16 | MCP2562; **P3 sorkapocs: 1 = CAN_L, 2 = CAN_H**; 120 Ω lezárás: H3 jumper – meglévő ISOBUS kábelen **nyitva** hagyd. A Keya motorok és a külső kijelző is ezen a buszon |
| 1. adagoló enkóder | GPIO21 (FLOW1) | optocsatolós bemenet |
| 2. adagoló enkóder | GPIO47 (FLOW2) | optocsatolós bemenet |
| 1. motor | **MOT9** sorkapocs – IN2 GPIO5 (előre), IN1 GPIO4 (hátra) | TB67H451 (max. 3,5 A) |
| 2. motor | **ZONE1** sorkapocs – IN2 GPIO15 (előre), IN1 GPIO7 (hátra) | TB67H451 (max. 3,5 A) |
| Külső Cytron | H2 tüskesor: ugyanezek a jelek (IN1 = DIR, IN2 = PWM) | nagyobb motorhoz |
| Cytron-engedélyező | GPIO13 | csak régebbi paneleken; elektromos hajtásnál magas |
| Munkakapcsoló | GPIO40 (WORK1) | optocsatolós; NO / NC beállítás |
| 1. / 2. tartály | **GPIO42 (BIN1) / GPIO41 (BIN2)** | optocsatolós; NO / NC beállítás |
| Kerékjeladó | GPIO1 (H4) | **3,3 V-os láb – optocsatoló vagy feszültségosztó kell** |
| Turbina-jeladó | GPIO2 (H4) | **3,3 V-os láb – optocsatoló vagy feszültségosztó kell** |
| Leforgató nyomógomb | GPIO9 (H26) | GND-re záró gomb; bekapcsoláskor 3 s nyomva: WiFi szerviz mód |
| Kimenet 1 / 2 | PCA9685, sorkapocs 1A–1B / 2A–2B | 1. / 2. adagoló kuplung |
| Kimenet 3 | 3A–3B | terelőszelep (nyitás / nyomjelző) |
| Kimenet 4 / 5 | 4A–4B / 5A–5B | 1. / 2. munkalámpa (Beállítások → Kiegészítők: a kimenet állítható) |
| Kimenet 6–8 | 6A–8B | tartalék |
| Nyomás | ADS1115 AIN0 (ADC 5V / 4–20 mA sorkapocs) | 0–5 V |
| Ethernet W5500 | MISO37 MOSI35 SCLK36 CS38 INT45 RST48 | AgOpenGPS, web, Windows-program |

A tartályok a rajz szerint vannak bekötve (BIN1 = GPIO42). Ha a Rate Controller programban fordítva
állítottad be (Sensor 1 → 41), a vetőgépen az 1. tartály a BIN1 sorkapocs.

A PCA9685 kimenetek is TB67H451 H-hidak (k. kimenet: csatorna 2k−2 és 2k−1, az eredeti firmware kiosztása).
Bekapcsolt kimenetnél **A = +12 V, B = 0 V**, kikapcsoltnál mindkét kapocs szabad (nagy impedancia):
a kuplungot / lámpát az **A és B kapocs közé** kösd. A beállításokban a „Relé n” megnevezés ezen a panelen a
„Kimenet n”-t jelenti (a számozás a Waveshare reléivel azonos).

Szabad lábak: GPIO1, 2 (H4) és 9–12 (H26). A GPIO3 és 46 is a H26-on van, de strapping lábak – nem használjuk.
A szabad lábak közvetlenül az ESP32-re mennek: 12 V-os jeladót **csak illesztéssel** (pl. PC817
optocsatoló, vagy osztó + Zener) szabad rákötni. A jel aktív alacsony (GND-re húz), belső felhúzással.

A panelből 8 MB-os és 16 MB-os flash-ű is készült; ez a firmware a **16 MB-osra** (WROOM-1U-N16) való.
PSRAM-os (R8) modullal a panel nem működik (a GPIO35–37 az Etherneté).

## Beállítás

* Beállítások → Hajtás, motor → Hajtás: **Elektr. PWM**, Motorvezérlő:
  * a panel saját MOT9 / ZONE1 kimenetén (TB67H451): **PWM + PWM**,
  * a H2-re kötött külső Cytronnal: **PWM + DIR**,
  * „Csak PWM” (visszafordítás nélkül) mindkettővel működik: az IN1 / DIR alacsony kimenet marad.
* A Rate Controller programban bepipált **Invert** a forgásirányt fordítja: itt Motorszabályzás →
  **Forgásirány megfordítása**.
* Keya CAN-motorral: Hajtás = **Elektr. Keya**; a motorok a 17/16-os CAN-en, a többi ugyanaz, mint a Waveshare-en.
* Olajhűtés, mérleg ezen a panelen nem használható (nincs hőmérő- és mérlegbemenet) – maradjanak kikapcsolva.
* A külső kijelzőn a leforgatás itt is teljes (kézi és motoros), csak a mérleg gombjai (átvétel, tára) nem jelennek
  meg: a vezérlő a kijelzőnek nem jelent mérleget.

# Paraméterek

_Generált fájl (tools/gen_params.py). Minden paraméter a webes felületen (Beállítások) és – ha van VT oldal megadva – a terminálon is állítható._

## Gép

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `width_m` | Munkaszélesség | 6.0 m | 1 – 20 m | GÉP |  |
| `section_count` | Szakaszok száma | 2 db | 1 – 2 db | GÉP | 2 szakasz: az 1. adagoló a bal, a 2. a jobb félszélességet veti. 1 szakasz: a Task Controller a teljes szélességet egyben kapcsolja (pl. első/hátsó soros gépnél mindkét adagolót együtt), a két adagoló gombja a kezelőnek továbbra is külön működik. |
| `meter_count` | Adagolók száma | 2 db | 1 – 2 db | GÉP |  |
| `tank_count` | Tartályok (rekeszek) száma | 2 db | 1 – 2 db | GÉP |  |
| `meter_layout` | Két adagoló elrendezése | Bal / jobb félszélesség | Bal / jobb félszélesség / Első / hátsó sor (teljes szélesség) | GÉP | Első/hátsó soros gépnél mindkét adagoló a teljes szélességet veti: a terület mindig a teljes szélességgel számolódik, és csak 1 szakasz lehet. |
| `sec_w1` | 1. (bal) szakasz szélessége | 3.0 m | 0.1 – 20 m | GÉP | 2 szakasznál. A 2. szakasz = munkaszélesség − 1. szakasz. |

## Adagolás és feltöltés

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `rate1` | 1. adagoló előírt mennyiség | 200.0 kg/ha | 0.5 – 1000 kg/ha | ADAGOLÁS, FELTÖLTÉS |  |
| `rate2` | 2. adagoló előírt mennyiség | 200.0 kg/ha | 0.5 – 1000 kg/ha | ADAGOLÁS, FELTÖLTÉS |  |
| `cal1_kg_rev` | 1. adagoló kg / adagolófordulat (leforgatásból) | 0.2353 kg | 0.0001 – 20 kg | ADAGOLÁS, FELTÖLTÉS | A leforgatás eredménye. Elektromos hajtásnál ebből számolja a szükséges fordulatot, talajhajtásnál a kijuttatott mennyiséget. |
| `cal2_kg_rev` | 2. adagoló kg / adagolófordulat (leforgatásból) | 0.2353 kg | 0.0001 – 20 kg | ADAGOLÁS, FELTÖLTÉS |  |
| `rate_step_pct` | Léptetés (+/− gomb) | 5.0 % | 1 – 50 % | ADAGOLÁS, FELTÖLTÉS |  |
| `rate_tol_pct` | Eltérés riasztás (elektromos hajtás) | 10.0 % | 2 – 50 % | ADAGOLÁS, FELTÖLTÉS |  |
| `prefill_auto` | Automatikus feltöltés induláskor (sarkokban) | Igen | Nem / Igen | ADAGOLÁS, FELTÖLTÉS | Csak elektromos hajtásnál. Ha álló gépnél leengeded a gépet (munkakapcsoló) vagy elindítod a vetést, az adagolók a 'feltöltési sebességhez' tartozó fordulattal előre feltöltik a csöveket, majd az indulás után még az utánfutási ideig így forognak. |
| `prefill_speed_kmh` | Feltöltési (elméleti) sebesség | 8.0 km/h | 1 – 20 km/h | ADAGOLÁS, FELTÖLTÉS |  |
| `prefill_time_s` | Feltöltési idő (álló gép) | 3.0 s | 0 – 30 s | ADAGOLÁS, FELTÖLTÉS |  |
| `prefill_after_s` | Utánfutás indulás után | 4.0 s | 0 – 20 s | ADAGOLÁS, FELTÖLTÉS |  |

## Adagolóhajtás

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `drive_type` | Adagolók hajtása | Talajhajtás + elektromágneses kuplung | Talajhajtás + elektromágneses kuplung / Elektromos motor (PWM) | ADAGOLÓHAJTÁS |  |
| `clutch_mode` | Kuplung típusa | Áram alatt hajt | Áram alatt hajt / Áram alatt old | ADAGOLÓHAJTÁS |  |
| `meter_ppr` | Adagolótengely-jeladó impulzus / fordulat (talajhajtás) | 1 imp | 1 – 100 imp | ADAGOLÓHAJTÁS |  |
| `meter_timeout_s` | Adagoló nem forog riasztás késleltetése | 5.0 s | 1 – 60 s | ADAGOLÓHAJTÁS | Ha a vetés feltételei teljesülnek (vetés BE, gép lent, halad, adagoló bekapcsolva), és ennyi ideig nem jön impulzus az adagolótól, riasztás. |
| `encoder_ppr` | Enkóder impulzus / adagolófordulat (motor) | 50 imp | 1 – 5000 imp | ADAGOLÓHAJTÁS |  |
| `motor_max_rpm` | Adagoló fordulat 100% PWM-nél | 60.0 1/min | 1 – 600 1/min | ADAGOLÓHAJTÁS | Az előrecsatoláshoz; a szabályzó ebből indul, utána az enkóder alapján korrigál. |
| `pwm_min_pct` | PWM minimum (forgás közben) | 8.0 % | 0 – 100 % | ADAGOLÓHAJTÁS |  |
| `pwm_max_pct` | PWM maximum | 100.0 % | 1 – 100 % | ADAGOLÓHAJTÁS |  |
| `ramp_up_s` | Felfutás (0 → 100%) | 1.0 s | 0 – 10 s | ADAGOLÓHAJTÁS |  |
| `ramp_down_s` | Lefutás (100% → 0) | 0.5 s | 0 – 10 s | ADAGOLÓHAJTÁS |  |
| `reg_gain` | Szabályzási állandó (1 lassú – 10 gyors) | 5 | 1 – 10 | ADAGOLÓHAJTÁS |  |
| `pwm_freq_hz` | PWM frekvencia | 1000 Hz | 100 – 20000 Hz | – |  |

## Turbina

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `fan_ppr` | Turbina impulzus / fordulat | 1 imp | 1 – 100 imp | TURBINA |  |
| `fan_min_rpm` | Riasztás e fordulat alatt | 2500 1/min | 0 – 20000 1/min | TURBINA | Csak akkor riaszt, ha a gép dolgozik: munkakapcsolóval lent van és halad; munkakapcsoló nélkül ha halad és a vetés BE. |
| `fan_max_rpm` | Riasztás e fordulat felett (0 = nincs) | 0 1/min | 0 – 20000 1/min | TURBINA |  |
| `fan_delay_s` | Riasztás késleltetése | 5.0 s | 0 – 60 s | TURBINA |  |

## Sebesség

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `speed_src` | Sebesség forrása | Kerékjeladó | Kerékjeladó / ISOBUS (GPS/radar/traktor) / ISOBUS, kiesésnél kerék / Szimulált | SEBESSÉG |  |
| `isobus_speed_pref` | ISOBUS sebességüzenet | Automatikus | Automatikus / Földsebesség (radar/GPS) / Traktor kerék / Gép által választott / NMEA GPS (SOG) | SEBESSÉG | Automatikus sorrend: gép által választott > föld (radar/GPS) > NMEA GPS > traktor kerék. Ha a terminál megosztja a GPS sebességét, az 'NMEA GPS' vagy 'Földsebesség' jön. |
| `wheel_ppm` | Kerékjeladó impulzus / 100 m | 100.0 imp | 1 – 100000 imp | SEBESSÉG |  |
| `sim_speed` | Szimulált sebesség | 8.0 km/h | 0 – 30 km/h | SEBESSÉG |  |
| `min_work_speed` | Haladás küszöb (ez alatt áll a gép) | 1.0 km/h | 0.1 – 10 km/h | SEBESSÉG |  |
| `speed_timeout_ms` | Sebességjel időtúllépés (ISOBUS) | 1500 ms | 200 – 10000 ms | – |  |

## Kapcsolók

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `work_sw` | Munkakapcsoló (gép lent) | Van – NO (lent zár) | Nincs / Van – NO (lent zár) / Van – NC (lent bont) | KAPCSOLÓK | Ha van, a 'gép lent' állapot a vetés (adagolók, terület) feltétele. |
| `work_sw_tc` | Munkakapcsoló az automata szakaszolásnál is feltétel | Igen | Nem / Igen | KAPCSOLÓK | Igen: TC-SEC automata szakaszolásnál is csak lent vet (a TC-nek kiemelt gépnél 'nem dolgozik' jelentést küldünk). Nem: automatában csak a TC dönt. |
| `hopper1_nc` | 1. tartály leürülésgátló kapcsoló | NO (záró) | NO (záró) / NC (bontó) | KAPCSOLÓK | NO: üres tartálynál zár. NC: üres tartálynál bont. |
| `hopper2_nc` | 2. tartály leürülésgátló kapcsoló | NO (záró) | NO (záró) / NC (bontó) | KAPCSOLÓK |  |
| `hopper_delay_s` | Leürülés jelzés szűrése | 2.0 s | 0 – 30 s | KAPCSOLÓK |  |
| `out_invert` | Kimenetek fordított szintje (relémodul) | Nem | Nem / Igen | KAPCSOLÓK | Igen: alacsony szint kapcsolja be a relét (kuplung, lámpa, szelep, hűtő). |

## Kiegészítők

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `lights_enable` | Munkalámpák (2 db) | Nem | Nem / Igen | KIEGÉSZÍTÕK |  |
| `valve_enable` | Nyitás / nyomjelző váltószelep | Nem | Nem / Igen | KIEGÉSZÍTÕK | Alapban a hidraulikakör a gépet nyitja/csukja; a szelep bekapcsolva a nyomjelzőre tereli. |
| `pressure_enable` | Nyomásmérés (lenyomatás, ADS1115) | Nem | Nem / Igen | KIEGÉSZÍTÕK |  |
| `temp_enable` | Olajhőmérséklet + hűtőventilátor (2× DS18B20) | Nem | Nem / Igen | KIEGÉSZÍTÕK |  |
| `scale_enable` | Leforgatási mérleg (HX711) | Nem | Nem / Igen | KIEGÉSZÍTÕK |  |

## Nyomásmérés (lenyomatás)

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `pressure_range_bar` | Szenzor méréshatár | 250.0 bar | 1 – 1000 bar | NYOMÁSMÉRÉS |  |
| `pressure_signal` | Szenzor kimenőjele | 0,5–4,5 V | 0–5 V / 0,5–4,5 V / 0,5–5 V / 0–4,5 V | NYOMÁSMÉRÉS |  |
| `pressure_cal` | Kalibrációs szorzó | 1.0 | 0.5 – 2 | NYOMÁSMÉRÉS | Az 'Ismert nyomás' kalibrálás állítja be (1,000 = gyári jelleggörbe). |
| `pressure_zero_bar` | Nullpont-eltolás | 0.0 bar | -50 – 50 bar | NYOMÁSMÉRÉS |  |
| `pressure_ch` | ADS1115 csatorna | 0 | 0 – 3 | – |  |
| `pressure_divider` | Bemeneti feszültségosztó (V szenzor / V ADS) | 1.0 | 1 – 10 | – | Ha az ADS1115 bemenete előtt osztó van (pl. 3,3 V-os táp esetén), add meg az arányát. |

## Olajhőmérséklet és hűtés

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `cooler_on_c` | Hűtőventilátor bekapcsol | 60.0 °C | 20 – 120 °C | OLAJHÕMÉRSÉKLET |  |
| `cooler_off_c` | Hűtőventilátor kikapcsol | 50.0 °C | 10 – 115 °C | OLAJHÕMÉRSÉKLET |  |
| `cooler_src` | Szabályozó hőmérő | Olajtartály | Olajtartály / Hűtő kimenet / A melegebb | OLAJHÕMÉRSÉKLET |  |
| `temp_alarm_c` | Túlmelegedés riasztás (0 = nincs) | 85.0 °C | 0 – 130 °C | OLAJHÕMÉRSÉKLET |  |
| `temp_swap` | A két DS18B20 hőmérő felcserélése | Nem | Nem / Igen | – | A hőmérőket a firmware a ROM-azonosítójuk sorrendjében számozza. Ha az olajtartály és a hűtő kimenet értéke fordítva jelenik meg, kapcsold be. |

## Mérleg

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `scale_capacity_kg` | Mérlegcella méréshatára | 50.0 kg | 1 – 500 kg | MÉRLEG |  |
| `scale_factor` | Mérleg kalibrációs tényező (beütés / kg) | 85000.0 1/kg | 1 – 1e+07 1/kg | MÉRLEG | A 'Kalibrálás ismert súllyal' állítja be. |
| `scale_offset` | Mérleg nullpont (tára, nyers) | 0.0 | -2e+07 – 2e+07 | – |  |

## Task Controller és geometria

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `tc_enable` | Task Controller kliens | Igen | Nem / Igen | TASK CONTROLLER |  |
| `tc_sec` | Automatikus szakaszvezérlés (TC-SEC) | Igen | Nem / Igen | TASK CONTROLLER |  |
| `tc_geo` | Kijuttatási térkép (TC-GEO, kg/ha az adagolóknak) | Nem | Nem / Igen | TASK CONTROLLER |  |
| `hitch_type` | Csatlakozás | Vonórúd (vonószem) | Függesztett / Kormányzott vonórúd / Vonórúd (vonószem) / Vonóhorog / Gömbfej (K80) | GEOMETRIA |  |
| `geo_a_m` | A: vonópont → csuklópont (kormányzott vonórúd) | 0.0 m | 0 – 20 m | GEOMETRIA |  |
| `geo_b_m` | B: csuklópont → csoroszlyák (függesztett: vonópont → csoroszlyák) | 4.0 m | 0 – 20 m | GEOMETRIA |  |
| `geo_c_m` | C: csuklópont → kerékagy (fordulópont) | 3.0 m | 0 – 20 m | GEOMETRIA |  |
| `geo_y_m` | D: oldalirányú eltolás (+ jobbra) | 0.0 m | -5 – 5 m | GEOMETRIA |  |
| `delay_on_ms` | Tehetetlenség BE | 800 ms | 0 – 5000 ms | TASK CONTROLLER |  |
| `delay_off_ms` | Tehetetlenség KI | 400 ms | 0 – 5000 ms | TASK CONTROLLER |  |

## Szimuláció / teszt

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `sim_inputs` | Bemenetek szimulációja (turbina, adagolók, munkakapcsoló) | Nem | Nem / Igen | SZIMULÁCIÓ / TESZT | Próbapadi teszthez: a turbina, az adagolók és az enkóderek impulzusait a firmware állítja elő, a munkakapcsoló 'lent' állapotú. |
| `sim_fan_rpm` | Szimulált turbinafordulat | 3500 1/min | 0 – 10000 1/min | SZIMULÁCIÓ / TESZT |  |
| `sim_wheel` | Kerékimpulzus szimuláció a szimulált sebességből | Nem | Nem / Igen | SZIMULÁCIÓ / TESZT |  |

## Rendszer

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `wifi_pass` | Wi-Fi jelszó (8–32 karakter) | `12345678` | 8–32 karakter | – |  |
| `ecu_instance` | ECU példány (több gép egy buszon) | 0 | 0 – 7 | – |  |
| `realarm_s` | Nyugtázott, fennálló riasztás újra (0 = soha) | 0 s | 0 – 3600 s | – |  |

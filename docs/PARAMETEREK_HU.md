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
| `crank_autostop` | Leforgatás megáll a célfordulatnál | Igen | Nem / Igen | ADAGOLÁS, FELTÖLTÉS | Elektromos hajtásnál a motor a célfordulatnál (85 / 42,5 / 17) magától megáll, akkor is, ha a gombot még nyomod. 'Nem': amíg nyomod, forog. |
| `prefill_auto` | Automatikus feltöltés induláskor (sarkokban) | Igen | Nem / Igen | ADAGOLÁS, FELTÖLTÉS | Csak elektromos hajtásnál. Ha álló gépnél leengeded a gépet (munkakapcsoló) vagy elindítod a vetést, az adagolók a 'feltöltési sebességhez' tartozó fordulattal előre feltöltik a csöveket, majd az indulás után még az utánfutási ideig így forognak. |
| `prefill_speed_kmh` | Feltöltési (elméleti) sebesség | 8.0 km/h | 1 – 20 km/h | ADAGOLÁS, FELTÖLTÉS |  |
| `prefill_time_s` | Feltöltési idő (álló gép) | 3.0 s | 0 – 30 s | ADAGOLÁS, FELTÖLTÉS |  |
| `prefill_after_s` | Utánfutás indulás után | 4.0 s | 0 – 20 s | ADAGOLÁS, FELTÖLTÉS |  |

## Adagolóhajtás

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `drive_type` | Adagolók hajtása | Talajhajtás + elektromágneses kuplung | Talajhajtás + elektromágneses kuplung / Elektromos motor (PWM) | ADAGOLÓHAJTÁS |  |
| `clutch_mode` | Kuplung típusa | Áram alatt hajt | Áram alatt hajt / Áram alatt old | ADAGOLÓHAJTÁS |  |
| `clutch_lift` | Kuplung kiemeléskor / megálláskor (talajhajtás) | Nem old (csak szakasz / gomb) | Old (kiemeléskor lekapcsol) / Nem old (csak szakasz / gomb) | ADAGOLÓHAJTÁS | Talajhajtásnál kiemelt vagy álló gépnél az adagoló úgysem forog, ezért a kuplungot alapból csak a szakaszolás és az adagológombok kapcsolják. 'Old': a régi működés, kiemeléskor is lekapcsol. |
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
| `speed_src` | Sebesség forrása | Kerékjeladó | Kerékjeladó / ISOBUS (GPS/radar/traktor) / ISOBUS, kiesésnél kerék / Szimulált / AgOpenGPS (GPS) / AgOpenGPS, kiesésnél kerék | SEBESSÉG | AgOpenGPS: a sebességet az AgIO-tól kapja Ethernet/UDP-n (PGN 254), ehhez az AgOpenGPS kapcsolatot be kell kapcsolni. |
| `isobus_speed_pref` | ISOBUS sebességüzenet **[ISOBUS]** | Automatikus | Automatikus / Földsebesség (radar/GPS) / Traktor kerék / Gép által választott / NMEA GPS (SOG) | SEBESSÉG | Automatikus sorrend: gép által választott > föld (radar/GPS) > NMEA GPS > traktor kerék. Ha a terminál megosztja a GPS sebességét, az 'NMEA GPS' vagy 'Földsebesség' jön. |
| `wheel_ppm` | Kerékjeladó impulzus / 100 m | 100.0 imp | 1 – 100000 imp | SEBESSÉG |  |
| `sim_speed` | Szimulált sebesség | 8.0 km/h | 0 – 30 km/h | SEBESSÉG |  |
| `min_work_speed` | Haladás küszöb (ez alatt áll a gép) | 1.0 km/h | 0.1 – 10 km/h | SEBESSÉG |  |
| `slip_alarm_pct` | Kerék – GPS sebességeltérés riasztás (0 = nincs) | 0.0 % | 0 – 50 % | SEBESSÉG | A kerékjeladó és a GPS (AgOpenGPS vagy ISOBUS) sebességének eltérése (csúszás). Csak haladás közben, mindkét jel megléte esetén figyel. |
| `slip_delay_s` | Eltérés riasztás késleltetése | 3.0 s | 0.5 – 30 s | SEBESSÉG |  |
| `speed_timeout_ms` | Sebességjel időtúllépés (ISOBUS) | 1500 ms | 200 – 10000 ms | – |  |

## Kapcsolók

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `work_sw` | Munkakapcsoló (gép lent) | Van – NO (lent zár) | Nincs / Van – NO (lent zár) / Van – NC (lent bont) | KAPCSOLÓK | Ha van, a 'gép lent' állapot a vetés (adagolók, terület) feltétele. |
| `work_sw_tc` | Munkakapcsoló az automata szakaszolásnál is feltétel **[ISOBUS]** | Igen | Nem / Igen | KAPCSOLÓK | Igen: TC-SEC automata szakaszolásnál is csak lent vet (a TC-nek kiemelt gépnél 'nem dolgozik' jelentést küldünk). Nem: automatában csak a TC dönt. |
| `hopper1_nc` | 1. tartály leürülésgátló kapcsoló | NO (záró) | NO (záró) / NC (bontó) | KAPCSOLÓK | NO: üres tartálynál zár. NC: üres tartálynál bont. |
| `hopper2_nc` | 2. tartály leürülésgátló kapcsoló | NO (záró) | NO (záró) / NC (bontó) | KAPCSOLÓK |  |
| `hopper_delay_s` | Leürülés jelzés szűrése | 2.0 s | 0 – 30 s | KAPCSOLÓK |  |
| `crank_btn` | Leforgató nyomógomb a gépen (IN8) | Nem | Nem / Igen | KAPCSOLÓK | Elektromos hajtásnál amíg nyomod, a kiválasztott adagoló forog (leforgatás). Talajhajtásnál 2 s nyomás nullázza a leforgatási számlálót. NO gomb, GND felé zár. |
| `out_invert` | Kimenetek fordított szintje (relémodul) | Nem | Nem / Igen | KAPCSOLÓK | Igen: alacsony szint kapcsolja be a relét (kuplung, lámpa, szelep, hűtő). |

## Kiegészítők

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `lights_enable` | Munkalámpák (2 db) | Nem | Nem / Igen | KIEGÉSZÍTÕK |  |
| `crank_lamp` | Munkalámpa villan leforgatásnál | Igen | Nem / Igen | KIEGÉSZÍTÕK | Minden megszámolt fordulatnál röviden (0,15 s) felvillan; a célfordulatnál 5 s-ig gyorsan villog, utána a mentésig folyamatosan világít (a gép mögött is látszik). Csak ha a munkalámpák engedélyezve vannak. |
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
| `rev_enable` | Hűtőventilátor visszafúvás (forgásirány-váltás, relé 7) | Nem | Nem / Igen | OLAJHÕMÉRSÉKLET | A hűtő kifúvása: a ventilátor leáll, a várakozás után az irányváltó relé (motor polaritás / hidraulika irányváltó) bekapcsol és a ventilátor visszafelé fúj, majd újra várakozás után vissza. |
| `rev_interval_min` | Visszafúvás gyakorisága (0 = csak kézi) | 20 perc | 0 – 240 perc | OLAJHÕMÉRSÉKLET |  |
| `rev_time_s` | Visszafúvás időtartama | 15.0 s | 2 – 120 s | OLAJHÕMÉRSÉKLET |  |
| `rev_pause_s` | Leállási idő irányváltás előtt / után | 5.0 s | 1 – 60 s | OLAJHÕMÉRSÉKLET | Ennyi ideig áll a ventilátor, mielőtt irányt vált (motor / hidraulika kímélése). |
| `temp_swap` | A két DS18B20 hőmérő felcserélése | Nem | Nem / Igen | – | A hőmérőket a firmware a ROM-azonosítójuk sorrendjében számozza. Ha az olajtartály és a hűtő kimenet értéke fordítva jelenik meg, kapcsold be. |

## Mérleg

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `scale_capacity_kg` | Mérlegcella méréshatára | 50.0 kg | 1 – 500 kg | MÉRLEG |  |
| `scale_factor` | Mérleg kalibrációs tényező (beütés / kg) | 85000.0 1/kg | 1 – 1e+07 1/kg | MÉRLEG | A 'Kalibrálás ismert súllyal' állítja be. |
| `scale_offset` | Mérleg nullpont (tára, nyers) | 0.0 | -2e+07 – 2e+07 | – |  |

## Szakaszolás, Task Controller és geometria

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `sec_src` | Automata szakaszolás forrása | ISOBUS Task Controller | Nincs (csak kézi) / ISOBUS Task Controller / AgOpenGPS | SZAKASZOLÁS, TC | Az 'AUTO' gomb ezt a forrást kapcsolja be. A kézi adagológombok mindig érvényesek: kikapcsolt adagolót az automata sem kapcsol be. |
| `tc_enable` | Task Controller kliens **[ISOBUS]** | Igen | Nem / Igen | SZAKASZOLÁS, TC |  |
| `tc_sec` | Automatikus szakaszvezérlés (TC-SEC) **[ISOBUS]** | Igen | Nem / Igen | SZAKASZOLÁS, TC |  |
| `tc_geo` | Kijuttatási térkép (TC-GEO, kg/ha az adagolóknak) **[ISOBUS]** | Nem | Nem / Igen | SZAKASZOLÁS, TC |  |
| `hitch_type` | Csatlakozás **[ISOBUS]** | Vonórúd (vonószem) | Függesztett / Kormányzott vonórúd / Vonórúd (vonószem) / Vonóhorog / Gömbfej (K80) | GEOMETRIA |  |
| `geo_a_m` | A: vonópont → csuklópont (kormányzott vonórúd) **[ISOBUS]** | 0.0 m | 0 – 20 m | GEOMETRIA |  |
| `geo_b_m` | B: csuklópont → csoroszlyák (függesztett: vonópont → csoroszlyák) **[ISOBUS]** | 4.0 m | 0 – 20 m | GEOMETRIA |  |
| `geo_c_m` | C: csuklópont → kerékagy (fordulópont) **[ISOBUS]** | 3.0 m | 0 – 20 m | GEOMETRIA |  |
| `geo_y_m` | D: oldalirányú eltolás (+ jobbra) **[ISOBUS]** | 0.0 m | -5 – 5 m | GEOMETRIA |  |
| `delay_on_ms` | Tehetetlenség BE (csak ISOBUS TC) **[ISOBUS]** | 800 ms | 0 – 5000 ms | SZAKASZOLÁS, TC | Csak az ISOBUS Task Controllernek szól (előre kapcsolás). AgOpenGPS-nél az előretekintést az AgOpenGPS szakaszbeállításainál kell megadni. |
| `delay_off_ms` | Tehetetlenség KI (csak ISOBUS TC) **[ISOBUS]** | 400 ms | 0 – 5000 ms | SZAKASZOLÁS, TC |  |

## AgOpenGPS (Ethernet / UDP)

| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |
|---|---|---|---|---|---|
| `aog_enable` | AgOpenGPS kapcsolat (Ethernet, UDP 8888 / 9999) **[AgOpenGPS]** | Nem | Nem / Igen | AGOPENGPS | Csak Ethernetes panelen (Waveshare ESP32-S3-ETH). A vezérlő gépmodulként jelenik meg az AgIO-ban: sebességet (PGN 254) és szakaszállapotot (PGN 239) fogad, festést / munkakapcsolót (PGN 234) küld. Az autokormány-modul kommunikációjához nem nyúl. |
| `aog_ip_mode` | IP-cím **[AgOpenGPS]** | Fix IP | Fix IP / DHCP | AGOPENGPS | AgOpenGPS-hez fix IP kell, az AgIO alhálózatában (alap: 192.168.5.x). |
| `aog_ip` | Fix IP-cím (a 4. bájt maradjon szabad az AgIO hálózatán) **[AgOpenGPS]** | `192.168.5.130` | 7–15 karakter | – | Az AgIO 'alhálózat beállítása' (PGN 201) az első három bájtot átírja. |
| `aog_paint` | Festés / munkakapcsoló küldése az AgOpenGPS-nek **[AgOpenGPS]** | Igen | Nem / Igen | AGOPENGPS | Gép lent: 3 'BE' csomag (utána a kezelő az AgOpenGPS-ben kézzel leállíthatja), gép fent: 'KI' 500 ms-onként. Az AgOpenGPS a festést a szakaszaival együtt kezeli. |
| `aog_paint_mode` | Festés módja **[AgOpenGPS]** | Automata / zöld | Automata / zöld / Kézi / sárga | AGOPENGPS | Automata: az AgOpenGPS automata szakaszként kezeli (zöld). Kézi: fix kézi BE/KI a vetőgép szakaszai szerint (sárga). Ha a szakaszolás forrása AgOpenGPS, mindig automata. |

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
| `isobus_delay_s` | ISOBUS indulás késleltetése (terminál lassabban indul) **[ISOBUS]** | 0 s | 0 – 60 s | – | Ennyi másodperccel később jelentkezik be a CAN buszra. Ha a terminál (pl. Müller) a később induló gépet nem találja, állíts be 10–20 s-ot. |
| `isobus_retry_s` | Újracsatlakozás, ha nincs terminál (0 = ki) **[ISOBUS]** | 30 s | 0 – 600 s | – | Ha ennyi ideig nincs VT kapcsolat, a vezérlő újra bejelentkezik (címigénylés), és a VT/TC kapcsolatot újrakezdi. |
| `realarm_s` | Nyugtázott, fennálló riasztás újra (0 = soha) | 0 s | 0 – 3600 s | – |  |

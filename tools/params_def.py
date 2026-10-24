#!/usr/bin/env python3
"""Egyetlen igazságforrás a vetőgép-vezérlő beállítható paramétereihez.

Ebből generálódik:
  * include/seeder/params_gen.hpp  (C++ Settings struktúra + paramétertábla)
  * a VT object pool beállítóoldalai (tools/pool/generate_pool.py)
  * a webes felület űrlapjai (a firmware a /api/schema végponton adja ki)
  * docs/PARAMETEREK_HU.md

Szabályok:
  * A kulcs (key) soha ne változzon, ha már kint van firmware: ez az NVS/JSON azonosító.
  * Új paraméter bárhová kerülhet, de a kulcs legyen új. A tárolás kulcs alapú.
  * A `vt_page` adja meg, melyik VT beállítóoldalon jelenjen meg (None = csak web).
  * `feature`: a paraméter csak akkor látszik a weben, ha ez a (bool) funkció engedélyezve van.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Optional, Sequence

MAX_METERS = 2
MAX_SECTIONS = 2


@dataclass(frozen=True)
class Param:
    key: str
    group: str
    label: str            # teljes magyar felirat (web)
    vt_label: str         # rövid felirat a VT-re (max 20 karakter)
    kind: str             # "float" | "int" | "enum" | "bool" | "str"
    default: float | int | str
    min: float = 0
    max: float = 0
    step: float = 1
    decimals: int = 0
    unit: str = ""
    options: Sequence[str] = ()
    vt_page: Optional[str] = None
    help: str = ""
    feature: str = ""             # web: csak akkor látszik, ha ez a kapcsoló Igen
    c_name: str = ""

    @property
    def cname(self) -> str:
        return self.c_name or self.key


GROUPS = [
    # (azonosító, web felirat, ikon kulcs)
    ("machine", "Gép", "machine"),
    ("rate", "Adagolás és feltöltés", "rate"),
    ("drive", "Adagolóhajtás", "drive"),
    ("fan", "Turbina", "fan"),
    ("speed", "Sebesség", "speed"),
    ("switches", "Kapcsolók", "switches"),
    ("extras", "Kiegészítők", "extras"),
    ("pressure", "Nyomásmérés (lenyomatás)", "pressure"),
    ("cooling", "Olajhőmérséklet és hűtés", "cooling"),
    ("scale", "Mérleg", "scale"),
    ("tc", "Task Controller és geometria", "tc"),
    ("sim", "Szimuláció / teszt", "sim"),
    ("system", "Rendszer", "system"),
]

YES_NO = ("Nem", "Igen")
NO_NC = ("NO (záró)", "NC (bontó)")

P: list[Param] = []


def add(*args, **kw):
    P.append(Param(*args, **kw))


# --------------------------------------------------------------------------- gép
add("width_m", "machine", "Munkaszélesség", "Munkaszélesség", "float", 6.0, 1.0, 20.0, 0.01, 2, "m",
    vt_page="machine")
add("section_count", "machine", "Szakaszok száma", "Szakaszok száma", "int", 2, 1, MAX_SECTIONS, 1, 0, "db",
    vt_page="machine",
    help="2 szakasz: az 1. adagoló a bal, a 2. a jobb félszélességet veti. 1 szakasz: a Task Controller a "
         "teljes szélességet egyben kapcsolja (pl. első/hátsó soros gépnél mindkét adagolót együtt), a két "
         "adagoló gombja a kezelőnek továbbra is külön működik.")
add("meter_count", "machine", "Adagolók száma", "Adagolók száma", "int", 2, 1, MAX_METERS, 1, 0, "db",
    vt_page="machine")
add("tank_count", "machine", "Tartályok (rekeszek) száma", "Tartályok száma", "int", 2, 1, 2, 1, 0, "db",
    vt_page="machine")
add("meter_layout", "machine", "Két adagoló elrendezése", "Adagolók", "enum", 0,
    options=("Bal / jobb félszélesség", "Első / hátsó sor (teljes szélesség)"), vt_page="machine",
    help="Első/hátsó soros gépnél mindkét adagoló a teljes szélességet veti: a terület mindig a teljes "
         "szélességgel számolódik, és csak 1 szakasz lehet.")
add("sec_w1", "machine", "1. (bal) szakasz szélessége", "1. szakasz", "float", 3.0, 0.1, 20.0, 0.01, 2, "m",
    vt_page="machine", help="2 szakasznál. A 2. szakasz = munkaszélesség − 1. szakasz.")

# --------------------------------------------------------------------------- adagolás
add("rate1", "rate", "1. adagoló előírt mennyiség", "Előírt 1", "float", 200.0, 0.5, 1000.0, 0.5, 1, "kg/ha",
    vt_page="rate")
add("rate2", "rate", "2. adagoló előírt mennyiség", "Előírt 2", "float", 200.0, 0.5, 1000.0, 0.5, 1, "kg/ha",
    vt_page="rate")
add("cal1_kg_rev", "rate", "1. adagoló kg / adagolófordulat (leforgatásból)", "Kg/ford. 1", "float", 0.2353,
    0.0001, 20.0, 0.0001, 4, "kg", vt_page="rate",
    help="A leforgatás eredménye. Elektromos hajtásnál ebből számolja a szükséges fordulatot, talajhajtásnál "
         "a kijuttatott mennyiséget.")
add("cal2_kg_rev", "rate", "2. adagoló kg / adagolófordulat (leforgatásból)", "Kg/ford. 2", "float", 0.2353,
    0.0001, 20.0, 0.0001, 4, "kg", vt_page="rate")
add("rate_step_pct", "rate", "Léptetés (+/− gomb)", "Léptetés +/-", "float", 5.0, 1, 50, 1, 0, "%",
    vt_page="rate")
add("rate_tol_pct", "rate", "Eltérés riasztás (elektromos hajtás)", "Eltérés riasztás", "float", 10.0, 2, 50, 1, 0,
    "%", vt_page="rate")
add("prefill_auto", "rate", "Automatikus feltöltés induláskor (sarkokban)", "Auto feltöltés", "bool", 1,
    options=YES_NO, vt_page="rate",
    help="Csak elektromos hajtásnál. Ha álló gépnél leengeded a gépet (munkakapcsoló) vagy elindítod a vetést, "
         "az adagolók a 'feltöltési sebességhez' tartozó fordulattal előre feltöltik a csöveket, majd "
         "az indulás után még az utánfutási ideig így forognak.")
add("prefill_speed_kmh", "rate", "Feltöltési (elméleti) sebesség", "Feltöltési seb.", "float", 8.0, 1.0, 20.0,
    0.1, 1, "km/h", vt_page="rate")
add("prefill_time_s", "rate", "Feltöltési idő (álló gép)", "Feltöltési idő", "float", 3.0, 0.0, 30.0, 0.1, 1, "s",
    vt_page="rate")
add("prefill_after_s", "rate", "Utánfutás indulás után", "Utánfutás", "float", 4.0, 0.0, 20.0, 0.1, 1, "s",
    vt_page="rate")

# --------------------------------------------------------------------------- hajtás
add("drive_type", "drive", "Adagolók hajtása", "Hajtás", "enum", 0,
    options=("Talajhajtás + elektromágneses kuplung", "Elektromos motor (PWM)"), vt_page="drive")
add("clutch_mode", "drive", "Kuplung típusa", "Kuplung", "enum", 0,
    options=("Áram alatt hajt", "Áram alatt old"), vt_page="drive")
add("meter_ppr", "drive", "Adagolótengely-jeladó impulzus / fordulat (talajhajtás)", "Jeladó imp/ford", "int", 1,
    1, 100, 1, 0, "imp", vt_page="drive")
add("meter_timeout_s", "drive", "Adagoló nem forog riasztás késleltetése", "Nem forog késl.", "float", 5.0, 1.0,
    60.0, 0.5, 1, "s", vt_page="drive",
    help="Ha a vetés feltételei teljesülnek (vetés BE, gép lent, halad, adagoló bekapcsolva), és ennyi ideig "
         "nem jön impulzus az adagolótól, riasztás.")
add("encoder_ppr", "drive", "Enkóder impulzus / adagolófordulat (motor)", "Enkóder imp/ford", "int", 50, 1, 5000, 1,
    0, "imp", vt_page="drive")
add("motor_max_rpm", "drive", "Adagoló fordulat 100% PWM-nél", "Ford. 100% PWM", "float", 60.0, 1.0, 600.0, 0.5, 1,
    "1/min", vt_page="drive", help="Az előrecsatoláshoz; a szabályzó ebből indul, utána az enkóder alapján korrigál.")
add("pwm_min_pct", "drive", "PWM minimum (forgás közben)", "PWM minimum", "float", 8.0, 0, 100, 1, 0, "%",
    vt_page="drive")
add("pwm_max_pct", "drive", "PWM maximum", "PWM maximum", "float", 100.0, 1, 100, 1, 0, "%", vt_page="drive")
add("ramp_up_s", "drive", "Felfutás (0 → 100%)", "Felfutás", "float", 1.0, 0.0, 10.0, 0.1, 1, "s",
    vt_page="drive")
add("ramp_down_s", "drive", "Lefutás (100% → 0)", "Lefutás", "float", 0.5, 0.0, 10.0, 0.1, 1, "s",
    vt_page="drive")
add("reg_gain", "drive", "Szabályzási állandó (1 lassú – 10 gyors)", "Szab. állandó", "int", 5, 1, 10, 1, 0, "",
    vt_page="drive")
add("pwm_freq_hz", "drive", "PWM frekvencia", "PWM frekvencia", "int", 1000, 100, 20000, 100, 0, "Hz")

# --------------------------------------------------------------------------- turbina
add("fan_ppr", "fan", "Turbina impulzus / fordulat", "Imp/fordulat", "int", 1, 1, 100, 1, 0, "imp", vt_page="fan")
add("fan_min_rpm", "fan", "Riasztás e fordulat alatt", "Min. fordulat", "int", 2500, 0, 20000, 10, 0, "1/min",
    vt_page="fan",
    help="Csak akkor riaszt, ha a gép dolgozik: munkakapcsolóval lent van és halad; munkakapcsoló nélkül "
         "ha halad és a vetés BE.")
add("fan_max_rpm", "fan", "Riasztás e fordulat felett (0 = nincs)", "Max. fordulat", "int", 0, 0, 20000, 10, 0,
    "1/min", vt_page="fan")
add("fan_delay_s", "fan", "Riasztás késleltetése", "Késleltetés", "float", 5.0, 0.0, 60.0, 0.5, 1, "s",
    vt_page="fan")

# --------------------------------------------------------------------------- sebesség
add("speed_src", "speed", "Sebesség forrása", "Sebesség forrása", "enum", 0,
    options=("Kerékjeladó", "ISOBUS (GPS/radar/traktor)", "ISOBUS, kiesésnél kerék", "Szimulált"),
    vt_page="speed")
add("isobus_speed_pref", "speed", "ISOBUS sebességüzenet", "ISOBUS üzenet", "enum", 0,
    options=("Automatikus", "Földsebesség (radar/GPS)", "Traktor kerék", "Gép által választott", "NMEA GPS (SOG)"),
    vt_page="speed", help="Automatikus sorrend: gép által választott > föld (radar/GPS) > NMEA GPS > traktor kerék. "
                          "Ha a terminál megosztja a GPS sebességét, az 'NMEA GPS' vagy 'Földsebesség' jön.")
add("wheel_ppm", "speed", "Kerékjeladó impulzus / 100 m", "Imp/100 m", "float", 100.0, 1, 100000, 0.1, 1, "imp",
    vt_page="speed")
add("sim_speed", "speed", "Szimulált sebesség", "Szimulált seb.", "float", 8.0, 0, 30, 0.1, 1, "km/h",
    vt_page="speed")
add("min_work_speed", "speed", "Haladás küszöb (ez alatt áll a gép)", "Haladás küszöb", "float", 1.0, 0.1, 10.0, 0.1,
    1, "km/h", vt_page="speed")
add("speed_timeout_ms", "speed", "Sebességjel időtúllépés (ISOBUS)", "Jel időtúllépés", "int", 1500, 200, 10000, 100,
    0, "ms")

# --------------------------------------------------------------------------- kapcsolók
add("work_sw", "switches", "Munkakapcsoló (gép lent)", "Munkakapcsoló", "enum", 1,
    options=("Nincs", "Van – NO (lent zár)", "Van – NC (lent bont)"), vt_page="switches",
    help="Ha van, a 'gép lent' állapot a vetés (adagolók, terület) feltétele.")
add("work_sw_tc", "switches", "Munkakapcsoló az automata szakaszolásnál is feltétel", "Munkakapcs. → TC",
    "bool", 1, options=YES_NO, vt_page="switches",
    help="Igen: TC-SEC automata szakaszolásnál is csak lent vet (a TC-nek kiemelt gépnél 'nem dolgozik' "
         "jelentést küldünk). Nem: automatában csak a TC dönt.")
add("hopper1_nc", "switches", "1. tartály leürülésgátló kapcsoló", "1. tartály kapcs.", "enum", 0, options=NO_NC,
    vt_page="switches", help="NO: üres tartálynál zár. NC: üres tartálynál bont.")
add("hopper2_nc", "switches", "2. tartály leürülésgátló kapcsoló", "2. tartály kapcs.", "enum", 0, options=NO_NC,
    vt_page="switches")
add("hopper_delay_s", "switches", "Leürülés jelzés szűrése", "Tartály szűrés", "float", 2.0, 0.0, 30.0, 0.5, 1, "s",
    vt_page="switches")
add("out_invert", "switches", "Kimenetek fordított szintje (relémodul)", "Kimenet fordítva", "bool", 0,
    options=YES_NO, vt_page="switches", help="Igen: alacsony szint kapcsolja be a relét (kuplung, lámpa, szelep, hűtő).")

# --------------------------------------------------------------------------- kiegészítők
add("lights_enable", "extras", "Munkalámpák (2 db)", "Munkalámpák", "bool", 0, options=YES_NO, vt_page="extras")
add("valve_enable", "extras", "Nyitás / nyomjelző váltószelep", "Nyitás/nyomjelző", "bool", 0, options=YES_NO,
    vt_page="extras", help="Alapban a hidraulikakör a gépet nyitja/csukja; a szelep bekapcsolva a nyomjelzőre tereli.")
add("pressure_enable", "extras", "Nyomásmérés (lenyomatás, ADS1115)", "Nyomásmérés", "bool", 0, options=YES_NO,
    vt_page="extras")
add("temp_enable", "extras", "Olajhőmérséklet + hűtőventilátor (2× DS18B20)", "Olajhőmérséklet", "bool", 0,
    options=YES_NO, vt_page="extras")
add("scale_enable", "extras", "Leforgatási mérleg (HX711)", "Mérleg", "bool", 0, options=YES_NO, vt_page="extras")

# --------------------------------------------------------------------------- nyomás
add("pressure_range_bar", "pressure", "Szenzor méréshatár", "Méréshatár", "float", 250.0, 1.0, 1000.0, 1.0, 0, "bar",
    vt_page="pressure", feature="pressure_enable")
add("pressure_signal", "pressure", "Szenzor kimenőjele", "Kimenőjel", "enum", 1,
    options=("0–5 V", "0,5–4,5 V", "0,5–5 V", "0–4,5 V"), vt_page="pressure", feature="pressure_enable")
add("pressure_cal", "pressure", "Kalibrációs szorzó", "Kalibr. szorzó", "float", 1.0, 0.5, 2.0, 0.001, 3, "",
    vt_page="pressure", feature="pressure_enable",
    help="Az 'Ismert nyomás' kalibrálás állítja be (1,000 = gyári jelleggörbe).")
add("pressure_zero_bar", "pressure", "Nullpont-eltolás", "Nullpont", "float", 0.0, -50.0, 50.0, 0.1, 1, "bar",
    vt_page="pressure", feature="pressure_enable")
add("pressure_ch", "pressure", "ADS1115 csatorna", "ADS csatorna", "int", 0, 0, 3, 1, 0, "",
    feature="pressure_enable")
add("pressure_divider", "pressure", "Bemeneti feszültségosztó (V szenzor / V ADS)", "", "float", 1.0, 1.0, 10.0,
    0.001, 3, "", feature="pressure_enable",
    help="Ha az ADS1115 bemenete előtt osztó van (pl. 3,3 V-os táp esetén), add meg az arányát.")

# --------------------------------------------------------------------------- olajhűtés
add("cooler_on_c", "cooling", "Hűtőventilátor bekapcsol", "Bekapcsol", "float", 60.0, 20.0, 120.0, 0.5, 1, "°C",
    vt_page="cooling", feature="temp_enable")
add("cooler_off_c", "cooling", "Hűtőventilátor kikapcsol", "Kikapcsol", "float", 50.0, 10.0, 115.0, 0.5, 1, "°C",
    vt_page="cooling", feature="temp_enable")
add("cooler_src", "cooling", "Szabályozó hőmérő", "Szabályozó hőmérő", "enum", 0,
    options=("Olajtartály", "Hűtő kimenet", "A melegebb"), vt_page="cooling", feature="temp_enable")
add("temp_alarm_c", "cooling", "Túlmelegedés riasztás (0 = nincs)", "Riasztás felett", "float", 85.0, 0.0, 130.0, 0.5,
    1, "°C", vt_page="cooling", feature="temp_enable")
add("temp_swap", "cooling", "A két DS18B20 hőmérő felcserélése", "", "bool", 0, options=YES_NO,
    feature="temp_enable",
    help="A hőmérőket a firmware a ROM-azonosítójuk sorrendjében számozza. Ha az olajtartály és a hűtő "
         "kimenet értéke fordítva jelenik meg, kapcsold be.")

# --------------------------------------------------------------------------- mérleg
add("scale_capacity_kg", "scale", "Mérlegcella méréshatára", "Méréshatár", "float", 50.0, 1.0, 500.0, 0.5, 1, "kg",
    vt_page="scale", feature="scale_enable")
add("scale_factor", "scale", "Mérleg kalibrációs tényező (beütés / kg)", "Tényező", "float", 85000.0, 1.0, 10000000.0,
    0.1, 1, "1/kg", vt_page="scale", feature="scale_enable",
    help="A 'Kalibrálás ismert súllyal' állítja be.")
add("scale_offset", "scale", "Mérleg nullpont (tára, nyers)", "", "float", 0.0, -20000000.0, 20000000.0, 1.0, 0, "",
    feature="scale_enable")

# --------------------------------------------------------------------------- TC + geometria
add("tc_enable", "tc", "Task Controller kliens", "TC kliens", "bool", 1, options=YES_NO, vt_page="tc")
add("tc_sec", "tc", "Automatikus szakaszvezérlés (TC-SEC)", "TC-SEC", "bool", 1, options=YES_NO, vt_page="tc")
add("tc_geo", "tc", "Kijuttatási térkép (TC-GEO, kg/ha az adagolóknak)", "TC-GEO", "bool", 0, options=YES_NO,
    vt_page="tc")
add("hitch_type", "tc", "Csatlakozás", "Csatlakozás", "enum", 2,
    options=("Függesztett", "Kormányzott vonórúd", "Vonórúd (vonószem)", "Vonóhorog", "Gömbfej (K80)"),
    vt_page="geometry")
add("geo_a_m", "tc", "A: vonópont → csuklópont (kormányzott vonórúd)", "A vonópont-csukló", "float", 0.0, 0.0, 20.0,
    0.01, 2, "m", vt_page="geometry")
add("geo_b_m", "tc", "B: csuklópont → csoroszlyák (függesztett: vonópont → csoroszlyák)", "B csukló-csorosz.",
    "float", 4.0, 0.0, 20.0, 0.01, 2, "m", vt_page="geometry")
add("geo_c_m", "tc", "C: csuklópont → kerékagy (fordulópont)", "C csukló-tengely", "float", 3.0, 0.0, 20.0, 0.01, 2,
    "m", vt_page="geometry")
add("geo_y_m", "tc", "D: oldalirányú eltolás (+ jobbra)", "D oldaleltolás", "float", 0.0, -5.0, 5.0, 0.01, 2, "m",
    vt_page="geometry")
add("delay_on_ms", "tc", "Tehetetlenség BE", "Tehetetlenség BE", "int", 800, 0, 5000, 10, 0, "ms",
    vt_page="tc")
add("delay_off_ms", "tc", "Tehetetlenség KI", "Tehetetlenség KI", "int", 400, 0, 5000, 10, 0, "ms",
    vt_page="tc")

# --------------------------------------------------------------------------- szimuláció
add("sim_inputs", "sim", "Bemenetek szimulációja (turbina, adagolók, munkakapcsoló)", "Bemenetek szim.",
    "bool", 0, options=YES_NO, vt_page="diag",
    help="Próbapadi teszthez: a turbina, az adagolók és az enkóderek impulzusait a firmware állítja elő, a "
         "munkakapcsoló 'lent' állapotú.")
add("sim_fan_rpm", "sim", "Szimulált turbinafordulat", "Szim. turbina", "int", 3500, 0, 10000, 10, 0, "1/min",
    vt_page="diag")
add("sim_wheel", "sim", "Kerékimpulzus szimuláció a szimulált sebességből", "Kerék szimuláció", "bool", 0,
    options=YES_NO, vt_page="diag")

# --------------------------------------------------------------------------- rendszer
add("wifi_pass", "system", "Wi-Fi jelszó (8–32 karakter)", "", "str", "12345678", 8, 32)
add("ecu_instance", "system", "ECU példány (több gép egy buszon)", "", "int", 0, 0, 7, 1, 0, "")
add("realarm_s", "system", "Nyugtázott, fennálló riasztás újra (0 = soha)", "", "int", 0, 0, 3600, 10, 0, "s")


# --------------------------------------------------------------------------- VT oldalak
VT_PAGES = [
    # (azonosító, cím a VT-n)
    ("machine", "GÉP"),
    ("rate", "ADAGOLÁS, FELTÖLTÉS"),
    ("drive", "ADAGOLÓHAJTÁS"),
    ("fan", "TURBINA"),
    ("speed", "SEBESSÉG"),
    ("switches", "KAPCSOLÓK"),
    ("extras", "KIEGÉSZÍTÕK"),
    ("pressure", "NYOMÁSMÉRÉS"),
    ("cooling", "OLAJHÕMÉRSÉKLET"),
    ("scale", "MÉRLEG"),
    ("tc", "TASK CONTROLLER"),
    ("geometry", "GEOMETRIA"),
    ("diag", "SZIMULÁCIÓ / TESZT"),
]


def by_key():
    return {p.key: p for p in P}


def validate():
    keys = set()
    bools = {p.key for p in P if p.kind == "bool"}
    for p in P:
        assert p.key not in keys, p.key
        keys.add(p.key)
        assert p.group in {g[0] for g in GROUPS}, p
        if p.kind in ("float", "int"):
            assert p.min <= float(p.default) <= p.max, p
        if p.kind in ("enum", "bool"):
            assert 0 <= int(p.default) < len(p.options), p
        if p.vt_page:
            assert p.vt_page in {x[0] for x in VT_PAGES}, p
            assert len(p.vt_label) <= 17, p.vt_label
        if p.feature:
            assert p.feature in bools, p
    return True


validate()

// GENERÁLT FÁJL – ne szerkeszd kézzel! Forrás: tools/params_def.py, generátor: tools/gen_params.py
#pragma once
#include <cstddef>
#include <cstdint>

namespace seeder {
constexpr int MAX_SECTIONS = 2;
constexpr int MAX_METERS = 2;

struct Settings {
    float width_m;
    std::int32_t section_count;
    std::int32_t meter_count;
    std::int32_t tank_count;
    std::int32_t meter_layout;
    float sec_w1;
    float rate1;
    float rate2;
    float cal1_kg_rev;
    float cal2_kg_rev;
    std::int32_t meter_link;
    float rate_step_pct;
    float rate_tol_pct;
    std::int32_t prefill_auto;
    float prefill_speed_kmh;
    float prefill_time_s;
    float prefill_after_s;
    std::int32_t drive_type;
    std::int32_t clutch_mode;
    std::int32_t meter_ppr;
    float meter_timeout_s;
    std::int32_t encoder_ppr;
    float motor_max_rpm;
    float pwm_min_pct;
    float pwm_max_pct;
    float ramp_up_s;
    float ramp_down_s;
    std::int32_t reg_gain;
    std::int32_t pwm_freq_hz;
    std::int32_t fan_ppr;
    std::int32_t fan_min_rpm;
    std::int32_t fan_max_rpm;
    float fan_delay_s;
    std::int32_t speed_src;
    std::int32_t isobus_speed_pref;
    float wheel_ppm;
    float sim_speed;
    float min_work_speed;
    std::int32_t speed_timeout_ms;
    std::int32_t work_sw;
    std::int32_t work_sw_tc;
    std::int32_t hopper1_nc;
    std::int32_t hopper2_nc;
    float hopper_delay_s;
    std::int32_t out_invert;
    std::int32_t lights_enable;
    std::int32_t valve_enable;
    std::int32_t pressure_enable;
    std::int32_t temp_enable;
    std::int32_t scale_enable;
    float pressure_range_bar;
    std::int32_t pressure_signal;
    float pressure_cal;
    float pressure_zero_bar;
    std::int32_t pressure_ch;
    float pressure_divider;
    float cooler_on_c;
    float cooler_off_c;
    std::int32_t cooler_src;
    float temp_alarm_c;
    std::int32_t temp_swap;
    float scale_capacity_kg;
    float scale_factor;
    float scale_offset;
    std::int32_t tc_enable;
    std::int32_t tc_sec;
    std::int32_t tc_geo;
    std::int32_t hitch_type;
    float geo_a_m;
    float geo_b_m;
    float geo_c_m;
    float geo_y_m;
    std::int32_t delay_on_ms;
    std::int32_t delay_off_ms;
    std::int32_t sim_inputs;
    std::int32_t sim_fan_rpm;
    std::int32_t sim_wheel;
    char wifi_pass[33];
    std::int32_t ecu_instance;
    std::int32_t realarm_s;
};

enum class ParamKind : std::uint8_t { Float, Int, Enum, Bool, Str };

struct ParamDef {
    const char *key;
    const char *group;
    const char *label;
    const char *vt_label;
    const char *unit;
    const char *help;
    ParamKind kind;
    float min, max, step, def;
    std::uint8_t decimals;
    const char *const *options;
    std::uint8_t option_count;
    std::uint16_t offset;
    const char *def_str;
    std::uint16_t vt_input_id;
    const char *vt_page;
    const char *feature;
};

struct GroupDef { const char *id; const char *label; const char *icon; };

enum ParamId : std::uint16_t {
    P_width_m = 0,
    P_section_count = 1,
    P_meter_count = 2,
    P_tank_count = 3,
    P_meter_layout = 4,
    P_sec_w1 = 5,
    P_rate1 = 6,
    P_rate2 = 7,
    P_cal1_kg_rev = 8,
    P_cal2_kg_rev = 9,
    P_meter_link = 10,
    P_rate_step_pct = 11,
    P_rate_tol_pct = 12,
    P_prefill_auto = 13,
    P_prefill_speed_kmh = 14,
    P_prefill_time_s = 15,
    P_prefill_after_s = 16,
    P_drive_type = 17,
    P_clutch_mode = 18,
    P_meter_ppr = 19,
    P_meter_timeout_s = 20,
    P_encoder_ppr = 21,
    P_motor_max_rpm = 22,
    P_pwm_min_pct = 23,
    P_pwm_max_pct = 24,
    P_ramp_up_s = 25,
    P_ramp_down_s = 26,
    P_reg_gain = 27,
    P_pwm_freq_hz = 28,
    P_fan_ppr = 29,
    P_fan_min_rpm = 30,
    P_fan_max_rpm = 31,
    P_fan_delay_s = 32,
    P_speed_src = 33,
    P_isobus_speed_pref = 34,
    P_wheel_ppm = 35,
    P_sim_speed = 36,
    P_min_work_speed = 37,
    P_speed_timeout_ms = 38,
    P_work_sw = 39,
    P_work_sw_tc = 40,
    P_hopper1_nc = 41,
    P_hopper2_nc = 42,
    P_hopper_delay_s = 43,
    P_out_invert = 44,
    P_lights_enable = 45,
    P_valve_enable = 46,
    P_pressure_enable = 47,
    P_temp_enable = 48,
    P_scale_enable = 49,
    P_pressure_range_bar = 50,
    P_pressure_signal = 51,
    P_pressure_cal = 52,
    P_pressure_zero_bar = 53,
    P_pressure_ch = 54,
    P_pressure_divider = 55,
    P_cooler_on_c = 56,
    P_cooler_off_c = 57,
    P_cooler_src = 58,
    P_temp_alarm_c = 59,
    P_temp_swap = 60,
    P_scale_capacity_kg = 61,
    P_scale_factor = 62,
    P_scale_offset = 63,
    P_tc_enable = 64,
    P_tc_sec = 65,
    P_tc_geo = 66,
    P_hitch_type = 67,
    P_geo_a_m = 68,
    P_geo_b_m = 69,
    P_geo_c_m = 70,
    P_geo_y_m = 71,
    P_delay_on_ms = 72,
    P_delay_off_ms = 73,
    P_sim_inputs = 74,
    P_sim_fan_rpm = 75,
    P_sim_wheel = 76,
    P_wifi_pass = 77,
    P_ecu_instance = 78,
    P_realarm_s = 79,
    PARAM_COUNT = 80
};

constexpr std::size_t GROUP_COUNT = 13;
constexpr std::uint16_t VT_INPUT_BASE = 9000;
extern const ParamDef PARAMS[PARAM_COUNT];
extern const GroupDef GROUPS[GROUP_COUNT];

#ifdef SEEDER_PARAMS_IMPLEMENTATION
static const char *const OPT_meter_layout[] = {"Bal / jobb félszélesség", "Első / hátsó sor (teljes szélesség)"};
static const char *const OPT_meter_link[] = {"Nem", "Igen"};
static const char *const OPT_prefill_auto[] = {"Nem", "Igen"};
static const char *const OPT_drive_type[] = {"Talajhajtás + elektromágneses kuplung", "Elektromos motor (PWM)"};
static const char *const OPT_clutch_mode[] = {"Áram alatt hajt", "Áram alatt old"};
static const char *const OPT_speed_src[] = {"Kerékjeladó", "ISOBUS (GPS/radar/traktor)", "ISOBUS, kiesésnél kerék", "Szimulált"};
static const char *const OPT_isobus_speed_pref[] = {"Automatikus", "Földsebesség (radar/GPS)", "Traktor kerék", "Gép által választott", "NMEA GPS (SOG)"};
static const char *const OPT_work_sw[] = {"Nincs", "Van – NO (lent zár)", "Van – NC (lent bont)"};
static const char *const OPT_work_sw_tc[] = {"Nem", "Igen"};
static const char *const OPT_hopper1_nc[] = {"NO (záró)", "NC (bontó)"};
static const char *const OPT_hopper2_nc[] = {"NO (záró)", "NC (bontó)"};
static const char *const OPT_out_invert[] = {"Nem", "Igen"};
static const char *const OPT_lights_enable[] = {"Nem", "Igen"};
static const char *const OPT_valve_enable[] = {"Nem", "Igen"};
static const char *const OPT_pressure_enable[] = {"Nem", "Igen"};
static const char *const OPT_temp_enable[] = {"Nem", "Igen"};
static const char *const OPT_scale_enable[] = {"Nem", "Igen"};
static const char *const OPT_pressure_signal[] = {"0–5 V", "0,5–4,5 V", "0,5–5 V", "0–4,5 V"};
static const char *const OPT_cooler_src[] = {"Olajtartály", "Hűtő kimenet", "A melegebb"};
static const char *const OPT_temp_swap[] = {"Nem", "Igen"};
static const char *const OPT_tc_enable[] = {"Nem", "Igen"};
static const char *const OPT_tc_sec[] = {"Nem", "Igen"};
static const char *const OPT_tc_geo[] = {"Nem", "Igen"};
static const char *const OPT_hitch_type[] = {"Függesztett", "Kormányzott vonórúd", "Vonórúd (vonószem)", "Vonóhorog", "Gömbfej (K80)"};
static const char *const OPT_sim_inputs[] = {"Nem", "Igen"};
static const char *const OPT_sim_wheel[] = {"Nem", "Igen"};

const GroupDef GROUPS[GROUP_COUNT] = {
    {"machine", "Gép", "machine"},
    {"rate", "Adagolás és feltöltés", "rate"},
    {"drive", "Adagolóhajtás", "drive"},
    {"fan", "Turbina", "fan"},
    {"speed", "Sebesség", "speed"},
    {"switches", "Kapcsolók", "switches"},
    {"extras", "Kiegészítők", "extras"},
    {"pressure", "Nyomásmérés (lenyomatás)", "pressure"},
    {"cooling", "Olajhőmérséklet és hűtés", "cooling"},
    {"scale", "Mérleg", "scale"},
    {"tc", "Task Controller és geometria", "tc"},
    {"sim", "Szimuláció / teszt", "sim"},
    {"system", "Rendszer", "system"},
};

const ParamDef PARAMS[PARAM_COUNT] = {
    {"width_m", "machine", "Munkaszélesség", "Munkaszélesség", "m", "", ParamKind::Float, 1.0f, 20.0f, 0.01f, 6.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, width_m)), nullptr, 9000, "machine", ""},
    {"section_count", "machine", "Szakaszok száma", "Szakaszok száma", "db", "2 szakasz: az 1. adagoló a bal, a 2. a jobb félszélességet veti. 1 szakasz: a Task Controller a teljes szélességet egyben kapcsolja (pl. első/hátsó soros gépnél mindkét adagolót együtt), a két adagoló gombja a kezelőnek továbbra is külön működik.", ParamKind::Int, 1.0f, 2.0f, 1.0f, 2.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, section_count)), nullptr, 9001, "machine", ""},
    {"meter_count", "machine", "Adagolók száma", "Adagolók száma", "db", "", ParamKind::Int, 1.0f, 2.0f, 1.0f, 2.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, meter_count)), nullptr, 9002, "machine", ""},
    {"tank_count", "machine", "Tartályok (rekeszek) száma", "Tartályok száma", "db", "", ParamKind::Int, 1.0f, 2.0f, 1.0f, 2.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, tank_count)), nullptr, 9003, "machine", ""},
    {"meter_layout", "machine", "Két adagoló elrendezése", "Adagolók", "", "Első/hátsó soros gépnél mindkét adagoló a teljes szélességet veti: a terület mindig a teljes szélességgel számolódik, és csak 1 szakasz lehet.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_meter_layout, 2, static_cast<std::uint16_t>(offsetof(Settings, meter_layout)), nullptr, 9004, "machine", ""},
    {"sec_w1", "machine", "1. (bal) szakasz szélessége", "1. szakasz", "m", "2 szakasznál. A 2. szakasz = munkaszélesség − 1. szakasz.", ParamKind::Float, 0.1f, 20.0f, 0.01f, 3.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, sec_w1)), nullptr, 9005, "machine", ""},
    {"rate1", "rate", "1. adagoló előírt mennyiség", "Előírt 1", "kg/ha", "", ParamKind::Float, 0.5f, 1000.0f, 0.5f, 200.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rate1)), nullptr, 9006, "rate", ""},
    {"rate2", "rate", "2. adagoló előírt mennyiség", "Előírt 2", "kg/ha", "", ParamKind::Float, 0.5f, 1000.0f, 0.5f, 200.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rate2)), nullptr, 9007, "rate", ""},
    {"cal1_kg_rev", "rate", "1. adagoló kg / adagolófordulat (leforgatásból)", "Kg/ford. 1", "kg", "A leforgatás eredménye. Elektromos hajtásnál ebből számolja a szükséges fordulatot, talajhajtásnál a kijuttatott mennyiséget.", ParamKind::Float, 0.0001f, 20.0f, 0.0001f, 0.2353f, 4, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, cal1_kg_rev)), nullptr, 9008, "rate", ""},
    {"cal2_kg_rev", "rate", "2. adagoló kg / adagolófordulat (leforgatásból)", "Kg/ford. 2", "kg", "", ParamKind::Float, 0.0001f, 20.0f, 0.0001f, 0.2353f, 4, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, cal2_kg_rev)), nullptr, 9009, "rate", ""},
    {"meter_link", "rate", "A 2. adagoló ugyanazt adagolja (2. = 1.)", "2. = 1. adagoló", "", "Két adagolónál, ha mindkettő ugyanazt a vetőmagot veti: az előírt mennyiség és a kg/fordulat a két adagolón mindig egyforma (bármelyiket állítod vagy forgatod le, a másik is átveszi). A főképernyőn „1=2” jelzi.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_meter_link, 2, static_cast<std::uint16_t>(offsetof(Settings, meter_link)), nullptr, 9010, "rate", ""},
    {"rate_step_pct", "rate", "Léptetés (+/− gomb)", "Léptetés +/-", "%", "", ParamKind::Float, 1.0f, 50.0f, 1.0f, 5.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rate_step_pct)), nullptr, 9011, "rate", ""},
    {"rate_tol_pct", "rate", "Eltérés riasztás (elektromos hajtás)", "Eltérés riasztás", "%", "", ParamKind::Float, 2.0f, 50.0f, 1.0f, 10.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rate_tol_pct)), nullptr, 9012, "rate", ""},
    {"prefill_auto", "rate", "Automatikus feltöltés induláskor (sarkokban)", "Auto feltöltés", "", "Csak elektromos hajtásnál. Ha álló gépnél leengeded a gépet (munkakapcsoló) vagy elindítod a vetést, az adagolók a 'feltöltési sebességhez' tartozó fordulattal előre feltöltik a csöveket, majd az indulás után még az utánfutási ideig így forognak.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_prefill_auto, 2, static_cast<std::uint16_t>(offsetof(Settings, prefill_auto)), nullptr, 9013, "rate", ""},
    {"prefill_speed_kmh", "rate", "Feltöltési (elméleti) sebesség", "Feltöltési seb.", "km/h", "", ParamKind::Float, 1.0f, 20.0f, 0.1f, 8.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, prefill_speed_kmh)), nullptr, 9014, "rate", ""},
    {"prefill_time_s", "rate", "Feltöltési idő (álló gép)", "Feltöltési idő", "s", "", ParamKind::Float, 0.0f, 30.0f, 0.1f, 3.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, prefill_time_s)), nullptr, 9015, "rate", ""},
    {"prefill_after_s", "rate", "Utánfutás indulás után", "Utánfutás", "s", "", ParamKind::Float, 0.0f, 20.0f, 0.1f, 4.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, prefill_after_s)), nullptr, 9016, "rate", ""},
    {"drive_type", "drive", "Adagolók hajtása", "Hajtás", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_drive_type, 2, static_cast<std::uint16_t>(offsetof(Settings, drive_type)), nullptr, 9017, "drive", ""},
    {"clutch_mode", "drive", "Kuplung típusa", "Kuplung", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_clutch_mode, 2, static_cast<std::uint16_t>(offsetof(Settings, clutch_mode)), nullptr, 9018, "drive", ""},
    {"meter_ppr", "drive", "Adagolótengely-jeladó impulzus / fordulat (talajhajtás)", "Jeladó imp/ford", "imp", "", ParamKind::Int, 1.0f, 100.0f, 1.0f, 1.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, meter_ppr)), nullptr, 9019, "drive", ""},
    {"meter_timeout_s", "drive", "Adagoló nem forog riasztás késleltetése", "Nem forog késl.", "s", "Ha a vetés feltételei teljesülnek (vetés BE, gép lent, halad, adagoló bekapcsolva), és ennyi ideig nem jön impulzus az adagolótól, riasztás.", ParamKind::Float, 1.0f, 60.0f, 0.5f, 5.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, meter_timeout_s)), nullptr, 9020, "drive", ""},
    {"encoder_ppr", "drive", "Enkóder impulzus / adagolófordulat (motor)", "Enkóder imp/ford", "imp", "", ParamKind::Int, 1.0f, 5000.0f, 1.0f, 50.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, encoder_ppr)), nullptr, 9021, "drive", ""},
    {"motor_max_rpm", "drive", "Adagoló fordulat 100% PWM-nél", "Ford. 100% PWM", "1/min", "Az előrecsatoláshoz; a szabályzó ebből indul, utána az enkóder alapján korrigál.", ParamKind::Float, 1.0f, 600.0f, 0.5f, 60.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, motor_max_rpm)), nullptr, 9022, "drive", ""},
    {"pwm_min_pct", "drive", "PWM minimum (forgás közben)", "PWM minimum", "%", "", ParamKind::Float, 0.0f, 100.0f, 1.0f, 8.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pwm_min_pct)), nullptr, 9023, "drive", ""},
    {"pwm_max_pct", "drive", "PWM maximum", "PWM maximum", "%", "", ParamKind::Float, 1.0f, 100.0f, 1.0f, 100.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pwm_max_pct)), nullptr, 9024, "drive", ""},
    {"ramp_up_s", "drive", "Felfutás (0 → 100%)", "Felfutás", "s", "", ParamKind::Float, 0.0f, 10.0f, 0.1f, 1.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, ramp_up_s)), nullptr, 9025, "drive", ""},
    {"ramp_down_s", "drive", "Lefutás (100% → 0)", "Lefutás", "s", "", ParamKind::Float, 0.0f, 10.0f, 0.1f, 0.5f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, ramp_down_s)), nullptr, 9026, "drive", ""},
    {"reg_gain", "drive", "Szabályzási állandó (1 lassú – 10 gyors)", "Szab. állandó", "", "", ParamKind::Int, 1.0f, 10.0f, 1.0f, 5.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, reg_gain)), nullptr, 9027, "drive", ""},
    {"pwm_freq_hz", "drive", "PWM frekvencia", "PWM frekvencia", "Hz", "", ParamKind::Int, 100.0f, 20000.0f, 100.0f, 1000.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pwm_freq_hz)), nullptr, 0, nullptr, ""},
    {"fan_ppr", "fan", "Turbina impulzus / fordulat", "Imp/fordulat", "imp", "", ParamKind::Int, 1.0f, 100.0f, 1.0f, 1.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, fan_ppr)), nullptr, 9029, "fan", ""},
    {"fan_min_rpm", "fan", "Riasztás e fordulat alatt", "Min. fordulat", "1/min", "Csak akkor riaszt, ha a gép dolgozik: munkakapcsolóval lent van és halad; munkakapcsoló nélkül ha halad és a vetés BE.", ParamKind::Int, 0.0f, 20000.0f, 10.0f, 2500.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, fan_min_rpm)), nullptr, 9030, "fan", ""},
    {"fan_max_rpm", "fan", "Riasztás e fordulat felett (0 = nincs)", "Max. fordulat", "1/min", "", ParamKind::Int, 0.0f, 20000.0f, 10.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, fan_max_rpm)), nullptr, 9031, "fan", ""},
    {"fan_delay_s", "fan", "Riasztás késleltetése", "Késleltetés", "s", "", ParamKind::Float, 0.0f, 60.0f, 0.5f, 5.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, fan_delay_s)), nullptr, 9032, "fan", ""},
    {"speed_src", "speed", "Sebesség forrása", "Sebesség forrása", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_speed_src, 4, static_cast<std::uint16_t>(offsetof(Settings, speed_src)), nullptr, 9033, "speed", ""},
    {"isobus_speed_pref", "speed", "ISOBUS sebességüzenet", "ISOBUS üzenet", "", "Automatikus sorrend: gép által választott > föld (radar/GPS) > NMEA GPS > traktor kerék. Ha a terminál megosztja a GPS sebességét, az 'NMEA GPS' vagy 'Földsebesség' jön.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_isobus_speed_pref, 5, static_cast<std::uint16_t>(offsetof(Settings, isobus_speed_pref)), nullptr, 9034, "speed", ""},
    {"wheel_ppm", "speed", "Kerékjeladó impulzus / 100 m", "Imp/100 m", "imp", "", ParamKind::Float, 1.0f, 100000.0f, 0.1f, 100.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, wheel_ppm)), nullptr, 9035, "speed", ""},
    {"sim_speed", "speed", "Szimulált sebesség", "Szimulált seb.", "km/h", "", ParamKind::Float, 0.0f, 30.0f, 0.1f, 8.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, sim_speed)), nullptr, 9036, "speed", ""},
    {"min_work_speed", "speed", "Haladás küszöb (ez alatt áll a gép)", "Haladás küszöb", "km/h", "", ParamKind::Float, 0.1f, 10.0f, 0.1f, 1.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, min_work_speed)), nullptr, 9037, "speed", ""},
    {"speed_timeout_ms", "speed", "Sebességjel időtúllépés (ISOBUS)", "Jel időtúllépés", "ms", "", ParamKind::Int, 200.0f, 10000.0f, 100.0f, 1500.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, speed_timeout_ms)), nullptr, 0, nullptr, ""},
    {"work_sw", "switches", "Munkakapcsoló (gép lent)", "Munkakapcsoló", "", "Ha van, a 'gép lent' állapot a vetés (adagolók, terület) feltétele.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_work_sw, 3, static_cast<std::uint16_t>(offsetof(Settings, work_sw)), nullptr, 9039, "switches", ""},
    {"work_sw_tc", "switches", "Munkakapcsoló az automata szakaszolásnál is feltétel", "Munkakapcs. → TC", "", "Igen: TC-SEC automata szakaszolásnál is csak lent vet (a TC-nek kiemelt gépnél 'nem dolgozik' jelentést küldünk). Nem: automatában csak a TC dönt.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_work_sw_tc, 2, static_cast<std::uint16_t>(offsetof(Settings, work_sw_tc)), nullptr, 9040, "switches", ""},
    {"hopper1_nc", "switches", "1. tartály leürülésgátló kapcsoló", "1. tartály kapcs.", "", "NO: üres tartálynál zár. NC: üres tartálynál bont.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_hopper1_nc, 2, static_cast<std::uint16_t>(offsetof(Settings, hopper1_nc)), nullptr, 9041, "switches", ""},
    {"hopper2_nc", "switches", "2. tartály leürülésgátló kapcsoló", "2. tartály kapcs.", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_hopper2_nc, 2, static_cast<std::uint16_t>(offsetof(Settings, hopper2_nc)), nullptr, 9042, "switches", ""},
    {"hopper_delay_s", "switches", "Leürülés jelzés szűrése", "Tartály szűrés", "s", "", ParamKind::Float, 0.0f, 30.0f, 0.5f, 2.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, hopper_delay_s)), nullptr, 9043, "switches", ""},
    {"out_invert", "switches", "Kimenetek fordított szintje (relémodul)", "Kimenet fordítva", "", "Igen: alacsony szint kapcsolja be a relét (kuplung, lámpa, szelep, hűtő).", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_out_invert, 2, static_cast<std::uint16_t>(offsetof(Settings, out_invert)), nullptr, 9044, "switches", ""},
    {"lights_enable", "extras", "Munkalámpák (2 db)", "Munkalámpák", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_lights_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, lights_enable)), nullptr, 9045, "extras", ""},
    {"valve_enable", "extras", "Nyitás / nyomjelző váltószelep", "Nyitás/nyomjelző", "", "Alapban a hidraulikakör a gépet nyitja/csukja; a szelep bekapcsolva a nyomjelzőre tereli.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_valve_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, valve_enable)), nullptr, 9046, "extras", ""},
    {"pressure_enable", "extras", "Nyomásmérés (lenyomatás, ADS1115)", "Nyomásmérés", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_pressure_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, pressure_enable)), nullptr, 9047, "extras", ""},
    {"temp_enable", "extras", "Olajhőmérséklet + hűtőventilátor (2× DS18B20)", "Olajhőmérséklet", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_temp_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, temp_enable)), nullptr, 9048, "extras", ""},
    {"scale_enable", "extras", "Leforgatási mérleg (HX711)", "Mérleg", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_scale_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, scale_enable)), nullptr, 9049, "extras", ""},
    {"pressure_range_bar", "pressure", "Szenzor méréshatár", "Méréshatár", "bar", "", ParamKind::Float, 1.0f, 1000.0f, 1.0f, 250.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pressure_range_bar)), nullptr, 9050, "pressure", "pressure_enable"},
    {"pressure_signal", "pressure", "Szenzor kimenőjele", "Kimenőjel", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_pressure_signal, 4, static_cast<std::uint16_t>(offsetof(Settings, pressure_signal)), nullptr, 9051, "pressure", "pressure_enable"},
    {"pressure_cal", "pressure", "Kalibrációs szorzó", "Kalibr. szorzó", "", "Az 'Ismert nyomás' kalibrálás állítja be (1,000 = gyári jelleggörbe).", ParamKind::Float, 0.5f, 2.0f, 0.001f, 1.0f, 3, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pressure_cal)), nullptr, 9052, "pressure", "pressure_enable"},
    {"pressure_zero_bar", "pressure", "Nullpont-eltolás", "Nullpont", "bar", "", ParamKind::Float, -50.0f, 50.0f, 0.1f, 0.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pressure_zero_bar)), nullptr, 9053, "pressure", "pressure_enable"},
    {"pressure_ch", "pressure", "ADS1115 csatorna", "ADS csatorna", "", "", ParamKind::Int, 0.0f, 3.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pressure_ch)), nullptr, 0, nullptr, "pressure_enable"},
    {"pressure_divider", "pressure", "Bemeneti feszültségosztó (V szenzor / V ADS)", "", "", "Ha az ADS1115 bemenete előtt osztó van (pl. 3,3 V-os táp esetén), add meg az arányát.", ParamKind::Float, 1.0f, 10.0f, 0.001f, 1.0f, 3, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pressure_divider)), nullptr, 0, nullptr, "pressure_enable"},
    {"cooler_on_c", "cooling", "Hűtőventilátor bekapcsol", "Bekapcsol", "°C", "", ParamKind::Float, 20.0f, 120.0f, 0.5f, 60.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, cooler_on_c)), nullptr, 9056, "cooling", "temp_enable"},
    {"cooler_off_c", "cooling", "Hűtőventilátor kikapcsol", "Kikapcsol", "°C", "", ParamKind::Float, 10.0f, 115.0f, 0.5f, 50.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, cooler_off_c)), nullptr, 9057, "cooling", "temp_enable"},
    {"cooler_src", "cooling", "Szabályozó hőmérő", "Szabályozó hőmérő", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_cooler_src, 3, static_cast<std::uint16_t>(offsetof(Settings, cooler_src)), nullptr, 9058, "cooling", "temp_enable"},
    {"temp_alarm_c", "cooling", "Túlmelegedés riasztás (0 = nincs)", "Riasztás felett", "°C", "", ParamKind::Float, 0.0f, 130.0f, 0.5f, 85.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, temp_alarm_c)), nullptr, 9059, "cooling", "temp_enable"},
    {"temp_swap", "cooling", "A két DS18B20 hőmérő felcserélése", "", "", "A hőmérőket a firmware a ROM-azonosítójuk sorrendjében számozza. Ha az olajtartály és a hűtő kimenet értéke fordítva jelenik meg, kapcsold be.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_temp_swap, 2, static_cast<std::uint16_t>(offsetof(Settings, temp_swap)), nullptr, 0, nullptr, "temp_enable"},
    {"scale_capacity_kg", "scale", "Mérlegcella méréshatára", "Méréshatár", "kg", "", ParamKind::Float, 1.0f, 500.0f, 0.5f, 50.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, scale_capacity_kg)), nullptr, 9061, "scale", "scale_enable"},
    {"scale_factor", "scale", "Mérleg kalibrációs tényező (beütés / kg)", "Tényező", "1/kg", "A 'Kalibrálás ismert súllyal' állítja be.", ParamKind::Float, 1.0f, 10000000.0f, 0.1f, 85000.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, scale_factor)), nullptr, 9062, "scale", "scale_enable"},
    {"scale_offset", "scale", "Mérleg nullpont (tára, nyers)", "", "", "", ParamKind::Float, -20000000.0f, 20000000.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, scale_offset)), nullptr, 0, nullptr, "scale_enable"},
    {"tc_enable", "tc", "Task Controller kliens", "TC kliens", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_tc_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, tc_enable)), nullptr, 9064, "tc", ""},
    {"tc_sec", "tc", "Automatikus szakaszvezérlés (TC-SEC)", "TC-SEC", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_tc_sec, 2, static_cast<std::uint16_t>(offsetof(Settings, tc_sec)), nullptr, 9065, "tc", ""},
    {"tc_geo", "tc", "Kijuttatási térkép (TC-GEO, kg/ha az adagolóknak)", "TC-GEO", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_tc_geo, 2, static_cast<std::uint16_t>(offsetof(Settings, tc_geo)), nullptr, 9066, "tc", ""},
    {"hitch_type", "tc", "Csatlakozás", "Csatlakozás", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 2.0f, 0, OPT_hitch_type, 5, static_cast<std::uint16_t>(offsetof(Settings, hitch_type)), nullptr, 9067, "geometry", ""},
    {"geo_a_m", "tc", "A: vonópont → csuklópont (kormányzott vonórúd)", "A vonópont-csukló", "m", "", ParamKind::Float, 0.0f, 20.0f, 0.01f, 0.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, geo_a_m)), nullptr, 9068, "geometry", ""},
    {"geo_b_m", "tc", "B: csuklópont → csoroszlyák (függesztett: vonópont → csoroszlyák)", "B csukló-csorosz.", "m", "", ParamKind::Float, 0.0f, 20.0f, 0.01f, 4.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, geo_b_m)), nullptr, 9069, "geometry", ""},
    {"geo_c_m", "tc", "C: csuklópont → kerékagy (fordulópont)", "C csukló-tengely", "m", "", ParamKind::Float, 0.0f, 20.0f, 0.01f, 3.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, geo_c_m)), nullptr, 9070, "geometry", ""},
    {"geo_y_m", "tc", "D: oldalirányú eltolás (+ jobbra)", "D oldaleltolás", "m", "", ParamKind::Float, -5.0f, 5.0f, 0.01f, 0.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, geo_y_m)), nullptr, 9071, "geometry", ""},
    {"delay_on_ms", "tc", "Tehetetlenség BE", "Tehetetlenség BE", "ms", "", ParamKind::Int, 0.0f, 5000.0f, 10.0f, 800.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, delay_on_ms)), nullptr, 9072, "tc", ""},
    {"delay_off_ms", "tc", "Tehetetlenség KI", "Tehetetlenség KI", "ms", "", ParamKind::Int, 0.0f, 5000.0f, 10.0f, 400.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, delay_off_ms)), nullptr, 9073, "tc", ""},
    {"sim_inputs", "sim", "Bemenetek szimulációja (turbina, adagolók, munkakapcsoló)", "Bemenetek szim.", "", "Próbapadi teszthez: a turbina, az adagolók és az enkóderek impulzusait a firmware állítja elő, a munkakapcsoló 'lent' állapotú.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_sim_inputs, 2, static_cast<std::uint16_t>(offsetof(Settings, sim_inputs)), nullptr, 9074, "diag", ""},
    {"sim_fan_rpm", "sim", "Szimulált turbinafordulat", "Szim. turbina", "1/min", "", ParamKind::Int, 0.0f, 10000.0f, 10.0f, 3500.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, sim_fan_rpm)), nullptr, 9075, "diag", ""},
    {"sim_wheel", "sim", "Kerékimpulzus szimuláció a szimulált sebességből", "Kerék szimuláció", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_sim_wheel, 2, static_cast<std::uint16_t>(offsetof(Settings, sim_wheel)), nullptr, 9076, "diag", ""},
    {"wifi_pass", "system", "Wi-Fi jelszó (8–32 karakter)", "", "", "", ParamKind::Str, 8.0f, 32.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, wifi_pass)), "12345678", 0, nullptr, ""},
    {"ecu_instance", "system", "ECU példány (több gép egy buszon)", "", "", "", ParamKind::Int, 0.0f, 7.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, ecu_instance)), nullptr, 0, nullptr, ""},
    {"realarm_s", "system", "Nyugtázott, fennálló riasztás újra (0 = soha)", "", "s", "", ParamKind::Int, 0.0f, 3600.0f, 10.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, realarm_s)), nullptr, 0, nullptr, ""},
};
#endif
} // namespace seeder

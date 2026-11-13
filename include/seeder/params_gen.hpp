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
    std::int32_t tank_fill;
    std::int32_t meter_layout;
    float sec_w1;
    float rate1;
    float rate2;
    float cal1_kg_rev;
    float cal2_kg_rev;
    float rate_step_pct;
    float rate_tol_pct;
    float crank_turns_10;
    std::int32_t crank_autostop;
    std::int32_t prefill_auto;
    float prefill_speed_kmh;
    float prefill_time_s;
    float prefill_after_s;
    std::int32_t drive_type;
    std::int32_t motor_layout;
    std::int32_t motor_driver;
    std::int32_t clutch_mode;
    std::int32_t clutch_lift;
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
    std::int32_t motor_invert;
    std::int32_t unjam_enable;
    float unjam_deg;
    std::int32_t unjam_tries;
    float jam_detect_s;
    std::int32_t keya_node1;
    std::int32_t keya_node2;
    std::int32_t keya_unit;
    float keya_gear;
    std::int32_t keya_hb_ms;
    float motor_deadband_pct;
    std::int32_t fan_ppr;
    std::int32_t fan_min_rpm;
    std::int32_t fan_max_rpm;
    float fan_delay_s;
    std::int32_t speed_src;
    std::int32_t isobus_speed_pref;
    float wheel_ppm;
    float sim_speed;
    float min_work_speed;
    float slip_alarm_pct;
    float slip_delay_s;
    std::int32_t speed_timeout_ms;
    std::int32_t work_sw;
    std::int32_t work_sw_tc;
    std::int32_t hopper1_nc;
    std::int32_t hopper2_nc;
    float hopper_delay_s;
    std::int32_t crank_btn;
    std::int32_t out_invert;
    std::int32_t lights_enable;
    std::int32_t lamp1_relay;
    std::int32_t lamp2_relay;
    std::int32_t crank_lamp;
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
    std::int32_t rev_enable;
    std::int32_t rev_interval_min;
    float rev_time_s;
    float rev_pause_s;
    std::int32_t temp_swap;
    float scale_capacity_kg;
    float scale_factor;
    float scale_offset;
    std::int32_t sec_src;
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
    std::int32_t aog_enable;
    std::int32_t aog_ip_mode;
    char aog_ip[16];
    std::int32_t aog_paint;
    std::int32_t aog_paint_mode;
    std::int32_t sim_inputs;
    std::int32_t sim_fan_rpm;
    std::int32_t sim_wheel;
    char wifi_pass[33];
    std::int32_t wifi_timeout_min;
    std::int32_t ecu_instance;
    std::int32_t isobus_delay_s;
    std::int32_t isobus_retry_s;
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
    const char *scope;
};

struct GroupDef { const char *id; const char *label; const char *icon; };

enum ParamId : std::uint16_t {
    P_width_m = 0,
    P_section_count = 1,
    P_meter_count = 2,
    P_tank_count = 3,
    P_tank_fill = 4,
    P_meter_layout = 5,
    P_sec_w1 = 6,
    P_rate1 = 7,
    P_rate2 = 8,
    P_cal1_kg_rev = 9,
    P_cal2_kg_rev = 10,
    P_rate_step_pct = 11,
    P_rate_tol_pct = 12,
    P_crank_turns_10 = 13,
    P_crank_autostop = 14,
    P_prefill_auto = 15,
    P_prefill_speed_kmh = 16,
    P_prefill_time_s = 17,
    P_prefill_after_s = 18,
    P_drive_type = 19,
    P_motor_layout = 20,
    P_motor_driver = 21,
    P_clutch_mode = 22,
    P_clutch_lift = 23,
    P_meter_ppr = 24,
    P_meter_timeout_s = 25,
    P_encoder_ppr = 26,
    P_motor_max_rpm = 27,
    P_pwm_min_pct = 28,
    P_pwm_max_pct = 29,
    P_ramp_up_s = 30,
    P_ramp_down_s = 31,
    P_reg_gain = 32,
    P_pwm_freq_hz = 33,
    P_motor_invert = 34,
    P_unjam_enable = 35,
    P_unjam_deg = 36,
    P_unjam_tries = 37,
    P_jam_detect_s = 38,
    P_keya_node1 = 39,
    P_keya_node2 = 40,
    P_keya_unit = 41,
    P_keya_gear = 42,
    P_keya_hb_ms = 43,
    P_motor_deadband_pct = 44,
    P_fan_ppr = 45,
    P_fan_min_rpm = 46,
    P_fan_max_rpm = 47,
    P_fan_delay_s = 48,
    P_speed_src = 49,
    P_isobus_speed_pref = 50,
    P_wheel_ppm = 51,
    P_sim_speed = 52,
    P_min_work_speed = 53,
    P_slip_alarm_pct = 54,
    P_slip_delay_s = 55,
    P_speed_timeout_ms = 56,
    P_work_sw = 57,
    P_work_sw_tc = 58,
    P_hopper1_nc = 59,
    P_hopper2_nc = 60,
    P_hopper_delay_s = 61,
    P_crank_btn = 62,
    P_out_invert = 63,
    P_lights_enable = 64,
    P_lamp1_relay = 65,
    P_lamp2_relay = 66,
    P_crank_lamp = 67,
    P_valve_enable = 68,
    P_pressure_enable = 69,
    P_temp_enable = 70,
    P_scale_enable = 71,
    P_pressure_range_bar = 72,
    P_pressure_signal = 73,
    P_pressure_cal = 74,
    P_pressure_zero_bar = 75,
    P_pressure_ch = 76,
    P_pressure_divider = 77,
    P_cooler_on_c = 78,
    P_cooler_off_c = 79,
    P_cooler_src = 80,
    P_temp_alarm_c = 81,
    P_rev_enable = 82,
    P_rev_interval_min = 83,
    P_rev_time_s = 84,
    P_rev_pause_s = 85,
    P_temp_swap = 86,
    P_scale_capacity_kg = 87,
    P_scale_factor = 88,
    P_scale_offset = 89,
    P_sec_src = 90,
    P_tc_enable = 91,
    P_tc_sec = 92,
    P_tc_geo = 93,
    P_hitch_type = 94,
    P_geo_a_m = 95,
    P_geo_b_m = 96,
    P_geo_c_m = 97,
    P_geo_y_m = 98,
    P_delay_on_ms = 99,
    P_delay_off_ms = 100,
    P_aog_enable = 101,
    P_aog_ip_mode = 102,
    P_aog_ip = 103,
    P_aog_paint = 104,
    P_aog_paint_mode = 105,
    P_sim_inputs = 106,
    P_sim_fan_rpm = 107,
    P_sim_wheel = 108,
    P_wifi_pass = 109,
    P_wifi_timeout_min = 110,
    P_ecu_instance = 111,
    P_isobus_delay_s = 112,
    P_isobus_retry_s = 113,
    P_realarm_s = 114,
    PARAM_COUNT = 115
};

constexpr std::size_t GROUP_COUNT = 14;
constexpr std::uint16_t VT_INPUT_BASE = 9000;
extern const ParamDef PARAMS[PARAM_COUNT];
extern const GroupDef GROUPS[GROUP_COUNT];
constexpr const char *VT_GROUPS_JSON = "[{\"id\":\"rate\",\"label\":\"Adagolás\",\"pages\":[{\"id\":\"rate\",\"title\":\"ADAGOLÁS, FELTÖLTÉS\"}]},{\"id\":\"machine\",\"label\":\"Gép, geometria\",\"pages\":[{\"id\":\"machine\",\"title\":\"GÉP\"},{\"id\":\"geometry\",\"title\":\"GEOMETRIA\"}]},{\"id\":\"drive\",\"label\":\"Hajtás, motor\",\"pages\":[{\"id\":\"drive\",\"title\":\"HAJTÁS\"},{\"id\":\"motor\",\"title\":\"MOTORSZABÁLYZÁS\"},{\"id\":\"jam\",\"title\":\"BESZORULÁS\"},{\"id\":\"keya\",\"title\":\"KEYA CAN-MOTOR\"}]},{\"id\":\"fan\",\"label\":\"Turbina, hűtés\",\"pages\":[{\"id\":\"fan\",\"title\":\"TURBINA\"},{\"id\":\"cooling\",\"title\":\"OLAJHŐMÉRSÉKLET\"}]},{\"id\":\"measure\",\"label\":\"Mérés\",\"pages\":[{\"id\":\"speed\",\"title\":\"SEBESSÉG\"},{\"id\":\"scale\",\"title\":\"MÉRLEG\"},{\"id\":\"pressure\",\"title\":\"NYOMÁSMÉRÉS\"}]},{\"id\":\"switches\",\"label\":\"Kapcsolók\",\"pages\":[{\"id\":\"switches\",\"title\":\"KAPCSOLÓK\"},{\"id\":\"extras\",\"title\":\"KIEGÉSZÍTŐK\"}]},{\"id\":\"tc\",\"label\":\"Szakaszolás, TC\",\"pages\":[{\"id\":\"tc\",\"title\":\"SZAKASZOLÁS, TC\"},{\"id\":\"aog\",\"title\":\"AGOPENGPS\"}]},{\"id\":\"service\",\"label\":\"Szerviz\",\"pages\":[{\"id\":\"diag\",\"title\":\"SZIMULÁCIÓ / TESZT\"}]}]";

#ifdef SEEDER_PARAMS_IMPLEMENTATION
static const char *const OPT_tank_fill[] = {"Kikapcsolva", "Közös mennyiség", "Tartályonként"};
static const char *const OPT_meter_layout[] = {"Bal / jobb félszélesség", "Első / hátsó sor (teljes szélesség)"};
static const char *const OPT_crank_autostop[] = {"Nem", "Igen"};
static const char *const OPT_prefill_auto[] = {"Nem", "Igen"};
static const char *const OPT_drive_type[] = {"Talajhajtás + elektromágneses kuplung", "Elektromos – PWM motorvezérlő", "Elektromos – Keya CAN-motor"};
static const char *const OPT_motor_layout[] = {"Adagolónként saját motor", "Egy motor hajtja mindkét adagolót (kuplungokkal)"};
static const char *const OPT_motor_driver[] = {"Csak PWM (egy irány)", "PWM + DIR (Cytron, Pololu)", "PWM + PWM (IBT-2)"};
static const char *const OPT_clutch_mode[] = {"Áram alatt hajt", "Áram alatt old"};
static const char *const OPT_clutch_lift[] = {"Old (kiemeléskor lekapcsol)", "Nem old (csak szakasz / gomb)"};
static const char *const OPT_motor_invert[] = {"Nem", "Igen"};
static const char *const OPT_unjam_enable[] = {"Nem", "Igen"};
static const char *const OPT_keya_unit[] = {"Hajtómű kimenete (adagoló)", "Motortengely (× áttétel)"};
static const char *const OPT_speed_src[] = {"Kerékjeladó", "ISOBUS (GPS/radar/traktor)", "ISOBUS, kiesésnél kerék", "Szimulált", "AgOpenGPS (GPS)", "AgOpenGPS, kiesésnél kerék"};
static const char *const OPT_isobus_speed_pref[] = {"Automatikus", "Földsebesség (radar/GPS)", "Traktor kerék", "Gép által választott", "NMEA GPS (SOG)"};
static const char *const OPT_work_sw[] = {"Nincs", "Van – NO (lent zár)", "Van – NC (lent bont)"};
static const char *const OPT_work_sw_tc[] = {"Nem", "Igen"};
static const char *const OPT_hopper1_nc[] = {"NO (záró)", "NC (bontó)"};
static const char *const OPT_hopper2_nc[] = {"NO (záró)", "NC (bontó)"};
static const char *const OPT_crank_btn[] = {"Nem", "Igen"};
static const char *const OPT_out_invert[] = {"Nem", "Igen"};
static const char *const OPT_lights_enable[] = {"Nem", "Igen"};
static const char *const OPT_lamp1_relay[] = {"Relé 1 (kuplung 1)", "Relé 2 (kuplung 2)", "Relé 3 (szelep)", "Relé 4", "Relé 5", "Relé 6 (hűtő)", "Relé 7 (visszafúvás)", "Relé 8 (szabad)"};
static const char *const OPT_lamp2_relay[] = {"Relé 1 (kuplung 1)", "Relé 2 (kuplung 2)", "Relé 3 (szelep)", "Relé 4", "Relé 5", "Relé 6 (hűtő)", "Relé 7 (visszafúvás)", "Relé 8 (szabad)"};
static const char *const OPT_crank_lamp[] = {"Nem", "Igen"};
static const char *const OPT_valve_enable[] = {"Nem", "Igen"};
static const char *const OPT_pressure_enable[] = {"Nem", "Igen"};
static const char *const OPT_temp_enable[] = {"Nem", "Igen"};
static const char *const OPT_scale_enable[] = {"Nem", "Igen"};
static const char *const OPT_pressure_signal[] = {"0–5 V", "0,5–4,5 V", "0,5–5 V", "0–4,5 V"};
static const char *const OPT_cooler_src[] = {"Olajtartály", "Hűtő kimenet", "A melegebb"};
static const char *const OPT_rev_enable[] = {"Nem", "Igen"};
static const char *const OPT_temp_swap[] = {"Nem", "Igen"};
static const char *const OPT_sec_src[] = {"Nincs (csak kézi)", "ISOBUS Task Controller", "AgOpenGPS"};
static const char *const OPT_tc_enable[] = {"Nem", "Igen"};
static const char *const OPT_tc_sec[] = {"Nem", "Igen"};
static const char *const OPT_tc_geo[] = {"Nem", "Igen"};
static const char *const OPT_hitch_type[] = {"Függesztett", "Kormányzott vonórúd", "Vonórúd (vonószem)", "Vonóhorog", "Gömbfej (K80)"};
static const char *const OPT_aog_enable[] = {"Nem", "Igen"};
static const char *const OPT_aog_ip_mode[] = {"Fix IP", "DHCP"};
static const char *const OPT_aog_paint[] = {"Nem", "Igen"};
static const char *const OPT_aog_paint_mode[] = {"Automata / zöld", "Kézi / sárga"};
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
    {"tc", "Szakaszolás, Task Controller és geometria", "tc"},
    {"aog", "AgOpenGPS (Ethernet / UDP)", "aog"},
    {"sim", "Szimuláció / teszt", "sim"},
    {"system", "Rendszer", "system"},
};

const ParamDef PARAMS[PARAM_COUNT] = {
    {"width_m", "machine", "Munkaszélesség", "Munkaszélesség", "m", "", ParamKind::Float, 1.0f, 20.0f, 0.01f, 6.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, width_m)), nullptr, 9000, "machine", "", ""},
    {"section_count", "machine", "Szakaszok száma", "Szakaszok száma", "db", "2 szakasz: az 1. adagoló a bal, a 2. a jobb félszélességet veti. 1 szakasz: a Task Controller a teljes szélességet egyben kapcsolja (pl. első/hátsó soros gépnél mindkét adagolót együtt), a két adagoló gombja a kezelőnek továbbra is külön működik.", ParamKind::Int, 1.0f, 2.0f, 1.0f, 2.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, section_count)), nullptr, 9001, "machine", "", ""},
    {"meter_count", "machine", "Adagolók száma", "Adagolók száma", "db", "", ParamKind::Int, 1.0f, 2.0f, 1.0f, 2.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, meter_count)), nullptr, 9002, "machine", "", ""},
    {"tank_count", "machine", "Tartályok (rekeszek) száma", "Tartályok száma", "db", "", ParamKind::Int, 1.0f, 2.0f, 1.0f, 2.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, tank_count)), nullptr, 9003, "machine", "", ""},
    {"tank_fill", "machine", "Tartálytöltet követése", "Töltet követés", "", "Tájékoztató: a megadott tankolt mennyiségből levonja a kijuttatott vetőmagot, és a főképernyő tartályán mutatja a maradékot (nem riaszt). Közös: két tartálynál is egy mennyiség; Tartályonként: rekeszenként külön. A tankolás a Beállítások → Tartálytöltet oldalon (VT) vagy a weben adható meg.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_tank_fill, 3, static_cast<std::uint16_t>(offsetof(Settings, tank_fill)), nullptr, 9004, "machine", "", ""},
    {"meter_layout", "machine", "Két adagoló elrendezése", "Adagolók", "", "Első/hátsó soros gépnél mindkét adagoló a teljes szélességet veti: a terület mindig a teljes szélességgel számolódik, és csak 1 szakasz lehet.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_meter_layout, 2, static_cast<std::uint16_t>(offsetof(Settings, meter_layout)), nullptr, 9005, "machine", "", ""},
    {"sec_w1", "machine", "1. (bal) szakasz szélessége", "1. szakasz", "m", "2 szakasznál. A 2. szakasz = munkaszélesség − 1. szakasz.", ParamKind::Float, 0.1f, 20.0f, 0.01f, 3.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, sec_w1)), nullptr, 9006, "machine", "", ""},
    {"rate1", "rate", "1. adagoló előírt mennyiség", "Előírt 1", "kg/ha", "", ParamKind::Float, 0.5f, 1000.0f, 0.5f, 200.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rate1)), nullptr, 9007, "rate", "", ""},
    {"rate2", "rate", "2. adagoló előírt mennyiség", "Előírt 2", "kg/ha", "", ParamKind::Float, 0.5f, 1000.0f, 0.5f, 200.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rate2)), nullptr, 9008, "rate", "", ""},
    {"cal1_kg_rev", "rate", "1. adagoló kg / adagolófordulat (leforgatásból)", "Kg/ford. 1", "kg", "A leforgatás eredménye. Elektromos hajtásnál ebből számolja a szükséges fordulatot, talajhajtásnál a kijuttatott mennyiséget.", ParamKind::Float, 0.0001f, 20.0f, 0.0001f, 0.2353f, 4, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, cal1_kg_rev)), nullptr, 9009, "rate", "", ""},
    {"cal2_kg_rev", "rate", "2. adagoló kg / adagolófordulat (leforgatásból)", "Kg/ford. 2", "kg", "", ParamKind::Float, 0.0001f, 20.0f, 0.0001f, 0.2353f, 4, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, cal2_kg_rev)), nullptr, 9010, "rate", "", ""},
    {"rate_step_pct", "rate", "Léptetés (+/− gomb)", "Léptetés +/-", "%", "", ParamKind::Float, 1.0f, 50.0f, 1.0f, 5.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rate_step_pct)), nullptr, 9011, "rate", "", ""},
    {"rate_tol_pct", "rate", "Eltérés riasztás (elektromos hajtás)", "Eltérés riasztás", "%", "", ParamKind::Float, 2.0f, 50.0f, 1.0f, 10.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rate_tol_pct)), nullptr, 9012, "rate", "", ""},
    {"crank_turns_10", "rate", "Leforgatás: adagoló-fordulat 1/10 ha-ra", "Ford. 1/10 ha", "ford", "Gépállandó: ennyi adagolófordulat felel meg 1/10 hektárnak (Accord: 85). Ebből számolódik a leforgatás 1/10, 1/20 és 1/50 ha-os célja, és egymotoros elektromos hajtásnál a motor fordulata.", ParamKind::Float, 5.0f, 2000.0f, 0.5f, 85.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, crank_turns_10)), nullptr, 9013, "rate", "", ""},
    {"crank_autostop", "rate", "Leforgatás megáll a célfordulatnál", "Leforg. megáll", "", "Elektromos hajtásnál a motor a célfordulatnál (85 / 42,5 / 17) magától megáll, akkor is, ha a gombot még nyomod. 'Nem': amíg nyomod, forog.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_crank_autostop, 2, static_cast<std::uint16_t>(offsetof(Settings, crank_autostop)), nullptr, 9014, "rate", "", ""},
    {"prefill_auto", "rate", "Automatikus feltöltés induláskor (sarkokban)", "Auto feltöltés", "", "Csak elektromos hajtásnál. Ha álló gépnél leengeded a gépet (munkakapcsoló) vagy elindítod a vetést, az adagolók a 'feltöltési sebességhez' tartozó fordulattal előre feltöltik a csöveket, majd az indulás után még az utánfutási ideig így forognak.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_prefill_auto, 2, static_cast<std::uint16_t>(offsetof(Settings, prefill_auto)), nullptr, 9015, "rate", "", ""},
    {"prefill_speed_kmh", "rate", "Feltöltési (elméleti) sebesség", "Feltöltési seb.", "km/h", "", ParamKind::Float, 1.0f, 20.0f, 0.1f, 8.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, prefill_speed_kmh)), nullptr, 9016, "rate", "", ""},
    {"prefill_time_s", "rate", "Feltöltési idő (álló gép)", "Feltöltési idő", "s", "", ParamKind::Float, 0.0f, 30.0f, 0.1f, 3.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, prefill_time_s)), nullptr, 9017, "rate", "", ""},
    {"prefill_after_s", "rate", "Utánfutás indulás után", "Utánfutás", "s", "", ParamKind::Float, 0.0f, 20.0f, 0.1f, 4.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, prefill_after_s)), nullptr, 9018, "rate", "", ""},
    {"drive_type", "drive", "Adagolók hajtása", "Hajtás", "", "Talajhajtás: a talajkerék hajtja az adagolókat, a vezérlő a kuplungokat kapcsolja. Elektromos – PWM: motorvezérlő (Cytron, Pololu, IBT-2 …) a panel PWM kimenetein, enkóderrel. Elektromos – Keya: saját fordulatszabályzású CAN-motor az ISOBUS-szal közös buszon (beállítás: Keya CAN-motor oldal).", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_drive_type, 3, static_cast<std::uint16_t>(offsetof(Settings, drive_type)), nullptr, 9019, "drive", "", ""},
    {"motor_layout", "drive", "Motorok (elektromos hajtás)", "Motorok", "", "Egy motornál a motor a talajkereket helyettesíti: a haladással arányosan forog (fordulat 1/10 ha-ra alapján), a kg/ha-t adagolónként a skála adja, a kuplungok adagolónként kapcsolnak (mint talajhajtásnál). Az 1. motor kimenetét használja, a fordulatot az 1. adagoló enkódere (IN3) méri.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_motor_layout, 2, static_cast<std::uint16_t>(offsetof(Settings, motor_layout)), nullptr, 9020, "drive", "", ""},
    {"motor_driver", "drive", "PWM motorvezérlő bekötése", "Motorvezérlő", "", "Csak PWM: GPIO47 / GPIO48 a PWM, a motor csak előre forog. PWM + DIR: GPIO47 / 48 a PWM, GPIO21 / 45 az irány (DIR). PWM + PWM (IBT-2): előre GPIO47 / 48 (RPWM), hátra GPIO21 / 45 (LPWM), R_EN és L_EN 5 V-on. Az 1. motor 47 + 21, a 2. motor 48 + 45. A visszafordításhoz (beszorulás) irányváltós bekötés kell. Csak 'Elektromos – PWM motorvezérlő' hajtásnál számít.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_motor_driver, 3, static_cast<std::uint16_t>(offsetof(Settings, motor_driver)), nullptr, 9021, "drive", "", ""},
    {"clutch_mode", "drive", "Kuplung típusa", "Kuplung", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_clutch_mode, 2, static_cast<std::uint16_t>(offsetof(Settings, clutch_mode)), nullptr, 9022, "drive", "", ""},
    {"clutch_lift", "drive", "Kuplung kiemeléskor / megálláskor (talajhajtás)", "Kuplung kiemel.", "", "Talajhajtásnál kiemelt vagy álló gépnél az adagoló úgysem forog, ezért a kuplungot alapból csak a szakaszolás és az adagológombok kapcsolják. 'Old': a régi működés, kiemeléskor is lekapcsol.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_clutch_lift, 2, static_cast<std::uint16_t>(offsetof(Settings, clutch_lift)), nullptr, 9023, "drive", "", ""},
    {"meter_ppr", "drive", "Adagolótengely-jeladó impulzus / fordulat (talajhajtás)", "Jeladó imp/ford", "imp", "", ParamKind::Int, 1.0f, 100.0f, 1.0f, 1.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, meter_ppr)), nullptr, 9024, "drive", "", ""},
    {"meter_timeout_s", "drive", "Adagoló nem forog riasztás késleltetése", "Nem forog késl.", "s", "Ha a vetés feltételei teljesülnek (vetés BE, gép lent, halad, adagoló bekapcsolva), és ennyi ideig nem jön impulzus az adagolótól, riasztás.", ParamKind::Float, 1.0f, 60.0f, 0.5f, 5.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, meter_timeout_s)), nullptr, 9025, "drive", "", ""},
    {"encoder_ppr", "drive", "Enkóder impulzus / adagolófordulat (motor)", "Enkóder imp/ford", "imp", "", ParamKind::Int, 1.0f, 5000.0f, 1.0f, 50.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, encoder_ppr)), nullptr, 9026, "drive", "", ""},
    {"motor_max_rpm", "drive", "Adagoló fordulat 100% PWM-nél", "Ford. 100% PWM", "1/min", "Az előrecsatoláshoz; a szabályzó ebből indul, utána az enkóder alapján korrigál.", ParamKind::Float, 1.0f, 600.0f, 0.5f, 60.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, motor_max_rpm)), nullptr, 9027, "motor", "", ""},
    {"pwm_min_pct", "drive", "PWM minimum (forgás közben)", "PWM minimum", "%", "", ParamKind::Float, 0.0f, 100.0f, 1.0f, 8.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pwm_min_pct)), nullptr, 9028, "motor", "", ""},
    {"pwm_max_pct", "drive", "PWM maximum", "PWM maximum", "%", "", ParamKind::Float, 1.0f, 100.0f, 1.0f, 100.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pwm_max_pct)), nullptr, 9029, "motor", "", ""},
    {"ramp_up_s", "drive", "Felfutás (0 → 100%)", "Felfutás", "s", "", ParamKind::Float, 0.0f, 10.0f, 0.1f, 1.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, ramp_up_s)), nullptr, 9030, "motor", "", ""},
    {"ramp_down_s", "drive", "Lefutás (100% → 0)", "Lefutás", "s", "", ParamKind::Float, 0.0f, 10.0f, 0.1f, 0.5f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, ramp_down_s)), nullptr, 9031, "motor", "", ""},
    {"reg_gain", "drive", "Szabályzási állandó (1 lassú – 10 gyors)", "Szab. állandó", "", "", ParamKind::Int, 1.0f, 10.0f, 1.0f, 5.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, reg_gain)), nullptr, 9032, "motor", "", ""},
    {"pwm_freq_hz", "drive", "PWM frekvencia", "PWM frekvencia", "Hz", "Cytron / Pololu / IBT-2: 16000–20000 Hz csendes (nem sípol). Alacsonyabb frekvencián a motor jobban indul kis PWM-nél, de hallhatóan sípol.", ParamKind::Int, 100.0f, 20000.0f, 100.0f, 1000.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pwm_freq_hz)), nullptr, 9033, "motor", "", ""},
    {"motor_invert", "drive", "Forgásirány megfordítása", "Irány fordítva", "", "Ha a motor 'előre' parancsra visszafelé forog (irányváltós bekötésnél).", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_motor_invert, 2, static_cast<std::uint16_t>(offsetof(Settings, motor_invert)), nullptr, 9034, "motor", "", ""},
    {"unjam_enable", "drive", "Beszorulásnál visszafordítás", "Visszafordítás", "", "Ha az adagoló beszorul (hajtott motor, de nem jön enkóderjel), a motor megáll, a beállított szöggel visszafordul, majd újra előre indul. Csak irányváltós bekötésnél (PWM + DIR vagy IBT-2). 'Nem' (vagy egyirányú bekötés): beszorulásnál megáll és riaszt.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_unjam_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, unjam_enable)), nullptr, 9035, "jam", "", ""},
    {"unjam_deg", "drive", "Visszafordítás mértéke", "Vissza mérték", "°", "Adagolótengely-fok, az enkóder impulzusaiból számolva.", ParamKind::Float, 5.0f, 180.0f, 5.0f, 30.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, unjam_deg)), nullptr, 9036, "jam", "", ""},
    {"unjam_tries", "drive", "Visszafordítási próbák száma", "Vissza próbák", "db", "Ennyi sikertelen próba után a motor áll és riaszt. 5 s rendben forgás után nullázódik.", ParamKind::Int, 1.0f, 5.0f, 1.0f, 3.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, unjam_tries)), nullptr, 9037, "jam", "", ""},
    {"jam_detect_s", "drive", "Beszorulás felismerési ideje", "Felismerés", "s", "Ennyi ideig nem jön enkóderjel elég nagy PWM mellett (vagy a PWM a maximumon, és a fordulat a cél negyede alatt) → beszorulás.", ParamKind::Float, 0.2f, 5.0f, 0.1f, 0.6f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, jam_detect_s)), nullptr, 9038, "jam", "", ""},
    {"keya_node1", "drive", "Keya: 1. motor CAN azonosító (node ID)", "1. motor ID", "", "A motorban beállított node ID (gyárilag 1). Két motornál a 2. motort át kell állítani (pl. 2).", ParamKind::Int, 1.0f, 31.0f, 1.0f, 1.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, keya_node1)), nullptr, 9039, "keya", "", ""},
    {"keya_node2", "drive", "Keya: 2. motor CAN azonosító (node ID)", "2. motor ID", "", "", ParamKind::Int, 1.0f, 31.0f, 1.0f, 2.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, keya_node2)), nullptr, 9040, "keya", "", ""},
    {"keya_unit", "drive", "Keya: a fordulat/s mire vonatkozik", "Ford. egység", "", "A gyári leírás nem mondja meg. Próbával dönthető el: 1 ford/s parancsra a kimenő tengely másodpercenként 1-et fordul (→ kimenet), vagy 1/áttételnyit (→ motortengely). KY40…-PX40-49: áttétel 49.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_keya_unit, 2, static_cast<std::uint16_t>(offsetof(Settings, keya_unit)), nullptr, 9041, "keya", "", ""},
    {"keya_gear", "drive", "Keya: hajtómű áttétel", "Áttétel", "", "", ParamKind::Float, 1.0f, 500.0f, 0.5f, 49.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, keya_gear)), nullptr, 9042, "keya", "", ""},
    {"keya_hb_ms", "drive", "Keya: szívverés-időkorlát a motorban", "Szívverés", "ms", "Induláskor a vezérlő beírja a motorba (33-as paraméter): ha ennyi ideig nem jön szívverés, a motor magától megáll. 0 = nem írja be.", ParamKind::Int, 0.0f, 5000.0f, 50.0f, 500.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, keya_hb_ms)), nullptr, 9043, "keya", "", ""},
    {"motor_deadband_pct", "drive", "Indulási PWM (holtsáv)", "Indulási PWM", "%", "Ennél kisebb PWM-nél a motor nem forog. Az önhangolás méri és menti; a terhelési mutató és az előrecsatolás pontosításához.", ParamKind::Float, 0.0f, 60.0f, 0.5f, 0.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, motor_deadband_pct)), nullptr, 9044, "motor", "", ""},
    {"fan_ppr", "fan", "Turbina impulzus / fordulat", "Imp/fordulat", "imp", "", ParamKind::Int, 1.0f, 100.0f, 1.0f, 1.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, fan_ppr)), nullptr, 9045, "fan", "", ""},
    {"fan_min_rpm", "fan", "Riasztás e fordulat alatt", "Min. fordulat", "1/min", "Csak akkor riaszt, ha a gép dolgozik: munkakapcsolóval lent van és halad; munkakapcsoló nélkül ha halad és a vetés BE.", ParamKind::Int, 0.0f, 20000.0f, 10.0f, 2500.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, fan_min_rpm)), nullptr, 9046, "fan", "", ""},
    {"fan_max_rpm", "fan", "Riasztás e fordulat felett (0 = nincs)", "Max. fordulat", "1/min", "", ParamKind::Int, 0.0f, 20000.0f, 10.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, fan_max_rpm)), nullptr, 9047, "fan", "", ""},
    {"fan_delay_s", "fan", "Riasztás késleltetése", "Késleltetés", "s", "", ParamKind::Float, 0.0f, 60.0f, 0.5f, 5.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, fan_delay_s)), nullptr, 9048, "fan", "", ""},
    {"speed_src", "speed", "Sebesség forrása", "Sebesség forrása", "", "AgOpenGPS: a sebességet az AgIO-tól kapja Ethernet/UDP-n (PGN 254), ehhez az AgOpenGPS kapcsolatot be kell kapcsolni.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_speed_src, 6, static_cast<std::uint16_t>(offsetof(Settings, speed_src)), nullptr, 9049, "speed", "", ""},
    {"isobus_speed_pref", "speed", "ISOBUS sebességüzenet", "ISOBUS üzenet", "", "Automatikus sorrend: gép által választott > föld (radar/GPS) > NMEA GPS > traktor kerék. Ha a terminál megosztja a GPS sebességét, az 'NMEA GPS' vagy 'Földsebesség' jön.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_isobus_speed_pref, 5, static_cast<std::uint16_t>(offsetof(Settings, isobus_speed_pref)), nullptr, 9050, "speed", "", "isobus"},
    {"wheel_ppm", "speed", "Kerékjeladó impulzus / 100 m", "Imp/100 m", "imp", "", ParamKind::Float, 1.0f, 100000.0f, 0.1f, 100.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, wheel_ppm)), nullptr, 9051, "speed", "", ""},
    {"sim_speed", "speed", "Szimulált sebesség", "Szimulált seb.", "km/h", "", ParamKind::Float, 0.0f, 30.0f, 0.1f, 8.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, sim_speed)), nullptr, 9052, "speed", "", ""},
    {"min_work_speed", "speed", "Haladás küszöb (ez alatt áll a gép)", "Haladás küszöb", "km/h", "", ParamKind::Float, 0.1f, 10.0f, 0.1f, 1.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, min_work_speed)), nullptr, 9053, "speed", "", ""},
    {"slip_alarm_pct", "speed", "Kerék – GPS sebességeltérés riasztás (0 = nincs)", "Eltérés riasztás", "%", "A kerékjeladó és a GPS (AgOpenGPS vagy ISOBUS) sebességének eltérése (csúszás). Csak haladás közben, mindkét jel megléte esetén figyel.", ParamKind::Float, 0.0f, 50.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, slip_alarm_pct)), nullptr, 9054, "speed", "", ""},
    {"slip_delay_s", "speed", "Eltérés riasztás késleltetése", "Eltérés késl.", "s", "", ParamKind::Float, 0.5f, 30.0f, 0.5f, 3.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, slip_delay_s)), nullptr, 9055, "speed", "", ""},
    {"speed_timeout_ms", "speed", "Sebességjel időtúllépés (ISOBUS)", "Jel időtúllépés", "ms", "", ParamKind::Int, 200.0f, 10000.0f, 100.0f, 1500.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, speed_timeout_ms)), nullptr, 0, nullptr, "", ""},
    {"work_sw", "switches", "Munkakapcsoló (gép lent)", "Munkakapcsoló", "", "Ha van, a 'gép lent' állapot a vetés (adagolók, terület) feltétele.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_work_sw, 3, static_cast<std::uint16_t>(offsetof(Settings, work_sw)), nullptr, 9057, "switches", "", ""},
    {"work_sw_tc", "switches", "Munkakapcsoló az automata szakaszolásnál is feltétel", "Munkakapcs. → TC", "", "Igen: TC-SEC automata szakaszolásnál is csak lent vet (a TC-nek kiemelt gépnél 'nem dolgozik' jelentést küldünk). Nem: automatában csak a TC dönt.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_work_sw_tc, 2, static_cast<std::uint16_t>(offsetof(Settings, work_sw_tc)), nullptr, 9058, "switches", "", "isobus"},
    {"hopper1_nc", "switches", "1. tartály leürülésgátló kapcsoló", "1. tartály kapcs.", "", "NO: üres tartálynál zár. NC: üres tartálynál bont.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_hopper1_nc, 2, static_cast<std::uint16_t>(offsetof(Settings, hopper1_nc)), nullptr, 9059, "switches", "", ""},
    {"hopper2_nc", "switches", "2. tartály leürülésgátló kapcsoló", "2. tartály kapcs.", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_hopper2_nc, 2, static_cast<std::uint16_t>(offsetof(Settings, hopper2_nc)), nullptr, 9060, "switches", "", ""},
    {"hopper_delay_s", "switches", "Leürülés jelzés szűrése", "Tartály szűrés", "s", "", ParamKind::Float, 0.0f, 30.0f, 0.5f, 2.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, hopper_delay_s)), nullptr, 9061, "switches", "", ""},
    {"crank_btn", "switches", "Leforgató nyomógomb a gépen (IN8)", "Leforgató gomb", "", "Elektromos hajtásnál amíg nyomod, a kiválasztott adagoló forog (leforgatás). Talajhajtásnál 2 s nyomás nullázza a leforgatási számlálót. NO gomb, GND felé zár.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_crank_btn, 2, static_cast<std::uint16_t>(offsetof(Settings, crank_btn)), nullptr, 9062, "switches", "", ""},
    {"out_invert", "switches", "Kimenetek fordított szintje (relémodul)", "Kimenet fordítva", "", "Igen: alacsony szint kapcsolja be a relét (kuplung, lámpa, szelep, hűtő).", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_out_invert, 2, static_cast<std::uint16_t>(offsetof(Settings, out_invert)), nullptr, 9063, "switches", "", ""},
    {"lights_enable", "extras", "Munkalámpák (2 db)", "Munkalámpák", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_lights_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, lights_enable)), nullptr, 9064, "extras", "", ""},
    {"lamp1_relay", "extras", "1. munkalámpa kimenete", "Lámpa 1 kimenet", "", "Melyik relé kapcsolja az 1. munkalámpát (alap: relé 4). Ha más funkció reléjét választod, mindkettő kapcsolja (VAGY kapcsolat) – csak szabad relét válassz.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 3.0f, 0, OPT_lamp1_relay, 8, static_cast<std::uint16_t>(offsetof(Settings, lamp1_relay)), nullptr, 0, nullptr, "", ""},
    {"lamp2_relay", "extras", "2. munkalámpa kimenete", "Lámpa 2 kimenet", "", "Melyik relé kapcsolja a 2. munkalámpát (alap: relé 5).", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 4.0f, 0, OPT_lamp2_relay, 8, static_cast<std::uint16_t>(offsetof(Settings, lamp2_relay)), nullptr, 0, nullptr, "", ""},
    {"crank_lamp", "extras", "Munkalámpa villan leforgatásnál", "Lámpa leforgatás", "", "Minden megszámolt fordulatnál röviden (0,15 s) felvillan; a célfordulatnál 5 s-ig gyorsan villog, utána a mentésig folyamatosan világít (a gép mögött is látszik). Csak ha a munkalámpák engedélyezve vannak.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_crank_lamp, 2, static_cast<std::uint16_t>(offsetof(Settings, crank_lamp)), nullptr, 9067, "extras", "", ""},
    {"valve_enable", "extras", "Nyitás / nyomjelző váltószelep", "Nyitás/nyomjelző", "", "Alapban a hidraulikakör a gépet nyitja/csukja; a szelep bekapcsolva a nyomjelzőre tereli.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_valve_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, valve_enable)), nullptr, 9068, "extras", "", ""},
    {"pressure_enable", "extras", "Nyomásmérés (lenyomatás, ADS1115)", "Nyomásmérés", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_pressure_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, pressure_enable)), nullptr, 9069, "extras", "", ""},
    {"temp_enable", "extras", "Olajhőmérséklet + hűtőventilátor (2× DS18B20)", "Olajhőmérséklet", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_temp_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, temp_enable)), nullptr, 9070, "extras", "", ""},
    {"scale_enable", "extras", "Leforgatási mérleg (HX711)", "Mérleg", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_scale_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, scale_enable)), nullptr, 9071, "extras", "", ""},
    {"pressure_range_bar", "pressure", "Szenzor méréshatár", "Méréshatár", "bar", "", ParamKind::Float, 1.0f, 1000.0f, 1.0f, 250.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pressure_range_bar)), nullptr, 9072, "pressure", "pressure_enable", ""},
    {"pressure_signal", "pressure", "Szenzor kimenőjele", "Kimenőjel", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_pressure_signal, 4, static_cast<std::uint16_t>(offsetof(Settings, pressure_signal)), nullptr, 9073, "pressure", "pressure_enable", ""},
    {"pressure_cal", "pressure", "Kalibrációs szorzó", "Kalibr. szorzó", "", "Az 'Ismert nyomás' kalibrálás állítja be (1,000 = gyári jelleggörbe).", ParamKind::Float, 0.5f, 2.0f, 0.001f, 1.0f, 3, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pressure_cal)), nullptr, 9074, "pressure", "pressure_enable", ""},
    {"pressure_zero_bar", "pressure", "Nullpont-eltolás", "Nullpont", "bar", "", ParamKind::Float, -50.0f, 50.0f, 0.1f, 0.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pressure_zero_bar)), nullptr, 9075, "pressure", "pressure_enable", ""},
    {"pressure_ch", "pressure", "ADS1115 csatorna", "ADS csatorna", "", "", ParamKind::Int, 0.0f, 3.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pressure_ch)), nullptr, 0, nullptr, "pressure_enable", ""},
    {"pressure_divider", "pressure", "Bemeneti feszültségosztó (V szenzor / V ADS)", "", "", "Ha az ADS1115 bemenete előtt osztó van (pl. 3,3 V-os táp esetén), add meg az arányát.", ParamKind::Float, 1.0f, 10.0f, 0.001f, 1.0f, 3, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, pressure_divider)), nullptr, 0, nullptr, "pressure_enable", ""},
    {"cooler_on_c", "cooling", "Hűtőventilátor bekapcsol", "Bekapcsol", "°C", "", ParamKind::Float, 20.0f, 120.0f, 0.5f, 60.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, cooler_on_c)), nullptr, 9078, "cooling", "temp_enable", ""},
    {"cooler_off_c", "cooling", "Hűtőventilátor kikapcsol", "Kikapcsol", "°C", "", ParamKind::Float, 10.0f, 115.0f, 0.5f, 50.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, cooler_off_c)), nullptr, 9079, "cooling", "temp_enable", ""},
    {"cooler_src", "cooling", "Szabályozó hőmérő", "Szabályozó hőmérő", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_cooler_src, 3, static_cast<std::uint16_t>(offsetof(Settings, cooler_src)), nullptr, 9080, "cooling", "temp_enable", ""},
    {"temp_alarm_c", "cooling", "Túlmelegedés riasztás (0 = nincs)", "Riasztás felett", "°C", "", ParamKind::Float, 0.0f, 130.0f, 0.5f, 85.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, temp_alarm_c)), nullptr, 9081, "cooling", "temp_enable", ""},
    {"rev_enable", "cooling", "Hűtőventilátor visszafúvás (forgásirány-váltás, relé 7)", "Visszafúvás", "", "A hűtő kifúvása: a ventilátor leáll, a várakozás után az irányváltó relé (motor polaritás / hidraulika irányváltó) bekapcsol és a ventilátor visszafelé fúj, majd újra várakozás után vissza.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_rev_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, rev_enable)), nullptr, 9082, "cooling", "temp_enable", ""},
    {"rev_interval_min", "cooling", "Visszafúvás gyakorisága (0 = csak kézi)", "Gyakoriság", "perc", "", ParamKind::Int, 0.0f, 240.0f, 1.0f, 20.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rev_interval_min)), nullptr, 9083, "cooling", "temp_enable", ""},
    {"rev_time_s", "cooling", "Visszafúvás időtartama", "Időtartam", "s", "", ParamKind::Float, 2.0f, 120.0f, 1.0f, 15.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rev_time_s)), nullptr, 9084, "cooling", "temp_enable", ""},
    {"rev_pause_s", "cooling", "Leállási idő irányváltás előtt / után", "Leállási idő", "s", "Ennyi ideig áll a ventilátor, mielőtt irányt vált (motor / hidraulika kímélése).", ParamKind::Float, 1.0f, 60.0f, 0.5f, 5.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, rev_pause_s)), nullptr, 9085, "cooling", "temp_enable", ""},
    {"temp_swap", "cooling", "A két DS18B20 hőmérő felcserélése", "", "", "A hőmérőket a firmware a ROM-azonosítójuk sorrendjében számozza. Ha az olajtartály és a hűtő kimenet értéke fordítva jelenik meg, kapcsold be.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_temp_swap, 2, static_cast<std::uint16_t>(offsetof(Settings, temp_swap)), nullptr, 0, nullptr, "temp_enable", ""},
    {"scale_capacity_kg", "scale", "Mérlegcella méréshatára", "Méréshatár", "kg", "", ParamKind::Float, 1.0f, 500.0f, 0.5f, 50.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, scale_capacity_kg)), nullptr, 9087, "scale", "scale_enable", ""},
    {"scale_factor", "scale", "Mérleg kalibrációs tényező (beütés / kg)", "Tényező", "1/kg", "A 'Kalibrálás ismert súllyal' állítja be.", ParamKind::Float, 1.0f, 10000000.0f, 0.1f, 85000.0f, 1, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, scale_factor)), nullptr, 9088, "scale", "scale_enable", ""},
    {"scale_offset", "scale", "Mérleg nullpont (tára, nyers)", "", "", "", ParamKind::Float, -20000000.0f, 20000000.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, scale_offset)), nullptr, 0, nullptr, "scale_enable", ""},
    {"sec_src", "tc", "Automata szakaszolás forrása", "Szakaszolás forr.", "", "Az 'AUTO' gomb ezt a forrást kapcsolja be. A kézi adagológombok mindig érvényesek: kikapcsolt adagolót az automata sem kapcsol be.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_sec_src, 3, static_cast<std::uint16_t>(offsetof(Settings, sec_src)), nullptr, 9090, "tc", "", ""},
    {"tc_enable", "tc", "Task Controller kliens", "TC kliens", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_tc_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, tc_enable)), nullptr, 9091, "tc", "", "isobus"},
    {"tc_sec", "tc", "Automatikus szakaszvezérlés (TC-SEC)", "TC-SEC", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_tc_sec, 2, static_cast<std::uint16_t>(offsetof(Settings, tc_sec)), nullptr, 9092, "tc", "", "isobus"},
    {"tc_geo", "tc", "Kijuttatási térkép (TC-GEO, kg/ha az adagolóknak)", "TC-GEO", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_tc_geo, 2, static_cast<std::uint16_t>(offsetof(Settings, tc_geo)), nullptr, 9093, "tc", "", "isobus"},
    {"hitch_type", "tc", "Csatlakozás", "Csatlakozás", "", "", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 2.0f, 0, OPT_hitch_type, 5, static_cast<std::uint16_t>(offsetof(Settings, hitch_type)), nullptr, 9094, "geometry", "", "isobus"},
    {"geo_a_m", "tc", "A: vonópont → csuklópont (kormányzott vonórúd)", "A vonópont-csukló", "m", "", ParamKind::Float, 0.0f, 20.0f, 0.01f, 0.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, geo_a_m)), nullptr, 9095, "geometry", "", "isobus"},
    {"geo_b_m", "tc", "B: csuklópont → csoroszlyák (függesztett: vonópont → csoroszlyák)", "B csukló-csorosz.", "m", "", ParamKind::Float, 0.0f, 20.0f, 0.01f, 4.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, geo_b_m)), nullptr, 9096, "geometry", "", "isobus"},
    {"geo_c_m", "tc", "C: csuklópont → kerékagy (fordulópont)", "C csukló-tengely", "m", "", ParamKind::Float, 0.0f, 20.0f, 0.01f, 3.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, geo_c_m)), nullptr, 9097, "geometry", "", "isobus"},
    {"geo_y_m", "tc", "D: oldalirányú eltolás (+ jobbra)", "D oldaleltolás", "m", "", ParamKind::Float, -5.0f, 5.0f, 0.01f, 0.0f, 2, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, geo_y_m)), nullptr, 9098, "geometry", "", "isobus"},
    {"delay_on_ms", "tc", "Tehetetlenség BE (csak ISOBUS TC)", "Tehetetl. BE (TC)", "ms", "Csak az ISOBUS Task Controllernek szól (előre kapcsolás). AgOpenGPS-nél az előretekintést az AgOpenGPS szakaszbeállításainál kell megadni.", ParamKind::Int, 0.0f, 5000.0f, 10.0f, 800.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, delay_on_ms)), nullptr, 9099, "tc", "", "isobus"},
    {"delay_off_ms", "tc", "Tehetetlenség KI (csak ISOBUS TC)", "Tehetetl. KI (TC)", "ms", "", ParamKind::Int, 0.0f, 5000.0f, 10.0f, 400.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, delay_off_ms)), nullptr, 9100, "tc", "", "isobus"},
    {"aog_enable", "aog", "AgOpenGPS kapcsolat (Ethernet, UDP 8888 / 9999)", "AgOpenGPS", "", "Csak Ethernetes panelen (Waveshare ESP32-S3-ETH). A vezérlő gépmodulként jelenik meg az AgIO-ban: sebességet (PGN 254) és szakaszállapotot (PGN 239) fogad, festést / munkakapcsolót (PGN 234) küld. Az autokormány-modul kommunikációjához nem nyúl.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_aog_enable, 2, static_cast<std::uint16_t>(offsetof(Settings, aog_enable)), nullptr, 9101, "aog", "", "aog"},
    {"aog_ip_mode", "aog", "IP-cím", "IP-cím", "", "AgOpenGPS-hez fix IP kell, az AgIO alhálózatában (alap: 192.168.5.x).", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_aog_ip_mode, 2, static_cast<std::uint16_t>(offsetof(Settings, aog_ip_mode)), nullptr, 9102, "aog", "", "aog"},
    {"aog_ip", "aog", "Fix IP-cím (a 4. bájt maradjon szabad az AgIO hálózatán)", "", "", "Az AgIO 'alhálózat beállítása' (PGN 201) az első három bájtot átírja.", ParamKind::Str, 7.0f, 15.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, aog_ip)), "192.168.5.130", 0, nullptr, "aog_enable", "aog"},
    {"aog_paint", "aog", "Festés / munkakapcsoló küldése az AgOpenGPS-nek", "Festés küldése", "", "Gép lent: 3 'BE' csomag (utána a kezelő az AgOpenGPS-ben kézzel leállíthatja), gép fent: 'KI' 500 ms-onként. Az AgOpenGPS a festést a szakaszaival együtt kezeli.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 1.0f, 0, OPT_aog_paint, 2, static_cast<std::uint16_t>(offsetof(Settings, aog_paint)), nullptr, 9104, "aog", "", "aog"},
    {"aog_paint_mode", "aog", "Festés módja", "Festés módja", "", "Automata: az AgOpenGPS automata szakaszként kezeli (zöld). Kézi: fix kézi BE/KI a vetőgép szakaszai szerint (sárga). Ha a szakaszolás forrása AgOpenGPS, mindig automata.", ParamKind::Enum, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_aog_paint_mode, 2, static_cast<std::uint16_t>(offsetof(Settings, aog_paint_mode)), nullptr, 9105, "aog", "", "aog"},
    {"sim_inputs", "sim", "Bemenetek szimulációja (turbina, adagolók, munkakapcsoló)", "Bemenetek szim.", "", "Próbapadi teszthez: a turbina, az adagolók és az enkóderek impulzusait a firmware állítja elő, a munkakapcsoló 'lent' állapotú.", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_sim_inputs, 2, static_cast<std::uint16_t>(offsetof(Settings, sim_inputs)), nullptr, 9106, "diag", "", ""},
    {"sim_fan_rpm", "sim", "Szimulált turbinafordulat", "Szim. turbina", "1/min", "", ParamKind::Int, 0.0f, 10000.0f, 10.0f, 3500.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, sim_fan_rpm)), nullptr, 9107, "diag", "", ""},
    {"sim_wheel", "sim", "Kerékimpulzus szimuláció a szimulált sebességből", "Kerék szimuláció", "", "", ParamKind::Bool, 0.0f, 0.0f, 1.0f, 0.0f, 0, OPT_sim_wheel, 2, static_cast<std::uint16_t>(offsetof(Settings, sim_wheel)), nullptr, 9108, "diag", "", ""},
    {"wifi_pass", "system", "Wi-Fi jelszó (8–32 karakter)", "", "", "", ParamKind::Str, 8.0f, 32.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, wifi_pass)), "12345678", 0, nullptr, "", ""},
    {"wifi_timeout_min", "system", "WiFi szerviz mód: kikapcsol tétlenség után", "", "perc", "A WiFi alapból KI van kapcsolva; bekapcsolás a VT menüjéből (WiFi), a külső kijelzőről vagy a leforgató gomb (IN8) bekapcsoláskori 3 s-os nyomva tartásával. Ennyi perc tétlenség után magától lekapcsol (kliens csatlakozása vagy OTA alatt nem). Aktív munkakapcsolónál nem kapcsolható be. Az Etherneten át a web mindig elérhető.", ParamKind::Int, 5.0f, 60.0f, 1.0f, 20.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, wifi_timeout_min)), nullptr, 0, nullptr, "", ""},
    {"ecu_instance", "system", "ECU példány (több gép egy buszon)", "", "", "", ParamKind::Int, 0.0f, 7.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, ecu_instance)), nullptr, 0, nullptr, "", ""},
    {"isobus_delay_s", "system", "ISOBUS indulás késleltetése (terminál lassabban indul)", "", "s", "Ennyi másodperccel később jelentkezik be a CAN buszra. Ha a terminál (pl. Müller) a később induló gépet nem találja, állíts be 10–20 s-ot.", ParamKind::Int, 0.0f, 60.0f, 1.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, isobus_delay_s)), nullptr, 0, nullptr, "", "isobus"},
    {"isobus_retry_s", "system", "Újracsatlakozás, ha nincs terminál (0 = ki)", "", "s", "Ha ennyi ideig nincs VT kapcsolat, a vezérlő újra bejelentkezik (címigénylés), és a VT/TC kapcsolatot újrakezdi.", ParamKind::Int, 0.0f, 600.0f, 5.0f, 30.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, isobus_retry_s)), nullptr, 0, nullptr, "", "isobus"},
    {"realarm_s", "system", "Nyugtázott, fennálló riasztás újra (0 = soha)", "", "s", "", ParamKind::Int, 0.0f, 3600.0f, 10.0f, 0.0f, 0, nullptr, 0, static_cast<std::uint16_t>(offsetof(Settings, realarm_s)), nullptr, 0, nullptr, "", ""},
};
#endif
} // namespace seeder

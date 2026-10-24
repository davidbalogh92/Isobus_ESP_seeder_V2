#pragma once
// A vetőgép-vezérlő magja. Hordozható (ESP32 + PC), nincs benne hardver- vagy ISOBUS-függés:
// a bemeneteket ciklusonként kapja (TickInputs), a kimeneteket (Outputs) és a kijelzendő
// állapotot (Status) visszaadja. A hívó gondoskodik a szálbiztonságról.
//
// Feladatai: sebesség (kerékjeladó / ISOBUS / szimulált), munkakapcsoló, adagolók kapcsolása
// (kuplung) vagy szabályzása (elektromos motor + enkóder, PI + előrecsatolás, rámpák),
// szakaszvezérlés (kézi + TC-SEC), indulási csőfeltöltés, turbina- és adagolófigyelés,
// leürülésgátlók, lenyomatási nyomás, olajhőmérséklet + hűtőventilátor, mérleg, leforgatás.
#include "seeder/pulse.hpp"
#include "seeder/rate_chart.hpp"
#include "seeder/settings.hpp"

#include <cstdint>
#include <string>

namespace seeder {

// ------------------------------------------------------------------------- sebesség
enum class SpeedSource : std::uint8_t { None, Wheel, IsobusGround, IsobusWheel, IsobusMachine, Nmea, Simulated };
const char *speed_source_label(SpeedSource s);

struct IsobusSpeedValue {
    bool valid = false;        ///< érkezett már ilyen üzenet
    float kmh = 0.0f;
    std::uint32_t age_ms = 0xFFFFFFFFu;
};
struct IsobusSpeeds {
    IsobusSpeedValue ground, wheel, machine, nmea;
};
struct SpeedResult {
    float kmh = 0.0f;
    SpeedSource used = SpeedSource::None;
    bool valid = false;        ///< van használható sebességjel
    bool fallback = false;     ///< ISOBUS kiesett, a kerékjeladó helyettesít
};
SpeedResult select_speed(const Settings &s, float wheel_kmh, const IsobusSpeeds &isobus);

// ------------------------------------------------------------------------- riasztások
enum class AlarmId : std::uint8_t {
    Meter1Stopped,
    Meter2Stopped,
    Meter1Rate,
    Meter2Rate,
    FanLow,
    FanHigh,
    Hopper1Empty,
    Hopper2Empty,
    NoSpeed,
    SpeedFallback,
    OilHot,
    TempSensor,
    PressureSensor,
    ScaleSensor,
    WheelNotCalibrated,
    SimulationActive,
    TcLost,
    COUNT
};
enum class Severity : std::uint8_t { Info = 0, Warning = 1, Alarm = 2 };
struct AlarmInfo {
    const char *code;
    const char *text;
    Severity severity;
};
const AlarmInfo &alarm_info(AlarmId id);

class Alarms {
public:
    void set(AlarmId id, bool active);
    bool active(AlarmId id) const { return (active_ >> static_cast<unsigned>(id)) & 1u; }
    std::uint32_t active_mask() const { return active_; }
    std::uint32_t unacked_mask() const { return unacked_ & active_; }
    /// A legsúlyosabb nyugtázatlan riasztás (Alarm szint), vagy COUNT.
    AlarmId top_unacked_alarm() const;
    void ack_all();
    /// Nyugtázott, de még fennálló riasztás újra feldobása (realarm_s > 0).
    void tick(float dt, float realarm_s);

private:
    std::uint32_t active_ = 0, unacked_ = 0;
    float acked_for_[static_cast<int>(AlarmId::COUNT)] = {};
};

// ------------------------------------------------------------------------- adagoló motor
/// Egy elektromos adagolómotor szabályzója: előrecsatolás (cél ford. / ford. 100%-nál) +
/// PI korrekció az enkóder alapján, PWM min/max, felfutás/lefutás rámpa.
struct MotorOutput {
    float pwm_pct = 0.0f;
    bool saturated = false;   ///< max PWM-en van, mégsem éri el a célt
    float error_pct = 0.0f;
};
class MotorController {
public:
    void reset() { integral_ = 0.0f; out_ = 0.0f; run_time_ = 0.0f; }
    MotorOutput step(const Settings &s, float dt, float target_rpm, float actual_rpm);
    float integral() const { return integral_; }
    void restore(float integral) { integral_ = integral; }
    float running_for() const { return run_time_; }

private:
    float integral_ = 0.0f;   ///< %-ban, tartósan menthető ("tanult" eltérés)
    float out_ = 0.0f;
    float run_time_ = 0.0f;
};

// ------------------------------------------------------------------------- feltöltés
enum class PrefillState : std::uint8_t { Idle, Filling, AfterRun };
const char *prefill_label(PrefillState s);

// ------------------------------------------------------------------------- kalibrálások
enum class CalState : std::uint8_t { Idle, Running, Stopped };

struct WheelCalibration {
    CalState state = CalState::Idle;
    std::uint32_t pulses = 0;
    float distance_m = 100.0f;
    bool compute(float &pulses_per_100m, const char **error) const;
};

/// Leforgatási segéd (Accord táblázat) + mérés.
struct CrankCalibration {
    int meter = 0;                 ///< melyik adagoló (0/1)
    int crop = chart::WHEAT;
    int turn_option = 0;           ///< 0: 85, 1: 42,5, 2: 17 fordulat
    float test_scale = 0.0f;       ///< 0 = a táblázati beállítás
    float measured_kg = 0.0f;
    float hectares = 0.0f;
    // motoros leforgatás (elektromos hajtás): pontosan N fordulat az enkóder alapján
    bool motor_running = false;
    float motor_revs_done = 0.0f;
    // talajhajtásnál a tengelyjeladó számolja a kézi tekerést
    std::uint32_t meter_pulses = 0;
};

struct ScaleState {
    bool valid = false;            ///< van friss mérés a HX711-től
    float raw = 0.0f;              ///< szűrt nyers érték
    float net_kg = 0.0f;
    bool overload = false;
};

// ------------------------------------------------------------------------- számlálók
struct Totals {
    double area_m2 = 0.0;          ///< vetett terület
    double distance_m = 0.0;       ///< megtett út
    double work_time_s = 0.0;      ///< vetéssel töltött idő
    double kg[MAX_METERS] = {0.0, 0.0};   ///< kijuttatott mennyiség adagolónként
};

/// Tartósan mentendő állapot (NVS). Verzióváltáskor a magic/version alapján eldobható.
struct Persistent {
    static constexpr std::uint32_t MAGIC = 0x53454544u; // "SEED"
    static constexpr std::uint32_t VERSION = 1;
    std::uint32_t magic = MAGIC;
    std::uint32_t version = VERSION;
    Totals day;
    Totals total;                  ///< nem törölhető összesítő
    float motor_integral[MAX_METERS] = {0.0f, 0.0f};
    std::uint8_t cooler_manual = 0; ///< hűtőventilátor kézi mód
    std::uint8_t reserved8[3] = {};
    std::uint32_t reserved[8] = {};
};

// ------------------------------------------------------------------------- vezérlő
/// Egy ciklus bemenetei. A digitális érintkezőket nyersen kapja (zárt = true), a NO/NC-t a mag
/// alkalmazza. Hiányzó analóg érték: NaN.
struct TickInputs {
    float dt_s = 0.05f;
    PulseSample wheel;                 ///< kerékjeladó
    PulseSample fan;                   ///< turbina
    PulseSample meter[MAX_METERS];     ///< adagolótengely-jeladó (talajhajtás) vagy enkóder (motor)
    bool work_contact = false;         ///< munkakapcsoló érintkező zárva
    bool hopper_contact[2] = {false, false};
    IsobusSpeeds isobus;
    bool tc_connected = false;
    float pressure_v = -1.0f;          ///< ADS1115 feszültség (V), < 0 = nincs adat
    float temp_c[2] = {-300.0f, -300.0f}; ///< DS18B20: [0] olajtartály, [1] hűtő kimenet; < -100 = nincs
    bool scale_valid = false;
    float scale_raw = 0.0f;            ///< HX711 nyers érték
};

/// Kimenetek. A *_level a fizikai szint (a kuplungtípus és a kimenet-fordítás már alkalmazva).
struct Outputs {
    bool clutch_level[MAX_METERS] = {false, false};
    bool clutch_drive[MAX_METERS] = {false, false};   ///< logikai: az adagoló hajtva
    float motor_pwm_pct[MAX_METERS] = {0.0f, 0.0f};
    bool lamp_level[2] = {false, false};
    bool valve_level = false;
    bool cooler_level = false;
};

enum class SowState : std::uint8_t { Off, Raised, Standing, Sowing, Prefill, Alarm };
const char *sow_state_label(SowState s);

/// Adagoló állapota a kijelzéshez (ESC ikonok sorrendje).
enum class MeterView : std::uint8_t { Ready = 0, Work = 1, ShutDown = 2, Alarm = 3, Idle = 4 };

struct MeterStatus {
    bool enabled = true;           ///< kezelő engedélye (gomb)
    bool section_on = false;       ///< szakaszvezérlés szerint vetnie kell
    bool running = false;          ///< hajtva (kuplung zárva / motor forog)
    float rpm = 0.0f;              ///< mért adagolófordulat
    float target_rpm = 0.0f;       ///< elektromos hajtás célfordulat
    float pwm_pct = 0.0f;
    float target_rate = 0.0f;      ///< kg/ha (+ ±% eltolás, TC-GEO)
    float actual_rate = 0.0f;      ///< kg/ha a mért fordulatból
    float kg_per_min = 0.0f;
    float width_m = 0.0f;
    float since_pulse_s = 0.0f;
    MeterView view = MeterView::Idle;
};

struct Status {
    // sebesség, munka
    float speed_kmh = 0.0f;
    SpeedSource speed_src = SpeedSource::None;
    bool speed_valid = false;
    bool speed_fallback = false;
    float wheel_hz = 0.0f;
    bool moving = false;
    bool work_down = false;        ///< gép lent (vagy nincs munkakapcsoló)
    bool work_contact = false;
    bool master = false;           ///< vetés BE
    SowState state = SowState::Off;
    // adagolók, szakaszok
    MeterStatus meter[MAX_METERS];
    int meter_count = 2;
    int section_count = 2;
    int tank_count = 2;
    bool section_on[MAX_SECTIONS] = {false, false};
    bool tc_setpoint[MAX_SECTIONS] = {false, false};
    bool tc_section_auto = false;  ///< a TC vezérli a szakaszokat
    float sown_width_m = 0.0f;
    float rate_offset_pct = 0.0f;
    bool manual = false;           ///< kézi adagolás (elektromos hajtás): állandó fordulat
    float manual_pct = 0.0f;       ///< kézi fordulat a "fordulat 100% PWM-nél" %-ában
    bool tc_rate = false;          ///< az előírt értéket a TC adja (TC-GEO)
    // feltöltés
    PrefillState prefill = PrefillState::Idle;
    float prefill_left_s = 0.0f;
    // turbina, tartályok
    float fan_rpm = 0.0f;
    bool hopper_empty[2] = {false, false};
    bool hopper_contact[2] = {false, false};
    // kiegészítők
    bool lamp[2] = {false, false};
    bool valve_marker = false;     ///< a hidraulika a nyomjelzőre van terelve
    bool pressure_ok = false;
    float pressure_bar = 0.0f;
    float pressure_v = 0.0f;
    bool temp_ok[2] = {false, false};
    float temp_c[2] = {0.0f, 0.0f};
    bool cooler_on = false;
    bool cooler_manual = false;
    ScaleState scale;
    // kalibrálás
    WheelCalibration wheel_cal;
    float wheel_cal_ppm = 0.0f;
    bool wheel_cal_ok = false;
    const char *wheel_cal_error = "";
    CrankCalibration crank;
    chart::Result crank_result;
    // egyéb
    bool tc_connected = false;
    std::uint32_t alarms_active = 0;
    std::uint32_t alarms_unacked = 0;
    AlarmId top_alarm = AlarmId::COUNT;
    std::uint32_t uptime_s = 0;
    const char *message = "";      ///< utolsó parancs-visszajelzés (pl. elutasítás oka)
    std::uint32_t message_seq = 0; ///< minden új visszajelzésnél nő
};

class Seeder {
public:
    Seeder();

    void load(const Settings &settings, const Persistent &persistent);
    const Settings &settings() const { return settings_; }
    bool set_param(std::uint16_t id, float value);
    bool set_param_str(std::uint16_t id, const char *value);
    void replace_settings(const Settings &s);

    void tick(const TickInputs &in);

    // --- kezelőparancsok (VT softkey, web) ---
    void set_master(bool on);
    void toggle_master() { set_master(!master_); }
    void toggle_meter(int i);
    void set_meter(int i, bool on);
    void toggle_section_auto();
    /// +/−: automatában az előírt értéket lépteti, kézi módban a motorfordulatot
    void rate_step(int dir);
    void set_auto(bool on);        ///< kézi mód csak elektromos hajtásnál
    void toggle_auto() { set_auto(manual_); }
    bool manual() const { return manual_; }
    /// egy adagoló előírt értékének és kg/fordulatának átmásolása a másikra
    bool copy_meter(int from);
    void rate_offset_reset() { rate_offset_steps_ = 0; }
    void toggle_lamp(int i);
    void toggle_valve();
    void toggle_cooler_mode();
    void toggle_cooler();
    bool prefill_start();          ///< kézi csőfeltöltés (elektromos hajtás)
    void prefill_stop();
    void ack_alarms() { alarms_.ack_all(); }
    void reset_day();

    // --- Task Controller felől ---
    void tc_set_section_setpoint(int index, bool on);
    void tc_set_section_auto(bool on) { tc_section_auto_ = on; }
    void tc_set_rate(int meter, float kg_ha);
    void tc_clear_rate();
    bool tc_section_auto_requested() const { return tc_section_auto_; }

    // --- kalibrálás ---
    void wheel_cal_start();
    void wheel_cal_stop();
    void wheel_cal_cancel();
    void wheel_cal_set_pulses(std::uint32_t pulses);
    void wheel_cal_set_distance(float meters);
    bool wheel_cal_save();

    void crank_set_meter(int meter);
    void crank_set_crop(int crop);
    void crank_set_turns(int option);
    void crank_set_test_scale(float scale);
    void crank_set_measured(float kg);
    void crank_set_hectares(float ha);
    bool crank_take_scale();       ///< mért tömeg = a mérleg nettó értéke
    bool crank_save();             ///< kg/ford mentése a kiválasztott adagolóhoz
    bool crank_motor_start();      ///< elektromos hajtás: pontosan N fordulat
    void crank_motor_stop();
    void crank_reset_count();
    /// a leforgatott (kiválasztott) adagoló adatai a másikra: ha van mentetlen mérés, előbb ment
    bool crank_copy_to_other();

    void scale_tare();
    bool scale_calibrate(float known_kg);
    bool pressure_calibrate(float known_bar);

    const Outputs &outputs() const { return outputs_; }
    const Status &status() const { return status_; }
    const Persistent &persistent() const { return persistent_; }

    // módosítás-jelzők a mentéshez / TC frissítéshez (a hívó törli)
    bool settings_dirty = false;
    bool persistent_dirty = false;

    std::string status_json() const;

private:
    void update_crank_result();
    void update_simulation(TickInputs &in, float dt);

    Settings settings_;
    Persistent persistent_;
    Alarms alarms_;
    Outputs outputs_;
    Status status_;
    MotorController motor_[MAX_METERS];
    PulseRate wheel_rate_{1.5f, 0.3f};
    PulseRate fan_rate_{1.0f, 0.4f};
    PulseRate meter_rate_[MAX_METERS] = {PulseRate{2.0f, 0.3f}, PulseRate{2.0f, 0.3f}};

    bool master_ = false;
    bool meter_enable_[MAX_METERS] = {true, true};
    bool tc_section_auto_ = false;
    bool tc_setpoint_[MAX_SECTIONS] = {false, false};
    float tc_rate_[MAX_METERS] = {0.0f, 0.0f};
    bool tc_rate_valid_ = false;
    bool tc_rate_has_[MAX_METERS] = {false, false};   ///< adagolónként kapott-e előírt értéket
    int rate_offset_steps_ = 0;
    bool manual_ = false;
    float manual_pct_ = 30.0f;
    bool lamp_[2] = {false, false};
    bool valve_ = false;
    bool cooler_manual_on_ = false;
    bool cooler_on_ = false;

    // munkakapcsoló, tartályok szűrése
    bool work_down_ = false;
    float work_timer_ = 0.0f;
    bool hopper_empty_[2] = {false, false};
    float hopper_timer_[2] = {0.0f, 0.0f};
    bool last_work_down_ = false;
    bool last_master_ = false;

    // feltöltés
    PrefillState prefill_ = PrefillState::Idle;
    float prefill_timer_ = 0.0f;

    // figyelés
    float since_pulse_[MAX_METERS] = {0.0f, 0.0f};
    float meter_run_time_[MAX_METERS] = {0.0f, 0.0f};
    float rate_dev_timer_[MAX_METERS] = {0.0f, 0.0f};
    float fan_low_timer_ = 0.0f, fan_high_timer_ = 0.0f;
    double scale_acc_ = 0.0;
    float scale_filtered_ = 0.0f;
    bool scale_has_ = false;
    float scale_timeout_ = 10.0f;
    float pressure_filtered_ = -1.0f;
    float pressure_raw_bar_ = 0.0f;   ///< kalibrációs szorzó és nullpont nélkül

    WheelCalibration wheel_cal_;
    CrankCalibration crank_;
    chart::Result crank_result_;

    // szimuláció
    float sim_motor_rpm_[MAX_METERS] = {0.0f, 0.0f};
    double sim_acc_[4] = {0.0, 0.0, 0.0, 0.0};
    double sim_since_[4] = {1e12, 1e12, 1e12, 1e12};

    float persist_timer_ = 0.0f;
    double uptime_s_ = 0.0;
    const char *message_ = "";
    std::uint32_t message_seq_ = 0;
    void say(const char *text) {
        message_ = text;
        ++message_seq_;
        status_.message = text;          // azonnal olvasható (pl. a parancs elutasításának oka)
        status_.message_seq = message_seq_;
    }
};

/// Egyszerű lapos JSON mezőolvasók (web API-hoz).
bool json_get_string(const char *json, const char *key, std::string &out);
bool json_get_number(const char *json, const char *key, double &out);

/// Webes/külső parancs végrehajtása a vezérlőn (a hívó zárol). A parancsok listája: commands.cpp.
bool apply_command(Seeder &s, const std::string &cmd, double value, std::string &error);

/// A nyomásszenzor jelének tartománya (V) a beállítás szerint.
void pressure_signal_range(int signal, float &v_min, float &v_max);

} // namespace seeder

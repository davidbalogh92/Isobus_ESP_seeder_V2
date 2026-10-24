#include "seeder/seeder.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace seeder {
namespace {
float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
float maxf(float a, float b) { return a > b ? a : b; }

const AlarmInfo ALARMS[] = {
    {"METER1_STOPPED", "1. adagoló nem forog", Severity::Alarm},
    {"METER2_STOPPED", "2. adagoló nem forog", Severity::Alarm},
    {"METER1_RATE", "1. adagoló: előírt mennyiség nem tartható", Severity::Alarm},
    {"METER2_RATE", "2. adagoló: előírt mennyiség nem tartható", Severity::Alarm},
    {"FAN_LOW", "Turbina fordulat alacsony", Severity::Alarm},
    {"FAN_HIGH", "Turbina fordulat magas", Severity::Warning},
    {"HOPPER1_EMPTY", "1. tartály kiürült", Severity::Alarm},
    {"HOPPER2_EMPTY", "2. tartály kiürült", Severity::Alarm},
    {"NO_SPEED", "Nincs sebességjel", Severity::Alarm},
    {"SPEED_FALLBACK", "ISOBUS sebesség kiesett, kerékjeladó", Severity::Warning},
    {"OIL_HOT", "Hidraulikaolaj túlmelegedett", Severity::Alarm},
    {"TEMP_SENSOR", "Olajhőmérő hiba", Severity::Warning},
    {"PRESSURE_SENSOR", "Nyomásszenzor hiba", Severity::Warning},
    {"SCALE_SENSOR", "Mérleg (HX711) nem válaszol", Severity::Warning},
    {"WHEEL_NOT_CAL", "Kerékjeladó nincs kalibrálva", Severity::Info},
    {"SIMULATION", "Szimuláció aktív", Severity::Info},
    {"TC_LOST", "Task Controller kapcsolat megszakadt", Severity::Warning},
};
static_assert(sizeof(ALARMS) / sizeof(ALARMS[0]) == static_cast<std::size_t>(AlarmId::COUNT), "alarm table");

void append_kv(std::string &o, const char *key, double v, int decimals) {
    char buf[80];
    std::snprintf(buf, sizeof(buf), "\"%s\":%.*f,", key, decimals, std::isfinite(v) ? v : 0.0);
    o += buf;
}
void append_kv(std::string &o, const char *key, bool v) {
    o += '"';
    o += key;
    o += "\":";
    o += v ? "true," : "false,";
}
void append_kv(std::string &o, const char *key, const char *v) {
    o += '"';
    o += key;
    o += "\":\"";
    o += json_escape(v);
    o += "\",";
}
void append_totals(std::string &o, const char *key, const Totals &t) {
    char buf[220];
    std::snprintf(buf, sizeof(buf), "\"%s\":{\"ha\":%.3f,\"km\":%.3f,\"h\":%.2f,\"kg1\":%.1f,\"kg2\":%.1f},", key,
                  t.area_m2 / 10000.0, t.distance_m / 1000.0, t.work_time_s / 3600.0, t.kg[0], t.kg[1]);
    o += buf;
}

/// Impulzusok előállítása egy frekvenciából (szimuláció).
PulseSample make_pulses(float hz, float dt, double &acc, double &since_us) {
    PulseSample p;
    since_us += static_cast<double>(dt) * 1e6;
    if (hz > 0.0f) {
        acc += static_cast<double>(hz) * dt;
        const double whole = std::floor(acc);
        if (whole >= 1.0) {
            p.count = static_cast<std::uint32_t>(whole);
            acc -= whole;
            since_us = acc / hz * 1e6;
        }
        p.last_period_us = static_cast<std::uint32_t>(1e6 / hz > 4e9 ? 4e9 : 1e6 / hz);
    }
    p.us_since_last = since_us > 4e9 ? 0xFFFFFFFFu : static_cast<std::uint32_t>(since_us);
    return p;
}

constexpr AlarmId METER_STOPPED[MAX_METERS] = {AlarmId::Meter1Stopped, AlarmId::Meter2Stopped};
constexpr AlarmId METER_RATE[MAX_METERS] = {AlarmId::Meter1Rate, AlarmId::Meter2Rate};
constexpr AlarmId HOPPER_EMPTY[2] = {AlarmId::Hopper1Empty, AlarmId::Hopper2Empty};
} // namespace

// ------------------------------------------------------------------------- címkék
const char *speed_source_label(SpeedSource s) {
    switch (s) {
    case SpeedSource::Wheel: return "Kerékjeladó";
    case SpeedSource::IsobusGround: return "ISOBUS föld (radar/GPS)";
    case SpeedSource::IsobusWheel: return "ISOBUS traktor kerék";
    case SpeedSource::IsobusMachine: return "ISOBUS gép által választott";
    case SpeedSource::Nmea: return "NMEA GPS";
    case SpeedSource::Simulated: return "Szimulált";
    default: return "Nincs";
    }
}

const char *sow_state_label(SowState s) {
    switch (s) {
    case SowState::Off: return "Vetés KI";
    case SowState::Raised: return "Gép kiemelve";
    case SowState::Standing: return "Áll";
    case SowState::Sowing: return "Vetés";
    case SowState::Prefill: return "Feltöltés";
    case SowState::Alarm: return "Riasztás";
    }
    return "";
}

const char *prefill_label(PrefillState s) {
    switch (s) {
    case PrefillState::Filling: return "Feltöltés";
    case PrefillState::AfterRun: return "START – indulhatsz";
    default: return "Nincs";
    }
}

void pressure_signal_range(int signal, float &v_min, float &v_max) {
    switch (signal) {
    case 1: v_min = 0.5f; v_max = 4.5f; break;
    case 2: v_min = 0.5f; v_max = 5.0f; break;
    case 3: v_min = 0.0f; v_max = 4.5f; break;
    default: v_min = 0.0f; v_max = 5.0f; break;
    }
}

const AlarmInfo &alarm_info(AlarmId id) {
    const auto i = static_cast<std::size_t>(id);
    return ALARMS[i < static_cast<std::size_t>(AlarmId::COUNT) ? i : 0];
}

void Alarms::set(AlarmId id, bool on) {
    const unsigned i = static_cast<unsigned>(id);
    const std::uint32_t bit = 1u << i;
    if (on && !(active_ & bit)) {
        unacked_ |= bit; // felfutó él: új, nyugtázatlan
        acked_for_[i] = 0.0f;
    }
    if (on) active_ |= bit;
    else {
        active_ &= ~bit;
        unacked_ &= ~bit;
    }
}

void Alarms::ack_all() {
    unacked_ = 0;
    for (auto &t : acked_for_) t = 0.0f;
}

void Alarms::tick(float dt, float realarm_s) {
    if (realarm_s <= 0.0f) return;
    for (unsigned i = 0; i < static_cast<unsigned>(AlarmId::COUNT); ++i) {
        const std::uint32_t bit = 1u << i;
        if (!(active_ & bit) || (unacked_ & bit) || ALARMS[i].severity != Severity::Alarm) continue;
        acked_for_[i] += dt;
        if (acked_for_[i] >= realarm_s) {
            unacked_ |= bit;
            acked_for_[i] = 0.0f;
        }
    }
}

AlarmId Alarms::top_unacked_alarm() const {
    const std::uint32_t m = unacked_mask();
    for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(AlarmId::COUNT); ++i)
        if ((m >> i) & 1u && ALARMS[i].severity == Severity::Alarm) return static_cast<AlarmId>(i);
    return AlarmId::COUNT;
}

// ------------------------------------------------------------------------- sebesség
SpeedResult select_speed(const Settings &s, float wheel_kmh, const IsobusSpeeds &iso) {
    SpeedResult r;
    const std::uint32_t timeout = static_cast<std::uint32_t>(s.speed_timeout_ms);
    auto ok = [timeout](const IsobusSpeedValue &v) { return v.valid && v.age_ms <= timeout; };
    switch (s.speed_src) {
    case 0:
        r.kmh = wheel_kmh;
        r.used = SpeedSource::Wheel;
        r.valid = s.wheel_ppm >= 1.0f;
        return r;
    case 3:
        r.kmh = s.sim_speed;
        r.used = SpeedSource::Simulated;
        r.valid = true;
        return r;
    default: break;
    }
    struct Cand {
        const IsobusSpeedValue *v;
        SpeedSource src;
    };
    Cand order[4];
    int n = 0;
    switch (s.isobus_speed_pref) {
    case 1: order[n++] = {&iso.ground, SpeedSource::IsobusGround}; break;
    case 2: order[n++] = {&iso.wheel, SpeedSource::IsobusWheel}; break;
    case 3: order[n++] = {&iso.machine, SpeedSource::IsobusMachine}; break;
    case 4: order[n++] = {&iso.nmea, SpeedSource::Nmea}; break;
    default:
        order[n++] = {&iso.machine, SpeedSource::IsobusMachine};
        order[n++] = {&iso.ground, SpeedSource::IsobusGround};
        order[n++] = {&iso.nmea, SpeedSource::Nmea};
        order[n++] = {&iso.wheel, SpeedSource::IsobusWheel};
        break;
    }
    for (int i = 0; i < n; ++i) {
        if (ok(*order[i].v)) {
            r.kmh = order[i].v->kmh < 0.0f ? 0.0f : order[i].v->kmh;
            r.used = order[i].src;
            r.valid = true;
            return r;
        }
    }
    if (s.speed_src == 2) {
        r.kmh = wheel_kmh;
        r.used = SpeedSource::Wheel;
        r.valid = s.wheel_ppm >= 1.0f;
        r.fallback = true;
        return r;
    }
    return r; // nincs érvényes jel: 0 km/h, valid=false
}

// ------------------------------------------------------------------------- kalibrálás
bool WheelCalibration::compute(float &ppm, const char **error) const {
    const char *dummy = nullptr;
    const char **e = error ? error : &dummy;
    ppm = 0.0f;
    if (pulses == 0) {
        *e = "Nincs számolt impulzus";
        return false;
    }
    if (distance_m < 10.0f) {
        *e = "A mérési távolság legalább 10 m legyen";
        return false;
    }
    ppm = static_cast<float>(pulses) * 100.0f / distance_m;
    if (ppm < 1.0f || ppm > 100000.0f) {
        *e = "Irreális eredmény";
        return false;
    }
    *e = "";
    return true;
}

// ------------------------------------------------------------------------- motor
MotorOutput MotorController::step(const Settings &s, float dt, float target_rpm, float actual_rpm) {
    MotorOutput o;
    const float down_rate = 100.0f / (s.ramp_down_s > 0.01f ? s.ramp_down_s : 0.01f);
    const float up_rate = 100.0f / (s.ramp_up_s > 0.01f ? s.ramp_up_s : 0.01f);
    if (target_rpm <= 0.01f) {
        run_time_ = 0.0f;
        out_ -= down_rate * dt;
        if (out_ < 0.0f) out_ = 0.0f;
        o.pwm_pct = out_;
        return o;
    }
    run_time_ += dt;
    const float max_rpm = s.motor_max_rpm > 0.1f ? s.motor_max_rpm : 0.1f;
    const float gain = static_cast<float>(s.reg_gain);
    const float ff = target_rpm / max_rpm * 100.0f;
    const float err_pct = (target_rpm - actual_rpm) / max_rpm * 100.0f;
    const float kp = 0.06f * gain;
    const float ki = 0.12f * gain;
    const float raw_cmd = ff + kp * err_pct + integral_;
    const bool sat_hi = raw_cmd >= s.pwm_max_pct && err_pct > 0.0f;
    const bool sat_lo = raw_cmd <= s.pwm_min_pct && err_pct < 0.0f;
    // integrálás csak felfutás után és nem a telítés irányába (anti-windup)
    if (run_time_ > 0.6f && !sat_hi && !sat_lo) integral_ = clampf(integral_ + ki * err_pct * dt, -60.0f, 60.0f);
    const float cmd = clampf(ff + kp * err_pct + integral_, s.pwm_min_pct, s.pwm_max_pct);
    if (cmd > out_) out_ = out_ + up_rate * dt > cmd ? cmd : out_ + up_rate * dt;
    else out_ = out_ - down_rate * dt < cmd ? cmd : out_ - down_rate * dt;
    o.pwm_pct = out_;
    o.saturated = out_ >= s.pwm_max_pct - 0.01f && (target_rpm - actual_rpm) / target_rpm > 0.03f;
    o.error_pct = (actual_rpm - target_rpm) / target_rpm * 100.0f;
    return o;
}

// ------------------------------------------------------------------------- vezérlő
Seeder::Seeder() : settings_(default_settings()) { update_crank_result(); }

void Seeder::load(const Settings &settings, const Persistent &persistent) {
    settings_ = settings;
    normalize(settings_);
    if (persistent.magic == Persistent::MAGIC && persistent.version == Persistent::VERSION) persistent_ = persistent;
    else persistent_ = Persistent{};
    for (int i = 0; i < MAX_METERS; ++i) {
        motor_[i].reset();
        motor_[i].restore(persistent_.motor_integral[i]);
        meter_rate_[i].reset();
    }
    cooler_manual_on_ = false;
    wheel_rate_.reset();
    fan_rate_.reset();
    master_ = false; // indításkor mindig KI: hiba / újraindulás ne indítson vetést
    prefill_ = PrefillState::Idle;
    crank_ = CrankCalibration{};
    update_crank_result();
}

bool Seeder::set_param(std::uint16_t id, float value) {
    const bool changed = apply_param(settings_, id, value);
    if (changed) {
        settings_dirty = true;
        if (id == P_drive_type || id == P_motor_max_rpm) {
            for (auto &m : motor_) m.reset();
            prefill_ = PrefillState::Idle;
            crank_.motor_running = false;
        }
        update_crank_result();
    }
    return changed;
}

bool Seeder::set_param_str(std::uint16_t id, const char *value) {
    if (!param_set_str(settings_, id, value)) return false;
    settings_dirty = true;
    return true;
}

void Seeder::replace_settings(const Settings &s) {
    const int old_drive = settings_.drive_type;
    settings_ = s;
    normalize(settings_);
    if (old_drive != settings_.drive_type) {
        for (auto &m : motor_) m.reset();
        prefill_ = PrefillState::Idle;
    }
    settings_dirty = true;
    update_crank_result();
}

// --- kezelőparancsok ---------------------------------------------------------------------
void Seeder::set_master(bool on) {
    if (master_ && !on) persistent_dirty = true;
    if (on && crank_.motor_running) crank_.motor_running = false;
    master_ = on;
    if (!on && prefill_ == PrefillState::AfterRun) prefill_ = PrefillState::Idle;
}

void Seeder::set_meter(int i, bool on) {
    if (i >= 0 && i < MAX_METERS) meter_enable_[i] = on;
}
void Seeder::toggle_meter(int i) {
    if (i >= 0 && i < meter_count(settings_)) meter_enable_[i] = !meter_enable_[i];
}

void Seeder::toggle_section_auto() {
    if (!settings_.tc_enable || !settings_.tc_sec) {
        say("A TC-SEC ki van kapcsolva a beállításokban");
        return;
    }
    if (!status_.tc_connected && !tc_section_auto_) {
        say("Nincs Task Controller kapcsolat");
        return;
    }
    tc_section_auto_ = !tc_section_auto_;
    say(tc_section_auto_ ? "Automata szakaszolás" : "Kézi szakaszolás");
}

void Seeder::rate_step(int dir) {
    const int next = rate_offset_steps_ + (dir > 0 ? 1 : -1);
    if (std::fabs(static_cast<float>(next) * settings_.rate_step_pct) <= 50.0f) rate_offset_steps_ = next;
}

void Seeder::toggle_lamp(int i) {
    if (i >= 0 && i < 2 && settings_.lights_enable) lamp_[i] = !lamp_[i];
}
void Seeder::toggle_valve() {
    if (settings_.valve_enable) valve_ = !valve_;
}
void Seeder::toggle_cooler_mode() {
    persistent_.cooler_manual = persistent_.cooler_manual ? 0 : 1;
    cooler_manual_on_ = cooler_on_; // átváltáskor a ventilátor állapota nem ugrik
    persistent_dirty = true;
}
void Seeder::toggle_cooler() {
    if (!persistent_.cooler_manual) {
        say("A hűtőventilátor automata módban van");
        return;
    }
    cooler_manual_on_ = !cooler_manual_on_;
}

bool Seeder::prefill_start() {
    if (!electric_drive(settings_)) {
        say("Feltöltés csak elektromos adagolóhajtással");
        return false;
    }
    if (crank_.motor_running) {
        say("Előbb állítsd le a leforgatást");
        return false;
    }
    prefill_ = settings_.prefill_time_s > 0.05f ? PrefillState::Filling : PrefillState::AfterRun;
    prefill_timer_ = prefill_ == PrefillState::Filling ? settings_.prefill_time_s : settings_.prefill_after_s;
    say("Csőfeltöltés");
    return true;
}
void Seeder::prefill_stop() { prefill_ = PrefillState::Idle; }

void Seeder::reset_day() {
    persistent_.day = Totals{};
    persistent_dirty = true;
}

// --- TC ---------------------------------------------------------------------------------
void Seeder::tc_set_section_setpoint(int i, bool on) {
    if (i >= 0 && i < MAX_SECTIONS) tc_setpoint_[i] = on;
}
void Seeder::tc_set_rate(int meter, float kg_ha) {
    if (meter < 0 || meter >= MAX_METERS) return;
    tc_rate_[meter] = kg_ha < 0.0f ? 0.0f : kg_ha;
    tc_rate_valid_ = true;
}
void Seeder::tc_clear_rate() { tc_rate_valid_ = false; }

// --- kerékkalibrálás -------------------------------------------------------------------------
void Seeder::wheel_cal_start() {
    const float d = wheel_cal_.distance_m;
    wheel_cal_ = WheelCalibration{};
    wheel_cal_.distance_m = d;
    wheel_cal_.state = CalState::Running;
}
void Seeder::wheel_cal_stop() {
    if (wheel_cal_.state == CalState::Running) wheel_cal_.state = CalState::Stopped;
}
void Seeder::wheel_cal_cancel() {
    const float d = wheel_cal_.distance_m;
    wheel_cal_ = WheelCalibration{};
    wheel_cal_.distance_m = d;
}
void Seeder::wheel_cal_set_pulses(std::uint32_t pulses) {
    if (wheel_cal_.state == CalState::Stopped) wheel_cal_.pulses = pulses;
}
void Seeder::wheel_cal_set_distance(float m) { wheel_cal_.distance_m = clampf(m, 10.0f, 1000.0f); }
bool Seeder::wheel_cal_save() {
    if (wheel_cal_.state != CalState::Stopped) return false;
    float ppm = 0.0f;
    if (!wheel_cal_.compute(ppm, nullptr)) return false;
    set_param(P_wheel_ppm, ppm);
    wheel_cal_cancel();
    return true;
}

// --- leforgatás --------------------------------------------------------------------------------
void Seeder::update_crank_result() {
    const int m = crank_.meter < meter_count(settings_) ? crank_.meter : 0;
    crank_result_ = chart::calculate(crank_.crop, rate_setting(settings_, m), crank_.turn_option, crank_.measured_kg,
                                     crank_.hectares, crank_.test_scale);
    status_.crank = crank_;
    status_.crank_result = crank_result_;
}
void Seeder::crank_set_meter(int meter) {
    if (crank_.motor_running) return;
    crank_.meter = meter >= 0 && meter < meter_count(settings_) ? meter : 0;
    crank_.meter_pulses = 0;
    update_crank_result();
}
void Seeder::crank_set_crop(int crop) {
    if (crop >= 0 && crop < chart::CROP_COUNT) crank_.crop = crop;
    update_crank_result();
}
void Seeder::crank_set_turns(int option) {
    if (option >= 0 && option < chart::TURN_OPTIONS) crank_.turn_option = option;
    update_crank_result();
}
void Seeder::crank_set_test_scale(float scale) {
    crank_.test_scale = clampf(scale, 0.0f, 200.0f);
    update_crank_result();
}
void Seeder::crank_set_measured(float kg) {
    crank_.measured_kg = clampf(kg, 0.0f, 500.0f);
    update_crank_result();
}
void Seeder::crank_set_hectares(float ha) {
    crank_.hectares = clampf(ha, 0.0f, 10000.0f);
    update_crank_result();
}
bool Seeder::crank_take_scale() {
    if (!settings_.scale_enable || !status_.scale.valid) {
        say("Nincs érvényes mérlegadat");
        return false;
    }
    crank_set_measured(status_.scale.net_kg > 0.0f ? status_.scale.net_kg : 0.0f);
    say("Tömeg átvéve a mérlegről");
    return true;
}
bool Seeder::crank_save() {
    update_crank_result();
    if (crank_result_.kg_per_rev <= 0.0) {
        say("Előbb add meg a lemért tömeget");
        return false;
    }
    const int m = crank_.meter < meter_count(settings_) ? crank_.meter : 0;
    set_param(m == 1 ? P_cal2_kg_rev : P_cal1_kg_rev, static_cast<float>(crank_result_.kg_per_rev));
    say("Kg/fordulat elmentve");
    return true;
}
bool Seeder::crank_motor_start() {
    if (!electric_drive(settings_)) {
        say("Motoros leforgatás csak elektromos hajtással (talajhajtásnál kézzel tekerj)");
        return false;
    }
    if (master_ || status_.moving) {
        say("Leforgatás csak álló gépnél, vetés KI állapotban");
        return false;
    }
    prefill_ = PrefillState::Idle;
    crank_.motor_running = true;
    crank_.motor_revs_done = 0.0f;
    crank_.meter_pulses = 0;
    motor_[crank_.meter].reset();
    say("Motoros leforgatás fut");
    return true;
}
void Seeder::crank_motor_stop() { crank_.motor_running = false; }
void Seeder::crank_reset_count() {
    crank_.meter_pulses = 0;
    crank_.motor_revs_done = 0.0f;
}

// --- mérleg, nyomás ----------------------------------------------------------------------------
void Seeder::scale_tare() {
    if (!scale_has_) {
        say("Nincs mérlegadat");
        return;
    }
    set_param(P_scale_offset, std::round(scale_filtered_));
    say("Mérleg nullázva (tára)");
}
bool Seeder::scale_calibrate(float known_kg) {
    if (!scale_has_ || known_kg <= 0.0f) {
        say("Tegyél fel ismert súlyt és add meg a tömegét");
        return false;
    }
    const float diff = scale_filtered_ - settings_.scale_offset;
    const float factor = diff / known_kg;
    if (!(std::fabs(factor) >= 1.0f)) {
        say("A mérleg nem érzékel terhelést");
        return false;
    }
    set_param(P_scale_factor, std::fabs(factor));
    if (factor < 0.0f) say("Kalibrálva (fordított bekötésű cella)");
    else say("Mérleg kalibrálva");
    return true;
}
bool Seeder::pressure_calibrate(float known_bar) {
    if (!settings_.pressure_enable || !status_.pressure_ok || pressure_raw_bar_ <= 0.5f) {
        say("Nincs érvényes nyomásjel (legyen nyomás alatt)");
        return false;
    }
    const float factor = (known_bar - settings_.pressure_zero_bar) / pressure_raw_bar_;
    if (factor < PARAMS[P_pressure_cal].min || factor > PARAMS[P_pressure_cal].max) {
        say("Túl nagy eltérés (ellenőrizd a méréshatárt és a jeltípust)");
        return false;
    }
    set_param(P_pressure_cal, factor);
    say("Nyomásmérés kalibrálva");
    return true;
}

// --- szimuláció ----------------------------------------------------------------------------------
void Seeder::update_simulation(TickInputs &in, float dt) {
    const Settings &S = settings_;
    in.fan = make_pulses(static_cast<float>(S.sim_fan_rpm) / 60.0f * static_cast<float>(S.fan_ppr), dt, sim_acc_[0],
                         sim_since_[0]);
    // munkakapcsoló: lent
    in.work_contact = S.work_sw != 2;
    // tartályok: tele
    in.hopper_contact[0] = S.hopper1_nc != 0;
    in.hopper_contact[1] = S.hopper2_nc != 0;
    const bool elec = electric_drive(S);
    for (int i = 0; i < MAX_METERS; ++i) {
        float rpm = 0.0f;
        if (elec) {
            // motor: a PWM-ből kissé lassabban forog, mint az előrecsatolás feltételezi (élethű eltérés)
            const float want = outputs_.motor_pwm_pct[i] / 100.0f * S.motor_max_rpm * 0.93f;
            sim_motor_rpm_[i] += (want - sim_motor_rpm_[i]) * dt / (0.25f + dt);
            rpm = sim_motor_rpm_[i];
        } else if (outputs_.clutch_drive[i] && status_.moving) {
            const float kg_rev = cal_kg_rev(S, i);
            rpm = kg_rev > 0.0f ? rate_setting(S, i) * status_.speed_kmh * meter_width(S, i) / 600.0f / kg_rev : 0.0f;
        }
        const float ppr = static_cast<float>(elec ? S.encoder_ppr : S.meter_ppr);
        in.meter[i] = make_pulses(rpm / 60.0f * ppr, dt, sim_acc_[1 + i], sim_since_[1 + i]);
    }
}

// --- ciklus ------------------------------------------------------------------------------------------
void Seeder::tick(const TickInputs &input) {
    TickInputs in = input;
    const float dt = clampf(in.dt_s, 0.001f, 0.5f);
    const Settings &S = settings_;
    uptime_s_ += dt;
    const int n_met = meter_count(S);
    const int n_sec = section_count(S);
    const int n_tank = tank_count(S);
    const bool elec = electric_drive(S);
    const bool invert = S.out_invert != 0;

    if (S.sim_inputs) update_simulation(in, dt);

    // --- sebesség
    PulseSample wheel = in.wheel;
    if (S.sim_wheel) wheel = make_pulses(S.sim_speed / 3.6f * S.wheel_ppm / 100.0f, dt, sim_acc_[3], sim_since_[3]);
    if (wheel_cal_.state == CalState::Running) wheel_cal_.pulses += wheel.count;
    const float wheel_hz = wheel_rate_.update(dt, wheel, 4.0f);
    const float wheel_kmh = S.wheel_ppm >= 1.0f ? wheel_hz / S.wheel_ppm * 100.0f * 3.6f : 0.0f;
    const SpeedResult sp = select_speed(S, wheel_kmh, in.isobus);
    const bool moving = sp.valid && sp.kmh >= S.min_work_speed;

    // --- munkakapcsoló (300 ms pergésszűrés)
    bool work_raw = true;
    if (S.work_sw == 1) work_raw = in.work_contact;
    else if (S.work_sw == 2) work_raw = !in.work_contact;
    if (S.work_sw == 0) {
        work_down_ = true;
        work_timer_ = 0.0f;
    } else if (work_raw != work_down_) {
        work_timer_ += dt;
        if (work_timer_ >= 0.3f) {
            work_down_ = work_raw;
            work_timer_ = 0.0f;
        }
    } else {
        work_timer_ = 0.0f;
    }

    // --- leürülésgátlók
    for (int i = 0; i < 2; ++i) {
        const bool nc = (i == 0 ? S.hopper1_nc : S.hopper2_nc) != 0;
        const bool empty_raw = i < n_tank && (nc ? !in.hopper_contact[i] : in.hopper_contact[i]);
        if (empty_raw != hopper_empty_[i]) {
            hopper_timer_[i] += dt;
            if (hopper_timer_[i] >= S.hopper_delay_s || i >= n_tank) {
                hopper_empty_[i] = empty_raw;
                hopper_timer_[i] = 0.0f;
            }
        } else {
            hopper_timer_[i] = 0.0f;
        }
    }

    // --- szakaszok (kézi + TC-SEC)
    const bool tc_auto = S.tc_enable && S.tc_sec && tc_section_auto_ && in.tc_connected;
    // TC kiesésekor a kezelő gombjai érvényesek (biztonságos visszaesés kézi módba)
    // automatában a munkakapcsoló csak akkor szól bele, ha így állították be
    const bool work_ok = (tc_auto && !S.work_sw_tc) ? true : work_down_;
    bool meter_on[MAX_METERS] = {false, false};
    for (int i = 0; i < n_met; ++i) {
        if (tc_auto) meter_on[i] = meter_enable_[i] && (n_sec == 2 ? tc_setpoint_[i] : tc_setpoint_[0]);
        else meter_on[i] = meter_enable_[i];
    }
    bool section_on[MAX_SECTIONS] = {false, false};
    if (n_sec == 2) {
        section_on[0] = meter_on[0];
        section_on[1] = meter_on[1];
    } else {
        section_on[0] = meter_on[0] || meter_on[1];
    }

    // --- csőfeltöltés (csak elektromos hajtás)
    if (!elec) prefill_ = PrefillState::Idle;
    const bool work_edge = S.work_sw != 0 && work_down_ && !last_work_down_ && master_;
    const bool master_edge = master_ && !last_master_ && work_down_;
    if (elec && S.prefill_auto && !crank_.motor_running && prefill_ == PrefillState::Idle && !moving &&
        (work_edge || master_edge))
        prefill_start();
    last_work_down_ = work_down_;
    last_master_ = master_;
    if (prefill_ == PrefillState::Filling) {
        prefill_timer_ -= dt;
        if (prefill_timer_ <= 0.0f) {
            prefill_ = PrefillState::AfterRun;
            prefill_timer_ = S.prefill_after_s;
        }
    } else if (prefill_ == PrefillState::AfterRun) {
        prefill_timer_ -= dt;
        // kiemeléskor az utánfutás megszakad
        if (prefill_timer_ <= 0.0f || (S.work_sw != 0 && !work_down_)) prefill_ = PrefillState::Idle;
    }
    const bool prefill_active = prefill_ != PrefillState::Idle;

    // --- előírt érték
    const bool use_tc_rate = S.tc_enable && S.tc_geo && tc_rate_valid_ && in.tc_connected;
    const float offset_pct = static_cast<float>(rate_offset_steps_) * S.rate_step_pct;
    const bool run_cond = (master_ && work_ok && moving) || prefill_active;
    const float v_eff = prefill_active ? maxf(moving ? sp.kmh : 0.0f, S.prefill_speed_kmh) : (moving ? sp.kmh : 0.0f);
    if (crank_.motor_running && (master_ || moving)) crank_.motor_running = false; // biztonság

    // --- adagolók
    Status &st = status_;
    float sown_w = 0.0f;
    bool any_running = false;
    for (int i = 0; i < MAX_METERS; ++i) {
        MeterStatus &m = st.meter[i];
        const bool exists = i < n_met;
        const float ppr = static_cast<float>(elec ? S.encoder_ppr : S.meter_ppr);
        const PulseSample &mp = in.meter[i];
        const float timeout = maxf(2.0f, S.meter_timeout_s);
        const float hz = meter_rate_[i].update(dt, mp, timeout);
        m.rpm = ppr > 0.0f ? hz * 60.0f / ppr : 0.0f;
        if (mp.count > 0) since_pulse_[i] = 0.0f;
        else since_pulse_[i] += dt;
        if (i == crank_.meter) crank_.meter_pulses += mp.count;
        const float revs = ppr > 0.0f ? static_cast<float>(mp.count) / ppr : 0.0f;

        m.enabled = meter_enable_[i];
        m.section_on = meter_on[i];
        m.width_m = meter_width(S, i);
        const float base = use_tc_rate ? tc_rate_[i] : rate_setting(S, i);
        m.target_rate = base * (1.0f + offset_pct / 100.0f);
        const float kg_rev = cal_kg_rev(S, i);
        const bool drive = exists && meter_on[i] && run_cond;

        float target_rpm = 0.0f;
        const bool crank_this = crank_.motor_running && crank_.meter == i;
        if (elec) {
            if (crank_this) {
                const double turns = chart::turns(crank_.turn_option);
                crank_.motor_revs_done += revs;
                const double left = turns - crank_.motor_revs_done;
                const float crank_rpm = S.motor_max_rpm * 0.5f < 40.0f ? S.motor_max_rpm * 0.5f : 40.0f;
                if (left <= 0.0) {
                    crank_.motor_running = false;
                    say("Leforgatás kész – mérd le a vetőmagot");
                } else {
                    target_rpm = left < 1.5 ? crank_rpm * 0.3f : crank_rpm;
                }
            } else if (drive && kg_rev > 0.0f) {
                target_rpm = m.target_rate * v_eff * m.width_m / 600.0f / kg_rev;
            }
            const MotorOutput mo = motor_[i].step(S, dt, target_rpm, m.rpm);
            outputs_.motor_pwm_pct[i] = exists ? mo.pwm_pct : 0.0f;
            outputs_.clutch_drive[i] = false;
            m.running = exists && (target_rpm > 0.0f || mo.pwm_pct > 0.5f);
            // eltérés figyelés (felfutás után)
            const bool dev = target_rpm > 0.5f && !crank_this && motor_[i].running_for() > 3.0f &&
                             (std::fabs(mo.error_pct) > S.rate_tol_pct || mo.saturated);
            rate_dev_timer_[i] = dev ? rate_dev_timer_[i] + dt : 0.0f;
            alarms_.set(METER_RATE[i], rate_dev_timer_[i] >= 5.0f);
            persistent_.motor_integral[i] = motor_[i].integral();
        } else {
            motor_[i].reset();
            outputs_.motor_pwm_pct[i] = 0.0f;
            outputs_.clutch_drive[i] = exists && meter_on[i] && master_ && work_ok;
            m.running = outputs_.clutch_drive[i] && moving;
            alarms_.set(METER_RATE[i], false);
            rate_dev_timer_[i] = 0.0f;
        }
        m.target_rpm = target_rpm;
        m.pwm_pct = outputs_.motor_pwm_pct[i];
        m.since_pulse_s = since_pulse_[i];

        // "nem forog" figyelés: vetés közben, felfutás után
        const bool expect = exists && !crank_this && (elec ? (drive && target_rpm > 0.5f) : (drive && moving));
        meter_run_time_[i] = expect ? meter_run_time_[i] + dt : 0.0f;
        const bool stopped = expect && meter_run_time_[i] > 1.0f && since_pulse_[i] >= S.meter_timeout_s;
        alarms_.set(METER_STOPPED[i], stopped);

        // mennyiség elszámolása
        m.kg_per_min = m.rpm * kg_rev;
        const bool counting = exists && !crank_this && (elec ? drive : outputs_.clutch_drive[i]);
        if (counting && revs > 0.0f) {
            const double kg = static_cast<double>(revs) * kg_rev;
            persistent_.day.kg[i] += kg;
            persistent_.total.kg[i] += kg;
        }
        const float area_rate = sp.kmh * m.width_m / 600.0f; // ha/perc
        m.actual_rate = (m.running && moving && area_rate > 0.0f) ? m.kg_per_min / area_rate : 0.0f;
        const bool sowing_now = exists && m.running && moving;
        if (sowing_now) any_running = true;
        if (sowing_now && S.meter_layout == 0 && n_met == 2) sown_w += m.width_m;

        if (!exists) m.view = MeterView::Idle;
        else if (alarms_.active(METER_STOPPED[i]) || alarms_.active(METER_RATE[i])) m.view = MeterView::Alarm;
        else if (!meter_on[i]) m.view = MeterView::ShutDown;
        else if (m.running && (moving || prefill_active || crank_this)) m.view = MeterView::Work;
        else m.view = master_ ? MeterView::Ready : MeterView::Idle;
    }
    if (any_running && (n_met == 1 || S.meter_layout == 1)) sown_w = S.width_m;

    // --- turbina
    const float fan_hz = fan_rate_.update(dt, in.fan, 2.0f);
    const float fan_rpm = S.fan_ppr > 0 ? fan_hz * 60.0f / static_cast<float>(S.fan_ppr) : 0.0f;
    const bool fan_watch = S.work_sw != 0 ? (work_down_ && moving) : (moving && master_);
    fan_low_timer_ = (fan_watch && S.fan_min_rpm > 0 && fan_rpm < static_cast<float>(S.fan_min_rpm))
                         ? fan_low_timer_ + dt : 0.0f;
    fan_high_timer_ = (fan_watch && S.fan_max_rpm > 0 && fan_rpm > static_cast<float>(S.fan_max_rpm))
                          ? fan_high_timer_ + dt : 0.0f;
    alarms_.set(AlarmId::FanLow, fan_low_timer_ >= S.fan_delay_s && fan_watch);
    alarms_.set(AlarmId::FanHigh, fan_high_timer_ >= S.fan_delay_s && fan_watch);

    // --- nyomás
    st.pressure_ok = false;
    if (S.pressure_enable) {
        if (in.pressure_v >= 0.0f) {
            pressure_filtered_ = pressure_filtered_ < 0.0f ? in.pressure_v
                                                           : pressure_filtered_ + (in.pressure_v - pressure_filtered_) *
                                                                                      dt / (0.3f + dt);
            float vmin = 0.0f, vmax = 5.0f;
            pressure_signal_range(S.pressure_signal, vmin, vmax);
            const float vs = pressure_filtered_ * S.pressure_divider;
            pressure_raw_bar_ = (vs - vmin) / (vmax - vmin) * S.pressure_range_bar;
            const bool fault = (vmin >= 0.4f && vs < vmin - 0.3f) || vs > vmax + 0.3f;
            st.pressure_ok = !fault;
            st.pressure_v = vs;
            st.pressure_bar = maxf(0.0f, pressure_raw_bar_ * S.pressure_cal + S.pressure_zero_bar);
            alarms_.set(AlarmId::PressureSensor, fault);
        } else {
            alarms_.set(AlarmId::PressureSensor, true);
        }
    } else {
        alarms_.set(AlarmId::PressureSensor, false);
        pressure_filtered_ = -1.0f;
    }

    // --- olajhőmérséklet, hűtőventilátor
    bool temp_any = false;
    float temp_max = -1000.0f;
    for (int i = 0; i < 2; ++i) {
        st.temp_ok[i] = S.temp_enable && in.temp_c[i] > -100.0f && in.temp_c[i] < 200.0f;
        st.temp_c[i] = st.temp_ok[i] ? in.temp_c[i] : 0.0f;
        if (st.temp_ok[i]) {
            temp_any = true;
            temp_max = maxf(temp_max, in.temp_c[i]);
        }
    }
    if (S.temp_enable) {
        float ctrl = -1000.0f;
        if (S.cooler_src == 0) ctrl = st.temp_ok[0] ? st.temp_c[0] : (st.temp_ok[1] ? st.temp_c[1] : -1000.0f);
        else if (S.cooler_src == 1) ctrl = st.temp_ok[1] ? st.temp_c[1] : (st.temp_ok[0] ? st.temp_c[0] : -1000.0f);
        else ctrl = temp_max;
        if (persistent_.cooler_manual) {
            cooler_on_ = cooler_manual_on_;
        } else if (ctrl < -900.0f) {
            cooler_on_ = true; // nincs hőmérő: biztonságból hűt
        } else if (ctrl >= S.cooler_on_c) {
            cooler_on_ = true;
        } else if (ctrl <= S.cooler_off_c) {
            cooler_on_ = false;
        }
        alarms_.set(AlarmId::TempSensor, !st.temp_ok[0] || !st.temp_ok[1]);
        alarms_.set(AlarmId::OilHot, S.temp_alarm_c > 0.0f && temp_any && temp_max >= S.temp_alarm_c);
    } else {
        cooler_on_ = false;
        alarms_.set(AlarmId::TempSensor, false);
        alarms_.set(AlarmId::OilHot, false);
    }

    // --- mérleg
    if (S.scale_enable) {
        if (in.scale_valid) {
            scale_filtered_ = scale_has_ ? scale_filtered_ + (in.scale_raw - scale_filtered_) * dt / (0.5f + dt)
                                         : in.scale_raw;
            // nagy ugrásnál (rátétel) gyorsabban kövessen
            if (std::fabs(in.scale_raw - scale_filtered_) > S.scale_factor * 2.0f) scale_filtered_ = in.scale_raw;
            scale_has_ = true;
            scale_timeout_ = 0.0f;
        } else {
            scale_timeout_ += dt;
        }
        if (scale_timeout_ > 3.0f) scale_has_ = false;
        st.scale.valid = scale_has_;
        st.scale.raw = scale_filtered_;
        st.scale.net_kg = scale_has_ && S.scale_factor > 0.0f ? (scale_filtered_ - S.scale_offset) / S.scale_factor : 0.0f;
        st.scale.overload = scale_has_ && st.scale.net_kg > S.scale_capacity_kg * 1.05f;
        alarms_.set(AlarmId::ScaleSensor, !scale_has_);
    } else {
        st.scale = ScaleState{};
        scale_has_ = false;
        alarms_.set(AlarmId::ScaleSensor, false);
    }

    // --- egyéb riasztások
    for (int i = 0; i < 2; ++i) alarms_.set(HOPPER_EMPTY[i], i < n_tank && hopper_empty_[i]);
    alarms_.set(AlarmId::NoSpeed, master_ && !sp.valid);
    alarms_.set(AlarmId::SpeedFallback, sp.fallback && master_);
    const bool wheel_used = S.speed_src == 0 || sp.used == SpeedSource::Wheel;
    alarms_.set(AlarmId::WheelNotCalibrated, wheel_used && S.wheel_ppm == PARAMS[P_wheel_ppm].def);
    alarms_.set(AlarmId::SimulationActive, S.sim_inputs || S.sim_wheel || S.speed_src == 3);
    alarms_.set(AlarmId::TcLost, S.tc_enable && S.tc_sec && tc_section_auto_ && !in.tc_connected);
    alarms_.tick(dt, static_cast<float>(S.realarm_s));

    // --- elszámolás
    const double dist = sp.valid ? static_cast<double>(sp.kmh) / 3.6 * dt : 0.0;
    persistent_.day.distance_m += dist;
    persistent_.total.distance_m += dist;
    const bool sowing = master_ && work_ok && moving && sown_w > 0.0f;
    if (sowing) {
        persistent_.day.area_m2 += dist * sown_w;
        persistent_.total.area_m2 += dist * sown_w;
        persistent_.day.work_time_s += dt;
        persistent_.total.work_time_s += dt;
    }
    persist_timer_ += dt;
    if (persist_timer_ >= 30.0f && (sowing || dist > 0.0)) {
        persist_timer_ = 0.0f;
        persistent_dirty = true;
    }

    // --- kimenetek (fizikai szint)
    for (int i = 0; i < MAX_METERS; ++i) {
        const bool energized = elec ? false : (S.clutch_mode == 0 ? outputs_.clutch_drive[i] : !outputs_.clutch_drive[i]);
        outputs_.clutch_level[i] = energized != invert;
    }
    for (int i = 0; i < 2; ++i) outputs_.lamp_level[i] = (S.lights_enable && lamp_[i]) != invert;
    outputs_.valve_level = (S.valve_enable && valve_) != invert;
    outputs_.cooler_level = (S.temp_enable && cooler_on_) != invert;

    // --- állapot
    st.speed_kmh = sp.kmh;
    st.speed_src = sp.used;
    st.speed_valid = sp.valid;
    st.speed_fallback = sp.fallback;
    st.wheel_hz = wheel_hz;
    st.moving = moving;
    st.work_down = work_down_;
    st.work_contact = in.work_contact;
    st.master = master_;
    st.meter_count = n_met;
    st.section_count = n_sec;
    st.tank_count = n_tank;
    for (int i = 0; i < MAX_SECTIONS; ++i) {
        st.section_on[i] = section_on[i] && master_ && work_ok;
        st.tc_setpoint[i] = tc_setpoint_[i];
    }
    st.tc_section_auto = tc_auto;
    st.sown_width_m = sowing ? sown_w : 0.0f;
    st.rate_offset_pct = offset_pct;
    st.tc_rate = use_tc_rate;
    st.prefill = prefill_;
    st.prefill_left_s = prefill_active ? (prefill_timer_ > 0.0f ? prefill_timer_ : 0.0f) : 0.0f;
    st.fan_rpm = fan_rpm;
    for (int i = 0; i < 2; ++i) {
        st.hopper_empty[i] = i < n_tank && hopper_empty_[i];
        st.hopper_contact[i] = in.hopper_contact[i];
        st.lamp[i] = S.lights_enable && lamp_[i];
    }
    st.valve_marker = S.valve_enable && valve_;
    st.cooler_on = S.temp_enable && cooler_on_;
    st.cooler_manual = persistent_.cooler_manual != 0;
    st.wheel_cal = wheel_cal_;
    st.wheel_cal_ok = wheel_cal_.compute(st.wheel_cal_ppm, &st.wheel_cal_error);
    update_crank_result();
    st.crank = crank_;
    st.crank_result = crank_result_;
    st.tc_connected = in.tc_connected;
    st.alarms_active = alarms_.active_mask();
    st.alarms_unacked = alarms_.unacked_mask();
    st.top_alarm = alarms_.top_unacked_alarm();
    st.uptime_s = static_cast<std::uint32_t>(uptime_s_);
    st.message = message_;
    st.message_seq = message_seq_;

    bool meter_alarm = false;
    for (int i = 0; i < n_met; ++i)
        meter_alarm |= alarms_.active(METER_STOPPED[i]) || alarms_.active(METER_RATE[i]);
    if (prefill_active) st.state = SowState::Prefill;
    else if (!master_) st.state = SowState::Off;
    else if (S.work_sw != 0 && !work_down_) st.state = SowState::Raised;
    else if (!moving) st.state = SowState::Standing;
    else if (meter_alarm || alarms_.active(AlarmId::FanLow)) st.state = SowState::Alarm;
    else st.state = SowState::Sowing;
}

// --- JSON ---------------------------------------------------------------------------------------------
std::string Seeder::status_json() const {
    const Status &st = status_;
    std::string o;
    o.reserve(3072);
    o += '{';
    append_kv(o, "speed", st.speed_kmh, 2);
    append_kv(o, "speed_src", speed_source_label(st.speed_src));
    append_kv(o, "speed_valid", st.speed_valid);
    append_kv(o, "speed_fallback", st.speed_fallback);
    append_kv(o, "wheel_hz", st.wheel_hz, 2);
    append_kv(o, "moving", st.moving);
    append_kv(o, "work_down", st.work_down);
    append_kv(o, "work_contact", st.work_contact);
    append_kv(o, "master", st.master);
    append_kv(o, "state", sow_state_label(st.state));
    append_kv(o, "state_id", static_cast<double>(st.state), 0);
    append_kv(o, "meters", static_cast<double>(st.meter_count), 0);
    append_kv(o, "sections", static_cast<double>(st.section_count), 0);
    append_kv(o, "tanks", static_cast<double>(st.tank_count), 0);
    append_kv(o, "electric", electric_drive(settings_));
    o += "\"meter\":[";
    for (int i = 0; i < MAX_METERS; ++i) {
        const MeterStatus &m = st.meter[i];
        if (i) o += ',';
        o += '{';
        append_kv(o, "enabled", m.enabled);
        append_kv(o, "section_on", m.section_on);
        append_kv(o, "running", m.running);
        append_kv(o, "rpm", m.rpm, 2);
        append_kv(o, "target_rpm", m.target_rpm, 2);
        append_kv(o, "pwm", m.pwm_pct, 1);
        append_kv(o, "target_rate", m.target_rate, 1);
        append_kv(o, "actual_rate", m.actual_rate, 1);
        append_kv(o, "kg_min", m.kg_per_min, 3);
        append_kv(o, "width", m.width_m, 2);
        append_kv(o, "since_pulse", m.since_pulse_s, 1);
        append_kv(o, "view", static_cast<double>(m.view), 0);
        append_kv(o, "clutch", outputs_.clutch_drive[i]);
        o.back() = '}';
    }
    o += "],";
    o += "\"section_on\":[";
    o += st.section_on[0] ? "true," : "false,";
    o += st.section_on[1] ? "true" : "false";
    o += "],\"tc_setpoint\":[";
    o += st.tc_setpoint[0] ? "true," : "false,";
    o += st.tc_setpoint[1] ? "true" : "false";
    o += "],";
    append_kv(o, "sec_auto", st.tc_section_auto);
    append_kv(o, "sec_auto_req", tc_section_auto_);
    append_kv(o, "sown_w", st.sown_width_m, 2);
    append_kv(o, "rate_offset", st.rate_offset_pct, 0);
    append_kv(o, "tc_rate", st.tc_rate);
    append_kv(o, "prefill", prefill_label(st.prefill));
    append_kv(o, "prefill_id", static_cast<double>(st.prefill), 0);
    append_kv(o, "prefill_left", st.prefill_left_s, 1);
    append_kv(o, "fan_rpm", st.fan_rpm, 0);
    o += "\"hopper_empty\":[";
    o += st.hopper_empty[0] ? "true," : "false,";
    o += st.hopper_empty[1] ? "true" : "false";
    o += "],\"hopper_contact\":[";
    o += st.hopper_contact[0] ? "true," : "false,";
    o += st.hopper_contact[1] ? "true" : "false";
    o += "],\"lamp\":[";
    o += st.lamp[0] ? "true," : "false,";
    o += st.lamp[1] ? "true" : "false";
    o += "],";
    append_kv(o, "valve_marker", st.valve_marker);
    append_kv(o, "pressure_ok", st.pressure_ok);
    append_kv(o, "pressure_bar", st.pressure_bar, 1);
    append_kv(o, "pressure_v", st.pressure_v, 3);
    o += "\"temp\":[";
    char tb[64];
    std::snprintf(tb, sizeof(tb), "%s,%s", st.temp_ok[0] ? std::to_string(st.temp_c[0]).c_str() : "null",
                  st.temp_ok[1] ? std::to_string(st.temp_c[1]).c_str() : "null");
    o += tb;
    o += "],";
    append_kv(o, "cooler_on", st.cooler_on);
    append_kv(o, "cooler_manual", st.cooler_manual);
    o += "\"scale\":{";
    append_kv(o, "valid", st.scale.valid);
    append_kv(o, "raw", st.scale.raw, 0);
    append_kv(o, "kg", st.scale.net_kg, 3);
    append_kv(o, "overload", st.scale.overload);
    o.back() = '}';
    o += ',';
    static const char *const cal_states[] = {"Idle", "Running", "Stopped"};
    o += "\"wheel_cal\":{";
    append_kv(o, "state", cal_states[static_cast<int>(st.wheel_cal.state)]);
    append_kv(o, "pulses", static_cast<double>(st.wheel_cal.pulses), 0);
    append_kv(o, "distance", st.wheel_cal.distance_m, 1);
    append_kv(o, "ppm", st.wheel_cal_ppm, 1);
    append_kv(o, "ok", st.wheel_cal_ok);
    append_kv(o, "error", st.wheel_cal_error);
    o.back() = '}';
    o += ",\"crank\":{";
    const auto &c = st.crank;
    const auto &r = st.crank_result;
    append_kv(o, "meter", static_cast<double>(c.meter), 0);
    append_kv(o, "crop", static_cast<double>(c.crop), 0);
    append_kv(o, "crop_name", chart::crop_name(c.crop));
    append_kv(o, "turn_option", static_cast<double>(c.turn_option), 0);
    append_kv(o, "turns", chart::turns(c.turn_option), 1);
    append_kv(o, "wanted", rate_setting(settings_, c.meter), 1);
    append_kv(o, "test_scale", c.test_scale, 1);
    append_kv(o, "measured", c.measured_kg, 3);
    append_kv(o, "hectares", c.hectares, 2);
    append_kv(o, "motor_running", c.motor_running);
    append_kv(o, "motor_revs", c.motor_revs_done, 2);
    const float cppr = static_cast<float>(electric_drive(settings_) ? settings_.encoder_ppr : settings_.meter_ppr);
    append_kv(o, "counted_revs", cppr > 0 ? static_cast<double>(c.meter_pulses) / cppr : 0.0, 2);
    append_kv(o, "scale", r.scale, 1);
    append_kv(o, "setup", chart::setup_text(r));
    append_kv(o, "expected_kg", r.expected_kg, 3);
    append_kv(o, "corrected_valid", r.corrected_valid);
    append_kv(o, "corrected_scale", r.corrected_scale, 1);
    append_kv(o, "actual_rate", r.actual_rate, 1);
    append_kv(o, "kg_per_rev", r.kg_per_rev, 4);
    append_kv(o, "seed_kg", r.seed_kg, 0);
    append_kv(o, "note", chart::note_text(chart::note(r, c.measured_kg > 0)));
    o.back() = '}';
    o += ',';
    append_totals(o, "day", persistent_.day);
    append_totals(o, "total", persistent_.total);
    append_kv(o, "clutch_level", outputs_.clutch_level[0]);
    append_kv(o, "clutch2_level", outputs_.clutch_level[1]);
    append_kv(o, "lamp1_level", outputs_.lamp_level[0]);
    append_kv(o, "lamp2_level", outputs_.lamp_level[1]);
    append_kv(o, "valve_level", outputs_.valve_level);
    append_kv(o, "cooler_level", outputs_.cooler_level);
    append_kv(o, "pwm1", outputs_.motor_pwm_pct[0], 1);
    append_kv(o, "pwm2", outputs_.motor_pwm_pct[1], 1);
    append_kv(o, "tc", st.tc_connected);
    append_kv(o, "alarms", static_cast<double>(st.alarms_active), 0);
    append_kv(o, "alarms_new", static_cast<double>(st.alarms_unacked), 0);
    o += "\"alarm_list\":[";
    bool first = true;
    for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(AlarmId::COUNT); ++i) {
        if (!((st.alarms_active >> i) & 1u)) continue;
        const AlarmInfo &a = alarm_info(static_cast<AlarmId>(i));
        char buf[220];
        std::snprintf(buf, sizeof(buf), "%s{\"code\":\"%s\",\"text\":\"%s\",\"sev\":%d,\"new\":%s}",
                      first ? "" : ",", a.code, json_escape(a.text).c_str(), static_cast<int>(a.severity),
                      ((st.alarms_unacked >> i) & 1u) ? "true" : "false");
        o += buf;
        first = false;
    }
    o += "],";
    append_kv(o, "message", st.message);
    append_kv(o, "message_seq", static_cast<double>(st.message_seq), 0);
    append_kv(o, "uptime", static_cast<double>(st.uptime_s), 0);
    o.back() = '}';
    return o;
}

} // namespace seeder

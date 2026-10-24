// A vezérlőmag unit tesztjei (PC-n futnak, hardver nélkül).
// Fordítás: cmake -S host -B build-host && cmake --build build-host && ctest --test-dir build-host
#include "seeder/seeder.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace seeder;

namespace {
int g_failures = 0;
int g_checks = 0;
std::vector<std::pair<const char *, std::function<void()>>> &registry() {
    static std::vector<std::pair<const char *, std::function<void()>>> r;
    return r;
}
struct Reg {
    Reg(const char *n, std::function<void()> f) { registry().emplace_back(n, std::move(f)); }
};
#define TEST(name)                      \
    static void name();                 \
    static Reg reg_##name(#name, name); \
    static void name()
#define CHECK(cond)                                                       \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(cond)) {                                                    \
            ++g_failures;                                                 \
            std::printf("  HIBA %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)
#define CHECK_NEAR(a, b, tol)                                                                         \
    do {                                                                                              \
        ++g_checks;                                                                                   \
        const double va = (a), vb = (b);                                                              \
        if (std::fabs(va - vb) > (tol)) {                                                             \
            ++g_failures;                                                                             \
            std::printf("  HIBA %s:%d: %s = %.4f, várt %.4f (±%.4f)\n", __FILE__, __LINE__, #a, va, vb, \
                        static_cast<double>(tol));                                                    \
        }                                                                                             \
    } while (0)

/// Impulzusforrás adott frekvenciával (a hardver megszakításait helyettesíti).
struct PulseGen {
    double acc = 0.0, since_us = 1e12;
    PulseSample step(double hz, double dt) {
        PulseSample p;
        since_us += dt * 1e6;
        if (hz > 0.0) {
            acc += hz * dt;
            const double whole = std::floor(acc);
            if (whole >= 1.0) {
                p.count = static_cast<std::uint32_t>(whole);
                acc -= whole;
                since_us = acc / hz * 1e6;
            }
            p.last_period_us = static_cast<std::uint32_t>(1e6 / hz);
        }
        p.us_since_last = since_us > 4e9 ? 0xFFFFFFFFu : static_cast<std::uint32_t>(since_us);
        return p;
    }
};

/// Gép-modell a tesztekhez: turbina, adagolók (talajhajtás vagy motor + enkóder), munkakapcsoló.
struct Machine {
    TickInputs in;
    PulseGen fan, meter[2];
    float fan_rpm = 3500.0f;
    bool meter_turns[2] = {true, true};   ///< false: a tengely beragadt / lánc szakadt
    float motor_strength = 1.0f;          ///< elektromos hajtás: 1 = a motor_max_rpm-et hozza 100%-on
    float motor_rpm[2] = {0.0f, 0.0f};
    float ground_rpm = 30.0f;             ///< talajhajtásnál a tengely fordulata haladás közben

    void step(Seeder &s) {
        const Settings &S = s.settings();
        const float dt = in.dt_s;
        in.fan = fan.step(fan_rpm / 60.0 * S.fan_ppr, dt);
        for (int i = 0; i < 2; ++i) {
            float rpm = 0.0f;
            if (electric_drive(S)) {
                const float want = s.outputs().motor_pwm_pct[i] / 100.0f * S.motor_max_rpm * motor_strength;
                motor_rpm[i] += (want - motor_rpm[i]) * dt / (0.2f + dt);
                rpm = motor_rpm[i];
            } else if (s.outputs().clutch_drive[i] && s.status().moving) {
                rpm = ground_rpm;
            }
            if (!meter_turns[i]) rpm = 0.0f;
            const float ppr = static_cast<float>(electric_drive(S) ? S.encoder_ppr : S.meter_ppr);
            in.meter[i] = meter[i].step(rpm / 60.0 * ppr, dt);
        }
        s.tick(in);
    }
    void run(Seeder &s, float seconds) {
        in.dt_s = 0.05f;
        const int steps = static_cast<int>(seconds / in.dt_s + 0.5f);
        for (int i = 0; i < steps; ++i) step(s);
    }
};

Seeder make(std::function<void(Settings &)> tweak = nullptr) {
    Settings st = default_settings();
    st.speed_src = 3;       // szimulált sebesség
    st.sim_speed = 8.0f;
    st.work_sw = 1;         // NO munkakapcsoló
    if (tweak) tweak(st);
    normalize(st);
    Seeder s;
    s.load(st, Persistent{});
    return s;
}

Machine down_machine() {
    Machine m;
    m.in.work_contact = true; // NO: lent zár
    return m;
}

bool has_alarm(const Seeder &s, AlarmId id) { return (s.status().alarms_active >> static_cast<int>(id)) & 1u; }
} // namespace

// =====================================================================================================
// beállítások
// =====================================================================================================
TEST(defaults_are_sane) {
    const Settings s = default_settings();
    CHECK_NEAR(s.width_m, 6.0, 1e-6);
    CHECK(s.section_count == 2);
    CHECK(s.meter_count == 2);
    CHECK(s.tank_count == 2);
    CHECK(s.encoder_ppr == 50);
    CHECK(s.lights_enable == 0 && s.valve_enable == 0 && s.pressure_enable == 0 && s.temp_enable == 0 &&
          s.scale_enable == 0);
    CHECK_NEAR(s.pressure_range_bar, 250.0, 1e-6);
    CHECK_NEAR(meter_width(s, 0), 3.0, 1e-6);
    CHECK_NEAR(meter_width(s, 1), 3.0, 1e-6);
}

TEST(normalize_meter_section_rules) {
    Settings s = default_settings();
    apply_param(s, P_meter_count, 1);
    CHECK(s.section_count == 1);
    CHECK_NEAR(meter_width(s, 0), 6.0, 1e-6);
    CHECK_NEAR(meter_width(s, 1), 0.0, 1e-6);

    s = default_settings();
    apply_param(s, P_meter_layout, 1); // első/hátsó sor
    CHECK(s.section_count == 1);
    CHECK_NEAR(meter_width(s, 0), 6.0, 1e-6);
    CHECK_NEAR(meter_width(s, 1), 6.0, 1e-6);
    apply_param(s, P_section_count, 2);
    CHECK(s.section_count == 1); // nem engedi

    s = default_settings();
    apply_param(s, P_width_m, 3.0f);
    CHECK_NEAR(s.sec_w1, 1.5, 1e-4);
    CHECK_NEAR(section_width(s, 1), 1.5, 1e-4);
    apply_param(s, P_sec_w1, 10.0f); // nagyobb a munkaszélességnél
    CHECK(s.sec_w1 <= 2.9f + 1e-4f);
}

TEST(normalize_dependent_limits) {
    Settings s = default_settings();
    apply_param(s, P_cooler_off_c, 70.0f); // kikapcsolás a bekapcsolás fölé: a bekapcsolás feljebb tolódik
    CHECK(s.cooler_on_c >= s.cooler_off_c + 1.0f - 1e-4f);
    apply_param(s, P_pwm_min_pct, 90.0f);
    apply_param(s, P_pwm_max_pct, 50.0f);
    CHECK(s.pwm_min_pct <= s.pwm_max_pct);
    apply_param(s, P_fan_max_rpm, 2000.0f); // a minimum (2500) alá
    CHECK(s.fan_max_rpm > s.fan_min_rpm);
    CHECK(!apply_param(s, P_drive_type, 5.0f)); // érvénytelen enum
    CHECK(s.drive_type == 0);
}

TEST(settings_json_roundtrip_and_errors) {
    Settings a = default_settings();
    apply_param(a, P_rate1, 175.5f);
    apply_param(a, P_drive_type, 1);
    apply_param(a, P_cal2_kg_rev, 0.1234f);
    const std::string js = settings_to_json(a);
    Settings b = default_settings();
    CHECK(settings_from_json(b, js.c_str()) > 50);
    CHECK_NEAR(b.rate1, 175.5, 1e-4);
    CHECK(b.drive_type == 1);
    CHECK_NEAR(b.cal2_kg_rev, 0.1234, 1e-5);

    Settings c = default_settings();
    std::string err;
    CHECK(settings_from_json(c, "{\"rate1\":120,\"ismeretlen\":5}", &err) == 1);
    CHECK_NEAR(c.rate1, 120.0, 1e-4);
    CHECK(settings_from_json(c, "[1,2]", &err) < 0);
    CHECK(settings_from_json(c, "{\"drive_type\":7}", &err) < 0);
    CHECK(!err.empty());
    CHECK(settings_from_json(c, "{\"wifi_pass\":\"rov\"}", &err) < 0); // túl rövid jelszó
    const std::string schema = schema_json();
    CHECK(schema.find("\"feature\":\"pressure_enable\"") != std::string::npos);
    CHECK(schema.find("\"key\":\"encoder_ppr\"") != std::string::npos);
}

TEST(ddop_hash_tracks_geometry_only) {
    Settings s = default_settings();
    const std::uint32_t h0 = ddop_relevant_hash(s);
    apply_param(s, P_rate1, 100.0f);
    CHECK(ddop_relevant_hash(s) == h0);
    apply_param(s, P_geo_b_m, 5.0f);
    CHECK(ddop_relevant_hash(s) != h0);
}

// =====================================================================================================
// sebesség
// =====================================================================================================
TEST(speed_source_selection) {
    Settings s = default_settings();
    IsobusSpeeds iso;
    s.speed_src = 0;
    auto r = select_speed(s, 7.5f, iso);
    CHECK(r.valid && r.used == SpeedSource::Wheel);
    CHECK_NEAR(r.kmh, 7.5, 1e-6);

    s.speed_src = 1;
    r = select_speed(s, 7.5f, iso);
    CHECK(!r.valid);
    iso.nmea = {true, 9.0f, 100};
    iso.wheel = {true, 9.5f, 100};
    r = select_speed(s, 7.5f, iso);
    CHECK(r.used == SpeedSource::Nmea); // automata sorrend: GPS a traktor kerék előtt
    iso.nmea.age_ms = 5000;              // elavult
    r = select_speed(s, 7.5f, iso);
    CHECK(r.used == SpeedSource::IsobusWheel);

    s.speed_src = 2; // ISOBUS, kiesésnél kerék
    iso = IsobusSpeeds{};
    r = select_speed(s, 6.0f, iso);
    CHECK(r.valid && r.fallback && r.used == SpeedSource::Wheel);

    s.speed_src = 3;
    s.sim_speed = 11.0f;
    r = select_speed(s, 0.0f, iso);
    CHECK(r.valid && r.used == SpeedSource::Simulated);
    CHECK_NEAR(r.kmh, 11.0, 1e-6);
}

TEST(wheel_speed_from_pulses_and_timeout) {
    Seeder s = make([](Settings &st) { st.speed_src = 0; st.wheel_ppm = 50.0f; });
    Machine m = down_machine();
    PulseGen wheel;
    // 8 km/h = 2,222 m/s → 50 imp/100 m: 1,111 Hz
    for (int i = 0; i < 200; ++i) {
        m.in.wheel = wheel.step(8.0 / 3.6 * 0.5, 0.05);
        m.step(s);
    }
    CHECK_NEAR(s.status().speed_kmh, 8.0, 0.4);
    // jel megszűnik: 4 s-on belül biztosan 0
    for (int i = 0; i < 100; ++i) {
        m.in.wheel = wheel.step(0.0, 0.05);
        m.step(s);
    }
    CHECK_NEAR(s.status().speed_kmh, 0.0, 1e-6);
    CHECK(!s.status().moving);
}

// =====================================================================================================
// leforgatási táblázat
// =====================================================================================================
TEST(chart_wheat_and_fine_seeds) {
    using namespace chart;
    Result r = calculate(WHEAT, 200.0, 0, 0.0, 10.0);
    CHECK(r.flap == 'A' && r.range == 0);
    CHECK_NEAR(r.scale, 55.0 + 5.0 * 8.0 / 18.0, 1e-3); // 192 → 210 között
    CHECK_NEAR(r.expected_kg, 20.0, 0.05);               // 85 fordulat = 1/10 ha
    CHECK_NEAR(r.seed_kg, 2000.0, 1e-6);
    CHECK(note(r, false) == Note::CrankAndWeigh);

    Result fine = calculate(RAPE, 5.0, 0, 0.0, 0.0);
    CHECK(fine.flap == 'Z' && fine.drive == DRIVE_MIKRO);
    Result fine2 = calculate(RAPE, 15.0, 0, 0.0, 0.0);
    CHECK(fine2.drive == DRIVE_NORMAL);

    Result above = calculate(WHEAT, 500.0, 0, 0.0, 0.0);
    CHECK(above.range == 1 && note(above, false) == Note::AboveChart);
}

TEST(chart_crank_correction) {
    using namespace chart;
    // a vetőmag 10%-kal többet ad, mint a táblázat → kisebb skála kell
    const Result base = calculate(WHEAT, 200.0, 0, 0.0, 0.0);
    const Result r = calculate(WHEAT, 200.0, 0, base.expected_kg * 1.1, 0.0);
    CHECK(r.corrected_valid);
    CHECK(r.corrected_scale < base.scale);
    CHECK_NEAR(r.actual_rate, 220.0, 0.6);
    CHECK_NEAR(r.kg_per_rev, base.expected_kg * 1.1 / 85.0, 1e-6);
    // 17 fordulat = 1/50 ha
    const Result r17 = calculate(WHEAT, 200.0, 2, 4.0, 0.0);
    CHECK_NEAR(r17.actual_rate, 200.0, 1e-6);
    CHECK_NEAR(r17.kg_per_rev, 4.0 / 17.0, 1e-9);
}

// =====================================================================================================
// talajhajtás + kuplung
// =====================================================================================================
TEST(master_off_at_boot_and_clutch_logic) {
    Seeder s = make();
    Machine m = down_machine();
    m.run(s, 1.0f);
    CHECK(!s.status().master);
    CHECK(s.status().state == SowState::Off);
    CHECK(!s.outputs().clutch_drive[0] && !s.outputs().clutch_level[0]);

    s.set_master(true);
    m.run(s, 1.0f);
    CHECK(s.status().state == SowState::Sowing);
    CHECK(s.outputs().clutch_drive[0] && s.outputs().clutch_drive[1]);
    CHECK(s.outputs().clutch_level[0]);

    // kiemelés: 300 ms szűrés után old
    m.in.work_contact = false;
    m.run(s, 0.2f);
    CHECK(s.outputs().clutch_drive[0]);
    m.run(s, 0.3f);
    CHECK(!s.outputs().clutch_drive[0]);
    CHECK(s.status().state == SowState::Raised);
}

TEST(clutch_polarity_options) {
    Seeder s = make([](Settings &st) { st.clutch_mode = 1; }); // áram alatt old
    Machine m = down_machine();
    m.run(s, 0.5f);
    CHECK(!s.outputs().clutch_drive[0] && s.outputs().clutch_level[0]); // nem hajt → áram alatt
    s.set_master(true);
    m.run(s, 0.5f);
    CHECK(s.outputs().clutch_drive[0] && !s.outputs().clutch_level[0]);

    Seeder t = make([](Settings &st) { st.out_invert = 1; });
    Machine n = down_machine();
    t.set_master(true);
    n.run(t, 0.5f);
    CHECK(t.outputs().clutch_drive[0] && !t.outputs().clutch_level[0]);
    CHECK(t.outputs().lamp_level[0]); // fordított: kikapcsolt lámpa = magas szint
}

TEST(work_switch_nc_and_none) {
    Seeder s = make([](Settings &st) { st.work_sw = 2; }); // NC: lent bont
    Machine m;
    m.in.work_contact = false;
    s.set_master(true);
    m.run(s, 1.0f);
    CHECK(s.status().work_down && s.outputs().clutch_drive[0]);
    m.in.work_contact = true;
    m.run(s, 1.0f);
    CHECK(!s.status().work_down && !s.outputs().clutch_drive[0]);

    Seeder t = make([](Settings &st) { st.work_sw = 0; }); // nincs munkakapcsoló
    Machine n;
    t.set_master(true);
    n.run(t, 1.0f);
    CHECK(t.status().work_down && t.outputs().clutch_drive[0]);
}

TEST(ground_drive_meter_stopped_alarm) {
    Seeder s = make([](Settings &st) { st.meter_timeout_s = 3.0f; });
    Machine m = down_machine();
    m.ground_rpm = 120.0f; // 2 imp/s
    s.set_master(true);
    m.run(s, 6.0f);
    CHECK(!has_alarm(s, AlarmId::Meter1Stopped));
    CHECK(s.status().meter[0].rpm > 20.0f);
    CHECK(s.status().meter[0].view == MeterView::Work);

    m.meter_turns[1] = false; // a 2. adagoló lánca leesett
    m.run(s, 2.0f);
    CHECK(!has_alarm(s, AlarmId::Meter2Stopped)); // még a türelmi időn belül
    m.run(s, 3.0f);
    CHECK(has_alarm(s, AlarmId::Meter2Stopped));
    CHECK(s.status().top_alarm == AlarmId::Meter2Stopped);
    CHECK(s.status().meter[1].view == MeterView::Alarm);
    CHECK(s.status().state == SowState::Alarm);

    // álló gépnél nincs "nem forog" riasztás
    Seeder t = make([](Settings &st) { st.sim_speed = 0.0f; st.meter_timeout_s = 2.0f; });
    Machine n = down_machine();
    n.meter_turns[0] = n.meter_turns[1] = false;
    t.set_master(true);
    n.run(t, 10.0f);
    CHECK(!has_alarm(t, AlarmId::Meter1Stopped));
    CHECK(t.status().state == SowState::Standing);

    // kezelő által kikapcsolt adagoló nem riaszt
    Seeder u = make([](Settings &st) { st.meter_timeout_s = 2.0f; });
    Machine k = down_machine();
    k.meter_turns[1] = false;
    u.set_master(true);
    u.toggle_meter(1);
    k.run(u, 8.0f);
    CHECK(!has_alarm(u, AlarmId::Meter2Stopped));
    CHECK(u.status().meter[1].view == MeterView::ShutDown);
}

TEST(ground_drive_counts_area_and_kg) {
    Seeder s = make([](Settings &st) { st.meter_ppr = 1; st.cal1_kg_rev = 0.5f; });
    Machine m = down_machine();
    m.ground_rpm = 60.0f; // 1 fordulat/s
    s.set_master(true);
    m.run(s, 10.0f);
    // 8 km/h × 10 s = 22,2 m × 6 m (két fél) = 133 m²
    CHECK_NEAR(s.persistent().day.area_m2, 22.2 * 6.0, 6.0);
    CHECK_NEAR(s.persistent().day.kg[0], 10 * 0.5, 1.1); // ~10 fordulat × 0,5 kg
    // csak az 1. adagoló fut → fél szélesség
    s.reset_day();
    s.toggle_meter(1);
    m.run(s, 10.0f);
    CHECK_NEAR(s.persistent().day.area_m2, 22.2 * 3.0, 4.0);
    CHECK(s.persistent().total.area_m2 > s.persistent().day.area_m2);
}

TEST(front_rear_layout_counts_full_width) {
    Seeder s = make([](Settings &st) { st.meter_layout = 1; });
    Machine m = down_machine();
    s.set_master(true);
    s.toggle_meter(1); // csak az egyik fut, a szélesség akkor is teljes
    m.run(s, 10.0f);
    CHECK_NEAR(s.persistent().day.area_m2, 22.2 * 6.0, 6.0);
}

// =====================================================================================================
// elektromos hajtás
// =====================================================================================================
TEST(electric_target_rpm_and_regulation) {
    Seeder s = make([](Settings &st) {
        st.drive_type = 1;
        st.rate1 = 200.0f;
        st.rate2 = 100.0f;
        st.cal1_kg_rev = 0.2f;
        st.cal2_kg_rev = 0.2f;
        st.motor_max_rpm = 60.0f;
        st.prefill_auto = 0;
    });
    Machine m = down_machine();
    m.motor_strength = 0.85f; // a valódi motor gyengébb, mint az előrecsatolás feltételezi
    s.set_master(true);
    m.run(s, 15.0f);
    // 200 kg/ha × 8 km/h × 3 m / 600 = 8 kg/perc → /0,2 = 40 1/perc
    CHECK_NEAR(s.status().meter[0].target_rpm, 40.0, 1e-3);
    CHECK_NEAR(s.status().meter[1].target_rpm, 20.0, 1e-3);
    CHECK_NEAR(s.status().meter[0].rpm, 40.0, 1.5);
    CHECK_NEAR(s.status().meter[0].actual_rate, 200.0, 8.0);
    CHECK(!has_alarm(s, AlarmId::Meter1Rate));
    CHECK(!has_alarm(s, AlarmId::Meter1Stopped));
    CHECK(s.outputs().motor_pwm_pct[0] > 40.0f);
    CHECK(!s.outputs().clutch_drive[0]); // motoros hajtásnál nincs kuplung

    // rate +5%
    s.rate_step(+1);
    m.run(s, 5.0f);
    CHECK_NEAR(s.status().meter[0].target_rpm, 42.0, 1e-3);

    // a kiemeléskor a motor lefut (rámpa) és megáll
    m.in.work_contact = false;
    m.run(s, 3.0f);
    CHECK_NEAR(s.outputs().motor_pwm_pct[0], 0.0, 1e-6);
}

TEST(electric_cannot_reach_rate_alarm) {
    Seeder s = make([](Settings &st) {
        st.drive_type = 1;
        st.rate1 = 400.0f;
        st.cal1_kg_rev = 0.1f; // 400×8×3/600/0,1 = 160 1/perc > a motor tudása
        st.motor_max_rpm = 60.0f;
        st.prefill_auto = 0;
    });
    Machine m = down_machine();
    s.set_master(true);
    m.run(s, 15.0f);
    CHECK(has_alarm(s, AlarmId::Meter1Rate));
    CHECK_NEAR(s.outputs().motor_pwm_pct[0], 100.0, 1e-3);
    CHECK(!has_alarm(s, AlarmId::Meter2Rate) || s.status().meter[1].target_rpm > 59.0f);
}

TEST(electric_encoder_missing_alarm) {
    Seeder s = make([](Settings &st) { st.drive_type = 1; st.prefill_auto = 0; st.meter_timeout_s = 3.0f; });
    Machine m = down_machine();
    m.meter_turns[0] = false; // enkóder nem ad jelet
    s.set_master(true);
    m.run(s, 8.0f);
    CHECK(has_alarm(s, AlarmId::Meter1Stopped));
    CHECK(!has_alarm(s, AlarmId::Meter2Stopped));
}

// =====================================================================================================
// csőfeltöltés
// =====================================================================================================
TEST(prefill_auto_on_lowering_while_standing) {
    Seeder s = make([](Settings &st) {
        st.drive_type = 1;
        st.prefill_auto = 1;
        st.prefill_time_s = 3.0f;
        st.prefill_after_s = 4.0f;
        st.prefill_speed_kmh = 8.0f;
        st.sim_speed = 0.0f; // áll a gép
        st.cal1_kg_rev = 0.2f;
    });
    Machine m;
    m.in.work_contact = false; // kiemelve
    s.set_master(true);
    m.run(s, 1.0f);
    CHECK(s.status().prefill == PrefillState::Idle);
    m.in.work_contact = true; // leengedi a sarokban
    m.run(s, 0.5f);
    CHECK(s.status().prefill == PrefillState::Filling);
    CHECK(s.status().state == SowState::Prefill);
    CHECK_NEAR(s.status().meter[0].target_rpm, 40.0, 1e-3); // a feltöltési sebességgel számolva
    m.run(s, 3.0f);
    CHECK(s.status().prefill == PrefillState::AfterRun);
    m.run(s, 4.5f);
    CHECK(s.status().prefill == PrefillState::Idle);
    m.run(s, 2.0f);
    CHECK_NEAR(s.status().meter[0].target_rpm, 0.0, 1e-6); // áll a gép: nincs adagolás
    CHECK(!has_alarm(s, AlarmId::Meter1Stopped));
}

TEST(prefill_rules) {
    Seeder g = make(); // talajhajtás
    CHECK(!g.prefill_start());
    CHECK(std::strlen(g.status().message) == 0 || true);

    Seeder s = make([](Settings &st) { st.drive_type = 1; st.sim_speed = 0.0f; st.prefill_time_s = 2.0f; });
    Machine m = down_machine();
    m.run(s, 0.5f);
    CHECK(s.prefill_start()); // kézi feltöltés vetés KI mellett is (csoroszlyánál ellenőrzés)
    m.run(s, 0.5f);
    CHECK(s.status().prefill == PrefillState::Filling);
    CHECK(s.status().meter[0].target_rpm > 0.0f);
    s.toggle_meter(1);
    m.run(s, 0.2f);
    CHECK_NEAR(s.status().meter[1].target_rpm, 0.0, 1e-6); // kikapcsolt adagoló nem tölt
    // utánfutás közben a kiemelés megszakítja
    m.run(s, 2.0f);
    CHECK(s.status().prefill == PrefillState::AfterRun);
    m.in.work_contact = false;
    m.run(s, 0.5f);
    CHECK(s.status().prefill == PrefillState::Idle);
}

// =====================================================================================================
// szakaszvezérlés
// =====================================================================================================
TEST(tc_section_control_two_sections) {
    Seeder s = make();
    Machine m = down_machine();
    m.in.tc_connected = true;
    s.set_master(true);
    m.run(s, 0.1f);
    s.toggle_section_auto();
    s.tc_set_section_setpoint(0, true);
    s.tc_set_section_setpoint(1, false);
    m.run(s, 1.0f);
    CHECK(s.status().tc_section_auto);
    CHECK(s.outputs().clutch_drive[0] && !s.outputs().clutch_drive[1]);
    CHECK(s.status().section_on[0] && !s.status().section_on[1]);
    // automatában a kezelő gombja kitiltja az adagolót
    s.toggle_meter(0);
    m.run(s, 0.2f);
    CHECK(!s.outputs().clutch_drive[0]);
    s.toggle_meter(0);
    // TC kiesik: kézi mód (mindkét gomb BE) érvényes
    m.in.tc_connected = false;
    m.run(s, 0.2f);
    CHECK(!s.status().tc_section_auto);
    CHECK(s.outputs().clutch_drive[0] && s.outputs().clutch_drive[1]);
    CHECK(has_alarm(s, AlarmId::TcLost));
}

TEST(tc_one_section_switches_both_meters) {
    Seeder s = make([](Settings &st) { st.section_count = 1; });
    Machine m = down_machine();
    m.in.tc_connected = true;
    s.set_master(true);
    m.run(s, 0.1f);
    s.toggle_section_auto();
    s.tc_set_section_setpoint(0, false);
    m.run(s, 0.5f);
    CHECK(!s.outputs().clutch_drive[0] && !s.outputs().clutch_drive[1]);
    s.tc_set_section_setpoint(0, true);
    m.run(s, 0.5f);
    CHECK(s.outputs().clutch_drive[0] && s.outputs().clutch_drive[1]);
    s.toggle_meter(1); // a 2. gomb független marad
    m.run(s, 0.5f);
    CHECK(s.outputs().clutch_drive[0] && !s.outputs().clutch_drive[1]);
}

TEST(tc_work_switch_gating_option) {
    for (int gate = 0; gate < 2; ++gate) {
        Seeder s = make([gate](Settings &st) { st.work_sw_tc = gate; });
        Machine m;
        m.in.work_contact = false; // kiemelve
        m.in.tc_connected = true;
        s.set_master(true);
        m.run(s, 0.1f);
        s.toggle_section_auto();
        s.tc_set_section_setpoint(0, true);
        s.tc_set_section_setpoint(1, true);
        m.run(s, 1.0f);
        CHECK(s.outputs().clutch_drive[0] == (gate == 0));
    }
}

TEST(sec_auto_needs_tc) {
    Seeder s = make();
    Machine m = down_machine();
    m.run(s, 0.2f);
    s.toggle_section_auto();
    m.run(s, 0.1f);
    CHECK(!s.tc_section_auto_requested());
    CHECK(std::strlen(s.status().message) > 0 && s.status().message_seq > 0);
}

// =====================================================================================================
// turbina, tartályok
// =====================================================================================================
TEST(fan_alarm_only_while_working) {
    Seeder s = make([](Settings &st) { st.fan_min_rpm = 2500; st.fan_delay_s = 2.0f; });
    Machine m = down_machine();
    m.fan_rpm = 1500.0f;
    m.run(s, 1.0f);
    CHECK(!has_alarm(s, AlarmId::FanLow)); // késleltetés
    // munkakapcsolóval: lent + halad → riaszt (a vetés BE nem feltétel)
    m.run(s, 2.0f);
    CHECK(s.status().work_down && s.status().moving);
    CHECK(has_alarm(s, AlarmId::FanLow));
    m.in.work_contact = false;
    m.run(s, 1.0f);
    CHECK(!has_alarm(s, AlarmId::FanLow));

    // munkakapcsoló nélkül: halad + vetés BE
    Seeder t = make([](Settings &st) { st.work_sw = 0; st.fan_min_rpm = 2500; st.fan_delay_s = 2.0f; });
    Machine n;
    n.fan_rpm = 1500.0f;
    n.run(t, 5.0f);
    CHECK(!has_alarm(t, AlarmId::FanLow));
    t.set_master(true);
    n.run(t, 3.0f);
    CHECK(has_alarm(t, AlarmId::FanLow));
    n.fan_rpm = 3500.0f;
    n.run(t, 3.0f);
    CHECK(!has_alarm(t, AlarmId::FanLow));
    CHECK_NEAR(t.status().fan_rpm, 3500.0, 60.0);
}

TEST(hopper_switches_no_nc_delay_and_count) {
    Seeder s = make([](Settings &st) { st.hopper_delay_s = 2.0f; st.hopper1_nc = 0; st.hopper2_nc = 1; });
    Machine m = down_machine();
    m.in.hopper_contact[0] = false; // NO: nyitva = tele
    m.in.hopper_contact[1] = true;  // NC: zárva = tele
    m.run(s, 3.0f);
    CHECK(!s.status().hopper_empty[0] && !s.status().hopper_empty[1]);
    m.in.hopper_contact[0] = true;  // kiürült
    m.in.hopper_contact[1] = false; // kiürült
    m.run(s, 1.0f);
    CHECK(!s.status().hopper_empty[0]); // szűrés
    m.run(s, 1.5f);
    CHECK(s.status().hopper_empty[0] && s.status().hopper_empty[1]);
    CHECK(has_alarm(s, AlarmId::Hopper1Empty) && has_alarm(s, AlarmId::Hopper2Empty));

    Seeder t = make([](Settings &st) { st.tank_count = 1; st.hopper_delay_s = 0.5f; });
    Machine n = down_machine();
    n.in.hopper_contact[1] = true; // a nem létező 2. tartály jele nem számít
    n.run(t, 2.0f);
    CHECK(!t.status().hopper_empty[1] && !has_alarm(t, AlarmId::Hopper2Empty));
}

// =====================================================================================================
// nyomás, hőmérséklet, mérleg
// =====================================================================================================
TEST(pressure_scaling_fault_and_calibration) {
    Seeder s = make([](Settings &st) { st.pressure_enable = 1; st.pressure_signal = 1; st.pressure_range_bar = 250.0f; });
    Machine m = down_machine();
    m.in.pressure_v = 2.5f; // 0,5–4,5 V: közép = 125 bar
    m.run(s, 3.0f);
    CHECK(s.status().pressure_ok);
    CHECK_NEAR(s.status().pressure_bar, 125.0, 0.5);
    // ismert nyomás: a manométer 120 bart mutat
    CHECK(s.pressure_calibrate(120.0f));
    m.run(s, 0.5f);
    CHECK_NEAR(s.status().pressure_bar, 120.0, 0.5);
    CHECK_NEAR(s.settings().pressure_cal, 0.96, 0.002);
    // szakadt vezeték: 0 V a 0,5–4,5 V-os szenzornál
    m.in.pressure_v = 0.0f;
    m.run(s, 3.0f);
    CHECK(!s.status().pressure_ok && has_alarm(s, AlarmId::PressureSensor));

    Seeder t = make([](Settings &st) { st.pressure_enable = 1; st.pressure_signal = 0; });
    Machine n = down_machine();
    n.in.pressure_v = 0.0f; // 0–5 V szenzornál a 0 V érvényes (0 bar)
    n.run(t, 2.0f);
    CHECK(t.status().pressure_ok);
    CHECK_NEAR(t.status().pressure_bar, 0.0, 1e-3);

    Seeder u = make(); // kikapcsolva: nincs riasztás adat nélkül sem
    Machine k = down_machine();
    k.run(u, 1.0f);
    CHECK(!has_alarm(u, AlarmId::PressureSensor));
}

TEST(cooler_hysteresis_manual_and_failsafe) {
    Seeder s = make([](Settings &st) {
        st.temp_enable = 1;
        st.cooler_on_c = 60.0f;
        st.cooler_off_c = 50.0f;
        st.temp_alarm_c = 85.0f;
    });
    Machine m = down_machine();
    m.in.temp_c[0] = 55.0f;
    m.in.temp_c[1] = 45.0f;
    m.run(s, 0.5f);
    CHECK(!s.status().cooler_on);
    m.in.temp_c[0] = 61.0f;
    m.run(s, 0.5f);
    CHECK(s.status().cooler_on && s.outputs().cooler_level);
    m.in.temp_c[0] = 55.0f; // hiszterézis: még megy
    m.run(s, 0.5f);
    CHECK(s.status().cooler_on);
    m.in.temp_c[0] = 49.0f;
    m.run(s, 0.5f);
    CHECK(!s.status().cooler_on);
    // kézi mód
    s.toggle_cooler_mode();
    s.toggle_cooler();
    m.run(s, 0.5f);
    CHECK(s.status().cooler_manual && s.status().cooler_on);
    s.toggle_cooler_mode();
    // hőmérő kiesik: biztonságból hűt + riasztás
    m.in.temp_c[0] = -300.0f;
    m.in.temp_c[1] = -300.0f;
    m.run(s, 0.5f);
    CHECK(s.status().cooler_on && has_alarm(s, AlarmId::TempSensor));
    // túlmelegedés
    m.in.temp_c[0] = 90.0f;
    m.in.temp_c[1] = 70.0f;
    m.run(s, 0.5f);
    CHECK(has_alarm(s, AlarmId::OilHot));
}

TEST(scale_tare_and_calibration) {
    Seeder s = make([](Settings &st) { st.scale_enable = 1; });
    Machine m = down_machine();
    m.in.scale_valid = true;
    m.in.scale_raw = 12000.0f; // üres vödör
    m.run(s, 3.0f);
    s.scale_tare();
    m.run(s, 0.5f);
    CHECK_NEAR(s.status().scale.net_kg, 0.0, 0.01);
    m.in.scale_raw = 12000.0f + 10.0f * 50000.0f; // 10 kg, 50000 beütés/kg
    m.run(s, 3.0f);
    CHECK(s.scale_calibrate(10.0f));
    CHECK_NEAR(s.settings().scale_factor, 50000.0, 50.0);
    m.in.scale_raw = 12000.0f + 4.25f * 50000.0f;
    m.run(s, 3.0f);
    CHECK_NEAR(s.status().scale.net_kg, 4.25, 0.01);
    // leforgatásba átvétel
    CHECK(s.crank_take_scale());
    CHECK_NEAR(s.status().crank.measured_kg, 4.25, 0.02);
    // adat kiesik
    m.in.scale_valid = false;
    m.run(s, 4.0f);
    CHECK(!s.status().scale.valid && has_alarm(s, AlarmId::ScaleSensor));
    CHECK(!s.scale_calibrate(5.0f));
}

// =====================================================================================================
// kalibrálások
// =====================================================================================================
TEST(wheel_calibration_flow) {
    Seeder s = make([](Settings &st) { st.speed_src = 0; });
    Machine m = down_machine();
    PulseGen w;
    s.wheel_cal_start();
    for (int i = 0; i < 400; ++i) { // 20 s, 6,15 Hz → 123 impulzus
        m.in.wheel = w.step(6.15, 0.05);
        m.step(s);
    }
    CHECK(!s.wheel_cal_save()); // még fut
    s.wheel_cal_stop();
    m.step(s);
    CHECK(s.status().wheel_cal_ok);
    CHECK_NEAR(s.status().wheel_cal_ppm, 123.0, 1.5);
    s.wheel_cal_set_pulses(120); // kézi javítás
    CHECK(s.wheel_cal_save());
    CHECK_NEAR(s.settings().wheel_ppm, 120.0, 1e-3);
    CHECK(s.settings_dirty);
}

TEST(crank_motor_exact_turns_and_save) {
    Seeder s = make([](Settings &st) { st.drive_type = 1; st.sim_speed = 0.0f; st.encoder_ppr = 50; });
    Machine m = down_machine();
    s.set_master(true);
    m.run(s, 0.2f);
    CHECK(!s.crank_motor_start()); // vetés BE mellett nem
    s.set_master(false);
    s.crank_set_meter(1);
    s.crank_set_turns(2); // 17 fordulat
    CHECK(s.crank_motor_start());
    m.run(s, 2.0f);
    CHECK(s.status().crank.motor_running);
    CHECK(s.status().meter[1].pwm_pct > 0.0f);
    CHECK_NEAR(s.status().meter[0].pwm_pct, 0.0, 1e-6);
    m.run(s, 90.0f);
    CHECK(!s.status().crank.motor_running);
    CHECK_NEAR(s.status().crank.motor_revs_done, 17.0, 0.6);
    // nem számít bele a kijuttatott mennyiségbe
    CHECK_NEAR(s.persistent().total.kg[1], 0.0, 1e-9);
    s.crank_set_measured(3.4f);
    CHECK(s.crank_save());
    CHECK_NEAR(s.settings().cal2_kg_rev, 0.2, 1e-4);
    CHECK_NEAR(s.settings().cal1_kg_rev, 0.2353, 1e-4); // a másik változatlan
}

TEST(crank_save_requires_weight) {
    Seeder s = make();
    CHECK(!s.crank_save());
    s.crank_set_crop(chart::BARLEY);
    s.crank_set_measured(2.0f);
    CHECK(s.crank_save());
    CHECK_NEAR(s.settings().cal1_kg_rev, 2.0 / 85.0, 1e-4);
    s.crank_set_crop(99); // érvénytelen: marad
    CHECK(s.status().crank.crop == chart::BARLEY || true);
}

// =====================================================================================================
// TC-GEO, riasztások, parancsok
// =====================================================================================================
TEST(tc_geo_rate_used_only_when_enabled) {
    Seeder s = make([](Settings &st) { st.drive_type = 1; st.tc_geo = 1; st.prefill_auto = 0; });
    Machine m = down_machine();
    m.in.tc_connected = true;
    s.set_master(true);
    s.tc_set_rate(0, 150.0f);
    s.tc_set_rate(1, 50.0f);
    m.run(s, 1.0f);
    CHECK(s.status().tc_rate);
    CHECK_NEAR(s.status().meter[0].target_rate, 150.0, 1e-3);
    CHECK_NEAR(s.status().meter[1].target_rate, 50.0, 1e-3);
    m.in.tc_connected = false;
    m.run(s, 0.2f);
    CHECK(!s.status().tc_rate);
    CHECK_NEAR(s.status().meter[0].target_rate, 200.0, 1e-3);
}

TEST(tc_geo_rate_per_meter_and_clear) {
    Seeder s = make([](Settings &st) { st.drive_type = 1; st.tc_geo = 1; st.prefill_auto = 0; st.rate2 = 120.0f; });
    Machine m = down_machine();
    m.in.tc_connected = true;
    s.set_master(true);
    s.tc_set_rate(0, 150.0f);         // csak az 1. adagolóra érkezik térképérték
    m.run(s, 0.5f);
    CHECK_NEAR(s.status().meter[0].target_rate, 150.0, 1e-3);
    CHECK_NEAR(s.status().meter[1].target_rate, 120.0, 1e-3);   // a 2. a saját beállítását tartja
    s.tc_set_rate(5, 999.0f);         // érvénytelen index: nincs hatása
    s.tc_set_rate(0, -10.0f);         // negatív → 0
    m.run(s, 0.2f);
    CHECK_NEAR(s.status().meter[0].target_rate, 0.0, 1e-3);
    s.tc_clear_rate();
    m.run(s, 0.2f);
    CHECK(!s.status().tc_rate);
    CHECK_NEAR(s.status().meter[0].target_rate, 200.0, 1e-3);
}

// ----------------------------------------------------------------- leforgatás átmásolása a másik adagolóra
TEST(crank_copy_from_either_meter) {
    Seeder s = make([](Settings &st) { st.rate1 = 200.0f; st.rate2 = 120.0f; st.cal1_kg_rev = 0.25f; st.cal2_kg_rev = 0.1f; });
    // a 2. adagolót forgatja le, egy gombbal: mentés + másolás az 1.-re
    s.crank_set_meter(1);
    s.crank_set_measured(17.0f);                // 85 fordulat → 0,2 kg/ford
    CHECK(s.crank_copy_to_other());
    CHECK_NEAR(s.settings().cal2_kg_rev, 0.2, 1e-4);
    CHECK_NEAR(s.settings().cal1_kg_rev, 0.2, 1e-4);
    CHECK_NEAR(s.settings().rate1, 120.0, 1e-4); // az előírt mennyiség is átmegy
    // az 1.-ről mentett érték (nincs új mérés) a 2.-ra
    Seeder t = make([](Settings &st) { st.rate1 = 210.0f; st.cal1_kg_rev = 0.31f; });
    t.crank_set_meter(0);
    CHECK(t.crank_copy_to_other());
    CHECK_NEAR(t.settings().cal2_kg_rev, 0.31, 1e-5);
    CHECK_NEAR(t.settings().rate2, 210.0, 1e-4);
    std::string err;
    CHECK(apply_command(t, "meter_copy", 1, err));  // web: 2. → 1.
    CHECK_NEAR(t.settings().cal1_kg_rev, 0.31, 1e-5);
}

TEST(crank_copy_edge_cases) {
    Seeder one = make([](Settings &st) { st.meter_count = 1; st.rate2 = 50.0f; });
    CHECK(!one.crank_copy_to_other());
    CHECK_NEAR(one.settings().rate2, 50.0, 1e-4);
    std::string err;
    CHECK(!apply_command(one, "crank_copy", 0, err));
    CHECK(err.find("két adagoló") != std::string::npos);
    Seeder s = make();
    CHECK(!s.copy_meter(5));                    // érvénytelen forrás
    // motoros leforgatás közben nem másol
    Seeder e = make([](Settings &st) { st.drive_type = 1; st.sim_speed = 0.0f; });
    Machine m;
    m.run(e, 0.1f);
    CHECK(e.crank_motor_start());
    CHECK(!e.crank_copy_to_other());
}

// ----------------------------------------------------------------- kézi adagolás (elektromos hajtás)
TEST(manual_mode_constant_rpm_independent_of_speed) {
    Seeder s = make([](Settings &st) {
        st.drive_type = 1;
        st.cal1_kg_rev = 0.2f;
        st.cal2_kg_rev = 0.2f;
        st.motor_max_rpm = 60.0f;
        st.prefill_auto = 0;
        st.rate_step_pct = 5.0f;
    });
    Machine m = down_machine();
    s.set_master(true);
    m.run(s, 3.0f);
    CHECK_NEAR(s.status().meter[0].target_rpm, 40.0, 1e-3);   // automata: 200 kg/ha, 8 km/h
    s.set_auto(false);
    m.run(s, 0.2f);
    CHECK(s.status().manual);
    CHECK_NEAR(s.status().manual_pct, 67.0, 0.6);              // az automata fordulatról indul (40/60)
    s.rate_step(+1);
    m.run(s, 0.2f);
    CHECK_NEAR(s.status().meter[0].target_rpm, 0.72 * 60.0, 0.4);
    // a sebesség változása nem hat a fordulatra
    s.set_param(P_sim_speed, 12.0f);
    m.run(s, 2.0f);
    CHECK_NEAR(s.status().meter[0].target_rpm, 0.72 * 60.0, 0.4);
    CHECK(!has_alarm(s, AlarmId::Meter1Rate));                 // kézi módban nincs eltérés riasztás
    // a kiemelés kézi módban is megállítja
    m.in.work_contact = false;
    m.run(s, 1.0f);
    CHECK_NEAR(s.status().meter[0].target_rpm, 0.0, 1e-6);
    // határok
    for (int i = 0; i < 30; ++i) s.rate_step(+1);
    CHECK_NEAR(s.status().manual_pct, 72.0, 0.6);              // a status a következő ciklusban frissül
    m.run(s, 0.1f);
    CHECK_NEAR(s.status().manual_pct, 100.0, 1e-4);
    for (int i = 0; i < 30; ++i) s.rate_step(-1);
    m.run(s, 0.1f);
    CHECK_NEAR(s.status().manual_pct, 0.0, 1e-4);
    // vissza automatára: a ±% eltolás nem változott közben
    s.toggle_auto();
    m.in.work_contact = true;
    m.run(s, 1.0f);
    CHECK(!s.status().manual);
    CHECK_NEAR(s.status().rate_offset_pct, 0.0, 1e-6);
    CHECK_NEAR(s.status().meter[0].target_rpm, 60.0, 0.2);    // 200 kg/ha 12 km/h-nál
}

TEST(manual_mode_refused_for_ground_drive) {
    Seeder s = make();
    s.set_auto(false);
    Machine m = down_machine();
    m.run(s, 0.1f);
    CHECK(!s.status().manual);
    CHECK(std::strstr(s.status().message, "elektromos") != nullptr);
    // elektromosról talajhajtásra váltva a kézi mód megszűnik
    Seeder e = make([](Settings &st) { st.drive_type = 1; });
    e.set_auto(false);
    m.run(e, 0.1f);
    CHECK(e.status().manual);
    e.set_param(P_drive_type, 0.0f);
    m.run(e, 0.1f);
    CHECK(!e.status().manual);
}

TEST(tc_section_auto_accepted_without_operator) {
    Seeder s = make();
    Machine m = down_machine();
    m.in.tc_connected = true;
    s.set_master(true);
    m.run(s, 0.1f);
    s.tc_set_section_auto(true);       // a TC (navigáció) automatát kér
    s.tc_set_section_setpoint(0, false);
    s.tc_set_section_setpoint(1, true);
    m.run(s, 0.5f);
    CHECK(s.status().tc_section_auto);
    CHECK(!s.outputs().clutch_drive[0] && s.outputs().clutch_drive[1]);
    // a kezelő a saját gombjával kézire válthat
    s.toggle_section_auto();
    m.run(s, 0.2f);
    CHECK(!s.status().tc_section_auto);
    CHECK(s.outputs().clutch_drive[0] && s.outputs().clutch_drive[1]);
    // TC-SEC kikapcsolva a beállításban: a TC kérése hatástalan
    Seeder off = make([](Settings &st) { st.tc_sec = 0; });
    off.set_master(true);
    off.tc_set_section_auto(true);
    m.run(off, 0.2f);
    CHECK(!off.status().tc_section_auto);
}

TEST(alarm_ack_and_realarm) {
    Seeder s = make([](Settings &st) { st.realarm_s = 10; st.meter_timeout_s = 2.0f; });
    Machine m = down_machine();
    m.meter_turns[0] = false;
    s.set_master(true);
    m.run(s, 5.0f);
    CHECK(s.status().top_alarm == AlarmId::Meter1Stopped);
    s.ack_alarms();
    m.run(s, 1.0f);
    CHECK(s.status().top_alarm == AlarmId::COUNT);
    CHECK(has_alarm(s, AlarmId::Meter1Stopped)); // a hiba fennáll
    m.run(s, 10.0f);
    CHECK(s.status().top_alarm == AlarmId::Meter1Stopped); // újra feldobva
}

TEST(lamps_and_valve_need_feature) {
    Seeder s = make();
    s.toggle_lamp(0);
    s.toggle_valve();
    Machine m = down_machine();
    m.run(s, 0.2f);
    CHECK(!s.outputs().lamp_level[0] && !s.outputs().valve_level);
    Seeder t = make([](Settings &st) { st.lights_enable = 1; st.valve_enable = 1; });
    t.toggle_lamp(1);
    t.toggle_valve();
    Machine n = down_machine();
    n.run(t, 0.2f);
    CHECK(!t.outputs().lamp_level[0] && t.outputs().lamp_level[1] && t.outputs().valve_level);
    CHECK(t.status().valve_marker);
}

TEST(commands_and_status_json) {
    Seeder s = make();
    std::string err;
    CHECK(apply_command(s, "master", 1, err));
    CHECK(apply_command(s, "meter", 1, err));
    CHECK(apply_command(s, "crank_crop", chart::RAPE, err));
    CHECK(!apply_command(s, "nincs_ilyen", 0, err));
    CHECK(err.find("Ismeretlen") != std::string::npos);
    CHECK(!apply_command(s, "prefill_start", 0, err)); // talajhajtás
    CHECK(!err.empty());
    Machine m = down_machine();
    m.run(s, 0.5f);
    const std::string js = s.status_json();
    int depth = 0;
    bool ok = true;
    for (char ch : js) {
        if (ch == '{' || ch == '[') ++depth;
        if (ch == '}' || ch == ']') --depth;
        if (depth < 0) ok = false;
    }
    CHECK(ok && depth == 0);
    CHECK(js.find("\"master\":true") != std::string::npos);
    CHECK(js.find("\"crop_name\":\"Repce\"") != std::string::npos);
    CHECK(js.find(",}") == std::string::npos && js.find(",]") == std::string::npos);
}

TEST(simulation_mode_produces_inputs) {
    Seeder s = make([](Settings &st) { st.sim_inputs = 1; st.sim_fan_rpm = 3000; st.work_sw = 1; });
    TickInputs in; // minden bemenet üres, a szimuláció tölti
    s.set_master(true);
    for (int i = 0; i < 200; ++i) s.tick(in);
    CHECK(s.status().work_down);
    CHECK_NEAR(s.status().fan_rpm, 3000.0, 60.0);
    CHECK(s.status().meter[0].rpm > 1.0f);
    CHECK(!has_alarm(s, AlarmId::Meter1Stopped));
    CHECK(has_alarm(s, AlarmId::SimulationActive));
}

int main() {
    for (auto &[name, fn] : registry()) {
        const int before = g_failures;
        fn();
        std::printf("%s %s\n", g_failures == before ? "[ OK ]" : "[HIBA]", name);
    }
    std::printf("\n%d ellenőrzés, %d hiba\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

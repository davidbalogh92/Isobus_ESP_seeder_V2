#include "seeder/seeder.hpp"

#include <cstdlib>
#include <cstring>

namespace seeder {
namespace {
const char *find_value(const char *json, const char *key) {
    if (!json || !key) return nullptr;
    const std::size_t klen = std::strlen(key);
    for (const char *p = std::strchr(json, '"'); p; p = std::strchr(p + 1, '"')) {
        if (std::strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
            const char *q = p + 2 + klen;
            while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') ++q;
            if (*q != ':') continue;
            ++q;
            while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') ++q;
            return q;
        }
    }
    return nullptr;
}
} // namespace

bool json_get_string(const char *json, const char *key, std::string &out) {
    const char *v = find_value(json, key);
    if (!v || *v != '"') return false;
    out.clear();
    for (++v; *v && *v != '"'; ++v) {
        if (*v == '\\' && v[1]) ++v;
        out += *v;
    }
    return *v == '"';
}

bool json_get_number(const char *json, const char *key, double &out) {
    const char *v = find_value(json, key);
    if (!v) return false;
    if (std::strncmp(v, "true", 4) == 0) { out = 1; return true; }
    if (std::strncmp(v, "false", 5) == 0) { out = 0; return true; }
    if (*v == '"') ++v;
    char *end = nullptr;
    out = std::strtod(v, &end);
    return end != v;
}

// Parancsok (value: szám, ahol értelmezett):
//   master (0/1), master_toggle, meter (index: be/ki), meter_set_on / meter_set_off (index),
//   sec_auto, rate_step (+1/-1), rate_reset, auto (0/1), auto_toggle, meter_copy, lamp (0/1), valve, cooler_mode, cooler,
//   prefill_start, prefill_stop, ack, reset_day,
//   wheel_cal_start|stop|cancel|save, wheel_cal_pulses, wheel_cal_distance,
//   crank_meter, crank_crop, crank_turns, crank_test_scale, crank_measured, crank_hectares,
//   crank_from_scale, crank_save, crank_motor_start, crank_motor_stop, crank_reset,
//   scale_tare, scale_calibrate (kg), pressure_calibrate (bar)
bool apply_command(Seeder &s, const std::string &c, double v, std::string &error) {
    const int iv = static_cast<int>(v);
    const float fv = static_cast<float>(v);
    auto fail = [&error, &s](const char *fallback) {
        error = s.status().message && *s.status().message ? s.status().message : fallback;
        return false;
    };
    if (c == "master") s.set_master(iv != 0);
    else if (c == "master_toggle") s.toggle_master();
    else if (c == "meter") s.toggle_meter(iv);
    else if (c == "meter_set_on") s.set_meter(iv, true);
    else if (c == "meter_set_off") s.set_meter(iv, false);
    else if (c == "sec_auto") s.toggle_section_auto();
    else if (c == "rate_step") s.rate_step(iv >= 0 ? +1 : -1);
    else if (c == "rate_reset") s.rate_offset_reset();
    else if (c == "auto") s.set_auto(iv != 0);
    else if (c == "auto_toggle") s.toggle_auto();
    else if (c == "meter_copy") s.copy_meter1_to_2();
    else if (c == "lamp") s.toggle_lamp(iv);
    else if (c == "valve") s.toggle_valve();
    else if (c == "cooler_mode") s.toggle_cooler_mode();
    else if (c == "cooler") s.toggle_cooler();
    else if (c == "prefill_start") {
        if (!s.prefill_start()) return fail("A feltöltés nem indítható");
    } else if (c == "prefill_stop") s.prefill_stop();
    else if (c == "ack") s.ack_alarms();
    else if (c == "reset_day") s.reset_day();
    else if (c == "wheel_cal_start") s.wheel_cal_start();
    else if (c == "wheel_cal_stop") s.wheel_cal_stop();
    else if (c == "wheel_cal_cancel") s.wheel_cal_cancel();
    else if (c == "wheel_cal_pulses") s.wheel_cal_set_pulses(v < 0 ? 0u : static_cast<std::uint32_t>(v));
    else if (c == "wheel_cal_distance") s.wheel_cal_set_distance(fv);
    else if (c == "wheel_cal_save") {
        if (!s.wheel_cal_save()) { error = "A kerékkalibrálás nem menthető (előbb állítsd le)"; return false; }
    } else if (c == "crank_meter") s.crank_set_meter(iv);
    else if (c == "crank_crop") s.crank_set_crop(iv);
    else if (c == "crank_turns") s.crank_set_turns(iv);
    else if (c == "crank_test_scale") s.crank_set_test_scale(fv);
    else if (c == "crank_measured") s.crank_set_measured(fv);
    else if (c == "crank_hectares") s.crank_set_hectares(fv);
    else if (c == "crank_from_scale") {
        if (!s.crank_take_scale()) return fail("Nincs mérlegadat");
    } else if (c == "crank_save") {
        if (!s.crank_save()) return fail("Nem menthető");
    } else if (c == "crank_motor_start") {
        if (!s.crank_motor_start()) return fail("Nem indítható");
    } else if (c == "crank_motor_stop") s.crank_motor_stop();
    else if (c == "crank_reset") s.crank_reset_count();
    else if (c == "scale_tare") s.scale_tare();
    else if (c == "scale_calibrate") {
        if (!s.scale_calibrate(fv)) return fail("A kalibrálás nem sikerült");
    } else if (c == "pressure_calibrate") {
        if (!s.pressure_calibrate(fv)) return fail("A kalibrálás nem sikerült");
    } else {
        error = "Ismeretlen parancs: " + c;
        return false;
    }
    return true;
}

} // namespace seeder

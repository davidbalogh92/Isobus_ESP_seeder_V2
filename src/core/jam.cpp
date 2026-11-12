#include "seeder/jam.hpp"

namespace seeder {

const char *jam_phase_label(JamPhase p) {
    switch (p) {
    case JamPhase::Normal: return "Rendben";
    case JamPhase::Stop: return "Beszorulás – megáll";
    case JamPhase::Reverse: return "Visszafordítás";
    case JamPhase::Pause: return "Szünet";
    case JamPhase::Latched: return "Beszorult – áll";
    case JamPhase::TestFwd: return "Próba: előre";
    case JamPhase::TestStop: return "Próba: megáll";
    case JamPhase::TestRev: return "Próba: visszafordítás";
    case JamPhase::TestPause: return "Próba: szünet";
    case JamPhase::TestFwd2: return "Próba: újra előre";
    }
    return "";
}

namespace {
constexpr float STOP_S = 0.15f;        // megállás a visszafordítás előtt / után
constexpr float REV_TIMEOUT_S = 1.5f;  // ennyi idő alatt sem fordult vissza → kész (beragadt)
constexpr float OK_RESET_S = 5.0f;     // ennyi rendben forgás után nullázódik a próbaszámláló
constexpr float TEST_FWD_S = 1.0f;
constexpr float TEST_FWD2_S = 0.6f;
} // namespace

float JamGuard::reverse_pwm(const JamConfig &c) const {
    float p = c.deadband + 15.0f;
    if (p < 35.0f) p = 35.0f;
    if (p < c.pwm_min) p = c.pwm_min;
    if (p > c.pwm_max) p = c.pwm_max;
    return p;
}

void JamGuard::begin_reverse(const JamConfig &c) {
    (void)c;
    rev_pulses_ = 0;
    rev_t_ = 0.0f;
}

void JamGuard::release() {
    if (phase_ == JamPhase::Latched) {
        phase_ = JamPhase::Normal;
        msg_ = "";
    }
    tries_ = 0;
    jam_t_ = 0.0f;
}

bool JamGuard::start_test(const JamConfig &c) {
    (void)c;
    if (phase_ != JamPhase::Normal && phase_ != JamPhase::Latched) return false;
    phase_ = JamPhase::TestFwd;
    t_ = 0.0f;
    last_ = JamResult{};
    msg_ = "Próba fut";
    return true;
}

void JamGuard::stop_test() {
    if (testing()) {
        phase_ = JamPhase::Normal;
        msg_ = "Próba megszakítva";
    }
}

JamCommand JamGuard::step(const JamConfig &c, float dt, bool driving, float running_for, float pwm, float rpm,
                          float target, std::uint32_t pulses, float since_pulse_s, bool rev_active) {
    JamCommand o;
    t_ += dt;
    const float ppr = c.ppr > 0.0f ? c.ppr : 1.0f;
    const auto want_pulses = static_cast<std::uint32_t>(c.reverse_deg / 360.0f * ppr + 0.5f);
    const std::uint32_t need = want_pulses > 0 ? want_pulses : 1u;
    const float test_pwm = reverse_pwm(c);
    auto finish_reverse = [&](bool test) {
        last_.pulses = rev_pulses_;
        last_.degrees = static_cast<float>(rev_pulses_) / ppr * 360.0f;
        last_.duration_s = rev_t_;
        last_.pwm = test_pwm;
        last_.valid = true;
        phase_ = test ? JamPhase::TestPause : JamPhase::Pause;
        t_ = 0.0f;
    };

    switch (phase_) {
    case JamPhase::Normal: {
        if (!driving) {
            jam_t_ = 0.0f;
            break;
        }
        if (tries_ > 0) {
            ok_t_ += dt;
            if (ok_t_ >= OK_RESET_S && since_pulse_s < 0.5f) {
                ++recoveries_;
                tries_ = 0;
                msg_ = "Visszafordítás után rendben forog";
            }
        }
        const float strong = (c.deadband > c.pwm_min ? c.deadband : c.pwm_min) + 5.0f;
        const bool no_pulse = pwm >= strong && since_pulse_s >= c.detect_s;
        const bool stalled = pwm >= c.pwm_max - 1.0f && target > 0.5f && rpm < 0.25f * target;
        const bool cond = running_for > 1.0f && (no_pulse || stalled);
        jam_t_ = cond ? jam_t_ + dt : 0.0f;
        if (jam_t_ >= (no_pulse ? 0.0f : c.detect_s) && cond) {
            ++jams_;
            jam_t_ = 0.0f;
            if (c.reverse_enabled && tries_ < c.tries) {
                ++tries_;
                ok_t_ = 0.0f;
                phase_ = JamPhase::Stop;
                t_ = 0.0f;
                msg_ = "Beszorulás – visszafordítás";
            } else {
                phase_ = JamPhase::Latched;
                msg_ = c.reverse_enabled ? "Beszorult – a visszafordítás nem segített" : "Beszorult – a motor áll";
            }
        }
        break;
    }
    case JamPhase::Stop:
        o.override = true;
        if (t_ >= STOP_S) {
            phase_ = JamPhase::Reverse;
            t_ = 0.0f;
            begin_reverse(c);
        }
        break;
    case JamPhase::Reverse:
    case JamPhase::TestRev: {
        const bool test = phase_ == JamPhase::TestRev;
        o.override = true;
        o.reverse = true;
        o.pwm = test_pwm;
        if (rev_active) {   // holtidő után már hátrafelé hajt
            rev_t_ += dt;
            rev_pulses_ += pulses;
        }
        if (rev_pulses_ >= need || rev_t_ >= REV_TIMEOUT_S || t_ >= REV_TIMEOUT_S + 0.5f) {
            finish_reverse(test);
            o.pwm = 0.0f;
        }
        break;
    }
    case JamPhase::Pause:
        o.override = true;
        if (t_ >= STOP_S) {
            phase_ = JamPhase::Normal;   // újra előre, a szabályzó felfuttat
            t_ = 0.0f;
            jam_t_ = 0.0f;
        }
        break;
    case JamPhase::Latched:
        o.override = true;
        o.alarm = true;
        break;
    case JamPhase::TestFwd:
        o.override = true;
        o.pwm = test_pwm;
        if (t_ >= TEST_FWD_S) {
            phase_ = JamPhase::TestStop;
            t_ = 0.0f;
        }
        break;
    case JamPhase::TestStop:
        o.override = true;
        if (t_ >= STOP_S + 0.15f) {
            phase_ = JamPhase::TestRev;
            t_ = 0.0f;
            begin_reverse(c);
        }
        break;
    case JamPhase::TestPause:
        o.override = true;
        if (t_ >= STOP_S + 0.15f) {
            phase_ = JamPhase::TestFwd2;
            t_ = 0.0f;
        }
        break;
    case JamPhase::TestFwd2:
        o.override = true;
        o.pwm = test_pwm;
        if (t_ >= TEST_FWD2_S) {
            phase_ = JamPhase::Normal;
            t_ = 0.0f;
            o.pwm = 0.0f;
            msg_ = last_.pulses > 0 ? "Próba kész" : "Próba kész – visszafelé nem jött impulzus";
        }
        break;
    }
    o.count_ok = phase_ == JamPhase::Normal;
    return o;
}

} // namespace seeder

#include "seeder/motor_tune.hpp"

#include <cmath>

namespace seeder {

const char *tune_phase_label(TunePhase p) {
    switch (p) {
    case TunePhase::StartUp: return "Indulás keresése";
    case TunePhase::StopDown: return "Megállás keresése";
    case TunePhase::Points: return "Fordulat mérése";
    case TunePhase::Done: return "Kész";
    case TunePhase::Failed: return "Sikertelen";
    default: return "Nincs mérés";
    }
}

void MotorTuner::clear() {
    phase_ = TunePhase::Idle;
    pwm_ = 0.0f;
    elapsed_ = phase_t_ = run_t_ = 0.0f;
    first_run_pwm_ = -1.0f;
    nlevels_ = point_ = 0;
    sum_ = 0.0;
    n_ = 0;
    step_track_ = false;
    step_t63_ = -1.0f;
    ntrace_ = 0;
    fast_head_ = 0;
    for (int k = 0; k < FAST_N; ++k) {
        fast_dt_[k] = 0.0f;
        fast_cnt_[k] = 0;
    }
    res_ = TuneResult{};
    msg_ = "";
}

void MotorTuner::start(int meter, float pwm_cap) {
    clear();
    meter_ = meter;
    cap_ = pwm_cap > 5.0f ? (pwm_cap < 100.0f ? pwm_cap : 100.0f) : 5.0f;
    phase_ = TunePhase::StartUp;
    msg_ = "Indulás keresése";
}

void MotorTuner::abort(const char *reason) {
    if (!active()) return;
    const int m = meter_;
    clear();
    meter_ = m;
    phase_ = TunePhase::Failed;
    msg_ = reason;
}

void MotorTuner::fail(const char *m) {
    pwm_ = 0.0f;
    phase_ = TunePhase::Failed;
    res_.valid = false;
    msg_ = m;
}

int MotorTuner::progress_pct() const {
    switch (phase_) {
    case TunePhase::StartUp: return static_cast<int>(pwm_ / START_MAX_PCT * 20.0f);
    case TunePhase::StopDown: return 25;
    case TunePhase::Points: {
        const float per = SETTLE_S + MEASURE_S;
        const float f = nlevels_ > 0 ? (static_cast<float>(point_) + phase_t_ / per) / static_cast<float>(nlevels_) : 0.0f;
        return 35 + static_cast<int>(f * 64.0f);
    }
    case TunePhase::Done: return 100;
    default: return 0;
    }
}

float MotorTuner::step(float dt, float rpm, float since_pulse_s, std::uint32_t pulses, float ppr) {
    if (!active()) return 0.0f;
    elapsed_ += dt;
    // rövid ablakos fordulat (~0,25 s)
    fast_dt_[fast_head_] = dt;
    fast_cnt_[fast_head_] = pulses;
    fast_head_ = (fast_head_ + 1) % FAST_N;
    float fast_rpm = rpm;
    if (ppr > 0.0f) {
        float t = 0.0f;
        std::uint32_t c = 0;
        for (int k = 0; k < FAST_N && t < 0.25f; ++k) {
            const int j = (fast_head_ + FAST_N - 1 - k) % FAST_N;
            t += fast_dt_[j];
            c += fast_cnt_[j];
        }
        fast_rpm = t > 0.0f ? static_cast<float>(c) / t * 60.0f / ppr : 0.0f;
    }
    if (elapsed_ > MAX_TIME_S) {
        fail("Időtúllépés – a mérés nem fejeződött be");
        return 0.0f;
    }
    const bool turning = since_pulse_s < 0.8f && rpm > 0.2f;
    switch (phase_) {
    case TunePhase::StartUp:
        if (turning) {
            if (first_run_pwm_ < 0.0f) first_run_pwm_ = pwm_;
            run_t_ += dt;   // a PWM-et tartjuk, amíg megerősítjük a folyamatos forgást
            if (run_t_ >= RUN_CONFIRM_S) {
                res_.start_pwm = first_run_pwm_;
                phase_ = TunePhase::StopDown;
                phase_t_ = 0.0f;
                pwm_ = res_.start_pwm + 5.0f < cap_ ? res_.start_pwm + 5.0f : cap_;
                msg_ = "Megállás keresése";
            }
        } else {
            run_t_ = 0.0f;
            first_run_pwm_ = -1.0f;
            pwm_ += START_RAMP_PCT_S * dt;
            if (pwm_ > START_MAX_PCT || pwm_ > cap_) {
                fail("A motor nem indul (enkóder, bekötés, PWM max?)");
                return 0.0f;
            }
        }
        break;
    case TunePhase::StopDown:
        phase_t_ += dt;
        if (phase_t_ < 1.5f) break;   // előbb biztosan forogjon
        if (since_pulse_s >= STOP_PULSE_S || pwm_ <= 0.0f) {
            // a megállás jelzése késik (STOP_PULSE_S): ennyivel korábbi PWM-nél állt meg
            float sp = pwm_ + STOP_RAMP_PCT_S * STOP_PULSE_S;
            if (pwm_ <= 0.0f && since_pulse_s < STOP_PULSE_S) sp = 0.0f;   // 0%-ig sem állt meg (kifut)
            if (sp > res_.start_pwm) sp = res_.start_pwm;
            res_.stop_pwm = sp < 0.0f ? 0.0f : sp;
            // mérési pontok: 25 / 50 / 75 / 100%, a PWM maximumig, az indulás felett
            static const float L[4] = {25.0f, 50.0f, 75.0f, 100.0f};
            nlevels_ = 0;
            for (float l : L)
                if (l <= cap_ + 0.01f && l >= res_.start_pwm + 3.0f) levels_[nlevels_++] = l;
            if (nlevels_ > 0 && levels_[nlevels_ - 1] < cap_ - 10.0f && nlevels_ < TuneResult::MAX_POINTS)
                levels_[nlevels_++] = cap_;
            if (nlevels_ < 2 && cap_ >= res_.start_pwm + 6.0f) {   // szűk tartomány: két pont a maximum alatt
                nlevels_ = 0;
                levels_[nlevels_++] = (res_.start_pwm + cap_) / 2.0f;
                levels_[nlevels_++] = cap_;
            }
            if (nlevels_ < 2) {
                fail("Az indulási PWM túl közel van a PWM maximumhoz");
                return 0.0f;
            }
            phase_ = TunePhase::Points;
            point_ = -1;
            next_point();
            msg_ = "Fordulat mérése";
        } else {
            pwm_ -= STOP_RAMP_PCT_S * dt;
            if (pwm_ < 0.0f) pwm_ = 0.0f;
        }
        break;
    case TunePhase::Points:
        phase_t_ += dt;
        if (step_track_ && ntrace_ < TRACE) {
            trace_t_[ntrace_] = phase_t_;
            trace_[ntrace_++] = fast_rpm;
        }
        if (phase_t_ >= SETTLE_S) {
            sum_ += rpm;
            ++n_;
        }
        if (phase_t_ >= SETTLE_S + MEASURE_S) {
            res_.pwm[point_] = levels_[point_];
            res_.rpm[point_] = n_ > 0 ? static_cast<float>(sum_ / n_) : rpm;
            res_.points = point_ + 1;
            next_point();
        }
        break;
    default: break;
    }
    return phase_ == TunePhase::Done || phase_ == TunePhase::Failed ? 0.0f : pwm_;
}

void MotorTuner::next_point() {
    ++point_;
    if (point_ >= nlevels_) {
        finish();
        return;
    }
    step_track_ = point_ == nlevels_ - 1 && point_ > 0;
    step_from_ = point_ > 0 ? res_.rpm[point_ - 1] : 0.0f;
    ntrace_ = 0;
    pwm_ = levels_[point_];
    phase_t_ = 0.0f;
    sum_ = 0.0;
    n_ = 0;
}

void MotorTuner::finish() {
    pwm_ = 0.0f;
    const int n = res_.points;
    if (n < 2 || res_.rpm[n - 1] < 1.0f) {
        fail("Nem mérhető fordulat");
        return;
    }
    // egyenes illesztése (legkisebb négyzetek): rpm = a·pwm + b
    double sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < n; ++i) {
        sx += res_.pwm[i];
        sy += res_.rpm[i];
        sxx += static_cast<double>(res_.pwm[i]) * res_.pwm[i];
        sxy += static_cast<double>(res_.pwm[i]) * res_.rpm[i];
    }
    const double den = n * sxx - sx * sx;
    const double a = den != 0.0 ? (n * sxy - sx * sy) / den : 0.0;
    const double b = (sy - a * sx) / n;
    if (a <= 0.0) {
        fail("A fordulat nem nő a PWM-mel (enkóder hiba?)");
        return;
    }
    res_.max_rpm = res_.pwm[n - 1] >= 99.9f ? res_.rpm[n - 1] : static_cast<float>(a * 100.0 + b);
    double dev = 0.0;
    for (int i = 0; i < n; ++i) dev = std::fmax(dev, std::fabs(res_.rpm[i] - (a * res_.pwm[i] + b)));
    res_.linearity_pct = res_.max_rpm > 0.0f ? static_cast<float>(dev / res_.max_rpm * 100.0) : 0.0f;
    // reakcióidő: az utolsó ugrásnál a 63%-os beállás
    res_.tau_s = -1.0f;
    const float d = res_.rpm[n - 1] - step_from_;
    if (d > 0.05f * res_.rpm[n - 1]) {
        for (int i = 0; i < ntrace_; ++i)
            if (trace_[i] - step_from_ >= 0.63f * d) {
                res_.tau_s = trace_t_[i];
                break;
            }
    }
    // javaslatok
    float pm = std::round((res_.stop_pwm + 2.0f) * 2.0f) / 2.0f;
    res_.pwm_min = pm < 0.0f ? 0.0f : (pm > 50.0f ? 50.0f : pm);
    const float t = res_.tau_s;
    // a szabályzó a szűrt (lassabb) fordulatot látja, ezért óvatos tartomány (3–6)
    res_.gain = t < 0.0f ? 5 : t <= 0.25f ? 6 : t <= 0.5f ? 5 : t <= 0.9f ? 4 : 3;
    res_.valid = true;
    phase_ = TunePhase::Done;
    msg_ = "Kész – fogadd el, vagy vesd el";
}

} // namespace seeder

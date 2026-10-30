#pragma once
// Adagolómotor automatikus beállítása (önhangolás), elektromos hajtásnál, álló gépen.
// Kiméri:
//  1. az indulási PWM-et (lassan emelve, amíg az enkóder folyamatos forgást jelez),
//  2. a megállási PWM-et (onnan lassan csökkentve, amíg megáll) → PWM minimum javaslat,
//  3. a fordulatot 25 / 50 / 75 / 100% PWM-en (a PWM maximumig) → "fordulat 100% PWM-nél",
//  4. a reakcióidőt (63%-os beállás az utolsó PWM-ugrásnál) → szabályzási állandó javaslat.
// Az eredményt a kezelő fogadja el (akkor kerül a beállításokba), utána kézzel tovább állítható.
// Tiszta logika, hardver nélkül tesztelhető: ciklusonként a mért fordulatot kapja, PWM-et ad.
#include <cstdint>

namespace seeder {

enum class TunePhase : std::uint8_t { Idle, StartUp, StopDown, Points, Done, Failed };
const char *tune_phase_label(TunePhase p);

struct TuneResult {
    bool valid = false;
    float start_pwm = 0.0f;        ///< ennél indult el (%)
    float stop_pwm = 0.0f;         ///< ennél állt meg csökkentéskor (%)
    static constexpr int MAX_POINTS = 4;
    int points = 0;
    float pwm[MAX_POINTS] = {};    ///< mérési pontok (%)
    float rpm[MAX_POINTS] = {};    ///< mért fordulat (1/min)
    float max_rpm = 0.0f;          ///< fordulat 100% PWM-nél (mért vagy kivetített)
    float tau_s = -1.0f;           ///< reakcióidő (63%), < 0 = nem mérhető
    float linearity_pct = 0.0f;    ///< a közbenső pontok legnagyobb eltérése az egyenestől (a 100%-os fordulat %-ában)
    // javaslatok
    float pwm_min = 0.0f;
    int gain = 5;
};

class MotorTuner {
public:
    /// Indítás; pwm_cap = a beállított PWM maximum (efölé nem megy).
    void start(int meter, float pwm_cap);
    /// Megszakítás (a kezelő vagy biztonsági feltétel). Az eredmény elvész.
    void abort(const char *reason);
    /// Eredmény elvetése / új mérés előtti alapállapot.
    void clear();

    /// Egy ciklus: dt [s], a mért (szűrt) fordulat [1/min], az utolsó enkóderimpulzus óta eltelt idő [s],
    /// az e ciklusban jött impulzusok és az enkóder imp/ford (a reakcióidőt saját, rövid ablakkal méri).
    /// @return a kiadandó PWM [%] a mért adagolóra
    float step(float dt, float rpm, float since_pulse_s, std::uint32_t pulses = 0, float ppr = 0.0f);

    bool active() const { return phase_ == TunePhase::StartUp || phase_ == TunePhase::StopDown || phase_ == TunePhase::Points; }
    TunePhase phase() const { return phase_; }
    int meter() const { return meter_; }
    float pwm() const { return pwm_; }
    float elapsed_s() const { return elapsed_; }
    int progress_pct() const;
    const TuneResult &result() const { return res_; }
    const char *message() const { return msg_; }

    // a mérés korlátai (tesztekhez is)
    static constexpr float START_RAMP_PCT_S = 2.5f;   ///< indulás keresése: emelés %/s
    static constexpr float STOP_RAMP_PCT_S = 2.0f;    ///< megállás keresése: csökkentés %/s
    static constexpr float START_MAX_PCT = 70.0f;     ///< eddig sem indul → hiba
    static constexpr float RUN_CONFIRM_S = 1.0f;      ///< ennyi ideig folyamatos forgás = elindult
    static constexpr float STOP_PULSE_S = 1.2f;       ///< ennyi ideig nincs impulzus = megállt
    static constexpr float SETTLE_S = 2.5f;           ///< pontonként beállás
    static constexpr float MEASURE_S = 1.5f;          ///< pontonként átlagolás
    static constexpr float MAX_TIME_S = 120.0f;       ///< teljes mérés időkorlátja

private:
    void fail(const char *m);
    void finish();
    void next_point();

    TunePhase phase_ = TunePhase::Idle;
    int meter_ = 0;
    float cap_ = 100.0f;
    float pwm_ = 0.0f;
    float elapsed_ = 0.0f;
    float phase_t_ = 0.0f;
    float run_t_ = 0.0f;           ///< folyamatos forgás ideje
    float first_run_pwm_ = -1.0f;
    // pontok
    float levels_[TuneResult::MAX_POINTS] = {};
    int nlevels_ = 0;
    int point_ = 0;
    double sum_ = 0.0;
    int n_ = 0;
    // reakcióidő (az utolsó ugrásnál)
    bool step_track_ = false;
    float step_from_ = 0.0f;
    float step_t63_ = -1.0f;
    // rövid ablakos fordulat (a reakcióidőhöz; a szabályzó szűrt fordulata lassabb)
    static constexpr int FAST_N = 8;
    float fast_dt_[FAST_N] = {};
    std::uint32_t fast_cnt_[FAST_N] = {};
    int fast_head_ = 0;
    static constexpr int TRACE = 160;
    float trace_[TRACE] = {};
    float trace_t_[TRACE] = {};
    int ntrace_ = 0;
    TuneResult res_;
    const char *msg_ = "";
};

} // namespace seeder

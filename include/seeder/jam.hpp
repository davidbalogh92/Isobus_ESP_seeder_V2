#pragma once
// Adagolómotor beszorulás-kezelése (elektromos hajtás).
//  * Felismerés: a motor hajtva (cél > 0, felfutás után), elég nagy PWM mellett nem jön enkóderimpulzus
//    a beállított ideig, vagy a PWM a maximumon van, és a fordulat a cél negyede alatt marad.
//  * Visszafordítás (ha be van kapcsolva és a bekötés irányváltós): megáll → hátra a beállított szöggel
//    (az enkóder impulzusaiból számolva) → szünet → újra előre. Legfeljebb N próba; ha egy próba után 5 s-ig
//    rendben forog, a számláló nullázódik.
//  * Ha nincs visszafordítás, vagy elfogytak a próbák: a motor áll, riasztás; a kezelő nyugtázása vagy a
//    vetés újraindítása oldja.
//  * Próba (tesztfelület): álló gépen egy teljes ciklus előre → állj → hátra → állj → előre, mérési eredménnyel.
// Tiszta logika, hardver nélkül tesztelhető.
#include <cstdint>

namespace seeder {

enum class JamPhase : std::uint8_t { Normal, Stop, Reverse, Pause, Latched, TestFwd, TestStop, TestRev, TestPause, TestFwd2 };
const char *jam_phase_label(JamPhase p);

struct JamConfig {
    bool reverse_enabled = true;   ///< visszafordítás be (és irányváltós a bekötés)
    float reverse_deg = 30.0f;     ///< visszafordítás mértéke (adagolófok)
    int tries = 3;                 ///< próbálkozások száma
    float detect_s = 0.6f;         ///< felismerési idő
    float ppr = 50.0f;             ///< enkóder impulzus / adagolófordulat
    float pwm_min = 8.0f;          ///< a szabályzó PWM minimuma
    float pwm_max = 100.0f;        ///< PWM maximum
    float deadband = 0.0f;         ///< indulási PWM
};

/// Egy ciklus kimenete: ha override, a hajtást a beszorulás-kezelő vezérli (pwm, irány).
struct JamCommand {
    bool override = false;
    float pwm = 0.0f;
    bool reverse = false;
    bool alarm = false;            ///< beszorult, áll (riasztás)
    bool count_ok = true;          ///< mehet-e a mennyiség / leforgatás számlálás (visszafordításkor nem)
};

struct JamResult {                 ///< az utolsó visszafordítás mérése
    std::uint32_t pulses = 0;
    float degrees = 0.0f;
    float duration_s = 0.0f;
    float pwm = 0.0f;
    bool valid = false;
};

class JamGuard {
public:
    /// Egy ciklus. driving: a szabályzó hajtja a motort (cél > 0); running_for: a hajtás óta eltelt idő;
    /// pwm: a szabályzó PWM-je; rpm / target: mért és cél fordulat; pulses: e ciklus impulzusai;
    /// since_pulse_s: utolsó impulzus óta; rev_active: a kimeneten már a hátrameneti irány él (holtidő után).
    JamCommand step(const JamConfig &c, float dt, bool driving, float running_for, float pwm, float rpm, float target,
                    std::uint32_t pulses, float since_pulse_s, bool rev_active);

    /// Nyugtázás / vetés újraindítás: a beragadt állapot oldása.
    void release();
    /// Próba indítása (álló gépen).
    bool start_test(const JamConfig &c);
    void stop_test();

    JamPhase phase() const { return phase_; }
    bool latched() const { return phase_ == JamPhase::Latched; }
    bool testing() const { return phase_ >= JamPhase::TestFwd; }
    bool busy() const { return phase_ != JamPhase::Normal; }
    int tries_used() const { return tries_; }
    unsigned jams() const { return jams_; }
    unsigned recoveries() const { return recoveries_; }
    const JamResult &last() const { return last_; }
    const char *message() const { return msg_; }

private:
    void begin_reverse(const JamConfig &c);
    float reverse_pwm(const JamConfig &c) const;

    JamPhase phase_ = JamPhase::Normal;
    float t_ = 0.0f;               ///< az aktuális fázis ideje
    float jam_t_ = 0.0f;           ///< felismerési számláló
    float ok_t_ = 0.0f;            ///< próba után rendben forgás ideje
    int tries_ = 0;
    unsigned jams_ = 0, recoveries_ = 0;
    std::uint32_t rev_pulses_ = 0;
    float rev_t_ = 0.0f;
    JamResult last_;
    const char *msg_ = "";
};

} // namespace seeder

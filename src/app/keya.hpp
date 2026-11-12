#pragma once
// Keya „Sowing Motor All-in-One” CAN-motor meghajtó (KY40ACS…-PX40-49, saját fordulatszabályzással).
// Protokoll (gyári leírás 6. fejezet): 11 bites standard keret, ID = (node << 6) | parancs, little-endian adatok.
// A motorok az ISOBUS buszon osztoznak (250 kbit/s); a 11 bites kereteket a CAN-meghajtó szintjén választjuk le.
//
// Működés ciklusonként (a vezérlési ciklus, ~50 ms):
//  * a mag „PWM”-jéből (0–100 % = 0 – fordulat 100 %-on) és irányából célfordulatot küld (0x03, ford/s),
//  * lekérdezi a fordulatot (0x11) és a pozíciót (0x12) minden ciklusban, az állapotot / áramot / feszültséget
//    ritkábban, szívverést küld (0x3F),
//  * a pozíció változásából „virtuális enkóder” impulzusokat képez (KEYA_PPR / adagolófordulat), így a mag
//    leforgatás-számlálása, beszorulás-figyelése és mennyiség-elszámolása változatlanul működik.
// Hordozható (CAN nélkül tesztelhető): a keretek küldése egy callbackon át megy.
#include "seeder/pulse.hpp"
#include "seeder/seeder.hpp"

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace app {

constexpr float KEYA_PPR = 360.0f;   ///< virtuális impulzus / adagolófordulat (1 imp = 1°)

struct KeyaMotorState {
    bool online = false;        ///< az utolsó 0,6 s-ban válaszolt
    bool enabled = false;       ///< állapotbit 0
    bool reached = false;       ///< állapotbit 1: cél elérve
    bool current_limit = false; ///< állapotbit 2
    std::uint32_t faults = 0;   ///< hibabitek (0 önteszt, 1 jeladó, 17 túlfesz., 18 alacsony fesz., 19 túláram)
    float rpm = 0.0f;           ///< nyers fordulat a motortól (ford/s, motor egységben)
    double pos_rev = 0.0;       ///< nyers pozíció a motortól (fordulat, motor egységben)
    float current_a = 0.0f;
    float vbus_v = 0.0f;
    float cmd_rpm = 0.0f;       ///< utoljára küldött cél (adagoló 1/min, előjeles)
    float since_rx_s = 99.0f;
    unsigned rx = 0, tx = 0;
};

class KeyaBus {
public:
    using Send = std::function<bool(std::uint32_t id, const std::uint8_t *data, std::uint8_t len)>;
    void set_sender(Send s) { send_ = std::move(s); }

    /// Bejövő 11 bites keret (a CAN olvasó szálból). true: Keya keret volt.
    bool on_frame(std::uint32_t id, const std::uint8_t *data, std::uint8_t len);

    /// A mag bemenetei elé: virtuális enkóderimpulzusok és hibák. Csak Keya hajtásnál módosít.
    void feedback(const seeder::Settings &s, float dt, seeder::PulseSample meter[seeder::MAX_METERS],
                  bool motor_fault[seeder::MAX_METERS]);
    /// A mag kimenetei után: parancsok a motoroknak.
    void command(const seeder::Settings &s, const seeder::Outputs &o, float dt);

    // --- beállító mód (asztali, csak a motor a buszon!): a motor node ID-jának és sebességének átírása
    /// CAN-sebesség váltása (az ESP réteg adja: TWAI újraindítás). kbps: 250 / 500.
    using Rebaud = std::function<bool(int kbps)>;
    void set_rebaud(Rebaud r) { rebaud_ = std::move(r); }
    /// from_node 0: keresés 1..31 között. A motor új sebessége mindig 250 kbit/s (ISOBUS).
    bool start_setup(int from_node, int from_kbps, int new_node, std::string &err);
    void setup_step(float dt);
    bool setup_busy() const;
    std::string setup_json() const;

    KeyaMotorState state(int i) const;
    std::string json() const;   ///< diagnosztika

    static bool active(const seeder::Settings &s);
    /// motortengely-egység: a parancs / visszajelzés a motortengelyre vonatkozik (× áttétel)
    static float unit_factor(const seeder::Settings &s);

private:
    void tx(int node, std::uint8_t cmd, const std::uint8_t *data = nullptr, std::uint8_t len = 0);
    void tx_float(int node, std::uint8_t cmd, float v);
    int node_of(const seeder::Settings &s, int i) const;

    mutable std::mutex mx_;
    Send send_;
    struct Motor {
        KeyaMotorState st;
        double last_pos = 0.0;
        bool have_pos = false;
        double frac = 0.0;          ///< fel nem használt tört impulzus
        float idle_s = 0.0f;        ///< álló cél óta
        float enable_retry = 0.0f;
        bool want_enable = false;
    } m_[seeder::MAX_METERS];
    int node_[seeder::MAX_METERS] = {1, 2};
    unsigned cycle_ = 0;
    float hb_s_ = 0.0f;
    float setup_s_ = 0.0f;          ///< paraméterek beírása (csatlakozáskor)
    bool configured_[seeder::MAX_METERS] = {false, false};

    enum class Setup { Idle, Rebaud, Probe, Write, Save, Back, Verify };
    void setup_finish(bool ok, const char *msg);
    Rebaud rebaud_;
    Setup setup_ = Setup::Idle;
    float setup_t_ = 0.0f;
    int setup_from_ = 0, setup_kbps_ = 250, setup_new_ = 1, setup_found_ = 0, setup_seen_ = 0;
    bool setup_ok_ = false;
    std::string setup_msg_;
};

} // namespace app

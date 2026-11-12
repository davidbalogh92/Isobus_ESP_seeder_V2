#include "keya.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace app {
namespace {
// parancsok (az azonosító alsó 6 bitje)
constexpr std::uint8_t C_DISABLE = 0x00, C_ENABLE = 0x01, C_SET_VEL = 0x03, C_ERR_RESET = 0x0D, C_GET_STATUS = 0x0E,
                       C_STATUS_REPORT = 0x0F, C_GET_VEL = 0x11, C_GET_POS = 0x12, C_GET_IQ = 0x13,
                       C_GET_VBUS = 0x14, C_SET_PARAM = 0x17, C_SAVE = 0x19, C_HEARTBEAT = 0x3F;
constexpr std::uint8_t P_NODE_ID = 31, P_BAUD = 32, P_HB_TIMEOUT = 33;   // szívverés-fogadás időkorlát [ms]
constexpr float ONLINE_S = 0.6f;
constexpr float IDLE_DISABLE_S = 3.0f;     // ennyi álló cél után letiltjuk (nem tart nyomatékot, nem melegszik)

float rd_f32(const std::uint8_t *d) {
    float f;
    std::memcpy(&f, d, 4);   // little-endian (ESP32 és a PC is az)
    return f;
}
std::uint32_t rd_u32(const std::uint8_t *d) {
    return static_cast<std::uint32_t>(d[0]) | (static_cast<std::uint32_t>(d[1]) << 8) |
           (static_cast<std::uint32_t>(d[2]) << 16) | (static_cast<std::uint32_t>(d[3]) << 24);
}
} // namespace

bool KeyaBus::active(const seeder::Settings &s) { return seeder::keya_drive(s); }

float KeyaBus::unit_factor(const seeder::Settings &s) { return s.keya_unit == 1 ? (s.keya_gear > 0.1f ? s.keya_gear : 1.0f) : 1.0f; }

int KeyaBus::node_of(const seeder::Settings &s, int i) const { return i == 0 ? s.keya_node1 : s.keya_node2; }

void KeyaBus::tx(int node, std::uint8_t cmd, const std::uint8_t *data, std::uint8_t len) {
    if (!send_ || node < 1 || node > 31) return;
    const std::uint32_t id = (static_cast<std::uint32_t>(node) << 6) | cmd;
    send_(id, data, len);
    for (int i = 0; i < seeder::MAX_METERS; ++i)
        if (node_[i] == node) ++m_[i].st.tx;
}

void KeyaBus::tx_float(int node, std::uint8_t cmd, float v) {
    std::uint8_t d[4];
    std::memcpy(d, &v, 4);
    tx(node, cmd, d, 4);
}

bool KeyaBus::on_frame(std::uint32_t id, const std::uint8_t *data, std::uint8_t len) {
    if (id > 0x7FF) return false;
    const int node = static_cast<int>(id >> 6);
    const std::uint8_t cmd = static_cast<std::uint8_t>(id & 0x3F);
    std::lock_guard<std::mutex> g(mx_);
    if (setup_ != Setup::Idle) {   // beállító módban bármely node válasza érdekes
        if (node >= 1 && node <= 31 && (cmd == C_GET_STATUS || cmd == C_STATUS_REPORT || cmd == C_SET_PARAM || cmd == C_SAVE)) {
            setup_seen_ = node;
            if (setup_found_ == 0 && setup_ == Setup::Probe) setup_found_ = node;
        }
        return true;
    }
    for (int i = 0; i < seeder::MAX_METERS; ++i) {
        if (node_[i] != node) continue;
        Motor &m = m_[i];
        m.st.since_rx_s = 0.0f;
        ++m.st.rx;
        switch (cmd) {
        case C_GET_STATUS:
        case C_STATUS_REPORT:
            if (len >= 8) {
                const std::uint32_t sw = rd_u32(data), err = rd_u32(data + 4);
                m.st.enabled = (sw & 1u) != 0;
                m.st.reached = (sw & 2u) != 0;
                m.st.current_limit = (sw & 4u) != 0;
                m.st.faults = err;
            }
            break;
        case C_GET_VEL:
            if (len >= 4) m.st.rpm = rd_f32(data);   // nyers ford/s; a feedback() váltja át
            break;
        case C_GET_POS:
            if (len >= 4) m.st.pos_rev = rd_f32(data);
            break;
        case C_GET_IQ:
            if (len >= 4) m.st.current_a = rd_f32(data);
            break;
        case C_GET_VBUS:
            if (len >= 4) m.st.vbus_v = rd_f32(data);
            break;
        default: break;
        }
        return true;
    }
    return false;
}

void KeyaBus::feedback(const seeder::Settings &s, float dt, seeder::PulseSample meter[seeder::MAX_METERS],
                       bool motor_fault[seeder::MAX_METERS]) {
    if (!active(s)) return;
    std::lock_guard<std::mutex> g(mx_);
    node_[0] = s.keya_node1;
    node_[1] = s.keya_node2;
    const float k = unit_factor(s);
    const int used = seeder::shared_motor(s) ? 1 : seeder::meter_count(s);
    for (int i = 0; i < seeder::MAX_METERS; ++i) {
        Motor &m = m_[i];
        m.st.since_rx_s += dt;
        m.st.online = m.st.since_rx_s < ONLINE_S;
        // pozíció (motor egységben) → adagoló fordulat → virtuális impulzusok
        const double pos = m.st.pos_rev;
        seeder::PulseSample ps;
        ps.us_since_last = 0xFFFFFFFFu;
        if (m.st.online && m.have_pos) {
            double d = std::fabs(pos - m.last_pos) / k * seeder::KEYA_VIRTUAL_PPR + m.frac;
            if (d > 1e6) d = 0;   // pozíció-ugrás (pl. a motor újraindult)
            const auto n = static_cast<std::uint32_t>(d);
            m.frac = d - n;
            ps.count = n;
            if (n > 0) {
                ps.us_since_last = 0;
                ps.last_period_us = static_cast<std::uint32_t>(dt * 1e6f / static_cast<float>(n));
            }
        }
        m.last_pos = pos;
        m.have_pos = m.st.online;
        if (i < used) {
            meter[i] = ps;
            motor_fault[i] = !m.st.online || (m.st.faults & ((1u << 0) | (1u << 1) | (1u << 17) | (1u << 18) | (1u << 19))) != 0;
        }
    }
}

void KeyaBus::command(const seeder::Settings &s, const seeder::Outputs &o, float dt) {
    if (!active(s)) return;
    std::lock_guard<std::mutex> g(mx_);
    if (setup_ != Setup::Idle) return;
    node_[0] = s.keya_node1;
    node_[1] = s.keya_node2;
    ++cycle_;
    hb_s_ += dt;
    const float k = unit_factor(s);
    const float dir = s.motor_invert ? -1.0f : 1.0f;
    const int used = seeder::shared_motor(s) ? 1 : seeder::meter_count(s);
    const bool hb = hb_s_ >= 0.2f;
    if (hb) hb_s_ = 0.0f;
    for (int i = 0; i < used; ++i) {
        Motor &m = m_[i];
        const int node = node_[i];
        // csatlakozáskor (vagy beállítás változásakor) a szívverés-időkorlát beírása
        if (m.st.online && !configured_[i]) {
            if (s.keya_hb_ms > 0) {
                std::uint8_t d[8];
                const std::int32_t idx = P_HB_TIMEOUT, val = s.keya_hb_ms;
                std::memcpy(d, &idx, 4);
                std::memcpy(d + 4, &val, 4);
                tx(node, C_SET_PARAM, d, 8);
            }
            if (m.st.faults & ((1u << 17) | (1u << 18) | (1u << 19))) tx(node, C_ERR_RESET);
            configured_[i] = true;
        }
        if (!m.st.online) configured_[i] = false;
        // célfordulat: a mag "PWM"-je a fordulat 100 %-ához viszonyított cél
        const float pct = o.motor_pwm_pct[i];
        float rpm = pct / 100.0f * s.motor_max_rpm;
        if (o.motor_rev[i]) rpm = -rpm;
        const bool run = std::fabs(rpm) > 0.01f;
        m.idle_s = run ? 0.0f : m.idle_s + dt;
        m.want_enable = run || m.idle_s < IDLE_DISABLE_S;
        if (m.want_enable && !m.st.enabled) {
            m.enable_retry -= dt;
            if (m.enable_retry <= 0.0f) {
                tx(node, C_ENABLE);
                m.enable_retry = 0.5f;
            }
        } else if (!m.want_enable && m.st.enabled) {
            m.enable_retry -= dt;
            if (m.enable_retry <= 0.0f) {
                tx_float(node, C_SET_VEL, 0.0f);
                tx(node, C_DISABLE);
                m.enable_retry = 1.0f;
            }
        } else {
            m.enable_retry = 0.0f;
        }
        m.st.cmd_rpm = rpm;
        if (m.want_enable) tx_float(node, C_SET_VEL, rpm / 60.0f * k * dir);
        // lekérdezések: fordulat + pozíció minden ciklusban, a többi ritkábban
        tx(node, C_GET_VEL);
        tx(node, C_GET_POS);
        switch (cycle_ % 8) {
        case 0: tx(node, C_GET_STATUS); break;
        case 4: tx(node, C_GET_IQ); break;
        case 6: tx(node, C_GET_VBUS); break;
        default: break;
        }
        if (hb) tx(node, C_HEARTBEAT);
    }
}

bool KeyaBus::start_setup(int from_node, int from_kbps, int new_node, std::string &err) {
    std::lock_guard<std::mutex> g(mx_);
    if (setup_ != Setup::Idle) { err = "A beállítás már fut"; return false; }
    if (!send_ || !rebaud_) { err = "Nincs CAN"; return false; }
    if (from_node < 0 || from_node > 31 || new_node < 1 || new_node > 31) { err = "Node ID: 1..31"; return false; }
    if (from_kbps != 250 && from_kbps != 500) { err = "Sebesség: 250 vagy 500 kbit/s"; return false; }
    setup_from_ = from_node;
    setup_kbps_ = from_kbps;
    setup_new_ = new_node;
    setup_found_ = setup_seen_ = 0;
    setup_ok_ = false;
    setup_t_ = 0.0f;
    setup_msg_ = "Indul…";
    setup_ = Setup::Rebaud;
    return true;
}

bool KeyaBus::setup_busy() const {
    std::lock_guard<std::mutex> g(mx_);
    return setup_ != Setup::Idle;
}

void KeyaBus::setup_finish(bool ok, const char *msg) {
    setup_ok_ = ok;
    setup_msg_ = msg;
    setup_ = Setup::Idle;
    for (auto &c : configured_) c = false;
}

void KeyaBus::setup_step(float dt) {
    // a CAN-sebesség váltása a zároláson kívül történik (a TWAI újraindítás alatt keret is érkezhet)
    int rebaud_to = 0;
    {
        std::lock_guard<std::mutex> g(mx_);
        if (setup_ == Setup::Idle) return;
        setup_t_ += dt;
        auto status = [&](int n) { tx(n, C_GET_STATUS); };
        auto param = [&](int n, std::int32_t idx, std::int32_t val) {
            std::uint8_t d[8];
            std::memcpy(d, &idx, 4);
            std::memcpy(d + 4, &val, 4);
            tx(n, C_SET_PARAM, d, 8);
        };
        switch (setup_) {
        case Setup::Rebaud:
            if (setup_kbps_ != 250) rebaud_to = setup_kbps_;
            setup_ = Setup::Probe;
            setup_t_ = 0.0f;
            setup_msg_ = "Motor keresése…";
            break;
        case Setup::Probe:
            if (setup_t_ < 0.3f) break;
            if (setup_from_ > 0) {
                if (setup_seen_ == setup_from_) setup_found_ = setup_from_;
                else if (static_cast<int>(setup_t_ * 10) % 3 == 0) status(setup_from_);
            } else if (setup_found_ == 0 && static_cast<int>(setup_t_ * 20) % 10 == 0) {
                for (int n = 1; n <= 31; ++n) status(n);
            }
            if (setup_found_ > 0) {
                setup_from_ = setup_found_;
                tx(setup_from_, C_DISABLE);   // mentés csak letiltott állapotban
                setup_ = Setup::Write;
                setup_t_ = 0.0f;
                setup_msg_ = "Paraméterek írása…";
            } else if (setup_t_ > 3.0f) {
                if (setup_kbps_ != 250) rebaud_to = 250;
                setup_finish(false, "A motor nem válaszol (ellenőrizd a bekötést, a sebességet és hogy csak a motor van a buszon)");
            }
            break;
        case Setup::Write:
            if (setup_t_ < 0.2f) break;
            param(setup_from_, P_NODE_ID, setup_new_);
            param(setup_from_, P_BAUD, 0);   // 0 = 250 kbit/s
            setup_ = Setup::Save;
            setup_t_ = 0.0f;
            break;
        case Setup::Save:
            if (setup_t_ < 0.3f) break;
            tx(setup_from_, C_SAVE);
            setup_ = Setup::Back;
            setup_t_ = 0.0f;
            setup_msg_ = "Mentve. Kapcsold ki-be a motor tápját!";
            break;
        case Setup::Back:
            if (setup_t_ < 0.5f) break;
            if (setup_kbps_ != 250) rebaud_to = 250;
            setup_seen_ = 0;
            setup_ = Setup::Verify;
            setup_t_ = 0.0f;
            break;
        case Setup::Verify:
            if (setup_seen_ == setup_new_) {
                setup_finish(true, "Kész: a motor 250 kbit/s-on, az új címen válaszol");
                break;
            }
            if (static_cast<int>(setup_t_ * 10) % 5 == 0) status(setup_new_);
            if (setup_t_ > 30.0f)
                setup_finish(false, "Mentve, de az új címen nem jelentkezett 30 s alatt (táp ki-be megtörtént?)");
            break;
        case Setup::Idle: break;
        }
    }
    if (rebaud_to && rebaud_) rebaud_(rebaud_to);
}

std::string KeyaBus::setup_json() const {
    std::lock_guard<std::mutex> g(mx_);
    std::string m;
    for (char c : setup_msg_) m += c == '"' ? '\'' : c;
    char b[96];
    std::snprintf(b, sizeof b, "{\"busy\":%s,\"ok\":%s,\"found\":%d,\"msg\":\"",
                  setup_ != Setup::Idle ? "true" : "false", setup_ok_ ? "true" : "false", setup_found_);
    return b + m + "\"}";
}

KeyaMotorState KeyaBus::state(int i) const {
    std::lock_guard<std::mutex> g(mx_);
    return i >= 0 && i < seeder::MAX_METERS ? m_[i].st : KeyaMotorState{};
}

std::string KeyaBus::json() const {
    std::lock_guard<std::mutex> g(mx_);
    std::string o = "[";
    for (int i = 0; i < seeder::MAX_METERS; ++i) {
        const auto &st = m_[i].st;
        char b[320];
        std::snprintf(b, sizeof b,
                      "%s{\"node\":%d,\"online\":%s,\"enabled\":%s,\"reached\":%s,\"current_limit\":%s,\"faults\":%lu,"
                      "\"vel_raw\":%.3f,\"pos_raw\":%.3f,\"current_a\":%.2f,\"vbus_v\":%.2f,\"cmd_rpm\":%.2f,\"rx\":%u,\"tx\":%u}",
                      i ? "," : "", node_[i], st.online ? "true" : "false", st.enabled ? "true" : "false",
                      st.reached ? "true" : "false", st.current_limit ? "true" : "false",
                      static_cast<unsigned long>(st.faults), static_cast<double>(st.rpm), st.pos_rev,
                      static_cast<double>(st.current_a), static_cast<double>(st.vbus_v),
                      static_cast<double>(st.cmd_rpm), st.rx, st.tx);
        o += b;
    }
    return o + "]";
}

} // namespace app

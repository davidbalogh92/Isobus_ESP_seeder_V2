#include "seeder/aog.hpp"

namespace seeder {
namespace aog {
namespace {
constexpr std::uint8_t SRC_AGIO = 0x7F;
constexpr std::uint8_t SRC_MACHINE = 0x7B;     // gépmodul (hello / scan válasz)
constexpr std::uint8_t SRC_SWITCH = 0x77;      // kapcsolóvezérlés (a régi SeederMonitorban bevált)
constexpr std::uint8_t PGN_STEER = 0xFE;       // 254
constexpr std::uint8_t PGN_MACHINE = 0xEF;     // 239
constexpr std::uint8_t PGN_HELLO = 0xC8;       // 200
constexpr std::uint8_t PGN_SUBNET = 0xC9;      // 201
constexpr std::uint8_t PGN_SCAN = 0xCA;        // 202
constexpr std::uint8_t PGN_SWITCH = 0xEA;      // 234
constexpr std::uint32_t OFF_REPEAT_MS = 500;
constexpr std::uint32_t BURST_GAP_MS = 120;
constexpr int BURST = 3;

std::uint32_t age(std::uint32_t now, std::uint32_t then) { return now >= then ? now - then : 0u; }
} // namespace

std::uint8_t checksum(const std::uint8_t *f, std::size_t len) {
    std::uint32_t sum = 0;
    for (std::size_t i = 2; i + 1 < len; ++i) sum += f[i];
    return static_cast<std::uint8_t>(sum & 0xFFu);
}

bool valid_frame(const std::uint8_t *d, std::size_t n) {
    if (!d || n < 6 || d[0] != 0x80 || d[1] != 0x81) return false;
    if (static_cast<std::size_t>(d[4]) + 6 > n) return false;
    const std::size_t len = static_cast<std::size_t>(d[4]) + 6;
    return checksum(d, len) == d[len - 1];
}

void Link::set_own_ip(const std::uint8_t ip[4]) {
    for (int i = 0; i < 4; ++i) ip_[i] = ip[i];
}

void Link::on_packet(const std::uint8_t *d, std::size_t n, std::uint32_t now, const Sender &send) {
    ++stats_.rx_packets;
    if (!valid_frame(d, n) || d[2] != SRC_AGIO) {
        ++stats_.rx_rejected;
        return;
    }
    if (any_rx_) {
        const std::uint32_t gap = age(now, last_rx_ms_);
        if (gap > stats_.max_gap_ms) stats_.max_gap_ms = gap;
    }
    any_rx_ = true;
    last_rx_ms_ = now;
    const std::uint8_t pgn = d[3];
    const std::uint8_t len = d[4];
    stats_.last_pgn = pgn;
    switch (pgn) {
    case PGN_STEER:
        if (len >= 2) {
            const float kmh = static_cast<float>(d[5] | (d[6] << 8)) / 10.0f;
            if (kmh <= 80.0f) {   // sérült / irreális érték eldobva
                speed_kmh_ = kmh;
                last_254_ms_ = now;
                have_254_ = true;
                ++stats_.rx_254;
            }
        }
        break;
    case PGN_MACHINE:
        if (len >= 8) {
            speed_239_kmh_ = static_cast<float>(d[6]) / 10.0f;
            sections_ = static_cast<std::uint16_t>(d[11] | (d[12] << 8));
            last_239_ms_ = now;
            have_239_ = true;
            ++stats_.rx_239;
        }
        break;
    case PGN_HELLO: {
        ++stats_.rx_hello;
        std::uint8_t r[11] = {0x80, 0x81, SRC_MACHINE, SRC_MACHINE, 5,
                              static_cast<std::uint8_t>(sections_ & 0xFF), static_cast<std::uint8_t>(sections_ >> 8),
                              0, 0, 0, 0};
        r[10] = checksum(r, sizeof(r));
        if (send && send(r, sizeof(r))) ++stats_.tx_hello;
        break;
    }
    case PGN_SCAN: {
        ++stats_.rx_scan;
        std::uint8_t r[13] = {0x80, 0x81, SRC_MACHINE, 0xCB, 7, ip_[0], ip_[1], ip_[2], ip_[3],
                              ip_[0], ip_[1], ip_[2], 0};
        r[12] = checksum(r, sizeof(r));
        if (send && send(r, sizeof(r))) ++stats_.tx_scan;
        break;
    }
    case PGN_SUBNET:
        if (len >= 5 && d[5] == 201 && d[6] == 201) {
            subnet_[0] = d[7];
            subnet_[1] = d[8];
            subnet_[2] = d[9];
            subnet_pending_ = true;
        }
        break;
    default: break;
    }
}

bool Link::connected(std::uint32_t now) const { return any_rx_ && age(now, last_rx_ms_) <= TIMEOUT_MS; }

bool Link::speed_valid(std::uint32_t now) const {
    return (have_254_ && age(now, last_254_ms_) <= SPEED_TIMEOUT_MS) ||
           (have_239_ && age(now, last_239_ms_) <= SPEED_TIMEOUT_MS);
}

std::uint32_t Link::speed_age_ms(std::uint32_t now) const {
    if (have_254_ && age(now, last_254_ms_) <= SPEED_TIMEOUT_MS) return age(now, last_254_ms_);
    if (have_239_) return age(now, last_239_ms_);
    return 0xFFFFFFFFu;
}

bool Link::sections_valid(std::uint32_t now) const { return have_239_ && age(now, last_239_ms_) <= TIMEOUT_MS; }

bool Link::take_subnet_request(std::uint8_t out[3]) {
    if (!subnet_pending_) return false;
    subnet_pending_ = false;
    for (int i = 0; i < 3; ++i) out[i] = subnet_[i];
    return true;
}

bool Link::send_paint(const PaintState &p, std::uint32_t now, const Sender &send) {
    const int n = p.sections < 1 ? 1 : (p.sections > 16 ? 16 : p.sections);
    const std::uint16_t all = static_cast<std::uint16_t>(n >= 16 ? 0xFFFFu : ((1u << n) - 1u));
    std::uint16_t auto_g = 0, on_g = 0, off_g = 0;
    if (p.mode == 0) {
        auto_g = all;                                       // automata / zöld
    } else if (p.work) {
        on_g = static_cast<std::uint16_t>(p.on_mask & all); // kézi / sárga: a vető szakaszok BE
        off_g = static_cast<std::uint16_t>(~p.on_mask & all);
    } else {
        off_g = all;
    }
    // [0] főkapcsoló 1 BE / 2 KI, [1–2] automata csoport, [3] szakaszszám, [4] kézi BE 1–8, [5] kézi KI 1–8,
    // [6] kézi BE 9–16, [7] kézi KI 9–16
    std::uint8_t f[14] = {0x80, 0x81, SRC_SWITCH, PGN_SWITCH, 8,
                          static_cast<std::uint8_t>(p.work ? 0x01 : 0x02),
                          static_cast<std::uint8_t>(auto_g & 0xFF), static_cast<std::uint8_t>(auto_g >> 8),
                          static_cast<std::uint8_t>(n),
                          static_cast<std::uint8_t>(on_g & 0xFF), static_cast<std::uint8_t>(off_g & 0xFF),
                          static_cast<std::uint8_t>(on_g >> 8), static_cast<std::uint8_t>(off_g >> 8), 0};
    f[13] = checksum(f, sizeof(f));
    const bool ok = send && send(f, sizeof(f));
    if (ok) {
        ++stats_.tx_paint;
        last_tx_ms_ = now;
        last_main_ = f[5];
    } else {
        ++stats_.tx_failed;
    }
    return ok;
}

void Link::update(std::uint32_t now, const PaintState &p, const Sender &send) {
    const bool online = connected(now);
    if (online != was_connected_ && !online && any_rx_) ++stats_.outages;
    const bool came_back = online && !was_connected_;
    was_connected_ = online;
    if (!p.enabled) {
        have_state_ = false;
        burst_left_ = 0;
        return;
    }
    const std::uint16_t mask = p.mode == 1 ? p.on_mask : 0;
    const bool changed = !have_state_ || p.work != last_work_ || p.mode != last_mode_ || p.sections != last_sections_ ||
                         (p.work && mask != last_on_mask_);
    if (changed || came_back) {
        have_state_ = true;
        last_work_ = p.work;
        last_mode_ = p.mode;
        last_sections_ = p.sections;
        last_on_mask_ = mask;
        if (p.work) {
            // leengedéskor 3 BE csomag 120 ms-onként, utána csend: a kezelő az AgOpenGPS-ben kézzel
            // leállíthatja a festést anélkül, hogy azonnal visszakapcsolnánk
            burst_left_ = BURST;
            next_burst_ms_ = now;
        } else {
            burst_left_ = 0;
            send_paint(p, now, send);   // kiemeléskor KI azonnal
        }
    }
    if (p.work) {
        if (burst_left_ > 0 && static_cast<std::int32_t>(now - next_burst_ms_) >= 0) {
            if (send_paint(p, now, send)) {
                --burst_left_;
                next_burst_ms_ = now + BURST_GAP_MS;
            } else {
                next_burst_ms_ = now + 250;   // sikertelen küldés: hamar újra
            }
        }
    } else if (age(now, last_tx_ms_) >= OFF_REPEAT_MS) {
        send_paint(p, now, send);   // kiemelt gép: KI folyamatosan, hogy ne fessen
    }
}

} // namespace aog
} // namespace seeder

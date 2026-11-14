#include "ext_display.hpp"

#include <cstdio>
#include <cstring>

namespace app {

void ExtDisplay::on_frame(const std::uint8_t *data, std::size_t len) {
    if (!data || len < 1) return;
    std::lock_guard<std::mutex> g(mx_);
    ++rx_;
    const std::uint8_t id = data[0];
    if (id == EXTDISP_ID_HEARTBEAT) {
        extdisp_heartbeat_t hb{};
        if (extdisp_decode_heartbeat(data, len, &hb) == EXTDISP_OK) {
            const bool stop = (hb.flags & EXTDISP_HB_FLAG_STOP) != 0;
            if (stop && !prev_stop_) ++stop_edges_;
            prev_stop_ = stop;
            hb_ = hb;
            hb_seen_ = true;
            last_hb_ms_ = now_;
            ++hb_rx_;
        } else {
            ++rx_bad_;
        }
        return;
    }
    if (!extdisp_is_command_id(id)) {   // állapot-azonosító a kijelzőtől: nem nekünk szól
        ++rx_bad_;
        return;
    }
    if (queue_.size() >= 16) {   // elárasztás ellen (normál esetben 1–2 parancs ciklusonként)
        ++dropped_;
        return;
    }
    Rx r{};
    r.len = len > EXTDISP_FRAME_LEN ? EXTDISP_FRAME_LEN + 1 : len;   // a hossz-hibát a dekóder jelzi
    std::memcpy(r.data, data, len < EXTDISP_FRAME_LEN ? len : EXTDISP_FRAME_LEN);
    queue_.push_back(r);
    ++cmd_rx_;
}

bool ExtDisplay::send(const std::uint8_t *frame) {
    const bool ok = send_ && send_(frame);
    std::lock_guard<std::mutex> g(mx_);
    if (ok) ++tx_;
    else ++tx_fail_;
    return ok;
}

void ExtDisplay::send_ack(std::uint8_t id, const ExtAck &a) {
    extdisp_ack_t ack{};
    ack.command_id = id;
    ack.result = a.result;
    ack.inhibit_reason = a.result == EXTDISP_ACK_INHIBITED ? a.reason : EXTDISP_INHIBIT_NONE;
    std::uint8_t f[EXTDISP_FRAME_LEN];
    if (extdisp_encode_ack(&ack, f) != EXTDISP_OK) return;
    send(f);
    std::lock_guard<std::mutex> g(mx_);
    ++acks_;
    last_cmd_ = id;
    last_ack_ = a;
}

void ExtDisplay::process(const Rx &rx) {
    const std::uint8_t id = rx.data[0];
    ExtAck a;
    auto decoded = [&](extdisp_result_t r) {
        if (r == EXTDISP_OK) return true;
        a.result = EXTDISP_ACK_BAD_DATA;   // hibás mező (ERR_VALUE) vagy nem 8 bájt (ERR_LENGTH)
        return false;
    };
    switch (id) {
    case EXTDISP_ID_LIGHT_COMMAND: {
        extdisp_light_command_t c{};
        if (decoded(extdisp_decode_light_command(rx.data, rx.len, &c)))
            a = handlers_.light ? handlers_.light(c) : ExtAck{EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NOT_SUPPORTED};
        break;
    }
    case EXTDISP_ID_WIFI_COMMAND: {
        extdisp_wifi_command_t c{};
        if (decoded(extdisp_decode_wifi_command(rx.data, rx.len, &c)))
            a = handlers_.wifi ? handlers_.wifi(c) : ExtAck{EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NOT_SUPPORTED};
        break;
    }
    case EXTDISP_ID_TANK_COMMAND: {
        extdisp_tank_command_t c{};
        if (decoded(extdisp_decode_tank_command(rx.data, rx.len, &c)))
            a = handlers_.tank ? handlers_.tank(c) : ExtAck{EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NOT_SUPPORTED};
        break;
    }
    case EXTDISP_ID_CALIB_COMMAND: {   // leforgatás (vetőgép): a mag meglévő crank_* műveletei
        extdisp_calib_command_t c{};
        if (decoded(extdisp_decode_calib_command(rx.data, rx.len, &c)))
            a = handlers_.calib ? handlers_.calib(c) : ExtAck{EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NOT_SUPPORTED};
        break;
    }
    default:   // a parancstartományban, de ismeretlen: ezen a gépen nincs ilyen funkció
        a = {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NOT_SUPPORTED};
        break;
    }
    if (extdisp_command_needs_ack(id)) send_ack(id, a);
}

void ExtDisplay::tick(std::uint32_t now_ms, bool link_ok, const ExtDisplayMachine &m) {
    std::deque<Rx> cmds;
    {
        std::lock_guard<std::mutex> g(mx_);
        now_ = now_ms;
        cmds.swap(queue_);
        if (!link_ok) hb_seen_ = false;
    }
    if (!link_ok) {   // nincs kijelző: semmit nem küldünk, a parancsokat eldobjuk
        link_prev_ = false;
        wifi_prev_ = EXTDISP_WIFI_OFF;
        calib_prev_ = false;
        return;
    }
    for (const auto &c : cmds) process(c);

    std::uint8_t f[EXTDISP_FRAME_LEN];
    // képességek: a kijelző megjelenésekor (bekapcsoláskor is ez az első) azonnal, utána 5 s-onként
    if (!link_prev_ || now_ms - last_caps_ >= EXTDISP_CAPABILITIES_PERIOD_MS) {
        if (extdisp_encode_capabilities(&m.caps, f) == EXTDISP_OK && send(f)) {
            last_caps_ = now_ms;
            std::lock_guard<std::mutex> g(mx_);
            ++caps_sent_;
        }
    }
    // gépállapot / szívverés 200 ms-onként
    if (!link_prev_ || now_ms - last_status_ >= EXTDISP_HEARTBEAT_PERIOD_MS) {
        extdisp_machine_status_t st{};
        st.counter = counter_;
        st.flags = m.flags;
        st.speed_ckmh = m.speed_ckmh;
        st.fan_rpm = m.fan_rpm;
        if (extdisp_encode_machine_status(&st, f) == EXTDISP_OK) {
            send(f);
            ++counter_;
        }
        last_status_ = now_ms;
    }
    // WiFi állapot 1 s-onként, amíg be van kapcsolva; a kikapcsoláskor még egyszer (KI)
    const bool wifi_changed = m.wifi_state != wifi_prev_;
    if (m.wifi_state != EXTDISP_WIFI_OFF || wifi_changed) {
        if (wifi_changed || now_ms - last_wifi_ >= EXTDISP_WIFI_STATUS_PERIOD_MS) {
            extdisp_wifi_status_t w{};
            w.state = m.wifi_state;
            w.minutes_left = m.wifi_minutes;
            if (extdisp_encode_wifi_status(&w, f) == EXTDISP_OK) send(f);
            last_wifi_ = now_ms;
        }
    }
    wifi_prev_ = m.wifi_state;
    // tartály állapot 1 s-onként, tartályonként (csak ha a tartály funkció be van jelentve)
    if ((m.caps.features & EXTDISP_FEATURE_TANK) && m.tank_n > 0 && now_ms - last_tank_ >= EXTDISP_TANK_STATUS_PERIOD_MS) {
        for (int i = 0; i < m.tank_n && i < 2; ++i)
            if (extdisp_encode_tank_status(&m.tanks[i], f) == EXTDISP_OK) send(f);
        last_tank_ = now_ms;
    }
    // leforgatás: csak amíg a kijelző aktív képernyője a leforgatás (és a gép jelenti a funkciót)
    const bool calib_on = (m.caps.features & EXTDISP_FEATURE_MACHINE_CALIB) && connected() &&
                          screen() == EXTDISP_SCREEN_CALIB;
    if (calib_on) {
        if (!calib_prev_ || now_ms - last_calib_ >= EXTDISP_CALIB_STATUS_PERIOD_MS) {
            unsigned n = 0;
            if (extdisp_encode_calib_status(&m.calib, f) == EXTDISP_OK && send(f)) ++n;
            if (extdisp_encode_calib_result(&m.calib_result, f) == EXTDISP_OK && send(f)) ++n;
            if (m.scale && extdisp_encode_scale_status(&m.scale_status, f) == EXTDISP_OK && send(f)) ++n;
            // részmérések: ciklusonként egy tétel, körbejárva
            extdisp_calib_weight_item_t w{};
            w.count = m.weight_count > EXTDISP_CALIB_MAX_WEIGHTS ? EXTDISP_CALIB_MAX_WEIGHTS : m.weight_count;
            if (weight_item_ >= w.count) weight_item_ = 0;
            w.index = w.count ? weight_item_ : 0;
            w.weight_g = w.count ? m.weights_g[w.index] : 0;
            if (extdisp_encode_calib_weight_item(&w, f) == EXTDISP_OK && send(f)) ++n;
            weight_item_ = w.count ? static_cast<std::uint8_t>((weight_item_ + 1) % w.count) : 0;
            last_calib_ = now_ms;
            std::lock_guard<std::mutex> g(mx_);
            calib_sent_ += n;
        }
        if (!calib_prev_ || now_ms - last_saved_ >= 1000u) {
            for (int i = 0; i < m.saved_n && i < 2; ++i) {
                extdisp_calib_saved_t sv{};
                sv.feeder = static_cast<std::uint8_t>(i + 1);
                sv.mg_per_rev = m.saved_mg_per_rev[i];
                if (extdisp_encode_calib_saved(&sv, f) == EXTDISP_OK) send(f);
            }
            last_saved_ = now_ms;
        }
    } else {
        weight_item_ = 0;
    }
    calib_prev_ = calib_on;
    link_prev_ = true;
}

bool ExtDisplay::connected() const {
    std::lock_guard<std::mutex> g(mx_);
    return hb_seen_ && now_ - last_hb_ms_ < EXTDISP_HEARTBEAT_TIMEOUT_MS;
}

bool ExtDisplay::stop_held() const {
    const bool c = connected();
    std::lock_guard<std::mutex> g(mx_);
    return c && (hb_.flags & EXTDISP_HB_FLAG_STOP) != 0;
}

bool ExtDisplay::start_held() const {
    const bool c = connected();
    std::lock_guard<std::mutex> g(mx_);
    return c && (hb_.flags & EXTDISP_HB_FLAG_START) != 0;
}

bool ExtDisplay::calib_hold(std::uint32_t now_ms) const {
    std::lock_guard<std::mutex> g(mx_);
    return hb_seen_ && now_ms - last_hb_ms_ < EXTDISP_CALIB_HOLD_TIMEOUT_MS &&
           hb_.active_screen == EXTDISP_SCREEN_CALIB && (hb_.flags & EXTDISP_HB_FLAG_START) &&
           !(hb_.flags & EXTDISP_HB_FLAG_STOP);
}

std::uint8_t ExtDisplay::screen() const {
    std::lock_guard<std::mutex> g(mx_);
    return hb_.active_screen;
}

unsigned ExtDisplay::stop_edges() const {
    std::lock_guard<std::mutex> g(mx_);
    return stop_edges_;
}

std::string ExtDisplay::json() const {
    const bool conn = connected();
    std::lock_guard<std::mutex> g(mx_);
    char b[448];
    std::snprintf(b, sizeof b,
                  "{\"connected\":%s,\"link\":%s,\"stop\":%s,\"start\":%s,\"screen\":%u,\"hb_rx\":%u,\"rx\":%u,"
                  "\"rx_bad\":%u,\"cmd_rx\":%u,\"dropped\":%u,\"tx\":%u,\"tx_fail\":%u,\"caps_sent\":%u,\"acks\":%u,"
                  "\"last_cmd\":%u,\"last_ack\":%u,\"last_reason\":%u,\"stop_edges\":%u,\"calib_tx\":%u}",
                  conn ? "true" : "false", link_prev_ ? "true" : "false",
                  conn && (hb_.flags & EXTDISP_HB_FLAG_STOP) ? "true" : "false",
                  conn && (hb_.flags & EXTDISP_HB_FLAG_START) ? "true" : "false", hb_.active_screen, hb_rx_, rx_,
                  rx_bad_, cmd_rx_, dropped_, tx_, tx_fail_, caps_sent_, acks_, last_cmd_, last_ack_.result,
                  last_ack_.reason, stop_edges_, calib_sent_);
    return b;
}

} // namespace app

#include "wifi_service.hpp"

#include <cmath>
#include <cstdio>

namespace app {
namespace {
int clamp_timeout(int m) { return m < 5 ? 5 : (m > 60 ? 60 : m); }
} // namespace

void WifiService::set_driver(std::function<void(bool)> d) {
    std::lock_guard<std::mutex> g(mx_);
    driver_ = std::move(d);
}

const char *WifiService::source_name(Source s) {
    switch (s) {
    case Source::Vt: return "VT";
    case Source::Display: return "külső kijelző";
    case Source::Web: return "web";
    case Source::Input: return "bemenet (IN8)";
    }
    return "?";
}

void WifiService::apply(bool on) {
    if (on_ == on) return;
    on_ = on;
    ++switches_;
    pending_ = on ? 1 : 0;
    if (on) idle_left_s_ = static_cast<float>(timeout_min_) * 60.0f;
}

WifiService::Result WifiService::request(bool on, Source src) {
    std::function<void(bool)> drv;
    int pend = -1;
    Result r = Result::Ok;
    {
        std::lock_guard<std::mutex> g(mx_);
        if (on) {
            if (work_active_) r = Result::InhibitedWork;
            else {
                last_src_ = src;
                if (on_) idle_left_s_ = static_cast<float>(timeout_min_) * 60.0f;   // újrakérés: időkorlát elölről
                apply(true);
            }
        } else if (on_) {
            if (ota_) r = Result::BusyOta;
            else {
                last_src_ = src;
                last_off_reason_ = "kézi";
                apply(false);
            }
        }
        pend = pending_;
        pending_ = -1;
        drv = driver_;
    }
    if (pend >= 0 && drv) drv(pend == 1);
    return r;
}

WifiService::Result WifiService::toggle(Source src) {
    bool cur;
    {
        std::lock_guard<std::mutex> g(mx_);
        cur = on_;
    }
    return request(!cur, src);
}

void WifiService::tick(float dt_s, bool work_active, int clients, bool ota_running, int timeout_min) {
    std::function<void(bool)> drv;
    int pend = -1;
    {
        std::lock_guard<std::mutex> g(mx_);
        work_active_ = work_active;
        const int prev_clients = clients_;
        clients_ = clients < 0 ? 0 : clients;
        ota_ = ota_running;
        const int tmo = clamp_timeout(timeout_min);
        if (tmo != timeout_min_ && on_) idle_left_s_ = static_cast<float>(tmo) * 60.0f;   // új beállítás
        timeout_min_ = tmo;
        if (on_) {
            if (work_active_ && !ota_) {
                last_off_reason_ = "munkakapcsoló";
                apply(false);
            } else if (clients_ > 0 || ota_ || clients_ != prev_clients) {
                // kliens / OTA alatt nem fogy, és a kliens távozásától indul újra a teljes idő
                idle_left_s_ = static_cast<float>(timeout_min_) * 60.0f;
            } else {
                idle_left_s_ -= dt_s;
                if (idle_left_s_ <= 0.0f) {
                    last_off_reason_ = "tétlenség";
                    apply(false);
                }
            }
        }
        pend = pending_;
        pending_ = -1;
        drv = driver_;
    }
    if (pend >= 0 && drv) drv(pend == 1);
}

bool WifiService::on() const {
    std::lock_guard<std::mutex> g(mx_);
    return on_;
}

WifiService::State WifiService::state() const {
    std::lock_guard<std::mutex> g(mx_);
    if (!on_) return Off;
    if (ota_) return Ota;
    return clients_ > 0 ? Client : On;
}

std::uint16_t WifiService::minutes_left() const {
    std::lock_guard<std::mutex> g(mx_);
    if (!on_) return 0;
    const float m = std::ceil(idle_left_s_ / 60.0f);
    return static_cast<std::uint16_t>(m < 0.0f ? 0.0f : m);
}

std::string WifiService::json() const {
    const State s = state();
    const unsigned left = minutes_left();
    std::lock_guard<std::mutex> g(mx_);
    char b[256];
    std::snprintf(b, sizeof b,
                  "{\"on\":%s,\"state\":%u,\"minutes_left\":%u,\"timeout_min\":%d,\"clients\":%d,\"ota\":%s,"
                  "\"work_active\":%s,\"source\":\"%s\",\"off_reason\":\"%s\",\"switches\":%u}",
                  on_ ? "true" : "false", static_cast<unsigned>(s), left, timeout_min_, clients_,
                  ota_ ? "true" : "false", work_active_ ? "true" : "false", source_name(last_src_), last_off_reason_,
                  switches_);
    return b;
}

} // namespace app

#pragma once
// Impulzusfrekvencia-becslő (kerékjeladó, turbina, adagolótengely, enkóder).
// Sűrű impulzusoknál ablakos számlálás, ritkánál (pl. 1 imp/kerékfordulat) periódusidő
// alapján számol, és impulzus nélkül biztosan lecseng nullára (nem ragad be utolsó érték).
#include <array>
#include <cstdint>

namespace seeder {

/// Egy vezérlési ciklus alatt beérkezett impulzusok a hardver/szimulátor felől.
struct PulseSample {
    std::uint32_t count = 0;             ///< impulzusok a ciklusban
    std::uint32_t last_period_us = 0;    ///< az utolsó két él közti idő (0 = ismeretlen)
    std::uint32_t us_since_last = 0xFFFFFFFFu; ///< az utolsó él óta eltelt idő
};

class PulseRate {
public:
    explicit PulseRate(float window_s = 1.0f, float smoothing_tau_s = 0.25f)
        : window_s_(window_s), tau_s_(smoothing_tau_s) {}

    void reset() {
        head_ = 0;
        filled_ = 0;
        hz_ = 0.0f;
        raw_hz_ = 0.0f;
    }

    /// @param dt_s ciklusidő
    /// @param timeout_s ennyi impulzusmentes idő után a frekvencia 0
    float update(float dt_s, const PulseSample &s, float timeout_s) {
        if (dt_s <= 0.0f) return hz_;
        dt_[head_] = dt_s;
        cnt_[head_] = s.count;
        head_ = (head_ + 1) % N;
        if (filled_ < N) ++filled_;
        float t = 0.0f;
        std::uint32_t c = 0;
        for (std::size_t i = 0; i < filled_ && t < window_s_; ++i) {
            const std::size_t k = (head_ + N - 1 - i) % N;
            t += dt_[k];
            c += cnt_[k];
        }
        const float since_s = s.us_since_last == 0xFFFFFFFFu ? 1e9f : s.us_since_last * 1e-6f;
        float raw = 0.0f;
        if (since_s > timeout_s) {
            raw = 0.0f;
        } else if (c >= 4 && t > 0.0f) {
            raw = static_cast<float>(c) / t;
        } else if (s.last_period_us > 0) {
            // ritka impulzusok: a periódusidő, de ha már régebben jött él, annál lassabb
            const float period_s = s.last_period_us * 1e-6f;
            raw = 1.0f / (since_s > period_s ? since_s : period_s);
        } else if (c > 0 && t > 0.0f) {
            raw = static_cast<float>(c) / t;
        }
        raw_hz_ = raw;
        if (raw == 0.0f) {
            hz_ = 0.0f; // azonnali nullázás időtúllépéskor
        } else {
            const float a = tau_s_ > 0.0f ? dt_s / (tau_s_ + dt_s) : 1.0f;
            hz_ += a * (raw - hz_);
        }
        return hz_;
    }

    float hz() const { return hz_; }
    float raw_hz() const { return raw_hz_; }

private:
    static constexpr std::size_t N = 64;
    std::array<float, N> dt_{};
    std::array<std::uint32_t, N> cnt_{};
    std::size_t head_ = 0, filled_ = 0;
    float window_s_, tau_s_;
    float hz_ = 0.0f, raw_hz_ = 0.0f;
};

} // namespace seeder

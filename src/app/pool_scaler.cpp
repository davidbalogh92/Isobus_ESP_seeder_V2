#include "pool_scaler.hpp"

#include "seeder/vt_scale.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace app {
namespace {
// ISO 11783-6 fontméretek (szélesség × magasság, px), a kód sorrendjében
constexpr std::uint8_t FONT_W[15] = {6, 8, 8, 12, 16, 16, 24, 32, 32, 48, 64, 64, 96, 128, 128};
constexpr std::uint8_t FONT_H[15] = {8, 8, 12, 16, 16, 24, 32, 32, 48, 64, 64, 96, 128, 128, 192};

std::uint16_t rd16(const std::uint8_t *p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }
void wr16(std::uint8_t *p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v & 0xFF);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
std::uint16_t clamp16(long v, long lo, long hi) { return static_cast<std::uint16_t>(std::max(lo, std::min(hi, v))); }
} // namespace

std::uint8_t scale_font(std::uint8_t code, float f, const std::function<bool(std::uint8_t)> &supported) {
    if (code > 14) return code;
    auto ok = [&](int c) { return !supported || supported(static_cast<std::uint8_t>(c)); };
    if (std::fabs(f - 1.0f) < 0.05f && ok(code)) return code;
    const float tw = FONT_W[code] * f * 1.05f, th = FONT_H[code] * f * 1.05f;
    int best = -1;
    for (int c = 0; c < 15; ++c) {
        if (FONT_W[c] > tw || FONT_H[c] > th || !ok(c)) continue;
        if (best < 0 || FONT_H[c] > FONT_H[best] || (FONT_H[c] == FONT_H[best] && FONT_W[c] > FONT_W[best])) best = c;
    }
    if (best >= 0) return static_cast<std::uint8_t>(best);
    for (int c = 0; c < 15; ++c)   // semmi sem fér: a legkisebb támogatott
        if (ok(c)) return static_cast<std::uint8_t>(c);
    return 0;
}

namespace {
// Egy táblabejegyzés módosítása a p-n kezdődő (legalább 4 bájtos, az eredeti pool-ból vett) területen.
// Visszaadja, hány bájtot ír át (p[0..n)).
unsigned apply_entry(const vt::ScaleEntry &e, std::uint8_t *p, float mf, float kf, unsigned kw, unsigned kh,
                     const std::function<bool(std::uint8_t)> &font_supported) {
    switch (e.kind) {
    case vt::SC_MASK_U16:
        wr16(p, clamp16(std::lround(rd16(p) * mf), 1, 65535));
        return 2;
    case vt::SC_MASK_I16: {
        const auto v = static_cast<std::int16_t>(rd16(p));
        wr16(p, static_cast<std::uint16_t>(static_cast<std::int16_t>(std::lround(v * mf))));
        return 2;
    }
    case vt::SC_FONT:
        *p = scale_font(*p, mf, font_supported);
        return 1;
    case vt::SC_KEY_PIC_W:
        wr16(p, clamp16(std::lround(e.w * kf), 1, 65535));
        return 2;
    case vt::SC_KEY_CHILD: {
        // a gyerek (kép) a gomb közepére; ismeretlen méretű gyerek: arányos pozíció
        long x, y;
        if (e.w && e.h) {
            x = std::lround((static_cast<float>(kw) - e.w * kf) / 2.0f);
            y = std::lround((static_cast<float>(kh) - e.h * kf) / 2.0f);
        } else {
            x = std::lround(static_cast<std::int16_t>(rd16(p)) * kf);
            y = std::lround(static_cast<std::int16_t>(rd16(p + 2)) * kf);
        }
        wr16(p, static_cast<std::uint16_t>(std::max(0L, x)));
        wr16(p + 2, static_cast<std::uint16_t>(std::max(0L, y)));
        return 4;
    }
    default: return 0;
    }
}
} // namespace

bool scale_pool_range(const std::uint8_t *pool, std::uint32_t size, const ScaleTarget &t, std::uint32_t offset,
                      std::uint32_t n, std::uint8_t *buf) {
    if (!pool || !buf || static_cast<std::uint64_t>(offset) + n > size) return false;
    std::memcpy(buf, pool + offset, n);
    const std::uint64_t lo = offset, hi = static_cast<std::uint64_t>(offset) + n;   // [lo, hi)
    // sérült tábla (a pool-on kívüli bejegyzés): az eredeti pool megy, módosítás nélkül
    for (std::uint32_t i = 0; i < vt::SCALE_ENTRY_COUNT; ++i)
        if (vt::SCALE_MAP[i].offset + 4ull > size) return true;
    const float mf = static_cast<float>(t.mask_px > 0 ? t.mask_px : vt::SCALE_DESIGN_MASK_PX) /
                     static_cast<float>(vt::SCALE_DESIGN_MASK_PX);
    const unsigned kw = t.key_w > 0 ? t.key_w : vt::SCALE_DESIGN_KEY_PX;
    const unsigned kh = t.key_h > 0 ? t.key_h : vt::SCALE_DESIGN_KEY_PX;
    const float kf = static_cast<float>(std::min(kw, kh)) / static_cast<float>(vt::SCALE_DESIGN_KEY_PX);
    for (std::uint32_t i = 0; i < vt::SCALE_ENTRY_COUNT; ++i) {
        const auto &e = vt::SCALE_MAP[i];
        if (e.offset >= hi || e.offset + 4ull <= lo) continue;   // nem érinti ezt a darabot
        std::uint8_t tmp[4];
        std::memcpy(tmp, pool + e.offset, 4);
        const unsigned w = apply_entry(e, tmp, mf, kf, kw, kh, t.font_supported);
        for (unsigned k = 0; k < w; ++k) {
            const std::uint64_t at = e.offset + k;
            if (at >= lo && at < hi) buf[at - lo] = tmp[k];
        }
    }
    if (!t.aux_objects) {
        // VT2: az AUX-N funkció (31) helyett String Variable (22) szóközökkel – az azonosító és a hossz marad
        for (std::uint32_t i = 0; i < vt::AUX_OBJECT_COUNT; ++i) {
            const auto &a = vt::AUX_OBJECTS[i];
            if (a.offset + a.len > size || a.len < 5 || pool[a.offset + 2] != 31) continue;
            const std::uint64_t a_lo = a.offset, a_hi = static_cast<std::uint64_t>(a.offset) + a.len;
            if (a_lo >= hi || a_hi <= lo) continue;
            const std::uint16_t slen = static_cast<std::uint16_t>(a.len - 5);
            for (std::uint64_t at = std::max(lo, a_lo + 2); at < std::min(hi, a_hi); ++at) {
                const std::uint64_t k = at - a_lo;
                std::uint8_t v = ' ';
                if (k == 2) v = 22;
                else if (k == 3) v = static_cast<std::uint8_t>(slen & 0xFF);
                else if (k == 4) v = static_cast<std::uint8_t>(slen >> 8);
                buf[at - lo] = v;
            }
        }
    }
    return true;
}

std::vector<std::uint8_t> scale_pool(const std::uint8_t *pool, std::uint32_t size, const ScaleTarget &t) {
    std::vector<std::uint8_t> out(size);
    if (size && !scale_pool_range(pool, size, t, 0, size, out.data())) out.assign(pool, pool + size);
    return out;
}

} // namespace app

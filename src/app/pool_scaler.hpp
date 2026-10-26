#pragma once
// A VT object pool átméretezése a terminál valódi méreteire, feltöltés előtt.
//  * maszk-tartalom (pozíció, méret): × (VT maszk szélesség / 400),
//  * fontok: a méretezett betűmérethez legközelebbi (nem nagyobb), a VT által támogatott font,
//  * softkey-k és a working set jelölője: a kép mérete min(gomb szél., gomb mag.) / 80 arányban,
//    a gomb közepére igazítva – így nem négyzetes vagy más arányú gombnál sem lóg ki.
// A méretezendő mezők listáját a pool generátor adja (include/seeder/vt_scale.hpp).
#include <cstdint>
#include <functional>
#include <vector>

namespace app {

struct ScaleTarget {
    unsigned mask_px = 400;      ///< a VT data mask szélessége (px)
    unsigned key_w = 80;         ///< softkey jelölő szélessége (px)
    unsigned key_h = 80;         ///< softkey jelölő magassága (px)
    /// a VT támogatja-e az adott ISO fontméret-kódot (0..14); üres = mind támogatott
    std::function<bool(std::uint8_t)> font_supported;
};

/// A pool méretezett másolata. Ha a méretezés nem lehetséges (sérült tábla), az eredeti másolatát adja.
std::vector<std::uint8_t> scale_pool(const std::uint8_t *pool, std::uint32_t size, const ScaleTarget &t);

/// A méretezett fontkód (0..14) a tervezési kódból és a maszk-arányból.
std::uint8_t scale_font(std::uint8_t design_code, float factor, const std::function<bool(std::uint8_t)> &supported);

} // namespace app

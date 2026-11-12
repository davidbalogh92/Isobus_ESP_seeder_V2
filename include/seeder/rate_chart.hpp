#pragma once
// Leforgatási segéd térfogat-állítós (változó volumenű) adagolóhoz, a gép beállítási táblázata
// ("Einstelltabelle") alapján. Tiszta logika, unit teszttel.
//
// Gépprofil: a táblázat futás közben cserélhető (web / Windows program: CSV feltöltés, NVS-ben tárolva).
// Beépített alap: Accord DA / DL / DT / DF (2–8 m), az ESC v0.10–0.16 táblázata változatlanul:
//   Normál vetőmag: skála 10 – 110, "A" (nyitott) torlócsappantyú.
//   Aprómag:        skála 2,5 – 25, "Z" (zárt) torlócsappantyú, N (normál) vagy M (mikro) hajtás.
//   Leforgatás:     85 fordulat = 1/10 ha (gépenként állítható: crank_turns_10).
// Egy növénynek 1–3 változata lehet (hajtás, cserebetét, adagolókerék …); a segéd a legfinomabbat
// választja, amelyikbe a kívánt mennyiség belefér (a legkisebb maximumú, ami még elég).
// A táblázat kiindulási érték; a leforgatás és mérlegelés korrigál az adott vetőmagtételre.
#include <cstdint>
#include <string>

namespace seeder {
namespace chart {

// a beépített táblázat növényeinek sorszáma (tesztek, alapértékek)
enum Crop : int {
    WHEAT = 0, RYE, BARLEY, OATS, FIELD_BEANS, PEAS, LUPINS, VETCHES, MAIZE, GRASS,   // normál vetőmag
    RAPE, RED_CLOVER, GRASS_FINE, STUBBLE_TURNIP,                                     // aprómag
    BUILTIN_CROPS
};
constexpr int MAX_CROPS = 16;      ///< a VT lista hossza
constexpr int MAX_VARIANTS = 3;
constexpr int MAX_POINTS = 24;
constexpr int NAME_LEN = 32;       ///< UTF-8 bájt, lezáróval
constexpr int LABEL_LEN = 24;

struct Curve {
    const float *scale;
    const float *rate;      // kg/ha az adott skálaértéknél, növekvő
    int n;
};

struct Variant {
    char label[LABEL_LEN] = {};    ///< pl. "N", "M", "Betét kicsi" (üres, ha egy változat van)
    char setup[LABEL_LEN] = {};    ///< beállítási szöveg a kijelzőn, pl. "Z + MIKRO"
    int n = 0;
    float scale[MAX_POINTS] = {};
    float rate[MAX_POINTS] = {};
    float max_rate() const { return n > 0 ? rate[n - 1] : 0.0f; }
};
struct CropDef {
    char name[NAME_LEN] = {};
    int nvar = 0;
    Variant var[MAX_VARIANTS];
};
struct Table {
    char machine[NAME_LEN] = {};   ///< gépprofil neve
    float turns10 = 0.0f;          ///< fordulat / 1/10 ha a profilból (0 = nincs megadva)
    int ncrop = 0;
    CropDef crop[MAX_CROPS];
    bool builtin = true;
};

/// A beépített Accord táblázat.
const Table &builtin_table();
/// Az aktív táblázat (alap: beépített). A hívó a mag zárolása alatt használja / cseréli.
const Table &table();
void set_table(const Table &t);
void reset_table();

/// CSV (pontosvessző vagy vessző, tizedesvessző is): növény;változat;beállítás;skála;kg/ha
/// Megjegyzés / meta: "# gép;<név>", "# fordulat_10;<szám>". Fejlécsor megengedett.
bool parse_csv(const std::string &csv, Table &out, std::string &err);
std::string to_csv(const Table &t);
std::string to_json(const Table &t, float turns10);

inline int crop_count() { return table().ncrop; }
/// Név (UTF-8, web és napló).
inline const char *crop_name(int c) { return c >= 0 && c < table().ncrop ? table().crop[c].name : ""; }
inline int variant_count(int c) { return c >= 0 && c < table().ncrop ? table().crop[c].nvar : 0; }

/// Egy növény egy változatának görbéje.
inline Curve curve(int c, int variant) {
    const Table &t = table();
    if (c < 0 || c >= t.ncrop) c = 0;
    const CropDef &cd = t.crop[c];
    if (variant < 0 || variant >= cd.nvar) variant = 0;
    const Variant &v = cd.var[variant];
    return {v.scale, v.rate, v.n};
}

/// kg/ha egy skálaértéknél (a táblázatpontok között lineáris, a végeken vágva).
inline double rate_at(const Curve &k, double scale) {
    if (scale <= k.scale[0]) return k.rate[0];
    for (int i = 1; i < k.n; i++)
        if (scale <= k.scale[i])
            return k.rate[i - 1] + (k.rate[i] - k.rate[i - 1]) * (scale - k.scale[i - 1]) / (k.scale[i] - k.scale[i - 1]);
    return k.rate[k.n - 1];
}

/// Skála egy kg/ha-hoz; range = -1 a táblázat alatt, +1 felette (vágott skála), 0 benne.
inline double scale_for(const Curve &k, double rate, int *range = nullptr) {
    if (range) *range = 0;
    if (rate < k.rate[0] - 1e-4) { if (range) *range = -1; return k.scale[0]; }
    if (rate > k.rate[k.n - 1] + 1e-4) { if (range) *range = 1; return k.scale[k.n - 1]; }
    for (int i = 1; i < k.n; i++)
        if (rate <= k.rate[i])
            return k.scale[i - 1] + (k.scale[i] - k.scale[i - 1]) * (rate - k.rate[i - 1]) / (k.rate[i] - k.rate[i - 1]);
    return k.scale[k.n - 1];
}

/// A legfinomabb változat, amelybe a mennyiség belefér; ha egyikbe sem, a legnagyobb.
inline int choose_variant(int c, double rate) {
    const int nv = variant_count(c);
    if (nv <= 1) return 0;
    const CropDef &cd = table().crop[c];
    int best = -1, biggest = 0;
    for (int i = 0; i < nv; ++i) {
        const double mx = cd.var[i].max_rate();
        if (mx > cd.var[biggest].max_rate()) biggest = i;
        if (rate <= mx + 1e-4 && (best < 0 || mx < cd.var[best].max_rate())) best = i;
    }
    return best >= 0 ? best : biggest;
}

/// Leforgatás: base fordulat = 1/10 ha (gépállandó, Accord: 85). Opciók: 1/10, 1/20, 1/50 ha.
static constexpr int TURN_OPTIONS = 3;
inline double test_area_ha(int option) { return option == 1 ? 0.05 : option == 2 ? 0.02 : 0.1; }
inline double turns(int option, double base = 85.0) { return (base > 0.1 ? base : 85.0) * 10.0 * test_area_ha(option); }

struct Result {
    int variant = 0;             // a választott változat (hajtás / betét)
    char setup[LABEL_LEN] = {};  // beállítási szöveg, pl. "A (nyitott)", "Z + MIKRO"
    double scale = 0;            // táblázati beállítás a kívánt mennyiséghez
    int range = 0;               // -1 a kívánt mennyiség a táblázat alatt / +1 felett
    double test_scale = 0;       // a leforgatás skálája (alap: a táblázati beállítás)
    int test_range = 0;          // -1 / +1: a leforgatási skála a táblázaton kívül
    double expected_kg = 0;      // a táblázat szerint várható tömeg a leforgatásnál
    bool corrected_valid = false;
    double corrected_scale = 0;  // a mérlegelés után javított skála
    int corrected_range = 0;
    double actual_rate = 0;      // kg/ha, amit a leforgatás a test_scale-en mutatott
    double kg_per_rev = 0;       // mért kg / fordulat (0 = nincs mérés)
    double seed_kg = 0;          // a vetendő hektárokhoz szükséges vetőmag
};

/// wanted_kg_ha > 0; measured_kg = 0, ha még nincs mérlegelve. test_scale <= 0: a táblázati beállítás.
/// revs > 0: a ténylegesen megszámolt fordulat (pl. 84,5 vagy megszakított leforgatás) – a kg/fordulat és a
/// kg/ha ebből számolódik; 0 = a kiválasztott opció (85 / 42,5 / 17).
inline Result calculate(int crop, double wanted_kg_ha, int turn_option, double measured_kg, double hectares,
                        double test_scale = 0, double revs = 0, double base = 85.0) {
    Result r;
    r.variant = choose_variant(crop, wanted_kg_ha);
    const Curve k = curve(crop, r.variant);
    if (crop >= 0 && crop < table().ncrop) {
        const char *src = table().crop[crop].var[r.variant].setup;
        int i = 0;
        for (; src[i] && i < LABEL_LEN - 1; ++i) r.setup[i] = src[i];
        r.setup[i] = 0;
    }
    if (k.n <= 0) return r;
    r.scale = scale_for(k, wanted_kg_ha, &r.range);
    r.test_scale = test_scale > 0 ? test_scale : r.scale;
    if (r.test_scale < k.scale[0] - 1e-4) r.test_range = -1;
    else if (r.test_scale > k.scale[k.n - 1] + 1e-4) r.test_range = 1;
    const double chart_test = rate_at(k, r.test_scale);
    r.expected_kg = chart_test * test_area_ha(turn_option);
    r.seed_kg = wanted_kg_ha * (hectares > 0 ? hectares : 0);
    if (measured_kg > 0) {
        const double n = revs > 0.05 ? revs : turns(turn_option, base);
        r.actual_rate = measured_kg / (n / ((base > 0.1 ? base : 85.0) * 10.0));
        r.kg_per_rev = measured_kg / n;
        if (chart_test > 0 && r.test_range == 0) {
            const double factor = r.actual_rate / chart_test;          // ez a vetőmagtétel a táblázathoz képest
            r.corrected_scale = scale_for(k, wanted_kg_ha / factor, &r.corrected_range);
            r.corrected_valid = true;
        }
    }
    return r;
}

/// Beállítás szövege (a táblázat változatából).
inline const char *setup_text(const Result &r) { return r.setup; }
/// Rövid tipp (0 = nincs külön megjegyzés).
enum class Note : int { None = 0, AboveChart, BelowChart, TestOutside, NewScaleMax, NewScaleMin, CrankAndWeigh };
inline Note note(const Result &r, bool weighed) {
    if (r.range > 0) return Note::AboveChart;
    if (r.range < 0) return Note::BelowChart;
    if (r.test_range != 0) return Note::TestOutside;
    if (weighed && r.corrected_valid && r.corrected_range > 0) return Note::NewScaleMax;
    if (weighed && r.corrected_valid && r.corrected_range < 0) return Note::NewScaleMin;
    if (!weighed) return Note::CrankAndWeigh;
    return Note::None;
}
inline const char *note_text(Note n) {
    switch (n) {
    case Note::AboveChart: return "A kívánt mennyiség a táblázat felett van";
    case Note::BelowChart: return "A kívánt mennyiség a táblázat alatt van";
    case Note::TestOutside: return "A leforgatási skála a táblázaton kívül esik";
    case Note::NewScaleMax: return "Az új skála a maximumon – nagyobb adagolókerék / betét kell";
    case Note::NewScaleMin: return "Az új skála a minimumon – finomabb beállítás kell";
    case Note::CrankAndWeigh: return "Állítsd be a skálát, forgasd le és mérd le";
    default: return "";
    }
}

} // namespace chart
} // namespace seeder

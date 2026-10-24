#pragma once
// Leforgatási segéd Accord DA / DL / DT / DF térfogat-állítós (változó volumenű) adagolóhoz
// (2–8 m), a gép beállítási táblázata ("Einstelltabelle") alapján. Tiszta logika, unit teszttel.
// Az ESC v0.10–0.16 (Isobus_Esp_Seeder) leforgatási segédjének átvétele, változatlan táblázattal.
//
//   Normál vetőmag: skála 10 – 110, "A" (nyitott) torlócsappantyú.
//   Aprómag:        skála 2,5 – 25, "Z" (zárt) torlócsappantyú, N (normál) vagy M (mikro) hajtás.
//   Leforgatás:     85 fordulat = 1/10 ha, 42,5 fordulat = 1/20 ha, 17 fordulat = 1/50 ha.
// A táblázat kiindulási érték; a leforgatás és mérlegelés korrigál az adott vetőmagtételre.
// FIGYELEM: csak a térfogat-állítós Accord adagolóra érvényes.
#include <cstdint>

namespace seeder {
namespace chart {

enum Crop : int {
    WHEAT = 0, RYE, BARLEY, OATS, FIELD_BEANS, PEAS, LUPINS, VETCHES, MAIZE, GRASS,   // normál vetőmag
    RAPE, RED_CLOVER, GRASS_FINE, STUBBLE_TURNIP,                                     // aprómag
    CROP_COUNT
};
enum Drive : int { DRIVE_NONE = 0, DRIVE_NORMAL = 1, DRIVE_MIKRO = 2 };

/// Magyar nevek (UTF-8, web és napló). A VT listák szövegét a pool generátor adja (ugyanez a sorrend).
inline const char *crop_name(int c) {
    static const char *const n[CROP_COUNT] = {"Búza", "Rozs", "Árpa", "Zab", "Lóbab", "Borsó", "Csillagfürt",
                                              "Bükköny", "Kukorica", "Fű", "Repce", "Vöröshere", "Fű (aprómag)",
                                              "Tarlórépa"};
    return c >= 0 && c < CROP_COUNT ? n[c] : "";
}
constexpr bool is_fine(int c) { return c >= RAPE && c < CROP_COUNT; }

struct Curve {
    const float *scale;
    const float *rate;      // kg/ha az adott skálaértéknél, növekvő
    int n;
};

namespace data {
static const float NS[21] = {10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110};
static const float WHEAT_R[21] = {34, 51, 69, 86, 104, 122, 140, 157, 174, 192, 210, 228, 246, 264, 281, 298, 316, 335, 352, 370, 387};
static const float RYE_R[21] = {33, 49, 66, 83, 100, 117, 134, 151, 168, 184, 200, 217, 235, 252, 269, 286, 302, 319, 337, 354, 371};
static const float BARLEY_R[21] = {32, 48, 64, 79, 95, 111, 127, 143, 159, 174, 190, 206, 222, 238, 253, 268, 284, 300, 316, 332, 348};
static const float OATS_R[21] = {24, 35, 47, 59, 71, 82, 94, 106, 118, 130, 141, 153, 165, 177, 189, 200, 212, 224, 236, 248, 260};
static const float BEANS_R[21] = {23, 42, 61, 79, 98, 116, 135, 154, 172, 191, 209, 228, 246, 265, 283, 302, 320, 338, 356, 374, 393};
static const float PEAS_R[21] = {21, 40, 59, 78, 97, 117, 136, 155, 174, 194, 213, 232, 251, 270, 289, 309, 328, 347, 366, 385, 404};
static const float LUPINS_R[21] = {28, 45, 62, 79, 96, 113, 130, 147, 164, 181, 198, 216, 234, 251, 268, 285, 302, 320, 337, 354, 371};
static const float VETCH_R[21] = {32, 51, 70, 89, 108, 127, 146, 165, 184, 203, 222, 241, 260, 279, 299, 317, 336, 355, 374, 393, 412};
static const float MAIZE_R[21] = {8, 24, 47, 70, 92, 115, 137, 156, 175, 194, 212, 231, 249, 267, 285, 304, 323, 342, 361, 380, 398};
static const float GRASS_S[5] = {15, 20, 25, 30, 35};
static const float GRASS_R[5] = {18, 26, 34, 42, 50};

static const float FS[10] = {2.5f, 5, 7.5f, 10, 12.5f, 15, 17.5f, 20, 22.5f, 25};
static const float RAPE_N[10] = {1.8f, 4.6f, 6.8f, 9.1f, 11.4f, 13.7f, 15.9f, 18.2f, 20.5f, 22.8f};
static const float RAPE_M[10] = {0.9f, 2.3f, 3.4f, 4.55f, 5.7f, 6.85f, 7.95f, 9.1f, 10.25f, 11.4f};
static const float CLOVER_N[10] = {2.3f, 5.3f, 8.6f, 12.0f, 15.3f, 18.0f, 21.3f, 24.0f, 26.6f, 27.5f};
static const float CLOVER_M[10] = {1.15f, 2.65f, 4.3f, 6.0f, 7.65f, 9.0f, 10.65f, 12.0f, 13.3f, 13.75f};
static const float GRASSF_S[8] = {7.5f, 10, 12.5f, 15, 17.5f, 20, 22.5f, 25};
static const float GRASSF_N[8] = {2.8f, 5.2f, 7.2f, 9.2f, 11.2f, 13.2f, 15.0f, 16.2f};
static const float GRASSF_M[8] = {1.4f, 2.6f, 3.6f, 4.6f, 5.6f, 6.6f, 7.5f, 8.1f};
static const float TURNIP_N[10] = {2.5f, 5.0f, 7.5f, 10.0f, 12.5f, 15.0f, 17.5f, 20.0f, 21.5f, 23.0f};
static const float TURNIP_M[10] = {1.25f, 2.5f, 3.75f, 5.0f, 6.25f, 7.5f, 8.75f, 10.0f, 10.75f, 11.5f};
} // namespace data

/// Egy növény görbéje; normál vetőmagnál a hajtás nem számít (egy görbe van).
inline Curve curve(int c, int drive) {
    using namespace data;
    const bool m = drive == DRIVE_MIKRO;
    switch (c) {
    case WHEAT: return {NS, WHEAT_R, 21};
    case RYE: return {NS, RYE_R, 21};
    case BARLEY: return {NS, BARLEY_R, 21};
    case OATS: return {NS, OATS_R, 21};
    case FIELD_BEANS: return {NS, BEANS_R, 21};
    case PEAS: return {NS, PEAS_R, 21};
    case LUPINS: return {NS, LUPINS_R, 21};
    case VETCHES: return {NS, VETCH_R, 21};
    case MAIZE: return {NS, MAIZE_R, 21};
    case GRASS: return {GRASS_S, GRASS_R, 5};
    case RAPE: return {FS, m ? RAPE_M : RAPE_N, 10};
    case RED_CLOVER: return {FS, m ? CLOVER_M : CLOVER_N, 10};
    case GRASS_FINE: return {GRASSF_S, m ? GRASSF_M : GRASSF_N, 8};
    case STUBBLE_TURNIP: return {FS, m ? TURNIP_M : TURNIP_N, 10};
    default: return {NS, WHEAT_R, 21};
    }
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

/// Aprómag: Mikro, ha a mennyiség belefér (finomabb skálafelbontás), különben Normál.
inline int choose_drive(int c, double rate) {
    if (!is_fine(c)) return DRIVE_NONE;
    const Curve m = curve(c, DRIVE_MIKRO);
    return rate <= m.rate[m.n - 1] + 1e-4 ? DRIVE_MIKRO : DRIVE_NORMAL;
}

/// Leforgatás: 85 fordulat = 1/10 ha.
static constexpr int TURN_OPTIONS = 3;
inline double turns(int option) { return option == 1 ? 42.5 : option == 2 ? 17.0 : 85.0; }
inline double test_area_ha(int option) { return turns(option) / 850.0; }

struct Result {
    int drive = DRIVE_NONE;      // aprómag: N vagy M
    char flap = 'A';             // 'A' normál vetőmag, 'Z' aprómag
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
inline Result calculate(int crop, double wanted_kg_ha, int turn_option, double measured_kg, double hectares,
                        double test_scale = 0) {
    Result r;
    r.drive = choose_drive(crop, wanted_kg_ha);
    r.flap = is_fine(crop) ? 'Z' : 'A';
    const Curve k = curve(crop, r.drive);
    r.scale = scale_for(k, wanted_kg_ha, &r.range);
    r.test_scale = test_scale > 0 ? test_scale : r.scale;
    if (r.test_scale < k.scale[0] - 1e-4) r.test_range = -1;
    else if (r.test_scale > k.scale[k.n - 1] + 1e-4) r.test_range = 1;
    const double chart_test = rate_at(k, r.test_scale);
    r.expected_kg = chart_test * test_area_ha(turn_option);
    r.seed_kg = wanted_kg_ha * (hectares > 0 ? hectares : 0);
    if (measured_kg > 0) {
        r.actual_rate = measured_kg / test_area_ha(turn_option);
        r.kg_per_rev = measured_kg / turns(turn_option);
        if (chart_test > 0 && r.test_range == 0) {
            const double factor = r.actual_rate / chart_test;          // ez a vetőmagtétel a táblázathoz képest
            r.corrected_scale = scale_for(k, wanted_kg_ha / factor, &r.corrected_range);
            r.corrected_valid = true;
        }
    }
    return r;
}

/// Beállítás szövege: csappantyú és aprómagnál a hajtás.
inline const char *setup_text(const Result &r) {
    if (r.flap == 'A') return "A (nyitott)";
    return r.drive == DRIVE_MIKRO ? "Z + MIKRO" : "Z + NORMÁL";
}
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
    case Note::NewScaleMax: return "Az új skála a maximumon – nagyobb adagolókerék kell";
    case Note::NewScaleMin: return "Az új skála a minimumon – aprómag-beállítás kell";
    case Note::CrankAndWeigh: return "Állítsd be a skálát, forgasd le és mérd le";
    default: return "";
    }
}

} // namespace chart
} // namespace seeder

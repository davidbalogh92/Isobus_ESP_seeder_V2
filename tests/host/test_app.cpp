// Az ISOBUS alkalmazásréteg tiszta függvényeinek unit tesztjei: TC geometria, kondenzált
// munkaállapot, DDOP felépítés minden gépváltozatra, VT szövegkódolás, geometria-vázlat választás.
#include "aux_n.hpp"
#include "pool_scaler.hpp"
#include "seeder_app.hpp"
#include "tc_link.hpp"
#include "vt_ui.hpp"
#include "vt_sink.hpp"
#include "seeder/vt_ids.hpp"

#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"
#include "seeder/vt_scale.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

using namespace seeder;

namespace {
int g_failures = 0;
int g_checks = 0;
std::vector<std::pair<const char *, std::function<void()>>> &registry() {
    static std::vector<std::pair<const char *, std::function<void()>>> r;
    return r;
}
struct Reg {
    Reg(const char *n, std::function<void()> f) { registry().emplace_back(n, std::move(f)); }
};
#define TEST(name)                      \
    static void name();                 \
    static Reg reg_##name(#name, name); \
    static void name()
#define CHECK(cond)                                                       \
    do {                                                                  \
        ++g_checks;                                                       \
        if (!(cond)) {                                                    \
            ++g_failures;                                                 \
            std::printf("  HIBA %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                 \
    } while (0)
#define CHECK_NEAR(a, b, tol)                                                                         \
    do {                                                                                              \
        ++g_checks;                                                                                   \
        const double va = (a), vb = (b);                                                              \
        if (std::fabs(va - vb) > (tol)) {                                                             \
            ++g_failures;                                                                             \
            std::printf("  HIBA %s:%d: %s = %.4f, várt %.4f (±%.4f)\n", __FILE__, __LINE__, #a, va, vb, \
                        static_cast<double>(tol));                                                    \
        }                                                                                             \
    } while (0)

} // namespace

TEST(geometry_mounted) {
    Settings s = default_settings();
    s.hitch_type = 0;
    s.geo_b_m = 1.5f;
    s.geo_y_m = 0.2f;
    normalize(s);
    const auto g = app::make_tc_geometry(s);
    CHECK(g.connector_type == 3);
    CHECK(g.connector_x_mm == 0);
    CHECK(g.boom_x_mm == -1500);
    CHECK(g.boom_y_mm == 200);
    CHECK(!g.has_pivot);
}

TEST(geometry_trailed_drawbar) {
    Settings s = default_settings();
    s.hitch_type = 2;
    s.geo_b_m = 4.0f;
    s.geo_c_m = 3.0f;
    normalize(s);
    const auto g = app::make_tc_geometry(s);
    CHECK(g.connector_type == 1);
    CHECK(g.connector_x_mm == 3000);   // vonópont a tengely előtt
    CHECK(g.boom_x_mm == -1000);       // csoroszlyák 1 m-rel a tengely mögött
    CHECK(!g.has_pivot);
}

TEST(geometry_steering_drawbar_has_pivot) {
    Settings s = default_settings();
    s.hitch_type = 1;
    s.geo_a_m = 1.0f;
    s.geo_b_m = 2.0f;
    s.geo_c_m = 3.0f;
    normalize(s);
    const auto g = app::make_tc_geometry(s);
    CHECK(g.connector_type == 2);
    CHECK(g.has_pivot);
    CHECK(g.pivot_x_mm == 3000);
    CHECK(g.connector_x_mm == 4000);
    CHECK(g.boom_x_mm == 1000);
}

TEST(connector_types) {
    CHECK(app::connector_type_for(0) == 3);
    CHECK(app::connector_type_for(1) == 2);
    CHECK(app::connector_type_for(2) == 1);
    CHECK(app::connector_type_for(3) == 4);
    CHECK(app::connector_type_for(4) == 8);
    CHECK(app::connector_type_for(99) == 3);   // ismeretlen → hárompont
}

TEST(sections_left_right_offsets) {
    Settings s = default_settings();       // 6 m, 2 szakasz, 3 + 3 m
    normalize(s);
    auto g = app::make_tc_geometry(s);
    CHECK(g.sections == 2);
    CHECK(g.sec_w_mm[0] == 3000 && g.sec_w_mm[1] == 3000);
    CHECK(g.sec_y_mm[0] == -1500 && g.sec_y_mm[1] == 1500);
    // aszimmetrikus: 2,5 + 3,5 m
    s.sec_w1 = 2.5f;
    normalize(s);
    g = app::make_tc_geometry(s);
    CHECK(g.sec_w_mm[0] == 2500 && g.sec_w_mm[1] == 3500);
    CHECK(g.sec_y_mm[0] == -1750 && g.sec_y_mm[1] == 1250);
    CHECK(g.sec_w_mm[0] + g.sec_w_mm[1] == g.width_mm);
}

TEST(single_section_full_width) {
    Settings s = default_settings();
    s.section_count = 1;
    normalize(s);
    const auto g = app::make_tc_geometry(s);
    CHECK(g.sections == 1);
    CHECK(g.sec_w_mm[0] == 6000);
    CHECK(g.sec_y_mm[0] == 0);
    // első/hátsó soros két adagoló: egy szakasz
    s = default_settings();
    s.meter_layout = 1;
    normalize(s);
    CHECK(app::make_tc_geometry(s).sections == 1);
}

TEST(condensed_work_state) {
    bool on[2] = {true, false};
    const std::uint32_t v = app::condensed_state(on, 2);
    CHECK((v & 0x3u) == 1u);
    CHECK(((v >> 2) & 0x3u) == 0u);
    CHECK((v >> 4) == 0x0FFFFFFFu);          // a nem létező szakaszok: 11
    bool none[1] = {false};
    CHECK(app::condensed_state(none, 0) == 0xFFFFFFFFu);
}

TEST(ddop_builds_for_all_variants) {
    for (int meters = 1; meters <= 2; ++meters)
        for (int sections = 1; sections <= 2; ++sections)
            for (int hitch = 0; hitch < 5; ++hitch)
                for (int sec = 0; sec <= 1; ++sec)
                    for (int geo = 0; geo <= 1; ++geo) {
                        Settings s = default_settings();
                        s.meter_count = meters;
                        s.section_count = sections;
                        s.hitch_type = hitch;
                        s.tc_sec = sec;
                        s.tc_geo = geo;
                        normalize(s);
                        isobus::DeviceDescriptorObjectPool p;
                        const bool ok = app::build_ddop(p, s, 0x123456789ABCDEFull);
                        CHECK(ok);
                        if (!ok)
                            std::printf("  DDOP hiba: adagoló %d, szakasz %d, csatl. %d, sec %d, geo %d\n", meters,
                                        sections, hitch, sec, geo);
                    }
}

TEST(ddop_rebuild_is_repeatable) {
    Settings s = default_settings();
    normalize(s);
    isobus::DeviceDescriptorObjectPool p;
    std::vector<std::uint8_t> a, b;
    CHECK(app::build_ddop(p, s, 1));
    CHECK(p.generate_binary_object_pool(a));
    CHECK(app::build_ddop(p, s, 1));          // ugyanazon a poolon újraépítve
    CHECK(p.generate_binary_object_pool(b));
    CHECK(a == b);
    CHECK(!a.empty());
}

TEST(latin1_conversion) {
    CHECK(app::vt_latin1("Árvíztűrő tükörfúrógép", 64) == std::string("\xC1rv\xEDzt\xFBr\xF5 t\xFCk\xF6rf\xFAr\xF3g\xE9p"));
    CHECK(app::vt_latin1("ŐŰ", 8) == std::string("\xD5\xDB"));
    CHECK(app::vt_latin1("abcdef", 3) == "abc");                  // levágás karakterre
    CHECK(app::vt_latin1("a – b", 16) == "a - b");                // gondolatjel
    CHECK(app::vt_latin1("€", 4) == "?");                         // nincs Latin-1-ben
    CHECK(app::vt_latin1(nullptr, 4).empty());
    CHECK(app::vt_latin1("", 4).empty());
}

TEST(geo_picture_choice) {
    CHECK(app::geo_picture(0, 1.0f, 0.0f) == 0);
    CHECK(app::geo_picture(1, 4.0f, 3.0f) == 1);   // kerék a csoroszlyák előtt
    CHECK(app::geo_picture(1, 2.0f, 3.0f) == 2);
    CHECK(app::geo_picture(2, 4.0f, 3.0f) == 3);
    CHECK(app::geo_picture(4, 2.0f, 3.0f) == 4);
}

// ------------------------------------------------------------------ pool méretezés (HD12 javítás)
namespace {
std::vector<std::uint8_t> load_pool() {
    std::ifstream f(SEEDER_POOL_FILE, std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
std::uint16_t rd16(const std::vector<std::uint8_t> &v, std::uint32_t o) {
    return static_cast<std::uint16_t>(v[o] | (v[o + 1] << 8));
}
// a softkey gyerekek: a kép (w×h × kf) a gomb közepén, és a gombon belül
bool keys_fit(const std::vector<std::uint8_t> &p, unsigned kw, unsigned kh, int &n) {
    const float kf = static_cast<float>(std::min(kw, kh)) / 80.0f;
    bool ok = true;
    n = 0;
    for (std::uint32_t i = 0; i < vt::SCALE_ENTRY_COUNT; ++i) {
        const auto &e = vt::SCALE_MAP[i];
        if (e.kind != vt::SC_KEY_CHILD || !e.w) continue;
        ++n;
        const int x = rd16(p, e.offset), y = rd16(p, e.offset + 2);
        const float w = e.w * kf, h = e.h * kf;
        if (x + w > kw + 0.5f || y + h > kh + 0.5f) ok = false;
        if (std::abs(x - (kw - w) / 2.0f) > 1.0f || std::abs(y - (kh - h) / 2.0f) > 1.0f) ok = false;
    }
    return ok;
}
} // namespace

TEST(pool_scaler_basic_120x80) {
    const auto pool = load_pool();
    CHECK(pool.size() > 1000);
    if (pool.size() < 1000) return;
    app::ScaleTarget t;
    t.mask_px = 400;
    t.key_w = 120;
    t.key_h = 80;
    const auto out = app::scale_pool(pool.data(), static_cast<std::uint32_t>(pool.size()), t);
    CHECK(out.size() == pool.size());
    int n = 0;
    CHECK(keys_fit(out, 120, 80, n));
    CHECK(n > 20);
    // a maszk tartalma 400 px-en változatlan, a képszélesség 60 marad
    for (std::uint32_t i = 0; i < vt::SCALE_ENTRY_COUNT; ++i) {
        const auto &e = vt::SCALE_MAP[i];
        if (e.kind == vt::SC_MASK_U16 || e.kind == vt::SC_MASK_I16 || e.kind == vt::SC_FONT)
            CHECK(rd16(out, e.offset) == rd16(pool, e.offset));
        if (e.kind == vt::SC_KEY_PIC_W) CHECK(rd16(out, e.offset) == e.w);
    }
}

TEST(pool_scaler_hd12_square_keys) {
    const auto pool = load_pool();
    CHECK(pool.size() > 1000);
    if (pool.size() < 1000) return;
    app::ScaleTarget t;
    t.mask_px = 480;
    t.key_w = 80;
    t.key_h = 80;
    const auto out = app::scale_pool(pool.data(), static_cast<std::uint32_t>(pool.size()), t);
    int n = 0;
    CHECK(keys_fit(out, 80, 80, n));
    bool any_mask = false;
    for (std::uint32_t i = 0; i < vt::SCALE_ENTRY_COUNT; ++i) {
        const auto &e = vt::SCALE_MAP[i];
        if (e.kind == vt::SC_KEY_PIC_W) CHECK(rd16(out, e.offset) == e.w);   // 80/80 → nem nő
        if (e.kind == vt::SC_MASK_U16 && rd16(pool, e.offset) == 100) {
            CHECK(rd16(out, e.offset) == 120);                                 // ×1,2
            any_mask = true;
        }
    }
    CHECK(any_mask);
}

TEST(pool_scaler_small_and_wide_keys) {
    const auto pool = load_pool();
    CHECK(pool.size() > 1000);
    if (pool.size() < 1000) return;
    for (auto kw : {60u, 64u, 100u, 140u}) {
        for (auto kh : {48u, 60u, 64u, 100u}) {
            app::ScaleTarget t;
            t.mask_px = 600;
            t.key_w = kw;
            t.key_h = kh;
            const auto out = app::scale_pool(pool.data(), static_cast<std::uint32_t>(pool.size()), t);
            int n = 0;
            CHECK(keys_fit(out, kw, kh, n));
            for (std::uint32_t i = 0; i < vt::SCALE_ENTRY_COUNT; ++i) {
                const auto &e = vt::SCALE_MAP[i];
                if (e.kind == vt::SC_KEY_PIC_W)
                    CHECK(rd16(out, e.offset) == static_cast<std::uint16_t>(std::lround(e.w * std::min(kw, kh) / 80.0)));
            }
        }
    }
}

TEST(pool_scaler_invalid_sizes_fall_back) {
    const auto pool = load_pool();
    CHECK(pool.size() > 1000);
    if (pool.size() < 1000) return;
    app::ScaleTarget t;
    t.mask_px = 0;   // ismeretlen: tervezési méret
    t.key_w = 0;
    t.key_h = 0;
    const auto out = app::scale_pool(pool.data(), static_cast<std::uint32_t>(pool.size()), t);
    int n = 0;
    CHECK(keys_fit(out, 80, 80, n));
    // túl rövid pool: változatlan másolat, nincs túlírás
    const auto shortp = app::scale_pool(pool.data(), 100, t);
    CHECK(shortp.size() == 100 && std::memcmp(shortp.data(), pool.data(), 100) == 0);
}

TEST(pool_scaler_fonts) {
    using app::scale_font;
    CHECK(scale_font(3, 1.0f, nullptr) == 3);              // 12×16 marad
    CHECK(scale_font(3, 2.0f, nullptr) == 6);              // 24×32
    CHECK(scale_font(5, 1.2f, nullptr) == 5);              // 16×24 → 19×29-ig: 16×24 a legnagyobb
    CHECK(scale_font(6, 0.5f, nullptr) == 3);              // 24×32 fele: 12×16
    CHECK(scale_font(2, 0.5f, nullptr) == 0);              // semmi sem fér: legkisebb
    // nem támogatott méret helyett a kisebb támogatott
    auto only_small = [](std::uint8_t c) { return c <= 3; };
    CHECK(scale_font(6, 1.0f, only_small) == 3);
    CHECK(scale_font(20, 1.0f, nullptr) == 20);            // érvénytelen kód változatlan
}

// --------------------------------------------------------------------------- virtuális terminál
namespace {
std::string vt_json(app::SeederApp &a) { return a.virtual_vt_json(); }
bool has(const std::string &s, const std::string &needle) { return s.find(needle) != std::string::npos; }
void run_app(app::SeederApp &a, float seconds) {
    for (int k = 0; k < static_cast<int>(seconds / 0.05f); ++k) a.update(0.05f, app::HwInputs{});
}
} // namespace

TEST(virtual_terminal_without_can) {
    // CAN / ISOBUS indítása nélkül is működik a virtuális terminál (Windows-program)
    app::Shared shared;
    {
        Settings st = default_settings();
        st.drive_type = 1;
        st.speed_src = 3;
        normalize(st);
        shared.core.load(st, Persistent{});
    }
    app::SeederApp a(shared);
    run_app(a, 0.5f);
    std::string j = vt_json(a);
    CHECK(has(j, "\"mask\":" + std::to_string(vt::DM_MAIN)));
    CHECK(has(j, std::string("\"pool\":\"") + vt::POOL_VERSION + "\""));
    // softkey: Beállítások (elengedéskor hat) → menü
    CHECK(a.virtual_vt_event("key", vt::K_MENU, 1, 0));
    CHECK(a.virtual_vt_event("key", vt::K_MENU, 0, 0));
    run_app(a, 0.3f);
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(vt::DM_MENU)));
    // vissza, majd vetés BE a VT softkey-jével
    a.virtual_vt_event("key", vt::K_BACK, 0, 0);
    a.virtual_vt_event("key", vt::K_MASTER, 0, 0);
    run_app(a, 0.3f);
    {
        std::lock_guard<std::mutex> g(shared.lock);
        CHECK(shared.core.status().master);
    }
    CHECK(has(vt_json(a), "\"" + std::to_string(vt::OP_K_MASTER) + "\":" + std::to_string(vt::PIC_K_MASTER[1])));
    // beviteli mező: paraméter (munkaszélesség, 0,01 m lépés) a virtuális terminálról
    const auto pid = static_cast<std::uint16_t>(vt::VT_INPUT_BASE + P_width_m);
    CHECK(a.virtual_vt_event("num", pid, 0, 450));
    {
        std::lock_guard<std::mutex> g(shared.lock);
        CHECK(std::fabs(shared.core.settings().width_m - 4.5f) < 1e-3f);
    }
    CHECK(has(vt_json(a), "\"" + std::to_string(pid) + "\":450"));
    CHECK(!a.virtual_vt_event("rossz", 1, 0, 0));
}

TEST(virtual_screen_json_escapes_latin1) {
    app::VirtualScreen v(1000);
    v.string_value(11000, std::string("\xC1rv\xEDzt\xFBr\xF5 \"t\xFCk\xF6rf\xFAr\xF3g\xE9p\""));
    const auto j = v.json("SD00000");
    CHECK(has(j, "\"11000\":\"\u00c1rv\u00edzt\u00fbr\u00f5 \\\"t") || has(j, "\"11000\":\"\xC3\x81rv"));
    const auto ver = v.version();
    v.numeric(5, 7);
    v.numeric(5, 7);   // azonos érték: a verzió csak egyszer nő
    CHECK(v.version() == ver + 1);
}

TEST(pool_scaler_vt2_replaces_aux_objects) {
    const auto pool = load_pool();
    CHECK(vt::AUX_OBJECT_COUNT == sizeof(vt::AUX_FUNCTIONS) / sizeof(vt::AUX_FUNCTIONS[0]));
    if (pool.size() < 1000) return;
    app::ScaleTarget t;
    const auto vt3 = app::scale_pool(pool.data(), static_cast<std::uint32_t>(pool.size()), t);
    t.aux_objects = false;
    const auto vt2 = app::scale_pool(pool.data(), static_cast<std::uint32_t>(pool.size()), t);
    CHECK(vt2.size() == pool.size());
    for (std::uint32_t i = 0; i < vt::AUX_OBJECT_COUNT; ++i) {
        const auto &a = vt::AUX_OBJECTS[i];
        CHECK(vt3[a.offset + 2] == 31);
        CHECK(rd16(vt3, a.offset) == vt::AUX_FUNCTIONS[i].id);
        CHECK(vt2[a.offset + 2] == 22);                       // String Variable
        CHECK(rd16(vt2, a.offset) == vt::AUX_FUNCTIONS[i].id); // az azonosító marad
        CHECK(rd16(vt2, a.offset + 3) == a.len - 5u);
    }
    // a többi bájt változatlan
    std::size_t diff = 0;
    for (std::size_t i = 0; i < vt2.size(); ++i) diff += vt2[i] != vt3[i];
    CHECK(diff <= vt::AUX_OBJECT_COUNT * 12u);
}

TEST(aux_assignments_roundtrip) {
    std::vector<app::AuxDevice> d = {{0xA00086001234ULL, 7, {{vt::AUX_MASTER, 1, 2}, {vt::AUX_CRANK, 5, 2}}},
                                     {0x1122334455667788ULL, 0, {}}};
    const auto b = app::aux_serialize(d);
    std::vector<app::AuxDevice> r;
    CHECK(app::aux_deserialize(b.data(), b.size(), r));
    CHECK(r.size() == 2 && r[0].name == d[0].name && r[0].model == 7 && r[0].list.size() == 2 &&
          r[0].list[1].function == vt::AUX_CRANK && r[0].list[1].input == 5 && r[1].list.empty());
    auto bad = b;
    bad.pop_back();
    CHECK(!app::aux_deserialize(bad.data(), bad.size(), r));
    CHECK(!app::aux_deserialize(nullptr, 0, r));

    // tárolás: eszközönként, a legutóbbi elöl, legfeljebb 4 eszköz
    std::vector<std::uint8_t> nvs;
    unsigned saves = 0;
    app::AuxN aux([](std::uint16_t, std::uint8_t) {});
    aux.set_storage([&nvs] { return nvs; }, [&nvs, &saves](const std::vector<std::uint8_t> &v) {
        nvs = v;
        ++saves;
        return true;
    });
    CHECK(aux.load(1, 0).empty());
    for (std::uint64_t n = 1; n <= 6; ++n) aux.store(n, 0, {{vt::AUX_LAMP1, static_cast<std::uint16_t>(n), 2}});
    CHECK(saves == 6);
    app::AuxN aux2([](std::uint16_t, std::uint8_t) {});
    aux2.set_storage([&nvs] { return nvs; }, nullptr);
    CHECK(aux2.load(6, 0).size() == 1 && aux2.load(6, 0)[0].input == 6);
    CHECK(aux2.load(3, 0).size() == 1);
    CHECK(aux2.load(2, 0).empty());          // kiesett (csak 4 eszköz)
    CHECK(aux2.load(6, 1).empty());          // más modellkód
    // régi poolból maradt, ismeretlen funkció nem töltődik vissza
    aux.store(9, 0, {{123, 1, 2}, {vt::AUX_ACK, 2, 2}});
    CHECK(aux.load(9, 0).size() == 1);
}

TEST(aux_buttons_drive_functions) {
    app::Shared shared;
    {
        Settings st = default_settings();
        st.drive_type = 1;   // elektromos
        st.speed_src = 3;
        st.lights_enable = true;
        st.sim_speed = 0.0f;   // leforgatás csak álló gépnél
        normalize(st);
        shared.core.load(st, Persistent{});
    }
    app::SeederApp a(shared);
    run_app(a, 0.3f);
    auto master = [&] {
        run_app(a, 0.1f);
        std::lock_guard<std::mutex> g(shared.lock);
        return shared.core.status().master;
    };
    auto lamp = [&] {
        run_app(a, 0.1f);
        std::lock_guard<std::mutex> g(shared.lock);
        return shared.core.status().lamp[0];
    };
    auto hold = [&] {
        run_app(a, 0.1f);
        std::lock_guard<std::mutex> g(shared.lock);
        return shared.core.status().crank.hold;
    };
    CHECK(!master());
    // lenyomás → egyszer vált; a nyomva tartás (ismétlődő 2-es) és az elengedés nem
    CHECK(a.aux_input(vt::AUX_MASTER, 1));
    CHECK(master());
    a.aux_input(vt::AUX_MASTER, 2);
    a.aux_input(vt::AUX_MASTER, 2);
    a.aux_input(vt::AUX_MASTER, 0);
    a.aux_input(vt::AUX_MASTER, 0);
    CHECK(master());
    a.aux_input(vt::AUX_MASTER, 1);
    CHECK(!master());
    // ismeretlen funkció
    CHECK(!a.aux_input(1234, 1));
    // munkalámpa a virtuális terminál „aux” eseményével (Windows-program próbája)
    CHECK(!lamp());
    CHECK(a.virtual_vt_event("aux", vt::AUX_LAMP1, 0, 1));
    CHECK(lamp());
    a.virtual_vt_event("aux", vt::AUX_LAMP1, 0, 0);
    CHECK(lamp());
    // leforgatás: nyomva tartásra fut, elengedésre áll
    a.aux_input(vt::AUX_CRANK, 1);
    CHECK(hold());
    a.aux_input(vt::AUX_CRANK, 2);
    CHECK(hold());
    a.aux_input(vt::AUX_CRANK, 0);
    CHECK(!hold());
    CHECK(has(a.aux_json(), "\"functions\":13"));
}

int main() {
    for (auto &[name, fn] : registry()) {
        const int before = g_failures;
        fn();
        std::printf("%s %s\n", g_failures == before ? "[ OK ]" : "[HIBA]", name);
    }
    std::printf("\n%d ellenőrzés, %d hiba\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

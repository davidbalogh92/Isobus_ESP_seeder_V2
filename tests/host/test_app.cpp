// Az ISOBUS alkalmazásréteg tiszta függvényeinek unit tesztjei: TC geometria, kondenzált
// munkaállapot, DDOP felépítés minden gépváltozatra, VT szövegkódolás, geometria-vázlat választás.
#include "aux_n.hpp"
#include "ext_display.hpp"
#include "wifi_service.hpp"
#include "keya.hpp"
#include "pool_scaler.hpp"
#include "seeder_app.hpp"
#include "tc_link.hpp"
#include "vt_ui.hpp"
#include "vt_sink.hpp"
#include "seeder/vt_ids.hpp"

#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"
#include "seeder/vt_scale.hpp"

#include <algorithm>
#include <array>
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

TEST(virtual_terminal_motor_tune_page) {
    app::Shared shared;
    {
        Settings st = default_settings();
        st.drive_type = 1;
        st.speed_src = 3;
        st.sim_speed = 0.0f;
        st.prefill_auto = 0;
        normalize(st);
        shared.core.load(st, Persistent{});
    }
    app::SeederApp a(shared);
    run_app(a, 0.3f);
    auto key = [&](std::uint16_t k) {
        a.virtual_vt_event("key", k, 0, 0);
        run_app(a, 0.3f);
    };
    key(vt::K_MENU);
    key(vt::K_PG_MOTOR);
    std::uint16_t drive_mask = 0;
    for (const auto &p : vt::PAGES)
        if (std::strcmp(p.id, "motor") == 0) drive_mask = p.mask;
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(drive_mask)));
    CHECK(has(vt_json(a), "\"" + std::to_string(drive_mask) + "\":" + std::to_string(vt::SK_MOTOR)));
    key(vt::K_TUNE_PAGE);   // előbb a tudnivalók
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(vt::DM_TUNE_INFO)));
    CHECK(has(vt_json(a), "kb. "));
    key(vt::K_TUNE_GO);
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(vt::DM_TUNE)));
    key(vt::K_TUNE_START);
    {
        std::lock_guard<std::mutex> g(shared.lock);
        CHECK(shared.core.status().tune_phase == TunePhase::StartUp);
    }
    run_app(a, 2.0f);
    CHECK(has(vt_json(a), "\"" + std::to_string(vt::OP_TUNE_STATE) + "\":" + std::to_string(vt::STR_TUNE[1])));
    // a lapról kilépve a mérés leáll
    key(vt::K_BACK);
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(drive_mask)));
    {
        std::lock_guard<std::mutex> g(shared.lock);
        CHECK(shared.core.status().tune_phase == TunePhase::Failed);
        CHECK(shared.core.outputs().motor_pwm_pct[0] < 30.0f);
    }
}

TEST(virtual_terminal_jam_test_page) {
    app::Shared shared;
    {
        Settings st = default_settings();
        st.drive_type = 1;
        st.motor_driver = 2;
        st.speed_src = 3;
        st.sim_speed = 0.0f;
        st.prefill_auto = 0;
        normalize(st);
        shared.core.load(st, Persistent{});
    }
    app::SeederApp a(shared);
    run_app(a, 0.3f);
    auto key = [&](std::uint16_t k) {
        a.virtual_vt_event("key", k, 0, 0);
        run_app(a, 0.3f);
    };
    key(vt::K_MENU);
    key(vt::K_GR_DRIVE);   // csoport almenü: Hajtás, motor
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(vt::DM_GR_DRIVE)));
    key(vt::K_PG_JAM);
    std::uint16_t jam_mask = 0;
    for (const auto &p : vt::PAGES)
        if (std::strcmp(p.id, "jam") == 0) jam_mask = p.mask;
    CHECK(has(vt_json(a), "\"" + std::to_string(jam_mask) + "\":" + std::to_string(vt::SK_JAMPG)));
    key(vt::K_JAM_PAGE);
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(vt::DM_JAM)));
    CHECK(has(vt_json(a), "\"" + std::to_string(vt::OP_JAM_DRV) + "\":" + std::to_string(vt::STR_DRV[2])));
    key(vt::K_JAM_START);
    {
        std::lock_guard<std::mutex> g(shared.lock);
        CHECK(shared.core.status().jam[0].phase >= JamPhase::TestFwd);
    }
    key(vt::K_BACK);
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(jam_mask)));
    {
        std::lock_guard<std::mutex> g(shared.lock);
        CHECK(shared.core.status().jam[0].phase == JamPhase::Normal);
    }
}

TEST(virtual_terminal_settings_groups) {
    app::Shared shared;
    app::SeederApp a(shared);
    run_app(a, 0.3f);
    auto key = [&](std::uint16_t k) {
        a.virtual_vt_event("key", k, 0, 0);
        run_app(a, 0.3f);
    };
    auto btn = [&](std::uint16_t b) {
        a.virtual_vt_event("button", b, 0, 0);
        run_app(a, 0.3f);
    };
    auto mask_is = [&](std::uint16_t m) { return has(vt_json(a), "\"mask\":" + std::to_string(m)); };
    key(vt::K_MENU);
    CHECK(mask_is(vt::DM_MENU));
    // egyoldalas csoport: közvetlenül az oldal, vissza a menübe
    key(vt::K_GR_RATE);
    CHECK(mask_is(vt::PAGES[vt::GROUPS[0].first_page].mask));
    key(vt::K_BACK);
    CHECK(mask_is(vt::DM_MENU));
    // több oldalas csoport érintőgombbal: almenü → oldal → vissza az almenübe → vissza a menübe
    std::uint16_t speed_mask = 0, speed_btn = 0;
    for (const auto &p : vt::PAGES)
        if (std::strcmp(p.id, "speed") == 0) {
            speed_mask = p.mask;
            speed_btn = p.button;
        }
    btn(vt::BTN_GR_MEASURE);
    CHECK(mask_is(vt::DM_GR_MEASURE));
    btn(speed_btn);
    CHECK(mask_is(speed_mask));
    key(vt::K_BACK);
    CHECK(mask_is(vt::DM_GR_MEASURE));
    key(vt::K_BACK);
    CHECK(mask_is(vt::DM_MENU));
    // minden oldal pontosan egy csoportban
    for (const auto &p : vt::PAGES) CHECK(p.group < sizeof(vt::GROUPS) / sizeof(vt::GROUPS[0]));
    // Hajtás csoport: a hajtás típusa szerint látszanak az oldalak
    auto hid = [&](std::uint16_t id) { return has(vt_json(a), "\"" + std::to_string(id) + "\":1"); };
    auto vis = [&](std::uint16_t id) { return has(vt_json(a), "\"" + std::to_string(id) + "\":0"); };
    auto set_drive = [&](int d) {
        std::lock_guard<std::mutex> g(shared.lock);
        shared.core.set_param(seeder::P_drive_type, static_cast<float>(d));
    };
    set_drive(0);   // talajhajtás: csak a Hajtás oldal
    btn(vt::BTN_GR_DRIVE);
    CHECK(mask_is(vt::DM_GR_DRIVE));
    CHECK(hid(vt::CONT_PG_MOTOR) && hid(vt::CONT_PG_JAM) && hid(vt::CONT_PG_KEYA));
    set_drive(1);   // PWM: motor + beszorulás, Keya nem
    run_app(a, 0.3f);
    CHECK(vis(vt::CONT_PG_MOTOR) && vis(vt::CONT_PG_JAM) && hid(vt::CONT_PG_KEYA));
    set_drive(2);   // Keya: minden
    run_app(a, 0.3f);
    CHECK(vis(vt::CONT_PG_KEYA) && vis(vt::CONT_PG_JAM));
}

TEST(virtual_terminal_wifi_service_button_and_icon) {
    app::Shared shared;
    app::SeederApp a(shared);
    std::vector<bool> radio;
    a.wifi().set_driver([&](bool on) { radio.push_back(on); });
    {
        std::lock_guard<std::mutex> g(shared.lock);
        shared.core.set_param(seeder::P_work_sw, 1);   // NO munkakapcsoló, érintkező nyitva: gép FENT
    }
    run_app(a, 0.3f);
    auto hidden = [&](std::uint16_t id) { return has(vt_json(a), "\"" + std::to_string(id) + "\":1"); };
    const auto str_is = [&](std::uint16_t id, const std::string &v) {
        return has(vt_json(a), "\"" + std::to_string(id) + "\":\"" + v + "\"");
    };
    CHECK(!a.wifi().on() && radio.empty());                 // alapból KI
    CHECK(hidden(vt::CONT_WIFI) && str_is(vt::STR_MENU_WIFI, "WiFi: KI"));
    a.virtual_vt_event("key", vt::K_MENU, 0, 0);
    run_app(a, 0.3f);
    a.virtual_vt_event("button", vt::BTN_WIFI, 0, 0);       // menü → WiFi
    run_app(a, 0.3f);
    CHECK(a.wifi().on() && radio.size() == 1 && radio[0]);
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(vt::DM_MENU)));   // a menüben marad
    CHECK(str_is(vt::STR_MENU_WIFI, "WiFi: BE 20p"));
    CHECK(!hidden(vt::CONT_WIFI) && has(vt_json(a), "\"" + std::to_string(vt::OUT_WIFI_MIN) + "\":20"));
    {
        std::lock_guard<std::mutex> g(shared.lock);
        CHECK(std::string(shared.core.status().message) == "WiFi szerviz mód BE");
    }
    // a gép leengedése (aktív munkakapcsoló) lekapcsolja, és utána nem kapcsolható be
    app::HwInputs hw;
    hw.work_contact = true;
    for (int k = 0; k < 20; ++k) a.update(0.05f, hw);
    CHECK(!a.wifi().on() && radio.size() == 2 && !radio[1]);
    a.virtual_vt_event("button", vt::BTN_WIFI, 0, 0);
    for (int k = 0; k < 10; ++k) a.update(0.05f, hw);
    CHECK(!a.wifi().on());
    {
        std::lock_guard<std::mutex> g(shared.lock);
        CHECK(std::string(shared.core.status().message).find("munkakapcsol") != std::string::npos);
    }
    // munkakapcsoló nélkül a "gép lent" nem tiltja
    {
        std::lock_guard<std::mutex> g(shared.lock);
        shared.core.set_param(seeder::P_work_sw, 0);
    }
    for (int k = 0; k < 10; ++k) a.update(0.05f, hw);
    a.virtual_vt_event("button", vt::BTN_WIFI, 0, 0);
    CHECK(a.wifi().on());
    CHECK(a.wifi().json().find("\"source\":\"VT\"") != std::string::npos);
}

TEST(virtual_terminal_tank_page_and_profile_names) {
    app::Shared shared;
    app::SeederApp a(shared);
    {
        std::lock_guard<std::mutex> g(shared.lock);
        shared.core.set_param(seeder::P_tank_fill, 1);   // közös
        shared.core.tank_set(0, 500.0f);
    }
    run_app(a, 0.3f);
    const auto str_is = [&](std::uint16_t id, const std::string &v) {
        return has(vt_json(a), "\"" + std::to_string(id) + "\":\"" + v + "\"");
    };
    CHECK(str_is(vt::STR_TANK_C, "500 kg"));
    a.virtual_vt_event("key", vt::K_MENU, 0, 0);
    run_app(a, 0.3f);
    a.virtual_vt_event("button", vt::BTN_PAGE_TANK, 0, 0);
    run_app(a, 0.3f);
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(vt::DM_TANK)));
    a.virtual_vt_event("num", vt::VT_IN_TANK1_ADD_DEF.id, 0, 250);   // tankolás +250 kg
    run_app(a, 0.3f);
    {
        std::lock_guard<std::mutex> g(shared.lock);
        CHECK(std::fabs(shared.core.status().tank_kg[0] - 750.0f) < 0.01f);
    }
    CHECK(str_is(vt::STR_TANK_C, "750 kg"));
    a.virtual_vt_event("key", vt::K_BACK, 0, 0);
    run_app(a, 0.3f);
    CHECK(has(vt_json(a), "\"mask\":" + std::to_string(vt::DM_MENU)));
    // gépprofil: a VT növénylista a táblázatból íródik
    seeder::chart::Table t;
    std::string err;
    CHECK(seeder::chart::parse_csv("Napraforgó;;Betét;10;2\nNapraforgó;;Betét;50;10\n", t, err));
    {
        std::lock_guard<std::mutex> g(shared.lock);
        seeder::chart::set_table(t);
        shared.core.profile_changed(true);
    }
    run_app(a, 0.5f);
    CHECK(str_is(vt::STR_CROP_0, "Napraforg\u00f3"));
    CHECK(str_is(vt::STR_CROP_1, ""));
    CHECK(str_is(vt::STR_SETUP_DYN, "Betét") || str_is(vt::STR_SETUP_DYN, "Bet\u00e9t"));
    seeder::chart::reset_table();
}

namespace {
/// Keya motor modell: a célfordulatot (ford/s, motor egységben) 0,15 s időállandóval követi, válaszol.
struct FakeKeya {
    int node = 1;
    bool enabled = false;
    float cmd = 0.0f, vel = 0.0f;
    double pos = 0.0;
    std::uint32_t faults = 0;
    bool alive = true;
    std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> out;   // válaszok
    unsigned enables = 0, vel_cmds = 0, hb = 0, params = 0;
    float last_cmd_sent = 0.0f;
    void rx(std::uint32_t id, const std::uint8_t *d, std::uint8_t len) {
        if (!alive || static_cast<int>(id >> 6) != node) return;
        const std::uint8_t c = id & 0x3F;
        auto reply_f = [&](float f) {
            std::vector<std::uint8_t> v(4);
            std::memcpy(v.data(), &f, 4);
            out.push_back({id, v});
        };
        switch (c) {
        case 0x00: enabled = false; out.push_back({id, std::vector<std::uint8_t>(4, 0)}); break;
        case 0x01: enabled = true; ++enables; out.push_back({id, std::vector<std::uint8_t>(4, 0)}); break;
        case 0x03: std::memcpy(&cmd, d, 4); last_cmd_sent = cmd; ++vel_cmds; break;
        case 0x0E: {
            std::vector<std::uint8_t> v(8, 0);
            v[0] = enabled ? 1 : 0;
            std::memcpy(v.data() + 4, &faults, 4);
            out.push_back({id, v});
            break;
        }
        case 0x11: reply_f(vel); break;
        case 0x12: reply_f(static_cast<float>(pos)); break;
        case 0x13: reply_f(1.5f); break;
        case 0x14: reply_f(13.2f); break;
        case 0x17: ++params; out.push_back({id, std::vector<std::uint8_t>(d, d + len)}); break;
        case 0x3F: ++hb; break;
        default: break;
        }
        (void)len;
    }
    void step(float dt) {
        const float target = enabled ? cmd : 0.0f;
        vel += (target - vel) * dt / (0.15f + dt);
        pos += vel * dt;
    }
};

void run_keya(app::SeederApp &a, FakeKeya &k, float seconds) {
    for (int i = 0; i < static_cast<int>(seconds / 0.05f); ++i) {
        k.step(0.05f);
        a.update(0.05f, app::HwInputs{});
        for (auto &r : k.out) a.on_can_frame(r.first, r.second.data(), static_cast<std::uint8_t>(r.second.size()));
        k.out.clear();
    }
}
} // namespace

TEST(keya_motor_follows_target_over_can) {
    app::Shared shared;
    {
        Settings st = default_settings();
        st.drive_type = 2;        // elektromos – Keya CAN-motor
        st.meter_count = 1;
        st.keya_unit = 1;         // motortengely
        st.keya_gear = 49.0f;
        st.motor_max_rpm = 120.0f;
        st.work_sw = 0;           // nincs munkakapcsoló
        st.speed_src = 3;
        st.sim_speed = 8.0f;
        st.prefill_auto = 0;
        normalize(st);
        shared.core.load(st, Persistent{});
    }
    app::SeederApp a(shared);
    FakeKeya k;
    a.set_can_sender([&](std::uint32_t id, const std::uint8_t *d, std::uint8_t len) {
        CHECK(id <= 0x7FF);
        k.rx(id, d, len);
        return true;
    });
    run_keya(a, k, 1.0f);
    CHECK(k.params >= 1);   // szívverés-időkorlát beírva
    CHECK(k.hb >= 3);
    {
        std::lock_guard<std::mutex> g(shared.lock);
        shared.core.set_master(true);
    }
    run_keya(a, k, 4.0f);
    float target, rpm;
    {
        std::lock_guard<std::mutex> g(shared.lock);
        target = shared.core.status().meter[0].target_rpm;
        rpm = shared.core.status().meter[0].rpm;
        CHECK(shared.core.outputs().motor_pwm_pct[0] > 1.0f);
        CHECK(!(shared.core.status().alarms_active & (1u << static_cast<int>(AlarmId::Motor1Fault))));
    }
    CHECK(k.enabled && k.enables >= 1);
    CHECK(target > 1.0f);
    CHECK(std::fabs(k.last_cmd_sent - target / 60.0f * 49.0f) < 0.05f);   // ford/s a motortengelyen
    CHECK(std::fabs(rpm - target) < target * 0.1f);                        // a pozícióból mért adagoló-fordulat
    CHECK(has(a.keya_json(), "\"online\":true"));
    // a motor elhallgat → riasztás
    k.alive = false;
    run_keya(a, k, 1.5f);
    {
        std::lock_guard<std::mutex> g(shared.lock);
        CHECK(shared.core.status().alarms_active & (1u << static_cast<int>(AlarmId::Motor1Fault)));
    }
    // vetés KI után letiltja (3 s álló cél)
    k.alive = true;
    {
        std::lock_guard<std::mutex> g(shared.lock);
        shared.core.set_master(false);
    }
    run_keya(a, k, 4.5f);
    CHECK(!k.enabled);
}

TEST(keya_setup_mode_rewrites_node_and_baud) {
    app::Shared shared;
    shared.core.load(default_settings(), Persistent{});   // a beállító mód hajtástípustól független
    app::SeederApp a(shared);
    FakeKeya k;
    k.node = 5;                         // ismeretlen címen, 500 kbit/s-on (gyári)
    int motor_kbps = 500, bus_kbps = 250, saved_node = 0, saved_baud = -1, saves = 0;
    a.set_can_sender([&](std::uint32_t id, const std::uint8_t *d, std::uint8_t len) {
        if (bus_kbps != motor_kbps) return true;   // eltérő sebesség: a motor nem hallja
        if (static_cast<int>(id >> 6) == k.node) {
            if ((id & 0x3F) == 0x17 && len == 8) {
                std::int32_t idx, val;
                std::memcpy(&idx, d, 4);
                std::memcpy(&val, d + 4, 4);
                if (idx == 31) saved_node = val;
                if (idx == 32) saved_baud = val;
            }
            if ((id & 0x3F) == 0x19) ++saves;
        }
        k.rx(id, d, len);
        return true;
    });
    a.keya().set_rebaud([&](int kbps) { bus_kbps = kbps; k.out.clear(); return true; });
    std::string err;
    CHECK(!a.keya().start_setup(0, 300, 1, err));          // érvénytelen sebesség
    CHECK(a.keya().start_setup(0, 500, 2, err));           // keresés, új cím: 2
    CHECK(a.keya().setup_busy());
    run_keya(a, k, 3.0f);
    CHECK(saved_node == 2 && saved_baud == 0 && saves == 1);
    CHECK(!k.enabled);
    CHECK(bus_kbps == 250);                                 // visszaállt az ISOBUS sebességre
    CHECK(a.keya().setup_busy());                           // vár a táp ki-be kapcsolására
    CHECK(has(a.keya().setup_json(), "ki-be"));
    // "táp ki-be": a motor az új címen, 250 kbit/s-on jelentkezik
    k.node = saved_node;
    motor_kbps = 250;
    run_keya(a, k, 1.0f);
    CHECK(!a.keya().setup_busy());
    CHECK(has(a.keya().setup_json(), "\"ok\":true"));
    // nem válaszoló motor → hiba, visszaáll 250-re
    k.alive = false;
    CHECK(a.keya().start_setup(3, 500, 1, err));
    run_keya(a, k, 4.0f);
    CHECK(!a.keya().setup_busy() && bus_kbps == 250);
    CHECK(has(a.keya().setup_json(), "\"ok\":false"));
}

TEST(keya_ignores_other_frames) {
    app::KeyaBus kb;
    std::uint8_t d[4] = {0, 0, 0x80, 0x3F};
    CHECK(!kb.on_frame(0x18FEF100u, d, 4));     // 29 bites ISOBUS
    CHECK(!kb.on_frame((5u << 6) | 0x11, d, 4)); // ismeretlen node
    CHECK(kb.on_frame((1u << 6) | 0x11, d, 4));
}

// ------------------------------------------------------------------ WiFi szerviz mód
TEST(wifi_service_rules) {
    app::WifiService w;
    std::vector<bool> drv;
    w.set_driver([&](bool on) { drv.push_back(on); });
    CHECK(!w.on() && w.state() == app::WifiService::Off && w.minutes_left() == 0);   // alapból KI
    // aktív munkakapcsolónál nem kapcsolható be
    w.tick(0.05f, true, 0, false, 20);
    CHECK(w.request(true, app::WifiService::Source::Vt) == app::WifiService::Result::InhibitedWork);
    CHECK(!w.on() && drv.empty());
    w.tick(0.05f, false, 0, false, 20);
    CHECK(w.request(true, app::WifiService::Source::Display) == app::WifiService::Result::Ok);
    CHECK(w.on() && drv.size() == 1 && drv[0]);
    CHECK(w.minutes_left() == 20 && w.state() == app::WifiService::On);
    // tétlenség: 20 perc után lekapcsol
    for (int i = 0; i < 19 * 60; ++i) w.tick(1.0f, false, 0, false, 20);
    CHECK(w.on() && w.minutes_left() == 1);
    // kliens csatlakozik: nem fogy, és a távozásától újra a teljes idő
    for (int i = 0; i < 120; ++i) w.tick(1.0f, false, 1, false, 20);
    CHECK(w.on() && w.state() == app::WifiService::Client);
    w.tick(1.0f, false, 0, false, 20);
    CHECK(w.minutes_left() == 20);
    for (int i = 0; i < 20 * 60 + 2; ++i) w.tick(1.0f, false, 0, false, 20);
    CHECK(!w.on() && drv.size() == 2 && !drv[1]);
    CHECK(w.json().find("tétlenség") != std::string::npos);
    // a munkakapcsoló aktiválódása azonnal lekapcsol – OTA alatt nem, és OTA alatt kézzel sem kapcsolható ki
    w.request(true, app::WifiService::Source::Web);
    w.tick(0.05f, false, 0, true, 20);
    CHECK(w.state() == app::WifiService::Ota);
    w.tick(0.05f, true, 0, true, 20);
    CHECK(w.on());
    CHECK(w.request(false, app::WifiService::Source::Vt) == app::WifiService::Result::BusyOta && w.on());
    for (int i = 0; i < 61 * 60; ++i) w.tick(1.0f, false, 0, true, 5);   // OTA alatt az időkorlát sem jár le
    CHECK(w.on());
    w.tick(0.05f, true, 0, false, 5);
    CHECK(!w.on());
    // az időkorlát 5–60 perc közé szorul
    w.tick(0.05f, false, 0, false, 5);
    CHECK(w.request(true, app::WifiService::Source::Input) == app::WifiService::Result::Ok);
    w.tick(0.05f, false, 0, false, 1);
    CHECK(w.minutes_left() == 5);
    w.tick(0.05f, false, 0, false, 500);
    CHECK(w.minutes_left() == 60);
    CHECK(w.toggle(app::WifiService::Source::Vt) == app::WifiService::Result::Ok && !w.on());
}

// ------------------------------------------------------------------ külső kijelző (protokoll logika)
namespace {
struct FakeDisplay {
    std::vector<std::array<std::uint8_t, 8>> tx;
    app::ExtDisplay d;
    int light_calls = 0, wifi_calls = 0;
    app::ExtAck wifi_answer{};
    FakeDisplay() {
        d.set_sender([this](const std::uint8_t *f) {
            std::array<std::uint8_t, 8> a{};
            std::memcpy(a.data(), f, 8);
            tx.push_back(a);
            return true;
        });
        app::ExtDisplay::Handlers h;
        h.light = [this](const extdisp_light_command_t &c) {
            ++light_calls;
            return c.light == 2 ? app::ExtAck{EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NO_SUCH_UNIT} : app::ExtAck{};
        };
        h.wifi = [this](const extdisp_wifi_command_t &) {
            ++wifi_calls;
            return wifi_answer;
        };
        d.set_handlers(h);
    }
    int count(std::uint8_t id) const {
        int n = 0;
        for (const auto &f : tx) n += f[0] == id;
        return n;
    }
    const std::array<std::uint8_t, 8> *last(std::uint8_t id) const {
        for (auto it = tx.rbegin(); it != tx.rend(); ++it)
            if ((*it)[0] == id) return &*it;
        return nullptr;
    }
};
app::ExtDisplayMachine machine() {
    app::ExtDisplayMachine m;
    m.caps.machine_type = EXTDISP_MACHINE_SEEDER;
    m.caps.protocol_version = EXTDISP_PROTOCOL_VERSION;
    m.caps.tank_count = 2;
    m.caps.tank_mode = EXTDISP_TANK_MODE_SHARED;
    m.caps.feeder_count = 2;
    m.caps.light_count = 2;
    m.caps.features = EXTDISP_FEATURE_LIGHTS | EXTDISP_FEATURE_WIFI_SERVICE;
    m.caps.fw_major = 2;
    m.flags = EXTDISP_ST_FLAG_MOVING | EXTDISP_ST_FLAG_LIGHT2;
    m.speed_ckmh = 812;
    return m;
}
void hb(app::ExtDisplay &d, std::uint8_t counter, std::uint8_t flags) {
    extdisp_heartbeat_t h{counter, flags, EXTDISP_SCREEN_MAIN};
    std::uint8_t f[8];
    extdisp_encode_heartbeat(&h, f);
    d.on_frame(f, 8);
}
} // namespace

TEST(ext_display_capabilities_and_status) {
    FakeDisplay fd;
    auto m = machine();
    fd.d.tick(1000, false, m);   // nincs kijelző a buszon: semmit nem küld
    CHECK(fd.tx.empty());
    fd.d.tick(1020, true, m);    // megjelent: képességek + gépállapot azonnal
    CHECK(fd.count(EXTDISP_ID_CAPABILITIES) == 1 && fd.count(EXTDISP_ID_MACHINE_STATUS) == 1);
    extdisp_capabilities_t c{};
    CHECK(extdisp_decode_capabilities(fd.last(EXTDISP_ID_CAPABILITIES)->data(), 8, &c) == EXTDISP_OK);
    CHECK(c.machine_type == EXTDISP_MACHINE_SEEDER && c.protocol_version == 1 && c.feeder_count == 2 &&
          c.tank_count == 2 && c.light_count == 2 && c.fw_major == 2);
    CHECK((c.features & EXTDISP_FEATURE_MACHINE_CALIB) == 0);   // a leforgatás gomb rejtve (2. kör)
    CHECK((*fd.last(EXTDISP_ID_CAPABILITIES))[4] == 0x20);       // adagolók a bit4–5-ben, közös tartály
    extdisp_machine_status_t st{};
    CHECK(extdisp_decode_machine_status(fd.last(EXTDISP_ID_MACHINE_STATUS)->data(), 8, &st) == EXTDISP_OK);
    CHECK(st.counter == 0 && st.speed_ckmh == 812 && st.flags == (EXTDISP_ST_FLAG_MOVING | EXTDISP_ST_FLAG_LIGHT2));
    CHECK((*fd.last(EXTDISP_ID_MACHINE_STATUS))[5] == 0xFF);    // tartalék bájt
    // 200 ms-onként gépállapot (számláló nő), 5 s-onként képességek
    for (std::uint32_t t = 1040; t <= 6020; t += 20) fd.d.tick(t, true, m);
    CHECK(fd.count(EXTDISP_ID_MACHINE_STATUS) >= 25 && fd.count(EXTDISP_ID_MACHINE_STATUS) <= 27);
    CHECK(fd.count(EXTDISP_ID_CAPABILITIES) == 2);
    CHECK(extdisp_decode_machine_status(fd.last(EXTDISP_ID_MACHINE_STATUS)->data(), 8, &st) == EXTDISP_OK && st.counter >= 24);
    CHECK(fd.count(EXTDISP_ID_WIFI_STATUS) == 0 && fd.count(EXTDISP_ID_TANK_STATUS) == 0);
    // eltűnik, majd újra megjelenik: azonnal újra képességek
    fd.d.tick(6040, false, m);
    fd.d.tick(6060, true, m);
    CHECK(fd.count(EXTDISP_ID_CAPABILITIES) == 3);
    // WiFi állapot 1 s-onként, amíg be van; kikapcsoláskor még egyszer KI
    m.wifi_state = EXTDISP_WIFI_ON;
    m.wifi_minutes = 20;
    for (std::uint32_t t = 6080; t <= 8080; t += 20) fd.d.tick(t, true, m);
    CHECK(fd.count(EXTDISP_ID_WIFI_STATUS) == 3);
    extdisp_wifi_status_t w{};
    CHECK(extdisp_decode_wifi_status(fd.last(EXTDISP_ID_WIFI_STATUS)->data(), 8, &w) == EXTDISP_OK && w.state == 1 && w.minutes_left == 20);
    m.wifi_state = EXTDISP_WIFI_OFF;
    fd.d.tick(8100, true, m);
    fd.d.tick(9500, true, m);
    CHECK(fd.count(EXTDISP_ID_WIFI_STATUS) == 4);
    CHECK(extdisp_decode_wifi_status(fd.last(EXTDISP_ID_WIFI_STATUS)->data(), 8, &w) == EXTDISP_OK && w.state == 0);
    // tartály: csak ha a funkció be van jelentve
    m.tank_n = 1;
    m.tanks[0] = {EXTDISP_TANK_SHARED, 8505, 64};
    fd.d.tick(11000, true, m);
    CHECK(fd.count(EXTDISP_ID_TANK_STATUS) == 0);
    m.caps.features |= EXTDISP_FEATURE_TANK;
    fd.d.tick(12000, true, m);
    extdisp_tank_status_t ts{};
    CHECK(fd.count(EXTDISP_ID_TANK_STATUS) == 1 &&
          extdisp_decode_tank_status(fd.last(EXTDISP_ID_TANK_STATUS)->data(), 8, &ts) == EXTDISP_OK &&
          ts.remaining_hg == 8505 && ts.fill_percent == 64);
}

TEST(ext_display_heartbeat_timeout_and_stop) {
    FakeDisplay fd;
    const auto m = machine();
    fd.d.tick(100, true, m);
    CHECK(!fd.d.connected());
    hb(fd.d, 1, 0);
    fd.d.tick(120, true, m);
    CHECK(fd.d.connected() && !fd.d.stop_held());
    const std::size_t before = fd.tx.size();
    hb(fd.d, 2, EXTDISP_HB_FLAG_STOP);   // STOP: szívverésre nincs nyugta
    fd.d.tick(140, true, m);
    CHECK(fd.d.stop_held() && fd.d.stop_edges() == 1);
    CHECK(fd.count(EXTDISP_ID_ACK) == 0 && fd.tx.size() == before);
    hb(fd.d, 3, EXTDISP_HB_FLAG_STOP);   // nyomva tartva: nem új él
    hb(fd.d, 4, EXTDISP_HB_FLAG_START);
    fd.d.tick(160, true, m);
    CHECK(fd.d.stop_edges() == 1 && !fd.d.stop_held() && fd.d.start_held());
    // 1 s kimaradás: a kapcsolat elveszett, a gombok elengedettnek számítanak
    fd.d.tick(1100, true, m);
    CHECK(fd.d.connected());
    fd.d.tick(1170, true, m);
    CHECK(!fd.d.connected() && !fd.d.start_held());
    hb(fd.d, 5, EXTDISP_HB_FLAG_STOP);
    fd.d.tick(1180, true, m);
    CHECK(fd.d.connected() && fd.d.stop_held() && fd.d.stop_edges() == 2);
    fd.d.tick(1200, false, m);           // a kijelző eltűnt a buszról
    CHECK(!fd.d.connected() && !fd.d.stop_held());
}

TEST(ext_display_commands_and_acks) {
    FakeDisplay fd;
    const auto m = machine();
    fd.d.tick(0, true, m);
    auto ack = [&](std::uint8_t id) {
        extdisp_ack_t a{};
        const auto *f = fd.last(EXTDISP_ID_ACK);
        if (!f || extdisp_decode_ack(f->data(), 8, &a) != EXTDISP_OK || a.command_id != id) a.result = 0xEE;
        return a;
    };
    std::uint8_t f[8];
    // lámpa 1 vált → OK
    extdisp_light_command_t lc{1, EXTDISP_LIGHT_TOGGLE};
    extdisp_encode_light_command(&lc, f);
    fd.d.on_frame(f, 8);
    CHECK(fd.light_calls == 0);   // a fogadó szálon nem hajt végre
    fd.d.tick(20, true, m);
    CHECK(fd.light_calls == 1 && ack(EXTDISP_ID_LIGHT_COMMAND).result == EXTDISP_ACK_OK);
    CHECK((*fd.last(EXTDISP_ID_ACK))[3] == 0 && (*fd.last(EXTDISP_ID_ACK))[4] == 0xFF);
    // lámpa 2: a handler tiltja (nincs ilyen) → tiltva, ok 7
    lc.light = 2;
    extdisp_encode_light_command(&lc, f);
    fd.d.on_frame(f, 8);
    fd.d.tick(40, true, m);
    CHECK(ack(EXTDISP_ID_LIGHT_COMMAND).result == EXTDISP_ACK_INHIBITED && ack(EXTDISP_ID_LIGHT_COMMAND).inhibit_reason == 7);
    // hibás mező (3. lámpa, ERR_VALUE) → hibás adat, a handler nem hívódik
    const std::uint8_t bad_light[8] = {EXTDISP_ID_LIGHT_COMMAND, 3, 1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    fd.d.on_frame(bad_light, 8);
    fd.d.tick(60, true, m);
    CHECK(fd.light_calls == 2 && ack(EXTDISP_ID_LIGHT_COMMAND).result == EXTDISP_ACK_BAD_DATA);
    // rövid keret → hibás adat
    fd.d.on_frame(bad_light, 5);
    fd.d.tick(80, true, m);
    CHECK(ack(EXTDISP_ID_LIGHT_COMMAND).result == EXTDISP_ACK_BAD_DATA);
    // WiFi: a handler válasza megy vissza (tiltva: munkakapcsoló)
    fd.wifi_answer = {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_WORK_SWITCH};
    extdisp_wifi_command_t wc{EXTDISP_WIFI_CMD_ON};
    extdisp_encode_wifi_command(&wc, f);
    fd.d.on_frame(f, 8);
    fd.d.tick(100, true, m);
    CHECK(fd.wifi_calls == 1 && ack(EXTDISP_ID_WIFI_COMMAND).result == EXTDISP_ACK_INHIBITED &&
          ack(EXTDISP_ID_WIFI_COMMAND).inhibit_reason == EXTDISP_INHIBIT_WORK_SWITCH);
    const std::uint8_t bad_wifi[8] = {EXTDISP_ID_WIFI_COMMAND, 7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    fd.d.on_frame(bad_wifi, 8);
    fd.d.tick(120, true, m);
    CHECK(fd.wifi_calls == 1 && ack(EXTDISP_ID_WIFI_COMMAND).result == EXTDISP_ACK_BAD_DATA);
    // leforgatás (0x10–0x12): tiltva, ok 8 (a funkció ezen a gépen még nincs)
    extdisp_calib_command_t cc{EXTDISP_CALIB_ACT_PREPARE, 1, 85};
    extdisp_encode_calib_command(&cc, f);
    fd.d.on_frame(f, 8);
    fd.d.tick(140, true, m);
    CHECK(ack(EXTDISP_ID_CALIB_COMMAND).result == EXTDISP_ACK_INHIBITED && ack(EXTDISP_ID_CALIB_COMMAND).inhibit_reason == 8);
    extdisp_calib_weight_t cw{1, 1234};
    extdisp_encode_calib_weight(&cw, f);
    fd.d.on_frame(f, 8);
    fd.d.tick(160, true, m);
    CHECK(ack(EXTDISP_ID_CALIB_WEIGHT).inhibit_reason == 8);
    // tartály: nincs handler → tiltva, ok 8; ismeretlen parancs → tiltva, ok 8
    extdisp_tank_command_t tc{0, EXTDISP_TANK_ACT_ADD, 2500};
    extdisp_encode_tank_command(&tc, f);
    fd.d.on_frame(f, 8);
    const std::uint8_t unknown[8] = {0x05, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    fd.d.on_frame(unknown, 8);
    fd.d.tick(180, true, m);
    CHECK(fd.count(EXTDISP_ID_ACK) == 10);
    CHECK(ack(0x05).result == EXTDISP_ACK_INHIBITED && ack(0x05).inhibit_reason == 8);
    // állapot-azonosító a kijelzőtől: nem parancs, nincs nyugta
    const std::uint8_t status_id[8] = {EXTDISP_ID_MACHINE_STATUS, 0, 0, 0, 0, 0xFF, 0xFF, 0xFF};
    fd.d.on_frame(status_id, 8);
    fd.d.tick(200, true, m);
    CHECK(fd.count(EXTDISP_ID_ACK) == 10);
    CHECK(fd.d.json().find("\"acks\":10") != std::string::npos);
    // kapcsolat nélkül a sorban álló parancs elvész (nincs kinek nyugtázni)
    extdisp_encode_light_command(&lc, f);
    fd.d.on_frame(f, 8);
    fd.d.tick(220, false, m);
    fd.d.tick(240, true, m);
    CHECK(fd.count(EXTDISP_ID_ACK) == 10);
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

// Az ISOBUS alkalmazásréteg tiszta függvényeinek unit tesztjei: TC geometria, kondenzált
// munkaállapot, DDOP felépítés minden gépváltozatra, VT szövegkódolás, geometria-vázlat választás.
#include "seeder_app.hpp"
#include "tc_link.hpp"
#include "vt_ui.hpp"

#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"

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

int main() {
    for (auto &[name, fn] : registry()) {
        const int before = g_failures;
        fn();
        std::printf("%s %s\n", g_failures == before ? "[ OK ]" : "[HIBA]", name);
    }
    std::printf("\n%d ellenőrzés, %d hiba\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}

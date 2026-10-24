// PC-s demószerver a webes felülethez: ugyanaz a JSON API (src/app/web_api.cpp) és ugyanaz a
// vezérlőmag fut, mint az ESP32-n, szimulált sebességgel, turbinával és adagolókkal (ISOBUS nélkül).
// Indítás: web_demo [port=8080]   majd böngészőben: http://localhost:8080
#include "web_api.hpp"

#include "httplib.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <thread>

#ifndef WEB_DIR
#define WEB_DIR "web"
#endif

int main(int argc, char **argv) {
    const int port = argc > 1 ? std::atoi(argv[1]) : 8080;
    app::Shared shared;
    {
        seeder::Settings s = seeder::default_settings();
        s.speed_src = 3;
        s.sim_inputs = 1;
        s.lights_enable = s.valve_enable = s.pressure_enable = s.temp_enable = s.scale_enable = 1;
        seeder::normalize(s);
        seeder::Persistent p;
        p.total.area_m2 = 456780;
        p.total.kg[0] = 91350;
        p.total.kg[1] = 60200;
        shared.core.load(s, p);
    }
    std::atomic<bool> run{true};
    std::thread ticker([&] {
        while (run) {
            {
                std::lock_guard<std::mutex> g(shared.lock);
                seeder::TickInputs in;
                in.dt_s = 0.05f;
                in.pressure_v = 1.9f;          // szimulált analóg értékek
                in.temp_c[0] = 57.5f;
                in.temp_c[1] = 44.0f;
                in.scale_valid = true;
                in.scale_raw = 1000000.0f;
                shared.core.tick(in);
                shared.core.settings_dirty = shared.core.persistent_dirty = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    httplib::Server srv;
    auto api = [&](const httplib::Request &req, httplib::Response &res) {
        const auto r = app::handle_api(shared, nullptr, req.method, req.path, req.body,
                                       "{\"board\":\"PC demó (szimuláció)\",\"can_state\":\"-\"}", "demo", "PC-DEMO");
        res.status = r.status;
        res.set_content(r.body, r.content_type);
    };
    srv.Get("/", [](const httplib::Request &, httplib::Response &res) {
        std::ifstream f(std::string(WEB_DIR) + "/index.html");
        std::stringstream ss;
        ss << f.rdbuf();
        res.set_content(ss.str(), "text/html; charset=utf-8");
    });
    srv.Get("/api/logs", [](const httplib::Request &, httplib::Response &res) {
        res.set_content("[{\"ms\":0,\"level\":\"INFO\",\"message\":\"PC demó\"}]", "application/json");
    });
    for (const char *p : {"/api/state", "/api/schema", "/api/config", "/api/crops"}) srv.Get(p, api);
    for (const char *p : {"/api/config", "/api/param", "/api/cmd"}) srv.Post(p, api);
    std::printf("Webes demó: http://localhost:%d\n", port);
    srv.listen("0.0.0.0", port);
    run = false;
    ticker.join();
}

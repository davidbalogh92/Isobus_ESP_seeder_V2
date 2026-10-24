#include "web_api.hpp"

#include <cstdio>

namespace app {
namespace {
ApiResult ok(std::string body) {
    ApiResult r;
    r.status = 200;
    r.body = std::move(body);
    return r;
}
ApiResult error(const std::string &msg) {
    ApiResult r;
    r.status = 400;
    r.body = "{\"ok\":false,\"error\":\"" + seeder::json_escape(msg.c_str()) + "\"}";
    return r;
}
const char *b(bool v) { return v ? "true" : "false"; }
} // namespace

std::string isobus_diag_json(const IsobusDiag &d) {
    char buf[600];
    std::snprintf(buf, sizeof(buf),
                  "{\"claimed\":%s,\"address\":%d,\"vt_found\":%s,\"vt_connected\":%s,\"vt_w\":%u,\"vt_h\":%u,"
                  "\"sk_w\":%u,\"sk_h\":%u,\"phys_keys\":%u,\"virt_keys\":%u,\"vt_version\":%d,\"tc_found\":%s,"
                  "\"tc_connected\":%s,\"ddop_uploads\":%u,\"vt_sent\":%u,\"vt_failed\":%u,\"page\":\"%s\"}",
                  b(d.address_claimed), d.address, b(d.vt_found), b(d.vt_connected), d.vt_width, d.vt_height,
                  d.vt_softkey_w, d.vt_softkey_h, d.vt_phys_keys, d.vt_virt_keys, d.vt_version, b(d.tc_found),
                  b(d.tc_connected), d.ddop_uploads, d.vt_cmd_sent, d.vt_cmd_failed,
                  seeder::json_escape(d.page.c_str()).c_str());
    return buf;
}

ApiResult handle_api(Shared &shared, SeederApp *isobus, const std::string &method, const std::string &path,
                     const std::string &body, const std::string &hw_json, const char *fw_version, const char *ssid) {
    if (method == "GET" && path == "/api/state") {
        std::string st, cfg;
        {
            std::lock_guard<std::mutex> g(shared.lock);
            st = shared.core.status_json();
        }
        const IsobusDiag d = isobus ? isobus->diag() : IsobusDiag{};
        return ok("{\"status\":" + st + ",\"isobus\":" + isobus_diag_json(d) + ",\"hw\":" +
                  (hw_json.empty() ? std::string("{}") : hw_json) + ",\"fw\":\"" + seeder::json_escape(fw_version) +
                  "\",\"ssid\":\"" + seeder::json_escape(ssid) + "\"}");
    }
    if (method == "GET" && path == "/api/schema") return ok(seeder::schema_json());
    if (method == "GET" && path == "/api/crops") {
        // a leforgatási segéd növénylistája és fordulatszám-opciói
        std::string o = "{\"crops\":[";
        for (int i = 0; i < seeder::chart::CROP_COUNT; ++i) {
            if (i) o += ',';
            o += "\"" + seeder::json_escape(seeder::chart::crop_name(i)) + "\"";
        }
        o += "],\"turns\":[";
        for (int i = 0; i < seeder::chart::TURN_OPTIONS; ++i) {
            if (i) o += ',';
            char b[16];
            std::snprintf(b, sizeof(b), "%.1f", seeder::chart::turns(i));
            o += b;
        }
        return ok(o + "]}");
    }
    if (method == "GET" && path == "/api/config") {
        std::lock_guard<std::mutex> g(shared.lock);
        return ok(seeder::settings_to_json(shared.core.settings()));
    }
    if (method == "POST" && path == "/api/config") {
        std::string err;
        int applied = 0;
        {
            std::lock_guard<std::mutex> g(shared.lock);
            seeder::Settings s = shared.core.settings();
            applied = seeder::settings_from_json(s, body.c_str(), &err);
            if (applied > 0) shared.core.replace_settings(s);
        }
        if (applied < 0) return error(err);
        return ok("{\"ok\":true,\"applied\":" + std::to_string(applied) + "}");
    }
    if (method == "POST" && path == "/api/param") {
        std::string key, sval;
        if (!seeder::json_get_string(body.c_str(), "key", key)) return error("Hiányzó 'key'");
        const int id = seeder::param_find(key.c_str());
        if (id < 0) return error("Ismeretlen paraméter: " + key);
        bool done = false;
        std::string cfg;
        {
            std::lock_guard<std::mutex> g(shared.lock);
            if (seeder::PARAMS[id].kind == seeder::ParamKind::Str) {
                done = seeder::json_get_string(body.c_str(), "value", sval) &&
                       shared.core.set_param_str(static_cast<std::uint16_t>(id), sval.c_str());
            } else {
                double v = 0;
                done = seeder::json_get_number(body.c_str(), "value", v);
                if (done) shared.core.set_param(static_cast<std::uint16_t>(id), static_cast<float>(v));
            }
            cfg = seeder::settings_to_json(shared.core.settings());
        }
        if (!done) return error("Érvénytelen érték");
        return ok("{\"ok\":true,\"config\":" + cfg + "}");
    }
    if (method == "POST" && path == "/api/cmd") {
        std::string cmd, err;
        if (!seeder::json_get_string(body.c_str(), "cmd", cmd)) return error("Hiányzó 'cmd'");
        double v = 0;
        seeder::json_get_number(body.c_str(), "value", v);
        bool done = false;
        {
            std::lock_guard<std::mutex> g(shared.lock);
            done = seeder::apply_command(shared.core, cmd, v, err);
        }
        if (!done) return error(err);
        return ok("{\"ok\":true}");
    }
    return ApiResult{};
}

} // namespace app

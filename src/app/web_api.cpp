#include "web_api.hpp"
#include "ext_display.hpp"
#include "keya.hpp"

#include <cstdio>
#include <memory>
#include <new>

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
    char buf[720];
    std::snprintf(buf, sizeof(buf),
                  "{\"claimed\":%s,\"address\":%d,\"vt_found\":%s,\"vt_connected\":%s,\"vt_w\":%u,\"vt_h\":%u,"
                  "\"sk_w\":%u,\"sk_h\":%u,\"phys_keys\":%u,\"virt_keys\":%u,\"vt_version\":%d,\"tc_found\":%s,"
                  "\"tc_connected\":%s,\"ddop_uploads\":%u,\"vt_sent\":%u,\"vt_failed\":%u,\"page\":\"%s\","
                  "\"started\":%s,\"start_in\":%.0f,\"announces\":%u,\"reconnects\":%u}",
                  b(d.address_claimed), d.address, b(d.vt_found), b(d.vt_connected), d.vt_width, d.vt_height,
                  d.vt_softkey_w, d.vt_softkey_h, d.vt_phys_keys, d.vt_virt_keys, d.vt_version, b(d.tc_found),
                  b(d.tc_connected), d.ddop_uploads, d.vt_cmd_sent, d.vt_cmd_failed,
                  seeder::json_escape(d.page.c_str()).c_str(), b(d.started), static_cast<double>(d.start_in_s),
                  d.announces, d.reconnects);
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
        return ok("{\"status\":" + st + ",\"isobus\":" + isobus_diag_json(d) +
                  ",\"aux\":" + (isobus ? isobus->aux_json() : std::string("{}")) +
                  ",\"keya\":" + (isobus ? isobus->keya_json() : std::string("[]")) +
                  ",\"keya_setup\":" + (isobus ? isobus->keya().setup_json() : std::string("{}")) +
                  ",\"wifi\":" + (isobus ? isobus->wifi().json() : std::string("{}")) +
                  ",\"ext_display\":" + (isobus ? isobus->ext_display_json() : std::string("{}")) + ",\"hw\":" +
                  (hw_json.empty() ? std::string("{}") : hw_json) + ",\"fw\":\"" + seeder::json_escape(fw_version) +
                  "\",\"ssid\":\"" + seeder::json_escape(ssid) + "\"}");
    }
    if (method == "GET" && path == "/api/schema") return ok(seeder::schema_json());
    if (method == "GET" && path == "/api/crops") {
        // a leforgatási segéd növénylistája, fordulatszám-opciói és beállítási táblázata (skála → kg/ha)
        std::lock_guard<std::mutex> g(shared.lock);
        return ok(seeder::chart::to_json(seeder::chart::table(), shared.core.settings().crank_turns_10));
    }
    if (method == "GET" && path == "/api/profile.csv") {   // gépprofil letöltése (Excelben szerkeszthető)
        ApiResult r;
        r.status = 200;
        r.content_type = "text/csv; charset=utf-8";
        std::lock_guard<std::mutex> g(shared.lock);
        r.body = "\xEF\xBB\xBF" + seeder::chart::to_csv(seeder::chart::table());
        return r;
    }
    if (method == "POST" && path == "/api/profile") {
        // gépprofil feltöltése: CSV szöveg; üres törzs vagy "reset" = a beépített Accord táblázat
        const bool reset = body.empty() || body == "reset" || body == "{\"reset\":true}";
        std::unique_ptr<seeder::chart::Table> t(new (std::nothrow) seeder::chart::Table());   // ~10 kB: nem a vermen
        std::string err;
        if (!t) return error("Nincs elég memória");
        if (!reset && !seeder::chart::parse_csv(body, *t, err)) return error(err);
        if (shared.save_profile && !shared.save_profile(reset ? std::string() : body))
            return error("A gépprofil mentése nem sikerült (túl nagy?)");
        std::lock_guard<std::mutex> g(shared.lock);
        if (reset) seeder::chart::reset_table();
        else seeder::chart::set_table(*t);
        shared.core.profile_changed(true);
        const auto &tb = seeder::chart::table();
        char b[64];
        std::snprintf(b, sizeof b, ",\"crops\":%d,\"turns10\":%g}", tb.ncrop, static_cast<double>(tb.turns10));
        return ok("{\"ok\":true,\"machine\":\"" + seeder::json_escape(tb.machine) + "\"" + b);
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
    if (method == "GET" && path == "/api/vt") {
        if (!isobus) return error("A virtuális terminál nem fut");
        return ok(isobus->virtual_vt_json());
    }
    if (method == "POST" && path == "/api/vt/event") {
        if (!isobus) return error("A virtuális terminál nem fut");
        std::string kind;
        double id = 0, act = 0, value = 0;
        if (!seeder::json_get_string(body.c_str(), "kind", kind) || !seeder::json_get_number(body.c_str(), "id", id))
            return error("Hiányzó 'kind' / 'id'");
        seeder::json_get_number(body.c_str(), "act", act);
        seeder::json_get_number(body.c_str(), "value", value);
        if (!isobus->virtual_vt_event(kind, static_cast<std::uint16_t>(id), static_cast<std::uint8_t>(act),
                                      value < 0 ? 0u : static_cast<std::uint32_t>(value)))
            return error("Ismeretlen esemény: " + kind);
        return ok("{\"ok\":true}");
    }
    if (method == "POST" && path == "/api/cmd") {
        std::string cmd, err;
        if (!seeder::json_get_string(body.c_str(), "cmd", cmd)) return error("Hiányzó 'cmd'");
        double v = 0;
        seeder::json_get_number(body.c_str(), "value", v);
        if (cmd == "isobus_reconnect") {
            if (!isobus) return error("Az ISOBUS nem fut");
            isobus->request_reconnect();
            return ok("{\"ok\":true}");
        }
        if (cmd == "wifi") {   // {"cmd":"wifi","value":1} – WiFi szerviz mód (Etherneten át is)
            if (!isobus) return error("Nem érhető el");
            double v = 0;
            seeder::json_get_number(body.c_str(), "value", v);
            switch (isobus->wifi_request(v != 0, app::WifiService::Source::Web)) {
            case app::WifiService::Result::InhibitedWork: return error("Aktív munkakapcsolónál a WiFi nem kapcsolható be");
            case app::WifiService::Result::BusyOta: return error("Frissítés (OTA) fut – a WiFi most nem kapcsolható ki");
            default: return ok("{\"ok\":true}");
            }
        }
        if (cmd == "keya_setup") {   // {"cmd":"keya_setup","from_node":0,"from_kbps":500,"new_node":1}
            if (!isobus) return error("Az ISOBUS nem fut");
            double fn = 0, fk = 500, nn = 1;
            seeder::json_get_number(body.c_str(), "from_node", fn);
            seeder::json_get_number(body.c_str(), "from_kbps", fk);
            seeder::json_get_number(body.c_str(), "new_node", nn);
            if (!isobus->keya().start_setup(static_cast<int>(fn), static_cast<int>(fk), static_cast<int>(nn), err))
                return error(err);
            return ok("{\"ok\":true}");
        }
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

#pragma once
// A webes felület JSON API-ja, platformfüggetlenül (ESP32 httpd és PC-s tesztszerver is ezt hívja).
//   GET  /api/state   – élő állapot (vezérlő + ISOBUS + hardver)
//   GET  /api/schema  – paraméterleírás (csoportok, mértékegység, tartomány)
//   GET  /api/crops   – a leforgatási segéd növénylistája
//   GET  /api/config  – beállítások JSON
//   POST /api/config  – beállítások importja (teljes vagy részleges JSON)
//   POST /api/param   – {"key":"rate1","value":200}
//   POST /api/cmd     – {"cmd":"master","value":1}
#include "seeder_app.hpp"

#include <string>

namespace app {

struct ApiResult {
    int status = 404;
    std::string body;
    const char *content_type = "application/json; charset=utf-8";
};

/// @param hw_json  a platform hardverdiagnosztikája JSON objektumként (pl. {"can_state":"RUNNING"})
/// @returns status=404, ha az útvonalat nem ez a modul kezeli
ApiResult handle_api(Shared &shared, SeederApp *isobus, const std::string &method, const std::string &path,
                     const std::string &body, const std::string &hw_json, const char *fw_version, const char *ssid);

std::string isobus_diag_json(const IsobusDiag &d);

} // namespace app

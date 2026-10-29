#pragma once
// A gép bemenetei és kimenetei a kiválasztott panelen (board.hpp):
//  * impulzusbemenetek GPIO-megszakítással (kerék, turbina, enkóderek) vagy az I/O bővítő
//    5 ms-os mintavételével (ha egy bemenet I/O bővítőn van),
//  * érintkezők (munkahelyzet, tartályok), kimenetek (kuplungok, szelep, lámpák, hűtőventilátor),
//  * adagolómotor PWM (LEDC), ADS1115 (nyomás + motoráram), HX711 (mérleg), DS18B20 (hőmérők).
// A kimenetek csak az első apply() hívás után kapnak meghajtást (addig a lábak bemenetek / KI).
#include "seeder_app.hpp"

#include <string>

namespace io {

bool init();
/// Az előző hívás óta beérkezett impulzusok + a legfrissebb érintkező / analóg értékek.
app::HwInputs read(const seeder::Settings &s);
/// Fizikai kimeneti szintek (a mag már alkalmazta a kuplungtípust és a fordítást).
void apply(const seeder::Outputs &out, const seeder::Settings &s);
/// Diagnosztika a webes felületre (JSON objektum).
std::string diag_json();

} // namespace io

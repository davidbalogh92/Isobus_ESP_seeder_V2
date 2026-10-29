#pragma once
// Ethernet (W5500, csak a Waveshare panelen) + AgOpenGPS UDP gépmodul.
// A protokoll a hordozható seeder::aog::Link-ben van; itt csak a hálózat és a szál.
#include "seeder_app.hpp"
#include "seeder/aog.hpp"

#include <string>

namespace aognet {

/// Ethernet indítása (ha a panelen van) és az AgOpenGPS szál. A web is elérhető lesz a kábelen.
bool start(app::Shared *shared, const seeder::Settings &s);

/// A fő ciklus minden körben: aktuális beállítás + festési állapot be, AgOpenGPS adatok ki.
void exchange(const seeder::Settings &s, const seeder::aog::PaintState &paint, app::HwInputs &hw);

/// Diagnosztika (JSON objektum).
std::string diag_json();

} // namespace aognet

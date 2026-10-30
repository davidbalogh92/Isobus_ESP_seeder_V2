#pragma once
// Tartós tárolás NVS-ben: beállítások JSON-ként (kulcs alapú, firmware-frissítésnél is megmarad),
// számlálók/tartályszint bináris blobként (magic + verzió ellenőrzéssel).
#include "seeder/seeder.hpp"

#include <cstdint>
#include <vector>

namespace storage {

bool init();
bool load_settings(seeder::Settings &out);
bool save_settings(const seeder::Settings &s);
bool load_persistent(seeder::Persistent &out);
bool save_persistent(const seeder::Persistent &p);
bool factory_reset();
/// AUX-N (joystick) hozzárendelések (a gyári visszaállítás nem törli)
std::vector<std::uint8_t> load_aux();
bool save_aux(const std::vector<std::uint8_t> &blob);

} // namespace storage

#pragma once
// Beállítások kezelése a generált paramétertábla (params_gen.hpp) alapján.
// Hordozható kód: ESP-IDF és PC (teszt/szimuláció) alatt is fordul.
#include "seeder/params_gen.hpp"

#include <cstdint>
#include <string>

namespace seeder {

Settings default_settings();

/// Paraméter keresése kulcs alapján. -1, ha nincs ilyen.
int param_find(const char *key);

/// Számszerű érték (enum/bool/int is float-ként). Str típusnál 0.
float param_get(const Settings &s, std::uint16_t id);
const char *param_get_str(const Settings &s, std::uint16_t id);

/// Érték beírása tartományra vágással és lépésközre kerekítéssel (normalize() nélkül).
bool param_set(Settings &s, std::uint16_t id, float value);
bool param_set_str(Settings &s, std::uint16_t id, const char *value);

/// Egy paraméter módosítása + a függő értékek rendbetétele. Igazat ad, ha bármi változott.
bool apply_param(Settings &s, std::uint16_t id, float value);

/// Összefüggések kikényszerítése (szakasz/adagolószám, szélességek, küszöbök).
/// changed_id: az éppen módosított paraméter (-1 = teljes ellenőrzés).
void normalize(Settings &s, int changed_id = -1);

int meter_count(const Settings &s);
int section_count(const Settings &s);
int tank_count(const Settings &s);
bool electric_drive(const Settings &s);
/// Az i. adagoló által vetett szélesség (m). Bal/jobb elrendezésnél a félszélesség, első/hátsó
/// soros gépnél és 1 adagolónál a teljes munkaszélesség.
float meter_width(const Settings &s, int meter);
/// Az i. szakasz szélessége (m).
float section_width(const Settings &s, int section);
float rate_setting(const Settings &s, int meter);
float cal_kg_rev(const Settings &s, int meter);

std::string settings_to_json(const Settings &s);
/// Lapos JSON objektum beolvasása ({"kulcs":érték,...}). Ismeretlen kulcsot kihagy.
/// Visszaad: az alkalmazott kulcsok száma, hiba esetén -1.
int settings_from_json(Settings &s, const char *json, std::string *error = nullptr);
/// A web felület űrlapjaihoz: csoportok + paraméterek leírása.
std::string schema_json();

/// A TC eszközleíró (DDOP) szempontjából lényeges mezők ellenőrzőösszege.
std::uint32_t ddop_relevant_hash(const Settings &s);

std::string json_escape(const char *text);

} // namespace seeder

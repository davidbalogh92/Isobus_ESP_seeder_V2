#include "storage.hpp"

#include "nvs.h"
#include "nvs_flash.h"

#include <memory>
#include <string>

namespace storage {
namespace {
constexpr const char *NS = "seeder";
constexpr const char *KEY_SETTINGS = "settings";
constexpr const char *KEY_PERSIST = "persist";
} // namespace

bool init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        if (nvs_flash_erase() != ESP_OK) return false;
        err = nvs_flash_init();
    }
    return err == ESP_OK;
}

bool load_settings(seeder::Settings &out) {
    out = seeder::default_settings();
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = 0;
    bool ok = false;
    if (nvs_get_str(h, KEY_SETTINGS, nullptr, &len) == ESP_OK && len > 2 && len < 8192) {
        std::unique_ptr<char[]> buf(new char[len]);
        if (nvs_get_str(h, KEY_SETTINGS, buf.get(), &len) == ESP_OK)
            ok = seeder::settings_from_json(out, buf.get()) >= 0;
    }
    nvs_close(h);
    return ok;
}

bool save_settings(const seeder::Settings &s) {
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;
    const std::string js = seeder::settings_to_json(s);
    bool ok = nvs_set_str(h, KEY_SETTINGS, js.c_str()) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool load_persistent(seeder::Persistent &out) {
    out = seeder::Persistent{};
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    seeder::Persistent p;
    size_t len = sizeof(p);
    bool ok = nvs_get_blob(h, KEY_PERSIST, &p, &len) == ESP_OK && len == sizeof(p) &&
              p.magic == seeder::Persistent::MAGIC && p.version == seeder::Persistent::VERSION;
    nvs_close(h);
    if (ok) out = p;
    return ok;
}

bool save_persistent(const seeder::Persistent &p) {
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = nvs_set_blob(h, KEY_PERSIST, &p, sizeof(p)) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool factory_reset() {
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = nvs_erase_key(h, KEY_SETTINGS) != ESP_FAIL;
    ok = nvs_commit(h) == ESP_OK && ok;
    nvs_close(h);
    return ok;
}

std::vector<std::uint8_t> load_aux() {
    std::vector<std::uint8_t> out;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return out;
    size_t len = 0;
    if (nvs_get_blob(h, "aux", nullptr, &len) == ESP_OK && len > 0 && len <= 2048) {
        out.resize(len);
        if (nvs_get_blob(h, "aux", out.data(), &len) != ESP_OK) out.clear();
    }
    nvs_close(h);
    return out;
}

bool save_aux(const std::vector<std::uint8_t> &blob) {
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return false;
    bool ok = nvs_set_blob(h, "aux", blob.data(), blob.size()) == ESP_OK && nvs_commit(h) == ESP_OK;
    nvs_close(h);
    return ok;
}

} // namespace storage

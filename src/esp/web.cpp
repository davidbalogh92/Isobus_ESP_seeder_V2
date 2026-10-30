#include "web.hpp"

#include "web_api.hpp"
#include "board.hpp"

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>

#ifndef SEEDER_FW_VERSION
#define SEEDER_FW_VERSION "0.1.0-dev"
#endif

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char pool_bin_start[] asm("_binary_object_pool_iop_start");
extern const char pool_bin_end[] asm("_binary_object_pool_iop_end");
extern const char index_html_end[] asm("_binary_index_html_end");

namespace web {
namespace {
constexpr const char *TAG = "SEEDER_WEB";
constexpr std::size_t LOG_CAPACITY = 120;
constexpr std::size_t LOG_SIZE = 160;
struct LogLine {
    std::uint32_t ms;
    char severity[8];
    char text[LOG_SIZE];
};
SemaphoreHandle_t log_lock = nullptr;
std::array<LogLine, LOG_CAPACITY> lines{};
std::size_t count = 0, head = 0;
SemaphoreHandle_t hw_lock = nullptr;
HwDiag hw{};
app::Shared *shared = nullptr;
std::atomic<app::SeederApp *> isobus{nullptr};
httpd_handle_t server = nullptr;
char ssid[33] = "VETOGEP";
std::atomic<bool> factory_flag{false};

std::string esc(const char *s) { return seeder::json_escape(s); }

esp_err_t send_json(httpd_req_t *req, const std::string &body, const char *status = nullptr) {
    if (status) httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, body.c_str(), body.size());
}

esp_err_t send_error(httpd_req_t *req, const std::string &msg) {
    return send_json(req, "{\"ok\":false,\"error\":\"" + esc(msg.c_str()) + "\"}", "400 Bad Request");
}

bool read_body(httpd_req_t *req, std::string &out, std::size_t max = 8192) {
    if (req->content_len <= 0 || static_cast<std::size_t>(req->content_len) > max) return false;
    out.resize(req->content_len);
    std::size_t got = 0;
    int timeouts = 0;
    while (got < out.size()) {
        const int r = httpd_req_recv(req, &out[got], out.size() - got);
        if (r == HTTPD_SOCK_ERR_TIMEOUT && timeouts++ < 3) continue;
        if (r <= 0) return false;
        got += static_cast<std::size_t>(r);
    }
    return true;
}

// ------------------------------------------------------------------ oldalak és API
esp_err_t page_index(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    const std::size_t len = static_cast<std::size_t>(index_html_end - index_html_start);
    return httpd_resp_send(req, index_html_start, len > 0 ? len - 1 : 0); // a TEXT beágyazás \0-t fűz a végére
}

/// A VT object pool (a Windows-program ugyanezt rajzolja ki, mint a terminál)
esp_err_t api_pool(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/octet-stream");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, pool_bin_start, static_cast<ssize_t>(pool_bin_end - pool_bin_start));
}

std::string hw_json() {
    HwDiag h;
    if (xSemaphoreTake(hw_lock, pdMS_TO_TICKS(50)) == pdTRUE) {
        h = hw;
        xSemaphoreGive(hw_lock);
    }
    char buf[420];
    std::snprintf(buf, sizeof(buf),
                  "{\"can_started\":%s,\"can_state\":\"%s\",\"tec\":%u,\"rec\":%u,\"bus_errors\":%lu,"
                  "\"tx_failed\":%lu,\"rx_missed\":%lu,\"pool\":%lu,"
                  "\"heap\":%lu,\"heap_min\":%lu,\"saves\":%lu,\"saves_p\":%lu,\"board\":\"%s\",\"io\":",
                  h.can_started ? "true" : "false", h.can_state, h.tec, h.rec,
                  static_cast<unsigned long>(h.bus_errors), static_cast<unsigned long>(h.tx_failed),
                  static_cast<unsigned long>(h.rx_missed), static_cast<unsigned long>(h.pool_size),
                  static_cast<unsigned long>(h.free_heap), static_cast<unsigned long>(h.min_free_heap),
                  static_cast<unsigned long>(h.saves_settings), static_cast<unsigned long>(h.saves_persistent),
                  board::NAME);
    return std::string(buf) + (h.io_json.empty() ? std::string("{}") : h.io_json) +
           ",\"aog\":" + (h.aog_json.empty() ? std::string("{}") : h.aog_json) + "}";
}

/// Közös API-kezelő: a logika a platformfüggetlen app::handle_api()-ban van.
esp_err_t api_generic(httpd_req_t *req) {
    std::string body;
    if (req->method == HTTP_POST && !read_body(req, body)) return send_error(req, "Hiányzó vagy túl nagy kérés");
    std::string path = req->uri;
    const std::size_t q = path.find('?');
    const bool download = q != std::string::npos && path.find("download", q) != std::string::npos;
    if (q != std::string::npos) path.resize(q);
    const app::ApiResult r = app::handle_api(*shared, isobus.load(), req->method == HTTP_POST ? "POST" : "GET",
                                             path, body, hw_json(), SEEDER_FW_VERSION, ssid);
    if (r.status == 404) return httpd_resp_send_404(req);
    if (download) httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"vetogep-beallitasok.json\"");
    if (r.status == 400 && path == "/api/config") log("WARN", "Beállítás import elutasítva");
    return send_json(req, r.body, r.status == 200 ? nullptr : "400 Bad Request");
}

void copy_logs(std::unique_ptr<LogLine[]> &copy, std::size_t &n) {
    n = 0;
    if (xSemaphoreTake(log_lock, pdMS_TO_TICKS(100)) != pdTRUE) return;
    n = count;
    for (std::size_t i = 0; i < n; ++i) copy[i] = lines[(head + LOG_CAPACITY - count + i) % LOG_CAPACITY];
    xSemaphoreGive(log_lock);
}

esp_err_t api_logs(httpd_req_t *req) {
    std::unique_ptr<LogLine[]> copy(new (std::nothrow) LogLine[LOG_CAPACITY]);
    if (!copy) return httpd_resp_send_500(req);
    std::size_t n = 0;
    copy_logs(copy, n);
    const bool txt = std::strstr(req->uri, ".txt") != nullptr;
    httpd_resp_set_type(req, txt ? "text/plain; charset=utf-8" : "application/json; charset=utf-8");
    if (txt) httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=\"vetogep-naplo.txt\"");
    if (!txt && httpd_resp_send_chunk(req, "[", 1) != ESP_OK) return ESP_FAIL;
    for (std::size_t i = 0; i < n; ++i) {
        std::string item;
        if (txt) {
            char b[220];
            std::snprintf(b, sizeof(b), "%10lu %-6s %s\n", static_cast<unsigned long>(copy[i].ms), copy[i].severity,
                          copy[i].text);
            item = b;
        } else {
            item = std::string(i ? "," : "") + "{\"ms\":" + std::to_string(copy[i].ms) + ",\"level\":\"" +
                   esc(copy[i].severity) + "\",\"message\":\"" + esc(copy[i].text) + "\"}";
        }
        if (httpd_resp_send_chunk(req, item.c_str(), item.size()) != ESP_OK) return ESP_FAIL;
    }
    if (!txt && httpd_resp_send_chunk(req, "]", 1) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(req, nullptr, 0);
}

void delayed_restart(void *) {
    vTaskDelay(pdMS_TO_TICKS(1500));
    esp_restart();
}

esp_err_t api_restart(httpd_req_t *req) {
    log("WARN", "Újraindítás a webes felületről");
    send_json(req, "{\"ok\":true}");
    xTaskCreate(delayed_restart, "restart", 2048, nullptr, 4, nullptr);
    return ESP_OK;
}

esp_err_t api_factory(httpd_req_t *req) {
    factory_flag = true;
    log("WARN", "Gyári beállítások visszaállítása kérve");
    return send_json(req, "{\"ok\":true}");
}

esp_err_t api_ota(httpd_req_t *req) {
    {
        std::lock_guard<std::mutex> g(shared->lock);
        if (shared->core.status().master) return send_error(req, "Frissítés előtt kapcsold KI a vetést!");
    }
    const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
    if (!target || req->content_len < 4096 || static_cast<std::size_t>(req->content_len) > target->size)
        return send_error(req, "Érvénytelen OTA fájlméret");
    log("WARN", "OTA feltöltés indul (%u bájt)", static_cast<unsigned>(req->content_len));
    esp_ota_handle_t handle = 0;
    esp_err_t err = esp_ota_begin(target, OTA_SIZE_UNKNOWN, &handle);
    if (err != ESP_OK) return send_error(req, "OTA indítás sikertelen");
    std::array<char, 2048> buffer{};
    std::array<unsigned char, 36> prefix{};
    std::size_t prefix_count = 0;
    int left = req->content_len;
    bool validated = false;
    int timeouts = 0;
    while (left > 0) {
        const int amount = httpd_req_recv(req, buffer.data(), std::min<int>(buffer.size(), left));
        if (amount == HTTPD_SOCK_ERR_TIMEOUT && timeouts++ < 5) continue;
        if (amount <= 0) { err = ESP_FAIL; break; }
        timeouts = 0;
        if (!validated) {
            // alkalmazás-kép: app descriptor magic a 32. bájton (a FULL_0x0.bin itt elutasítva)
            const std::size_t copied = std::min<std::size_t>(prefix.size() - prefix_count, amount);
            std::memcpy(prefix.data() + prefix_count, buffer.data(), copied);
            prefix_count += copied;
            if (prefix_count == prefix.size()) {
                static const unsigned char MAGIC[] = {0x32, 0x54, 0xCD, 0xAB};
                if (prefix[0] != 0xE9 || std::memcmp(prefix.data() + 32, MAGIC, 4) != 0) { err = ESP_ERR_INVALID_ARG; break; }
                validated = true;
                err = esp_ota_write(handle, prefix.data(), prefix.size());
                if (err == ESP_OK && static_cast<std::size_t>(amount) > copied)
                    err = esp_ota_write(handle, buffer.data() + copied, amount - copied);
                if (err != ESP_OK) break;
            }
        } else {
            err = esp_ota_write(handle, buffer.data(), amount);
            if (err != ESP_OK) break;
        }
        left -= amount;
    }
    if (!validated) err = ESP_ERR_INVALID_ARG;
    if (err != ESP_OK || left != 0) {
        esp_ota_abort(handle);
        log("ERROR", "OTA hiba 0x%x, hiányzó %d bájt", err, left);
        return send_error(req, "Hibás vagy nem OTA alkalmazás BIN (a FULL_0x0.bin ide nem tölthető fel)");
    }
    if (esp_ota_end(handle) != ESP_OK) return send_error(req, "A firmware kép ellenőrzése sikertelen");
    if (esp_ota_set_boot_partition(target) != ESP_OK) return send_error(req, "Boot partíció váltás sikertelen");
    log("INFO", "OTA kész, újraindítás");
    send_json(req, "{\"ok\":true,\"message\":\"OTA sikeres, újraindulás...\"}");
    xTaskCreate(delayed_restart, "ota_restart", 2048, nullptr, 4, nullptr);
    return ESP_OK;
}
} // namespace

void init_log() {
    if (!log_lock) log_lock = xSemaphoreCreateMutex();
    if (!hw_lock) hw_lock = xSemaphoreCreateMutex();
}

void log(const char *severity, const char *format, ...) {
    char text[LOG_SIZE] = {0};
    va_list args;
    va_start(args, format);
    std::vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    ESP_LOGI("SEEDER", "[%s] %s", severity, text);
    if (!log_lock || xSemaphoreTake(log_lock, pdMS_TO_TICKS(50)) != pdTRUE) return;
    LogLine &l = lines[head];
    l.ms = static_cast<std::uint32_t>(esp_timer_get_time() / 1000);
    std::snprintf(l.severity, sizeof(l.severity), "%s", severity);
    std::snprintf(l.text, sizeof(l.text), "%s", text);
    head = (head + 1) % LOG_CAPACITY;
    if (count < LOG_CAPACITY) ++count;
    xSemaphoreGive(log_lock);
}

void update_hw(const HwDiag &d) {
    if (hw_lock && xSemaphoreTake(hw_lock, pdMS_TO_TICKS(20)) == pdTRUE) {
        hw = d;
        xSemaphoreGive(hw_lock);
    }
}

bool factory_reset_requested() { return factory_flag.exchange(false); }

void attach_isobus(app::SeederApp *ia) { isobus = ia; }

bool start(app::Shared *sh, app::SeederApp *ia, const char *wifi_password) {
    shared = sh;
    isobus = ia;
    std::uint8_t mac[6]{};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    std::snprintf(ssid, sizeof(ssid), "VETOGEP-%02X%02X", mac[4], mac[5]);
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return false;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return false;
    if (!esp_netif_create_default_wifi_ap()) return false;
    wifi_init_config_t wi = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&wi) != ESP_OK) return false;
    wifi_config_t conf{};
    const std::size_t ssid_len = std::strlen(ssid);
    std::memcpy(conf.ap.ssid, ssid, ssid_len);
    conf.ap.ssid_len = ssid_len;
    std::snprintf(reinterpret_cast<char *>(conf.ap.password), sizeof(conf.ap.password), "%s", wifi_password);
    conf.ap.channel = 6;
    conf.ap.max_connection = 3;
    conf.ap.authmode = WIFI_AUTH_WPA2_PSK;
    if (esp_wifi_set_mode(WIFI_MODE_AP) != ESP_OK || esp_wifi_set_config(WIFI_IF_AP, &conf) != ESP_OK ||
        esp_wifi_start() != ESP_OK)
        return false;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.stack_size = 10240;
    cfg.max_uri_handlers = 24;
    cfg.recv_wait_timeout = 20;
    cfg.send_wait_timeout = 15;
    cfg.lru_purge_enable = true;
    if (httpd_start(&server, &cfg) != ESP_OK) return false;
    struct Route {
        const char *uri;
        httpd_method_t method;
        esp_err_t (*fn)(httpd_req_t *);
    };
    static const Route routes[] = {
        {"/", HTTP_GET, page_index},
        {"/api/state", HTTP_GET, api_generic},
        {"/api/schema", HTTP_GET, api_generic},
        {"/api/crops", HTTP_GET, api_generic},
        {"/api/config", HTTP_GET, api_generic},
        {"/api/config", HTTP_POST, api_generic},
        {"/api/param", HTTP_POST, api_generic},
        {"/api/cmd", HTTP_POST, api_generic},
        {"/api/vt", HTTP_GET, api_generic},
        {"/api/vt/event", HTTP_POST, api_generic},
        {"/api/pool", HTTP_GET, api_pool},
        {"/api/logs", HTTP_GET, api_logs},
        {"/api/log.txt", HTTP_GET, api_logs},
        {"/api/ota", HTTP_POST, api_ota},
        {"/api/restart", HTTP_POST, api_restart},
        {"/api/factory", HTTP_POST, api_factory},
    };
    for (const auto &r : routes) {
        httpd_uri_t u{};
        u.uri = r.uri;
        u.method = r.method;
        u.handler = r.fn;
        if (httpd_register_uri_handler(server, &u) != ESP_OK) return false;
    }
    ESP_LOGW(TAG, "Wi-Fi AP: %s  jelszó: %s  cím: http://192.168.4.1", ssid, wifi_password);
    log("INFO", "Wi-Fi AP és webszerver kész: %s", ssid);
    return true;
}

void confirm_running_firmware() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state = ESP_OTA_IMG_UNDEFINED;
    if (running && esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
        const esp_err_t e = esp_ota_mark_app_valid_cancel_rollback();
        log(e == ESP_OK ? "INFO" : "ERROR", "Új firmware jóváhagyva: %s", esp_err_to_name(e));
    }
}

} // namespace web

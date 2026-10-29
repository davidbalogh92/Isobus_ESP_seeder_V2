// Vetőgép ISOBUS vezérlő – ESP32-S3 (AgIsoAuxInputs vagy Waveshare ESP32-S3-ETH-8DI-8RO-C)
//
// Accord-típusú (térfogatos adagolós) vetőgép: 1–2 adagoló talajhajtással + elektromágneses
// kuplunggal vagy elektromos motorral, turbinafigyelés, tartályszint, munkakapcsoló, TC-SEC/GEO,
// kiegészítők (lámpák, terelőszelep, lenyomatási nyomás, olajhűtés, mérleg).
#include "aog_net.hpp"
#include "board.hpp"
#include "io.hpp"
#include "storage.hpp"
#include "web.hpp"

#include "seeder_app.hpp"
#include "seeder/vt_ids.hpp"

#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/hardware_integration/twai_plugin.hpp"
#include "isobus/isobus/can_stack_logger.hpp"

#include "driver/twai.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <cstdint>
#include <memory>

extern "C" const std::uint8_t pool_start[] asm("_binary_object_pool_iop_start");
extern "C" const std::uint8_t pool_end[] asm("_binary_object_pool_iop_end");

namespace {
class WebLogger final : public isobus::CANStackLogger {
public:
    void sink_CAN_stack_log(LoggingLevel level, const std::string &text) override {
        const char *lv = level >= LoggingLevel::Error ? "ERROR" : (level == LoggingLevel::Warning ? "WARN" : "INFO");
        web::log(lv, "ISOBUS: %.140s", text.c_str());
    }
};
WebLogger stack_logger;

app::Shared shared;
std::unique_ptr<app::SeederApp> isobus_app;

const char *twai_state_name(twai_state_t s) {
    switch (s) {
    case TWAI_STATE_STOPPED: return "STOPPED";
    case TWAI_STATE_RUNNING: return "RUNNING";
    case TWAI_STATE_BUS_OFF: return "BUS_OFF";
    case TWAI_STATE_RECOVERING: return "RECOVERING";
    default: return "UNKNOWN";
    }
}
constexpr float CYCLE_S = 0.02f;   // 20 ms vezérlési ciklus (motorszabályzás, kuplungok)
} // namespace

extern "C" void app_main() {
    web::init_log();
    web::log("INFO", "Vetőgép-vezérlő indul – %s", board::NAME);

    // --- beállítások és tartós adatok
    if (!storage::init()) web::log("ERROR", "NVS inicializálás sikertelen");
    seeder::Settings settings;
    seeder::Persistent persistent;
    const bool had_settings = storage::load_settings(settings);
    const bool had_persistent = storage::load_persistent(persistent);
    web::log("INFO", "Beállítások: %s, számlálók: %s", had_settings ? "betöltve" : "alapértékek",
             had_persistent ? "betöltve" : "új");
    shared.core.load(settings, persistent);

    // --- gép I/O (a kimenetek az első vezérlési ciklusig nincsenek meghajtva)
    if (!io::init()) web::log("ERROR", "I/O inicializálási hiba");

    // --- Wi-Fi + web (a hálózati hiba nem állítja meg a vezérlést)
    const bool wifi_ok = web::start(&shared, nullptr, settings.wifi_pass);
    if (!wifi_ok) web::log("ERROR", "Wi-Fi / web indítás sikertelen");
    // --- Ethernet + AgOpenGPS (csak Ethernetes panelen)
    if (board::HAS_ETH && !aognet::start(&shared, settings)) web::log("WARN", "Ethernet nem indult");

    // --- CAN (250 kbit/s ISOBUS)
    twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT(
        static_cast<gpio_num_t>(board::CAN_TX_GPIO), static_cast<gpio_num_t>(board::CAN_RX_GPIO), TWAI_MODE_NORMAL);
    general.rx_queue_len = 64;
    general.tx_queue_len = 32;
    twai_timing_config_t timing = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t filter = TWAI_FILTER_CONFIG_ACCEPT_ALL();
    auto driver = std::make_shared<isobus::TWAIPlugin>(&general, &timing, &filter);
    isobus::CANStackLogger::set_can_stack_logger_sink(&stack_logger);
    isobus::CANStackLogger::set_log_level(isobus::CANStackLogger::LoggingLevel::Warning);
    isobus::CANHardwareInterface::set_number_of_can_channels(1);
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, driver);
    web::HwDiag hwd;
    hwd.can_started = isobus::CANHardwareInterface::start() && driver->get_is_valid();
    hwd.pool_size = static_cast<std::uint32_t>(pool_end - pool_start);
    web::log(hwd.can_started ? "INFO" : "ERROR", "CAN (TX GPIO%d, RX GPIO%d): %s", board::CAN_TX_GPIO,
             board::CAN_RX_GPIO, hwd.can_started ? "fut" : "NEM indult");
    web::log("INFO", "Object pool: %lu bájt, verzió %s", static_cast<unsigned long>(hwd.pool_size), vt::POOL_VERSION);

    isobus_app = std::make_unique<app::SeederApp>(shared);
    if (hwd.can_started) {
        if (!isobus_app->start(0, pool_start, hwd.pool_size)) web::log("ERROR", "ISOBUS alkalmazás indítás hiba");
    }
    web::attach_isobus(isobus_app.get());
    if (wifi_ok) web::confirm_running_firmware();

    // --- vezérlési ciklus
    TickType_t wake = xTaskGetTickCount();
    float settings_save_timer = 0.0f;
    unsigned tick = 0;
    seeder::Settings cur = settings;
    seeder::aog::PaintState paint;
    for (;;) {
        vTaskDelayUntil(&wake, pdMS_TO_TICKS(static_cast<int>(CYCLE_S * 1000.0f)));
        app::HwInputs hw = io::read(cur);
        if (board::HAS_ETH) aognet::exchange(cur, paint, hw);
        isobus_app->update(CYCLE_S, hw);

        // kimenetek + mentések (beállítás: 2 s késleltetéssel, hogy a gyors egymás utáni módosítás egyszer íródjon)
        bool save_settings = false, save_persist = false;
        seeder::Outputs out;
        seeder::Persistent p_copy;
        {
            std::lock_guard<std::mutex> g(shared.lock);
            out = shared.core.outputs();
            cur = shared.core.settings();
            // AgOpenGPS festés: főkapcsoló = gép lent és vet; automata (zöld) vagy kézi (sárga) szakaszok
            const seeder::Status &st = shared.core.status();
            paint.enabled = cur.aog_enable && cur.aog_paint;
            paint.work = st.master && st.work_down;
            paint.mode = (cur.sec_src == 2 && st.sec_auto_req) ? 0 : cur.aog_paint_mode;
            paint.sections = st.section_count;
            paint.on_mask = static_cast<std::uint16_t>((st.section_on[0] ? 1u : 0u) | (st.section_on[1] ? 2u : 0u));
            if (shared.core.settings_dirty) {
                settings_save_timer += CYCLE_S;
                if (settings_save_timer >= 2.0f) {
                    shared.core.settings_dirty = false;
                    settings_save_timer = 0.0f;
                    save_settings = true;
                }
            }
            if (shared.core.persistent_dirty) {
                shared.core.persistent_dirty = false;
                save_persist = true;
                p_copy = shared.core.persistent();
            }
        }
        io::apply(out, cur);
        if (save_settings) {
            if (storage::save_settings(cur)) ++hwd.saves_settings;
            else web::log("ERROR", "Beállítások mentése sikertelen");
        }
        if (save_persist) {
            if (storage::save_persistent(p_copy)) ++hwd.saves_persistent;
            else web::log("ERROR", "Számlálók mentése sikertelen");
        }
        if (web::factory_reset_requested()) {
            storage::factory_reset();
            web::log("WARN", "Gyári beállítások visszaállítva, újraindítás");
            vTaskDelay(pdMS_TO_TICKS(500));
            esp_restart();
        }

        // diagnosztika másodpercenként
        if (++tick % 50 == 0) {
            twai_status_info_t st{};
            if (twai_get_status_info(&st) == ESP_OK) {
                hwd.can_state = twai_state_name(st.state);
                hwd.tec = st.tx_error_counter;
                hwd.rec = st.rx_error_counter;
                hwd.bus_errors = st.bus_error_count;
                hwd.tx_failed = st.tx_failed_count;
                hwd.rx_missed = st.rx_missed_count;
                if (st.state == TWAI_STATE_BUS_OFF) {
                    web::log("ERROR", "CAN BUS_OFF – helyreállítás");
                    twai_initiate_recovery();
                } else if (st.state == TWAI_STATE_STOPPED && hwd.can_started) {
                    twai_start();   // a helyreállítás után újraindítjuk
                }
            }
            hwd.free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
            hwd.min_free_heap = heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT);
            hwd.io_json = io::diag_json();
            if (board::HAS_ETH) hwd.aog_json = aognet::diag_json();
            web::update_hw(hwd);
        }
    }
}

#pragma once
// Wi-Fi hozzáférési pont (AP) + webes kezelőfelület + OTA + eseménynapló.
#include "seeder_app.hpp"

#include <cstdint>
#include <string>

namespace web {

struct HwDiag {
    bool can_started = false;
    const char *can_state = "NOT_STARTED";
    unsigned tec = 0, rec = 0;
    std::uint32_t bus_errors = 0, tx_failed = 0, rx_missed = 0;
    std::uint32_t pool_size = 0;
    std::uint32_t free_heap = 0, min_free_heap = 0;
    std::uint32_t saves_settings = 0, saves_persistent = 0;
    std::string io_json;   ///< a bemenetek/kimenetek/perifériák állapota (io.cpp)
    std::string aog_json;  ///< Ethernet + AgOpenGPS (aog_net.cpp)
};

void init_log();
void log(const char *severity, const char *format, ...);
bool start(app::Shared *shared, app::SeederApp *isobus_app, const char *wifi_password);
void attach_isobus(app::SeederApp *isobus_app);
void update_hw(const HwDiag &d);
void confirm_running_firmware();
/// a web felületről kért gyári visszaállítás / újraindítás jelzése a fő ciklusnak
bool factory_reset_requested();

} // namespace web

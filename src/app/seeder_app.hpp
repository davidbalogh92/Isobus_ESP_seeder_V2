#pragma once
// ISOBUS alkalmazásréteg: összeköti a vezérlőmagot (seeder::Seeder) a VT-vel (kezelőfelület),
// a Task Controllerrel (TC-BAS/SEC/GEO) és az ISOBUS sebességüzenetekkel.
// Hordozható: ESP32-n (TWAI) és PC-n (virtuális CAN, szimuláció) is ugyanez a kód fut.
#include "seeder/seeder.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace isobus {
class InternalControlFunction;
class PartneredControlFunction;
class VirtualTerminalClient;
class SpeedMessagesInterface;
class NMEA2000MessageInterface;
} // namespace isobus

namespace app {

/// A vezérlőmag és zárja: minden szál (vezérlés, VT események, web) ezen keresztül éri el.
struct Shared {
    std::mutex lock;
    seeder::Seeder core;
};

/// Pillanatkép a kijelzéshez (zár nélkül dolgozható fel).
struct Snapshot {
    seeder::Status st;
    seeder::Settings set;
    seeder::Persistent per;
    seeder::Outputs out;
};

struct IsobusDiag {
    bool address_claimed = false;
    int address = -1;
    bool vt_found = false;
    bool vt_connected = false;
    unsigned vt_width = 0, vt_height = 0;
    unsigned vt_softkey_w = 0, vt_softkey_h = 0;
    unsigned vt_phys_keys = 0, vt_virt_keys = 0;
    int vt_version = 0;
    bool tc_found = false;
    bool tc_connected = false;
    unsigned ddop_uploads = 0;
    unsigned vt_cmd_sent = 0, vt_cmd_failed = 0;
    std::string page = "-";
};

/// Egy ciklus hardveres bemenetei (az ESP réteg vagy a szimulátor tölti ki).
struct HwInputs {
    seeder::PulseSample wheel, fan, meter[seeder::MAX_METERS];
    bool work_contact = false;
    bool hopper_contact[2] = {false, false};
    float pressure_v = -1.0f;
    float temp_c[2] = {-300.0f, -300.0f};
    bool scale_valid = false;
    float scale_raw = 0.0f;
};

class VtUi;
class TcLink;

class SeederApp {
public:
    explicit SeederApp(Shared &shared);
    ~SeederApp();

    /// Indítás a CAN hardver elindítása után.
    /// @param pool  a beágyazott object pool (élettartama végig érvényes kell legyen)
    bool start(std::uint8_t can_channel, const std::uint8_t *pool, std::uint32_t pool_size);

    /// Vezérlési ciklus (pl. 20–50 ms). Zárolja a magot.
    void update(float dt_s, const HwInputs &hw);

    IsobusDiag diag() const;

private:
    seeder::IsobusSpeeds read_speeds();

    Shared &shared_;
    std::shared_ptr<isobus::InternalControlFunction> ecu_;
    std::shared_ptr<isobus::PartneredControlFunction> vt_partner_;
    std::shared_ptr<isobus::PartneredControlFunction> tc_partner_;
    std::shared_ptr<isobus::VirtualTerminalClient> vt_;
    std::unique_ptr<VtUi> ui_;
    std::unique_ptr<TcLink> tc_;
    std::unique_ptr<isobus::SpeedMessagesInterface> speed_;
    std::unique_ptr<isobus::NMEA2000MessageInterface> nmea_;
    float ui_timer_ = 0.0f;
    bool started_ = false;
};

} // namespace app

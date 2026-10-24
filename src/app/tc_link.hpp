#pragma once
// Task Controller kapcsolat: eszközleíró (DDOP) a beállításokból, TC-BAS dokumentáció,
// TC-SEC automatikus szakaszvezérlés, TC-GEO előírt érték adagolónként (kg/ha).
#include "seeder_app.hpp"

#include <cstdint>
#include <memory>

namespace isobus {
class TaskControllerClient;
class DeviceDescriptorObjectPool;
} // namespace isobus

namespace app {

/// A gép geometriája a TC-nek (ISO 11783-10: X előre +, Y jobbra +, mm, a DRP-hez képest).
/// Függesztett: DRP = vonópont, a csoroszlyák B-vel mögötte. Vontatott: DRP = a kerékagy, a
/// vonópont C-vel előtte, a csoroszlyák B-vel a csuklópont mögött. Kormányzott vonórúd:
/// vonópont A + C, csuklópont C előtt.
struct TcGeometry {
    int sections = 1;
    std::uint8_t connector_type = 3;
    std::int32_t connector_x_mm = 0;
    bool has_pivot = false;
    std::int32_t pivot_x_mm = 0;
    std::int32_t boom_x_mm = 0;
    std::int32_t boom_y_mm = 0;
    std::int32_t width_mm = 0;
    std::int32_t sec_w_mm[seeder::MAX_SECTIONS] = {0, 0};
    std::int32_t sec_y_mm[seeder::MAX_SECTIONS] = {0, 0};   ///< a vetősor közepéhez képest
};
TcGeometry make_tc_geometry(const seeder::Settings &s);

/// DDI 157 csatlakozótípus a beállítás szerinti csatlakozásból.
std::uint8_t connector_type_for(int hitch);

/// Kondenzált munkaállapot (2 bit/szakasz; nem létező szakasz = 3).
std::uint32_t condensed_state(const bool *on, int count);

/// DDOP felépítése a beállításokból. Külön függvény, hogy PC-n tesztelhető legyen.
bool build_ddop(isobus::DeviceDescriptorObjectPool &pool, const seeder::Settings &s, std::uint64_t client_name);

class TcLink {
public:
    TcLink(Shared &shared, std::shared_ptr<isobus::PartneredControlFunction> tc,
           std::shared_ptr<isobus::InternalControlFunction> ecu);
    ~TcLink();

    /// Ciklikus hívás a vezérlő szálból (zár nélkül; a pillanatképből dolgozik).
    void update(const Snapshot &snap);
    bool connected() const;
    unsigned uploads() const { return uploads_; }

private:
    static bool on_request_value(std::uint16_t element, std::uint16_t ddi, std::int32_t &value, void *self);
    static bool on_value_command(std::uint16_t element, std::uint16_t ddi, std::int32_t value, void *self);

    Shared &shared_;
    std::shared_ptr<isobus::InternalControlFunction> ecu_;
    std::shared_ptr<isobus::TaskControllerClient> client_;
    std::shared_ptr<isobus::DeviceDescriptorObjectPool> ddop_;
    std::uint32_t ddop_hash_ = 0;
    bool pending_reupload_ = false;
    bool started_ = false;
    bool was_connected_ = false;
    unsigned uploads_ = 0;
    std::uint32_t last_sections_ = 0xFFFFFFFFu;
    bool last_work_ = false;
    std::int32_t last_rate_[seeder::MAX_METERS] = {-1, -1};
};

} // namespace app

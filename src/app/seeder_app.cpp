#include "seeder_app.hpp"

#include <cstring>
#include <vector>

#include "aux_n.hpp"
#include "pool_scaler.hpp"
#include "tc_link.hpp"
#include "vt_sink.hpp"
#include "vt_ui.hpp"

#include "seeder/vt_ids.hpp"

#include "isobus/isobus/can_NAME.hpp"
#include "isobus/isobus/can_NAME_filter.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_partnered_control_function.hpp"
#include "isobus/isobus/isobus_speed_distance_messages.hpp"
#include "isobus/isobus/isobus_virtual_terminal_client.hpp"
#include "isobus/isobus/nmea2000_message_interface.hpp"
#include "isobus/utility/system_timing.hpp"

namespace app {

#ifndef SEEDER_IDENTITY_NUMBER
#define SEEDER_IDENTITY_NUMBER 0x15EED
#endif
// Gyártókód 0: kizárólag fejlesztési/kísérleti célra (a vetőgép-monitorral a Müller terminál elfogadta).
#ifndef SEEDER_MANUFACTURER_CODE
#define SEEDER_MANUFACTURER_CODE 0
#endif

namespace {
// A pool feltöltése darabokban: az első darab kérésekor (ekkor a VT méretei már ismertek) a poolt
// a terminál maszk- és softkey-méretére méretezzük (pool_scaler), a többi darab ebből megy.
// A könyvtár saját méretezése a softkey-képeket a maszk arányában nagyítja, ami nem négyzetes /
// eltérő arányú softkey-knél (pl. Müller / Dammann HD12, Touch1200) kilógó ikonokat ad.
struct PoolSource {
    const std::uint8_t *pool = nullptr;
    std::uint32_t size = 0;
    std::vector<std::uint8_t> scaled;
};
PoolSource g_pool;

bool pool_chunk(std::uint32_t, std::uint32_t offset, std::uint32_t n, std::uint8_t *buf, void *parent) {
    if (!g_pool.pool) return false;
    if (offset == 0 || g_pool.scaled.size() != g_pool.size) {
        ScaleTarget t;
        if (auto *vt = static_cast<isobus::VirtualTerminalClient *>(parent)) {
            t.mask_px = vt->get_number_x_pixels();
            t.key_w = vt->get_softkey_x_axis_pixels();
            t.key_h = vt->get_softkey_y_axis_pixels();
            t.font_supported = [vt](std::uint8_t c) {
                return vt->get_font_size_supported(static_cast<isobus::VirtualTerminalClient::FontSize>(c));
            };
            t.aux_objects = vt->get_connected_vt_version() != isobus::VirtualTerminalClient::VTVersion::Version2OrOlder;
        }
        g_pool.scaled = scale_pool(g_pool.pool, g_pool.size, t);
    }
    if (static_cast<std::uint64_t>(offset) + n > g_pool.scaled.size()) return false;
    std::memcpy(buf, g_pool.scaled.data() + offset, n);
    return true;
}

// AUX-N hozzárendelések betöltése / mentése (a VT kliens hívja, context = AuxN)
using AuxFn = isobus::VirtualTerminalClient::AssignedAuxiliaryFunction;
std::vector<AuxFn> aux_load(std::uint64_t name, std::uint16_t model, void *ctx) {
    std::vector<AuxFn> r;
    if (!ctx) return r;
    for (const auto &a : static_cast<AuxN *>(ctx)->load(name, model))
        r.emplace_back(a.function, a.input, static_cast<isobus::VirtualTerminalClient::AuxiliaryTypeTwoFunctionType>(a.type));
    return r;
}
void aux_store(std::uint64_t name, std::uint16_t model, const std::vector<AuxFn> &list, void *ctx) {
    if (!ctx) return;
    std::vector<AuxAssignment> v;
    for (const auto &a : list)
        v.push_back({a.functionObjectID, a.inputObjectID, static_cast<std::uint8_t>(a.functionType)});
    static_cast<AuxN *>(ctx)->store(name, model, v);
}
} // namespace

SeederApp::SeederApp(Shared &shared) : shared_(shared) {
    auto screen = std::make_unique<VirtualScreen>(vt::DM_MAIN);
    vscreen_ = screen.get();
    vui_ = std::make_unique<VtUi>(shared_, std::move(screen));
    // AUX gomb → az azonos softkey művelete a valódi VT felületen (ha még nincs, a virtuálison)
    aux_ = std::make_unique<AuxN>([this](std::uint16_t key, std::uint8_t act) {
        VtUi *ui = ui_ ? ui_.get() : vui_.get();
        if (ui) ui->on_softkey(key, act);
    });
}

void SeederApp::set_aux_storage(std::function<std::vector<std::uint8_t>()> load,
                                std::function<bool(const std::vector<std::uint8_t> &)> save) {
    aux_->set_storage(std::move(load), std::move(save));
}

std::string SeederApp::aux_json() const { return aux_->json(); }

bool SeederApp::aux_input(std::uint16_t function, std::uint16_t value1) { return aux_->on_input(function, value1); }

std::string SeederApp::virtual_vt_json() const { return vscreen_ ? vscreen_->json(vt::POOL_VERSION) : "{}"; }

bool SeederApp::virtual_vt_event(const std::string &kind, std::uint16_t id, std::uint8_t act, std::uint32_t value) {
    if (!vui_) return false;
    if (kind == "key") vui_->on_softkey(id, act);
    else if (kind == "button") vui_->on_button(id, act);
    else if (kind == "num") vui_->on_numeric(id, value);
    else if (kind == "aux") return aux_->on_input(id, static_cast<std::uint16_t>(value));   // AUX-N próba
    else return false;
    return true;
}
SeederApp::~SeederApp() {
    // a VT kliens leállításkor még menthet AUX hozzárendelést (aux_ legyen még élő)
    if (vt_) vt_->terminate();
}

bool SeederApp::start(std::uint8_t can_channel, const std::uint8_t *pool, std::uint32_t pool_size) {
    if (!pool || pool_size < 256) return false;
    can_channel_ = can_channel;
    pool_ = pool;
    pool_size_ = pool_size;
    int delay = 0;
    {
        std::lock_guard<std::mutex> g(shared_.lock);
        delay = shared_.core.settings().isobus_delay_s;
    }
    if (delay > 0) {
        // a címigénylés (bejelentkezés a buszra) később indul, hogy a lassabban induló terminál lássa
        start_pending_ = true;
        start_delay_ = static_cast<float>(delay);
        return true;
    }
    return do_start();
}

bool SeederApp::do_start() {
    start_pending_ = false;
    const std::uint8_t can_channel = can_channel_;
    const std::uint8_t *pool = pool_;
    const std::uint32_t pool_size = pool_size_;
    int ecu_instance = 0;
    {
        std::lock_guard<std::mutex> g(shared_.lock);
        ecu_instance = shared_.core.settings().ecu_instance;
    }
    isobus::NAME name(0);
    name.set_arbitrary_address_capable(true);
    name.set_industry_group(2);   // mezőgazdasági
    name.set_device_class(4);     // vetőgépek (Planters/Seeders)
    name.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::RateControl));
    name.set_identity_number(SEEDER_IDENTITY_NUMBER + static_cast<unsigned>(ecu_instance));
    name.set_ecu_instance(static_cast<std::uint8_t>(ecu_instance));
    name.set_function_instance(0);
    name.set_device_class_instance(0);
    name.set_manufacturer_code(SEEDER_MANUFACTURER_CODE);

    ecu_ = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(name, can_channel);
    const std::vector<isobus::NAMEFilter> vt_filter = {
        isobus::NAMEFilter(isobus::NAME::NAMEParameters::FunctionCode,
                           static_cast<std::uint8_t>(isobus::NAME::Function::VirtualTerminal))};
    const std::vector<isobus::NAMEFilter> tc_filter = {
        isobus::NAMEFilter(isobus::NAME::NAMEParameters::FunctionCode,
                           static_cast<std::uint8_t>(isobus::NAME::Function::TaskController)),
        isobus::NAMEFilter(isobus::NAME::NAMEParameters::FunctionInstance, 0)};
    vt_partner_ = isobus::CANNetworkManager::CANNetwork.create_partnered_control_function(can_channel, vt_filter);
    tc_partner_ = isobus::CANNetworkManager::CANNetwork.create_partnered_control_function(can_channel, tc_filter);
    if (!ecu_ || !vt_partner_ || !tc_partner_ || !pool || pool_size < 256) return false;

    vt_ = std::make_shared<isobus::VirtualTerminalClient>(vt_partner_, ecu_);
    g_pool.pool = pool;
    g_pool.size = pool_size;
    g_pool.scaled.clear();
    // saját méretezés feltöltés közben (lásd pool_chunk): a 400×400-ra tervezett pool a terminál
    // maszkjához, a softkey-képek a gomb valódi méretéhez igazodnak
    vt_->register_object_pool_data_chunk_callback(0, pool_size, &pool_chunk, vt::POOL_VERSION);
    ui_ = std::make_unique<VtUi>(shared_, make_real_vt_sink(vt_));
    ui_->attach(*vt_);
    // AUX-N: a terminál AUX menüjében a joystick gombjai a pool AUX funkcióihoz köthetők
    vt_->set_auxiliary_assignment_callbacks(&aux_load, &aux_store, aux_.get());
    vt_->set_auxiliary_functions_enabled(true);
    vt_->get_auxiliary_function_event_dispatcher().add_listener(
        [this](const isobus::VirtualTerminalClient::AuxiliaryFunctionEvent &e) {
            aux_->on_input(e.function.functionObjectID, e.value1);
        });
    vt_->get_auxiliary_assignment_failure_event_dispatcher().add_listener(
        [this](const isobus::VirtualTerminalClient::AuxiliaryAssignmentFailureEvent &e) {
            aux_->failed(static_cast<unsigned>(e.reason));
        });
    vt_->initialize(true);

    tc_ = std::make_unique<TcLink>(shared_, tc_partner_, ecu_);

    // sebesség: csak figyelünk (nem küldünk sebességüzenetet)
    speed_ = std::make_unique<isobus::SpeedMessagesInterface>(nullptr);
    speed_->initialize();
    nmea_ = std::make_unique<isobus::NMEA2000MessageInterface>(nullptr, false, false, false, false, false, false, false);
    nmea_->initialize();
    started_ = true;
    return true;
}

seeder::IsobusSpeeds SeederApp::read_speeds() {
    seeder::IsobusSpeeds r;
    if (!speed_) return r;
    const std::uint32_t now = isobus::SystemTiming::get_timestamp_ms();
    auto age = [now](std::uint32_t ts) { return now >= ts ? now - ts : 0u; };
    if (speed_->get_number_received_ground_based_speed_sources() > 0) {
        auto m = speed_->get_received_ground_based_speed(0);
        if (m) r.ground = {true, m->get_machine_speed() * 0.0036f, age(m->get_timestamp_ms())};
    }
    if (speed_->get_number_received_wheel_based_speed_sources() > 0) {
        auto m = speed_->get_received_wheel_based_speed(0);
        if (m) r.wheel = {true, m->get_machine_speed() * 0.0036f, age(m->get_timestamp_ms())};
    }
    if (speed_->get_number_received_machine_selected_speed_sources() > 0) {
        auto m = speed_->get_received_machine_selected_speed(0);
        if (m) r.machine = {true, m->get_machine_speed() * 0.0036f, age(m->get_timestamp_ms())};
    }
    if (nmea_ && nmea_->get_number_received_course_speed_over_ground_message_sources() > 0) {
        auto m = nmea_->get_received_course_speed_over_ground_message(0);
        if (m) r.nmea = {true, m->get_speed_over_ground() * 3.6f, age(m->get_timestamp())};
    }
    return r;
}

void SeederApp::update(float dt_s, const HwInputs &hw) {
    if (speed_) speed_->update();
    seeder::TickInputs in;
    in.dt_s = dt_s;
    in.wheel = hw.wheel;
    in.fan = hw.fan;
    for (int i = 0; i < seeder::MAX_METERS; ++i) in.meter[i] = hw.meter[i];
    in.work_contact = hw.work_contact;
    in.hopper_contact[0] = hw.hopper_contact[0];
    in.hopper_contact[1] = hw.hopper_contact[1];
    in.pressure_v = hw.pressure_v;
    in.temp_c[0] = hw.temp_c[0];
    in.temp_c[1] = hw.temp_c[1];
    in.scale_valid = hw.scale_valid;
    in.scale_raw = hw.scale_raw;
    in.isobus = read_speeds();
    in.isobus.aog = hw.aog_speed;
    in.aog = hw.aog;
    in.crank_button = hw.crank_button;
    in.tc_connected = tc_ && tc_->connected();
    Snapshot snap;
    {
        std::lock_guard<std::mutex> g(shared_.lock);
        shared_.core.tick(in);
        snap.st = shared_.core.status();
        snap.set = shared_.core.settings();
        snap.per = shared_.core.persistent();
        snap.out = shared_.core.outputs();
    }
    vui_timer_ += dt_s;
    if (vui_ && vui_timer_ >= 0.2f) {
        vui_timer_ = 0.0f;
        vui_->update(snap);
    }
    if (start_pending_) {
        start_delay_ -= dt_s;
        if (start_delay_ <= 0.0f) do_start();
    }
    if (!started_) return;
    watchdog(dt_s, snap.set.isobus_retry_s);
    if (tc_) tc_->update(snap);
    ui_timer_ += dt_s;
    if (ui_timer_ >= 0.2f) {
        ui_timer_ = 0.0f;
        ui_->update(snap);
    }
}

void SeederApp::announce() {
    // saját címigénylés (PGN 60928) újraküldése a globális címre: a később induló terminál is lásson
    if (!ecu_ || !ecu_->get_address_valid()) return;
    const std::uint64_t name = ecu_->get_NAME().get_full_name();
    std::uint8_t d[8];
    for (int i = 0; i < 8; ++i) d[i] = static_cast<std::uint8_t>(name >> (8 * i));
    if (isobus::CANNetworkManager::CANNetwork.send_can_message(0xEE00, d, 8, ecu_, nullptr,
                                                                isobus::CANIdentifier::CANPriority::PriorityDefault6))
        ++announces_;
}

void SeederApp::watchdog(float dt, int retry_s) {
    const bool vt_ok = vt_ && vt_->get_is_connected();
    const bool vt_found = vt_partner_ && vt_partner_->get_address_valid();
    if (reconnect_req_.exchange(false)) {
        announce();
        if (vt_) vt_->restart_communication();
        if (tc_) tc_->restart();
        ++reconnects_;
        vt_down_s_ = vt_found_down_s_ = 0.0f;
        return;
    }
    if (vt_ok || retry_s <= 0) {
        vt_down_s_ = vt_found_down_s_ = announce_timer_ = 0.0f;
        return;
    }
    vt_down_s_ += dt;
    announce_timer_ += dt;
    // nincs VT kapcsolat: időnként újra bejelentkezünk (ártalmatlan, a címet nem változtatja)
    if (announce_timer_ >= static_cast<float>(retry_s)) {
        announce_timer_ = 0.0f;
        announce();
    }
    // a terminál a buszon van, mégsem jön létre a kapcsolat: a VT kapcsolatot újrakezdjük
    // (3× hosszabb várakozás, hogy egy lassú pool-feltöltést ne szakítsunk meg)
    vt_found_down_s_ = vt_found ? vt_found_down_s_ + dt : 0.0f;
    if (vt_found_down_s_ >= 3.0f * static_cast<float>(retry_s)) {
        vt_found_down_s_ = 0.0f;
        if (vt_) vt_->restart_communication();
        ++reconnects_;
    }
}

IsobusDiag SeederApp::diag() const {
    IsobusDiag d;
    d.started = started_;
    d.start_in_s = start_pending_ ? (start_delay_ > 0.0f ? start_delay_ : 0.0f) : 0.0f;
    d.announces = announces_;
    d.reconnects = reconnects_;
    if (ecu_) {
        d.address_claimed = ecu_->get_address_valid();
        d.address = d.address_claimed ? ecu_->get_address() : -1;
    }
    d.vt_found = vt_partner_ && vt_partner_->get_address_valid();
    d.tc_found = tc_partner_ && tc_partner_->get_address_valid();
    if (vt_) {
        d.vt_connected = vt_->get_is_connected();
        if (d.vt_connected) {
            d.vt_width = vt_->get_number_x_pixels();
            d.vt_height = vt_->get_number_y_pixels();
            d.vt_softkey_w = vt_->get_softkey_x_axis_pixels();
            d.vt_softkey_h = vt_->get_softkey_y_axis_pixels();
            d.vt_phys_keys = vt_->get_number_physical_softkeys();
            d.vt_virt_keys = vt_->get_number_virtual_softkeys();
            d.vt_version = static_cast<int>(vt_->get_connected_vt_version());
        }
    }
    if (ui_) {
        d.vt_cmd_sent = ui_->sent();
        d.vt_cmd_failed = ui_->failed();
        d.page = ui_->page_name();
    }
    if (tc_) {
        d.tc_connected = tc_->connected();
        d.ddop_uploads = tc_->uploads();
    }
    return d;
}

} // namespace app

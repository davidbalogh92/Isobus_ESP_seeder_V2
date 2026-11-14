#include "seeder_app.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#include "aux_n.hpp"
#include "ext_display.hpp"
#include "keya.hpp"
#include "wifi_service.hpp"
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

#include <cmath>
#include <cstdlib>

#ifndef SEEDER_FW_VERSION
#define SEEDER_FW_VERSION "2.5.1"
#endif

namespace app {

#ifndef SEEDER_IDENTITY_NUMBER
#define SEEDER_IDENTITY_NUMBER 0x15EED
#endif
// Gyártókód 0: kizárólag fejlesztési/kísérleti célra (a vetőgép-monitorral a Müller terminál elfogadta).
#ifndef SEEDER_MANUFACTURER_CODE
#define SEEDER_MANUFACTURER_CODE 0
#endif

namespace {
// A pool feltöltése darabokban: az első darab kérésekor (ekkor a VT méretei már ismertek) rögzítjük a
// terminál maszk- és softkey-méretét, és minden darabot ebből méretezünk (pool_scaler) – a pool teljes
// másolata nélkül, mert PSRAM nélküli panelen (RCv1.7) a ~180 kB-os másolatnak nincs hely, és a
// feltöltés elakadt (RTS / CTS után nem ment adat).
// A könyvtár saját méretezése a softkey-képeket a maszk arányában nagyítja, ami nem négyzetes /
// eltérő arányú softkey-knél (pl. Müller / Dammann HD12, Touch1200) kilógó ikonokat ad.
struct PoolSource {
    const std::uint8_t *pool = nullptr;
    std::uint32_t size = 0;
    ScaleTarget target;
    bool ready = false;
};
PoolSource g_pool;

bool pool_chunk(std::uint32_t, std::uint32_t offset, std::uint32_t n, std::uint8_t *buf, void *parent) {
    if (!g_pool.pool) return false;
    if (offset == 0 || !g_pool.ready) {
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
        g_pool.target = std::move(t);
        g_pool.ready = true;
    }
    return scale_pool_range(g_pool.pool, g_pool.size, g_pool.target, offset, n, buf);
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
    keya_ = std::make_unique<KeyaBus>();
    wifi_ = std::make_unique<WifiService>();
    ext_ = std::make_unique<ExtDisplay>();
    vui_->set_wifi_toggle([this] { wifi_request(!wifi_->on(), WifiService::Source::Vt); });
    // a külső kijelző parancsai (a vezérlési ciklusból, a mag zárja nélkül hívva → itt zárolunk)
    ExtDisplay::Handlers h;
    h.light = [this](const extdisp_light_command_t &c) -> ExtAck {
        std::lock_guard<std::mutex> g(shared_.lock);
        const seeder::Settings &s = shared_.core.settings();
        if (!s.lights_enable) return {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NOT_SUPPORTED};
        if (c.light > 2) return {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NO_SUCH_UNIT};
        shared_.core.set_lamp(c.light - 1, c.state);
        return {};
    };
    h.wifi = [this](const extdisp_wifi_command_t &c) -> ExtAck {
        switch (wifi_request(c.on == EXTDISP_WIFI_CMD_ON, WifiService::Source::Display)) {
        case WifiService::Result::InhibitedWork: return {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_WORK_SWITCH};
        case WifiService::Result::BusyOta: return {EXTDISP_ACK_BUSY, EXTDISP_INHIBIT_NONE};
        default: return {};
        }
    };
    h.tank = [this](const extdisp_tank_command_t &c) -> ExtAck {
        std::lock_guard<std::mutex> g(shared_.lock);
        const int n = seeder::Seeder::tank_fill_count(shared_.core.settings());
        if (n == 0) return {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NOT_SUPPORTED};
        // közös mennyiségnél a 0. (közös) tartály, tartályonkéntinél az 1. és 2.
        int i = -1;
        if (n == 1 && c.tank == EXTDISP_TANK_SHARED) i = 0;
        if (n == 2 && (c.tank == 1 || c.tank == 2)) i = c.tank - 1;
        if (i < 0) return {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NO_SUCH_UNIT};
        const float kg = static_cast<float>(c.amount_hg) / 10.0f;
        bool ok = false;
        if (c.action == EXTDISP_TANK_ACT_SET) ok = shared_.core.tank_set(i, kg);
        else if (c.action == EXTDISP_TANK_ACT_ADD) ok = shared_.core.tank_add(i, kg);
        else ok = shared_.core.tank_set(i, 0.0f);
        return ok ? ExtAck{} : ExtAck{EXTDISP_ACK_BAD_DATA, EXTDISP_INHIBIT_NONE};
    };
    h.calib = [this](const extdisp_calib_command_t &c) -> ExtAck { return ext_calib_command(c); };
    ext_->set_handlers(std::move(h));
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

void SeederApp::set_can_sender(std::function<bool(std::uint32_t, const std::uint8_t *, std::uint8_t)> send) {
    keya_->set_sender(std::move(send));
}
bool SeederApp::on_can_frame(std::uint32_t id, const std::uint8_t *data, std::uint8_t len) {
    return keya_->on_frame(id, data, len);
}
std::string SeederApp::keya_json() const { return keya_->json(); }

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
    // külső kijelző: saját NAME-mel jelentkezik (mezőgazdaság, funkciókód 135 „Operator Controls – Machine
    // Specific”, gyártókód 0); a hiánya semmit nem befolyásol
    const std::vector<isobus::NAMEFilter> disp_filter = {
        isobus::NAMEFilter(isobus::NAME::NAMEParameters::IndustryGroup, 2),
        isobus::NAMEFilter(isobus::NAME::NAMEParameters::FunctionCode, 135),
        isobus::NAMEFilter(isobus::NAME::NAMEParameters::ManufacturerCode, 0)};
    display_partner_ = isobus::CANNetworkManager::CANNetwork.create_partnered_control_function(can_channel, disp_filter);
    if (display_partner_ && ecu_) {
        display_partner_->add_parameter_group_number_callback(
            EXTDISP_PGN,
            [](const isobus::CANMessage &m, void *self) {
                const auto &d = m.get_data();
                static_cast<SeederApp *>(self)->ext_->on_frame(d.data(), d.size());
            },
            this, ecu_);
        ext_->set_sender([this](const std::uint8_t *f) {
            return isobus::CANNetworkManager::CANNetwork.send_can_message(
                EXTDISP_PGN, f, EXTDISP_FRAME_LEN, ecu_, display_partner_, isobus::CANIdentifier::CANPriority::PriorityDefault6);
        });
    }
    if (!ecu_ || !vt_partner_ || !tc_partner_ || !pool || pool_size < 256) return false;

    vt_ = std::make_shared<isobus::VirtualTerminalClient>(vt_partner_, ecu_);
    g_pool.pool = pool;
    g_pool.size = pool_size;
    g_pool.ready = false;
    // saját méretezés feltöltés közben (lásd pool_chunk): a 400×400-ra tervezett pool a terminál
    // maszkjához, a softkey-képek a gomb valódi méretéhez igazodnak
    vt_->register_object_pool_data_chunk_callback(0, pool_size, &pool_chunk, vt::POOL_VERSION);
    ui_ = std::make_unique<VtUi>(shared_, make_real_vt_sink(vt_));
    ui_->set_wifi_toggle([this] { wifi_request(!wifi_->on(), WifiService::Source::Vt); });
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
    keya_->setup_step(dt_s);   // Keya beállító mód (CAN-sebességváltás a magzáron kívül)
    Snapshot snap;
    {
        std::lock_guard<std::mutex> g(shared_.lock);
        // Keya CAN-motor: a pozícióból képzett impulzusok és a motorhibák a mag bemenetére
        const seeder::Settings &cs = shared_.core.settings();
        if (KeyaBus::active(cs) && !cs.sim_inputs) keya_->feedback(cs, dt_s, in.meter, in.motor_fault);
        shared_.core.tick(in);
        if (KeyaBus::active(shared_.core.settings()))
            keya_->command(shared_.core.settings(), shared_.core.outputs(), dt_s);
        // külső kijelző STOP gombja (a szívverés jelzőbitjéből; kapcsolat nélkül elengedettnek számít)
        shared_.core.set_external_stop(ext_->stop_held());
        ext_calib_hold(isobus::SystemTiming::get_timestamp_ms());
        snap.st = shared_.core.status();
        snap.set = shared_.core.settings();
        snap.per = shared_.core.persistent();
        snap.out = shared_.core.outputs();
    }
    // WiFi szerviz mód: munkakapcsoló aktív = van munkakapcsoló ÉS a gép lent van
    const bool work_active = snap.set.work_sw > 0 && snap.st.work_down;
    wifi_->tick(dt_s, work_active, wifi_clients_.load(), ota_running_.load(), snap.set.wifi_timeout_min);
    snap.wifi_state = static_cast<std::uint8_t>(wifi_->state());
    snap.wifi_minutes = wifi_->minutes_left();
    snap.ext_connected = ext_->connected();
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
    update_ext_display(snap);
    watchdog(dt_s, snap.set.isobus_retry_s);
    if (tc_) tc_->update(snap);
    ui_timer_ += dt_s;
    if (ui_timer_ >= 0.2f) {
        ui_timer_ = 0.0f;
        ui_->update(snap);
    }
}

std::string SeederApp::ext_display_json() const { return ext_->json(); }

WifiService::Result SeederApp::wifi_request(bool on, WifiService::Source src) {
    const bool was = wifi_->on();
    const WifiService::Result r = wifi_->request(on, src);
    const char *msg = nullptr;
    if (r == WifiService::Result::InhibitedWork) msg = "WiFi: aktív munkakapcsolónál nem kapcsolható be";
    else if (r == WifiService::Result::BusyOta) msg = "WiFi: frissítés (OTA) fut – nem kapcsolható ki";
    else if (on && !was) msg = "WiFi szerviz mód BE";
    else if (!on && was) msg = "WiFi szerviz mód KI";
    if (msg) {
        std::lock_guard<std::mutex> g(shared_.lock);
        shared_.core.notify(msg);
    }
    return r;
}

namespace {
std::uint8_t inhibit_of(seeder::Refusal r) {
    using seeder::Refusal;
    switch (r) {
    case Refusal::Moving: return EXTDISP_INHIBIT_MOVING;
    case Refusal::Metering: return EXTDISP_INHIBIT_METERING;
    case Refusal::Busy: return EXTDISP_INHIBIT_ALREADY_RUNNING;
    case Refusal::BadParam: return EXTDISP_INHIBIT_BAD_PARAM;
    case Refusal::NoSuchUnit: return EXTDISP_INHIBIT_NO_SUCH_UNIT;
    case Refusal::NotSupported: return EXTDISP_INHIBIT_NOT_SUPPORTED;
    case Refusal::NoScaleData: return EXTDISP_INHIBIT_NO_SCALE_DATA;
    case Refusal::ScaleEmpty: return EXTDISP_INHIBIT_SCALE_EMPTY;
    case Refusal::NoWeight: return EXTDISP_INHIBIT_NO_WEIGHT;
    case Refusal::ListFull: return EXTDISP_INHIBIT_LIST_FULL;
    case Refusal::NotElectric: return EXTDISP_INHIBIT_NOT_ELECTRIC;
    case Refusal::Tuning: return EXTDISP_INHIBIT_TUNING;
    case Refusal::TargetReached: return EXTDISP_INHIBIT_TARGET_REACHED;
    case Refusal::ExternalStop: return EXTDISP_INHIBIT_ALREADY_RUNNING;
    default: return EXTDISP_INHIBIT_BAD_PARAM;
    }
}
std::uint32_t u24(double v) {
    const double r = std::round(v);
    return r <= 0.0 ? 0u : (r >= 16777215.0 ? 16777215u : static_cast<std::uint32_t>(r));
}
} // namespace

ExtAck SeederApp::ext_calib_command(const extdisp_calib_command_t &c) {
    // a kijelző leforgatás-parancsa a mag meglévő crank_* műveleteire; elutasításnál a mag oka megy vissza
    std::lock_guard<std::mutex> g(shared_.lock);
    seeder::Seeder &core = shared_.core;
    const seeder::Settings &s = core.settings();
    core.clear_refusal();
    bool ok = true;
    switch (c.action) {
    case EXTDISP_CALIB_ACT_NEW: core.crank_reset_count(); break;
    case EXTDISP_CALIB_ACT_SELECT_FEEDER:
        if (c.param > seeder::meter_count(s)) return {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NO_SUCH_UNIT};
        ok = core.crank_set_meter(c.param - 1);
        break;
    case EXTDISP_CALIB_ACT_SELECT_TURNS: ok = core.crank_set_turns(c.param); break;
    case EXTDISP_CALIB_ACT_ADD_WEIGHT: ok = core.crank_add_weight(static_cast<float>(c.value) / 1000.0f); break;
    case EXTDISP_CALIB_ACT_TAKE_SCALE:
        if (!board_scale_ || !s.scale_enable) return {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NOT_SUPPORTED};
        ok = core.crank_take_scale();
        break;
    case EXTDISP_CALIB_ACT_DELETE_WEIGHT:
        if (c.param == EXTDISP_CALIB_DELETE_ALL) core.crank_clear_weights();
        else ok = core.crank_del_weight(c.param);
        break;
    case EXTDISP_CALIB_ACT_SAVE: ok = core.crank_save(); break;
    case EXTDISP_CALIB_ACT_COPY_TO_OTHER: ok = core.crank_copy_to_other(); break;
    case EXTDISP_CALIB_ACT_TARE_SCALE:
        if (!board_scale_ || !s.scale_enable) return {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NOT_SUPPORTED};
        ok = core.scale_tare();
        break;
    default: return {EXTDISP_ACK_INHIBITED, EXTDISP_INHIBIT_NOT_SUPPORTED};
    }
    if (ok) return {};
    return {EXTDISP_ACK_INHIBITED, inhibit_of(core.last_refusal())};
}

void SeederApp::ext_calib_hold(std::uint32_t now_ms) {
    // motoros leforgatás a kijelzőről: amíg a leforgatás képernyőn a START él (és nincs STOP, a szívverés
    // 600 ms-on belül jön), ciklusonként frissítjük a tartást; megszűnéskor azonnal elengedjük.
    // Csak a kijelző által indított tartást kezeljük: a VT / web tartását nem engedjük el.
    seeder::Seeder &core = shared_.core;
    const bool req = ext_->calib_hold(now_ms);
    if (req && !ext_hold_req_prev_) {
        // felfutó él: egyszer próbál indulni (elutasításnál a szöveg egyszer jelenik meg, nem ciklusonként)
        ext_holding_ = core.crank_hold(true, 0.3f);
    } else if (req && ext_holding_) {
        if (core.crank_held()) core.crank_hold(true, 0.3f);
        else ext_holding_ = false;   // a mag megállította (célfordulat, biztonság): új nyomásig nem indul
    } else if (!req && ext_holding_) {
        core.crank_hold(false);
        ext_holding_ = false;
    }
    ext_hold_req_prev_ = req;
}

void SeederApp::update_ext_display(const Snapshot &snap) {
    const std::uint32_t now = isobus::SystemTiming::get_timestamp_ms();
    const ExtDisplayMachine m = ext_display_machine(snap, now);
    const bool link = ecu_ && display_partner_ && ecu_->get_address_valid() && display_partner_->get_address_valid();
    ext_->tick(now, link, m);
}

ExtDisplayMachine SeederApp::ext_display_machine(const Snapshot &snap, std::uint32_t now_ms) {
    const seeder::Status &st = snap.st;
    const seeder::Settings &s = snap.set;
    ExtDisplayMachine m;
    m.caps.machine_type = EXTDISP_MACHINE_SEEDER;
    m.caps.protocol_version = EXTDISP_PROTOCOL_VERSION;
    m.caps.tank_count = static_cast<std::uint8_t>(st.tank_count < 0 ? 0 : (st.tank_count > 2 ? 2 : st.tank_count));
    m.caps.tank_mode = st.tank_fill_n == 2 ? EXTDISP_TANK_MODE_SEPARATE : EXTDISP_TANK_MODE_SHARED;
    m.caps.feeder_count = static_cast<std::uint8_t>(st.meter_count < 0 ? 0 : (st.meter_count > 2 ? 2 : st.meter_count));
    m.caps.light_count = s.lights_enable ? 2 : 0;
    const bool fan_watch = s.fan_min_rpm > 0 || s.fan_max_rpm > 0;   // van turbinafigyelés (riasztási határ)
    const bool scale = board_scale_ && s.scale_enable;
    m.caps.features = static_cast<std::uint8_t>(EXTDISP_FEATURE_WIFI_SERVICE | (s.lights_enable ? EXTDISP_FEATURE_LIGHTS : 0) |
                                                (st.tank_fill_n > 0 ? EXTDISP_FEATURE_TANK : 0) |
                                                EXTDISP_FEATURE_MACHINE_CALIB | (fan_watch ? EXTDISP_FEATURE_FAN : 0) |
                                                (scale ? EXTDISP_FEATURE_SCALE : 0));
    m.caps.fw_major = static_cast<std::uint8_t>(std::atoi(SEEDER_FW_VERSION));
    const bool work_active = s.work_sw > 0 && st.work_down;
    const bool metering = st.state == seeder::SowState::Sowing || st.state == seeder::SowState::Prefill;
    m.flags = static_cast<std::uint8_t>((work_active ? EXTDISP_ST_FLAG_WORK_SWITCH : 0) | (st.moving ? EXTDISP_ST_FLAG_MOVING : 0) |
                                        (metering ? EXTDISP_ST_FLAG_METERING : 0) |
                                        (snap.wifi_state != 0 ? EXTDISP_ST_FLAG_WIFI_ON : 0) |
                                        (st.lamp[0] ? EXTDISP_ST_FLAG_LIGHT1 : 0) | (st.lamp[1] ? EXTDISP_ST_FLAG_LIGHT2 : 0));
    const std::uint32_t fan_bits = (1u << static_cast<unsigned>(seeder::AlarmId::FanLow)) |
                                   (1u << static_cast<unsigned>(seeder::AlarmId::FanHigh));
    if (fan_watch && (st.alarms_active & fan_bits)) m.flags |= EXTDISP_ST_FLAG_FAN_ALARM;
    if (fan_watch) {
        const float r = std::round(st.fan_rpm);
        m.fan_rpm = static_cast<std::uint16_t>(r < 0.0f ? 0.0f : (r > 65000.0f ? 65000.0f : r));
    }
    const float ck = std::round(st.speed_kmh * 100.0f);
    m.speed_ckmh = static_cast<std::uint16_t>(ck < 0.0f ? 0.0f : (ck > 65000.0f ? 65000.0f : ck));
    m.wifi_state = snap.wifi_state;
    m.wifi_minutes = snap.wifi_minutes;
    m.tank_n = static_cast<std::uint8_t>(st.tank_fill_n);
    for (int i = 0; i < m.tank_n && i < 2; ++i) {
        extdisp_tank_status_t &t = m.tanks[i];
        t.tank = static_cast<std::uint8_t>(m.tank_n == 1 ? EXTDISP_TANK_SHARED : i + 1);
        const float hg = std::round(st.tank_kg[i] * 10.0f);
        t.remaining_hg = static_cast<std::uint32_t>(hg < 0.0f ? 0.0f : hg);
        const bool known = snap.per.tank_ref_kg[i] > 0.5f;
        t.fill_percent = known ? static_cast<std::uint8_t>(std::lround(st.tank_pct[i] > 100.0f ? 100.0f : st.tank_pct[i]))
                               : static_cast<std::uint8_t>(EXTDISP_FILL_UNKNOWN);
    }
    // leforgatás
    const seeder::CrankCalibration &cc = st.crank;
    const bool elec = seeder::electric_drive(s);
    const double target = seeder::chart::turns(cc.turn_option, s.crank_turns_10);
    const float ppr = seeder::meter_pulses_per_rev(s);
    const double whole = std::floor(target);
    const bool half_by_hand = ppr < 2 && target - whole > 0.25;   // = Seeder::crank_half_by_hand()
    m.calib.flags = static_cast<std::uint8_t>((cc.session ? EXTDISP_CALIB_FLAG_SESSION : 0) |
                                              (cc.armed ? EXTDISP_CALIB_FLAG_ZERO_FOUND : 0) |
                                              (cc.target_reached ? EXTDISP_CALIB_FLAG_TARGET_REACHED : 0) |
                                              (cc.motor_running ? EXTDISP_CALIB_FLAG_MOTOR_RUNNING : 0) |
                                              (elec ? EXTDISP_CALIB_FLAG_ELECTRIC : 0) |
                                              (half_by_hand ? EXTDISP_CALIB_FLAG_HALF_BY_HAND : 0));
    m.calib.feeder = static_cast<std::uint8_t>(cc.meter == 1 ? 2 : 1);
    m.calib.turn_option = static_cast<std::uint8_t>(cc.turn_option >= 0 && cc.turn_option < 3 ? cc.turn_option : 0);
    const double cr = std::round(static_cast<double>(cc.counted_revs) * 100.0);
    m.calib.counted_centirev = static_cast<std::uint16_t>(cr < 0.0 ? 0.0 : (cr > 65535.0 ? 65535.0 : cr));
    const double tr = std::round(target * 10.0);   // a V2 valódi célja („Ford. 1/10 ha” szerint)
    m.calib.target_decirev = static_cast<std::uint16_t>(tr < 0.0 ? 0.0 : (tr > 65535.0 ? 65535.0 : tr));
    const int wn = cc.weight_count < 0 ? 0 : (cc.weight_count > 10 ? 10 : cc.weight_count);
    m.calib.weight_count = static_cast<std::uint8_t>(wn);
    m.calib_result.feeder = m.calib.feeder;
    m.calib_result.total_g = u24(static_cast<double>(cc.measured_kg) * 1000.0);
    m.calib_result.mg_per_rev = st.crank_result.kg_per_rev > 0.0 ? u24(st.crank_result.kg_per_rev * 1.0e6) : 0u;
    m.weight_count = static_cast<std::uint8_t>(wn);
    for (int i = 0; i < wn; ++i) m.weights_g[i] = u24(static_cast<double>(cc.weights[i]) * 1000.0);
    m.saved_n = m.caps.feeder_count;
    for (int i = 0; i < m.saved_n && i < 2; ++i) m.saved_mg_per_rev[i] = u24(seeder::cal_kg_rev(s, i) * 1.0e6);
    m.scale = scale;
    if (scale) {
        // stabil: az utolsó ~1 s (5 × 200 ms) értékei 1 g-on belül
        const float v = st.scale.net_kg;
        bool stable = false;
        if (st.scale.valid) {
            if (scale_hist_n_ == 0 || now_ms - scale_hist_ms_ >= 200u) {   // 200 ms-onként egy minta
                scale_hist_[scale_hist_i_] = v;
                scale_hist_i_ = (scale_hist_i_ + 1) % 5;
                if (scale_hist_n_ < 5) ++scale_hist_n_;
                scale_hist_ms_ = now_ms;
            }
            float lo = v, hi = v;
            for (int i = 0; i < scale_hist_n_; ++i) {
                lo = std::min(lo, scale_hist_[i]);
                hi = std::max(hi, scale_hist_[i]);
            }
            stable = scale_hist_n_ >= 5 && hi - lo <= 0.001f;
        } else {
            scale_hist_n_ = 0;
        }
        m.scale_status.flags = static_cast<std::uint8_t>((st.scale.valid ? EXTDISP_SCALE_FLAG_VALID : 0) |
                                                         (stable ? EXTDISP_SCALE_FLAG_STABLE : 0));
        const double dg = std::round(static_cast<double>(v) * 10000.0);
        m.scale_status.weight_dg = st.scale.valid ? static_cast<std::int32_t>(std::max(-2.0e9, std::min(2.0e9, dg))) : 0;
    }
    return m;
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

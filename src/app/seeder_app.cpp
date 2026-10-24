#include "seeder_app.hpp"

#include "tc_link.hpp"
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

SeederApp::SeederApp(Shared &shared) : shared_(shared) {}
SeederApp::~SeederApp() = default;

bool SeederApp::start(std::uint8_t can_channel, const std::uint8_t *pool, std::uint32_t pool_size) {
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
    vt_->set_object_pool(0, pool, pool_size, vt::POOL_VERSION);
    // más felbontású terminálon a könyvtár feltöltés előtt átméretezi a 400×400-ra tervezett poolt
    vt_->set_object_pool_scaling(0, vt::DESIGN_MASK_PX, vt::DESIGN_SOFTKEY_PX);
    ui_ = std::make_unique<VtUi>(shared_, vt_);
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
    if (!started_) return;
    if (tc_) tc_->update(snap);
    ui_timer_ += dt_s;
    if (ui_timer_ >= 0.2f) {
        ui_timer_ = 0.0f;
        ui_->update(snap);
    }
}

IsobusDiag SeederApp::diag() const {
    IsobusDiag d;
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

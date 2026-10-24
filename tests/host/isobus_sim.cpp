// Teljes ISOBUS-szimuláció PC-n, virtuális CAN-buszon, hardver nélkül:
//   0. csatorna: a saját vetőgép ECU (SeederApp – ugyanaz a kód, mint az ESP32-n)
//   1. csatorna: egy VT-szerver és egy TC-szerver (AgIsoStack++)
// Ellenőrzi: címfoglalás, object pool feltöltés (+ átméretezés más VT felbontásra), VT értékfrissítés,
// softkey és beviteli események, DDOP feltöltés, TC-SEC szakaszparancs, TC-GEO előírt érték (kg/ha).
//
// Használat: isobus_sim <object_pool.iop> [vt_szélesség=400] [softkey_szélesség=120]
#include "seeder_app.hpp"

#include "seeder/vt_ids.hpp"

#include "isobus/hardware_integration/can_hardware_interface.hpp"
#include "isobus/hardware_integration/virtual_can_plugin.hpp"
#include "isobus/isobus/can_network_manager.hpp"
#include "isobus/isobus/can_stack_logger.hpp"
#include "isobus/isobus/isobus_standard_data_description_indices.hpp"
#include "isobus/isobus/isobus_task_controller_server.hpp"
#include "isobus/isobus/isobus_virtual_terminal_objects.hpp"
#include "isobus/isobus/isobus_virtual_terminal_server.hpp"
#include "isobus/isobus/isobus_virtual_terminal_server_managed_working_set.hpp"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <thread>

using namespace std::chrono_literals;

namespace {
int failures = 0;
void check(bool ok, const char *what) {
    std::printf("%s %s\n", ok ? "[ OK ]" : "[HIBA]", what);
    if (!ok) ++failures;
}

class QuietLogger : public isobus::CANStackLogger {
public:
    void sink_CAN_stack_log(LoggingLevel level, const std::string &text) override {
        if (level >= LoggingLevel::Warning) std::cout << "  [stack] " << text << "\n";
    }
};

// ------------------------------------------------------------------ minimális VT szerver
class SimVT : public isobus::VirtualTerminalServer {
public:
    SimVT(std::shared_ptr<isobus::InternalControlFunction> cf, std::uint16_t px, std::uint8_t sk)
        : VirtualTerminalServer(cf), px_(px), sk_(sk) {}
    using VirtualTerminalServer::update; // a könyvtárban protected
    bool get_is_enough_memory(std::uint32_t) const override { return true; }
    VTVersion get_version() const override { return VTVersion::Version4; }
    std::uint8_t get_number_of_navigation_soft_keys() const override { return 0; }
    std::uint8_t get_soft_key_descriptor_x_pixel_width() const override { return sk_; }
    std::uint8_t get_soft_key_descriptor_y_pixel_height() const override { return 80; }
    std::uint8_t get_number_of_possible_virtual_soft_keys_in_soft_key_mask() const override { return 64; }
    std::uint8_t get_number_of_physical_soft_keys() const override { return 10; }
    std::uint16_t get_data_mask_area_size_x_pixels() const override { return px_; }
    std::uint16_t get_data_mask_area_size_y_pixels() const override { return px_; }
    void suspend_working_set(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet>) override {}
    SupportedWideCharsErrorCode get_supported_wide_chars(std::uint8_t, std::uint16_t, std::uint16_t, std::uint8_t &,
                                                         std::vector<std::uint8_t> &) override {
        return SupportedWideCharsErrorCode::AnyOtherError;
    }
    std::vector<std::array<std::uint8_t, 7>> get_versions(isobus::NAME) override { return {}; }
    std::vector<std::array<std::uint8_t, 32>> get_extended_versions(isobus::NAME) override { return {}; }
    std::vector<std::uint8_t> get_supported_objects() const override {
        std::vector<std::uint8_t> v;
        for (std::uint8_t i = 0; i <= 47; ++i) v.push_back(i);
        return v;
    }
    std::vector<std::uint8_t> load_version(const std::vector<std::uint8_t> &, isobus::NAME) override { return {}; }
    bool save_version(const std::vector<std::uint8_t> &, const std::vector<std::uint8_t> &, isobus::NAME) override {
        ++saved;
        return true;
    }
    bool delete_version(const std::vector<std::uint8_t> &, isobus::NAME) override { return true; }
    bool delete_all_versions(std::uint8_t, isobus::NAME) override { return true; }
    bool delete_object_pool(isobus::NAME) override { return true; }
    void handle_unexpected_working_set_shutdown(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet>,
                                                UnexpectedShutdownReason) override {}
    void object_pool_parse_finished(std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> ws,
                                    bool success) override {
        parsed = success;
        working_set = ws;
        if (success) {
            // feltöltés után ez a working set kerül előtérbe (a valódi VT-n a kezelő választaná ki)
            activeWorkingSet = ws;
            auto cf = ws->get_control_function();
            if (cf) activeWorkingSetMasterAddress = cf->get_address();
        }
    }
    std::shared_ptr<isobus::VTObject> obj(std::uint16_t id) {
        return working_set ? working_set->get_object_by_id(id) : nullptr;
    }
    std::uint32_t number(std::uint16_t id) {
        auto o = std::dynamic_pointer_cast<isobus::OutputNumber>(obj(id));
        return o ? o->get_value() : 0xFFFFFFFFu;
    }
    std::uint16_t pointer(std::uint16_t id) {
        auto o = std::dynamic_pointer_cast<isobus::ObjectPointer>(obj(id));
        return o ? o->get_value() : 0xFFFF;
    }
    bool hidden(std::uint16_t id) {
        auto o = std::dynamic_pointer_cast<isobus::Container>(obj(id));
        return o ? o->get_hidden() : true;
    }
    std::uint16_t rect_width(std::uint16_t id) {
        auto o = obj(id);
        return o ? o->get_width() : 0;
    }
    bool parsed = false;
    int saved = 0;
    std::shared_ptr<isobus::VirtualTerminalServerManagedWorkingSet> working_set;

private:
    std::uint16_t px_;
    std::uint8_t sk_;
};

// ------------------------------------------------------------------ minimális TC szerver
class SimTC : public isobus::TaskControllerServer {
public:
    explicit SimTC(std::shared_ptr<isobus::InternalControlFunction> cf)
        : TaskControllerServer(cf, 1, 16, 16,
                               isobus::TaskControllerOptions().with_documentation().with_implement_section_control()
                                   .with_tc_geo_without_position_based_control()) {}
    bool activate_object_pool(std::shared_ptr<isobus::ControlFunction>, ObjectPoolActivationError &,
                              ObjectPoolErrorCodes &, std::uint16_t &, std::uint16_t &) override {
        activated = true;
        return true;
    }
    bool change_designator(std::shared_ptr<isobus::ControlFunction>, std::uint16_t,
                           const std::vector<std::uint8_t> &) override { return true; }
    bool deactivate_object_pool(std::shared_ptr<isobus::ControlFunction>) override { return true; }
    bool delete_device_descriptor_object_pool(std::shared_ptr<isobus::ControlFunction>,
                                              ObjectPoolDeletionErrors &) override { return true; }
    bool get_is_stored_device_descriptor_object_pool_by_structure_label(std::shared_ptr<isobus::ControlFunction>,
                                                                        const std::vector<std::uint8_t> &,
                                                                        const std::vector<std::uint8_t> &) override {
        return false;
    }
    bool get_is_stored_device_descriptor_object_pool_by_localization_label(
        std::shared_ptr<isobus::ControlFunction>, const std::array<std::uint8_t, 7> &) override { return false; }
    bool get_is_enough_memory_available(std::uint32_t) override { return true; }
    void identify_task_controller(std::uint8_t) override {}
    void on_client_version_received(std::shared_ptr<isobus::ControlFunction>, std::uint8_t) override {}
    void on_client_timeout(std::shared_ptr<isobus::ControlFunction>) override {}
    void on_process_data_acknowledge(std::shared_ptr<isobus::ControlFunction>, std::uint16_t, std::uint16_t,
                                     std::uint8_t, ProcessDataCommands) override {}
    bool on_value_command(std::shared_ptr<isobus::ControlFunction>, std::uint16_t, std::uint16_t ddi,
                          std::int32_t value, std::uint8_t &) override {
        (void)ddi;
        (void)value;
        ++values;
        return true;
    }
    bool store_device_descriptor_object_pool(std::shared_ptr<isobus::ControlFunction> client,
                                             const std::vector<std::uint8_t> &pool, bool) override {
        this->client = client;
        ddop_bytes = pool.size();
        return true;
    }
    using TaskControllerServer::send_set_value;
    std::shared_ptr<isobus::ControlFunction> client;
    std::size_t ddop_bytes = 0;
    bool activated = false;
    int values = 0;
};

bool wait_for(const std::function<bool()> &cond, const std::function<void()> &step, int seconds) {
    for (int i = 0; i < seconds * 20; ++i) {
        step();
        if (cond()) return true;
        std::this_thread::sleep_for(50ms);
    }
    return false;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Használat: isobus_sim <object_pool.iop> [vt_px] [softkey_px]\n";
        return 2;
    }
    const std::uint16_t vt_px = argc > 2 ? static_cast<std::uint16_t>(std::atoi(argv[2])) : 400;
    const std::uint8_t sk_px = argc > 3 ? static_cast<std::uint8_t>(std::atoi(argv[3])) : 120;
    std::ifstream f(argv[1], std::ios::binary);
    const std::vector<std::uint8_t> pool((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (pool.empty()) return 2;

    static QuietLogger logger;
    isobus::CANStackLogger::set_can_stack_logger_sink(&logger);
    isobus::CANStackLogger::set_log_level(isobus::CANStackLogger::LoggingLevel::Warning);
    isobus::CANHardwareInterface::set_number_of_can_channels(2);
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(0, std::make_shared<isobus::VirtualCANPlugin>("seeder"));
    isobus::CANHardwareInterface::assign_can_channel_frame_handler(1, std::make_shared<isobus::VirtualCANPlugin>("seeder"));
    if (!isobus::CANHardwareInterface::start()) return 3;
    std::this_thread::sleep_for(100ms);

    // --- szerverek az 1. csatornán
    isobus::NAME vt_name(0);
    vt_name.set_arbitrary_address_capable(true);
    vt_name.set_industry_group(2);
    vt_name.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::VirtualTerminal));
    vt_name.set_identity_number(1);
    vt_name.set_manufacturer_code(1407);
    auto vt_cf = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(vt_name, 1, 0x26);
    isobus::NAME tc_name(0);
    tc_name.set_arbitrary_address_capable(true);
    tc_name.set_industry_group(2);
    tc_name.set_function_code(static_cast<std::uint8_t>(isobus::NAME::Function::TaskController));
    tc_name.set_identity_number(2);
    tc_name.set_manufacturer_code(1407);
    auto tc_cf = isobus::CANNetworkManager::CANNetwork.create_internal_control_function(tc_name, 1, 0xF7);
    SimVT vt(vt_cf, vt_px, sk_px);
    vt.initialize();
    SimTC tc(tc_cf);
    tc.initialize();

    // --- saját ECU a 0. csatornán (szimulált sebesség és bemenetek: turbina, adagolók, munkakapcsoló)
    app::Shared shared;
    {
        seeder::Settings s = seeder::default_settings();
        s.speed_src = 3;
        s.sim_speed = 8.0f;
        s.sim_inputs = 1;
        s.tc_geo = 1;
        seeder::normalize(s);
        shared.core.load(s, seeder::Persistent{});
    }
    app::SeederApp seeder_app(shared);
    check(seeder_app.start(0, pool.data(), static_cast<std::uint32_t>(pool.size())), "ECU indítás");

    auto step = [&]() {
        seeder_app.update(0.05f, app::HwInputs{});
        vt.update();
        tc.update();
    };
    std::printf("VT szimulált felbontás: %u×%u, softkey %u px széles\n", vt_px, vt_px, sk_px);
    check(wait_for([&] { return seeder_app.diag().address_claimed; }, step, 10), "címfoglalás");
    check(wait_for([&] { return seeder_app.diag().vt_connected; }, step, 90), "VT kapcsolat, object pool feltöltve");
    check(vt.parsed, "a VT szerver hibátlanul feldolgozta a (méretezett) poolt");
    {
        const auto d = seeder_app.diag();
        std::printf("       VT jelentett mérete: %u×%u, softkey %u×%u\n", d.vt_width, d.vt_height, d.vt_softkey_w,
                    d.vt_softkey_h);
    }

    // --- VT: értékek frissülnek-e
    {
        std::lock_guard<std::mutex> g(shared.lock);
        shared.core.set_master(true);
    }
    wait_for([] { return false; }, step, 4);
    check(vt.number(vt::OUT_SPEED) == 80, "VT: sebesség = 8,0 km/h");
    check(vt.pointer(vt::OP_K_MASTER) == vt::PIC_K_MASTER[1], "VT: vetés BE softkey ikon");
    check(vt.pointer(vt::OP_METER1) == vt::PIC_METER[static_cast<int>(seeder::MeterView::Work)], "VT: 1. adagoló hajt");
    check(vt.pointer(vt::OP_MSG) == vt::STR_MSG_SOWING, "VT: állapotsor: VETÉS");
    check(!vt.hidden(vt::CONT_METER2), "VT: 2. adagoló látszik");
    check(vt.hidden(vt::CONT_PRESSURE), "VT: nyomásmérés (alapból kikapcsolva) rejtve");
    std::printf("       turbina a VT-n: %u 1/min, 1. adagoló: %u (×0,1 kg/ha)\n", vt.number(vt::OUT_FAN_RPM),
                vt.number(vt::OUT_ACT1));
    check(vt.number(vt::OUT_FAN_RPM) > 3000 && vt.number(vt::OUT_FAN_RPM) < 4000, "VT: turbina fordulat");

    // --- VT → ECU események: softkey (2. adagoló KI) és beviteli mező (előírt érték 1)
    auto ecu_cf = vt.working_set ? vt.working_set->get_control_function() : nullptr;
    check(ecu_cf != nullptr, "VT oldalon ismert az ECU");
    if (ecu_cf) {
        const std::uint16_t sk = vt::SK_MAIN[1][0];
        vt.send_soft_key_activation_message(isobus::VirtualTerminalBase::KeyActivationCode::ButtonPressedOrLatched,
                                            vt::K_METER2, sk, 2, ecu_cf);
        vt.send_soft_key_activation_message(isobus::VirtualTerminalBase::KeyActivationCode::ButtonUnlatchedOrReleased,
                                            vt::K_METER2, sk, 2, ecu_cf);
        const auto &rd = seeder::PARAMS[seeder::P_rate1];
        vt.send_change_numeric_value_message(rd.vt_input_id, static_cast<std::uint32_t>(std::lround(180.0f / rd.step)),
                                             ecu_cf);
        wait_for([] { return false; }, step, 2);
        std::lock_guard<std::mutex> g(shared.lock);
        check(!shared.core.status().meter[1].enabled, "softkey: 2. adagoló KI");
        check(std::abs(shared.core.settings().rate1 - 180.0f) < 0.01f, "beviteli mező: 1. előírt = 180 kg/ha");
        shared.core.set_meter(1, true);
    }
    wait_for([] { return false; }, step, 1);
    check(vt.pointer(vt::OP_K_METER2) == vt::PIC_K_METER2[1], "VT: 2. adagoló softkey ikon újra BE");

    // --- TC: DDOP és parancsok
    check(wait_for([&] { return seeder_app.diag().tc_connected; }, step, 40), "TC kapcsolat, DDOP aktiválva");
    std::printf("       DDOP méret: %zu bájt\n", tc.ddop_bytes);
    if (tc.client) {
        using DDI = isobus::DataDescriptionIndex;
        tc.send_set_value(tc.client, static_cast<std::uint16_t>(DDI::SectionControlState), 2, 1);
        // 1. szakasz BE, 2. KI (2 bit / szakasz, 01 = be, 00 = ki)
        tc.send_set_value(tc.client, static_cast<std::uint16_t>(DDI::SetpointCondensedWorkState1_16), 2,
                          static_cast<std::int32_t>(0xFFFFFFF0u | 0x1u));
        // TC-GEO: 1. adagoló 150 kg/ha (mg/m²: ×100)
        tc.send_set_value(tc.client, static_cast<std::uint16_t>(DDI::SetpointMassPerAreaApplicationRate), 3, 15000);
        wait_for([] { return false; }, step, 2);
        std::lock_guard<std::mutex> g(shared.lock);
        if (!shared.core.status().tc_section_auto) shared.core.toggle_section_auto();
    }
    wait_for([] { return false; }, step, 2);
    {
        std::lock_guard<std::mutex> g(shared.lock);
        const auto &st = shared.core.status();
        check(st.tc_section_auto, "TC-SEC: automatikus szakaszvezérlés aktív");
        check(st.section_on[0] && !st.section_on[1], "TC-SEC: 1. szakasz be, 2. ki");
        check(std::abs(st.meter[0].target_rate - 150.0f) < 0.01f, "TC-GEO: 1. adagoló előírt 150 kg/ha");
        check(std::abs(st.meter[1].target_rate - 200.0f) < 0.01f, "TC-GEO: a 2. adagoló a saját beállítását tartja");
    }
    wait_for([] { return false; }, step, 2);
    check(vt.pointer(vt::OP_K_SEC) == vt::PIC_K_SEC[1], "VT: TC-SEC softkey aktív");
    check(vt.pointer(vt::OP_METER2) != vt::PIC_METER[static_cast<int>(seeder::MeterView::Work)],
          "VT: a lekapcsolt szakasz adagolója nem hajt");

    // --- gépbeállítás változás → DDOP újratöltés
    const unsigned uploads_before = seeder_app.diag().ddop_uploads;
    {
        std::lock_guard<std::mutex> g(shared.lock);
        shared.core.set_param(seeder::P_meter_count, 1);
    }
    check(wait_for([&] { return seeder_app.diag().ddop_uploads > uploads_before && seeder_app.diag().tc_connected; },
                   step, 40),
          "adagolószám változás után a DDOP újratöltődött");
    wait_for([] { return false; }, step, 2);
    check(vt.hidden(vt::CONT_METER2), "VT: 1 adagolónál a 2. rejtve");

    const auto d = seeder_app.diag();
    std::printf("\nVT parancsok: %u elküldve, %u sikertelen\n", d.vt_cmd_sent, d.vt_cmd_failed);
    isobus::CANHardwareInterface::stop();
    std::printf("%s (%d hiba)\n", failures ? "SZIMULÁCIÓ: HIBA" : "SZIMULÁCIÓ: MINDEN RENDBEN", failures);
    std::cout.flush();
    std::fflush(stdout);
    std::_Exit(failures ? 1 : 0); // a könyvtár háttérszálai miatt gyors kilépés
}

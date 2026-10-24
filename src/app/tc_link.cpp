#include "tc_link.hpp"

#include "isobus/isobus/can_NAME.hpp"
#include "isobus/isobus/can_internal_control_function.hpp"
#include "isobus/isobus/can_partnered_control_function.hpp"
#include "isobus/isobus/isobus_device_descriptor_object_pool.hpp"
#include "isobus/isobus/isobus_standard_data_description_indices.hpp"
#include "isobus/isobus/isobus_task_controller_client.hpp"

#include <array>
#include <cmath>
#include <cstdio>

namespace app {
namespace {
using DDI = isobus::DataDescriptionIndex;
using PDO = isobus::task_controller_object::DeviceProcessDataObject;
using DEO = isobus::task_controller_object::DeviceElementObject;
constexpr std::uint16_t NUL = 0xFFFF;

// Elemszámok (process data címzés)
constexpr std::uint16_t EN_DEVICE = 0, EN_CONNECTOR = 1, EN_BOOM = 2, EN_BIN0 = 3, EN_SECTION0 = 5;

// Objektum azonosítók a DDOP-ban
enum : std::uint16_t {
    ID_DEVICE_EL = 100, ID_ACT_WORK, ID_REQ_DEFAULT, ID_TOTAL_TIME, ID_TOTAL_DIST, ID_RX_STATE,
    ID_CONNECTOR = 110, ID_CONN_X, ID_CONN_Y, ID_CONN_TYPE, ID_CONN_PIVOT,
    ID_BOOM = 120, ID_BOOM_X, ID_BOOM_Y, ID_BOOM_Z, ID_WIDTH, ID_SET_WORK, ID_AREA, ID_SC_STATE, ID_TURN_ON,
    ID_TURN_OFF, ID_ACT_COND, ID_SET_COND, ID_OPTYPE,
    ID_BIN0 = 140,        // adagolónként 10 azonosító: elem, előírt, tényleges, összes, élettartam
    ID_SEC_EL = 200, ID_SEC_X = 220, ID_SEC_Y = 240, ID_SEC_W = 260,
    ID_VP_MM = 300, ID_VP_M, ID_VP_HA, ID_VP_MIN, ID_VP_KGHA, ID_VP_MS, ID_VP_KG, ID_VP_KM,
};
constexpr std::uint16_t BIN_EL = 0, BIN_SET = 1, BIN_ACT = 2, BIN_TOTAL = 3, BIN_LIFE = 4;
constexpr std::uint16_t bin_id(int meter, std::uint16_t what) { return static_cast<std::uint16_t>(ID_BIN0 + meter * 10 + what); }

constexpr std::uint8_t P_DEFAULT = static_cast<std::uint8_t>(PDO::PropertiesBit::MemberOfDefaultSet);
constexpr std::uint8_t P_SETTABLE = static_cast<std::uint8_t>(PDO::PropertiesBit::Settable);
constexpr std::uint8_t T_CHANGE = static_cast<std::uint8_t>(PDO::AvailableTriggerMethods::OnChange);
constexpr std::uint8_t T_TIME = static_cast<std::uint8_t>(PDO::AvailableTriggerMethods::TimeInterval);
constexpr std::uint8_t T_TOTAL = static_cast<std::uint8_t>(PDO::AvailableTriggerMethods::Total);
constexpr std::uint16_t d(DDI x) { return static_cast<std::uint16_t>(x); }

std::int32_t mm(float meters) { return static_cast<std::int32_t>(std::lround(meters * 1000.0f)); }
// kg/ha → mg/m² (DDI 6/7 egysége): 1 kg/ha = 100 mg/m²
std::int32_t kgha_to_ddi(float kg_ha) { return static_cast<std::int32_t>(std::lround(kg_ha * 100.0f)); }
} // namespace

std::uint8_t connector_type_for(int hitch) {
    // 0 függesztett → 3 (hárompont), 1 kormányzott vonórúd → 2, 2 vonószem → 1, 3 horog → 4, 4 gömbfej → 8
    switch (hitch) {
    case 1: return 2;
    case 2: return 1;
    case 3: return 4;
    case 4: return 8;
    default: return 3;
    }
}

TcGeometry make_tc_geometry(const seeder::Settings &s) {
    TcGeometry g;
    g.sections = seeder::section_count(s);
    g.connector_type = connector_type_for(s.hitch_type);
    const std::int32_t a = mm(s.geo_a_m), b = mm(s.geo_b_m), c = mm(s.geo_c_m);
    if (s.hitch_type == 0) {          // függesztett: DRP = vonópont
        g.connector_x_mm = 0;
        g.boom_x_mm = -b;
    } else if (s.hitch_type == 1) {   // kormányzott vonórúd
        g.has_pivot = true;
        g.pivot_x_mm = c;
        g.connector_x_mm = a + c;
        g.boom_x_mm = c - b;
    } else {                          // vontatott: DRP = tengely, csuklópont = vonópont
        g.connector_x_mm = c;
        g.boom_x_mm = c - b;
    }
    g.boom_y_mm = mm(s.geo_y_m);
    g.width_mm = mm(s.width_m);
    if (g.sections == 2) {
        const std::int32_t w0 = mm(seeder::section_width(s, 0)), w1 = mm(seeder::section_width(s, 1));
        const std::int32_t total = w0 + w1;
        g.sec_w_mm[0] = w0;
        g.sec_w_mm[1] = w1;
        g.sec_y_mm[0] = -total / 2 + w0 / 2;          // 1. szakasz balra (Y negatív)
        g.sec_y_mm[1] = -total / 2 + w0 + w1 / 2;
    } else {
        g.sec_w_mm[0] = g.width_mm;
        g.sec_y_mm[0] = 0;
    }
    return g;
}

std::uint32_t condensed_state(const bool *on, int count) {
    std::uint32_t v = 0;
    for (int i = 0; i < 16; ++i) {
        const std::uint32_t code = i < count ? (on[i] ? 1u : 0u) : 3u;
        v |= code << (2 * i);
    }
    return v;
}

bool build_ddop(isobus::DeviceDescriptorObjectPool &p, const seeder::Settings &s, std::uint64_t client_name) {
    p.clear();
    const TcGeometry g = make_tc_geometry(s);
    const int meters = seeder::meter_count(s);
    const std::uint32_t hash = seeder::ddop_relevant_hash(s);
    char label[8];
    std::snprintf(label, sizeof(label), "SD%05X", static_cast<unsigned>(hash & 0xFFFFFu));
    // magyar, tizedesvessző, 24 órás idő, metrikus mértékegységek
    const std::array<std::uint8_t, 7> loc = {'h', 'u', 0x00, 0x00, 0x00, 0x00, 0xFF};
    bool ok = p.add_device("Vetogep ESP32", "2.0.0", "ESP32S3", label, loc, {}, client_name);

    // --- eszköz
    ok &= p.add_device_element("Vetogep", EN_DEVICE, 0, DEO::Type::Device, ID_DEVICE_EL);
    ok &= p.add_device_process_data("Munkaallapot", d(DDI::ActualWorkState), NUL, P_DEFAULT, T_CHANGE, ID_ACT_WORK);
    ok &= p.add_device_process_data("Alap PD keres", d(DDI::RequestDefaultProcessData), NUL, 0, T_TOTAL, ID_REQ_DEFAULT);
    ok &= p.add_device_process_data("Munkaido", d(DDI::EffectiveTotalTime), ID_VP_MIN, P_DEFAULT | P_SETTABLE, T_TOTAL,
                                    ID_TOTAL_TIME);
    ok &= p.add_device_process_data("Megtett ut", d(DDI::EffectiveTotalDistance), ID_VP_KM, P_DEFAULT | P_SETTABLE,
                                    T_TOTAL, ID_TOTAL_DIST);
    if (s.tc_geo) {
        ok &= p.add_device_process_data("Terkep allapot", d(DDI::PrescriptionControlState), NUL,
                                        P_DEFAULT | P_SETTABLE, T_CHANGE | T_TIME, ID_RX_STATE);
    }

    // --- csatlakozó
    ok &= p.add_device_element("Csatlakozo", EN_CONNECTOR, ID_DEVICE_EL, DEO::Type::Connector, ID_CONNECTOR);
    ok &= p.add_device_property("Csatl. X", g.connector_x_mm, d(DDI::DeviceElementOffsetX), ID_VP_MM, ID_CONN_X);
    ok &= p.add_device_property("Csatl. Y", 0, d(DDI::DeviceElementOffsetY), ID_VP_MM, ID_CONN_Y);
    ok &= p.add_device_property("Csatl. tipus", g.connector_type, d(DDI::ConnectorType), NUL, ID_CONN_TYPE);
    if (g.has_pivot)
        ok &= p.add_device_property("Csuklopont X", g.pivot_x_mm, d(DDI::ConnectorPivotXOffset), ID_VP_MM, ID_CONN_PIVOT);

    // --- vetősor (munkapont = csoroszlyák)
    ok &= p.add_device_element("Vetosor", EN_BOOM, ID_DEVICE_EL, DEO::Type::Function, ID_BOOM);
    ok &= p.add_device_property("Sor X", g.boom_x_mm, d(DDI::DeviceElementOffsetX), ID_VP_MM, ID_BOOM_X);
    ok &= p.add_device_property("Sor Y", g.boom_y_mm, d(DDI::DeviceElementOffsetY), ID_VP_MM, ID_BOOM_Y);
    ok &= p.add_device_property("Sor Z", 0, d(DDI::DeviceElementOffsetZ), ID_VP_MM, ID_BOOM_Z);
    ok &= p.add_device_process_data("Munkaszelesseg", d(DDI::ActualWorkingWidth), ID_VP_M, P_DEFAULT, T_CHANGE, ID_WIDTH);
    ok &= p.add_device_process_data("Munkaallapot kert", d(DDI::SetpointWorkState), NUL, P_DEFAULT | P_SETTABLE,
                                    T_CHANGE, ID_SET_WORK);
    ok &= p.add_device_process_data("Terulet", d(DDI::TotalArea), ID_VP_HA, P_DEFAULT | P_SETTABLE, T_TOTAL, ID_AREA);
    ok &= p.add_device_property("Muvelet", 2, d(DDI::ActualCulturalPractice), NUL, ID_OPTYPE); // 2 = vetés
    if (s.tc_sec) {
        ok &= p.add_device_process_data("Szakaszvez. allapot", d(DDI::SectionControlState), NUL,
                                        P_DEFAULT | P_SETTABLE, T_CHANGE | T_TIME, ID_SC_STATE);
        ok &= p.add_device_property("Tehetetlenseg BE", s.delay_on_ms, d(DDI::SCSetpointTurnOnTime), ID_VP_MS, ID_TURN_ON);
        ok &= p.add_device_property("Tehetetlenseg KI", s.delay_off_ms, d(DDI::SCSetpointTurnOffTime), ID_VP_MS,
                                    ID_TURN_OFF);
    }
    ok &= p.add_device_process_data("Szakaszok 1-16", d(DDI::ActualCondensedWorkState1_16), NUL, P_DEFAULT, T_CHANGE,
                                    ID_ACT_COND);
    if (s.tc_sec) {
        ok &= p.add_device_process_data("Szakaszok kert 1-16", d(DDI::SetpointCondensedWorkState1_16), NUL,
                                        P_DEFAULT | P_SETTABLE, T_CHANGE, ID_SET_COND);
    }

    // --- adagolók (termék / bin): dokumentáció + TC-GEO előírt érték kg/ha-ban
    for (int m = 0; m < meters; ++m) {
        char name[20];
        std::snprintf(name, sizeof(name), "Adagolo %d", m + 1);
        ok &= p.add_device_element(name, static_cast<std::uint16_t>(EN_BIN0 + m), ID_BOOM, DEO::Type::Bin,
                                   bin_id(m, BIN_EL));
        ok &= p.add_device_process_data("Eloirt ertek", d(DDI::SetpointMassPerAreaApplicationRate), ID_VP_KGHA,
                                        P_DEFAULT | (s.tc_geo ? P_SETTABLE : 0), T_CHANGE, bin_id(m, BIN_SET));
        ok &= p.add_device_process_data("Tenyleges ertek", d(DDI::ActualMassPerAreaApplicationRate), ID_VP_KGHA,
                                        P_DEFAULT, T_CHANGE | T_TIME, bin_id(m, BIN_ACT));
        ok &= p.add_device_process_data("Kijuttatott", d(DDI::ApplicationTotalMass_kg), ID_VP_KG,
                                        P_DEFAULT | P_SETTABLE, T_TOTAL, bin_id(m, BIN_TOTAL));
        ok &= p.add_device_process_data("Osszes kijuttatott", d(DDI::LifetimeApplicationTotalMass), ID_VP_KG, P_DEFAULT,
                                        T_TOTAL, bin_id(m, BIN_LIFE));
    }

    // --- szakaszok (balról jobbra; Y a vetősor közepétől)
    for (int i = 0; i < g.sections; ++i) {
        char name[16];
        std::snprintf(name, sizeof(name), "Szakasz %d", i + 1);
        ok &= p.add_device_element(name, static_cast<std::uint16_t>(EN_SECTION0 + i), ID_BOOM, DEO::Type::Section,
                                   static_cast<std::uint16_t>(ID_SEC_EL + i));
        ok &= p.add_device_property("X", 0, d(DDI::DeviceElementOffsetX), ID_VP_MM, static_cast<std::uint16_t>(ID_SEC_X + i));
        ok &= p.add_device_property("Y", g.sec_y_mm[i], d(DDI::DeviceElementOffsetY), ID_VP_MM,
                                    static_cast<std::uint16_t>(ID_SEC_Y + i));
        ok &= p.add_device_property("Szelesseg", g.sec_w_mm[i], d(DDI::ActualWorkingWidth), ID_VP_M,
                                    static_cast<std::uint16_t>(ID_SEC_W + i));
    }

    // --- megjelenítések
    ok &= p.add_device_value_presentation("mm", 0, 1.0f, 0, ID_VP_MM);
    ok &= p.add_device_value_presentation("m", 0, 0.001f, 2, ID_VP_M);
    ok &= p.add_device_value_presentation("ha", 0, 0.0001f, 2, ID_VP_HA);
    ok &= p.add_device_value_presentation("perc", 0, 1.0f / 60.0f, 0, ID_VP_MIN);
    ok &= p.add_device_value_presentation("kg/ha", 0, 0.01f, 1, ID_VP_KGHA);
    ok &= p.add_device_value_presentation("ms", 0, 1.0f, 0, ID_VP_MS);
    ok &= p.add_device_value_presentation("kg", 0, 1.0f, 0, ID_VP_KG);
    ok &= p.add_device_value_presentation("km", 0, 0.000001f, 2, ID_VP_KM);
    if (!ok) return false;

    // --- hierarchia
    auto el = [&p](std::uint16_t id) { return std::static_pointer_cast<DEO>(p.get_object_by_id(id)); };
    auto dev = el(ID_DEVICE_EL), con = el(ID_CONNECTOR), boom = el(ID_BOOM);
    if (!dev || !con || !boom) return false;
    for (auto id : {ID_ACT_WORK, ID_REQ_DEFAULT, ID_TOTAL_TIME, ID_TOTAL_DIST}) dev->add_reference_to_child_object(id);
    if (s.tc_geo) dev->add_reference_to_child_object(ID_RX_STATE);
    for (auto id : {ID_CONN_X, ID_CONN_Y, ID_CONN_TYPE}) con->add_reference_to_child_object(id);
    if (g.has_pivot) con->add_reference_to_child_object(ID_CONN_PIVOT);
    for (auto id : {ID_BOOM_X, ID_BOOM_Y, ID_BOOM_Z, ID_WIDTH, ID_SET_WORK, ID_AREA, ID_OPTYPE, ID_ACT_COND})
        boom->add_reference_to_child_object(id);
    if (s.tc_sec)
        for (auto id : {ID_SC_STATE, ID_TURN_ON, ID_TURN_OFF, ID_SET_COND}) boom->add_reference_to_child_object(id);
    for (int m = 0; m < meters; ++m) {
        auto bin = el(bin_id(m, BIN_EL));
        if (!bin) return false;
        for (auto w : {BIN_SET, BIN_ACT, BIN_TOTAL, BIN_LIFE}) bin->add_reference_to_child_object(bin_id(m, w));
    }
    for (int i = 0; i < g.sections; ++i) {
        auto sec = el(static_cast<std::uint16_t>(ID_SEC_EL + i));
        if (!sec) return false;
        sec->add_reference_to_child_object(static_cast<std::uint16_t>(ID_SEC_X + i));
        sec->add_reference_to_child_object(static_cast<std::uint16_t>(ID_SEC_Y + i));
        sec->add_reference_to_child_object(static_cast<std::uint16_t>(ID_SEC_W + i));
    }
    std::vector<std::uint8_t> bin_check;
    return p.generate_binary_object_pool(bin_check);
}

// ----------------------------------------------------------------------------- kliens
TcLink::TcLink(Shared &shared, std::shared_ptr<isobus::PartneredControlFunction> tc,
               std::shared_ptr<isobus::InternalControlFunction> ecu)
    : shared_(shared), ecu_(std::move(ecu)) {
    client_ = std::make_shared<isobus::TaskControllerClient>(tc, ecu_, nullptr);
    ddop_ = std::make_shared<isobus::DeviceDescriptorObjectPool>();
}

TcLink::~TcLink() {
    if (client_) client_->terminate();
}

bool TcLink::connected() const { return client_ && client_->get_is_connected(); }

void TcLink::update(const Snapshot &snap) {
    if (!snap.set.tc_enable) return; // TC kliens kikapcsolva: nem jelentkezünk be
    if (!started_) {
        if (!build_ddop(*ddop_, snap.set, ecu_->get_NAME().get_full_name())) return;
        ddop_hash_ = seeder::ddop_relevant_hash(snap.set);
        // A képességeket a maximumra jelentjük; azt, hogy a TC-SEC/TC-GEO ténylegesen él-e,
        // a DDOP tartalma dönti el (a beállítás változásakor újratöltjük).
        client_->configure(ddop_, 1, seeder::MAX_SECTIONS, seeder::MAX_METERS, true, true, false, false, true);
        client_->add_request_value_callback(&TcLink::on_request_value, this);
        client_->add_value_command_callback(&TcLink::on_value_command, this);
        client_->initialize(false);
        started_ = true;
        ++uploads_;
    }
    const std::uint32_t h = seeder::ddop_relevant_hash(snap.set);
    if (h != ddop_hash_) {
        pending_reupload_ = true;
        ddop_hash_ = h;
    }
    if (pending_reupload_ && client_->get_is_connected()) {
        auto fresh = std::make_shared<isobus::DeviceDescriptorObjectPool>();
        if (build_ddop(*fresh, snap.set, ecu_->get_NAME().get_full_name()) &&
            client_->reupload_device_descriptor_object_pool(fresh)) {
            ddop_ = fresh;
            pending_reupload_ = false;
            ++uploads_;
        }
    }
    client_->update();

    const bool conn = client_->get_is_connected();
    if (!conn && was_connected_) {
        // a TC eltűnt: a kézi kapcsolók döntenek, a térkép előírt értéke érvénytelen
        std::lock_guard<std::mutex> g(shared_.lock);
        shared_.core.tc_clear_rate();
    }
    was_connected_ = conn;
    if (conn) {
        // OnChange értékek frissítése
        const auto &st = snap.st;
        bool on[seeder::MAX_SECTIONS];
        for (int i = 0; i < seeder::MAX_SECTIONS; ++i) on[i] = st.section_on[i];
        const std::uint32_t sec = condensed_state(on, st.section_count);
        const bool work = st.state == seeder::SowState::Sowing || st.state == seeder::SowState::Prefill;
        if (sec != last_sections_) {
            client_->on_value_changed_trigger(EN_BOOM, d(DDI::ActualCondensedWorkState1_16));
            client_->on_value_changed_trigger(EN_BOOM, d(DDI::ActualWorkingWidth));
            last_sections_ = sec;
        }
        // a kezelő a saját felületén kézire válthatja: a TC is tudja meg (a terminál ezt kijelzi)
        if (snap.set.tc_sec && st.tc_section_auto != last_sec_auto_) {
            client_->on_value_changed_trigger(EN_BOOM, d(DDI::SectionControlState));
            last_sec_auto_ = st.tc_section_auto;
        }
        if (work != last_work_) {
            client_->on_value_changed_trigger(EN_DEVICE, d(DDI::ActualWorkState));
            last_work_ = work;
        }
        for (int m = 0; m < st.meter_count && m < seeder::MAX_METERS; ++m) {
            const std::int32_t r = kgha_to_ddi(st.meter[m].actual_rate);
            if (r != last_rate_[m]) {
                client_->on_value_changed_trigger(static_cast<std::uint16_t>(EN_BIN0 + m),
                                                  d(DDI::ActualMassPerAreaApplicationRate));
                last_rate_[m] = r;
            }
        }
    }
}

bool TcLink::on_request_value(std::uint16_t element, std::uint16_t ddi, std::int32_t &value, void *self_ptr) {
    auto *self = static_cast<TcLink *>(self_ptr);
    std::lock_guard<std::mutex> g(self->shared_.lock);
    const seeder::Seeder &c = self->shared_.core;
    const auto &st = c.status();
    const auto &per = c.persistent();
    const auto &s = c.settings();
    const int bin = element >= EN_BIN0 && element < EN_BIN0 + seeder::MAX_METERS ? element - EN_BIN0 : 0;
    const bool working = st.state == seeder::SowState::Sowing || st.state == seeder::SowState::Prefill;
    value = 0;
    switch (static_cast<DDI>(ddi)) {
    case DDI::ActualWorkState: value = working ? 1 : 0; break;
    case DDI::EffectiveTotalTime: value = static_cast<std::int32_t>(per.total.work_time_s); break;
    case DDI::EffectiveTotalDistance: value = static_cast<std::int32_t>(per.total.distance_m * 1000.0); break;
    case DDI::ActualWorkingWidth: value = mm(working ? st.sown_width_m : s.width_m); break;
    case DDI::SetpointWorkState: value = st.master ? 1 : 0; break;
    case DDI::TotalArea: value = static_cast<std::int32_t>(per.total.area_m2); break;
    case DDI::SectionControlState: value = st.tc_section_auto ? 1 : 0; break;
    case DDI::ActualCondensedWorkState1_16: {
        bool on[seeder::MAX_SECTIONS];
        for (int i = 0; i < seeder::MAX_SECTIONS; ++i) on[i] = st.section_on[i];
        value = static_cast<std::int32_t>(condensed_state(on, st.section_count));
        break;
    }
    case DDI::SetpointCondensedWorkState1_16: {
        bool on[seeder::MAX_SECTIONS];
        for (int i = 0; i < seeder::MAX_SECTIONS; ++i) on[i] = st.tc_setpoint[i];
        value = static_cast<std::int32_t>(condensed_state(on, st.section_count));
        break;
    }
    case DDI::PrescriptionControlState: value = st.tc_rate ? 1 : 0; break;
    case DDI::SetpointMassPerAreaApplicationRate: value = kgha_to_ddi(st.meter[bin].target_rate); break;
    case DDI::ActualMassPerAreaApplicationRate: value = kgha_to_ddi(st.meter[bin].actual_rate); break;
    case DDI::ApplicationTotalMass_kg:
    case DDI::LifetimeApplicationTotalMass: value = static_cast<std::int32_t>(per.total.kg[bin]); break;
    default: break;
    }
    return true;
}

bool TcLink::on_value_command(std::uint16_t element, std::uint16_t ddi, std::int32_t value, void *self_ptr) {
    auto *self = static_cast<TcLink *>(self_ptr);
    std::lock_guard<std::mutex> g(self->shared_.lock);
    seeder::Seeder &c = self->shared_.core;
    const auto &s = c.settings();
    switch (static_cast<DDI>(ddi)) {
    case DDI::SetpointCondensedWorkState1_16:
        if (s.tc_sec) {
            for (int i = 0; i < seeder::MAX_SECTIONS; ++i) {
                const std::uint32_t code = (static_cast<std::uint32_t>(value) >> (2 * i)) & 3u;
                if (code <= 1u) c.tc_set_section_setpoint(i, code == 1u);
            }
        }
        break;
    case DDI::SectionControlState:
        c.tc_set_section_auto(s.tc_sec && value == 1);
        break;
    case DDI::SetpointMassPerAreaApplicationRate:
        if (s.tc_geo && element >= EN_BIN0 && element < EN_BIN0 + seeder::MAX_METERS)
            c.tc_set_rate(element - EN_BIN0, static_cast<float>(value) / 100.0f);
        break;
    case DDI::PrescriptionControlState:
        if (value == 0) c.tc_clear_rate();
        break;
    default:
        // SetpointWorkState: biztonsági okból a TC nem kapcsolhatja be a vetést (master)
        break;
    }
    return true;
}

} // namespace app

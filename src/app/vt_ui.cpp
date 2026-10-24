#include "vt_ui.hpp"

#include "seeder/vt_ids.hpp"

#include "isobus/isobus/isobus_virtual_terminal_client.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace app {
namespace {
using VT = isobus::VirtualTerminalClient;
using seeder::AlarmId;
using seeder::ParamKind;
using seeder::PARAMS;

// VT paletta indexei
constexpr std::uint8_t C_WHITE = 1, C_YELLOW = 14, C_CYAN = 11, C_LIME = 10;

enum Cmd : std::uint64_t { CMD_NUM = 1, CMD_SHOW = 2, CMD_FILL = 3, CMD_TEXT = 4, CMD_MASK = 6, CMD_SK = 7 };
constexpr std::uint64_t key(Cmd c, std::uint32_t a, std::uint32_t b = 0) {
    return (static_cast<std::uint64_t>(c) << 48) | (static_cast<std::uint64_t>(a) << 16) | b;
}

std::int32_t raw_offset(const seeder::ParamDef &d) {
    return d.min < 0.0f ? static_cast<std::int32_t>(std::lround(d.min / d.step)) : 0;
}

/// mérnöki érték → VT nyers érték (azonos a generate_pool.py raw_scale() képletével)
std::uint32_t to_raw(const seeder::ParamDef &d, float value) {
    if (d.kind == ParamKind::Enum || d.kind == ParamKind::Bool) return static_cast<std::uint32_t>(std::lround(value));
    const long raw = std::lround(value / d.step) - raw_offset(d);
    return raw < 0 ? 0u : static_cast<std::uint32_t>(raw);
}

float from_raw(const seeder::ParamDef &d, std::uint32_t raw) {
    if (d.kind == ParamKind::Enum || d.kind == ParamKind::Bool) return static_cast<float>(raw);
    return (static_cast<float>(static_cast<std::int32_t>(raw)) + static_cast<float>(raw_offset(d))) * d.step;
}

std::uint32_t custom_raw(const vt::CustomInput &c, float value) {
    const long raw = std::lround(value / c.step);
    return raw < 0 ? 0u : static_cast<std::uint32_t>(raw);
}

std::uint32_t clamp_u(double v, double hi = 4.0e9) {
    if (!(v > 0.0)) return 0;
    return v > hi ? static_cast<std::uint32_t>(hi) : static_cast<std::uint32_t>(std::lround(v));
}

bool bit(std::uint32_t mask, AlarmId id) { return (mask >> static_cast<unsigned>(id)) & 1u; }

/// Az első aktív, nem Info szintű riasztás (állapotsorhoz), vagy -1.
int first_warning(std::uint32_t active) {
    for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(AlarmId::COUNT); ++i)
        if (((active >> i) & 1u) && seeder::alarm_info(static_cast<AlarmId>(i)).severity != seeder::Severity::Info)
            return i;
    return -1;
}
} // namespace

std::string vt_latin1(const char *s, std::size_t max_chars) {
    std::string out;
    if (!s) return out;
    const auto *p = reinterpret_cast<const unsigned char *>(s);
    while (*p && out.size() < max_chars) {
        unsigned cp = *p;
        int extra = 0;
        if (cp >= 0xF0) { cp &= 0x07; extra = 3; }
        else if (cp >= 0xE0) { cp &= 0x0F; extra = 2; }
        else if (cp >= 0xC0) { cp &= 0x1F; extra = 1; }
        ++p;
        for (int i = 0; i < extra && (*p & 0xC0) == 0x80; ++i, ++p) cp = (cp << 6) | (*p & 0x3F);
        switch (cp) {
        case 0x151: cp = 0xF5; break;   // ő → õ
        case 0x150: cp = 0xD5; break;   // Ő → Õ
        case 0x171: cp = 0xFB; break;   // ű → û
        case 0x170: cp = 0xDB; break;   // Ű → Û
        case 0x2013: case 0x2014: cp = '-'; break;
        case 0x2192: cp = '>'; break;
        default: break;
        }
        out += static_cast<char>(cp <= 0xFF ? cp : '?');
    }
    return out;
}

int geo_picture(int hitch, float b_m, float c_m) {
    const bool wheels_front = c_m < b_m;
    if (hitch == 0) return 0;
    if (hitch == 1) return wheels_front ? 1 : 2;
    return wheels_front ? 3 : 4;
}

VtUi::VtUi(Shared &shared, std::shared_ptr<isobus::VirtualTerminalClient> client)
    : shared_(shared), vt_(std::move(client)) {
    page_mask_ = vt::DM_MAIN;
    vt_->get_vt_soft_key_event_dispatcher().add_listener([this](const VT::VTKeyEvent &e) {
        on_softkey(e.objectID, static_cast<std::uint8_t>(e.keyEvent));
    });
    vt_->get_vt_button_event_dispatcher().add_listener([this](const VT::VTKeyEvent &e) {
        on_button(e.objectID, static_cast<std::uint8_t>(e.keyEvent));
    });
    vt_->get_vt_change_numeric_value_event_dispatcher().add_listener(
        [this](const VT::VTChangeNumericValueEvent &e) { on_numeric(e.objectID, e.value); });
}

const char *VtUi::page_name() const {
    if (alarm_shown_) return "Riasztás";
    switch (static_cast<Page>(page_.load())) {
    case Page::Main: return "Főképernyő";
    case Page::Func: return "Funkciók";
    case Page::Info: return "Számlálók";
    case Page::Menu: return "Menü";
    case Page::Settings: return "Beállítás";
    case Page::Calib: return "Leforgatás";
    default: return "?";
    }
}

void VtUi::go(Page page, std::uint16_t mask) {
    page_ = static_cast<std::uint8_t>(page);
    switch (page) {
    case Page::Main:
    case Page::Func: page_mask_ = vt::DM_MAIN; break;
    case Page::Info: page_mask_ = vt::DM_INFO; break;
    case Page::Menu: page_mask_ = vt::DM_MENU; break;
    case Page::Calib: page_mask_ = vt::DM_CALIB; break;
    default: page_mask_ = mask; break;
    }
    if (page != Page::Settings) settings_from_calib_ = false;
}

// ----------------------------------------------------------------------------- események
void VtUi::on_softkey(std::uint16_t id, std::uint8_t activation) {
    const auto act = static_cast<VT::KeyActivationCode>(activation);
    if (act != VT::KeyActivationCode::ButtonUnlatchedOrReleased) return;
    std::lock_guard<std::mutex> g(shared_.lock);
    seeder::Seeder &c = shared_.core;
    const auto &st = c.status();

    for (const auto &pg : vt::PAGES) {
        if (id == pg.key) {
            go(Page::Settings, pg.mask);
            return;
        }
    }
    switch (id) {
    case vt::K_MASTER: c.toggle_master(); break;
    case vt::K_METER1: c.toggle_meter(0); break;
    case vt::K_METER2: c.toggle_meter(1); break;
    case vt::K_SEC_AUTO: c.toggle_section_auto(); break;
    case vt::K_PREFILL:
        if (st.prefill == seeder::PrefillState::Idle) c.prefill_start();
        else c.prefill_stop();
        break;
    case vt::K_PG_INFO: go(Page::Info); break;
    case vt::K_MODE: c.toggle_auto(); break;
    case vt::K_MENU: go(Page::Menu); break;
    case vt::K_FUNC: go(Page::Func); break;
    case vt::K_CRANK:
    case vt::K_PG_CALIB:
        c.crank_reset_count();
        go(Page::Calib);
        break;
    case vt::K_BACK: {
        const auto pg = static_cast<Page>(page_.load());
        if (pg == Page::Settings) {
            if (settings_from_calib_) go(Page::Calib);
            else go(Page::Menu);
        } else if (pg == Page::Calib) {
            c.crank_motor_stop();
            go(Page::Main);
        } else {
            go(Page::Main);
        }
        break;
    }
    case vt::K_RATE_PLUS: c.rate_step(+1); break;
    case vt::K_RATE_MINUS: c.rate_step(-1); break;
    case vt::K_RATE_RESET: c.rate_offset_reset(); break;
    case vt::K_LAMP1: c.toggle_lamp(0); break;
    case vt::K_LAMP2: c.toggle_lamp(1); break;
    case vt::K_VALVE: c.toggle_valve(); break;
    case vt::K_COOLER:
        // automata → kézi BE → kézi KI → automata
        if (!st.cooler_manual) {
            c.toggle_cooler_mode();
            if (!c.status().cooler_on) c.toggle_cooler();
        } else if (st.cooler_on) {
            c.toggle_cooler();
        } else {
            c.toggle_cooler_mode();
        }
        break;
    case vt::K_RESET_DAY: c.reset_day(); break;
    case vt::K_START: c.wheel_cal_start(); break;
    case vt::K_STOP: c.wheel_cal_stop(); break;
    case vt::K_SAVE: c.wheel_cal_save(); break;
    case vt::K_CANCEL: c.wheel_cal_cancel(); break;
    case vt::K_TARE: c.scale_tare(); break;
    case vt::K_SCALE_CAL: {
        // az ismert súly beviteli mező aktuális értéke a gyorsítótárban
        auto it = cache_.find(key(CMD_NUM, vt::VT_IN_SCALE_REF_DEF.id));
        const float kg = it != cache_.end() ? it->second * vt::VT_IN_SCALE_REF_DEF.step : 10.0f;
        c.scale_calibrate(kg);
        break;
    }
    case vt::K_PRESS_CAL: {
        auto it = cache_.find(key(CMD_NUM, vt::VT_IN_PRESS_REF_DEF.id));
        const float bar = it != cache_.end() ? it->second * vt::VT_IN_PRESS_REF_DEF.step : 50.0f;
        c.pressure_calibrate(bar);
        break;
    }
    case vt::K_CAL_METER: c.crank_set_meter(st.crank.meter == 0 ? 1 : 0); break;
    case vt::K_SCALE_TAKE: c.crank_take_scale(); break;
    case vt::K_CRANK_SAVE: c.crank_save(); break;
    case vt::K_CRANK_COPY: c.crank_copy_to_other(); break;
    case vt::K_SCALE_PAGE:
        for (const auto &pg : vt::PAGES)
            if (std::strcmp(pg.id, "scale") == 0) {
                go(Page::Settings, pg.mask);
                settings_from_calib_ = true;
            }
        break;
    case vt::K_CRANK_START: c.crank_motor_start(); break;
    case vt::K_CRANK_STOP: c.crank_motor_stop(); break;
    case vt::K_ACK:
        c.ack_alarms();
        if (alarm_shown_) {
            alarm_dismissed_ = c.status().alarms_active;
            alarm_shown_ = false;
            page_ = page_before_alarm_.load();
            page_mask_ = mask_before_alarm_.load();
        }
        break;
    default: break;
    }
}

void VtUi::on_button(std::uint16_t id, std::uint8_t activation) {
    if (static_cast<VT::KeyActivationCode>(activation) != VT::KeyActivationCode::ButtonUnlatchedOrReleased) return;
    if (id == vt::BTN_PAGE_INFO) {
        go(Page::Info);
        return;
    }
    if (id == vt::BTN_PAGE_CALIB) {
        std::lock_guard<std::mutex> g(shared_.lock);
        shared_.core.crank_reset_count();
        go(Page::Calib);
        return;
    }
    for (const auto &pg : vt::PAGES)
        if (id == pg.button) go(Page::Settings, pg.mask);
}

void VtUi::on_numeric(std::uint16_t id, std::uint32_t value) {
    std::lock_guard<std::mutex> g(shared_.lock);
    seeder::Seeder &c = shared_.core;
    // a VT már kijelzi az új értéket: a gyorsítótár is ezt tudja, így csak akkor küldünk vissza,
    // ha a vezérlő korrigálta (pl. tartományra vágás)
    remember(key(CMD_NUM, id), value);
    if (id >= vt::VT_INPUT_BASE && id < vt::VT_INPUT_BASE + seeder::PARAM_COUNT) {
        const auto pid = static_cast<std::uint16_t>(id - vt::VT_INPUT_BASE);
        c.set_param(pid, from_raw(PARAMS[pid], value));
        return;
    }
    const auto &st = c.status();
    if (id == vt::VT_IN_WCAL_DIST_DEF.id) c.wheel_cal_set_distance(value * vt::VT_IN_WCAL_DIST_DEF.step);
    else if (id == vt::VT_IN_WCAL_PULSES_DEF.id) c.wheel_cal_set_pulses(value);
    else if (id == vt::VT_IN_CAL_CROP_DEF.id) c.crank_set_crop(static_cast<int>(value));
    else if (id == vt::VT_IN_CAL_TURNS_DEF.id) c.crank_set_turns(static_cast<int>(value));
    else if (id == vt::VT_IN_CAL_RATE_DEF.id)
        c.set_param(static_cast<std::uint16_t>(st.crank.meter == 1 ? seeder::P_rate2 : seeder::P_rate1),
                    value * vt::VT_IN_CAL_RATE_DEF.step);
    else if (id == vt::VT_IN_CAL_TSCALE_DEF.id) c.crank_set_test_scale(value * vt::VT_IN_CAL_TSCALE_DEF.step);
    else if (id == vt::VT_IN_CAL_MEAS_DEF.id) c.crank_set_measured(value * vt::VT_IN_CAL_MEAS_DEF.step);
    // VT_IN_SCALE_REF / VT_IN_PRESS_REF: csak megjegyezzük, a mentés gomb használja
}

// ----------------------------------------------------------------------------- küldés
bool VtUi::cached(std::uint64_t k, std::uint32_t v) const {
    auto it = cache_.find(k);
    return it != cache_.end() && it->second == v;
}
void VtUi::remember(std::uint64_t k, std::uint32_t v) { cache_[k] = v; }

void VtUi::num(std::uint16_t id, std::int64_t value) {
    const std::uint32_t v = value < 0 ? 0u : static_cast<std::uint32_t>(value);
    const auto k = key(CMD_NUM, id);
    if (cached(k, v) || !budget_ok()) return;
    --budget_;
    if (vt_->send_change_numeric_value(id, v)) {
        remember(k, v);
        ++sent_;
    } else {
        ++failed_;
    }
}

void VtUi::shown(std::uint16_t id, bool show) {
    const auto k = key(CMD_SHOW, id);
    if (cached(k, show) || !budget_ok()) return;
    --budget_;
    if (vt_->send_hide_show_object(id, show ? VT::HideShowObjectCommand::ShowObject
                                            : VT::HideShowObjectCommand::HideObject)) {
        remember(k, show);
        ++sent_;
    } else {
        ++failed_;
    }
}

void VtUi::fill(std::uint16_t id, std::uint8_t colour) {
    const auto k = key(CMD_FILL, id);
    if (cached(k, colour) || !budget_ok()) return;
    --budget_;
    if (vt_->send_change_fill_attributes(id, VT::FillType::FillWithSpecifiedColourInFillColourAttribute, colour,
                                         0xFFFF)) {
        remember(k, colour);
        ++sent_;
    } else {
        ++failed_;
    }
}

void VtUi::text(std::uint16_t id, const std::string &value) {
    std::uint32_t h = 2166136261u;
    for (unsigned char ch : value) h = (h ^ ch) * 16777619u;
    const auto k = key(CMD_TEXT, id);
    if (cached(k, h) || !budget_ok()) return;
    --budget_;
    if (vt_->send_change_string_value(id, value)) {
        remember(k, h);
        ++sent_;
    } else {
        ++failed_;
    }
}

void VtUi::active_mask(std::uint16_t mask) {
    const auto k = key(CMD_MASK, 0);
    if (cached(k, mask) || !budget_ok()) return;
    --budget_;
    if (vt_->send_change_active_mask(vt::WORKING_SET, mask)) {
        remember(k, mask);
        ++sent_;
    } else {
        ++failed_;
    }
}

void VtUi::softkeys(std::uint16_t mask, std::uint16_t sk) {
    const auto k = key(CMD_SK, mask);
    if (cached(k, sk) || !budget_ok()) return;
    --budget_;
    if (vt_->send_change_softkey_mask(VT::MaskType::DataMask, mask, sk)) {
        remember(k, sk);
        ++sent_;
    } else {
        ++failed_;
    }
}

// ----------------------------------------------------------------------------- frissítés
void VtUi::update(const Snapshot &s) {
    const bool connected = vt_->get_is_connected();
    if (!connected) {
        was_connected_ = false;
        return;
    }
    if (!was_connected_) {
        // új kapcsolat (vagy újrakapcsolódás): a pool a kezdőértékekkel töltődött be,
        // minden értéket újra kell küldeni
        cache_.clear();
        remember(key(CMD_MASK, 0), vt::DM_MAIN);
        was_connected_ = true;
    }
    budget_ = 30;

    // parancs-visszajelzés (pl. elutasítás oka) néhány másodpercig
    if (s.st.message_seq != last_msg_seq_) {
        last_msg_seq_ = s.st.message_seq;
        msg_timer_ = (s.st.message && *s.st.message) ? 4.0f : 0.0f;
        if (msg_timer_ > 0.0f) text(vt::STR_MSG_DYN, vt_latin1(s.st.message, 25));
    } else if (msg_timer_ > 0.0f) {
        msg_timer_ -= 0.2f;
    }

    // riasztás felugró maszk: új (nem nyugtázott) Alarm szintű riasztásra
    const bool new_alarm = s.st.top_alarm != AlarmId::COUNT &&
                           ((s.st.alarms_unacked & ~alarm_dismissed_.load()) != 0u);
    if (new_alarm && !alarm_shown_) {
        page_before_alarm_ = page_.load();
        mask_before_alarm_ = page_mask_.load();
        alarm_shown_ = true;
    } else if (alarm_shown_ && s.st.top_alarm == AlarmId::COUNT) {
        alarm_shown_ = false; // a hiba megszűnt
        page_ = page_before_alarm_.load();
        page_mask_ = mask_before_alarm_.load();
    }
    alarm_dismissed_ = alarm_dismissed_.load() & s.st.alarms_active;
    if (alarm_shown_) {
        num(vt::OP_ALARM_TEXT, vt::STR_ALARMBIG[static_cast<int>(s.st.top_alarm)]);
        active_mask(vt::AM_ALARM);
    } else {
        active_mask(page_mask_.load() ? page_mask_.load() : vt::DM_MAIN);
    }

    // az éppen látható oldal elemei előre, a többi a maradék keretből
    const auto pg = static_cast<Page>(page_.load());
    if (pg == Page::Info) update_info(s);
    if (pg == Page::Calib) update_calib(s);
    if (pg == Page::Settings) {
        update_inputs(s);
        update_pages(s);
    }
    update_main(s);
    update_calib(s);
    update_info(s);
    update_inputs(s);
    update_pages(s);
}

void VtUi::update_main(const Snapshot &s) {
    const auto &st = s.st;
    const auto &set = s.set;
    const int meters = st.meter_count;
    const bool elec = seeder::electric_drive(set);
    const auto pg = static_cast<Page>(page_.load());

    // softkey-ek
    if (pg == Page::Func) {
        const int bits = (set.lights_enable ? 1 : 0) | (set.valve_enable ? 2 : 0) | (set.temp_enable ? 4 : 0);
        softkeys(vt::DM_MAIN, vt::SK_FUNC[bits]);
    } else {
        softkeys(vt::DM_MAIN, vt::SK_MAIN[meters >= 2 ? 1 : 0][elec ? 1 : 0]);
    }
    num(vt::OP_K_MASTER, vt::PIC_K_MASTER[st.master ? 1 : 0]);
    num(vt::OP_K_METER1, vt::PIC_K_METER1[st.meter[0].enabled ? 1 : 0]);
    num(vt::OP_K_METER2, vt::PIC_K_METER2[st.meter[1].enabled ? 1 : 0]);
    num(vt::OP_K_SEC, vt::PIC_K_SEC[st.tc_section_auto ? 1 : 0]);
    num(vt::OP_K_MODE, vt::PIC_K_MODE[st.manual ? 1 : 0]);
    num(vt::OP_K_PREFILL, vt::PIC_K_PREFILL[st.prefill != seeder::PrefillState::Idle ? 1 : 0]);
    num(vt::OP_K_LAMP1, vt::PIC_K_LAMP1[st.lamp[0] ? 1 : 0]);
    num(vt::OP_K_LAMP2, vt::PIC_K_LAMP2[st.lamp[1] ? 1 : 0]);
    num(vt::OP_K_VALVE, vt::PIC_K_VALVE[st.valve_marker ? 1 : 0]);
    num(vt::OP_K_COOLER, vt::PIC_K_COOLER[!st.cooler_manual ? 0 : (st.cooler_on ? 1 : 2)]);

    // tartály(ok): egy tartálynál mindkét fél az 1. érzékelőt mutatja
    const bool empty_l = st.hopper_empty[0];
    const bool empty_r = st.tank_count >= 2 ? st.hopper_empty[1] : st.hopper_empty[0];
    num(vt::OP_TANK_L, vt::PIC_TANK_L[empty_l ? 0 : 1]);
    num(vt::OP_TANK_R, vt::PIC_TANK_R[empty_r ? 0 : 1]);

    // adagolók, munkahelyzet
    num(vt::OP_METER1, vt::PIC_METER[static_cast<int>(st.meter[0].view)]);
    num(vt::OP_METER2, vt::PIC_METER[static_cast<int>(st.meter[1].view)]);
    shown(vt::CONT_METER2, meters >= 2);
    shown(vt::CONT_RATE2, meters >= 2);
    const bool prefilling = st.prefill == seeder::PrefillState::Filling;
    num(vt::OP_WORK, prefilling ? vt::PIC_M_PREFILL_76 : vt::PIC_WORK[st.work_down ? 1 : 0]);
    num(vt::OUT_ACT1, std::lround(st.meter[0].actual_rate * 10.0f));
    num(vt::OUT_TGT1, std::lround(st.meter[0].target_rate * 10.0f));
    num(vt::OUT_ACT2, std::lround(st.meter[1].actual_rate * 10.0f));
    num(vt::OUT_TGT2, std::lround(st.meter[1].target_rate * 10.0f));
    // kézi módban a % mező a motorfordulatot mutatja (a "fordulat 100% PWM-nél" %-ában)
    num(vt::OUT_OFFSET, std::lround(st.manual ? st.manual_pct : st.rate_offset_pct) + 100);
    shown(vt::CONT_PREFILL, st.prefill != seeder::PrefillState::Idle);
    num(vt::OUT_PREFILL, std::lround(std::max(0.0f, st.prefill_left_s) * 10.0f));

    // állapotsor
    std::uint16_t msg = vt::STR_MSG_OFF;
    std::uint8_t colour = C_WHITE;
    const int warn = first_warning(st.alarms_active);
    if (msg_timer_ > 0.0f) {
        msg = vt::STR_MSG_DYN;
        colour = C_YELLOW;
    } else if (st.crank.motor_running) {
        msg = vt::STR_MSG_CRANK;
        colour = C_CYAN;
    } else if (st.prefill == seeder::PrefillState::Filling) {
        msg = vt::STR_MSG_PREFILL;
        colour = C_CYAN;
    } else if (st.prefill == seeder::PrefillState::AfterRun) {
        msg = vt::STR_MSG_START;
        colour = C_LIME;
    } else if (warn >= 0 && st.master) {
        msg = vt::STR_ALARM[warn];
        colour = C_YELLOW;
    } else {
        switch (st.state) {
        case seeder::SowState::Off: msg = vt::STR_MSG_OFF; break;
        case seeder::SowState::Raised: msg = vt::STR_MSG_RAISED; break;
        case seeder::SowState::Standing: msg = vt::STR_MSG_STANDING; break;
        case seeder::SowState::Sowing:
            msg = st.manual ? vt::STR_MSG_MANUAL : vt::STR_MSG_SOWING;
            colour = st.manual ? C_CYAN : C_LIME;
            break;
        case seeder::SowState::Prefill: msg = vt::STR_MSG_PREFILL; colour = C_CYAN; break;
        case seeder::SowState::Alarm: msg = warn >= 0 ? vt::STR_ALARM[warn] : vt::STR_MSG_ALARM; colour = C_YELLOW; break;
        }
        if (warn >= 0 && st.state == seeder::SowState::Off) {
            msg = vt::STR_ALARM[warn];
            colour = C_YELLOW;
        }
    }
    num(vt::OP_MSG, msg);
    fill(vt::FL_MSG, colour);
    num(vt::OUT_AREA_DAY, clamp_u(s.per.day.area_m2 / 100.0));

    // turbina, sebesség
    const bool fan_bad = bit(st.alarms_active, AlarmId::FanLow) || bit(st.alarms_active, AlarmId::FanHigh);
    num(vt::OP_FAN, vt::PIC_FAN[fan_bad ? 1 : 0]);
    num(vt::OUT_FAN_RPM, std::lround(st.fan_rpm));
    num(vt::OUT_SPEED, std::lround(st.speed_kmh * 10.0f));

    // kiegészítők sora
    shown(vt::CONT_PRESSURE, set.pressure_enable != 0);
    num(vt::OUT_PRESSURE, st.pressure_ok ? std::lround(std::max(0.0f, st.pressure_bar)) : 0);
    shown(vt::CONT_TEMP, set.temp_enable != 0);
    shown(vt::CONT_COOLER, set.temp_enable != 0);
    float t = -300.0f;
    for (int i = 0; i < 2; ++i)
        if (st.temp_ok[i]) t = std::max(t, st.temp_c[i]);
    num(vt::OUT_TEMP, t > -100.0f ? std::lround(std::max(0.0f, t)) : 0);
    num(vt::OP_COOLER, vt::PIC_COOLER_S[st.cooler_on ? 1 : 0]);
    shown(vt::CONT_LAMPS, set.lights_enable != 0);
    num(vt::OP_LAMP1, vt::PIC_LAMP_S[st.lamp[0] ? 1 : 0]);
    num(vt::OP_LAMP2, vt::PIC_LAMP_S[st.lamp[1] ? 1 : 0]);
    shown(vt::CONT_VALVE, set.valve_enable != 0);
    num(vt::OP_VALVE, vt::PIC_VALVE_S[st.valve_marker ? 1 : 0]);
    num(vt::OP_SEC, set.tc_enable ? vt::PIC_SEC_S[st.tc_section_auto ? 1 : 0] : 0xFFFF);
}

void VtUi::update_info(const Snapshot &s) {
    const auto &d = s.per.day, &t = s.per.total;
    num(vt::OUT_DAY_AREA, clamp_u(d.area_m2 / 100.0));        // ha × 100
    num(vt::OUT_TOT_AREA, clamp_u(t.area_m2 / 100.0));
    num(vt::OUT_DAY_DIST, clamp_u(d.distance_m / 10.0));      // km × 100
    num(vt::OUT_TOT_DIST, clamp_u(t.distance_m / 10.0));
    num(vt::OUT_DAY_TIME, clamp_u(d.work_time_s / 360.0));    // óra × 10
    num(vt::OUT_TOT_TIME, clamp_u(t.work_time_s / 360.0));
    num(vt::OUT_DAY_KG1, clamp_u(d.kg[0]));
    num(vt::OUT_TOT_KG1, clamp_u(t.kg[0]));
    num(vt::OUT_DAY_KG2, clamp_u(d.kg[1]));
    num(vt::OUT_TOT_KG2, clamp_u(t.kg[1]));
}

void VtUi::update_inputs(const Snapshot &s) {
    for (std::uint16_t i = 0; i < seeder::PARAM_COUNT; ++i) {
        const auto &d = PARAMS[i];
        if (!d.vt_input_id) continue;
        num(d.vt_input_id, to_raw(d, seeder::param_get(s.set, i)));
    }
    const auto &st = s.st;
    num(vt::VT_IN_WCAL_DIST_DEF.id, custom_raw(vt::VT_IN_WCAL_DIST_DEF, st.wheel_cal.distance_m));
    num(vt::VT_IN_WCAL_PULSES_DEF.id, st.wheel_cal.pulses);
}

void VtUi::update_pages(const Snapshot &s) {
    const auto &st = s.st;
    const auto &set = s.set;
    // sebesség
    num(vt::OUT_SPEED2, std::lround(st.speed_kmh * 10.0f));
    num(vt::OUT_WCAL_RES, std::lround(st.wheel_cal_ppm * 10.0f));
    num(vt::OP_WCAL_STATE, vt::STR_CAL[static_cast<int>(st.wheel_cal.state)]);
    // turbina
    num(vt::OUT_FAN_RPM2, std::lround(st.fan_rpm));
    // kapcsolók
    num(vt::OP_WORK_CONTACT, st.work_contact ? vt::STR_CLOSED : vt::STR_OPEN);
    num(vt::OP_WORK_STATE, st.work_down ? vt::STR_DOWN : vt::STR_UP);
    num(vt::OP_HOP1_CONTACT, st.hopper_contact[0] ? vt::STR_CLOSED : vt::STR_OPEN);
    num(vt::OP_HOP1_STATE, st.hopper_empty[0] ? vt::STR_EMPTY : vt::STR_FULL);
    num(vt::OP_HOP2_CONTACT, st.hopper_contact[1] ? vt::STR_CLOSED : vt::STR_OPEN);
    num(vt::OP_HOP2_STATE, st.hopper_empty[1] ? vt::STR_EMPTY : vt::STR_FULL);
    // nyomás
    num(vt::OUT_PRESS_V, std::lround(std::max(0.0f, st.pressure_v) * 1000.0f));
    num(vt::OUT_PRESSURE2, std::lround(std::max(0.0f, st.pressure_bar) * 10.0f));
    // olajhőmérséklet (nyers = °C×10 + 1000)
    num(vt::OUT_TEMP1, st.temp_ok[0] ? std::lround(st.temp_c[0] * 10.0f) + 1000 : 0);
    num(vt::OUT_TEMP2, st.temp_ok[1] ? std::lround(st.temp_c[1] * 10.0f) + 1000 : 0);
    num(vt::OP_COOLER_STATE, st.cooler_on ? vt::STR_ON : vt::STR_OFF);
    num(vt::OP_COOLER_MODE, st.cooler_manual ? vt::STR_MANUAL : vt::STR_AUTO);
    // mérleg (nyers: g + 100000, ill. érték + 2^23)
    num(vt::OUT_SCALE_KG, std::lround(st.scale.net_kg * 1000.0f) + 100000);
    num(vt::OUT_SCALE_RAW, std::lround(st.scale.raw) + 8388608);
    // TC, geometria
    num(vt::OP_TC_STATE, !set.tc_enable ? vt::STR_TC_DIS : (st.tc_connected ? vt::STR_TC_ON : vt::STR_TC_OFF));
    num(vt::OP_TC_MODE, st.tc_section_auto ? vt::STR_AUTO : vt::STR_MANUAL);
    num(vt::OP_GEO, vt::PIC_GEO[geo_picture(set.hitch_type, set.geo_b_m, set.geo_c_m)]);
    // diagnosztika
    num(vt::OP_DIAG_CL1, s.out.clutch_drive[0] ? vt::STR_ON : vt::STR_OFF);
    num(vt::OP_DIAG_CL2, s.out.clutch_drive[1] ? vt::STR_ON : vt::STR_OFF);
    num(vt::OUT_DIAG_PWM1, std::lround(s.out.motor_pwm_pct[0] * 10.0f));
    num(vt::OUT_DIAG_PWM2, std::lround(s.out.motor_pwm_pct[1] * 10.0f));
    num(vt::OUT_DIAG_RPM1, std::lround(st.meter[0].rpm * 10.0f));
    num(vt::OUT_DIAG_RPM2, std::lround(st.meter[1].rpm * 10.0f));
    num(vt::OUT_DIAG_WHEELHZ, std::lround(st.wheel_hz * 10.0f));
    num(vt::OUT_DIAG_FANHZ, std::lround(st.fan_rpm * static_cast<float>(std::max<int>(1, set.fan_ppr)) / 6.0f));
}

void VtUi::update_calib(const Snapshot &s) {
    const auto &st = s.st;
    const auto &set = s.set;
    const auto &cr = st.crank;
    const auto &r = st.crank_result;
    const bool elec = seeder::electric_drive(set);
    softkeys(vt::DM_CALIB, elec ? vt::SK_CALIB : vt::SK_CALIB_GROUND);
    num(vt::OP_K_CAL_METER, vt::PIC_K_CAL_METER[cr.meter == 1 ? 1 : 0]);
    num(vt::OP_CAL_METER, vt::STR_METER[cr.meter == 1 ? 1 : 0]);
    num(vt::VT_IN_CAL_CROP_DEF.id, cr.crop);
    num(vt::VT_IN_CAL_TURNS_DEF.id, cr.turn_option);
    num(vt::VT_IN_CAL_RATE_DEF.id, custom_raw(vt::VT_IN_CAL_RATE_DEF, seeder::rate_setting(set, cr.meter)));
    num(vt::VT_IN_CAL_TSCALE_DEF.id, custom_raw(vt::VT_IN_CAL_TSCALE_DEF, cr.test_scale));
    num(vt::VT_IN_CAL_MEAS_DEF.id, custom_raw(vt::VT_IN_CAL_MEAS_DEF, cr.measured_kg));
    num(vt::OUT_CAL_SCALE, std::lround(r.scale * 10.0));
    const int setup = r.flap == 'A' ? 0 : (r.drive == seeder::chart::DRIVE_MIKRO ? 2 : 1);
    num(vt::OP_CAL_SETUP, vt::STR_SETUP[setup]);
    num(vt::OUT_CAL_EXPECT, std::lround(r.expected_kg * 1000.0));
    num(vt::OUT_CAL_NEW, r.corrected_valid ? std::lround(r.corrected_scale * 10.0) : 0);
    const float ppr = static_cast<float>(std::max<int>(1, set.meter_ppr));
    const float revs = elec ? cr.motor_revs_done : static_cast<float>(cr.meter_pulses) / ppr;
    num(vt::OUT_CAL_REVS, std::lround(revs * 10.0f));
    num(vt::OUT_CAL_KGREV, std::lround((r.kg_per_rev > 0.0 ? r.kg_per_rev : seeder::cal_kg_rev(set, cr.meter)) * 10000.0));
    const auto note = seeder::chart::note(r, cr.measured_kg > 0.0f);
    num(vt::OP_CAL_NOTE, msg_timer_ > 0.0f ? vt::STR_MSG_DYN : vt::STR_NOTE[static_cast<int>(note)]);
}

} // namespace app

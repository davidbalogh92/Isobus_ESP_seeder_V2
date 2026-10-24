#pragma once
// VT kezelőfelület: az object pool objektumainak frissítése és a kezelői események feldolgozása.
#include "seeder_app.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

namespace app {

/// UTF-8 → a VT Latin-1 fontja (ő→õ, ű→û, ismeretlen → '?'), legfeljebb max_chars karakter.
std::string vt_latin1(const char *utf8, std::size_t max_chars);

/// Geometria oldal vázlata (PIC_GEO index) a csatlakozás és a távolságok szerint.
int geo_picture(int hitch, float b_m, float c_m);

class VtUi {
public:
    VtUi(Shared &shared, std::shared_ptr<isobus::VirtualTerminalClient> client);

    /// Kijelzés frissítése a pillanatkép alapján (kb. 200 ms-onként hívandó).
    void update(const Snapshot &snap);

    unsigned sent() const { return sent_; }
    unsigned failed() const { return failed_; }
    const char *page_name() const;

private:
    enum class Page : std::uint8_t { Main, Func, Info, Menu, Settings, Calib };

    void on_softkey(std::uint16_t object, std::uint8_t activation);
    void on_button(std::uint16_t object, std::uint8_t activation);
    void on_numeric(std::uint16_t object, std::uint32_t value);
    void go(Page page, std::uint16_t mask = 0);

    // gyorsítótárazott küldés: csak változáskor, sikertelen küldésnél a következő ciklus újrapróbálja
    bool budget_ok() const { return budget_ > 0; }
    void num(std::uint16_t id, std::int64_t value);
    void shown(std::uint16_t id, bool show);
    void fill(std::uint16_t fill_id, std::uint8_t colour);
    void text(std::uint16_t id, const std::string &value);
    void active_mask(std::uint16_t mask);
    void softkeys(std::uint16_t mask, std::uint16_t sk);
    bool cached(std::uint64_t key, std::uint32_t value) const;
    void remember(std::uint64_t key, std::uint32_t value);

    void update_main(const Snapshot &s);
    void update_info(const Snapshot &s);
    void update_inputs(const Snapshot &s);
    void update_pages(const Snapshot &s);
    void update_calib(const Snapshot &s);

    Shared &shared_;
    std::shared_ptr<isobus::VirtualTerminalClient> vt_;
    std::unordered_map<std::uint64_t, std::uint32_t> cache_;
    bool was_connected_ = false;
    int budget_ = 0;
    unsigned sent_ = 0, failed_ = 0;
    std::uint32_t last_msg_seq_ = 0;
    float msg_timer_ = 0.0f;       ///< visszajelzés megjelenítési ideje (s)

    // A VT eseményszál csak ezeket állítja; minden VT parancsot az update() küld.
    std::atomic<std::uint8_t> page_{static_cast<std::uint8_t>(Page::Main)};
    std::atomic<std::uint16_t> page_mask_{0};
    std::atomic<bool> settings_from_calib_{false};
    std::atomic<std::uint8_t> page_before_alarm_{static_cast<std::uint8_t>(Page::Main)};
    std::atomic<std::uint16_t> mask_before_alarm_{0};
    std::atomic<bool> alarm_shown_{false};
    std::atomic<std::uint32_t> alarm_dismissed_{0}; ///< a nyugtázáskor aktív riasztások
};

} // namespace app

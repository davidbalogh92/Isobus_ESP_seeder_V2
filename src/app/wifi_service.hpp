#pragma once
// WiFi szerviz mód (külső kijelző spec, „WiFi szerviz mód” fejezet).
//
// A vezérlő WiFi hozzáférési pontja alapból KI van kapcsolva, és csak igény szerint indul:
//  * bekapcsolás: VT menü, külső kijelző (0x40), web (Etherneten át), vagy vészmegoldásként a
//    leforgató gomb (IN8) lenyomva tartása bekapcsoláskor;
//  * kikapcsolás: kézzel, vagy a tétlenségi idő (alap 20 perc, 5–60) leteltével;
//  * nem kapcsol le magától, amíg kliens csatlakozik vagy OTA fut;
//  * aktív munkakapcsolónál nem kapcsolható be, és ha bekapcsolt állapotban aktiválódik,
//    azonnal lekapcsol (OTA alatt nem).
// A webszerver Etherneten (AgOpenGPS hálózat, Windows-program) a WiFi-től függetlenül fut.
//
// Hordozható logika: a rádió tényleges ki-/bekapcsolását a beállított driver végzi (ESP: esp_wifi_start/stop).
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>

namespace app {

class WifiService {
public:
    enum class Source : std::uint8_t { Vt, Display, Web, Input };
    enum class Result : std::uint8_t { Ok, InhibitedWork, BusyOta };
    /// állapot a kijelző 0xC0 üzenetének kódjaival
    enum State : std::uint8_t { Off = 0, On = 1, Client = 2, Ota = 3 };

    /// a rádió kapcsolása (true: be). Hívás a service zárján kívül.
    void set_driver(std::function<void(bool)> d);

    /// ki/be kérés. Bekapcsolás aktív munkakapcsolónál tiltva; kikapcsolás OTA alatt foglalt.
    Result request(bool on, Source src);
    Result toggle(Source src);

    /// ciklusonként: munkakapcsoló aktív (gép lent ÉS van munkakapcsoló), csatlakozott kliensek, OTA fut,
    /// tétlenségi időkorlát percben (5–60).
    void tick(float dt_s, bool work_active, int clients, bool ota_running, int timeout_min);

    bool on() const;
    State state() const;
    std::uint16_t minutes_left() const;   ///< felfelé kerekítve; KI állapotban 0
    std::string json() const;
    static const char *source_name(Source s);

private:
    void apply(bool on);   // zár alatt hívva: a driver hívását a zár után végezzük (pending_)

    mutable std::mutex mx_;
    std::function<void(bool)> driver_;
    bool on_ = false;
    bool work_active_ = false;
    int clients_ = 0;
    bool ota_ = false;
    int timeout_min_ = 20;
    float idle_left_s_ = 0.0f;
    Source last_src_ = Source::Vt;
    const char *last_off_reason_ = "";
    unsigned switches_ = 0;
    int pending_ = -1;   // -1: nincs, 0/1: a driver hívandó
};

} // namespace app

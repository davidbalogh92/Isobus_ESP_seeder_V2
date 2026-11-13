#pragma once
// Külső kijelző (Isobus_ExtDisplay_esp32) kapcsolat – V2 oldal, 1. kör.
// Specifikáció: docs/ext_display_spec.md; protokoll: protocol/ext_display_protocol.h (változatlan másolat,
// minden üzenet az ottani extdisp_encode_* / extdisp_decode_* függvényekkel).
//
// Átvitel: Proprietary A (PGN 0xEF00), célzottan a kijelző címére, 8 bájt. A kijelzőt a SeederApp
// NAME-szűrővel (ipari csoport 2, funkciókód 135, gyártókód 0) partnerként találja meg.
//
// Ez az osztály hordozható (CAN nélkül tesztelhető):
//  * on_frame(): bármely szálból (ISOBUS stack callback) – csak eltárol, nem küld és nem hív kifelé;
//  * tick(): a vezérlési ciklusból, a mag zárja NÉLKÜL – végrehajtja a sorban álló parancsokat a
//    handler függvényekkel (azok maguk zárolnak), nyugtáz, és küldi az időzített állapotokat.
// A kijelző hiánya semmit nem befolyásol: partner nélkül a tick() nem küld semmit.
#include "ext_display_protocol.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>

namespace app {

/// Egy ciklus gépállapota a kijelzőnek (a SeederApp tölti ki a pillanatképből).
struct ExtDisplayMachine {
    extdisp_capabilities_t caps{};
    std::uint8_t flags = 0;            ///< EXTDISP_ST_FLAG_*
    std::uint16_t speed_ckmh = 0;
    std::uint8_t wifi_state = EXTDISP_WIFI_OFF;
    std::uint16_t wifi_minutes = 0;
    std::uint8_t tank_n = 0;           ///< küldendő tartály-állapotok száma (0..2)
    extdisp_tank_status_t tanks[2]{};
};

/// Nyugta eredménye (EXTDISP_ACK_*, EXTDISP_INHIBIT_*)
struct ExtAck {
    std::uint8_t result = EXTDISP_ACK_OK;
    std::uint8_t reason = EXTDISP_INHIBIT_NONE;
};

class ExtDisplay {
public:
    using Send = std::function<bool(const std::uint8_t *frame)>;   ///< 8 bájt a kijelző címére
    struct Handlers {
        std::function<ExtAck(const extdisp_light_command_t &)> light;
        std::function<ExtAck(const extdisp_wifi_command_t &)> wifi;
        std::function<ExtAck(const extdisp_tank_command_t &)> tank;
    };

    void set_sender(Send s) { send_ = std::move(s); }
    void set_handlers(Handlers h) { handlers_ = std::move(h); }

    /// Bejövő Proprietary A keret a kijelzőtől (bármely szálból).
    void on_frame(const std::uint8_t *data, std::size_t len);

    /// Vezérlési ciklus. link_ok: saját és partner cím érvényes.
    void tick(std::uint32_t now_ms, bool link_ok, const ExtDisplayMachine &m);

    // --- a kijelző szívveréséből (1 s időkorláttal)
    bool connected() const;                 ///< 1 s-on belül jött szívverés
    bool stop_held() const;                 ///< STOP gomb (csak élő kapcsolatnál)
    bool start_held() const;
    std::uint8_t screen() const;
    /// a STOP lenyomások (felfutó élek) száma – a leforgatás állapotgépe (2. kör) ebből dolgozik
    unsigned stop_edges() const;

    std::string json() const;

private:
    struct Rx {
        std::uint8_t data[EXTDISP_FRAME_LEN];
        std::size_t len;
    };
    void process(const Rx &rx);
    bool send(const std::uint8_t *frame);
    void send_ack(std::uint8_t id, const ExtAck &a);

    Send send_;
    Handlers handlers_;

    mutable std::mutex mx_;
    std::deque<Rx> queue_;                  ///< parancsok (nem szívverés)
    std::uint32_t now_ = 0;                 ///< a legutóbbi tick ideje (a fogadás időbélyege)
    std::uint32_t last_hb_ms_ = 0;
    bool hb_seen_ = false;
    extdisp_heartbeat_t hb_{};
    unsigned stop_edges_ = 0;
    bool prev_stop_ = false;
    unsigned rx_ = 0, rx_bad_ = 0, hb_rx_ = 0, cmd_rx_ = 0, dropped_ = 0;

    // csak a tick() szálán
    bool link_prev_ = false;
    std::uint32_t last_caps_ = 0, last_status_ = 0, last_wifi_ = 0, last_tank_ = 0;
    std::uint8_t counter_ = 0;
    std::uint8_t wifi_prev_ = EXTDISP_WIFI_OFF;
    unsigned tx_ = 0, tx_fail_ = 0, acks_ = 0, caps_sent_ = 0;
    std::uint8_t last_cmd_ = 0;
    ExtAck last_ack_{};
};

} // namespace app

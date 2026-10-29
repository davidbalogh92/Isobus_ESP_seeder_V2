#pragma once
// AgOpenGPS gépmodul-protokoll (UDP). Hordozható, hálózatfüggetlen rész: a beérkező datagramokat
// feldolgozza, a kimenőket egy küldő függvényen át adja ki. A socketet az ESP réteg kezeli.
//
// Keret: 0x80 0x81 <forrás> <PGN> <hossz> <adat...> <CK>, CK = a 2. bájttól az utolsó előttiig vett
// összeg alsó bájtja. Az AgIO a 8888-as portra küld (forrás 0x7F), a modulok a 9999-es portra
// válaszolnak (szórás az alhálózaton: x.x.x.255).
//
// Fogadott:
//   PGN 254 (0xFE) autokormány-adat: sebesség (5–6. bájt, km/h × 10) – ebből a GPS-sebesség
//   PGN 239 (0xEF) gépadat: szakaszok 1–8 (11. bájt), 9–16 (12. bájt), sebesség (6. bájt, km/h × 10)
//   PGN 200 (0xC8) AgIO "hello" → válasz PGN 123 (gépmodul hello)
//   PGN 202 (0xCA) hálózatkeresés → válasz PGN 203 (IP-cím + alhálózat)
//   PGN 201 (0xC9) alhálózat beállítása (5–6. bájt = 201, 201; 7–9. bájt = az új alhálózat)
// Küldött:
//   PGN 234 (0xEA) kapcsolóvezérlés (forrás 0x77): főkapcsoló (munkakapcsoló), automata / kézi BE / KI
//   szakaszok – ezzel fest az AgOpenGPS (zöld = automata, sárga = kézi). A kormánymodul PGN 253-át
//   NEM küldjük, a meglévő autokormány-modulhoz nem nyúlunk.
#include <cstddef>
#include <cstdint>
#include <functional>

namespace seeder {
namespace aog {

constexpr std::uint16_t PORT_FROM_AGIO = 8888;   ///< ide hallgatunk
constexpr std::uint16_t PORT_TO_AGIO = 9999;     ///< ide küldünk (szórás)

std::uint8_t checksum(const std::uint8_t *frame, std::size_t len);
/// Érvényes AgOpenGPS keret (fejléc, hossz, ellenőrzőösszeg)?
bool valid_frame(const std::uint8_t *d, std::size_t n);

/// A festés / munkakapcsoló állapota, amit az AgOpenGPS felé jelzünk.
struct PaintState {
    bool enabled = false;        ///< küldjünk-e egyáltalán
    bool work = false;           ///< gép lent és dolgozik (főkapcsoló BE)
    int mode = 0;                ///< 0 automata / zöld, 1 kézi / sárga
    int sections = 1;            ///< a vetőgép szakaszainak száma (1–16)
    std::uint16_t on_mask = 0;   ///< kézi módban: mely szakaszok vetnek
};

struct LinkStats {
    std::uint32_t rx_packets = 0, rx_rejected = 0, rx_254 = 0, rx_239 = 0, rx_hello = 0, rx_scan = 0;
    std::uint32_t tx_paint = 0, tx_failed = 0, tx_hello = 0, tx_scan = 0;
    std::uint8_t last_pgn = 0;
    std::uint32_t max_gap_ms = 0;  ///< leghosszabb szünet két AgIO csomag között
    std::uint32_t outages = 0;     ///< kapcsolatkiesések száma
};

class Link {
public:
    /// Küldés a PORT_TO_AGIO portra, szórással. Igazat ad, ha sikerült.
    using Sender = std::function<bool(const std::uint8_t *data, std::size_t len)>;

    /// A saját IP-cím (a PGN 203 válaszhoz).
    void set_own_ip(const std::uint8_t ip[4]);
    /// Beérkezett datagram feldolgozása.
    void on_packet(const std::uint8_t *d, std::size_t n, std::uint32_t now_ms, const Sender &send);
    /// Periodikus hívás (pl. 20 ms): festés küldése.
    void update(std::uint32_t now_ms, const PaintState &paint, const Sender &send);

    bool connected(std::uint32_t now_ms) const;
    bool speed_valid(std::uint32_t now_ms) const;
    float speed_kmh() const { return speed_kmh_; }
    std::uint32_t speed_age_ms(std::uint32_t now_ms) const;
    bool sections_valid(std::uint32_t now_ms) const;
    std::uint16_t sections() const { return sections_; }
    /// Az AgIO új alhálózatot kért (PGN 201): igaz egyszer, az első három bájttal.
    bool take_subnet_request(std::uint8_t out[3]);
    const LinkStats &stats() const { return stats_; }
    std::uint8_t last_main() const { return last_main_; }

    static constexpr std::uint32_t TIMEOUT_MS = 2000;
    static constexpr std::uint32_t SPEED_TIMEOUT_MS = 1500;

private:
    bool send_paint(const PaintState &p, std::uint32_t now_ms, const Sender &send);

    std::uint8_t ip_[4] = {192, 168, 5, 130};
    std::uint32_t last_rx_ms_ = 0;
    bool any_rx_ = false;
    bool was_connected_ = false;
    std::uint32_t last_254_ms_ = 0, last_239_ms_ = 0;
    bool have_254_ = false, have_239_ = false;
    float speed_kmh_ = 0.0f;
    float speed_239_kmh_ = 0.0f;
    std::uint16_t sections_ = 0;
    bool subnet_pending_ = false;
    std::uint8_t subnet_[3] = {0, 0, 0};
    LinkStats stats_;
    // festés
    bool have_state_ = false;
    bool last_work_ = false;
    std::uint16_t last_on_mask_ = 0;
    int last_mode_ = -1;
    int last_sections_ = -1;
    int burst_left_ = 0;
    std::uint32_t next_burst_ms_ = 0;
    std::uint32_t last_tx_ms_ = 0;
    std::uint8_t last_main_ = 0;
};

} // namespace aog
} // namespace seeder

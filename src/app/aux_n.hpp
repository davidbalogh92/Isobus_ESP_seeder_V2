#pragma once
// AUX-N (ISO 11783-6, Auxiliary Control Type 2): joystick / gombdoboz gombjai a vetőgép funkcióihoz.
// A terminál „AUX hozzárendelés” menüjében a gépkezelő köti a gombokat a pool AUX funkcióihoz
// (tools/pool/generate_pool.py: AUX_FUNCS). Itt:
//  * a bejövő gombállapotokból (ismétlődő státuszüzenetek) élt képzünk, és a funkcióval azonos
//    softkey műveletét hívjuk (VtUi::on_softkey) – így a viselkedés a VT gombjával egyezik;
//  * a hozzárendelések tárolása (NVS blob) eszközönként (NAME + modellkód).
// Hordozható, CAN / AgIsoStack nélkül tesztelhető.
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace app {

struct AuxAssignment {
    std::uint16_t function = 0;   ///< a mi poolunk AUX funkció objektuma
    std::uint16_t input = 0;      ///< az AUX eszköz bemenet-objektuma
    std::uint8_t type = 0;        ///< funkciótípus (AuxiliaryTypeTwoFunctionType)
};

struct AuxDevice {
    std::uint64_t name = 0;       ///< az AUX eszköz ISO NAME-je
    std::uint16_t model = 0;      ///< modell-azonosító
    std::vector<AuxAssignment> list;
};

/// Bináris formátum: "AX1" | eszközszám | (NAME u64, modell u16, db u8, (fn u16, in u16, típus u8)*)*
std::vector<std::uint8_t> aux_serialize(const std::vector<AuxDevice> &devs);
bool aux_deserialize(const std::uint8_t *p, std::size_t n, std::vector<AuxDevice> &out);

class AuxN {
public:
    /// softkey művelet: (softkey ID, aktiválás: 0 elengedve, 1 lenyomva, 2 tartva)
    using KeyAction = std::function<void(std::uint16_t, std::uint8_t)>;
    using LoadBlob = std::function<std::vector<std::uint8_t>()>;
    using SaveBlob = std::function<bool(const std::vector<std::uint8_t> &)>;

    explicit AuxN(KeyAction action) : action_(std::move(action)) {}

    void set_storage(LoadBlob load, SaveBlob save);

    /// Bejövő gombállapot (value1: 0 elengedve, 1 lenyomva, 2 nyomva tartva; egyéb: hiba/ismeretlen).
    /// @return true, ha a funkció a miénk
    bool on_input(std::uint16_t function, std::uint16_t value1);

    /// Hozzárendelés betöltése / mentése (a VT kliens hívja).
    std::vector<AuxAssignment> load(std::uint64_t name, std::uint16_t model);
    void store(std::uint64_t name, std::uint16_t model, const std::vector<AuxAssignment> &list);
    void failed(unsigned reason);

    /// Diagnosztika (JSON objektum).
    std::string json() const;

private:
    void ensure_loaded();

    KeyAction action_;
    LoadBlob load_blob_;
    SaveBlob save_blob_;
    mutable std::mutex mx_;
    bool loaded_ = false;
    std::vector<AuxDevice> devs_;
    std::map<std::uint16_t, std::uint16_t> last_;   ///< funkció → utolsó value1
    unsigned events_ = 0, actions_ = 0, stores_ = 0, failures_ = 0;
    unsigned last_failure_ = 0;
    std::uint16_t last_fn_ = 0, last_val_ = 0xFFFF;
};

} // namespace app

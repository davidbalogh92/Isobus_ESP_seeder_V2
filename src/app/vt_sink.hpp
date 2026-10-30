#pragma once
// A VT kezelőfelület (VtUi) kimenete: hová mennek a "VT parancsok".
//  * RealVtSink    – az ISOBUS terminál (AgIsoStack VirtualTerminalClient)
//  * VirtualScreen – a vezérlőben tárolt virtuális képernyő: a Windows-program (vagy bármely más kliens)
//                    a weben át olvassa (/api/vt), és ugyanazt a poolt rajzolja ki (/api/pool).
// Így a terminál és a Windows-program ugyanazt a felületet, ugyanazzal a logikával mutatja.
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

namespace app {

class VtSink {
public:
    virtual ~VtSink() = default;
    virtual bool connected() const = 0;
    virtual bool numeric(std::uint16_t id, std::uint32_t value) = 0;
    virtual bool hide_show(std::uint16_t id, bool show) = 0;
    virtual bool fill_colour(std::uint16_t fill_id, std::uint8_t colour) = 0;
    virtual bool string_value(std::uint16_t id, const std::string &latin1) = 0;
    virtual bool active_mask(std::uint16_t mask) = 0;
    virtual bool softkey_mask(std::uint16_t data_mask, std::uint16_t softkey_mask) = 0;
    /// a kezelő által a terminálon beírt érték (a valódi VT már mutatja; a virtuálisnak tárolnia kell)
    virtual void input_echo(std::uint16_t, std::uint32_t) {}
    /// ciklusonként legfeljebb ennyi parancs (CAN terhelés); a virtuálisnál nincs korlát
    virtual int budget() const { return 30; }
};

/// A vezérlőben tárolt virtuális VT képernyő (szálbiztos).
class VirtualScreen final : public VtSink {
public:
    explicit VirtualScreen(std::uint16_t initial_mask) : mask_(initial_mask) {}

    bool connected() const override { return true; }
    bool numeric(std::uint16_t id, std::uint32_t value) override;
    bool hide_show(std::uint16_t id, bool show) override;
    bool fill_colour(std::uint16_t fill_id, std::uint8_t colour) override;
    bool string_value(std::uint16_t id, const std::string &latin1) override;
    bool active_mask(std::uint16_t mask) override;
    bool softkey_mask(std::uint16_t data_mask, std::uint16_t softkey_mask) override;
    void input_echo(std::uint16_t id, std::uint32_t value) override { numeric(id, value); }
    int budget() const override { return 100000; }

    /// {"ver":N,"pool":"...","mask":id,"sk":{..},"num":{..},"hid":{..},"fill":{..},"str":{..}}
    std::string json(const char *pool_version) const;
    std::uint32_t version() const;

private:
    mutable std::mutex mx_;
    std::uint32_t ver_ = 1;
    std::uint16_t mask_;
    std::map<std::uint16_t, std::uint32_t> num_;
    std::map<std::uint16_t, bool> shown_;
    std::map<std::uint16_t, std::uint8_t> fill_;
    std::map<std::uint16_t, std::string> str_;
    std::map<std::uint16_t, std::uint16_t> sk_;
};

} // namespace app

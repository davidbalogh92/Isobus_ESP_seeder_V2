#include "aux_n.hpp"

#include "seeder/vt_ids.hpp"

#include <cstdio>

namespace app {
namespace {
constexpr std::size_t MAX_DEVICES = 4;   // ennyi különböző AUX eszköz hozzárendelését őrizzük

void put16(std::vector<std::uint8_t> &o, std::uint16_t v) {
    o.push_back(static_cast<std::uint8_t>(v));
    o.push_back(static_cast<std::uint8_t>(v >> 8));
}
std::uint16_t get16(const std::uint8_t *p) { return static_cast<std::uint16_t>(p[0] | (p[1] << 8)); }

const vt::AuxFunction *find_fn(std::uint16_t id) {
    for (const auto &f : vt::AUX_FUNCTIONS)
        if (f.id == id) return &f;
    return nullptr;
}
} // namespace

std::vector<std::uint8_t> aux_serialize(const std::vector<AuxDevice> &devs) {
    std::vector<std::uint8_t> o = {'A', 'X', '1', static_cast<std::uint8_t>(devs.size())};
    for (const auto &d : devs) {
        for (int i = 0; i < 8; ++i) o.push_back(static_cast<std::uint8_t>(d.name >> (8 * i)));
        put16(o, d.model);
        const std::size_t n = d.list.size() > 255 ? 255 : d.list.size();
        o.push_back(static_cast<std::uint8_t>(n));
        for (std::size_t k = 0; k < n; ++k) {
            put16(o, d.list[k].function);
            put16(o, d.list[k].input);
            o.push_back(d.list[k].type);
        }
    }
    return o;
}

bool aux_deserialize(const std::uint8_t *p, std::size_t n, std::vector<AuxDevice> &out) {
    out.clear();
    if (!p || n < 4 || p[0] != 'A' || p[1] != 'X' || p[2] != '1') return false;
    std::size_t i = 4;
    for (unsigned d = 0; d < p[3]; ++d) {
        if (i + 11 > n) return false;
        AuxDevice dev;
        for (int b = 0; b < 8; ++b) dev.name |= static_cast<std::uint64_t>(p[i + b]) << (8 * b);
        dev.model = get16(p + i + 8);
        const unsigned cnt = p[i + 10];
        i += 11;
        if (i + 5u * cnt > n) return false;
        for (unsigned k = 0; k < cnt; ++k, i += 5) dev.list.push_back({get16(p + i), get16(p + i + 2), p[i + 4]});
        out.push_back(std::move(dev));
    }
    return i == n;
}

void AuxN::set_storage(LoadBlob load, SaveBlob save) {
    std::lock_guard<std::mutex> g(mx_);
    load_blob_ = std::move(load);
    save_blob_ = std::move(save);
    loaded_ = false;
}

void AuxN::ensure_loaded() {
    if (loaded_) return;
    loaded_ = true;
    devs_.clear();
    if (!load_blob_) return;
    const auto b = load_blob_();
    if (!b.empty() && !aux_deserialize(b.data(), b.size(), devs_)) devs_.clear();
}

bool AuxN::on_input(std::uint16_t function, std::uint16_t value1) {
    const vt::AuxFunction *f = find_fn(function);
    if (!f) return false;
    std::uint16_t prev;
    {
        std::lock_guard<std::mutex> g(mx_);
        ++events_;
        last_fn_ = function;
        last_val_ = value1;
        auto it = last_.find(function);
        prev = it == last_.end() ? 0xFFFF : it->second;
        last_[function] = value1;
    }
    const bool down = value1 == 1 || value1 == 2;
    const bool was_down = prev == 1 || prev == 2;
    std::uint8_t act = 0xFF;
    if (f->softkey == vt::K_CRANK_START) {
        // nyomva tartásos: minden "nyomva" üzenet frissít (a VT gombhoz hasonlóan), elengedéskor leáll
        if (down) act = 2;
        else if (was_down && value1 == 0) act = 0;
        else if (was_down) act = 3;   // hibás / ismeretlen érték: biztonsági leállítás
    } else if (value1 == 1 && !was_down) {
        // a többi: a lenyomás pillanatában egyszer (a softkey elengedés-művelete)
        act = 0;
    }
    if (act == 0xFF) return true;
    {
        std::lock_guard<std::mutex> g(mx_);
        ++actions_;
    }
    if (action_) action_(f->softkey, act);
    return true;
}

std::vector<AuxAssignment> AuxN::load(std::uint64_t name, std::uint16_t model) {
    std::lock_guard<std::mutex> g(mx_);
    ensure_loaded();
    for (const auto &d : devs_)
        if (d.name == name && d.model == model) {
            std::vector<AuxAssignment> r;
            for (const auto &a : d.list)
                if (find_fn(a.function)) r.push_back(a);   // régi poolból maradt funkció kimarad
            return r;
        }
    return {};
}

void AuxN::store(std::uint64_t name, std::uint16_t model, const std::vector<AuxAssignment> &list) {
    std::lock_guard<std::mutex> g(mx_);
    ensure_loaded();
    AuxDevice dev{name, model, list};
    for (auto it = devs_.begin(); it != devs_.end(); ++it)
        if (it->name == name && it->model == model) {
            devs_.erase(it);
            break;
        }
    devs_.insert(devs_.begin(), dev);   // a legutóbb használt elöl
    if (devs_.size() > MAX_DEVICES) devs_.resize(MAX_DEVICES);
    ++stores_;
    if (save_blob_) save_blob_(aux_serialize(devs_));
}

void AuxN::failed(unsigned reason) {
    std::lock_guard<std::mutex> g(mx_);
    ++failures_;
    last_failure_ = reason;
}

std::string AuxN::json() const {
    std::lock_guard<std::mutex> g(mx_);
    unsigned assigned = 0;
    for (const auto &d : devs_) assigned += static_cast<unsigned>(d.list.size());
    char b[256];
    std::snprintf(b, sizeof b,
                  "{\"functions\":%u,\"devices\":%u,\"assigned\":%u,\"events\":%u,\"actions\":%u,\"stores\":%u,"
                  "\"failures\":%u,\"last_failure\":%u,\"last_fn\":%u,\"last_value\":%d}",
                  static_cast<unsigned>(sizeof(vt::AUX_FUNCTIONS) / sizeof(vt::AUX_FUNCTIONS[0])),
                  static_cast<unsigned>(devs_.size()), assigned, events_, actions_, stores_, failures_, last_failure_,
                  static_cast<unsigned>(last_fn_), last_val_ == 0xFFFF ? -1 : static_cast<int>(last_val_));
    return b;
}

} // namespace app

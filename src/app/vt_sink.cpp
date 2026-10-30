#include "vt_sink.hpp"

#include <cstdio>

namespace app {

bool VirtualScreen::numeric(std::uint16_t id, std::uint32_t value) {
    std::lock_guard<std::mutex> g(mx_);
    auto it = num_.find(id);
    if (it == num_.end() || it->second != value) {
        num_[id] = value;
        ++ver_;
    }
    return true;
}

bool VirtualScreen::hide_show(std::uint16_t id, bool show) {
    std::lock_guard<std::mutex> g(mx_);
    auto it = shown_.find(id);
    if (it == shown_.end() || it->second != show) {
        shown_[id] = show;
        ++ver_;
    }
    return true;
}

bool VirtualScreen::fill_colour(std::uint16_t fill_id, std::uint8_t colour) {
    std::lock_guard<std::mutex> g(mx_);
    auto it = fill_.find(fill_id);
    if (it == fill_.end() || it->second != colour) {
        fill_[fill_id] = colour;
        ++ver_;
    }
    return true;
}

bool VirtualScreen::string_value(std::uint16_t id, const std::string &latin1) {
    std::lock_guard<std::mutex> g(mx_);
    auto it = str_.find(id);
    if (it == str_.end() || it->second != latin1) {
        str_[id] = latin1;
        ++ver_;
    }
    return true;
}

bool VirtualScreen::active_mask(std::uint16_t mask) {
    std::lock_guard<std::mutex> g(mx_);
    if (mask_ != mask) {
        mask_ = mask;
        ++ver_;
    }
    return true;
}

bool VirtualScreen::softkey_mask(std::uint16_t data_mask, std::uint16_t softkey_mask) {
    std::lock_guard<std::mutex> g(mx_);
    auto it = sk_.find(data_mask);
    if (it == sk_.end() || it->second != softkey_mask) {
        sk_[data_mask] = softkey_mask;
        ++ver_;
    }
    return true;
}

std::uint32_t VirtualScreen::version() const {
    std::lock_guard<std::mutex> g(mx_);
    return ver_;
}

namespace {
/// Latin-1 → UTF-8 JSON szöveg (idézőjelek, vezérlőkarakterek escape-elve).
void append_latin1_json(std::string &o, const std::string &s) {
    o += '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') {
            o += '\\';
            o += static_cast<char>(c);
        } else if (c < 0x20) {
            char b[8];
            std::snprintf(b, sizeof(b), "\\u%04x", c);
            o += b;
        } else if (c < 0x80) {
            o += static_cast<char>(c);
        } else {
            o += static_cast<char>(0xC0 | (c >> 6));
            o += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    o += '"';
}
} // namespace

std::string VirtualScreen::json(const char *pool_version) const {
    std::lock_guard<std::mutex> g(mx_);
    std::string o;
    o.reserve(4096 + 24 * num_.size() + 32 * str_.size());
    char b[48];
    std::snprintf(b, sizeof(b), "{\"ver\":%lu,\"pool\":\"", static_cast<unsigned long>(ver_));
    o += b;
    o += pool_version;
    std::snprintf(b, sizeof(b), "\",\"mask\":%u,\"sk\":{", static_cast<unsigned>(mask_));
    o += b;
    bool first = true;
    for (const auto &kv : sk_) {
        std::snprintf(b, sizeof(b), "%s\"%u\":%u", first ? "" : ",", kv.first, kv.second);
        o += b;
        first = false;
    }
    o += "},\"num\":{";
    first = true;
    for (const auto &kv : num_) {
        std::snprintf(b, sizeof(b), "%s\"%u\":%lu", first ? "" : ",", kv.first, static_cast<unsigned long>(kv.second));
        o += b;
        first = false;
    }
    o += "},\"hid\":{";
    first = true;
    for (const auto &kv : shown_) {
        std::snprintf(b, sizeof(b), "%s\"%u\":%d", first ? "" : ",", kv.first, kv.second ? 0 : 1);
        o += b;
        first = false;
    }
    o += "},\"fill\":{";
    first = true;
    for (const auto &kv : fill_) {
        std::snprintf(b, sizeof(b), "%s\"%u\":%u", first ? "" : ",", kv.first, kv.second);
        o += b;
        first = false;
    }
    o += "},\"str\":{";
    first = true;
    for (const auto &kv : str_) {
        std::snprintf(b, sizeof(b), "%s\"%u\":", first ? "" : ",", kv.first);
        o += b;
        append_latin1_json(o, kv.second);
        first = false;
    }
    o += "}}";
    return o;
}

} // namespace app

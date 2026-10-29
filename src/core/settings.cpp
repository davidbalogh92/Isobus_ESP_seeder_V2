#define SEEDER_PARAMS_IMPLEMENTATION
#include "seeder/settings.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace seeder {
namespace {

float *fptr(Settings &s, const ParamDef &d) {
    return reinterpret_cast<float *>(reinterpret_cast<std::uint8_t *>(&s) + d.offset);
}
const float *fptr(const Settings &s, const ParamDef &d) {
    return reinterpret_cast<const float *>(reinterpret_cast<const std::uint8_t *>(&s) + d.offset);
}
std::int32_t *iptr(Settings &s, const ParamDef &d) {
    return reinterpret_cast<std::int32_t *>(reinterpret_cast<std::uint8_t *>(&s) + d.offset);
}
const std::int32_t *iptr(const Settings &s, const ParamDef &d) {
    return reinterpret_cast<const std::int32_t *>(reinterpret_cast<const std::uint8_t *>(&s) + d.offset);
}
char *sptr(Settings &s, const ParamDef &d) {
    return reinterpret_cast<char *>(&s) + d.offset;
}
const char *sptr(const Settings &s, const ParamDef &d) {
    return reinterpret_cast<const char *>(&s) + d.offset;
}

float clampf(float v, float lo, float hi) {
    if (!(v == v)) return lo; // NaN
    return v < lo ? lo : (v > hi ? hi : v);
}

float quantize(float v, float step) {
    if (step <= 0.0f) return v;
    return std::round(v / step) * step;
}

void append_number(std::string &out, double v, int decimals) {
    char buf[48];
    if (decimals < 0) decimals = 0;
    if (decimals > 6) decimals = 6;
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    out += buf;
}

} // namespace

std::string json_escape(const char *text) {
    std::string r;
    if (!text) return r;
    for (const unsigned char *p = reinterpret_cast<const unsigned char *>(text); *p; ++p) {
        switch (*p) {
        case '"': r += "\\\""; break;
        case '\\': r += "\\\\"; break;
        case '\n': r += "\\n"; break;
        case '\r': r += "\\r"; break;
        case '\t': r += "\\t"; break;
        default:
            if (*p < 0x20) r += ' ';
            else r += static_cast<char>(*p);
        }
    }
    return r;
}

Settings default_settings() {
    Settings s;
    std::memset(&s, 0, sizeof(s));
    for (std::uint16_t i = 0; i < PARAM_COUNT; ++i) {
        const ParamDef &d = PARAMS[i];
        switch (d.kind) {
        case ParamKind::Float: *fptr(s, d) = d.def; break;
        case ParamKind::Str: std::snprintf(sptr(s, d), static_cast<std::size_t>(d.max) + 1, "%s", d.def_str); break;
        default: *iptr(s, d) = static_cast<std::int32_t>(d.def); break;
        }
    }
    normalize(s);
    return s;
}

int param_find(const char *key) {
    if (!key) return -1;
    for (std::uint16_t i = 0; i < PARAM_COUNT; ++i)
        if (std::strcmp(PARAMS[i].key, key) == 0) return i;
    return -1;
}

float param_get(const Settings &s, std::uint16_t id) {
    if (id >= PARAM_COUNT) return 0.0f;
    const ParamDef &d = PARAMS[id];
    switch (d.kind) {
    case ParamKind::Float: return *fptr(s, d);
    case ParamKind::Str: return 0.0f;
    default: return static_cast<float>(*iptr(s, d));
    }
}

const char *param_get_str(const Settings &s, std::uint16_t id) {
    if (id >= PARAM_COUNT || PARAMS[id].kind != ParamKind::Str) return "";
    return sptr(s, PARAMS[id]);
}

bool param_set(Settings &s, std::uint16_t id, float value) {
    if (id >= PARAM_COUNT) return false;
    const ParamDef &d = PARAMS[id];
    switch (d.kind) {
    case ParamKind::Float:
        *fptr(s, d) = clampf(quantize(value, d.step), d.min, d.max);
        return true;
    case ParamKind::Int:
        *iptr(s, d) = static_cast<std::int32_t>(std::lround(clampf(quantize(value, d.step), d.min, d.max)));
        return true;
    case ParamKind::Enum:
    case ParamKind::Bool: {
        long v = std::lround(value);
        if (v < 0 || v >= d.option_count) return false;
        *iptr(s, d) = static_cast<std::int32_t>(v);
        return true;
    }
    case ParamKind::Str: return false;
    }
    return false;
}

bool param_set_str(Settings &s, std::uint16_t id, const char *value) {
    if (id >= PARAM_COUNT || !value) return false;
    const ParamDef &d = PARAMS[id];
    if (d.kind != ParamKind::Str) return false;
    const std::size_t len = std::strlen(value);
    if (len < static_cast<std::size_t>(d.min) || len > static_cast<std::size_t>(d.max)) return false;
    for (std::size_t i = 0; i < len; ++i)
        if (static_cast<unsigned char>(value[i]) < 0x20) return false;
    std::snprintf(sptr(s, d), static_cast<std::size_t>(d.max) + 1, "%s", value);
    return true;
}

int meter_count(const Settings &s) { return s.meter_count >= 2 ? 2 : 1; }
int tank_count(const Settings &s) { return s.tank_count >= 2 ? 2 : 1; }
int section_count(const Settings &s) { return s.section_count >= 2 ? 2 : 1; }
bool electric_drive(const Settings &s) { return s.drive_type == 1; }

float section_width(const Settings &s, int section) {
    if (section_count(s) == 1) return section == 0 ? s.width_m : 0.0f;
    return section == 0 ? s.sec_w1 : s.width_m - s.sec_w1;
}

float meter_width(const Settings &s, int meter) {
    if (meter < 0 || meter >= meter_count(s)) return 0.0f;
    if (meter_count(s) == 1 || s.meter_layout == 1) return s.width_m;
    return meter == 0 ? s.sec_w1 : s.width_m - s.sec_w1;
}

bool parse_ipv4(const char *text, std::uint8_t out[4]) {
    if (!text) return false;
    const char *p = text;
    for (int i = 0; i < 4; ++i) {
        if (*p < '0' || *p > '9') return false;
        int v = 0, digits = 0;
        while (*p >= '0' && *p <= '9') {
            v = v * 10 + (*p++ - '0');
            if (++digits > 3 || v > 255) return false;
        }
        out[i] = static_cast<std::uint8_t>(v);
        if (i < 3) {
            if (*p != '.') return false;
            ++p;
        }
    }
    return *p == '\0';
}

float rate_setting(const Settings &s, int meter) { return meter == 1 ? s.rate2 : s.rate1; }
float cal_kg_rev(const Settings &s, int meter) { return meter == 1 ? s.cal2_kg_rev : s.cal1_kg_rev; }

void normalize(Settings &s, int changed_id) {
    // 1) tartományok
    for (std::uint16_t i = 0; i < PARAM_COUNT; ++i) {
        const ParamDef &d = PARAMS[i];
        if (d.kind == ParamKind::Str) {
            char *p = sptr(s, d);
            p[static_cast<std::size_t>(d.max)] = '\0';
            if (std::strlen(p) < static_cast<std::size_t>(d.min))
                std::snprintf(p, static_cast<std::size_t>(d.max) + 1, "%s", d.def_str);
            continue;
        }
        if (d.kind == ParamKind::Float) {
            float &v = *fptr(s, d);
            v = clampf(v, d.min, d.max);
        } else {
            std::int32_t &v = *iptr(s, d);
            const std::int32_t hi = (d.kind == ParamKind::Int) ? static_cast<std::int32_t>(d.max)
                                                               : static_cast<std::int32_t>(d.option_count) - 1;
            const std::int32_t lo = (d.kind == ParamKind::Int) ? static_cast<std::int32_t>(d.min) : 0;
            if (v < lo || v > hi) v = static_cast<std::int32_t>(d.def);
        }
    }
    // IP-cím formátum (hibás → alapérték)
    {
        std::uint8_t ip[4];
        if (!parse_ipv4(s.aog_ip, ip)) std::snprintf(s.aog_ip, sizeof(s.aog_ip), "%s", PARAMS[P_aog_ip].def_str);
    }
    // 2) adagoló / szakasz összefüggések: 1 adagolónál és első/hátsó soros gépnél csak 1 szakasz lehet
    if (s.meter_count < 2 || s.meter_layout == 1) s.section_count = 1;
    // 3) szakasz-/adagolószélesség: munkaszélesség változásakor felezünk
    if (changed_id == P_width_m) s.sec_w1 = std::round(s.width_m * 50.0f) / 100.0f;
    const float max_w1 = s.width_m - 0.1f;
    if (s.sec_w1 > max_w1) s.sec_w1 = max_w1;
    if (s.sec_w1 < 0.1f) s.sec_w1 = 0.1f;
    // 4) PWM min <= max
    if (s.pwm_min_pct > s.pwm_max_pct) {
        if (changed_id == P_pwm_max_pct) s.pwm_min_pct = s.pwm_max_pct;
        else s.pwm_max_pct = s.pwm_min_pct;
    }
    // 5) hűtő hiszterézis: kikapcsolás legalább 1 °C-kal a bekapcsolás alatt
    if (s.cooler_off_c > s.cooler_on_c - 1.0f) {
        if (changed_id == P_cooler_off_c) s.cooler_on_c = s.cooler_off_c + 1.0f;
        else s.cooler_off_c = s.cooler_on_c - 1.0f;
    }
    // 6) turbina: a max ne legyen a min alatt (0 = nincs felső riasztás)
    if (s.fan_max_rpm != 0 && s.fan_max_rpm <= s.fan_min_rpm) {
        if (changed_id == P_fan_min_rpm) s.fan_max_rpm = 0;
        else s.fan_max_rpm = s.fan_min_rpm + 10;
    }
}

bool apply_param(Settings &s, std::uint16_t id, float value) {
    if (id >= PARAM_COUNT) return false;
    const Settings before = s;
    if (!param_set(s, id, value)) return false;
    normalize(s, id);
    return std::memcmp(&before, &s, sizeof(Settings)) != 0;
}

std::string settings_to_json(const Settings &s) {
    std::string out = "{";
    for (std::uint16_t i = 0; i < PARAM_COUNT; ++i) {
        const ParamDef &d = PARAMS[i];
        if (i) out += ',';
        out += '"';
        out += d.key;
        out += "\":";
        if (d.kind == ParamKind::Str) {
            out += '"';
            out += json_escape(param_get_str(s, i));
            out += '"';
        } else if (d.kind == ParamKind::Float) {
            append_number(out, param_get(s, i), d.decimals + 1);
        } else {
            append_number(out, param_get(s, i), 0);
        }
    }
    out += '}';
    return out;
}

// ------------------------------------------------------------ minimális lapos JSON olvasó
namespace {
struct Reader {
    const char *p;
    void ws() {
        while (*p && std::isspace(static_cast<unsigned char>(*p))) ++p;
    }
    bool str(std::string &out) {
        ws();
        if (*p != '"') return false;
        ++p;
        out.clear();
        while (*p && *p != '"') {
            if (*p == '\\') {
                ++p;
                switch (*p) {
                case 'n': out += '\n'; break;
                case 't': out += '\t'; break;
                case 'r': out += '\r'; break;
                case 'u': {
                    // \uXXXX: csak ASCII/Latin tartomány, UTF-8-ra alakítva
                    unsigned v = 0;
                    for (int k = 1; k <= 4; ++k) {
                        char c = p[k];
                        v <<= 4;
                        if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
                        else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
                        else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
                        else return false;
                    }
                    p += 4;
                    if (v < 0x80) out += static_cast<char>(v);
                    else if (v < 0x800) {
                        out += static_cast<char>(0xC0 | (v >> 6));
                        out += static_cast<char>(0x80 | (v & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (v >> 12));
                        out += static_cast<char>(0x80 | ((v >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (v & 0x3F));
                    }
                    break;
                }
                case '\0': return false;
                default: out += *p; break;
                }
                ++p;
            } else {
                out += *p++;
            }
        }
        if (*p != '"') return false;
        ++p;
        return true;
    }
};
} // namespace

int settings_from_json(Settings &s, const char *json, std::string *error) {
    if (!json) return -1;
    Reader r{json};
    r.ws();
    if (*r.p != '{') {
        if (error) *error = "JSON objektum kell";
        return -1;
    }
    ++r.p;
    Settings work = s;
    int applied = 0;
    std::string key, sval;
    for (;;) {
        r.ws();
        if (*r.p == '}') { ++r.p; break; }
        if (!r.str(key)) { if (error) *error = "hibás kulcs"; return -1; }
        r.ws();
        if (*r.p != ':') { if (error) *error = "hiányzó ':'"; return -1; }
        ++r.p;
        r.ws();
        const int id = param_find(key.c_str());
        if (*r.p == '"') {
            if (!r.str(sval)) { if (error) *error = "hibás szöveg"; return -1; }
            if (id >= 0) {
                if (PARAMS[id].kind == ParamKind::Str) {
                    if (!param_set_str(work, static_cast<std::uint16_t>(id), sval.c_str())) {
                        if (error) *error = std::string("érvénytelen érték: ") + key;
                        return -1;
                    }
                } else {
                    char *end = nullptr;
                    const double v = std::strtod(sval.c_str(), &end);
                    if (end == sval.c_str() || !param_set(work, static_cast<std::uint16_t>(id), static_cast<float>(v))) {
                        if (error) *error = std::string("érvénytelen érték: ") + key;
                        return -1;
                    }
                }
                ++applied;
            }
        } else if (std::strncmp(r.p, "true", 4) == 0 || std::strncmp(r.p, "false", 5) == 0) {
            const bool v = (*r.p == 't');
            r.p += v ? 4 : 5;
            if (id >= 0) {
                if (!param_set(work, static_cast<std::uint16_t>(id), v ? 1.0f : 0.0f)) return -1;
                ++applied;
            }
        } else if (std::strncmp(r.p, "null", 4) == 0) {
            r.p += 4;
        } else {
            char *end = nullptr;
            const double v = std::strtod(r.p, &end);
            if (end == r.p) { if (error) *error = std::string("hibás szám: ") + key; return -1; }
            r.p = end;
            if (id >= 0) {
                if (!param_set(work, static_cast<std::uint16_t>(id), static_cast<float>(v))) {
                    if (error) *error = std::string("érvénytelen érték: ") + key;
                    return -1;
                }
                ++applied;
            }
        }
        r.ws();
        if (*r.p == ',') { ++r.p; continue; }
        if (*r.p == '}') { ++r.p; break; }
        if (error) *error = "hiányzó ',' vagy '}'";
        return -1;
    }
    normalize(work, -1);
    s = work;
    return applied;
}

std::string schema_json() {
    std::string out = "{\"groups\":[";
    for (std::size_t g = 0; g < GROUP_COUNT; ++g) {
        if (g) out += ',';
        out += "{\"id\":\"";
        out += GROUPS[g].id;
        out += "\",\"label\":\"";
        out += json_escape(GROUPS[g].label);
        out += "\",\"icon\":\"";
        out += GROUPS[g].icon;
        out += "\"}";
    }
    out += "],\"params\":[";
    for (std::uint16_t i = 0; i < PARAM_COUNT; ++i) {
        const ParamDef &d = PARAMS[i];
        if (i) out += ',';
        out += "{\"key\":\"";
        out += d.key;
        out += "\",\"group\":\"";
        out += d.group;
        out += "\",\"label\":\"";
        out += json_escape(d.label);
        out += "\",\"unit\":\"";
        out += json_escape(d.unit);
        out += "\",\"help\":\"";
        out += json_escape(d.help);
        out += "\",\"feature\":\"";
        out += d.feature;
        out += "\",\"scope\":\"";
        out += d.scope;
        out += "\",\"kind\":\"";
        static const char *const kinds[] = {"float", "int", "enum", "bool", "str"};
        out += kinds[static_cast<int>(d.kind)];
        out += "\",\"min\":";
        append_number(out, d.min, 3);
        out += ",\"max\":";
        append_number(out, d.max, 3);
        out += ",\"step\":";
        append_number(out, d.step, 4);
        out += ",\"decimals\":";
        append_number(out, d.decimals, 0);
        if (d.option_count) {
            out += ",\"options\":[";
            for (std::uint8_t k = 0; k < d.option_count; ++k) {
                if (k) out += ',';
                out += '"';
                out += json_escape(d.options[k]);
                out += '"';
            }
            out += ']';
        }
        out += '}';
    }
    out += "]}";
    return out;
}

std::uint32_t ddop_relevant_hash(const Settings &s) {
    // FNV-1a a TC-t érintő mezőkön
    std::uint32_t h = 2166136261u;
    auto mix = [&h](std::int32_t v) {
        for (int i = 0; i < 4; ++i) {
            h ^= static_cast<std::uint8_t>(static_cast<std::uint32_t>(v) >> (8 * i));
            h *= 16777619u;
        }
    };
    auto mm = [](float m) { return static_cast<std::int32_t>(std::lround(m * 1000.0f)); };
    mix(mm(s.width_m));
    mix(section_count(s));
    mix(meter_count(s));
    mix(s.meter_layout);
    mix(mm(s.sec_w1));
    mix(s.hitch_type);
    mix(mm(s.geo_a_m));
    mix(mm(s.geo_b_m));
    mix(mm(s.geo_c_m));
    mix(mm(s.geo_y_m));
    mix(s.delay_on_ms);
    mix(s.delay_off_ms);
    mix(s.tc_sec);
    mix(s.tc_geo);
    mix(2); // DDOP szerkezet verziója
    return h;
}

} // namespace seeder

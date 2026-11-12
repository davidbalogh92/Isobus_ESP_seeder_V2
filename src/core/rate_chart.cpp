// Gépprofil: beépített Accord táblázat, cserélhető aktív táblázat, CSV beolvasás / kiírás.
#include "seeder/rate_chart.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace seeder {
namespace chart {
namespace {

void copy_str(char *dst, int cap, const char *src) {
    int i = 0;
    // UTF-8 határon vágunk (ne maradjon félbe vágott több bájtos karakter)
    const int n = static_cast<int>(std::strlen(src));
    int lim = n < cap - 1 ? n : cap - 1;
    while (lim > 0 && lim < n && (static_cast<unsigned char>(src[lim]) & 0xC0) == 0x80) --lim;
    for (; i < lim; ++i) dst[i] = src[i];
    dst[i] = 0;
}

void add_variant(CropDef &c, const char *label, const char *setup, const float *sc, const float *rt, int n) {
    Variant &v = c.var[c.nvar++];
    copy_str(v.label, LABEL_LEN, label);
    copy_str(v.setup, LABEL_LEN, setup);
    v.n = n;
    for (int i = 0; i < n; ++i) {
        v.scale[i] = sc[i];
        v.rate[i] = rt[i];
    }
}

// FIGYELEM: a Table ~10 kB – az ESP-n soha ne kerüljön veremre (feladatverem 4–10 kB); helyben töltjük / kupacon
void make_builtin(Table &t) {
    static const float NS[21] = {10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110};
    static const float WHEAT_R[21] = {34, 51, 69, 86, 104, 122, 140, 157, 174, 192, 210, 228, 246, 264, 281, 298, 316, 335, 352, 370, 387};
    static const float RYE_R[21] = {33, 49, 66, 83, 100, 117, 134, 151, 168, 184, 200, 217, 235, 252, 269, 286, 302, 319, 337, 354, 371};
    static const float BARLEY_R[21] = {32, 48, 64, 79, 95, 111, 127, 143, 159, 174, 190, 206, 222, 238, 253, 268, 284, 300, 316, 332, 348};
    static const float OATS_R[21] = {24, 35, 47, 59, 71, 82, 94, 106, 118, 130, 141, 153, 165, 177, 189, 200, 212, 224, 236, 248, 260};
    static const float BEANS_R[21] = {23, 42, 61, 79, 98, 116, 135, 154, 172, 191, 209, 228, 246, 265, 283, 302, 320, 338, 356, 374, 393};
    static const float PEAS_R[21] = {21, 40, 59, 78, 97, 117, 136, 155, 174, 194, 213, 232, 251, 270, 289, 309, 328, 347, 366, 385, 404};
    static const float LUPINS_R[21] = {28, 45, 62, 79, 96, 113, 130, 147, 164, 181, 198, 216, 234, 251, 268, 285, 302, 320, 337, 354, 371};
    static const float VETCH_R[21] = {32, 51, 70, 89, 108, 127, 146, 165, 184, 203, 222, 241, 260, 279, 299, 317, 336, 355, 374, 393, 412};
    static const float MAIZE_R[21] = {8, 24, 47, 70, 92, 115, 137, 156, 175, 194, 212, 231, 249, 267, 285, 304, 323, 342, 361, 380, 398};
    static const float GRASS_S[5] = {15, 20, 25, 30, 35};
    static const float GRASS_R[5] = {18, 26, 34, 42, 50};
    static const float FS[10] = {2.5f, 5, 7.5f, 10, 12.5f, 15, 17.5f, 20, 22.5f, 25};
    static const float RAPE_N[10] = {1.8f, 4.6f, 6.8f, 9.1f, 11.4f, 13.7f, 15.9f, 18.2f, 20.5f, 22.8f};
    static const float RAPE_M[10] = {0.9f, 2.3f, 3.4f, 4.55f, 5.7f, 6.85f, 7.95f, 9.1f, 10.25f, 11.4f};
    static const float CLOVER_N[10] = {2.3f, 5.3f, 8.6f, 12.0f, 15.3f, 18.0f, 21.3f, 24.0f, 26.6f, 27.5f};
    static const float CLOVER_M[10] = {1.15f, 2.65f, 4.3f, 6.0f, 7.65f, 9.0f, 10.65f, 12.0f, 13.3f, 13.75f};
    static const float GRASSF_S[8] = {7.5f, 10, 12.5f, 15, 17.5f, 20, 22.5f, 25};
    static const float GRASSF_N[8] = {2.8f, 5.2f, 7.2f, 9.2f, 11.2f, 13.2f, 15.0f, 16.2f};
    static const float GRASSF_M[8] = {1.4f, 2.6f, 3.6f, 4.6f, 5.6f, 6.6f, 7.5f, 8.1f};
    static const float TURNIP_N[10] = {2.5f, 5.0f, 7.5f, 10.0f, 12.5f, 15.0f, 17.5f, 20.0f, 21.5f, 23.0f};
    static const float TURNIP_M[10] = {1.25f, 2.5f, 3.75f, 5.0f, 6.25f, 7.5f, 8.75f, 10.0f, 10.75f, 11.5f};

    t.machine[0] = 0;
    t.ncrop = 0;
    for (auto &c : t.crop) {
        c.name[0] = 0;
        c.nvar = 0;
        for (auto &v : c.var) v.n = 0;
    }
    copy_str(t.machine, NAME_LEN, "Accord DA/DL/DT/DF (gyári)");
    t.turns10 = 85.0f;
    t.builtin = true;
    struct N { const char *name; const float *s; const float *r; int n; };
    const N normal[] = {{"Búza", NS, WHEAT_R, 21},       {"Rozs", NS, RYE_R, 21},        {"Árpa", NS, BARLEY_R, 21},
                        {"Zab", NS, OATS_R, 21},         {"Lóbab", NS, BEANS_R, 21},     {"Borsó", NS, PEAS_R, 21},
                        {"Csillagfürt", NS, LUPINS_R, 21}, {"Bükköny", NS, VETCH_R, 21}, {"Kukorica", NS, MAIZE_R, 21},
                        {"Fű", GRASS_S, GRASS_R, 5}};
    for (const auto &c : normal) {
        CropDef &cd = t.crop[t.ncrop++];
        copy_str(cd.name, NAME_LEN, c.name);
        add_variant(cd, "", "A (nyitott)", c.s, c.r, c.n);
    }
    struct F { const char *name; const float *s; const float *rn; const float *rm; int n; };
    const F fine[] = {{"Repce", FS, RAPE_N, RAPE_M, 10},
                      {"Vöröshere", FS, CLOVER_N, CLOVER_M, 10},
                      {"Fű (aprómag)", GRASSF_S, GRASSF_N, GRASSF_M, 8},
                      {"Tarlórépa", FS, TURNIP_N, TURNIP_M, 10}};
    for (const auto &c : fine) {
        CropDef &cd = t.crop[t.ncrop++];
        copy_str(cd.name, NAME_LEN, c.name);
        add_variant(cd, "N", "Z + NORMÁL", c.s, c.rn, c.n);
        add_variant(cd, "M", "Z + MIKRO", c.s, c.rm, c.n);
    }
}

Table *make_static() {
    static Table t;   // statikus tároló (nem verem)
    make_builtin(t);
    return &t;
}
Table &active() {
    static Table *t = make_static();
    return *t;
}

std::string trim(const std::string &s) {
    std::size_t a = 0, b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '"' || s[a] == '\r')) ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '"' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

bool to_num(const std::string &in, float &v) {
    std::string s = trim(in);
    for (char &c : s)
        if (c == ',') c = '.';
    if (s.empty()) return false;
    char *end = nullptr;
    const double d = std::strtod(s.c_str(), &end);
    if (!end || *end != 0 || !std::isfinite(d)) return false;
    v = static_cast<float>(d);
    return true;
}

std::vector<std::string> split(const std::string &line, char sep) {
    std::vector<std::string> f;
    std::string cur;
    bool q = false;
    for (char c : line) {
        if (c == '"') q = !q;
        else if (c == sep && !q) {
            f.push_back(trim(cur));
            cur.clear();
        } else cur += c;
    }
    f.push_back(trim(cur));
    return f;
}

std::string fmt_num(float v) {
    char b[24];
    std::snprintf(b, sizeof b, "%g", static_cast<double>(v));
    for (char *p = b; *p; ++p)
        if (*p == '.') *p = ',';
    return b;
}

void json_str(std::string &o, const char *s) {
    o += '"';
    for (const char *p = s; *p; ++p) {
        if (*p == '"' || *p == '\\') o += '\\';
        if (static_cast<unsigned char>(*p) >= 0x20) o += *p;
    }
    o += '"';
}

} // namespace

const Table &builtin_table() {
    static Table b;
    static const bool once = (make_builtin(b), true);
    (void)once;
    return b;
}
const Table &table() { return active(); }
void set_table(const Table &t) { active() = t; }
void reset_table() { make_builtin(active()); }

bool parse_csv(const std::string &csv, Table &out, std::string &err) {
    std::unique_ptr<Table> tp(new (std::nothrow) Table());
    if (!tp) {
        err = "Nincs elég memória";
        return false;
    }
    Table &t = *tp;
    t.builtin = false;
    // elválasztó: ha van pontosvessző, az; különben tabulátor vagy vessző
    char sep = ',';
    if (csv.find(';') != std::string::npos) sep = ';';
    else if (csv.find('\t') != std::string::npos) sep = '\t';
    std::size_t pos = 0;
    int line_no = 0;
    std::string text = csv;
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF)
        text.erase(0, 3);   // UTF-8 BOM (Excel)
    char buf[96];
    while (pos <= text.size()) {
        std::size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) nl = text.size();
        const std::string line = trim(text.substr(pos, nl - pos));
        pos = nl + 1;
        ++line_no;
        if (line.empty()) continue;
        if (line[0] == '#') {   // meta
            auto f = split(line.substr(1), sep);
            if (f.size() >= 2) {
                std::string k = trim(f[0]);
                for (auto &c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                if (k == "gép" || k == "gep" || k == "machine") copy_str(t.machine, NAME_LEN, f[1].c_str());
                else if (k == "fordulat_10" || k == "turns_10") {
                    float v;
                    if (!to_num(f[1], v) || v < 1.0f || v > 1000.0f) {
                        std::snprintf(buf, sizeof buf, "%d. sor: hibás fordulatszám", line_no);
                        err = buf;
                        return false;
                    }
                    t.turns10 = v;
                }
            }
            continue;
        }
        auto f = split(line, sep);
        while (!f.empty() && f.back().empty()) f.pop_back();   // Excel záró üres oszlopai
        if (f.size() < 5) {
            if (f.size() == 1 && f[0].empty()) continue;
            std::snprintf(buf, sizeof buf, "%d. sor: 5 oszlop kell (növény;változat;beállítás;skála;kg/ha)", line_no);
            err = buf;
            return false;
        }
        float sc, rt;
        const bool ns = to_num(f[3], sc), nr = to_num(f[4], rt);
        if (!ns || !nr) {
            if (t.ncrop == 0) continue;   // fejlécsor
            std::snprintf(buf, sizeof buf, "%d. sor: hibás szám", line_no);
            err = buf;
            return false;
        }
        if (f[0].empty()) {
            std::snprintf(buf, sizeof buf, "%d. sor: hiányzó növénynév", line_no);
            err = buf;
            return false;
        }
        // növény keresése / új
        int ci = -1;
        for (int i = 0; i < t.ncrop; ++i)
            if (f[0] == t.crop[i].name) ci = i;
        if (ci < 0) {
            if (t.ncrop >= MAX_CROPS) {
                std::snprintf(buf, sizeof buf, "Legfeljebb %d növény lehet", MAX_CROPS);
                err = buf;
                return false;
            }
            ci = t.ncrop++;
            copy_str(t.crop[ci].name, NAME_LEN, f[0].c_str());
        }
        CropDef &cd = t.crop[ci];
        int vi = -1;
        for (int i = 0; i < cd.nvar; ++i)
            if (f[1] == cd.var[i].label) vi = i;
        if (vi < 0) {
            if (cd.nvar >= MAX_VARIANTS) {
                std::snprintf(buf, sizeof buf, "%d. sor: egy növénynek legfeljebb %d változata lehet", line_no, MAX_VARIANTS);
                err = buf;
                return false;
            }
            vi = cd.nvar++;
            copy_str(cd.var[vi].label, LABEL_LEN, f[1].c_str());
            copy_str(cd.var[vi].setup, LABEL_LEN, f[2].empty() ? f[1].c_str() : f[2].c_str());
        }
        Variant &v = cd.var[vi];
        if (v.n >= MAX_POINTS) {
            std::snprintf(buf, sizeof buf, "%d. sor: legfeljebb %d pont / változat", line_no, MAX_POINTS);
            err = buf;
            return false;
        }
        if (sc < 0.0f || rt < 0.0f || (v.n > 0 && (sc <= v.scale[v.n - 1] || rt <= v.rate[v.n - 1]))) {
            std::snprintf(buf, sizeof buf, "%d. sor: a skála és a kg/ha soronként növekedjen", line_no);
            err = buf;
            return false;
        }
        v.scale[v.n] = sc;
        v.rate[v.n] = rt;
        ++v.n;
    }
    if (t.ncrop == 0) {
        err = "Üres táblázat";
        return false;
    }
    for (int i = 0; i < t.ncrop; ++i)
        for (int k = 0; k < t.crop[i].nvar; ++k)
            if (t.crop[i].var[k].n < 2) {
                std::snprintf(buf, sizeof buf, "%.40s: legalább 2 pont kell változatonként", t.crop[i].name);
                err = buf;
                return false;
            }
    if (!t.machine[0]) copy_str(t.machine, NAME_LEN, "Saját táblázat");
    out = t;
    return true;
}

std::string to_csv(const Table &t) {
    std::string o = "# gép;" + std::string(t.machine) + "\n";
    if (t.turns10 > 0.0f) o += "# fordulat_10;" + fmt_num(t.turns10) + "\n";
    o += "növény;változat;beállítás;skála;kg/ha\n";
    for (int i = 0; i < t.ncrop; ++i)
        for (int k = 0; k < t.crop[i].nvar; ++k) {
            const Variant &v = t.crop[i].var[k];
            for (int p = 0; p < v.n; ++p)
                o += std::string(t.crop[i].name) + ";" + v.label + ";" + v.setup + ";" + fmt_num(v.scale[p]) + ";" +
                     fmt_num(v.rate[p]) + "\n";
        }
    return o;
}

std::string to_json(const Table &t, float turns10) {
    std::string o = "{\"machine\":";
    json_str(o, t.machine);
    o += ",\"builtin\":";
    o += t.builtin ? "true" : "false";
    char b[32];
    std::snprintf(b, sizeof b, ",\"profile_turns\":%g", static_cast<double>(t.turns10));
    o += b;
    o += ",\"crops\":[";
    auto arr = [&o, &b](const float *v, int n) {
        o += '[';
        for (int i = 0; i < n; ++i) {
            std::snprintf(b, sizeof(b), "%s%g", i ? "," : "", static_cast<double>(v[i]));
            o += b;
        }
        o += ']';
    };
    for (int c = 0; c < t.ncrop; ++c) {
        if (c) o += ',';
        o += "{\"name\":";
        json_str(o, t.crop[c].name);
        o += ",\"fine\":";
        o += t.crop[c].nvar > 1 ? "true" : "false";
        o += ",\"curves\":[";
        for (int k = 0; k < t.crop[c].nvar; ++k) {
            const Variant &v = t.crop[c].var[k];
            if (k) o += ',';
            o += "{\"drive\":";
            json_str(o, v.label);
            o += ",\"setup\":";
            json_str(o, v.setup);
            o += ",\"scale\":";
            arr(v.scale, v.n);
            o += ",\"rate\":";
            arr(v.rate, v.n);
            o += '}';
        }
        o += "]}";
    }
    o += "],\"turns\":[";
    for (int i = 0; i < TURN_OPTIONS; ++i) {
        std::snprintf(b, sizeof(b), "%s%.1f", i ? "," : "", turns(i, turns10));
        o += b;
    }
    return o + "]}";
}

} // namespace chart
} // namespace seeder

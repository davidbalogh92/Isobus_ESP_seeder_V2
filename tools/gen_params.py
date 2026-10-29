#!/usr/bin/env python3
"""include/seeder/params_gen.hpp generálása a tools/params_def.py alapján.

Használat:  python tools/gen_params.py [--check]
--check: nem ír, csak ellenőrzi, hogy a commitolt fájl naprakész (CI).
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import params_def as pd  # noqa: E402

OUT = ROOT / "include" / "seeder" / "params_gen.hpp"
DOC = ROOT / "docs" / "PARAMETEREK_HU.md"
VT_INPUT_BASE = 9000  # VT Input Number / Input List objektum ID = 9000 + paraméter index


def c_str(s: str) -> str:
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def vt_input_id(index: int, p: pd.Param) -> int:
    return VT_INPUT_BASE + index if p.vt_page else 0


def render() -> str:
    out: list[str] = []
    w = out.append
    w("// GENERÁLT FÁJL – ne szerkeszd kézzel! Forrás: tools/params_def.py, generátor: tools/gen_params.py")
    w("#pragma once")
    w("#include <cstddef>")
    w("#include <cstdint>")
    w("")
    w("namespace seeder {")
    w(f"constexpr int MAX_SECTIONS = {pd.MAX_SECTIONS};")
    w(f"constexpr int MAX_METERS = {pd.MAX_METERS};")
    w("")
    w("struct Settings {")
    for p in pd.P:
        if p.kind == "float":
            w(f"    float {p.cname};")
        elif p.kind == "str":
            w(f"    char {p.cname}[{int(p.max) + 1}];")
        else:
            w(f"    std::int32_t {p.cname};")
    w("};")
    w("")
    w("enum class ParamKind : std::uint8_t { Float, Int, Enum, Bool, Str };")
    w("")
    w("struct ParamDef {")
    w("    const char *key;")
    w("    const char *group;")
    w("    const char *label;")
    w("    const char *vt_label;")
    w("    const char *unit;")
    w("    const char *help;")
    w("    ParamKind kind;")
    w("    float min, max, step, def;")
    w("    std::uint8_t decimals;")
    w("    const char *const *options;")
    w("    std::uint8_t option_count;")
    w("    std::uint16_t offset;")
    w("    const char *def_str;")
    w("    std::uint16_t vt_input_id;")
    w("    const char *vt_page;")
    w("    const char *feature;")
    w("    const char *scope;")
    w("};")
    w("")
    w("struct GroupDef { const char *id; const char *label; const char *icon; };")
    w("")
    w("enum ParamId : std::uint16_t {")
    for i, p in enumerate(pd.P):
        w(f"    P_{p.key} = {i},")
    w(f"    PARAM_COUNT = {len(pd.P)}")
    w("};")
    w("")
    w(f"constexpr std::size_t GROUP_COUNT = {len(pd.GROUPS)};")
    w(f"constexpr std::uint16_t VT_INPUT_BASE = {VT_INPUT_BASE};")
    w("extern const ParamDef PARAMS[PARAM_COUNT];")
    w("extern const GroupDef GROUPS[GROUP_COUNT];")
    w("")
    w("#ifdef SEEDER_PARAMS_IMPLEMENTATION")
    for i, p in enumerate(pd.P):
        if p.options:
            w(f"static const char *const OPT_{p.key}[] = {{{', '.join(c_str(o) for o in p.options)}}};")
    w("")
    w("const GroupDef GROUPS[GROUP_COUNT] = {")
    for g in pd.GROUPS:
        w(f"    {{{c_str(g[0])}, {c_str(g[1])}, {c_str(g[2])}}},")
    w("};")
    w("")
    w("const ParamDef PARAMS[PARAM_COUNT] = {")
    kinds = {"float": "Float", "int": "Int", "enum": "Enum", "bool": "Bool", "str": "Str"}
    for i, p in enumerate(pd.P):
        off = f"static_cast<std::uint16_t>(offsetof(Settings, {p.cname}))"
        opts = f"OPT_{p.key}" if p.options else "nullptr"
        if p.kind == "str":
            dflt, dstr = "0.0f", c_str(str(p.default))
        else:
            dflt, dstr = f"{float(p.default)!r}f", "nullptr"
        page = c_str(p.vt_page) if p.vt_page else "nullptr"
        w(f"    {{{c_str(p.key)}, {c_str(p.group)}, {c_str(p.label)}, {c_str(p.vt_label)}, {c_str(p.unit)}, "
          f"{c_str(p.help)}, ParamKind::{kinds[p.kind]}, {float(p.min)!r}f, {float(p.max)!r}f, "
          f"{float(p.step)!r}f, {dflt}, {p.decimals}, {opts}, {len(p.options)}, {off}, {dstr}, "
          f"{vt_input_id(i, p)}, {page}, {c_str(p.feature)}, {c_str(p.scope)}}},")
    w("};")
    w("#endif")
    w("} // namespace seeder")
    w("")
    return "\n".join(out)


def render_doc() -> str:
    pages = dict(pd.VT_PAGES)
    out = ["# Paraméterek", "", "_Generált fájl (tools/gen_params.py). Minden paraméter a webes felületen "
           "(Beállítások) és – ha van VT oldal megadva – a terminálon is állítható._", ""]
    for gid, glabel, _ in pd.GROUPS:
        ps = [p for p in pd.P if p.group == gid]
        if not ps:
            continue
        out += [f"## {glabel}", "", "| Kulcs | Megnevezés | Alapérték | Tartomány | VT oldal | Megjegyzés |",
                "|---|---|---|---|---|---|"]
        for p in ps:
            if p.kind in ("enum", "bool"):
                dflt = p.options[int(p.default)]
                rng = " / ".join(p.options)
            elif p.kind == "str":
                dflt, rng = f"`{p.default}`", f"{int(p.min)}–{int(p.max)} karakter"
            else:
                dflt = f"{p.default} {p.unit}".strip()
                rng = f"{p.min:g} – {p.max:g} {p.unit}".strip()
            tag = {"isobus": " **[ISOBUS]**", "aog": " **[AgOpenGPS]**"}.get(p.scope, "")
            out.append(f"| `{p.key}` | {p.label}{tag} | {dflt} | {rng} | {pages.get(p.vt_page, '–')} | {p.help} |")
        out.append("")
    return "\n".join(out)


def main() -> int:
    text = render()
    doc = render_doc()
    if "--check" in sys.argv:
        if not OUT.exists() or OUT.read_text(encoding="utf-8") != text or \
                not DOC.exists() or DOC.read_text(encoding="utf-8") != doc:
            print("params_gen.hpp ELAVULT – futtasd: python tools/gen_params.py", file=sys.stderr)
            return 1
        print("params_gen.hpp naprakész")
        return 0
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text, encoding="utf-8")
    DOC.parent.mkdir(parents=True, exist_ok=True)
    DOC.write_text(doc, encoding="utf-8")
    print(f"{OUT.relative_to(ROOT)}: {len(pd.P)} paraméter")
    return 0


if __name__ == "__main__":
    sys.exit(main())

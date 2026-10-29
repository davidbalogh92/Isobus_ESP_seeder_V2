#!/usr/bin/env python3
"""Az ESC ikonkészletet kiegészítő ikonok (SVG, 128×128 viewBox) az ESC stílusában:
#171717 körvonal, 5–6 px vonal, lekerekített végek, zöld (#258b28) / szürke (#8a8a8a) / piros kitöltés.

Futtatás:  python tools/pool/make_icons.py   (az assets/icons/{main,softkeys,menu}/ alá ír)
"""
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets" / "icons"

INK = "#171717"
GREEN = "#258b28"
GREY = "#8a8a8a"
RED = "#d32020"
YELLOW = "#ffd600"
BLUE = "#1e5bd8"
WHITE = "white"


def svg(title: str, body: str, sw: float = 6) -> str:
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">'
            f'<title>{title}</title><g fill="none" stroke="{INK}" stroke-width="{sw}" stroke-linecap="round" '
            f'stroke-linejoin="round">{body}</g></svg>')


# ---------------------------------------------------------------- egyszerű vonalas számjegyek / betűk
def digit(ch: str, x: float, y: float, h: float, color=WHITE, w=6) -> str:
    """Számjegy/betű vonalakkal; (x, y) a bal felső sarok, h a magasság (szélesség ~0,55 h)."""
    s = h / 10.0
    W = 5.5 * s
    P = {
        "1": [[(1.5, 2), (3.5, 0), (3.5, 10)]],
        "2": [[(0, 2.5), (1.2, 0.4), (3, 0), (4.6, 0.6), (5.5, 2.4), (5, 4.5), (0, 10), (5.5, 10)]],
        "0": [[(2.75, 0), (5, 1.2), (5.5, 5), (5, 8.8), (2.75, 10), (0.5, 8.8), (0, 5), (0.5, 1.2), (2.75, 0)]],
        "A": [[(0, 10), (2.75, 0), (5.5, 10)], [(1.0, 6.5), (4.5, 6.5)]],
        "K": [[(0.3, 0), (0.3, 10)], [(5.3, 0), (0.6, 5.6)], [(2.2, 4.0), (5.5, 10)]],
        "+": [[(2.75, 2), (2.75, 8)], [(0, 5), (5.5, 5)]],
        "-": [[(0.3, 5), (5.2, 5)]],
    }[ch]
    out = []
    for stroke in P:
        pts = " ".join(f"{x + px * s:.1f},{y + py * s:.1f}" for px, py in stroke)
        out.append(f'<polyline points="{pts}" stroke="{color}" stroke-width="{w}"/>')
    return "".join(out), W


def badge(ch: str, cx=99, cy=99, r=23, fill=GREEN) -> str:
    body, w = digit(ch, 0, 0, 26)
    d, _ = digit(ch, cx - w / 2, cy - 13, 26)
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="{fill}" stroke="{INK}" stroke-width="5"/>' + d


def power(fill: str, glyph=WHITE) -> str:
    return (f'<circle cx="64" cy="64" r="50" fill="{fill}"/>'
            f'<path d="M44 42a30 30 0 1 0 40 0" stroke="{glyph}" stroke-width="10"/>'
            f'<path d="M64 26v38" stroke="{glyph}" stroke-width="10"/>')


def lamp(on: bool) -> str:
    beams = (f'<path d="M92 40l22-12M96 64h26M92 88l22 12" stroke="#e0a800" stroke-width="7"/>' if on else "")
    return (f'<path d="M18 38h52l14 14v24L70 90H18Z" fill="#5a5a5a"/>'
            f'<circle cx="62" cy="64" r="15" fill="{YELLOW if on else "#bdbdbd"}"/>'
            f'<path d="M30 90v18M22 108h18"/>' + beams)


def radiator(fill: str, fan_fill: str) -> str:
    fins = "".join(f'<path d="M{x} 30v68" stroke-width="4"/>' for x in (26, 36, 46, 56))
    blades = ""
    for ang in (0, 120, 240):
        blades += (f'<path d="M92 64 q-4 -22 8 -26 q10 6 -8 26Z" fill="{fan_fill}" stroke-width="3" '
                   f'transform="rotate({ang} 92 64)"/>')
    return (f'<rect x="14" y="24" width="54" height="80" rx="6" fill="{fill}"/>' + fins +
            f'<circle cx="92" cy="64" r="30" fill="white" stroke-width="5"/>' + blades +
            f'<circle cx="92" cy="64" r="6" fill="{INK}"/>')


def gauge(needle=RED) -> str:
    ticks = "".join(
        f'<path d="M64 64 m{40 * c:.1f} {40 * s:.1f} l{8 * c:.1f} {8 * s:.1f}" stroke-width="5"/>'
        for c, s in ((-0.94, -0.34), (-0.64, -0.77), (0, -1), (0.64, -0.77), (0.94, -0.34)))
    return (f'<circle cx="64" cy="64" r="52" fill="white"/>' + ticks +
            f'<path d="M64 64 L96 40" stroke="{needle}" stroke-width="8"/><circle cx="64" cy="64" r="8" fill="{INK}"/>'
            f'<path d="M40 96h48" stroke-width="5"/><path d="M58 116h12v-14h-12Z" fill="{GREY}" stroke-width="4"/>')


def thermo(fill=RED) -> str:
    return (f'<path d="M52 20a12 12 0 0 1 24 0v52a24 24 0 1 1 -24 0Z" fill="white"/>'
            f'<path d="M64 36v52" stroke="{fill}" stroke-width="12"/><circle cx="64" cy="92" r="15" fill="{fill}" stroke="none"/>'
            f'<path d="M82 30h12M82 46h12M82 62h12" stroke-width="5"/>')


def drop_pipe(color=GREEN) -> str:
    return (f'<path d="M26 14h76l-18 30H44Z" fill="{color}"/>'
            f'<path d="M56 44v40l-16 30M72 44v40l16 30" stroke-width="5"/>'
            f'<ellipse cx="64" cy="62" rx="5" ry="7" fill="{YELLOW}" stroke-width="3"/>'
            f'<ellipse cx="64" cy="82" rx="5" ry="7" fill="{YELLOW}" stroke-width="3"/>'
            f'<path d="M104 70v34M92 92l12 14 12-14" stroke="{GREEN}" stroke-width="7"/>')


ICONS: dict[str, str] = {}
# ---------------------------------------------------------------- softkey-ek
ICONS["softkeys/softkey_master_on"] = svg("Vetes BE", power(GREEN))
ICONS["softkeys/softkey_master_off"] = svg("Vetes KI", power(GREY))
ICONS["softkeys/softkey_prefill"] = svg("Csofeltoltes", drop_pipe())
ICONS["softkeys/softkey_prefill_on"] = svg("Csofeltoltes fut", drop_pipe(YELLOW))
ICONS["softkeys/softkey_info"] = svg("Szamlalok", '<circle cx="64" cy="64" r="50" fill="white"/>'
                                     '<path d="M64 56v36" stroke-width="12"/><circle cx="64" cy="36" r="7" fill="#171717"/>')
ICONS["softkeys/softkey_ack"] = svg("Nyugtazas", '<circle cx="64" cy="64" r="50" fill="#258b28"/>'
                                    '<path d="M38 66l18 18 34-38" stroke="white" stroke-width="12"/>')
ICONS["softkeys/softkey_start"] = svg("Start", '<circle cx="64" cy="64" r="50" fill="#258b28"/>'
                                      '<path d="M52 40v48l38-24Z" fill="white" stroke="white" stroke-width="6"/>')
ICONS["softkeys/softkey_stop"] = svg("Stop", '<circle cx="64" cy="64" r="50" fill="#d32020"/>'
                                     '<rect x="44" y="44" width="40" height="40" fill="white" stroke="white"/>')
ICONS["softkeys/softkey_save"] = svg("Mentes", '<path d="M20 20h72l16 16v72H20Z" fill="#1e5bd8"/>'
                                     '<rect x="38" y="20" width="44" height="28" fill="white" stroke-width="5"/>'
                                     '<rect x="36" y="66" width="56" height="42" fill="white" stroke-width="5"/>'
                                     '<path d="M48 88l10 10 22-22" stroke="#258b28" stroke-width="7"/>')
ICONS["softkeys/softkey_cancel"] = svg("Megse", '<circle cx="64" cy="64" r="50" fill="white"/>'
                                       '<path d="M42 42l44 44M86 42l-44 44" stroke="#d32020" stroke-width="12"/>')
ICONS["softkeys/softkey_tare"] = svg(
    "Tara", '<path d="M18 92h92" stroke-width="7"/><path d="M28 92l8-30h56l8 30Z" fill="#bdbdbd"/>'
            + digit("0", 52, 24, 30, color=INK, w=7)[0]
            + '<path d="M14 40h20M26 32l8 8-8 8M114 40H94M102 32l-8 8 8 8" stroke="#258b28" stroke-width="6"/>')
ICONS["softkeys/softkey_scale_take"] = svg(
    "Merlegrol", '<g transform="translate(-12 6) scale(0.85)"><path d="M64 14v84M40 110h48M64 98v12"/>'
                 '<path d="M18 30h92"/><path d="M18 30 6 66h24Z"/><path d="M110 30l-12 36h24Z"/>'
                 '<path d="M4 66a14 10 0 0 0 28 0Z" fill="white"/><path d="M96 66a14 10 0 0 0 28 0Z" fill="white"/></g>'
                 '<path d="M96 70h26M110 58l12 12-12 12" stroke="#258b28" stroke-width="7"/>')
ICONS["softkeys/softkey_crank"] = svg(
    "Leforgatas", '<circle cx="56" cy="64" r="34" fill="white"/><circle cx="56" cy="64" r="8" fill="#171717"/>'
                  '<path d="M56 64 L96 30" stroke-width="8"/><path d="M96 30h18" stroke-width="12"/>'
                  '<path d="M28 100a40 40 0 0 0 60 0" stroke="#258b28" stroke-width="6"/>'
                  '<path d="M80 108l8-8-10-4" stroke="#258b28" stroke-width="6"/>')
ICONS["softkeys/softkey_func"] = svg(
    "Funkciok", '<rect x="16" y="16" width="42" height="42" rx="6" fill="#ffd600"/>'
                '<rect x="70" y="16" width="42" height="42" rx="6" fill="#258b28"/>'
                '<rect x="16" y="70" width="42" height="42" rx="6" fill="#1e5bd8"/>'
                '<rect x="70" y="70" width="42" height="42" rx="6" fill="#bdbdbd"/>')
ICONS["softkeys/softkey_tc_on"] = svg(
    "Szakasz automata", '<path d="M50 28a20 20 0 0 1 28 0M42 18a32 32 0 0 1 44 0" stroke-width="5"/>'
                        '<path d="M64 36v20"/><rect x="10" y="60" width="52" height="22" fill="#258b28"/>'
                        '<rect x="66" y="60" width="52" height="22" fill="#258b28"/>'
                        + digit("A", 52, 88, 30, color=INK, w=6)[0])
ICONS["softkeys/softkey_tc_off"] = svg(
    "Szakasz kezi", '<path d="M50 28a20 20 0 0 1 28 0M42 18a32 32 0 0 1 44 0" stroke="#8a8a8a" stroke-width="5"/>'
                    '<path d="M64 36v20" stroke="#8a8a8a"/><rect x="10" y="60" width="52" height="22" fill="white"/>'
                    '<rect x="66" y="60" width="52" height="22" fill="white"/>'
                    + digit("K", 50, 88, 30, color=INK, w=6)[0])
for i in ("1", "2"):
    ICONS[f"softkeys/softkey_lamp{i}_on"] = svg(f"Lampa {i} be", lamp(True) + badge(i, 26, 26, 20, GREEN))
    ICONS[f"softkeys/softkey_lamp{i}_off"] = svg(f"Lampa {i} ki", lamp(False) + badge(i, 26, 26, 20, GREY))
ICONS["softkeys/softkey_cooler_auto"] = svg("Hutes auto", radiator("#bdbdbd", GREEN) + badge("A", 26, 104, 20, GREEN))
ICONS["softkeys/softkey_cooler_on"] = svg("Hutes kezi be", radiator("#bdbdbd", BLUE) + badge("K", 26, 104, 20, BLUE))
ICONS["softkeys/softkey_cooler_off"] = svg("Hutes kezi ki", radiator("#bdbdbd", GREY) + badge("K", 26, 104, 20, GREY))
for name, ch in (("plus", "+"), ("minus", "-")):
    ICONS[f"softkeys/softkey_rate_{name}"] = svg(
        f"Mennyiseg {ch}", '<path d="M10 52h14m80 0h14"/><rect x="24" y="22" width="80" height="61" rx="9" fill="#258b28"/>'
                           '<path d="M35 37h58M35 52h58M35 67h58" stroke="white" stroke-width="6"/>'
                           + badge(ch, 99, 99, 23, "#1e5bd8"))
ICONS["softkeys/softkey_rate_reset"] = svg(
    "Mennyiseg 100%", '<path d="M10 52h14m80 0h14"/><rect x="24" y="22" width="80" height="61" rx="9" fill="#258b28"/>'
                      '<path d="M35 37h58M35 52h58M35 67h58" stroke="white" stroke-width="6"/>'
                      '<circle cx="99" cy="99" r="23" fill="white" stroke-width="5"/>'
                      '<path d="M88 100a11 11 0 1 0 4 -10" stroke-width="5"/><path d="M86 84v8h8" stroke-width="5"/>')
METER_BODY = ('<path d="M10 52h14m80 0h14"/><rect x="24" y="22" width="80" height="61" rx="9" fill="{f}"/>'
              '<path d="M35 37h58M35 52h58M35 67h58" stroke="white" stroke-width="6"/>')
ICONS["softkeys/softkey_mode_auto"] = svg("Adagolas automata", METER_BODY.format(f=GREEN) + badge("A", 99, 99, 23, GREEN))
ICONS["softkeys/softkey_mode_manual"] = svg("Adagolas kezi", METER_BODY.format(f=GREY) + badge("K", 99, 99, 23, BLUE))
ICONS["softkeys/softkey_crank_copy"] = svg(
    "Leforgatas masolasa", '<rect x="8" y="14" width="50" height="38" rx="6" fill="#258b28"/>'
                           '<path d="M16 26h34M16 38h34" stroke="white" stroke-width="5"/>'
                           '<rect x="70" y="76" width="50" height="38" rx="6" fill="#258b28"/>'
                           '<path d="M78 88h34M78 100h34" stroke="white" stroke-width="5"/>'
                           '<path d="M34 60v34h26M50 82l12 12-12 12" stroke="#1e5bd8" stroke-width="7"/>'
                           '<path d="M70 20h22a12 12 0 0 1 12 12v28" stroke="#8a8a8a" stroke-width="5" stroke-dasharray="6 6"/>')
ICONS["softkeys/softkey_meter_select"] = svg(
    "Adagolo valasztas", '<path d="M10 52h14m80 0h14"/><rect x="24" y="22" width="80" height="61" rx="9" fill="#258b28"/>'
                         '<path d="M35 37h58M35 52h58M35 67h58" stroke="white" stroke-width="6"/>'
                         '<path d="M30 104h60M78 94l12 10-12 10" stroke="#1e5bd8" stroke-width="7"/>')

# ---------------------------------------------------------------- főképernyő
ICONS["main/main_pressure"] = svg("Nyomas", gauge())
ICONS["main/main_temp"] = svg("Olajhomerseklet", thermo())
ICONS["main/main_cooler_on"] = svg("Hutoventilator be", radiator("#bdbdbd", BLUE))
ICONS["main/main_cooler_off"] = svg("Hutoventilator ki", radiator("#e0e0e0", GREY))
ICONS["main/main_prefill"] = svg("Feltoltes", drop_pipe(YELLOW))

# ---------------------------------------------------------------- menü (beállítóoldalak)
ICONS["menu/menu_machine"] = svg(
    "Gep", '<path d="M14 40h100" stroke-width="6"/><path d="M30 40l10 28h48l10-28Z" fill="#258b28"/>'
           '<path d="M28 82h72"/><circle cx="40" cy="96" r="12" fill="white"/><circle cx="88" cy="96" r="12" fill="white"/>'
           '<path d="M14 28v24M114 28v24"/>')
ICONS["menu/menu_drive"] = svg(
    "Hajtas", '<rect x="12" y="38" width="64" height="52" rx="8" fill="#5a5a5a"/>'
              '<path d="M24 52h40M24 64h40M24 76h40" stroke="#bdbdbd" stroke-width="5"/>'
              '<path d="M76 64h20" stroke-width="10"/><circle cx="106" cy="64" r="14" fill="#258b28"/>'
              '<path d="M60 108h40M88 98l12 10-12 10" stroke="#1e5bd8" stroke-width="6"/>')
ICONS["menu/menu_extras"] = svg(
    "Kiegeszitok", '<path d="M24 24h34v14a10 10 0 1 0 20 0V24h26v34H90a10 10 0 1 0 0 20h14v26H24Z" fill="#ffd600"/>')
ICONS["menu/menu_counters"] = svg(
    "Szamlalok", '<rect x="12" y="20" width="104" height="88" rx="8" fill="white"/>'
                 '<path d="M88 40H40l24 24-24 24h48" stroke-width="8"/>')
ICONS["menu/menu_diag"] = svg(
    "Szimulacio", '<path d="M28 100l46-46" stroke-width="12"/><circle cx="84" cy="44" r="22" fill="white"/>'
                  '<path d="M84 24v20h20" stroke="#d32020" stroke-width="6"/><circle cx="26" cy="102" r="10" fill="#8a8a8a"/>')

# visszafúvás: hűtő + fordított irányú nyíl
REV_ARROW = ('<path d="M112 16a30 30 0 0 0-40 0" stroke="{c}" stroke-width="7"/>'
             '<path d="M72 4v14h14" stroke="{c}" stroke-width="7"/>')
ICONS["softkeys/softkey_cooler_rev"] = svg("Visszafujas", radiator("#bdbdbd", GREY) + REV_ARROW.format(c=BLUE))
ICONS["softkeys/softkey_cooler_rev_on"] = svg("Visszafujas fut", radiator("#bdbdbd", YELLOW) + REV_ARROW.format(c=RED))
# leforgatás számláló nullázása
ICONS["softkeys/softkey_crank_reset"] = svg(
    "Leforgatas nullazas", '<circle cx="56" cy="64" r="34" fill="white"/><circle cx="56" cy="64" r="8" fill="#171717"/>'
                           '<path d="M56 64 L96 30" stroke-width="8"/><path d="M96 30h18" stroke-width="12"/>'
                           + badge("0", 99, 99, 23, BLUE))
# AgOpenGPS: műhold + traktor-nyomvonal
ICONS["menu/menu_aog"] = svg(
    "AgOpenGPS", '<path d="M20 112l26-58h36l26 58Z" fill="#258b28"/><path d="M64 60v48" stroke="white" stroke-width="5" '
                 'stroke-dasharray="10 8"/><rect x="50" y="12" width="28" height="20" rx="3" fill="#1e5bd8"/>'
                 '<path d="M30 22h20M78 22h20" stroke-width="8"/><path d="M64 32v12"/>')
# diagnosztika: képernyő jelalakkal
ICONS["menu/menu_diagnose"] = svg(
    "Diagnosztika", '<rect x="12" y="20" width="104" height="76" rx="8" fill="white"/>'
                    '<path d="M22 66h18l8-24 14 44 12-34 8 14h24" stroke="#258b28" stroke-width="6"/>'
                    '<path d="M44 108h40M64 96v12"/>')

if __name__ == "__main__":
    for name, text in ICONS.items():
        p = OUT / f"{name}.svg"
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(text + "\n", encoding="utf-8")
    print(f"{len(ICONS)} ikon → {OUT.relative_to(ROOT)}")

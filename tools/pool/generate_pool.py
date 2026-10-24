#!/usr/bin/env python3
"""ISO 11783-6 (VT3) object pool generálása a vetőgép-vezérlőhöz.

Kimenetek:
  generated/object_pool.iop        – a firmware-be ágyazott bináris pool
  include/seeder/vt_ids.hpp        – objektumazonosítók és képtáblák a C++ kódnak
  docs/vt_preview/*.png            – előnézeti képek minden maszkról (--preview)

Tervezési méret: 400×400 data mask (Müller/Dammann BASIC terminál), softkey 80 px magas.
* A képek az ESC SVG ikonokból **pontos pixelméretre** renderelve, 8 bites (256 szín) VT
  palettára, RLE tömörítéssel, a világosszürke háttérre simítva (BASIC-en 1:1, nem méretez a VT).
* Más felbontású terminálon az AgIsoStack++ feltöltés előtt átméretezi a poolt
  (set_object_pool_scaling); a softkey-gyerekek pozícióját a softkey szélesség/80 arány skálázza,
  ezért a softkey-képek a BASIC 120×80-as gombjának közepére vannak igazítva.
* Csak VT2/VT3-biztos objektumok (nincs Output List, nincs függőleges igazítás).

Futtatás:  python tools/pool/generate_pool.py [--preview] [--check]
"""
from __future__ import annotations

import sys
from dataclasses import dataclass, field
from pathlib import Path
from struct import pack

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(ROOT / "tools"))

import params_def as pd  # noqa: E402
from PIL import Image  # noqa: E402
from svgr import render  # noqa: E402

OUT_IOP = ROOT / "generated" / "object_pool.iop"
OUT_HPP = ROOT / "include" / "seeder" / "vt_ids.hpp"
ICON_DIR = ROOT / "assets" / "icons"
PREVIEW_DIR = ROOT / "docs" / "vt_preview"

MASK = 400            # tervezési data mask méret (Müller BASIC)
SOFTKEY = 80          # tervezési softkey méret (a BASIC softkey 120×80: a pozíciók ×1,5-del skálázódnak)
KEY_REAL_W = 120      # BASIC softkey szélesség – a képek erre vannak középre igazítva
NULL = 0xFFFF
VT_INPUT_BASE = 9000  # == tools/gen_params.py

# ------------------------------------------------------------------ objektumtípusok
T_WS, T_DATAMASK, T_ALARMMASK, T_CONTAINER, T_SKMASK, T_KEY, T_BUTTON = 0, 1, 2, 3, 4, 5, 6
T_INNUM, T_INLIST, T_OUTSTR, T_OUTNUM, T_LINE, T_RECT, T_BAR = 9, 10, 11, 12, 13, 14, 18
T_PIC, T_NUMVAR, T_FONT, T_LINEATTR, T_FILLATTR, T_OBJPTR = 20, 21, 23, 24, 25, 27

# VT font méretkódok (ISO 11783-6)
FONT_PX = {0: (6, 8), 1: (8, 8), 2: (8, 12), 3: (12, 16), 4: (16, 16), 5: (16, 24), 6: (24, 32),
           7: (32, 32), 8: (32, 48), 9: (48, 64)}

# Latin-1 font: az ő/ű nincs benne, a megszokott õ/û helyettesítést használjuk
LATIN1_MAP = str.maketrans({"ő": "õ", "Ő": "Õ", "ű": "û", "Ű": "Û", "–": "-", "→": ">", "…": "...", "°": "°"})

# ------------------------------------------------------------------ VT 256 színű paletta (ISO 11783-6 B. melléklet)
BASIC = [(0, 0, 0), (255, 255, 255), (0, 153, 0), (0, 153, 153), (153, 0, 0), (153, 0, 153), (153, 153, 0),
         (204, 204, 204), (153, 153, 153), (0, 0, 255), (0, 255, 0), (0, 255, 255), (255, 0, 0),
         (255, 0, 255), (255, 255, 0), (0, 0, 153)]
LEVELS = (0, 51, 102, 153, 204, 255)
PALETTE = BASIC + [(LEVELS[r], LEVELS[g], LEVELS[b]) for r in range(6) for g in range(6) for b in range(6)]
BLACK, WHITE, GREEN, TEAL, MAROON, PURPLE, OLIVE, SILVER = range(8)
GREY, BLUE, LIME, CYAN, RED, MAGENTA, YELLOW, NAVY = range(8, 16)
BG = SILVER           # világosszürke háttér mindenhol


def vt_text(s: str) -> bytes:
    return s.translate(LATIN1_MAP).encode("latin-1")


_near = {}


def nearest(rgb):
    if rgb not in _near:
        r, g, b = rgb
        _near[rgb] = min(range(len(PALETTE)), key=lambda i: (PALETTE[i][0] - r) ** 2 + (PALETTE[i][1] - g) ** 2 +
                         (PALETTE[i][2] - b) ** 2)
    return _near[rgb]


# Lapos rajzok (geometria vázlatok) csak a saját színeikkel – jobb RLE tömörítés
FLAT_COLOURS = [(255, 255, 255), (23, 23, 23), (102, 102, 102), (204, 204, 204), (204, 0, 0), (37, 139, 40),
                (0, 51, 204)]


def flat_c(rgb):
    return min(FLAT_COLOURS, key=lambda c: sum((a - b) ** 2 for a, b in zip(c, rgb)))


def icon_pixels(name: str, size: int, crop=None, bg=BG):
    """SVG → (szélesség, magasság, palettaindexek) a háttérszínre simítva, opcionális vágással."""
    rgba = render(ICON_DIR / f"{name}.svg", size)
    base = Image.new("RGBA", rgba.size, PALETTE[bg] + (255,))
    base.alpha_composite(rgba)
    img = base.convert("RGB")
    if crop:
        k = size / 128.0
        img = img.crop(tuple(round(v * k) for v in crop))
    w, h = img.size
    px = img.load()
    flat = name.startswith("geo/")
    conv = (lambda c: nearest(flat_c(c))) if flat else nearest
    return w, h, bytes(conv(px[x, y]) for y in range(h) for x in range(w))


def rle8(data: bytes) -> bytes:
    out = bytearray()
    i = 0
    while i < len(data):
        j = i
        while j < len(data) and data[j] == data[i] and j - i < 255:
            j += 1
        out += bytes((j - i, data[i]))
        i = j
    return bytes(out)


# ------------------------------------------------------------------ pool modell
@dataclass
class Obj:
    id: int
    type: int
    body: bytes
    meta: dict = field(default_factory=dict)


class Pool:
    def __init__(self):
        self.objs: dict[int, Obj] = {}
        self.names: dict[str, int] = {}
        self.counters: dict[int, int] = {}

    def new_id(self, base: int) -> int:
        n = self.counters.get(base, base)
        while n in self.objs:
            n += 1
        self.counters[base] = n + 1
        return n

    def put(self, ident: int, kind: int, body: bytes, name: str | None = None, **meta) -> int:
        assert 0 <= ident < NULL, ident
        assert ident not in self.objs, f"duplikált ID {ident} ({name})"
        self.objs[ident] = Obj(ident, kind, body, meta)
        if name:
            assert name not in self.names, name
            self.names[name] = ident
        return ident

    def binary(self) -> bytes:
        out = bytearray()
        for ident in sorted(self.objs):
            o = self.objs[ident]
            out += pack("<HB", o.id, o.type) + o.body
        return bytes(out)

    @staticmethod
    def _children(items):
        return b"".join(pack("<Hhh", c, x, y) for c, x, y in items)

    def working_set(self, ident, active_mask, children, bg=BG):
        return self.put(ident, T_WS, pack("<BBHBBB", bg, 1, active_mask, len(children), 0, 0) +
                        self._children(children), kids=children, bg=bg)

    def data_mask(self, ident, softkey_mask, children, bg=BG, name=None):
        return self.put(ident, T_DATAMASK, pack("<BHBB", bg, softkey_mask, len(children), 0) +
                        self._children(children), name, kids=children, bg=bg, sk=softkey_mask)

    def alarm_mask(self, ident, softkey_mask, children, bg=WHITE, priority=1, acoustic=1, name=None):
        return self.put(ident, T_ALARMMASK, pack("<BHBBBB", bg, softkey_mask, priority, acoustic, len(children), 0)
                        + self._children(children), name, kids=children, bg=bg, sk=softkey_mask)

    def container(self, ident, w, h, children, hidden=False, name=None):
        return self.put(ident, T_CONTAINER, pack("<HHBBB", w, h, 1 if hidden else 0, len(children), 0) +
                        self._children(children), name, kids=children, w=w, h=h, hidden=hidden)

    def softkey_mask(self, ident, keys, bg=BG, name=None):
        return self.put(ident, T_SKMASK, pack("<BBB", bg, len(keys), 0) + b"".join(pack("<H", k) for k in keys),
                        name, keys=keys, bg=bg)

    def key(self, ident, code, children, bg=BG, name=None):
        assert 0 < code < 255
        return self.put(ident, T_KEY, pack("<BBBB", bg, code, len(children), 0) + self._children(children), name,
                        kids=children, bg=bg)

    def button(self, ident, w, h, code, children, bg=BG, border=BLACK, name=None):
        return self.put(ident, T_BUTTON, pack("<HHBBBBBB", w, h, bg, border, code, 0, len(children), 0) +
                        self._children(children), name, kids=children, w=w, h=h, bg=bg)

    def font(self, ident, colour, size, name=None):
        return self.put(ident, T_FONT, pack("<BBBBB", colour, size, 0, 0, 0), name, colour=colour, size=size)

    def line_attr(self, ident, colour, width=1, name=None):
        return self.put(ident, T_LINEATTR, pack("<BBHB", colour, width, 0xFFFF, 0), name, colour=colour, width=width)

    def fill_attr(self, ident, colour, name=None, ftype=2):
        return self.put(ident, T_FILLATTR, pack("<BBHB", ftype, colour, NULL, 0), name, colour=colour)

    def rect(self, ident, w, h, line_attr, fill_attr, suppress=0, name=None):
        return self.put(ident, T_RECT, pack("<HHHBHB", line_attr, w, h, suppress, fill_attr, 0), name,
                        w=w, h=h, line=line_attr, fill=fill_attr, suppress=suppress)

    def out_str(self, ident, text, w, h, font, bg=BG, transparent=True, align=0, name=None):
        raw = vt_text(text)
        return self.put(ident, T_OUTSTR, pack("<HHBHBHBH", w, h, bg, font, 1 if transparent else 0, NULL, align,
                                              len(raw)) + raw + b"\x00", name, text=text, w=w, h=h, font=font,
                        bg=bg, transparent=transparent, align=align)

    def out_num(self, ident, w, h, font, value=0, scale=1.0, offset=0, decimals=0, align=2, bg=BG,
                transparent=True, name=None):
        return self.put(ident, T_OUTNUM, pack("<HHBHBHIifBBBB", w, h, bg, font, 1 if transparent else 0, NULL,
                                              value, offset, scale, decimals, 0, align, 0), name,
                        w=w, h=h, font=font, value=value, scale=scale, offset=offset, decimals=decimals,
                        align=align, bg=bg, transparent=transparent)

    def in_num(self, ident, w, h, font, value, vmin, vmax, scale, offset, decimals, bg=WHITE, name=None):
        return self.put(ident, T_INNUM, pack("<HHBHBHIIIifBBBBB", w, h, bg, font, 0, NULL, value, vmin, vmax,
                                             offset, scale, decimals, 0, 2, 1, 0), name,
                        w=w, h=h, font=font, value=value, scale=scale, offset=offset, decimals=decimals, bg=bg)

    def in_list(self, ident, w, h, items, value=0, name=None):
        return self.put(ident, T_INLIST, pack("<HHHBBBB", w, h, NULL, value, len(items), 1, 0) +
                        b"".join(pack("<H", i) for i in items), name, w=w, h=h, items=items, value=value)

    def picture(self, ident, icon, size, crop=None, bg=BG, name=None):
        w, h, pix = icon_pixels(icon, size, crop, bg)
        packed = rle8(pix)
        use_rle = len(packed) < len(pix)
        data = packed if use_rle else pix
        # szélesség = tényleges szélesség: a VT nem méretez (BASIC-en 1:1)
        return self.put(ident, T_PIC, pack("<HHHBBBIB", w, w, h, 2, 0x04 if use_rle else 0x00, 0, len(data), 0) +
                        data, name, w=w, h=h, icon=icon, pix=pix)

    def obj_ptr(self, ident, target, name=None):
        return self.put(ident, T_OBJPTR, pack("<H", target), name, value=target)


P = Pool()

# ------------------------------------------------------------------ alapobjektumok
F_SMALL = P.font(23000, BLACK, 2, "F_SMALL")        # 8×12
F_LABEL = P.font(23001, BLACK, 3, "F_LABEL")        # 12×16
F_LABEL_W = P.font(23002, WHITE, 3, "F_LABEL_W")
F_VALUE = P.font(23003, BLACK, 5, "F_VALUE")        # 16×24
F_BIG = P.font(23004, BLACK, 6, "F_BIG")            # 24×32
F_HUGE = P.font(23005, NAVY, 8, "F_HUGE")           # 32×48
F_TITLE = P.font(23006, WHITE, 5, "F_TITLE")
F_INPUT = P.font(23007, NAVY, 5, "F_INPUT")         # beviteli mezők 16×24
F_INPUT_S = P.font(23008, NAVY, 3, "F_INPUT_S")     # beviteli listák 12×16
F_RED = P.font(23009, RED, 5, "F_RED")
F_VALUE_N = P.font(23010, NAVY, 5, "F_VALUE_N")
F_MSG = P.font(23011, BLACK, 5, "F_MSG")            # állapotsor 16×24
F_LABEL_R = P.font(23012, RED, 3, "F_LABEL_R")
F_VALUE_N_S = P.font(23013, NAVY, 3, "F_VALUE_N_S")

L_BLACK = P.line_attr(24000, BLACK, 1, "L_BLACK")
L_NAVY2 = P.line_attr(24001, NAVY, 2, "L_NAVY2")
L_GREY = P.line_attr(24002, GREY, 1, "L_GREY")

FL_WHITE = P.fill_attr(25000, WHITE, "FL_WHITE")
FL_NAVY = P.fill_attr(25001, NAVY, "FL_NAVY")
FL_SILVER = P.fill_attr(25002, SILVER, "FL_SILVER")
FL_BLACK = P.fill_attr(25003, BLACK, "FL_BLACK")
FL_MSG = P.fill_attr(25004, WHITE, "FL_MSG")        # állapotsor háttere (futásidőben színezve)

# ------------------------------------------------------------------ képek (lustán, méretenként egyszer)
_pic_cache: dict[tuple, int] = {}


def PIC(icon: str, size: int, crop=None, bg=BG) -> int:
    key = (icon, size, crop, bg)
    if key not in _pic_cache:
        stem = icon.split("/")[-1].upper().replace("SOFTKEY_", "SK_").replace("MAIN_", "M_").replace("MENU_", "MN_")
        name = f"PIC_{stem}_{size}" + ("" if bg == BG else "_W")
        if crop:
            name += "_C"
        _pic_cache[key] = P.picture(P.new_id(30000), icon, size, crop, bg, name)
    return _pic_cache[key]


# ------------------------------------------------------------------ segédek
def S(text, w=None, h=None, font=F_LABEL, align=0, name=None, transparent=True, bg=BG):
    fw, fh = FONT_PX[P.objs[font].meta["size"]]
    w = w or fw * len(text) + 2
    h = h or fh + 4
    return P.out_str(P.new_id(11000), text, w, h, font, bg=bg, transparent=transparent, align=align, name=name)


def N(name, w, font=F_VALUE, decimals=0, scale=1.0, offset=0, align=2, h=None, value=0):
    fh = FONT_PX[P.objs[font].meta["size"]][1]
    return P.out_num(P.new_id(12000), w, h or fh + 4, font, value=value, scale=scale, offset=offset,
                     decimals=decimals, align=align, name=name)


def R(w, h, fill=FL_WHITE, line=L_BLACK, suppress=0, name=None):
    return P.rect(P.new_id(14000), w, h, line, fill, suppress, name)


def title_bar(text):
    return [(R(MASK, 40, FL_NAVY, L_BLACK, 0x0F), 0, 0), (S(text, 390, 28, F_TITLE), 8, 8)]


def C(w, h, kids, hidden=False, name=None):
    return P.container(P.new_id(3000), w, h, kids, hidden, name)


# ------------------------------------------------------------------ softkey-ek
_key_code = [1]
KEY_PIC = 60
KEY_X = (KEY_REAL_W - KEY_PIC) * SOFTKEY // KEY_REAL_W // 2    # 20 → BASIC-en 30: középre
KEY_Y = (SOFTKEY - KEY_PIC) * SOFTKEY // KEY_REAL_W // 2        # 6 → BASIC-en 9-10


def KEY(name, icon=None, child=None):
    code = _key_code[0]
    _key_code[0] += 1
    kids = []
    if child is not None:
        kids = [(child, KEY_X, KEY_Y)]
    elif icon:
        kids = [(PIC(f"softkeys/{icon}", KEY_PIC), KEY_X, KEY_Y)]
    return P.key(P.new_id(5000), code, kids, name=name)


def KEYPTR(name, ptr_name, icons):
    pics = [PIC(f"softkeys/{i}", KEY_PIC) for i in icons]
    ptr = P.obj_ptr(P.new_id(27000), pics[0], ptr_name)
    return KEY(name, child=ptr), pics


K_MASTER, PIC_K_MASTER = KEYPTR("K_MASTER", "OP_K_MASTER", ["softkey_master_off", "softkey_master_on"])
K_METER1, PIC_K_METER1 = KEYPTR("K_METER1", "OP_K_METER1", ["softkey_meter1_off", "softkey_meter1_on"])
K_METER2, PIC_K_METER2 = KEYPTR("K_METER2", "OP_K_METER2", ["softkey_meter2_off", "softkey_meter2_on"])
K_SEC_AUTO, PIC_K_SEC = KEYPTR("K_SEC_AUTO", "OP_K_SEC", ["softkey_tc_off", "softkey_tc_on"])
K_PREFILL, PIC_K_PREFILL = KEYPTR("K_PREFILL", "OP_K_PREFILL", ["softkey_prefill", "softkey_prefill_on"])
K_LAMP1, PIC_K_LAMP1 = KEYPTR("K_LAMP1", "OP_K_LAMP1", ["softkey_lamp1_off", "softkey_lamp1_on"])
K_LAMP2, PIC_K_LAMP2 = KEYPTR("K_LAMP2", "OP_K_LAMP2", ["softkey_lamp2_off", "softkey_lamp2_on"])
K_VALVE, PIC_K_VALVE = KEYPTR("K_VALVE", "OP_K_VALVE", ["softkey_fold", "softkey_marker"])
K_COOLER, PIC_K_COOLER = KEYPTR("K_COOLER", "OP_K_COOLER", ["softkey_cooler_auto", "softkey_cooler_on",
                                                            "softkey_cooler_off"])
K_MODE, PIC_K_MODE = KEYPTR("K_MODE", "OP_K_MODE", ["softkey_mode_auto", "softkey_mode_manual"])
K_CAL_METER, PIC_K_CAL_METER = KEYPTR("K_CAL_METER", "OP_K_CAL_METER", ["softkey_meter1_on", "softkey_meter2_on"])
K_MENU = KEY("K_MENU", "softkey_set")
K_FUNC = KEY("K_FUNC", "softkey_func")
K_CRANK = KEY("K_CRANK", "softkey_crank")
K_BACK = KEY("K_BACK", "softkey_back")
K_BLANK = [KEY(f"K_BLANK{i}") for i in range(3)]
K_RATE_PLUS = KEY("K_RATE_PLUS", "softkey_rate_plus")
K_RATE_MINUS = KEY("K_RATE_MINUS", "softkey_rate_minus")
K_RATE_RESET = KEY("K_RATE_RESET", "softkey_rate_reset")
K_ACK = KEY("K_ACK", "softkey_ack")
K_RESET_DAY = KEY("K_RESET_DAY", "softkey_cancel")
K_START = KEY("K_START", "softkey_start")
K_STOP = KEY("K_STOP", "softkey_stop")
K_SAVE = KEY("K_SAVE", "softkey_save")
K_CANCEL = KEY("K_CANCEL", "softkey_cancel")
K_TARE = KEY("K_TARE", "softkey_tare")
K_SCALE_CAL = KEY("K_SCALE_CAL", "softkey_save")
K_SCALE_TAKE = KEY("K_SCALE_TAKE", "softkey_scale_take")
K_SCALE_PAGE = KEY("K_SCALE_PAGE", "softkey_calib")
K_CRANK_START = KEY("K_CRANK_START", "softkey_start")
K_CRANK_STOP = KEY("K_CRANK_STOP", "softkey_stop")
K_CRANK_SAVE = KEY("K_CRANK_SAVE", "softkey_save")
K_PRESS_CAL = KEY("K_PRESS_CAL", "softkey_save")

# oldalak (menü): azonosító → (menüfelirat, ikon)
PAGE_INFO = {
    "machine": ("Gép", "menu/menu_machine"),
    "rate": ("Adagolás", "softkeys/softkey_dosing"),
    "drive": ("Hajtás", "menu/menu_drive"),
    "fan": ("Turbina", "softkeys/softkey_fan"),
    "speed": ("Sebesség", "main/main_speed"),
    "switches": ("Kapcsolók", "softkeys/softkey_switch"),
    "extras": ("Kiegészítõk", "menu/menu_extras"),
    "pressure": ("Nyomás", "main/main_pressure"),
    "cooling": ("Olajhûtés", "main/main_temp"),
    "scale": ("Mérleg", "softkeys/softkey_calib"),
    "tc": ("Task Contr.", "softkeys/softkey_tc"),
    "geometry": ("Geometria", "softkeys/softkey_geom"),
    "diag": ("Szimuláció", "menu/menu_diag"),
}
K_PAGE = {pid: KEY("K_PG_" + pid.upper(), child=PIC(PAGE_INFO[pid][1], KEY_PIC)) for pid, _ in pd.VT_PAGES}
K_PG_CALIB = KEY("K_PG_CALIB", "softkey_crank")
K_PG_INFO = KEY("K_PG_INFO", child=PIC("menu/menu_counters", KEY_PIC))

# --- főképernyő softkey maszkok: (adagolók 1/2) × (elektromos hajtás 0/1).
# BASIC terminál: az első 5 gomb a jobb oszlop felülről, a következő 5 a bal oszlop.
def main_keys(meters: int, electric: bool):
    # a számlálók a Beállítások menübe kerültek; helyükön elektromos hajtásnál az AUTO/KÉZI gomb
    right = [K_MASTER] + ([K_METER2] if meters == 2 else []) + [K_SEC_AUTO, K_MODE if electric else K_FUNC]
    right += K_BLANK[:4 - len(right)] + [K_MENU]
    left = [K_METER1] + ([K_FUNC] if electric else []) + [K_CRANK] + ([K_PREFILL] if electric else [])
    return right + left


SK_MAIN = {}
for meters in (1, 2):
    for electric in (0, 1):
        SK_MAIN[(meters, electric)] = P.softkey_mask(P.new_id(4000), main_keys(meters, bool(electric)),
                                                     name=f"SK_MAIN_M{meters}_E{electric}")


# --- funkció softkey maszkok (a főképernyőn): lámpák / szelep / hűtő engedélyezése szerint
def func_keys(lights: bool, valve: bool, cooler: bool):
    right = [K_BACK, K_RATE_PLUS, K_RATE_MINUS, K_RATE_RESET, K_ACK]
    left = ([K_LAMP1, K_LAMP2] if lights else []) + ([K_VALVE] if valve else []) + ([K_COOLER] if cooler else [])
    return right + left


SK_FUNC = []
for bits in range(8):
    SK_FUNC.append(P.softkey_mask(P.new_id(4000), func_keys(bool(bits & 1), bool(bits & 2), bool(bits & 4)),
                                  name=f"SK_FUNC_{bits}"))
SK_BACK = P.softkey_mask(P.new_id(4000), [K_BACK], name="SK_BACK")
SK_INFO = P.softkey_mask(P.new_id(4000), [K_BACK, K_RESET_DAY], name="SK_INFO")
SK_MENU = P.softkey_mask(P.new_id(4000), [K_BACK, K_PG_CALIB, K_PG_INFO], name="SK_MENU")
SK_WCAL = P.softkey_mask(P.new_id(4000), [K_BACK, K_START, K_STOP, K_SAVE, K_CANCEL], name="SK_WCAL")
SK_SCALE = P.softkey_mask(P.new_id(4000), [K_BACK, K_TARE, K_SCALE_CAL], name="SK_SCALE")
SK_PRESSURE = P.softkey_mask(P.new_id(4000), [K_BACK, K_PRESS_CAL], name="SK_PRESSURE")
SK_COOLING = P.softkey_mask(P.new_id(4000), [K_BACK, K_COOLER], name="SK_COOLING")
SK_CALIB = P.softkey_mask(P.new_id(4000), [K_BACK, K_CAL_METER, K_SCALE_TAKE, K_CRANK_SAVE, K_SCALE_PAGE,
                                            K_CRANK_START, K_CRANK_STOP], name="SK_CALIB")
SK_CALIB_GROUND = P.softkey_mask(P.new_id(4000), [K_BACK, K_CAL_METER, K_SCALE_TAKE, K_CRANK_SAVE, K_SCALE_PAGE],
                                 name="SK_CALIB_GROUND")
SK_ALARM = P.softkey_mask(P.new_id(4000), [K_ACK], name="SK_ALARM")

# ------------------------------------------------------------------ szövegek (object pointerrel váltva)
MSG = {}
for key, text in [("manual", "KÉZI ADAGOLÁS"), ("off", "VETÉS KI"), ("raised", "GÉP KIEMELVE"), ("standing", "ÁLL - NEM VET"),
                  ("sowing", "VETÉS"), ("prefill", "CSÕFELTÖLTÉS..."), ("start", "START, INDULHATSZ!"),
                  ("crank", "LEFORGATÁS FUT"), ("alarm", "RIASZTÁS")]:
    MSG[key] = S(text, 300, 28, F_MSG, name="STR_MSG_" + key.upper())
ALARM_TEXTS = ["1. adagoló nem forog", "2. adagoló nem forog", "1. adagoló: adageltérés",
               "2. adagoló: adageltérés", "Turbina fordulat kicsi", "Turbina fordulat magas",
               "1. tartály kiürült", "2. tartály kiürült", "Nincs sebességjel", "ISOBUS sebesség kiesett",
               "Olaj túlmelegedett", "Olajhõmérõ hiba", "Nyomásszenzor hiba", "Mérleg nem válaszol",
               "Kerék nincs kalibrálva", "Szimuláció aktív", "Task Controller kiesett"]
STR_ALARM = [S(t.upper()[:25], 300, 28, F_LABEL_R, name=f"STR_ALARM_{i}") for i, t in enumerate(ALARM_TEXTS)]
STR_ALARM_BIG = [S(t, 380, 30, F_RED, name=f"STR_ALARMBIG_{i}") for i, t in enumerate(ALARM_TEXTS)]
CAL_STATE = [S("Indításra kész", 184, 22, F_LABEL, name="STR_CAL_IDLE"),
             S("Mérés fut...", 184, 22, F_LABEL, name="STR_CAL_RUN"),
             S("Kész, mentheted", 184, 22, F_LABEL, name="STR_CAL_STOP")]
# futásidőben szöveggel feltöltött visszajelzés (Change String Value), max. 25 látható karakter
STR_MSG_DYN = P.out_str(P.new_id(11000), " " * 32, 300, 18, F_LABEL, name="STR_MSG_DYN")
STR_YES = S("IGEN", 120, 22, F_LABEL, name="STR_YES")
STR_NO = S("NEM", 120, 22, F_LABEL, name="STR_NO")
STR_ON = S("BE", 60, 22, F_LABEL, name="STR_ON")
STR_OFF = S("KI", 60, 22, F_LABEL, name="STR_OFF")
STR_CLOSED = S("zárva", 88, 22, F_LABEL, name="STR_CLOSED")
STR_OPEN = S("nyitva", 88, 22, F_LABEL, name="STR_OPEN")
STR_DOWN = S("LENT", 88, 22, F_LABEL, name="STR_DOWN")
STR_UP = S("FENT", 88, 22, F_LABEL, name="STR_UP")
STR_EMPTY = S("ÜRES", 88, 22, F_LABEL, name="STR_EMPTY")
STR_FULL = S("TELE", 88, 22, F_LABEL, name="STR_FULL")
STR_TC_ON = S("Kapcsolódva", 180, 22, F_LABEL, name="STR_TC_ON")
STR_TC_OFF = S("Nincs kapcsolat", 180, 22, F_LABEL, name="STR_TC_OFF")
STR_TC_DIS = S("Kikapcsolva", 180, 22, F_LABEL, name="STR_TC_DIS")
STR_AUTO = S("Automata", 180, 22, F_LABEL, name="STR_AUTO")
STR_MANUAL = S("Kézi", 180, 22, F_LABEL, name="STR_MANUAL")
STR_METER = [S("1. adagoló", 150, 22, F_LABEL, name="STR_METER1"),
             S("2. adagoló", 150, 22, F_LABEL, name="STR_METER2")]
STR_SETUP = [S("A (nyitott)", 112, 14, F_SMALL, name="STR_SETUP_A"),
             S("Z + NORMÁL", 112, 14, F_SMALL, name="STR_SETUP_ZN"),
             S("Z + MIKRO", 112, 14, F_SMALL, name="STR_SETUP_ZM")]
NOTE_TEXTS = ["", "Kívánt menny. a táblázat felett!", "Kívánt menny. a táblázat alatt!",
              "Próbaskála a táblázaton kívül!", "Új skála max: nagyobb kerék kell",
              "Új skála min: aprómag-beállítás", "Állítsd be, forgasd le, mérd le"]
STR_NOTE = [S(t, 388, 18, F_LABEL_R if 0 < i < 6 else F_LABEL, name=f"STR_NOTE_{i}") for i, t in enumerate(NOTE_TEXTS)]
CROPS = ["Búza", "Rozs", "Árpa", "Zab", "Lóbab", "Borsó", "Csillagfürt", "Bükköny", "Kukorica", "Fû",
         "Repce", "Vöröshere", "Fû aprómag", "Tarlórépa"]
TURNS = ["85", "42,5", "17"]

# ================================================================== FŐKÉPERNYŐ
TANK_SIZE = 140
TANK_CROP_L = (8, 12, 116, 112)
TANK_CROP_R = (12, 12, 120, 112)
PIC_TANK_L = [PIC("main/main_tank_left_empty", TANK_SIZE, TANK_CROP_L), PIC("main/main_tank_left_full", TANK_SIZE, TANK_CROP_L)]
PIC_TANK_R = [PIC("main/main_tank_right_empty", TANK_SIZE, TANK_CROP_R), PIC("main/main_tank_right_full", TANK_SIZE, TANK_CROP_R)]
TW = P.objs[PIC_TANK_L[0]].meta["w"]
METER_SIZE = 76
METER_VIEWS = ["main/main_meter_ok", "main/main_meter_work", "main/main_meter_shut_down", "main/main_meter_alarm",
               "main/main_meter_idle"]
PIC_METER = [PIC(v, METER_SIZE) for v in METER_VIEWS]
PIC_WORK = [PIC("main/main_work_off", METER_SIZE), PIC("main/main_work_on", METER_SIZE)]
PIC_PREFILL = PIC("main/main_prefill", METER_SIZE)
BIG = 60
PIC_FAN = [PIC("main/main_fan_ok", BIG), PIC("main/main_fan_alarm", BIG)]
PIC_SPEED = PIC("main/main_speed", BIG)
SMALL = 40
PIC_LAMP_S = [PIC("main/main_lamp_off", SMALL), PIC("main/main_lamp_on", SMALL)]
PIC_VALVE_S = [PIC("main/main_fold_on", SMALL), PIC("main/main_marker_on", SMALL)]
PIC_COOLER_S = [PIC("main/main_cooler_off", SMALL), PIC("main/main_cooler_on", SMALL)]
PIC_SEC_S = [PIC("softkeys/softkey_tc_off", SMALL), PIC("softkeys/softkey_tc_on", SMALL)]

main_kids = []
add = main_kids.append
# tartály (két rekesz), középen
tank_x = (MASK - 2 * TW) // 2
add((P.obj_ptr(27100, PIC_TANK_L[1], "OP_TANK_L"), tank_x, 2))
add((P.obj_ptr(27101, PIC_TANK_R[1], "OP_TANK_R"), tank_x + TW, 2))
# adagolók + munkahelyzet
MY = 116
add((P.obj_ptr(27102, PIC_METER[4], "OP_METER1"), 30, MY))
add((P.obj_ptr(27104, PIC_WORK[0], "OP_WORK"), (MASK - METER_SIZE) // 2, MY))
METER2_CONT = C(METER_SIZE, METER_SIZE, [(P.obj_ptr(27103, PIC_METER[4], "OP_METER2"), 0, 0)], name="CONT_METER2")
add((METER2_CONT, MASK - 30 - METER_SIZE, MY))
# adagolónkénti mennyiség
VY = MY + METER_SIZE + 2
add((N("OUT_ACT1", 100, F_VALUE, decimals=1, scale=0.1, align=1), 18, VY))
add((S("kg/ha", 60, 14, F_SMALL), 118, VY + 8))
add((S("cél", 32, 14, F_SMALL), 18, VY + 28))
add((N("OUT_TGT1", 70, F_LABEL, decimals=1, scale=0.1, align=0), 52, VY + 26))
RATE2_CONT = C(160, 48, [(N("OUT_ACT2", 100, F_VALUE, decimals=1, scale=0.1, align=1), 0, 0),
                         (S("kg/ha", 60, 14, F_SMALL), 100, 8), (S("cél", 32, 14, F_SMALL), 0, 28),
                         (N("OUT_TGT2", 70, F_LABEL, decimals=1, scale=0.1, align=0), 34, 26)], name="CONT_RATE2")
add((RATE2_CONT, MASK - 164, VY))
# középen: eltérés % és feltöltés visszaszámlálás
add((N("OUT_OFFSET", 44, F_LABEL, offset=-100, value=100, align=2), 160, VY + 4))
add((S("%", 14, 16, F_LABEL), 206, VY + 4))
PREFILL_CONT = C(80, 22, [(N("OUT_PREFILL", 50, F_LABEL, decimals=1, scale=0.1, align=2), 0, 0),
                          (S("s", 14, 16, F_LABEL), 52, 0)], hidden=True, name="CONT_PREFILL")
add((PREFILL_CONT, 160, VY + 26))
LINK_CONT = C(80, 22, [(S("1 = 2", 80, 20, F_VALUE_N_S, align=1), 0, 0)], hidden=True, name="CONT_LINK")
add((LINK_CONT, 160, VY + 26))
# állapotsor
SY = 244
add((R(MASK - 8, 34, FL_MSG, L_BLACK), 4, SY))
add((P.obj_ptr(27105, MSG["off"], "OP_MSG"), 12, SY + 4))
add((N("OUT_AREA_DAY", 82, F_LABEL, decimals=2, scale=0.01), MASK - 120, SY + 9))
add((S("ha", 24, 16, F_LABEL), MASK - 36, SY + 9))
# turbina, sebesség
BY = 284
add((P.obj_ptr(27106, PIC_FAN[0], "OP_FAN"), 6, BY))
add((N("OUT_FAN_RPM", 112, F_BIG, align=2), 68, BY + 4))
add((S("1/min", 50, 14, F_SMALL), 130, BY + 40))
add((PIC_SPEED, 206, BY))
add((N("OUT_SPEED", 104, F_BIG, decimals=1, scale=0.1, align=2), 268, BY + 4))
add((S("km/h", 50, 14, F_SMALL), 330, BY + 40))
# kiegészítők sora (a funkció engedélyezésétől függően rejtett konténerek)
XY = 354
x = 2
PRESS_CONT = C(120, 44, [(PIC("main/main_pressure", SMALL), 0, 2), (N("OUT_PRESSURE", 50, F_LABEL, align=2), 40, 4),
                          (S("bar", 30, 14, F_SMALL), 92, 8)], hidden=True, name="CONT_PRESSURE")
add((PRESS_CONT, x, XY))
x += 120
TEMP_CONT = C(100, 44, [(PIC("main/main_temp", SMALL), 0, 2), (N("OUT_TEMP", 40, F_LABEL, align=2), 38, 4),
                         (S("°C", 22, 14, F_SMALL), 80, 8)], hidden=True, name="CONT_TEMP")
add((TEMP_CONT, x, XY))
x += 100
COOLER_CONT = C(SMALL, 44, [(P.obj_ptr(27107, PIC_COOLER_S[0], "OP_COOLER"), 0, 2)], hidden=True, name="CONT_COOLER")
add((COOLER_CONT, x, XY))
x += SMALL + 4
LAMPS_CONT = C(2 * SMALL + 4, 44, [(P.obj_ptr(27108, PIC_LAMP_S[0], "OP_LAMP1"), 0, 2),
                                   (P.obj_ptr(27109, PIC_LAMP_S[0], "OP_LAMP2"), SMALL + 4, 2)],
               hidden=True, name="CONT_LAMPS")
add((LAMPS_CONT, x, XY))
x += 2 * SMALL + 8
VALVE_CONT = C(SMALL, 44, [(P.obj_ptr(27110, PIC_VALVE_S[0], "OP_VALVE"), 0, 2)], hidden=True, name="CONT_VALVE")
add((VALVE_CONT, x, XY))
add((P.obj_ptr(27111, PIC_SEC_S[0], "OP_SEC"), MASK - SMALL - 4, XY + 2))
DM_MAIN = P.data_mask(1000, SK_MAIN[(2, 0)], main_kids, name="DM_MAIN")

# ================================================================== SZÁMLÁLÓK
info = title_bar("SZÁMLÁLÓK")
info += [(S("NAPI", 128, 26, F_VALUE, align=2), 136, 46), (S("ÖSSZES", 128, 26, F_VALUE, align=2), 268, 46)]
rows = [("Terület", "ha", "AREA", 2), ("Megtett út", "km", "DIST", 2), ("Vetési idõ", "óra", "TIME", 1),
        ("1. adagoló", "kg", "KG1", 0), ("2. adagoló", "kg", "KG2", 0)]
for r_i, (lab, unit, key, dec) in enumerate(rows):
    y = 82 + r_i * 44
    info += [(S(lab, font=F_LABEL), 6, y - 2), (S(unit, font=F_SMALL), 6, y + 18),
             (N(f"OUT_DAY_{key}", 128, F_VALUE, decimals=dec, scale=10 ** -dec), 136, y),
             (N(f"OUT_TOT_{key}", 128, F_VALUE, decimals=dec, scale=10 ** -dec), 268, y)]
info += [(R(MASK - 8, 2, FL_BLACK, L_BLACK, 0x0F), 4, 306),
         (S("A napi számlálót a piros X gomb törli.", font=F_SMALL), 6, 316),
         (S("Az összesítõ nem törölhetõ.", font=F_SMALL), 6, 334)]
DM_INFO = P.data_mask(1020, SK_INFO, info, name="DM_INFO")

# ================================================================== MENÜ
menu = title_bar("BEÁLLÍTÁSOK")
menu_items = ["calib"] + [p for p, _ in pd.VT_PAGES] + ["info"]
MENU_LABEL = {**{k: v[0] for k, v in PAGE_INFO.items()}, "calib": "Leforgatás", "info": "Számlálók"}
MENU_ICON = {**{k: v[1] for k, v in PAGE_INFO.items()}, "calib": "softkeys/softkey_crank", "info": "menu/menu_counters"}
BTN = {}
rows_n = (len(menu_items) + 1) // 2
bh = (MASK - 44) // rows_n
for i, pid in enumerate(menu_items):
    col, row = i % 2, i // 2
    pic = PIC(MENU_ICON[pid], 36)
    b = P.button(6000 + i, 192, bh - 4, 100 + i, [(pic, 4, (bh - 4 - 36) // 2 - 2),
                                                 (S(MENU_LABEL[pid], 140, 20, F_LABEL), 44, (bh - 4 - 16) // 2 - 3)],
                 name="BTN_" + pid.upper())
    BTN[pid] = b
    menu.append((b, 4 + col * 198, 42 + row * bh))
DM_MENU = P.data_mask(1030, SK_MENU, menu, name="DM_MENU")

# ================================================================== BEÁLLÍTÓOLDALAK
VT_OPTION_SHORT = {
    "meter_layout": ("Bal / jobb", "Elsõ / hátsó"),
    "drive_type": ("Talaj+kuplung", "El. motor"),
    "clutch_mode": ("Áram: hajt", "Áram: old"),
    "speed_src": ("Kerékjeladó", "ISOBUS", "ISOBUS+kerék", "Szimulált"),
    "isobus_speed_pref": ("Automatikus", "Föld/radar", "Trakt. kerék", "Gép választ.", "NMEA GPS"),
    "work_sw": ("Nincs", "NO lent zár", "NC lent bont"),
    "hopper1_nc": ("NO", "NC"),
    "hopper2_nc": ("NO", "NC"),
    "pressure_signal": ("0-5 V", "0,5-4,5 V", "0,5-5 V", "0-4,5 V"),
    "cooler_src": ("Olajtartály", "Hûtõ kimenet", "Melegebb"),
    "hitch_type": ("Függesztett", "Kormányzott", "Vonórúd", "Vonóhorog", "Gömbfej K80"),
}


def raw_scale(step, vmin, vmax):
    """Mérnöki érték ↔ VT nyers érték: kijelzett = (nyers + offset) × skála."""
    offset = round(vmin / step) if vmin < 0 else 0
    return step, offset, round(vmax / step) - offset, round(vmin / step) - offset


ROW_X_LABEL, ROW_X_INPUT, ROW_W_INPUT, ROW_X_UNIT = 6, 214, 136, 354
ROW_W_LIST = 180
CUSTOM_INPUTS = {}  # név → (id, step, min, max, decimals)


def input_number(ident, step, vmin, vmax, decimals, value, h, name=None, w=ROW_W_INPUT):
    scale, offset, rmax, rmin = raw_scale(step, vmin, vmax)
    raw = round(value / step) - offset
    font = F_INPUT if h >= 28 else F_INPUT_S
    return P.in_num(ident, w, h, font, max(rmin, min(rmax, raw)), rmin, rmax, scale, offset, decimals, name=name)


def label(text, h):
    assert 12 * len(text) <= 206, f"túl hosszú felirat: {text!r}"
    return (S(text, 206, 18, F_LABEL), ROW_X_LABEL, (h - 16) // 2)


def param_row(p: pd.Param, index: int, h: int):
    ident = VT_INPUT_BASE + index
    kids = [label(p.vt_label, h)]
    if p.kind in ("enum", "bool"):
        opts = VT_OPTION_SHORT.get(p.key, p.options)
        items = [S(o, ROW_W_LIST - 18, 18, F_INPUT_S) for o in opts]
        P.in_list(ident, ROW_W_LIST, h - 6, items, int(p.default))
    else:
        input_number(ident, p.step, p.min, p.max, p.decimals, float(p.default), h - 6)
    kids.append((ident, ROW_X_INPUT, 3))
    if p.unit:
        kids.append((S(p.unit, 46, 14, F_SMALL), ROW_X_UNIT, (h - 12) // 2))
    return kids


def custom_row(text, name, step, vmin, vmax, decimals, unit, h, value=0.0):
    ident = P.new_id(9500)
    CUSTOM_INPUTS[name] = (ident, step, vmin, vmax, decimals)
    input_number(ident, step, vmin, vmax, decimals, value, h - 6, name=name)
    kids = [label(text, h), (ident, ROW_X_INPUT, 3)]
    if unit:
        kids.append((S(unit, 46, 14, F_SMALL), ROW_X_UNIT, (h - 12) // 2))
    return kids


def custom_list(text, name, items, h):
    ident = P.new_id(9500)
    CUSTOM_INPUTS[name] = (ident, 1, 0, len(items) - 1, 0)
    P.in_list(ident, ROW_W_LIST, h - 6, [S(o, ROW_W_LIST - 18, 18, F_INPUT_S) for o in items], 0, name=name)
    return [label(text, h), (ident, ROW_X_INPUT, 3)]


def output_row(text, child, unit, h, x=ROW_X_INPUT):
    assert ROW_X_LABEL + 12 * len(text) <= x + 2, f"túl hosszú felirat: {text!r} (x={x})"
    kids = [label(text, h), (child, x, (h - 24) // 2)]
    if unit:
        kids.append((S(unit, 46, 14, F_SMALL), ROW_X_UNIT, (h - 12) // 2))
    return kids


def num_out(name, decimals=0, scale=1.0, offset=0):
    return N(name, ROW_W_INPUT, F_VALUE_N, decimals=decimals, scale=scale, offset=offset)


def heading(text, h):
    return [(R(MASK - 8, h - 6, FL_SILVER, L_NAVY2, 0x07), 4, 3), (S(text, 380, 18, F_LABEL), 10, (h - 16) // 2)]


PAGE_MASK = {}
PAGE_SK = {"speed": SK_WCAL, "scale": SK_SCALE, "pressure": SK_PRESSURE, "cooling": SK_COOLING}
param_index = {p.key: i for i, p in enumerate(pd.P)}
GEO_PICS = ["geo/geo_mounted", "geo/geo_steer_front", "geo/geo_steer_rear", "geo/geo_trailed_front",
            "geo/geo_trailed_rear"]
GEO_W = 380
GEO_CROP = (0, 58 * 128 / 240, 128, 176 * 128 / 240)
PIC_GEO = [PIC(g, GEO_W, GEO_CROP) for g in GEO_PICS]

for page_no, (pid, title) in enumerate(pd.VT_PAGES):
    params = [p for p in pd.P if p.vt_page == pid]
    extra = []
    if pid == "speed":
        extra = [lambda h: output_row("Sebesség most", num_out("OUT_SPEED2", 1, 0.1), "km/h", h),
                 lambda h: heading("KERÉKKALIBRÁLÁS (pl. 100 m)", h),
                 lambda h: custom_row("Mért távolság", "VT_IN_WCAL_DIST", 1, 10, 1000, 0, "m", h, 100),
                 lambda h: custom_row("Számolt impulzus", "VT_IN_WCAL_PULSES", 1, 0, 1000000, 0, "imp", h),
                 lambda h: output_row("Eredmény", num_out("OUT_WCAL_RES", 1, 0.1), "/100m", h),
                 lambda h: output_row("Állapot", P.obj_ptr(27120, CAL_STATE[0], "OP_WCAL_STATE"), "", h)]
    elif pid == "fan":
        extra = [lambda h: output_row("Fordulat most", num_out("OUT_FAN_RPM2"), "1/min", h)]
    elif pid == "switches":
        extra = [lambda h: heading("BEMENETEK ÁLLAPOTA", h),
                 lambda h: output_row("Munkakapcsoló", C(186, 28, [(P.obj_ptr(27121, STR_OPEN, "OP_WORK_CONTACT"), 0, 0),
                                                                    (P.obj_ptr(27122, STR_UP, "OP_WORK_STATE"), 92, 0)]),
                                      "", h),
                 lambda h: output_row("1. tartály", C(186, 28, [(P.obj_ptr(27123, STR_OPEN, "OP_HOP1_CONTACT"), 0, 0),
                                                                 (P.obj_ptr(27124, STR_FULL, "OP_HOP1_STATE"), 92, 0)]),
                                      "", h),
                 lambda h: output_row("2. tartály", C(186, 28, [(P.obj_ptr(27125, STR_OPEN, "OP_HOP2_CONTACT"), 0, 0),
                                                                 (P.obj_ptr(27126, STR_FULL, "OP_HOP2_STATE"), 92, 0)]),
                                      "", h)]
    elif pid == "pressure":
        extra = [lambda h: output_row("Jel most", num_out("OUT_PRESS_V", 3, 0.001), "V", h),
                 lambda h: output_row("Nyomás most", num_out("OUT_PRESSURE2", 1, 0.1), "bar", h),
                 lambda h: heading("KALIBRÁLÁS ISMERT NYOMÁSON", h),
                 lambda h: custom_row("Ismert nyomás", "VT_IN_PRESS_REF", 0.5, 0, 1000, 1, "bar", h, 50)]
    elif pid == "cooling":
        extra = [lambda h: output_row("Olajtartály", num_out("OUT_TEMP1", 1, 0.1, -1000), "°C", h),
                 lambda h: output_row("Hûtõ kimenet", num_out("OUT_TEMP2", 1, 0.1, -1000), "°C", h),
                 lambda h: output_row("Ventilátor", P.obj_ptr(27127, STR_OFF, "OP_COOLER_STATE"), "", h),
                 lambda h: output_row("Üzemmód", P.obj_ptr(27128, STR_AUTO, "OP_COOLER_MODE"), "", h)]
    elif pid == "scale":
        extra = [lambda h: output_row("Nettó tömeg", num_out("OUT_SCALE_KG", 3, 0.001, -100000), "kg", h),
                 lambda h: output_row("Nyers érték", N("OUT_SCALE_RAW", ROW_W_INPUT, F_VALUE_N, offset=-8388608), "", h),
                 lambda h: heading("Üres vödör: TÁRA, súly: MENTÉS", h),
                 lambda h: custom_row("Ismert súly", "VT_IN_SCALE_REF", 0.01, 0, 500, 2, "kg", h, 10)]
    elif pid == "tc":
        extra = [lambda h: output_row("TC kapcsolat", P.obj_ptr(27129, STR_TC_OFF, "OP_TC_STATE"), "", h),
                 lambda h: output_row("Szakaszvezérlés", P.obj_ptr(27130, STR_MANUAL, "OP_TC_MODE"), "", h)]
    elif pid == "diag":
        extra = [lambda h: heading("KIMENETEK / BEMENETEK", h),
                 lambda h: output_row("Kuplung 1 / 2", C(186, 28, [(P.obj_ptr(27131, STR_OFF, "OP_DIAG_CL1"), 0, 0),
                                                                    (P.obj_ptr(27132, STR_OFF, "OP_DIAG_CL2"), 70, 0)]),
                                      "", h),
                 lambda h: output_row("Motor 1 PWM", num_out("OUT_DIAG_PWM1", 1, 0.1), "%", h),
                 lambda h: output_row("Motor 2 PWM", num_out("OUT_DIAG_PWM2", 1, 0.1), "%", h),
                 lambda h: output_row("Adag.1/2 [1/min]", C(186, 28, [(N("OUT_DIAG_RPM1", 88, F_VALUE_N, 1, 0.1), 0, 0),
                                                                    (N("OUT_DIAG_RPM2", 88, F_VALUE_N, 1, 0.1), 96, 0)]),
                                      "", h),
                 lambda h: output_row("Kerék/turb. [Hz]", C(186, 28, [(N("OUT_DIAG_WHEELHZ", 88, F_VALUE_N, 1, 0.1), 0, 0),
                                                                      (N("OUT_DIAG_FANHZ", 88, F_VALUE_N, 1, 0.1), 96, 0)]),
                                      "", h)]
    nrows = len(params) + len(extra)
    reserve = P.objs[PIC_GEO[0]].meta["h"] + 4 if pid == "geometry" else 0
    h = max(28, min(44, (MASK - 42 - reserve) // max(1, nrows)))
    kids = title_bar(title)
    y = 42
    for p in params:
        kids += [(cid, x, y + yy) for cid, x, yy in param_row(p, param_index[p.key], h)]
        y += h
    for fn in extra:
        kids += [(cid, x, y + yy) for cid, x, yy in fn(h)]
        y += h
    if pid == "geometry":
        kids.append((P.obj_ptr(27133, PIC_GEO[3], "OP_GEO"), (MASK - GEO_W) // 2, y + 2))
        y += P.objs[PIC_GEO[0]].meta["h"] + 2
    assert y <= MASK + 2, (pid, y)
    PAGE_MASK[pid] = P.data_mask(1040 + page_no, PAGE_SK.get(pid, SK_BACK), kids, name="DM_PAGE_" + pid.upper())

# ================================================================== LEFORGATÁS
cal = title_bar("LEFORGATÁS (ACCORD)")
h = 32
y = 42
cal_rows = [
    lambda: output_row("Adagoló", P.obj_ptr(27140, STR_METER[0], "OP_CAL_METER"), "", h),
    lambda: custom_list("Növény", "VT_IN_CAL_CROP", CROPS, h),
    lambda: custom_row("Kívánt mennyiség", "VT_IN_CAL_RATE", 0.5, 0.5, 1000, 1, "kg/ha", h, 200),
    lambda: output_row("Táblázati skála", C(186, 28, [(N("OUT_CAL_SCALE", 64, F_VALUE_N, 1, 0.1), 0, 0),
                                                      (P.obj_ptr(27141, STR_SETUP[0], "OP_CAL_SETUP"), 70, 6)]),
                       "", h),
    lambda: custom_list("Fordulat", "VT_IN_CAL_TURNS", TURNS, h),
    lambda: custom_row("Próbaskála", "VT_IN_CAL_TSCALE", 0.1, 0, 200, 1, "", h),
    lambda: output_row("Várható tömeg", num_out("OUT_CAL_EXPECT", 3, 0.001), "kg", h),
    lambda: custom_row("Mért tömeg", "VT_IN_CAL_MEAS", 0.001, 0, 500, 3, "kg", h),
    lambda: output_row("Új skála, ford.", C(186, 28, [(N("OUT_CAL_NEW", 64, F_VALUE_N, 1, 0.1), 0, 0),
                                                      (N("OUT_CAL_REVS", 80, F_VALUE_N, 1, 0.1), 80, 0)]),
                       "", h),
    lambda: output_row("Kg / fordulat", num_out("OUT_CAL_KGREV", 4, 0.0001), "kg", h),
    lambda: output_row("", P.obj_ptr(27142, STR_NOTE[6], "OP_CAL_NOTE"), "", h, x=6),
]
for fn in cal_rows:
    cal += [(cid, x, y + yy) for cid, x, yy in fn()]
    y += h
assert y <= MASK + 2, y
DM_CALIB = P.data_mask(1060, SK_CALIB_GROUND, cal, name="DM_CALIB")

# ================================================================== RIASZTÁS
alarm_kids = [(PIC("main/main_alarm", 120, bg=WHITE), (MASK - 120) // 2, 40),
              (P.obj_ptr(27150, STR_ALARM_BIG[0], "OP_ALARM_TEXT"), 10, 190),
              (S("Nyugtázás: pipa gomb", 380, 20, F_LABEL, bg=WHITE), 10, 300)]
DM_ALARM = P.alarm_mask(1100, SK_ALARM, alarm_kids, name="AM_ALARM")

# ================================================================== WORKING SET
P.working_set(0, DM_MAIN, [(PIC("main/main_esc_logo_small", 60), 0, 0)])
P.names["WORKING_SET"] = 0


# ------------------------------------------------------------------ C++ fejléc
def pool_version(data: bytes) -> str:
    """7 karakteres verziócímke a pool tartalmából: ha bármi változik, a VT nem a régi tárolt poolt tölti be."""
    import zlib
    return "SD" + format(zlib.crc32(data) & 0xFFFFF, "05X")


ARRAYS = {
    "PIC_TANK_L": PIC_TANK_L, "PIC_TANK_R": PIC_TANK_R, "PIC_METER": PIC_METER, "PIC_WORK": PIC_WORK,
    "PIC_FAN": PIC_FAN, "PIC_LAMP_S": PIC_LAMP_S, "PIC_VALVE_S": PIC_VALVE_S, "PIC_COOLER_S": PIC_COOLER_S,
    "PIC_SEC_S": PIC_SEC_S, "PIC_GEO": PIC_GEO,
    "PIC_K_MASTER": PIC_K_MASTER, "PIC_K_METER1": PIC_K_METER1, "PIC_K_METER2": PIC_K_METER2, "PIC_K_SEC": PIC_K_SEC,
    "PIC_K_PREFILL": PIC_K_PREFILL, "PIC_K_MODE": PIC_K_MODE, "PIC_K_LAMP1": PIC_K_LAMP1, "PIC_K_LAMP2": PIC_K_LAMP2,
    "PIC_K_VALVE": PIC_K_VALVE, "PIC_K_COOLER": PIC_K_COOLER, "PIC_K_CAL_METER": PIC_K_CAL_METER,
    "SK_FUNC": SK_FUNC, "STR_ALARM": STR_ALARM, "STR_ALARMBIG": STR_ALARM_BIG, "STR_CAL": CAL_STATE,
    "STR_METER": STR_METER, "STR_SETUP": STR_SETUP, "STR_NOTE": STR_NOTE, "K_BLANK": K_BLANK,
}


def header_text(pool_bytes: bytes) -> str:
    lines = ["// GENERÁLT FÁJL – ne szerkeszd kézzel! Forrás: tools/pool/generate_pool.py",
             "#pragma once", "#include <cstdint>", "", "namespace vt {",
             f"constexpr std::uint32_t DESIGN_MASK_PX = {MASK};",
             f"constexpr std::uint32_t DESIGN_SOFTKEY_PX = {SOFTKEY};",
             f"constexpr std::uint32_t POOL_SIZE = {len(pool_bytes)};",
             f'constexpr const char *POOL_VERSION = "{pool_version(pool_bytes)}"; // a VT ez alapján cache-el',
             f"constexpr std::uint16_t VT_INPUT_BASE = {VT_INPUT_BASE};", ""]
    for name, ident in sorted(P.names.items(), key=lambda kv: kv[1]):
        lines.append(f"constexpr std::uint16_t {name} = {ident};")
    lines.append("")
    for aname, ids in ARRAYS.items():
        lines.append(f"constexpr std::uint16_t {aname}[{len(ids)}] = {{{', '.join(map(str, ids))}}};")
    lines.append("/// főképernyő softkey maszk: [adagolók-1][elektromos]")
    lines.append(f"constexpr std::uint16_t SK_MAIN[2][2] = {{{{{SK_MAIN[(1, 0)]}, {SK_MAIN[(1, 1)]}}}, "
                 f"{{{SK_MAIN[(2, 0)]}, {SK_MAIN[(2, 1)]}}}}};")
    lines.append("")
    lines.append("struct PageInfo { const char *id; std::uint16_t mask; std::uint16_t button; std::uint16_t key; };")
    lines.append(f"constexpr PageInfo PAGES[{len(pd.VT_PAGES)}] = {{")
    for pid, _ in pd.VT_PAGES:
        lines.append(f'    {{"{pid}", {PAGE_MASK[pid]}, {BTN[pid]}, {K_PAGE[pid]}}},')
    lines.append("};")
    lines.append(f"constexpr std::uint16_t BTN_PAGE_CALIB = {BTN['calib']};")
    lines.append(f"constexpr std::uint16_t BTN_PAGE_INFO = {BTN['info']};")
    lines.append("")
    lines.append("// Egyedi (nem paraméter) beviteli mezők: id, lépés, min, max")
    lines.append("struct CustomInput { std::uint16_t id; float step, min, max; };")
    for name, (ident, step, vmin, vmax, dec) in CUSTOM_INPUTS.items():
        lines.append(f"constexpr CustomInput {name}_DEF = {{{ident}, {float(step)!r}f, {float(vmin)!r}f, "
                     f"{float(vmax)!r}f}};")
    lines += ["} // namespace vt", ""]
    return "\n".join(lines)


def main():
    data = P.binary()
    hdr = header_text(data)
    if "--check" in sys.argv:
        ok = OUT_IOP.exists() and OUT_IOP.read_bytes() == data and OUT_HPP.exists() and \
            OUT_HPP.read_text(encoding="utf-8") == hdr
        print("object pool naprakész" if ok else "object_pool.iop / vt_ids.hpp ELAVULT – futtasd: "
              "python tools/pool/generate_pool.py", file=sys.stdout if ok else sys.stderr)
        return 0 if ok else 1
    OUT_IOP.parent.mkdir(parents=True, exist_ok=True)
    OUT_IOP.write_bytes(data)
    OUT_HPP.write_text(hdr, encoding="utf-8")
    pics = sum(1 for o in P.objs.values() if o.type == T_PIC)
    print(f"Object pool: {len(P.objs)} objektum ({pics} kép), {len(data)} bájt → {OUT_IOP.relative_to(ROOT)}")
    if "--preview" in sys.argv:
        import preview
        preview.render_all(sys.modules[__name__], PREVIEW_DIR)
    return 0


if __name__ == "__main__":
    sys.exit(main())

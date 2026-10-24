#!/usr/bin/env python3
"""Előnézeti képek a generált object poolból (docs/vt_preview/*.png).

Nem pixelpontos VT-emuláció (a betűk a terminálon a VT saját fix szélességű fontjai lesznek),
de az elrendezést, méreteket, ikonokat és színeket hűen mutatja. A softkey-ek a Müller BASIC
elrendezésben (jobb oszlop felülről az első 5, utána a bal oszlop), 120×80-as gombokkal jelennek meg.
"""
from __future__ import annotations

from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

G = None  # a generáló modul (render_all állítja be)

FONT_FILE = None
for cand in ("/usr/share/fonts/truetype/dejavu/DejaVuSansMono-Bold.ttf",
             "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf", "C:/Windows/Fonts/consola.ttf"):
    if Path(cand).exists():
        FONT_FILE = cand
        break
_font_cache = {}


def vt_font(size_code):
    w, h = G.FONT_PX[size_code]
    if (w, h) not in _font_cache:
        if FONT_FILE:
            _font_cache[(w, h)] = ImageFont.truetype(FONT_FILE, max(6, int(min(h * 0.95, w / 0.6))))
        else:
            _font_cache[(w, h)] = ImageFont.load_default()
    return _font_cache[(w, h)]


def colour(i):
    return G.PALETTE[i]


class Renderer:
    def __init__(self, pool, overrides=None):
        self.p = pool
        self.ov = overrides or {}

    def val(self, ident, default):
        return self.ov.get(ident, default)

    def text(self, d, x, y, w, h, s, font_id, align):
        meta = self.p.objs[font_id].meta
        f = vt_font(meta["size"])
        fw, fh = G.FONT_PX[meta["size"]]
        s = s.translate(G.LATIN1_MAP)
        tw = d.textlength(s, font=f)
        tx = x if align == 0 else (x + (w - tw) / 2 if align == 1 else x + w - tw)
        d.text((tx, y + (fh - f.size) / 2), s, fill=colour(meta["colour"]), font=f)

    def draw(self, img, d, ident, x, y):
        if ident == G.NULL:
            return
        o = self.p.objs[ident]
        m = o.meta
        t = o.type
        if t == G.T_RECT:
            w, h = m["w"], m["h"]
            fill = self.ov.get(("fill", m["fill"]), self.p.objs[m["fill"]].meta["colour"])
            d.rectangle([x, y, x + w - 1, y + h - 1], fill=colour(fill))
            lc = colour(self.p.objs[m["line"]].meta["colour"])
            lw = self.p.objs[m["line"]].meta["width"]
            s = m["suppress"]
            if not s & 1: d.line([x, y, x + w - 1, y], fill=lc, width=lw)
            if not s & 2: d.line([x + w - 1, y, x + w - 1, y + h - 1], fill=lc, width=lw)
            if not s & 4: d.line([x, y + h - 1, x + w - 1, y + h - 1], fill=lc, width=lw)
            if not s & 8: d.line([x, y, x, y + h - 1], fill=lc, width=lw)
        elif t == G.T_OUTSTR:
            if not m["transparent"]:
                d.rectangle([x, y, x + m["w"] - 1, y + m["h"] - 1], fill=colour(m["bg"]))
            self.text(d, x, y, m["w"], m["h"], m["text"], m["font"], m["align"] & 3)
        elif t == G.T_OUTNUM:
            v = (self.val(ident, m["value"]) + m["offset"]) * m["scale"]
            self.text(d, x, y, m["w"], m["h"], f"{v:.{m['decimals']}f}", m["font"], m["align"] & 3)
        elif t == G.T_INNUM:
            d.rectangle([x, y, x + m["w"] - 1, y + m["h"] - 1], fill=colour(1), outline=colour(8))
            v = (self.val(ident, m["value"]) + m["offset"]) * m["scale"]
            self.text(d, x + 4, y + 1, m["w"] - 8, m["h"], f"{v:.{m['decimals']}f}", m["font"], 2)
        elif t == G.T_INLIST:
            d.rectangle([x, y, x + m["w"] - 1, y + m["h"] - 1], fill=colour(1), outline=colour(8))
            self.draw(img, d, m["items"][self.val(ident, m["value"])], x + 4, y + 2)
            d.polygon([(x + m["w"] - 14, y + 8), (x + m["w"] - 4, y + 8), (x + m["w"] - 9, y + 14)], fill=colour(0))
        elif t == G.T_PIC:
            im = Image.new("RGB", (m["w"], m["h"]))
            im.putdata([G.PALETTE[i] for i in m["pix"]])
            img.paste(im, (int(x), int(y)))
        elif t == G.T_OBJPTR:
            self.draw(img, d, self.val(ident, m["value"]), x, y)
        elif t == G.T_CONTAINER:
            if self.ov.get(("hidden", ident), m["hidden"]):
                return
            for c, cx, cy in m["kids"]:
                self.draw(img, d, c, x + cx, y + cy)
        elif t == G.T_BUTTON:
            d.rectangle([x, y, x + m["w"] - 1, y + m["h"] - 1], fill=colour(m["bg"]), outline=colour(0), width=2)
            for c, cx, cy in m["kids"]:
                self.draw(img, d, c, x + cx, y + cy)

    def render_mask(self, mask_id, path, softkey_override=None):
        """BASIC elrendezés: [bal softkey oszlop][maszk 400×400][jobb softkey oszlop], 120×80-as gombok."""
        o = self.p.objs[mask_id]
        kw, kh = G.KEY_REAL_W, G.SOFTKEY
        img = Image.new("RGB", (G.MASK + 2 * kw + 8, G.MASK), colour(8))
        d = ImageDraw.Draw(img)
        mx = kw + 4
        d.rectangle([mx, 0, mx + G.MASK - 1, G.MASK - 1], fill=colour(o.meta["bg"]))
        mask = Image.new("RGB", (G.MASK, G.MASK), colour(o.meta["bg"]))
        md = ImageDraw.Draw(mask)
        for c, cx, cy in o.meta["kids"]:
            self.draw(mask, md, c, cx, cy)
        img.paste(mask, (mx, 0))
        sk = softkey_override or o.meta["sk"]
        if sk != G.NULL:
            scale = kw / G.SOFTKEY
            for i, k in enumerate(self.p.objs[sk].meta["keys"][:10]):
                kx = (mx + G.MASK + 4) if i < 5 else 0
                ky = (i % 5) * kh
                d.rectangle([kx, ky + 1, kx + kw - 1, ky + kh - 2], fill=colour(G.BG), outline=colour(0))
                for c, cx, cy in self.p.objs[k].meta["kids"]:
                    self.draw(img, d, c, kx + round(cx * scale), ky + round(cy * scale))
        img.save(path)


def render_all(gen, outdir: Path):
    global G
    G = gen
    pool = gen.P
    outdir.mkdir(parents=True, exist_ok=True)
    n = pool.names
    ov = {
        n["OP_K_MASTER"]: G.PIC_K_MASTER[1], n["OP_K_METER1"]: G.PIC_K_METER1[1], n["OP_K_METER2"]: G.PIC_K_METER2[1],
        n["OP_K_SEC"]: G.PIC_K_SEC[1], n["OP_METER1"]: G.PIC_METER[1], n["OP_METER2"]: G.PIC_METER[1],
        n["OP_WORK"]: G.PIC_WORK[1], n["OP_TANK_R"]: G.PIC_TANK_R[0], n["OP_MSG"]: n["STR_MSG_SOWING"],
        n["OUT_ACT1"]: 1985, n["OUT_TGT1"]: 2000, n["OUT_ACT2"]: 1460, n["OUT_TGT2"]: 1500, n["OUT_OFFSET"]: 105,
        n["OUT_AREA_DAY"]: 1234, n["OUT_FAN_RPM"]: 3450, n["OUT_SPEED"]: 112, n["OUT_PRESSURE"]: 85,
        n["OUT_TEMP"]: 54, n["OP_COOLER"]: G.PIC_COOLER_S[1], n["OP_LAMP1"]: G.PIC_LAMP_S[1],
        n["OP_SEC"]: G.PIC_SEC_S[1], n["OUT_PREFILL"]: 25,
        ("hidden", n["CONT_PRESSURE"]): False, ("hidden", n["CONT_TEMP"]): False,
        ("hidden", n["CONT_COOLER"]): False, ("hidden", n["CONT_LAMPS"]): False, ("hidden", n["CONT_VALVE"]): False,
        n["OUT_DAY_AREA"]: 1234, n["OUT_TOT_AREA"]: 45678, n["OUT_DAY_DIST"]: 3856, n["OUT_TOT_DIST"]: 142730,
        n["OUT_DAY_TIME"]: 31, n["OUT_TOT_TIME"]: 1102, n["OUT_DAY_KG1"]: 2470, n["OUT_TOT_KG1"]: 91350,
        n["OUT_DAY_KG2"]: 1800, n["OUT_TOT_KG2"]: 60200,
        n["OUT_CAL_SCALE"]: 545, n["OUT_CAL_EXPECT"]: 20000, n["OUT_CAL_NEW"]: 552, n["OUT_CAL_REVS"]: 850,
        n["OUT_CAL_KGREV"]: 2353, n["OUT_SCALE_KG"]: 100000 + 12345, n["OUT_SCALE_RAW"]: 8388608 + 104000,
        n["OUT_SPEED2"]: 112, n["OUT_FAN_RPM2"]: 3450,
    }
    r = Renderer(pool, ov)
    r.render_mask(G.DM_MAIN, outdir / "01_fokepernyo.png", G.SK_MAIN[(2, 1)])
    r.render_mask(G.DM_MAIN, outdir / "02_funkciok.png", G.SK_FUNC[7])
    ov2 = dict(ov)
    ov2[n["OP_MSG"]] = n["STR_MSG_PREFILL"]
    ov2[("hidden", n["CONT_PREFILL"])] = False
    ov2[n["OP_WORK"]] = G.PIC_PREFILL
    ov2[("hidden", n["CONT_METER2"])] = True
    ov2[("hidden", n["CONT_RATE2"])] = True
    Renderer(pool, ov2).render_mask(G.DM_MAIN, outdir / "03_feltoltes_1adagolo.png", G.SK_MAIN[(1, 1)])
    r.render_mask(G.DM_INFO, outdir / "04_szamlalok.png")
    r.render_mask(G.DM_MENU, outdir / "05_menu.png")
    r.render_mask(G.DM_CALIB, outdir / "06_leforgatas.png", G.SK_CALIB)
    for k, (pid, _) in enumerate(G.pd.VT_PAGES):
        r.render_mask(G.PAGE_MASK[pid], outdir / f"{10 + k:02d}_beallitas_{pid}.png")
    r.render_mask(G.DM_ALARM, outdir / "90_riasztas.png")
    print(f"Előnézetek: {outdir}")

"""Az object pool szerkezeti ellenőrzése (Python unittest)."""
import sys
import unittest
from pathlib import Path
from struct import unpack_from

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "pool"))
sys.path.insert(0, str(ROOT / "tools"))
import generate_pool as G  # noqa: E402
import params_def as pd  # noqa: E402

MASK_TYPES = (G.T_DATAMASK, G.T_ALARMMASK)


def unrle(data):
    out = bytearray()
    for i in range(0, len(data), 2):
        out += bytes([data[i + 1]]) * data[i]
    return bytes(out)


class PoolStructure(unittest.TestCase):
    def test_aux_functions(self):
        # a Windows-program (DiagPage.AuxNames) a 29000-től folyamatos sorrendre épít
        self.assertEqual(G.AUX_IDS, list(range(29000, 29000 + len(G.AUX_FUNCS))))
        for i in G.AUX_IDS:
            o = G.P.objs[i]
            self.assertEqual(o.type, G.T_AUXFN2)
            self.assertEqual(o.body[1] & 0x1F, G.AUX_MOMENTARY)
            (c, _x, _y), = o.meta["kids"]
            self.assertIn(c, G._key_pics)

    def test_every_child_reference_exists(self):
        for o in G.P.objs.values():
            for c, _x, _y in o.meta.get("kids", []):
                self.assertIn(c, G.P.objs, f"{o.id} gyereke hiányzik: {c}")
            for k in o.meta.get("keys", []):
                self.assertEqual(G.P.objs[k].type, G.T_KEY, f"softkey maszk {o.id}: {k} nem Key")
            for i in o.meta.get("items", []):
                self.assertEqual(G.P.objs[i].type, G.T_OUTSTR)

    def test_object_pointers_target_existing_objects(self):
        for o in G.P.objs.values():
            if o.type == G.T_OBJPTR and o.meta["value"] != G.NULL:
                self.assertIn(o.meta["value"], G.P.objs)

    def test_pointer_alternatives_have_same_type(self):
        # a firmware a tömbökből választ: egy tömb minden eleme azonos típusú legyen
        for name, ids in G.ARRAYS.items():
            types = {G.P.objs[i].type for i in ids}
            self.assertEqual(len(types), 1, name)

    def test_mask_children_inside_design_area(self):
        for o in G.P.objs.values():
            if o.type not in MASK_TYPES:
                continue
            for c, x, y in o.meta["kids"]:
                m = G.P.objs[c].meta
                w, h = m.get("w", 0), m.get("h", 0)
                self.assertTrue(0 <= x and 0 <= y, (o.id, c))
                self.assertLessEqual(x + w, G.MASK + 2, f"maszk {o.id}: {c} kilóg vízszintesen")
                self.assertLessEqual(y + h, G.MASK + 2, f"maszk {o.id}: {c} kilóg függőlegesen")

    def test_container_children_inside_container(self):
        for o in G.P.objs.values():
            if o.type != G.T_CONTAINER:
                continue
            for c, x, y in o.meta["kids"]:
                m = G.P.objs[c].meta
                self.assertLessEqual(x + m.get("w", 0), o.meta["w"] + 2, (o.id, c))
                self.assertLessEqual(y + m.get("h", 0), o.meta["h"] + 2, (o.id, c))

    def test_key_content_fits_softkey(self):
        for o in G.P.objs.values():
            if o.type != G.T_KEY:
                continue
            for c, x, y in o.meta["kids"]:
                obj = G.P.objs[c]
                if obj.type == G.T_OBJPTR:
                    obj = G.P.objs[obj.meta["value"]]
                if obj.type == G.T_PIC:
                    self.assertLessEqual(x + obj.meta["w"], G.SOFTKEY)
                    self.assertLessEqual(y + obj.meta["h"], G.SOFTKEY)
                    # BASIC 120×80: a skálázott pozíció a gomb közepén legyen
                    sx = x * G.KEY_REAL_W / G.SOFTKEY
                    self.assertAlmostEqual(sx + obj.meta["w"] / 2, G.KEY_REAL_W / 2, delta=2)

    def test_picture_records_8bit(self):
        for o in G.P.objs.values():
            if o.type != G.T_PIC:
                continue
            w, aw, h, depth, opts, _tr, raw_len = unpack_from("<HHHBBBI", o.body, 0)
            self.assertEqual(w, aw, "a kép ne skálázódjon a VT-n")
            self.assertEqual(depth, 2, "8 bites kép")
            self.assertEqual(len(o.body), 14 + raw_len)
            data = o.body[14:]
            if opts & 0x04:
                self.assertEqual(unrle(data), o.meta["pix"])
            else:
                self.assertEqual(data, o.meta["pix"])
            self.assertEqual(len(o.meta["pix"]), w * h)
            self.assertLess(max(o.meta["pix"]), 256)

    def test_rle_edge_cases(self):
        self.assertEqual(G.rle8(b""), b"")
        self.assertEqual(G.rle8(b"\x05"), b"\x01\x05")
        long = b"\x07" * 600
        self.assertEqual(unrle(G.rle8(long)), long)
        self.assertEqual(G.rle8(long)[:2], b"\xff\x07")
        mixed = bytes(range(256)) * 2
        self.assertEqual(unrle(G.rle8(mixed)), mixed)

    def test_palette(self):
        self.assertEqual(len(G.PALETTE), 232)
        self.assertEqual(G.PALETTE[G.SILVER], (204, 204, 204))
        self.assertEqual(G.nearest((204, 204, 204)), G.SILVER)

    def test_every_vt_param_has_input_object(self):
        for i, p in enumerate(pd.P):
            if p.vt_page:
                obj = G.P.objs.get(G.VT_INPUT_BASE + i)
                self.assertIsNotNone(obj, p.key)
                self.assertIn(obj.type, (G.T_INNUM, G.T_INLIST))
                if obj.type == G.T_INLIST:
                    self.assertEqual(len(obj.meta["items"]), len(p.options), p.key)

    def test_numeric_input_ranges_cover_defaults(self):
        for o in G.P.objs.values():
            if o.type == G.T_INNUM:
                value, vmin, vmax = unpack_from("<III", o.body, 10)
                self.assertLessEqual(vmin, value)
                self.assertLessEqual(value, vmax)

    def test_unique_key_codes_and_working_set(self):
        codes = [o.body[1] for o in G.P.objs.values() if o.type == G.T_KEY]
        self.assertEqual(len(codes), len(set(codes)))
        self.assertEqual(G.P.objs[0].type, G.T_WS)

    def test_softkey_masks_fit_basic_terminal(self):
        # Müller BASIC: 10 fizikai gomb – a fontos maszkoknak ennyibe bele kell férniük
        for o in G.P.objs.values():
            if o.type == G.T_SKMASK:
                self.assertLessEqual(len(o.meta["keys"]), 10, o.id)

    def test_strings_are_latin1_and_fit(self):
        for o in G.P.objs.values():
            if o.type == G.T_OUTSTR:
                txt = o.meta["text"].translate(G.LATIN1_MAP).rstrip()
                txt.encode("latin-1")
                fw, fh = G.FONT_PX[G.P.objs[o.meta["font"]].meta["size"]]
                self.assertLessEqual(len(txt) * fw, o.meta["w"], f"nem fér el: {txt!r}")
                self.assertLessEqual(fh, o.meta["h"] + 4, f"alacsony: {txt!r}")

    def test_numbers_fit(self):
        widest = {"OUT_FAN_RPM": 4, "OUT_SPEED": 4, "OUT_ACT1": 5, "OUT_TOT_AREA": 8, "OUT_SCALE_KG": 7}
        for name, chars in widest.items():
            o = G.P.objs[G.P.names[name]]
            fw, _ = G.FONT_PX[G.P.objs[o.meta["font"]].meta["size"]]
            self.assertLessEqual(chars * fw, o.meta["w"], name)

    def test_pool_size_reasonable(self):
        self.assertLess(len(G.P.binary()), 400_000)


if __name__ == "__main__":
    unittest.main()

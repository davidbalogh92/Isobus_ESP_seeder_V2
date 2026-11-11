#!/usr/bin/env python3
"""Keya vetőgép-motor (Sowing Motor All-in-One) próbaprogram PEAK PCAN-USB adapterrel.

Telepítés (Windows):
  1. PEAK illesztőprogram (a PCAN-Basic DLL-t is felteszi): https://www.peak-system.com/ → Downloads → Drivers
  2. Python 3 (python.org), majd parancssorban:  pip install python-can
Indítás:
  python keya_test.py                  (500 kbit/s, node 1 – gyári beállítás)
  python keya_test.py --bitrate 250000 --node 2

Bekötés: CAN-H → motor 3-as láb, CAN-L → 4-es láb, GND közös a motor táp mínuszával.
Motor tápja külön: 1-es láb +12 V, 2-es láb −. Lezárás: az adapter 120 Ω-os kapcsolója BE.

Protokoll (a gyári leírás 6. fejezete): 11 bites ID = (node << 6) | parancs, adatok little-endian.
Minden küldött és fogadott keret a keya_log_<idő>.csv fájlba is kerül.
"""
import argparse
import csv
import struct
import sys
import threading
import time
from datetime import datetime

try:
    import can
except ImportError:
    sys.exit("Hiányzik a python-can csomag:  pip install python-can")

# parancsok (az ID alsó 6 bitje)
DISABLE, ENABLE, SET_TORQUE, SET_VEL, SET_POS, SYNC = 0x00, 0x01, 0x02, 0x03, 0x04, 0x05
SET_HOME, ERR_RESET, GET_STATUS, STATUS_REPORT = 0x0C, 0x0D, 0x0E, 0x0F
GET_TORQUE, GET_VEL, GET_POS, GET_IQ, GET_VBUS, GET_IBUS, GET_POWER = 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16
SET_PARAM, SAVE, HEARTBEAT = 0x17, 0x19, 0x3F

NAMES = {DISABLE: "letiltás", ENABLE: "engedélyezés", SET_TORQUE: "célnyomaték", SET_VEL: "célfordulat",
         SET_POS: "célpozíció", SYNC: "szinkron", SET_HOME: "nullpont", ERR_RESET: "hibatörlés",
         GET_STATUS: "állapot", STATUS_REPORT: "állapotjelentés", GET_TORQUE: "nyomaték [Nm]",
         GET_VEL: "fordulat [ford/s]", GET_POS: "pozíció [ford]", GET_IQ: "motoráram [A]",
         GET_VBUS: "tápfeszültség [V]", GET_IBUS: "buszáram [A]", GET_POWER: "teljesítmény [W]",
         SET_PARAM: "paraméter írás", SAVE: "mentés", HEARTBEAT: "szívverés"}
FLOAT_REPLIES = {GET_TORQUE, GET_VEL, GET_POS, GET_IQ, GET_VBUS, GET_IBUS, GET_POWER}

STATUS_BITS = {0: "engedélyezve", 1: "cél elérve", 2: "áramkorlátban"}
ERROR_BITS = {0: "ÖNTESZT HIBA (újraindítás kell)", 1: "JELADÓ KIESETT (újraindítás kell)",
              17: "TÚLFESZÜLTSÉG", 18: "ALACSONY FESZÜLTSÉG", 19: "TÚLÁRAM"}
BAUDS = {250: 0, 500: 1, 800: 2, 1000: 3}


def decode_status(data: bytes) -> str:
    if len(data) < 8:
        return data.hex(" ")
    # a leírás példája szerint az első 4 bájt az állapot, a második 4 a hiba (mindkettő little-endian)
    st, err = struct.unpack("<II", data[:8])
    s = [t for b, t in STATUS_BITS.items() if st >> b & 1] or ["letiltva"]
    e = [t for b, t in ERROR_BITS.items() if err >> b & 1]
    other = err & ~sum(1 << b for b in ERROR_BITS)
    if other:
        e.append(f"ismeretlen hibabit 0x{other:08X}")
    return f"állapot: {', '.join(s)} | hiba: {', '.join(e) if e else 'nincs'}"


class Keya:
    def __init__(self, bus, node, log):
        self.bus, self.node, self.log = bus, node, log
        self.replies = {}
        self.monitor = False
        self.stop = False
        threading.Thread(target=self._rx, daemon=True).start()

    def cid(self, cmd, node=None):
        return ((self.node if node is None else node) << 6) | cmd

    def send(self, cmd, data=b"", node=None):
        msg = can.Message(arbitration_id=self.cid(cmd, node), data=data, is_extended_id=False)
        self.bus.send(msg)
        self._log("TX", msg)

    def _log(self, d, msg):
        self.log.writerow([datetime.now().strftime("%H:%M:%S.%f")[:-3], d, f"{msg.arbitration_id:03X}",
                           "X" if msg.is_extended_id else "S", msg.dlc, bytes(msg.data).hex(" ")])

    def _rx(self):
        while not self.stop:
            msg = self.bus.recv(0.2)
            if msg is None or msg.is_error_frame:
                continue
            self._log("RX", msg)
            if msg.is_extended_id:
                if self.monitor:
                    print(f"  (29 bites keret {msg.arbitration_id:08X} – nem Keya)")
                continue
            node, cmd = msg.arbitration_id >> 6, msg.arbitration_id & 0x3F
            data = bytes(msg.data)
            self.replies[(node, cmd)] = (time.time(), data)
            if cmd == STATUS_REPORT:
                print(f"\n  << node {node} ÁLLAPOTJELENTÉS: {decode_status(data)}")
            elif self.monitor:
                print(f"  << {msg.arbitration_id:03X} node {node} {NAMES.get(cmd, hex(cmd))}: {data.hex(' ')}")

    def ask(self, cmd, data=b"", timeout=0.3, node=None):
        n = self.node if node is None else node
        t0 = time.time()
        self.send(cmd, data, node)
        while time.time() - t0 < timeout:
            r = self.replies.get((n, cmd))
            if r and r[0] >= t0:
                return r[1]
            time.sleep(0.005)
        return None

    def read_float(self, cmd):
        d = self.ask(cmd)
        return struct.unpack("<f", d[:4])[0] if d and len(d) >= 4 else None

    def param(self, data):
        # a motor a beírt paramétert visszaküldi (a leírás szerint ugyanazokkal a bájtokkal)
        d = self.ask(SET_PARAM, data)
        if d is None:
            return "nincs válasz"
        return "OK (visszaigazolva)" if bytes(d[:8]) == bytes(data) else f"eltérő válasz: {d.hex(' ')}"

    def result(self, cmd, data=b""):
        d = self.ask(cmd, data)
        if d is None:
            return "nincs válasz"
        code = struct.unpack("<i", d[:4])[0] if len(d) >= 4 else -1
        return "OK" if code == 0 else f"hiba ({code}) – adat: {d.hex(' ')}"


HELP = """
Parancsok:
  s              állapot és hibák lekérdezése
  e / d          motor engedélyezése / letiltása
  v <ford/s>     célfordulat (pl. v 0.5 ; v -1 ; v 0 = áll)   – előtte: e
  t <Nm>         célnyomaték (nyomaték üzemmódban)
  r              egyszeri mérés: fordulat, pozíció, áram, feszültség, teljesítmény
  w [mp]         folyamatos mérés (alap 10 s), Ctrl+C megszakítja
  test <ford/s> <mp>   fordulat-próba: ennyi ideig forgat, közben mér, a végén pozíció-különbség
  home           a jelenlegi pozíció legyen 0
  c              hibatörlés
  scan           node 1..31 keresése (állapot-lekérdezéssel)
  node <n>       melyik motorral beszéljünk (a programban)
  setid <n>      a motor node ID-jának átírása (utána: save, tápot le-fel)
  baud <250|500|800|1000>   a motor CAN sebességének átírása (utána: save, tápot le-fel)
  param <sorszám> <egész>   tetszőleges paraméter írása egész értékkel (pl. param 33 500)
  paramf <sorszám> <tört>   tetszőleges paraméter írása float értékkel (pl. paramf 8 2.0)
  save           konfiguráció mentése a motor flash-ébe (letiltott állapotban!)
  raw <cmd hex> [bájtok hex]  nyers keret, pl. raw 0e   vagy  raw 03 00 00 00 3f
  mon            minden fogadott keret kiírása be/ki
  q              kilépés (a motort megállítja és letiltja)
"""


def main():
    ap = argparse.ArgumentParser(description="Keya motor próba PCAN-USB adapterrel")
    ap.add_argument("--interface", default="pcan", help="python-can interfész (pcan, gs_usb, slcan, ...)")
    ap.add_argument("--channel", default="PCAN_USBBUS1")
    ap.add_argument("--bitrate", type=int, default=500000)
    ap.add_argument("--node", type=int, default=1)
    a = ap.parse_args()

    try:
        bus = can.Bus(interface=a.interface, channel=a.channel, bitrate=a.bitrate)
    except Exception as e:
        sys.exit(f"Nem nyitható meg az adapter ({a.interface} {a.channel}): {e}\n"
                 "Telepítve van a PEAK illesztőprogram? Más program (PCAN-View) nem használja épp?")
    logname = f"keya_log_{datetime.now():%Y%m%d_%H%M%S}.csv"
    logf = open(logname, "w", newline="", encoding="utf-8")
    log = csv.writer(logf, delimiter=";")
    log.writerow(["idő", "irány", "ID", "típus", "DLC", "adat"])
    k = Keya(bus, a.node, log)
    print(f"Csatlakozva: {a.interface} {a.channel} {a.bitrate // 1000} kbit/s, node {a.node}. Napló: {logname}")
    st = k.ask(GET_STATUS)
    print("Állapot:", decode_status(st) if st else
          "nincs válasz – ellenőrizd a bekötést, a tápot, a sebességet és a node ID-t (scan)")
    print(HELP)

    try:
        while True:
            try:
                line = input(f"keya[node {k.node}]> ").strip()
            except EOFError:
                break
            if not line:
                continue
            p = line.split()
            c = p[0].lower()
            try:
                if c == "q":
                    break
                elif c in ("h", "help", "?"):
                    print(HELP)
                elif c == "s":
                    d = k.ask(GET_STATUS)
                    print(decode_status(d) if d else "nincs válasz")
                elif c == "e":
                    print("engedélyezés:", k.result(ENABLE))
                elif c == "d":
                    print("letiltás:", k.result(DISABLE))
                elif c == "v":
                    k.send(SET_VEL, struct.pack("<f", float(p[1])))
                    print(f"célfordulat elküldve: {float(p[1])} ford/s ({float(p[1]) * 60:.1f} 1/min)")
                elif c == "t":
                    k.send(SET_TORQUE, struct.pack("<f", float(p[1])))
                    print(f"célnyomaték elküldve: {float(p[1])} Nm")
                elif c == "r":
                    for cmd in (GET_VEL, GET_POS, GET_IQ, GET_VBUS, GET_IBUS, GET_POWER, GET_TORQUE):
                        v = k.read_float(cmd)
                        extra = f"  ({v * 60:.1f} 1/min)" if cmd == GET_VEL and v is not None else ""
                        print(f"  {NAMES[cmd]:22s} {'nincs válasz' if v is None else f'{v:.3f}'}{extra}")
                elif c == "w":
                    dur = float(p[1]) if len(p) > 1 else 10.0
                    t0 = time.time()
                    print("   idő   ford/s   1/min   pozíció   áram A   táp V")
                    while time.time() - t0 < dur:
                        v, pos, iq, vb = (k.read_float(x) for x in (GET_VEL, GET_POS, GET_IQ, GET_VBUS))
                        f = lambda x, n=2: "  —  " if x is None else f"{x:.{n}f}"
                        print(f"  {time.time() - t0:5.1f}  {f(v, 3):>7}  {f(v * 60 if v is not None else None, 1):>6}"
                              f"  {f(pos, 3):>8}  {f(iq):>7}  {f(vb, 1):>6}")
                        time.sleep(0.25)
                elif c == "test":
                    vel, dur = float(p[1]), float(p[2])
                    print("engedélyezés:", k.result(ENABLE))
                    p0 = k.read_float(GET_POS)
                    k.send(SET_VEL, struct.pack("<f", vel))
                    t0 = time.time()
                    while time.time() - t0 < dur:
                        v, iq = k.read_float(GET_VEL), k.read_float(GET_IQ)
                        print(f"  {time.time() - t0:5.1f} s  {v if v is not None else float('nan'):.3f} ford/s  "
                              f"{iq if iq is not None else float('nan'):.2f} A", end="\r")
                        time.sleep(0.2)
                    k.send(SET_VEL, struct.pack("<f", 0.0))
                    time.sleep(0.8)
                    p1 = k.read_float(GET_POS)
                    print()
                    if p0 is not None and p1 is not None:
                        print(f"pozíció-különbség: {p1 - p0:.3f} fordulat (elvárt ≈ {vel * dur:.3f}).")
                        print("Számold meg, a hajtómű KIMENŐ tengelye hányat fordult: ha ennyit → a fordulat a kimenetre"
                              " vonatkozik; ha ennek ~1/49-ét → a motortengelyre.")
                elif c == "home":
                    print("nullpont:", k.result(SET_HOME))
                elif c == "c":
                    print("hibatörlés:", k.result(ERR_RESET))
                elif c == "scan":
                    found = [n for n in range(1, 32) if k.ask(GET_STATUS, node=n, timeout=0.1) is not None]
                    print("válaszoló node-ok:", found or "egy sem")
                elif c == "node":
                    k.node = int(p[1])
                    print(f"mostantól node {k.node}")
                elif c == "setid":
                    n = int(p[1])
                    if not 1 <= n <= 31:
                        raise ValueError("1..31")
                    print("Csak EGY motor legyen a buszon! Letiltás, majd írás…")
                    k.result(DISABLE)
                    print("node ID írás:", k.param(struct.pack("<ii", 31, n)))
                    print("Most: save, majd tápot le-fel. Utána indítsd a programot --node", n, "kapcsolóval.")
                elif c == "baud":
                    kb = int(p[1])
                    if kb not in BAUDS:
                        raise ValueError("250, 500, 800 vagy 1000")
                    k.result(DISABLE)
                    print("baud írás:", k.param(struct.pack("<ii", 32, BAUDS[kb])))
                    print(f"Most: save, majd tápot le-fel. Utána a programot --bitrate {kb * 1000} kapcsolóval indítsd.")
                elif c == "param":
                    print("paraméter írás:", k.param(struct.pack("<ii", int(p[1]), int(p[2]))))
                elif c == "paramf":
                    print("paraméter írás:", k.param(struct.pack("<if", int(p[1]), float(p[2]))))
                elif c == "save":
                    print("mentés:", k.result(SAVE), "– a változás a tápfeszültség újraindítása után él.")
                elif c == "raw":
                    cmd = int(p[1], 16)
                    data = bytes(int(x, 16) for x in p[2:])
                    d = k.ask(cmd, data, timeout=0.3)
                    print("válasz:", d.hex(" ") if d is not None else "nincs (vagy erre a parancsra nem válaszol)")
                elif c == "mon":
                    k.monitor = not k.monitor
                    print("keretfigyelés", "BE" if k.monitor else "KI")
                else:
                    print("ismeretlen parancs – h = súgó")
            except (IndexError, ValueError) as e:
                print("hibás paraméter:", e)
            except KeyboardInterrupt:
                print("\nmegszakítva")
            except can.CanError as e:
                print("CAN hiba:", e)
    finally:
        try:
            k.send(SET_VEL, struct.pack("<f", 0.0))
            k.send(DISABLE)
            time.sleep(0.2)
        except Exception:
            pass
        k.stop = True
        time.sleep(0.3)
        bus.shutdown()
        logf.close()
        print("Motor megállítva, letiltva. Napló:", logname)


if __name__ == "__main__":
    main()

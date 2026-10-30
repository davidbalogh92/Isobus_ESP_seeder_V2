"""A Windows-program CI kirajzolás-ellenőrzésének mintaállapotai (tests/fixtures/vt/*.json).

A pool újragenerálása után az objektum-azonosítók elcsúszhatnak, ezért a mintákat újra kell venni:
    build-host/web_demo 8099 &     (PC-s demó, CAN nélkül)
    python tools/capture_vt_fixtures.py [http://localhost:8099]
"""
import json
import re
import sys
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8099"
IDS = {m.group(1): int(m.group(2)) for m in re.finditer(r"constexpr std::uint16_t (\w+) = (\d+);",
                                                        (ROOT / "include/seeder/vt_ids.hpp").read_text("utf-8"))}
OUT = ROOT / "tests/fixtures/vt"


def post(path, body):
    req = urllib.request.Request(BASE + path, json.dumps(body).encode(), {"Content-Type": "application/json"})
    return json.loads(urllib.request.urlopen(req, timeout=5).read())


def get(path):
    return json.loads(urllib.request.urlopen(BASE + path, timeout=5).read())


def ev(kind, name, act=0):
    post("/api/vt/event", {"kind": kind, "id": IDS[name], "act": act, "value": 0})
    time.sleep(0.6)


def save(name):
    (OUT / name).write_text(json.dumps(get("/api/vt"), ensure_ascii=False, indent=1), "utf-8")
    print("mentve:", name)


def main():
    for k, v in {"drive_type": 1, "sim_speed": 8, "prefill_auto": 0}.items():
        post("/api/param", {"key": k, "value": v})
    st = get("/api/state")["status"]
    if not st.get("master"):
        ev("key", "K_MASTER")
    time.sleep(3)
    save("01_main.json")
    ev("key", "K_FUNC")
    save("02_func.json")
    ev("key", "K_BACK")
    ev("key", "K_MENU")
    save("03_menu.json")
    ev("key", "K_PG_CALIB")
    save("04_calib.json")
    ev("key", "K_BACK")
    ev("key", "K_BACK")
    ev("key", "K_MENU")
    ev("button", "BTN_PAGE_DIAGNOSE")
    save("05_diag.json")
    ev("key", "K_BACK")
    ev("key", "K_PG_DRIVE")
    ev("key", "K_TUNE_PAGE")
    save("06_tune.json")
    ev("key", "K_BACK")
    ev("key", "K_BACK")
    ev("key", "K_BACK")


if __name__ == "__main__":
    main()

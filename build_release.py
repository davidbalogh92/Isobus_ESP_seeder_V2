#!/usr/bin/env python3
"""PlatformIO fordítás + kiadási BIN fájlok (USB-s FULL és webes OTA).

A kimenet CSAK akkor jön létre, ha a tényleges fordítás sikerült. A bootloader, a partíciós
tábla és az alkalmazás egy 0x0 címre írható FULL képbe olvad (tisztán Pythonban, esptool nélkül).

  python build_release.py            # fordít és csomagol
  python build_release.py --no-build # egy kész .pio build csomagolása (CI)
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent
OUT = ROOT / "firmware"
APP_ADDRESS = 0x20000
# környezet → (fájlnév előtag, partíciós tábla, OTA slot méret, flash méret, panel neve)
BOARDS = {
    "waveshare_8di8ro": ("VETOGEP_WAVESHARE_16MB", "partitions_16mb.csv", 0x700000, 0x1000000,
                         "Waveshare ESP32-S3-ETH-8DI-8RO-C (16 MB, CAN GPIO17/18)"),
}


def fw_version() -> str:
    m = re.search(r'SEEDER_FW_VERSION=\\"([^"\\]+)\\"', (ROOT / "platformio.ini").read_text(encoding="utf-8"))
    return m.group(1) if m else "0.0.0"


def sha256(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


def validate_partition_table(env: str):
    _, table, slot, flash, _ = BOARDS[env]
    t = (ROOT / table).read_text(encoding="utf-8")
    assert f"ota_0,      app,  ota_0,    0x20000,   {hex(slot)}" in t, table
    assert f"ota_1,      app,  ota_1,    {hex(0x20000 + slot)},  {hex(slot)}" in t, table
    assert "otadata,    data, ota,      0x19000,   0x2000" in t, table
    assert 0x20000 + 2 * slot <= flash, table


def merge_bin(parts, destination: Path, max_flash=0x1000000):
    """Nyers ESP képek összefűzése 0x0 címtől, a hézagok 0xFF-fel kitöltve."""
    end = max(a + p.stat().st_size for a, p in parts)
    if end >= max_flash:
        raise ValueError("Az összefűzött kép nagyobb a flash méreténél")
    spans = sorted((a, a + p.stat().st_size) for a, p in parts)
    for (_, e0), (s1, _) in zip(spans, spans[1:]):
        if s1 < e0:
            raise ValueError("Átfedő flash képek")
    with destination.open("wb") as out:
        pos = 0
        for start, path in sorted(parts):
            if path.stat().st_size == 0:
                raise ValueError("Üres kép: " + str(path))
            out.write(b"\xff" * (start - pos))
            with path.open("rb") as src:
                shutil.copyfileobj(src, out)
            pos = start + path.stat().st_size


def build(env: str, skip_compile=False):
    prefix, _, slot, flash, board = BOARDS[env]
    validate_partition_table(env)
    build_dir = ROOT / ".pio" / "build" / env
    if not skip_compile:
        exe = shutil.which("pio") or shutil.which("platformio")
        if not exe:
            raise RuntimeError("PlatformIO nem található (telepítsd: pip install platformio)")
        subprocess.run([exe, "run", "-e", env], cwd=ROOT, check=True)
    boot, parts, app = build_dir / "bootloader.bin", build_dir / "partitions.bin", build_dir / "firmware.bin"
    for p in (boot, parts, app):
        if not p.exists():
            raise RuntimeError("Hiányzó fordítási kimenet: " + str(p))
    if app.stat().st_size > slot:
        raise RuntimeError("Az alkalmazás nagyobb az OTA partíciónál")
    head = app.read_bytes()[:36]
    if len(head) < 36 or head[0] != 0xE9 or head[32:36] != bytes.fromhex("3254cdab"):
        raise RuntimeError("Nem érvényes ESP alkalmazás-kép")
    if boot.read_bytes()[:1] != b"\xe9":
        raise RuntimeError("A bootloader nem ESP kép")
    OUT.mkdir(exist_ok=True)
    stem = f"{prefix}_v{fw_version().replace('.', '')}"
    ota, full = OUT / f"{stem}_OTA.bin", OUT / f"{stem}_FULL_0x0.bin"
    shutil.copyfile(app, ota)
    merge_bin([(0x0, boot), (0x8000, parts), (APP_ADDRESS, app)], full, flash)
    manifest_path = OUT / "MANIFEST.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8")) if manifest_path.exists() else {}
    manifest["firmware"] = fw_version()
    manifest[env] = {
        "board": board,
        "flash_size_bytes": flash,
        "usb_full_flash_address": "0x0",
        "files": {p.name: {"bytes": p.stat().st_size, "sha256": sha256(p)} for p in (full, ota)},
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"[{env}] USB (0x0):", full)
    print(f"[{env}] OTA      :", ota)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--no-build", action="store_true")
    ap.add_argument("--env", choices=list(BOARDS) + ["all"], default="all")
    a = ap.parse_args()
    try:
        for env in (BOARDS if a.env == "all" else [a.env]):
            build(env, skip_compile=a.no_build)
    except (RuntimeError, ValueError, AssertionError, subprocess.CalledProcessError) as exc:
        print("[HIBA] Nem készült BIN: " + str(exc), file=sys.stderr)
        sys.exit(1)

#!/usr/bin/env python3
"""
Read the transponder ID of an OpenStint v2 board (ATtiny816) over UPDI.

The firmware derives the ID at runtime from the chip's factory serial number,
so the value is never stored in flash or EEPROM. This script reads the raw
serial number over UPDI and reproduces the firmware's calculation on the host:

    crc32(SIGROW.SERNUM0..9) % 10000000

Usage:
    python transponder_id.py --port COM5
    python transponder_id.py --port COM5 --csv transponders.csv --label 07
"""

import argparse
import csv
import os
import subprocess
import sys
import zlib
from datetime import datetime

ID_MODULO = 10_000_000


def read_sernum(avrdude, port, baud, part):
    """Read the 10-byte factory serial number over UPDI."""
    cmd = [
        avrdude,
        "-c", "serialupdi",
        "-P", port,
        "-b", str(baud),
        "-p", part,
        "-U", "sernum:r:-:h",
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"avrdude failed to read sernum:\n{proc.stderr.strip()}")

    # avrdude prints the bytes as "0x1e,0x93,..." on stdout and the progress
    # bar on stderr.
    tokens = []
    for line in proc.stdout.splitlines():
        line = line.strip().rstrip(",")
        if not line.startswith("0x"):
            continue
        tokens.extend(t for t in line.split(",") if t.strip())
    if not tokens:
        raise RuntimeError("avrdude returned no data for sernum.")

    sernum = bytes(int(t, 0) for t in tokens)
    if len(sernum) < 10:
        raise RuntimeError(f"sernum too short: got {len(sernum)} bytes, expected 10.")
    return sernum[:10]


def transponder_id(sernum):
    """Same calculation as crc32_calc(...) % 10000000 in main.c."""
    return zlib.crc32(sernum) % ID_MODULO


def append_csv(path, label, sernum_hex, tid):
    """Append one row, writing the header if the file is new."""
    is_new = not os.path.exists(path)
    with open(path, "a", newline="", encoding="utf-8") as fh:
        writer = csv.writer(fh, delimiter=";")
        if is_new:
            writer.writerow(["timestamp", "label", "sernum", "transponder_id"])
        writer.writerow([
            datetime.now().isoformat(timespec="seconds"),
            label, sernum_hex, f"{tid:07d}",
        ])


def main():
    ap = argparse.ArgumentParser(
        description="Read an OpenStint transponder ID over UPDI")
    ap.add_argument("--port", required=True, help="serial port, e.g. COM5")
    ap.add_argument("--baud", type=int, default=230400)
    ap.add_argument("--part", default="attiny816")
    ap.add_argument("--avrdude", default="avrdude", help="path to the avrdude binary")
    ap.add_argument("--csv", help="append the result to this CSV file")
    ap.add_argument("--label", default="", help="your own board number for the CSV")
    args = ap.parse_args()

    try:
        sernum = read_sernum(args.avrdude, args.port, args.baud, args.part)
    except (RuntimeError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    tid = transponder_id(sernum)
    sernum_hex = sernum.hex().upper()

    print(f"SERNUM : {sernum_hex}")
    print(f"ID     : {tid:07d}")

    if args.csv:
        append_csv(args.csv, args.label, sernum_hex, tid)
        print(f"-> appended to {args.csv}")

    return 0


if __name__ == "__main__":
    sys.exit(main())

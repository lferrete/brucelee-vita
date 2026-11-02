#!/usr/bin/env python3
"""
Static import audit for the Bruce Lee port.

Compares the undefined symbols of libdefault.so / libzplatform.so against the
symbols supplied by libzplatform and by source/dynlib.c.  Run from the project
root after tools/prepare_data.ps1:

    py -3 tools/check_imports.py
"""
from pathlib import Path
import re
import sys

try:
    from elftools.elf.elffile import ELFFile
except ImportError:
    sys.exit("pyelftools is required: pip install pyelftools")

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data" / "lib" / "armeabi"


def undef(path):
    with path.open("rb") as f:
        d = ELFFile(f).get_section_by_name(".dynsym")
        return {s.name for s in d.iter_symbols() if s.name and s["st_shndx"] == "SHN_UNDEF"}


def exports(path):
    with path.open("rb") as f:
        d = ELFFile(f).get_section_by_name(".dynsym")
        return {s.name for s in d.iter_symbols() if s.name and s["st_shndx"] != "SHN_UNDEF"}


def main():
    zp = DATA / "libzplatform.so"
    default = DATA / "libdefault.so"
    if not zp.exists() or not default.exists():
        sys.exit("Missing data/lib/armeabi/*.so; run tools/prepare_data.ps1 first.")

    zp_und, df_und = undef(zp), undef(default)
    zp_exp = exports(zp)
    src = (ROOT / "source" / "dynlib.c").read_text(encoding="utf-8", errors="replace")
    table = set(re.findall(r'\{\s*"([^"]+)"\s*,\s*\(uintptr_t\)', src))

    missing_zp = sorted(zp_und - table)
    missing_df = sorted(df_und - zp_exp - table)

    print(f"dynlib table entries: {len(table)}")
    print(f"libzplatform imports: {len(zp_und)} (missing {len(missing_zp)})")
    print(f"libdefault imports:   {len(df_und)} (missing {len(missing_df)})")

    for name in missing_zp:
        print("MISSING zplatform:", name)
    for name in missing_df:
        print("MISSING default:  ", name)

    return 1 if (missing_zp or missing_df) else 0


if __name__ == "__main__":
    raise SystemExit(main())

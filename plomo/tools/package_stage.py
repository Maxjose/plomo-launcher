#!/usr/bin/env python3
"""Assemble the folder that the PLOMO installer packages, from a `cmake --install` output.

Only a whitelist is copied, so accounts, settings, instances and downloaded game files can never end up in the package.
Usage: package_stage.py --install install --out stage [--crt <folder with the MSVC runtime DLLs>]
"""

import argparse
import shutil
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
DIRECTORIES = ("iconengines", "imageformats", "jars", "platforms", "styles", "tls")
FILES = ("PLOMO.exe", "PLOMO_filelink.exe", "portable.txt", "qt.conf")
# Personal or generated data: if any of these shows up in the staging folder something is wrong.
FORBIDDEN = ("accounts.json", "PLOMO.cfg", "instances", "logs", "assets", "libraries", "java", "meta", "metacache", "cache")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--install", required=True, type=Path, help="output of cmake --install (plus --component portable)")
    parser.add_argument("--out", required=True, type=Path, help="staging folder, recreated on every run")
    parser.add_argument("--crt", type=Path, help="folder with the MSVC runtime DLLs (Microsoft.VC143.CRT)")
    args = parser.parse_args()

    if args.out.exists():
        shutil.rmtree(args.out)  # derived folder
    args.out.mkdir(parents=True)
    for name in DIRECTORIES:
        shutil.copytree(args.install / name, args.out / name)
    for name in FILES:
        shutil.copy2(args.install / name, args.out / name)
    for dll in args.install.glob("*.dll"):
        shutil.copy2(dll, args.out / dll.name)
    if args.crt:
        for dll in args.crt.glob("*.dll"):
            shutil.copy2(dll, args.out / dll.name)
    shutil.copytree(REPO / "plomo" / "translations", args.out / "translations")
    shutil.copy2(REPO / "launcher" / "qtlogging.ini", args.out / "qtlogging.ini")

    leaked = [name for name in FORBIDDEN if (args.out / name).exists()]
    if leaked:
        sys.exit(f"Personal or generated data in the package, aborting: {leaked}")
    print(f"Staged {sum(1 for f in args.out.rglob('*') if f.is_file())} files in {args.out}")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Package the PLOMO launcher as PLOMO-<version>.zip from a clean copy of the portable program files.

Only a whitelist is copied, so accounts, settings, instances and downloaded game files never end up in the ZIP.
The version is read from source/CMakeLists.txt (Launcher_VERSION_MAJOR/MINOR/PATCH).
"""

import re
import shutil
import zipfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "portable"
STAGING = ROOT / "work" / "portable-clean"
DIRECTORIES = ("iconengines", "imageformats", "jars", "platforms", "styles", "tls", "translations")
FILES = ("PLOMO.exe", "PLOMO_filelink.exe", "portable.txt", "qt.conf", "qtlogging.ini")
# Personal or generated data: if any of these shows up in the staging folder something is wrong.
FORBIDDEN = ("accounts.json", "PLOMO.cfg", "instances", "logs", "assets", "libraries", "java", "meta", "metacache", "cache")


def launcher_version() -> str:
    cmake = (ROOT / "source" / "CMakeLists.txt").read_text(encoding="utf-8")
    parts = [re.search(rf"set\(Launcher_VERSION_{name} (\d+)\)", cmake) for name in ("MAJOR", "MINOR", "PATCH")]
    if not all(parts):
        raise SystemExit("Could not read Launcher_VERSION_MAJOR/MINOR/PATCH from source/CMakeLists.txt")
    return ".".join(match.group(1) for match in parts)


def main():
    output = ROOT / f"PLOMO-{launcher_version()}.zip"
    if STAGING.exists():
        shutil.rmtree(STAGING)  # derived folder, rebuilt on every run
    STAGING.mkdir(parents=True)
    for name in DIRECTORIES:
        shutil.copytree(SOURCE / name, STAGING / name)
    for name in FILES:
        shutil.copy2(SOURCE / name, STAGING / name)
    for dll in SOURCE.glob("*.dll"):
        shutil.copy2(dll, STAGING / dll.name)
    leaked = [name for name in FORBIDDEN if (STAGING / name).exists()]
    if leaked:
        raise SystemExit(f"Personal or generated data in the package, aborting: {leaked}")
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
        for file in sorted(STAGING.rglob("*")):
            if file.is_file():
                archive.write(file, file.relative_to(STAGING))
    print(f"{output} ({output.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()

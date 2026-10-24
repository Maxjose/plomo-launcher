#!/usr/bin/env python3
"""Build a PLOMO file manifest and optional HTTP payload from a Prism instance ZIP."""

import argparse
import hashlib
import json
import sys
import zipfile
from pathlib import Path, PurePosixPath


MANAGED_DIRS = {
    "config", "defaultconfigs", "mods", "resourcepacks",
    "shaderpacks", "tacz", "scripts", "patchouli_books",
}
# kubejs and ESM are deliberately not distributed to players.
PACK_COMPONENTS = {"formatVersion": 1, "components": [
    {"important": True, "uid": "net.minecraft", "version": "1.20.1"},
    {"uid": "net.minecraftforge", "version": "47.4.16"},
]}
# Files the game or its mods rewrite on every start (video/shader settings, machine fingerprints). Publishing them
# would make the launcher offer an update after each session and overwrite the player's own settings.
EXCLUDED_FILES = {
    "config/embeddium-options.json",
    "config/embeddium-fingerprint.json",
    "config/oculus.properties",
    "config/fml.toml",
    "config/asyncparticles/asyncparticles-mixin.properties",
    "config/fabric/indigo-renderer.properties",
}
PRESERVED_NAMES = {
    "options.txt", "optionsof.txt", "servers.dat", "servers.dat_old",
    "usercache.json", "logs", "saves", "screenshots", "crash-reports",
    "xaero", "journeymap",
}


def managed_path(name: str, root: str = "minecraft") -> str | None:
    path = PurePosixPath(name)
    if path.is_absolute() or "\\" in name or ".." in path.parts:
        raise ValueError(f"Unsafe ZIP path: {name}")
    parts = path.parts
    if len(parts) < 3 or parts[0] != root or parts[1] not in MANAGED_DIRS:
        return None
    # config/defaultoptions holds the initial options for new players, so its options.txt is published.
    if parts[1:3] != ("config", "defaultoptions") and any(part.lower() in PRESERVED_NAMES for part in parts[1:]):
        return None
    relative = "/".join(parts[1:])
    if relative.lower() in EXCLUDED_FILES:
        return None
    return relative


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zip_path", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--base-url", required=True,
                        help="Public URL of the payload directory, e.g. http://host:8090/files/")
    parser.add_argument("--materialize", action="store_true",
                        help="Extract managed files under output_dir/files for HTTP hosting")
    args = parser.parse_args()

    files = []
    seen = set()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(args.zip_path) as archive:
        names = archive.namelist()
        if "mmc-pack.json" in names:
            # Full Prism instance export: files live under minecraft/.
            root = "minecraft"
            pack = json.loads(archive.read("mmc-pack.json"))
            instance_cfg = archive.read("instance.cfg").decode("utf-8-sig", errors="replace")
            declared_version = next((line.split("=", 1)[1].strip() for line in instance_cfg.splitlines()
                                     if line.startswith("ManagedPackVersionName=")), None)
            if declared_version and declared_version != args.version:
                raise ValueError(f"Instance declares version {declared_version}, not {args.version}")
        else:
            # Plain folder ZIP (e.g. beta-0.34/config, beta-0.34/mods, ...): one top-level folder.
            roots = {name.split("/", 1)[0] for name in names}
            if len(roots) != 1:
                raise ValueError(f"Expected a single top-level folder in the ZIP, found {sorted(roots)[:5]}")
            root = roots.pop()
            pack = PACK_COMPONENTS
        components = {item["uid"]: item["version"] for item in pack["components"]}
        if components.get("net.minecraft") != "1.20.1" or components.get("net.minecraftforge") != "47.4.16":
            raise ValueError("Expected Minecraft 1.20.1 and Forge 47.4.16")
        for member in archive.infolist():
            if member.is_dir():
                continue
            rel = managed_path(member.filename, root)
            if rel is None:
                continue
            if rel.casefold() in seen:
                raise ValueError(f"Duplicate destination path: {rel}")
            seen.add(rel.casefold())
            digest = hashlib.sha256()
            target = args.output_dir / "files" / Path(*PurePosixPath(rel).parts)
            if args.materialize:
                target.parent.mkdir(parents=True, exist_ok=True)
                out = target.open("wb")
            else:
                out = None
            try:
                with archive.open(member) as source:
                    while chunk := source.read(1024 * 1024):
                        digest.update(chunk)
                        if out is not None:
                            out.write(chunk)
            finally:
                if out is not None:
                    out.close()
            files.append({"path": rel, "size": member.file_size, "sha256": digest.hexdigest()})

    files.sort(key=lambda item: item["path"].casefold())
    manifest = {
        "schema": 1,
        "pack": "PLOMO",
        "version": args.version,
        "minecraft": "1.20.1",
        "forge": "47.4.16",
        "java": 17,
        "mmc_pack": pack,
        "base_url": args.base_url.rstrip("/") + "/",
        "files": files,
    }
    destination = args.output_dir / "manifest.json"
    destination.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    total = sum(item["size"] for item in files)
    print(f"{destination}: {len(files)} files, {total:,} bytes")
    if not args.materialize:
        print("Payload not extracted; run again with --materialize before publishing.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

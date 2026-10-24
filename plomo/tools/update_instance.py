#!/usr/bin/env python3
"""Reference installer for the PLOMO manifest; the native launcher will use this contract."""

import argparse
import hashlib
import json
import os
import shutil
import sys
import tempfile
import urllib.parse
import urllib.request
from pathlib import Path, PurePosixPath


PRESERVED = {"saves", "screenshots", "logs", "crash-reports", "options.txt",
             "optionsof.txt", "usercache.json", "servers.dat", "servers.dat_old"}


def safe_relative(value):
    path = PurePosixPath(value)
    if (not value or path.is_absolute() or "\\" in value or ".." in path.parts
            or ":" in value or path.parts[0].lower() in PRESERVED):
        raise ValueError(f"Unsafe or preserved path in manifest: {value}")
    return Path(*path.parts)


def hash_file(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        while chunk := source.read(1024 * 1024):
            digest.update(chunk)
    return digest.hexdigest()


def load_manifest(location):
    if location.startswith(("http://", "https://", "file://")):
        with urllib.request.urlopen(location, timeout=30) as response:
            data = response.read()
    else:
        data = Path(location).read_bytes()
    manifest = json.loads(data)
    if (manifest.get("schema") != 1 or manifest.get("pack") != "PLOMO"
            or manifest.get("minecraft") != "1.20.1"
            or manifest.get("forge") != "47.4.16"
            or manifest.get("java") != 17):
        raise ValueError("Unsupported PLOMO manifest")
    base = manifest.get("base_url", "")
    if not base.startswith(("http://", "https://", "file://")) or not base.endswith("/"):
        raise ValueError("Invalid base_url")
    seen = set()
    for entry in manifest["files"]:
        rel = safe_relative(entry["path"])
        if rel.as_posix().casefold() in seen:
            raise ValueError(f"Duplicate path: {rel}")
        seen.add(rel.as_posix().casefold())
        if (not isinstance(entry["size"], int) or entry["size"] < 0
                or len(entry["sha256"]) != 64
                or any(c not in "0123456789abcdef" for c in entry["sha256"])):
            raise ValueError(f"Invalid file metadata: {rel}")
    return manifest


def plan(manifest, instance):
    game = instance / "minecraft"
    changes = []
    for entry in manifest["files"]:
        destination = game / safe_relative(entry["path"])
        if (not destination.is_file() or destination.stat().st_size != entry["size"]
                or hash_file(destination) != entry["sha256"]):
            changes.append(entry)
    return changes


def atomic_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile("w", encoding="utf-8", dir=path.parent,
                                     suffix=".tmp", delete=False) as temp:
        json.dump(value, temp, ensure_ascii=False, indent=2)
        temp.write("\n")
        temp_path = Path(temp.name)
    os.replace(temp_path, path)


def install(manifest, instance, changes):
    work = instance / ".plomo" / "downloads"
    work.mkdir(parents=True, exist_ok=True)
    staged = []
    try:
        for index, entry in enumerate(changes, 1):
            rel = safe_relative(entry["path"])
            url = manifest["base_url"] + urllib.parse.quote(rel.as_posix(), safe="/")
            target = work / rel
            target.parent.mkdir(parents=True, exist_ok=True)
            with urllib.request.urlopen(url, timeout=60) as response, target.open("wb") as output:
                shutil.copyfileobj(response, output)
            if target.stat().st_size != entry["size"] or hash_file(target) != entry["sha256"]:
                raise ValueError(f"Downloaded file failed verification: {rel}")
            staged.append((target, instance / "minecraft" / rel))
            print(f"\rVerified {index}/{len(changes)}", end="", flush=True)
        if changes:
            print()
        for source, destination in staged:
            destination.parent.mkdir(parents=True, exist_ok=True)
            os.replace(source, destination)

        previous_path = instance / ".plomo" / "installed-manifest.json"
        if previous_path.is_file():
            previous = json.loads(previous_path.read_text(encoding="utf-8"))
            current = {entry["path"].casefold() for entry in manifest["files"]}
            for entry in previous.get("files", []):
                rel = safe_relative(entry["path"])
                if entry["path"].casefold() not in current:
                    old_file = instance / "minecraft" / rel
                    if old_file.is_file() and hash_file(old_file) == entry["sha256"]:
                        old_file.unlink()

        if not (instance / "instance.cfg").exists():
            (instance / "instance.cfg").write_text(
                "[General]\nConfigVersion=1.3\nInstanceType=OneSix\nname=PLOMO\nAutomaticJava=true\n", encoding="utf-8")
        if not (instance / "mmc-pack.json").exists():
            atomic_json(instance / "mmc-pack.json", manifest["mmc_pack"])
        atomic_json(previous_path, manifest)
    finally:
        shutil.rmtree(work, ignore_errors=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("manifest", help="Manifest URL or local JSON path")
    parser.add_argument("instance", type=Path, help="Prism/PineconeMC instance directory")
    parser.add_argument("--install", action="store_true", help="Download and apply changes")
    args = parser.parse_args()
    manifest = load_manifest(args.manifest)
    changes = plan(manifest, args.instance)
    installed_file = args.instance / ".plomo" / "installed-manifest.json"
    installed_version = (json.loads(installed_file.read_text(encoding="utf-8")).get("version")
                         if installed_file.is_file() else None)
    state = ("Descargar" if not (args.instance / "instance.cfg").is_file()
             else "Actualizar" if changes or installed_version != manifest["version"] else "Jugar")
    print(f"PLOMO {manifest['version']}: {state}; {len(changes)} files to download")
    if args.install:
        install(manifest, args.instance, changes)
        print("Installation verified; ready to play")
    elif changes:
        print("Run again with --install to apply")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, KeyError, urllib.error.URLError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)

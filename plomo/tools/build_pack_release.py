#!/usr/bin/env python3
"""Build a PLOMO file manifest and optional HTTP payload from a Prism instance ZIP."""

import argparse
import hashlib
import json
import re
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
# Client config that holds the server the PLOMO start screen connects to. Alpha builds point at the alpha server and
# releases at the real one, whatever address the source instance has.
CLIENT_CONFIG = "config/plomo-client.toml"
RELEASE_SERVER = "plomo.raidzone.online"
_SERVER_LINE = re.compile(r'^(\s*serverAddress\s*=\s*)"[^"]*"', re.MULTILINE)


def set_server_address(data: bytes, address: str) -> bytes:
    """Return the client config with serverAddress replaced; fail if the line is not there."""
    text = data.decode("utf-8")
    new, count = _SERVER_LINE.subn(lambda m: f'{m.group(1)}"{address}"', text, count=1)
    if count != 1:
        raise ValueError(f"{CLIENT_CONFIG} has no serverAddress line")
    return new.encode("utf-8")


def server_address(data: bytes) -> str:
    match = re.search(r'^\s*serverAddress\s*=\s*"([^"]*)"', data.decode("utf-8"), re.MULTILINE)
    return match.group(1) if match else ""


PRESERVED_NAMES = {
    "options.txt", "optionsof.txt", "servers.dat", "servers.dat_old",
    "usercache.json", "logs", "saves", "screenshots", "crash-reports",
    "xaero", "journeymap",
}


def managed_path(name: str, root: str | None = "minecraft") -> str | None:
    path = PurePosixPath(name)
    if path.is_absolute() or "\\" in name or ".." in path.parts:
        raise ValueError(f"Unsafe ZIP path: {name}")
    parts = path.parts
    if root is None:  # no wrapper folder: config/, mods/, ... sit at the top of the ZIP
        parts = ("",) + parts
        root = ""
    if len(parts) < 3 or parts[0] != root or parts[1] not in MANAGED_DIRS:
        return None
    # config/defaultoptions holds the initial options for new players, so its options.txt is published.
    if parts[1:3] != ("config", "defaultoptions") and any(part.lower() in PRESERVED_NAMES for part in parts[1:]):
        return None
    relative = "/".join(parts[1:])
    if relative.lower() in EXCLUDED_FILES:
        return None
    return relative


class DirArchive:
    """A folder presented with the small part of the zipfile.ZipFile API this script uses."""

    class Member:
        def __init__(self, filename: str, size: int):
            self.filename = filename
            self.file_size = size

        def is_dir(self) -> bool:
            return False

    def __init__(self, root: Path):
        self.root = root
        self._members = [self.Member(path.relative_to(root).as_posix(), path.stat().st_size)
                         for path in sorted(root.rglob("*")) if path.is_file()]

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        return False

    def namelist(self):
        return [m.filename for m in self._members]

    def infolist(self):
        return list(self._members)

    def open(self, member):
        return (self.root / member.filename).open("rb")

    def read(self, member) -> bytes:
        name = member if isinstance(member, str) else member.filename
        return (self.root / name).read_bytes()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("zip_path", type=Path, help="pack ZIP, or the pack folder itself")
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--base-url", required=True,
                        help="Public URL of the payload directory, e.g. http://host:8090/files/")
    parser.add_argument("--channel", choices=("release", "alpha"), default="release",
                        help="alpha writes manifest-alpha.json with \"channel\": \"alpha\"; release writes manifest.json")
    parser.add_argument("--alpha-server", default="alpha.raidzone.online",
                        help="serverAddress written into config/plomo-client.toml on the alpha channel")
    parser.add_argument("--release-server", default=RELEASE_SERVER,
                        help="serverAddress written into config/plomo-client.toml on the release channel")
    parser.add_argument("--materialize", action="store_true",
                        help="Extract managed files under output_dir/files for HTTP hosting")
    args = parser.parse_args()

    files = []
    seen = set()
    client_config_checked = False
    args.output_dir.mkdir(parents=True, exist_ok=True)
    with (DirArchive(args.zip_path) if args.zip_path.is_dir() else zipfile.ZipFile(args.zip_path)) as archive:
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
            # Plain folder ZIP: either config/, mods/, ... directly at the top, or inside one wrapper folder
            # (e.g. beta-0.34/config, beta-0.34/mods).
            roots = {name.split("/", 1)[0] for name in names}
            if roots & MANAGED_DIRS:
                root = None
            elif len(roots) == 1:
                root = roots.pop()
            else:
                raise ValueError(f"Expected the pack folders (config, mods, ...) or a single wrapper folder, found {sorted(roots)[:5]}")
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
            if rel == CLIENT_CONFIG:
                # The test instance the pack comes from points at the alpha server. Force the right address for the channel
                # so it is never forgotten: alpha servers for alpha builds, the real server for releases.
                data = archive.read(member)
                data = set_server_address(data, args.alpha_server if args.channel == "alpha" else args.release_server)
                client_config_checked = True
                digest.update(data)
                if args.materialize:
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(data)
                files.append({"path": rel, "size": len(data), "sha256": digest.hexdigest()})
                continue
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

    if args.channel == "alpha" and not client_config_checked:
        raise ValueError(f"{CLIENT_CONFIG} is not in the ZIP; the alpha would connect to the real server")
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
    if args.channel == "alpha":
        manifest["channel"] = "alpha"  # the launcher shows "(alpha)" next to the installed version
    destination = args.output_dir / ("manifest-alpha.json" if args.channel == "alpha" else "manifest.json")
    destination.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    total = sum(item["size"] for item in files)
    print(f"{destination}: {len(files)} files, {total:,} bytes")
    if not args.materialize:
        print("Payload not extracted; run again with --materialize before publishing.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

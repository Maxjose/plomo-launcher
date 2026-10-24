#!/usr/bin/env python3
"""Mirror the launcher metadata PLOMO needs from Prism's public meta server, so the launcher stops depending on a
third-party server.

The launcher reads <META_URL>/index.json, then <uid>/index.json and <uid>/<version>.json, and checks the SHA-256 of each
file against the entry in the file above it. Version files are copied byte for byte (their hashes stay valid); the package
indexes are trimmed to the versions PLOMO uses, and the root index is rebuilt with the new hashes.

Libraries that upstream hosts on Prism's own file server (ForgeWrapper) are copied too, and their URL is rewritten to
the CDN, so that nothing is fetched from a third party except Mojang and Forge themselves.

Upload the resulting v1/ and maven/ folders to the CDN (meta/v1/ and meta/maven/) and point Launcher_META_URL at v1/.
"""

import argparse
import hashlib
import json
import sys
import urllib.request
from pathlib import Path
from urllib.parse import urlparse

SOURCE = "https://meta.prismlauncher.org/v1/"
ALWAYS_ALL = {"net.minecraft.java"}  # Java runtimes: tiny, and the launcher picks one by major version
THIRD_PARTY_HOSTS = {"files.prismlauncher.org"}  # jar hosts we replace with the CDN


def download(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "PLOMO-meta-mirror"})
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read()


def fetch(path: str) -> bytes:
    return download(SOURCE + path)


def localize(document: dict, base_url: str, maven_dir: Path) -> int:
    """Copy third-party jars next to the metadata and point the document at the copies. Returns how many were copied."""
    copied = 0
    for section in ("libraries", "mavenFiles"):
        for library in document.get(section, []):
            artifact = library.get("downloads", {}).get("artifact")
            if not artifact or urlparse(artifact["url"]).hostname not in THIRD_PARTY_HOSTS:
                continue
            jar = download(artifact["url"])
            if hashlib.sha1(jar).hexdigest() != artifact["sha1"] or len(jar) != artifact["size"]:
                raise SystemExit(f"Hash or size mismatch for {artifact['url']}")
            relative = urlparse(artifact["url"]).path.split("/maven/", 1)[1]
            (maven_dir / relative).parent.mkdir(parents=True, exist_ok=True)
            (maven_dir / relative).write_bytes(jar)
            artifact["url"] = f"{base_url}maven/{relative}"
            copied += 1
    return copied


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def pick(entries, wanted):
    found = {entry["version"]: entry for entry in entries}
    missing = [version for version in wanted if version not in found]
    if missing:
        raise SystemExit(f"Versions not published upstream: {missing}")
    return [found[version] for version in wanted]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--minecraft", default="1.20.1")
    parser.add_argument("--forge", default="47.4.16")
    parser.add_argument("--out", type=Path, default=Path("meta-mirror"))
    parser.add_argument("--cdn", default="https://cdn.playplomo.com/meta/", help="public URL that --out will be uploaded to")
    args = parser.parse_args()

    root = json.loads(fetch("index.json"))
    root_entries = {package["uid"]: package for package in root["packages"]}
    out = args.out / "v1"
    out.mkdir(parents=True, exist_ok=True)

    selected = {}  # uid -> versions to publish (None means all)
    for uid in ("net.minecraft", "net.minecraftforge", "org.lwjgl3", *ALWAYS_ALL):
        if uid not in root_entries:
            raise SystemExit(f"Package {uid} missing upstream")
    packages = {}
    for uid in root_entries:
        if uid not in ("net.minecraft", "net.minecraftforge", "org.lwjgl3", *ALWAYS_ALL):
            continue
        raw = fetch(f"{uid}/index.json")
        if sha256(raw) != root_entries[uid]["sha256"]:
            raise SystemExit(f"Hash mismatch for {uid}/index.json")
        packages[uid] = json.loads(raw)

    minecraft_entry = pick(packages["net.minecraft"]["versions"], [args.minecraft])[0]
    forge_entry = pick(packages["net.minecraftforge"]["versions"], [args.forge])[0]
    if not any(r.get("uid") == "net.minecraft" and r.get("equals") == args.minecraft for r in forge_entry.get("requires", [])):
        raise SystemExit(f"Forge {args.forge} does not target Minecraft {args.minecraft}")
    lwjgl = [r.get("suggests") or r.get("equals") for r in minecraft_entry.get("requires", []) if r.get("uid") == "org.lwjgl3"]
    if not lwjgl or not all(lwjgl):
        raise SystemExit("Could not work out the LWJGL version required by this Minecraft version")
    selected["net.minecraft"] = [args.minecraft]
    selected["net.minecraftforge"] = [args.forge]
    selected["org.lwjgl3"] = lwjgl
    for uid in ALWAYS_ALL:
        selected[uid] = [entry["version"] for entry in packages[uid]["versions"]]

    base_url = args.cdn.rstrip("/") + "/"
    root_out = {"formatVersion": root["formatVersion"], "packages": []}
    files = 0
    jars = 0
    for uid, versions in selected.items():
        trimmed = dict(packages[uid])
        trimmed["versions"] = pick(packages[uid]["versions"], versions)
        (out / uid).mkdir(parents=True, exist_ok=True)
        for entry in trimmed["versions"]:
            version_file = fetch(f"{uid}/{entry['version']}.json")
            if sha256(version_file) != entry["sha256"]:
                raise SystemExit(f"Hash mismatch for {uid}/{entry['version']}.json")
            document = json.loads(version_file)
            copied = localize(document, base_url, args.out / "maven")
            if copied:  # the file changed, so the package index must carry its new hash
                version_file = (json.dumps(document, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
                entry["sha256"] = sha256(version_file)
                jars += copied
            (out / uid / f"{entry['version']}.json").write_bytes(version_file)
            files += 1
        data = (json.dumps(trimmed, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
        (out / uid / "index.json").write_bytes(data)
        files += 1
        root_out["packages"].append({"name": root_entries[uid]["name"], "sha256": sha256(data), "uid": uid})
    (out / "index.json").write_text(json.dumps(root_out, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{out}: {files + 1} metadata files and {jars} jar(s) copied from third-party hosts; packages: {', '.join(selected)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

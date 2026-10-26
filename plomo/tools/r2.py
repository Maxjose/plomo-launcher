#!/usr/bin/env python3
"""Talks to the PLOMO Cloudflare R2 bucket through its S3 API. No mounted drive and no rclone needed.

Credentials live in r2-config.json (never committed, see r2-config.example.json), or in the environment:
  R2_ENDPOINT, R2_BUCKET, R2_ACCESS_KEY_ID, R2_SECRET_ACCESS_KEY.  PLOMO_R2_CONFIG points to another config file.

  r2.py check                          credentials and bucket work
  r2.py sync <folder> <prefix>         upload every file under <prefix>/ (skips files already there with the same content)
  r2.py put <file> <key> [--content-type T]
  r2.py get <key>                      print an object as text
"""

import argparse
import concurrent.futures
import hashlib
import json
import mimetypes
import os
import sys
import threading
from pathlib import Path

import boto3
from boto3.exceptions import S3UploadFailedError
from botocore.config import Config
from botocore.exceptions import BotoCoreError, ClientError

CONFIG_NAME = "r2-config.json"
PLACEHOLDER_MARKERS = ("<", "ACCOUNT_ID", "BUCKET_NAME", "PEGA_")


class ConfigError(Exception):
    pass


def find_config_file() -> Path | None:
    explicit = os.environ.get("PLOMO_R2_CONFIG")
    if explicit:
        return Path(explicit)
    here = Path(__file__).resolve().parent
    for folder in [here, *list(here.parents)[:4]]:
        for candidate in (folder / CONFIG_NAME, folder / "tools" / CONFIG_NAME):
            if candidate.is_file():
                return candidate
    return None


def load_settings() -> dict:
    settings = {}
    path = find_config_file()
    if path is not None:
        try:
            settings = json.loads(path.read_text(encoding="utf-8-sig"))
        except (OSError, ValueError) as error:
            raise ConfigError(f"No se pudo leer {path}: {error}")
    env = {"endpoint": "R2_ENDPOINT", "bucket": "R2_BUCKET",
           "access_key_id": "R2_ACCESS_KEY_ID", "secret_access_key": "R2_SECRET_ACCESS_KEY"}
    for key, name in env.items():
        if os.environ.get(name):
            settings[key] = os.environ[name]
    missing = [k for k in env if not str(settings.get(k, "")).strip()]
    if missing:
        where = path if path else f"{CONFIG_NAME} (no se encontro)"
        raise ConfigError(f"Faltan datos de R2 ({', '.join(missing)}). Rellena {where}; hay una plantilla en r2-config.example.json.")
    for key in env:
        if any(marker in str(settings[key]) for marker in PLACEHOLDER_MARKERS):
            raise ConfigError(f"El campo '{key}' de {path} sigue con el texto de ejemplo: ponle el valor real.")
    if not str(settings["endpoint"]).startswith(("https://", "http://")):
        raise ConfigError("El campo 'endpoint' debe empezar por https:// (por ejemplo https://<ACCOUNT_ID>.r2.cloudflarestorage.com).")
    return settings


def make_client(settings: dict):
    return boto3.client(
        "s3",
        endpoint_url=settings["endpoint"],
        aws_access_key_id=settings["access_key_id"],
        aws_secret_access_key=settings["secret_access_key"],
        region_name="auto",
        config=Config(
            signature_version="s3v4",
            retries={"max_attempts": 5, "mode": "standard"},
            # R2 does not accept the optional checksum headers newer boto3 versions add by default.
            request_checksum_calculation="when_required",
            response_checksum_validation="when_required",
        ),
    )


def head_info(client, bucket: str, key: str) -> dict | None:
    try:
        return client.head_object(Bucket=bucket, Key=key)
    except ClientError as error:
        if error.response.get("Error", {}).get("Code") in ("404", "NoSuchKey", "NotFound"):
            return None
        raise


def file_hashes(path: Path) -> tuple[str, str]:
    md5, sha = hashlib.md5(), hashlib.sha256()
    with path.open("rb") as handle:
        while chunk := handle.read(1024 * 1024):
            md5.update(chunk)
            sha.update(chunk)
    return md5.hexdigest(), sha.hexdigest()


def same_content(head: dict | None, size: int, md5: str, sha: str) -> bool:
    """True when the object in R2 is the same file: same size and the same SHA-256 (stored by this script) or the same
    MD5 (the ETag of any single-part upload, e.g. the ones rclone made before)."""
    if head is None or head.get("ContentLength") != size:
        return False
    if head.get("Metadata", {}).get("sha256") == sha:
        return True
    etag = str(head.get("ETag", "")).strip('"')
    return "-" not in etag and etag == md5


def content_type(path: Path, override: str | None = None) -> str:
    if override:
        return override
    guessed, _ = mimetypes.guess_type(path.name)
    return guessed or "application/octet-stream"


def upload(client, bucket: str, path: Path, key: str, sha: str, ctype: str | None = None) -> None:
    client.upload_file(str(path), bucket, key,
                       ExtraArgs={"ContentType": content_type(path, ctype), "Metadata": {"sha256": sha}})
    remote = head_info(client, bucket, key)
    if remote is None or remote["ContentLength"] != path.stat().st_size:
        raise RuntimeError(f"{key}: el tamano en R2 no coincide con el del archivo local")


def cmd_check(client, settings, args) -> int:
    client.head_bucket(Bucket=settings["bucket"])
    count = client.list_objects_v2(Bucket=settings["bucket"], MaxKeys=1).get("KeyCount", 0)
    print(f"OK: conectado al bucket '{settings['bucket']}' ({'con' if count else 'sin'} objetos).")
    return 0


def cmd_sync(client, settings, args) -> int:
    folder = Path(args.folder)
    if not folder.is_dir():
        raise ConfigError(f"No existe la carpeta {folder}")
    prefix = args.prefix.strip("/")
    files = sorted(p for p in folder.rglob("*") if p.is_file())
    total = len(files)
    done = {"uploaded": 0, "skipped": 0, "bytes": 0, "seen": 0}
    lock = threading.Lock()

    def work(path: Path) -> None:
        key = f"{prefix}/{path.relative_to(folder).as_posix()}"
        size = path.stat().st_size
        md5, sha = file_hashes(path)
        if same_content(head_info(client, settings["bucket"], key), size, md5, sha):
            with lock:
                done["skipped"] += 1
                done["seen"] += 1
            return
        upload(client, settings["bucket"], path, key, sha)
        with lock:
            done["uploaded"] += 1
            done["bytes"] += size
            done["seen"] += 1
            if done["seen"] % 25 == 0 or done["seen"] == total:
                print(f"  {done['seen']}/{total} archivos revisados, {done['uploaded']} subidos "
                      f"({done['bytes'] / 1e6:.0f} MB)", flush=True)

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        for future in concurrent.futures.as_completed([pool.submit(work, p) for p in files]):
            future.result()  # re-raises the first failure
    print(f"OK: {total} archivos en {prefix}/ ({done['uploaded']} subidos, {done['skipped']} ya estaban).")
    return 0


def cmd_put(client, settings, args) -> int:
    path = Path(args.file)
    upload(client, settings["bucket"], path, args.key.lstrip("/"), file_hashes(path)[1], args.content_type)
    print(f"OK: subido {args.key} ({path.stat().st_size} bytes).")
    return 0


def cmd_get(client, settings, args) -> int:
    body = client.get_object(Bucket=settings["bucket"], Key=args.key.lstrip("/"))["Body"].read()
    sys.stdout.write(body.decode("utf-8", errors="replace"))
    return 0


def explain(code: str, text: str) -> str:
    """A short Spanish hint for the usual S3 errors (the raw message is kept at the end)."""
    hints = (("InvalidAccessKeyId", "la clave (access_key_id) no es valida"),
             ("SignatureDoesNotMatch", "la clave secreta (secret_access_key) no coincide"),
             ("AccessDenied", "el token no tiene permiso de lectura y escritura sobre ese bucket"),
             ("NoSuchBucket", "el bucket no existe o su nombre es incorrecto"),
             ("404", "el bucket no existe o su nombre es incorrecto"))
    for marker, hint in hints:
        if marker == code or marker in text:
            return f"{hint}. [{code or marker}]"
    return text


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    sub.add_parser("check")
    p = sub.add_parser("sync"); p.add_argument("folder"); p.add_argument("prefix"); p.add_argument("--workers", type=int, default=8)
    p = sub.add_parser("put"); p.add_argument("file"); p.add_argument("key"); p.add_argument("--content-type")
    p = sub.add_parser("get"); p.add_argument("key")
    args = parser.parse_args()
    handlers = {"check": cmd_check, "sync": cmd_sync, "put": cmd_put, "get": cmd_get}
    try:
        settings = load_settings()
        return handlers[args.command](make_client(settings), settings, args)
    except ConfigError as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1
    except ClientError as error:
        print(f"ERROR de R2: {explain(error.response.get('Error', {}).get('Code', ''), str(error))}", file=sys.stderr)
        return 1
    except S3UploadFailedError as error:  # boto3 wraps the S3 error of an upload in this one
        print(f"ERROR de R2: {explain('', str(error))}", file=sys.stderr)
        return 1
    except (BotoCoreError, RuntimeError, OSError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())

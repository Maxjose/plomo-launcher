import hashlib
import json
import tempfile
import unittest
from pathlib import Path

from update_instance import install, load_manifest, plan, safe_relative


class UpdateInstanceTests(unittest.TestCase):
    def test_first_install_and_delta_preserve_player_files(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            payload = root / "payload"
            payload.mkdir()
            game = root / "instance" / "minecraft"
            (game / "saves").mkdir(parents=True)
            (game / "saves" / "world.dat").write_bytes(b"player world")
            (game / "options.txt").write_text("player options")

            mod = payload / "mods" / "demo.jar"
            mod.parent.mkdir()
            mod.write_bytes(b"version one")
            entry = {"path": "mods/demo.jar", "size": mod.stat().st_size,
                     "sha256": hashlib.sha256(mod.read_bytes()).hexdigest()}
            manifest = {"schema": 1, "pack": "PLOMO", "version": "0.28",
                        "minecraft": "1.20.1", "forge": "47.4.16", "java": 17,
                        "mmc_pack": {"formatVersion": 1, "components": []},
                        "base_url": payload.as_uri() + "/", "files": [entry]}
            manifest_file = root / "manifest.json"
            manifest_file.write_text(json.dumps(manifest))
            loaded = load_manifest(str(manifest_file))
            self.assertEqual(len(plan(loaded, root / "instance")), 1)
            install(loaded, root / "instance", plan(loaded, root / "instance"))
            self.assertEqual(plan(loaded, root / "instance"), [])
            self.assertEqual((game / "saves" / "world.dat").read_bytes(), b"player world")
            self.assertEqual((game / "options.txt").read_text(), "player options")

            mod.write_bytes(b"version two")
            loaded["files"][0]["size"] = mod.stat().st_size
            loaded["files"][0]["sha256"] = hashlib.sha256(mod.read_bytes()).hexdigest()
            loaded["version"] = "0.29"
            self.assertEqual(len(plan(loaded, root / "instance")), 1)
            install(loaded, root / "instance", plan(loaded, root / "instance"))
            self.assertEqual((game / "mods" / "demo.jar").read_bytes(), b"version two")
            self.assertEqual((game / "saves" / "world.dat").read_bytes(), b"player world")

    def test_rejects_player_and_traversal_paths(self):
        for path in ("../outside", "saves/world.dat", "options.txt", "C:/outside"):
            with self.subTest(path=path), self.assertRaises(ValueError):
                safe_relative(path)


if __name__ == "__main__":
    unittest.main()

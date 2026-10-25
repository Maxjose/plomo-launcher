"""Self-check for the alpha server rewrite: python plomo/tools/test_build_pack_release.py"""
import build_pack_release as b

SAMPLE = b'\n[startup]\n\t#Connect automatically.\n\tautoConnect = true\n\t#PLOMO server address.\n\tserverAddress = "plomo.raidzone.online"\n'

out = b.set_server_address(SAMPLE, "alpha.raidzone.online")
assert b'serverAddress = "alpha.raidzone.online"' in out
assert out.replace(b"alpha.raidzone.online", b"plomo.raidzone.online") == SAMPLE  # nothing else changed
assert b.server_address(out) == "alpha.raidzone.online" and b.server_address(SAMPLE) == b.RELEASE_SERVER
try:
    b.set_server_address(b"[startup]\n", "x")
    raise SystemExit("expected ValueError when serverAddress is missing")
except ValueError:
    pass
# A release must get the real server even when the source instance carries the alpha address.
rel = b.set_server_address(out, b.RELEASE_SERVER)
assert b.server_address(rel) == "plomo.raidzone.online" and rel == SAMPLE
print("ok")

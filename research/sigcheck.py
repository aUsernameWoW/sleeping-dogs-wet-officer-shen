"""Checks IDA-style byte signatures against both game builds.

python sigcheck.py "Name=48 8B C4 ? ? 57" ["Other=..."]

Prints the match count and file offsets in the installed exe and the legacy v1.0 exe (the one with the PDB,
reference\\SDmodding\\game-itself). A signature is usable when it matches exactly once in both.
"""
import os
import re
import sys

from common import GAME, WORKSPACE

EXES = {
	"installed": os.path.join(GAME, "sdhdship.exe"),
	"legacy": os.path.join(WORKSPACE, "reference", "SDmodding", "game-itself", "sdhdship.exe"),
}


def main():
	sigs = dict(a.split("=", 1) for a in sys.argv[1:])
	data = {k: open(v, "rb").read() for k, v in EXES.items() if os.path.exists(v)}
	for name, sig in sigs.items():
		rx = b"".join(b"." if t.startswith("?") else re.escape(bytes([int(t, 16)])) for t in sig.split())
		for k, d in data.items():
			m = [x.start() for x in re.finditer(rx, d, re.S)]
			print("%-34s %-9s %d match(es) %s" % (name, k, len(m), " ".join(hex(x) for x in m[:4])))


if __name__ == "__main__":
	main()

"""How many character specular maps (*_S textures in Characters / CharactersHD) carry a wet mask.

python specsurvey.py    -> counts by category, the clothing items that have one; specsurvey.json

Run with tools\\extract's venv Python (decodes DXT). The _WS character shaders use texSpecular.x (and .z) as
the wet mask; y is the ordinary glossiness. "Has R data" = more than 5% of texels with x above ~6%.
Result on the DE data: 1/62 tops, 1/46 pants (P_SLACKS), 2/34 shoes, 5/27 hats, 78/87 hair maps.
"""
import collections
import json
import os
import re

from common import GAME, OUT, use_extract

use_extract()
from extract import Archives, load_names  # noqa: E402
from sdbig import extract  # noqa: E402
from textures import dds_bytes, parse_textures, to_image  # noqa: E402


def survey():
	names, arcs = load_names(), Archives()
	done, rows = set(), []
	for arc in ("Characters", "CharactersHD"):
		for uid, entry in arcs.index(arc).items():
			path = names.get(uid)
			if not path or not path.lower().endswith(".perm.bin"):
				continue
			perm = extract(os.path.join(GAME, arc + ".big"), entry)
			textures = [t for t in parse_textures(perm) if t["name"].upper().endswith("_S") and (arc, t["name"]) not in done]
			if not textures:
				continue
			temp = arcs.read(arc, path[:-len(".perm.bin")] + ".temp.bin") or b""
			for t in textures:
				pixels = temp[t["pos"]:t["pos"] + t["size"]]
				if len(pixels) != t["size"]:
					continue
				try:
					image = to_image(dds_bytes(t, pixels)).convert("RGB")
				except Exception:
					continue
				image.thumbnail((256, 256))
				n = image.size[0] * image.size[1]
				share = [round(sum(image.getchannel(c).histogram()[16:]) / n, 3) for c in "RGB"]
				done.add((arc, t["name"]))
				rows.append((arc, path, t["name"], *share))
	return rows


def category(name):
	name = name.upper()
	if "HAIR" in name:
		return "hair"
	m = re.match(r"^(T|P|S|H|G|A|J|W|O)_", name)
	return "clothing %s_" % m.group(1) if m else "other (props, NPC bodies, heads)"


def main():
	rows = survey()
	json.dump(rows, open(os.path.join(OUT, "specsurvey.json"), "w"), indent=0)
	print(len(rows), "specular maps ->", os.path.join(OUT, "specsurvey.json"))
	counts = collections.defaultdict(lambda: [0, 0])
	for _, _, name, r, _, _ in rows:
		counts[category(name)][0] += 1
		counts[category(name)][1] += r > 0.05
	for kind, (n, wet) in sorted(counts.items()):
		print("%-34s %4d / %4d have R data" % (kind, wet, n))
	print("clothing with R data:", sorted({r[2] for r in rows if category(r[2]).startswith("clothing") and r[3] > 0.05}))


if __name__ == "__main__":
	main()

# qrenderdoc --python report.py   (use ..\renderdoc.ps1 report <capture>)
# Env: RDC capture, OUT output dir, NAMES names.json from shaders.py unpack.
# Every draw with a character pixel shader: the permutation (named by the DXBC checksum; "+SDWet" = patched),
# cbSceneryInstance.Mask (.y sweat, .z wetness, .w charred) and the diffuse texture, saved as tex_<id>.png so
# draws can be matched to clothing pieces. Writes report.txt (per draw + summary) and draws.json.
import collections
import json
import os
import traceback

import renderdoc as rd

RDC = os.environ["RDC"]
OUT = os.environ["OUT"]
os.makedirs(OUT, exist_ok=True)
log = open(os.path.join(OUT, "report.txt"), "w", encoding="utf-8")


def P(*a):
	print(*a, file=log, flush=True)


def main():
	names = json.load(open(os.environ["NAMES"]))
	cap = rd.OpenCaptureFile()
	if cap.OpenFile(RDC, "", None) != rd.ResultCode.Succeeded:
		P("cannot open", RDC)
		return
	res, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
	if res != rd.ResultCode.Succeeded:
		P("cannot replay:", res)
		return

	def walk(actions):
		for a in actions:
			if a.flags & rd.ActionFlags.Drawcall:
				yield a
			yield from walk(a.children)

	saved, rows = set(), []
	for a in walk(ctl.GetRootActions()):
		ctl.SetFrameEvent(a.eventId, False)
		st = ctl.GetPipelineState()
		ps = st.GetShaderReflection(rd.ShaderStage.Pixel)
		if ps is None:
			continue
		name = names.get(bytes(ps.rawBytes[4:20]).hex(), "?")
		if not any(k in name for k in ("HK_CHARACTER", "HK_HAIR", "HK_SKINSS")):
			continue
		mask = None
		for i, cb in enumerate(ps.constantBlocks):
			if cb.name != "cbSceneryInstance":
				continue
			d = st.GetConstantBlock(rd.ShaderStage.Pixel, i, 0).descriptor
			for v in ctl.GetCBufferVariableContents(st.GetGraphicsPipelineObject(), ps.resourceId, rd.ShaderStage.Pixel,
					st.GetShaderEntryPoint(rd.ShaderStage.Pixel), i, d.resource, d.byteOffset, d.byteSize):
				for m in v.members:
					if m.name == "Mask":
						mask = [round(x, 3) for x in m.value.f32v[:4]]
		textures = {}
		for ro in st.GetReadOnlyResources(rd.ShaderStage.Pixel):
			if ro.access.index < len(ps.readOnlyResources):
				textures[ps.readOnlyResources[ro.access.index].name] = ro.descriptor.resource
		diffuse = textures.get("texDiffuse")
		png = ""
		if diffuse is not None and diffuse != rd.ResourceId.Null():
			png = "tex_%d.png" % int(diffuse)
			if diffuse not in saved:
				saved.add(diffuse)
				ts = rd.TextureSave()
				ts.resourceId = diffuse
				ts.destType = rd.FileType.PNG
				ts.mip = 1
				ts.alpha = rd.AlphaMapping.Discard
				ctl.SaveTexture(ts, os.path.join(OUT, png))
		short = name.split("|")[-1].replace(".PSBIN", "")
		rows.append((a.eventId, short, mask, a.numIndices, png))
		P("%6d  %-36s mask=%s indices=%d %s" % rows[-1])

	P("\nsummary (permutation, Mask = [x, sweat, wetness, charred]):")
	for (short, mask), n in sorted(collections.Counter((r[1], str(r[2])) for r in rows).items()):
		P("%4d  %-36s %s" % (n, short, mask))
	json.dump(rows, open(os.path.join(OUT, "draws.json"), "w"))
	ctl.Shutdown()
	cap.Shutdown()


try:
	main()
except Exception:
	P(traceback.format_exc())
log.close()
os._exit(0)

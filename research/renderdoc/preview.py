# qrenderdoc --python preview.py   (use ..\renderdoc.ps1 preview <capture>)
# Env: RDC capture, VARIANTS variants.json from shaders.py unpack, OUT png prefix.
# Replays the frame once as captured and once per variant (de = the game's original, default = SDWet's
# defaults, strong = ...), with every wet character pixel shader of the frame replaced by that variant's
# bytecode, and saves each presented image as <OUT>_<tag>.png. Works on captures made with or without SDWet.
import json
import os
import traceback

import renderdoc as rd


def main():
	variants = json.load(open(os.environ["VARIANTS"]))
	out = os.environ["OUT"]
	log = open(out + "_log.txt", "w")
	cap = rd.OpenCaptureFile()
	cap.OpenFile(os.environ["RDC"], "", None)
	_, ctl = cap.OpenCapture(rd.ReplayOptions(), None)

	draws, present = [], None

	def walk(actions):
		nonlocal present
		for a in actions:
			if a.flags & rd.ActionFlags.Drawcall:
				draws.append(a)
			if a.flags & rd.ActionFlags.Present:
				present = a
			walk(a.children)

	walk(ctl.GetRootActions())

	targets = {}  # pixel shader resource id -> checksum
	for a in draws:
		ctl.SetFrameEvent(a.eventId, False)
		st = ctl.GetPipelineState()
		ps = st.GetShaderReflection(rd.ShaderStage.Pixel)
		if ps is not None and bytes(ps.rawBytes[4:20]).hex() in variants["map"]:
			targets[st.GetShader(rd.ShaderStage.Pixel)] = bytes(ps.rawBytes[4:20]).hex()
	log.write("%d wet shaders used in the frame\n" % len(targets))

	def save(tag):
		ctl.SetFrameEvent(present.eventId, True)
		ts = rd.TextureSave()
		ts.resourceId = present.copyDestination
		ts.destType = rd.FileType.PNG
		ts.alpha = rd.AlphaMapping.Discard
		ctl.SaveTexture(ts, "%s_%s.png" % (out, tag))

	save("captured")
	for tag in variants["variants"]:
		built = []
		for rid, key in targets.items():
			blob = open(variants["map"][key][tag], "rb").read()
			new_id, errors = ctl.BuildTargetShader("main", rd.ShaderEncoding.DXBC, blob, rd.ShaderCompileFlags(), rd.ShaderStage.Pixel)
			if new_id == rd.ResourceId.Null():
				log.write("%s: build failed: %s\n" % (tag, errors))
				continue
			ctl.ReplaceResource(rid, new_id)
			built.append((rid, new_id))
		save(tag)
		log.write("%s: replaced %d\n" % (tag, len(built)))
		for rid, new_id in built:
			ctl.RemoveReplacement(rid)
			ctl.FreeTargetResource(new_id)
	log.close()
	ctl.Shutdown()
	cap.Shutdown()


try:
	main()
except Exception:
	open(os.environ["OUT"] + ".err.txt", "w").write(traceback.format_exc())
os._exit(0)

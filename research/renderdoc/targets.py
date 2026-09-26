# qrenderdoc --python targets.py   (use ..\renderdoc.ps1 targets <capture> -Event N)
# Env: RDC capture, EID event, OUT path prefix. Saves every render target bound at the event as
# <OUT>_rt<i>_rgb.png and <OUT>_rt<i>_a.png. At the end of the character G-buffer pass: rt0 = sqrt(albedo),
# rt1.xyz = normal, rt1.w = glossiness. The G-buffer is lighting-independent, so dry and wet frames compare.
import os
import traceback

import renderdoc as rd


def main():
	cap = rd.OpenCaptureFile()
	cap.OpenFile(os.environ["RDC"], "", None)
	_, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
	ctl.SetFrameEvent(int(os.environ["EID"]), True)
	info = []
	for i, target in enumerate(ctl.GetPipelineState().GetOutputTargets()):
		rid = target.resource
		if rid == rd.ResourceId.Null():
			continue
		tex = next(t for t in ctl.GetTextures() if t.resourceId == rid)
		info.append("rt%d %s %dx%d" % (i, tex.format.Name(), tex.width, tex.height))
		for suffix, channel in (("rgb", -1), ("a", 3)):
			ts = rd.TextureSave()
			ts.resourceId = rid
			ts.destType = rd.FileType.PNG
			ts.alpha = rd.AlphaMapping.Discard
			ts.channelExtract = channel
			ctl.SaveTexture(ts, "%s_rt%d_%s.png" % (os.environ["OUT"], i, suffix))
	open(os.environ["OUT"] + "_targets.txt", "w").write("\n".join(info))
	ctl.Shutdown()
	cap.Shutdown()


try:
	main()
except Exception:
	open(os.environ["OUT"] + ".err.txt", "w").write(traceback.format_exc())
os._exit(0)

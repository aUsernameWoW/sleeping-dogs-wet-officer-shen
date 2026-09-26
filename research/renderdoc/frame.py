# qrenderdoc --python frame.py   (use ..\renderdoc.ps1 frame <capture>)
# Env: RDC capture, OUT png path. Saves the image the frame presented.
import os
import traceback

import renderdoc as rd


def main():
	cap = rd.OpenCaptureFile()
	cap.OpenFile(os.environ["RDC"], "", None)
	_, ctl = cap.OpenCapture(rd.ReplayOptions(), None)
	present = None

	def walk(actions):
		nonlocal present
		for a in actions:
			if a.flags & rd.ActionFlags.Present:
				present = a
			walk(a.children)

	walk(ctl.GetRootActions())
	ctl.SetFrameEvent(present.eventId, True)
	ts = rd.TextureSave()
	ts.resourceId = present.copyDestination
	ts.destType = rd.FileType.PNG
	ts.alpha = rd.AlphaMapping.Discard
	ctl.SaveTexture(ts, os.environ["OUT"])
	ctl.Shutdown()
	cap.Shutdown()


try:
	main()
except Exception:
	open(os.environ["OUT"] + ".err.txt", "w").write(traceback.format_exc())
os._exit(0)

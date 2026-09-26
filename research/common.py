"""Paths shared by the research scripts.

Output goes to mods\\SDWet\\build\\research, which is gitignored: it is game data (shader bytecode, textures,
captures) and must not be committed.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
MOD = os.path.dirname(HERE)
WORKSPACE = os.path.dirname(os.path.dirname(MOD))
GAME = os.environ.get("SDDE_DIR", r"D:\SteamLibrary\steamapps\common\SleepingDogsDefinitiveEdition")
OUT = os.path.join(MOD, "build", "research")
SHADERS = os.path.join(OUT, "shaders")  # every blob of data\shaders\shaders.temp.bin, named like HK_CHARACTER_DR_WS.PSBIN
FXC = os.environ.get("FXC", r"C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\fxc.exe")
EXTRACT = os.path.join(WORKSPACE, "tools", "extract")  # the workspace's archive/texture reader

os.makedirs(OUT, exist_ok=True)


def use_extract():
	"""Makes tools\\extract's modules importable (sdbig, textures, extract). Run with its venv's Python
	(tools\\extract\\build\\venv, created by tools\\extract.ps1) when textures are decoded (Pillow)."""
	if EXTRACT not in sys.path:
		sys.path.insert(0, EXTRACT)

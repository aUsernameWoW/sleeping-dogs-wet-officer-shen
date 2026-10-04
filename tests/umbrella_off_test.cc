// Loads SDWet.asi with the umbrella switched off ([Umbrella] Enabled = 0) into a process that isn't the game: it
// must say so and not look for the umbrella's game functions. argv[1] = path to the .asi (build.ps1 passes a sandbox
// copy).

#include <Windows.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv)
{
	if (argc < 2) {
		std::printf("usage: umbrella_off_test <SDWet.asi>\n");
		return 2;
	}

	std::string dir = argv[1];
	dir = dir.substr(0, dir.find_last_of("\\/") + 1);

	// Only the key under test: everything else keeps its default.
	DeleteFileA((dir + "SDWet.log").c_str());
	{
		std::ofstream ini(dir + "SDWet.ini", std::ios::trunc);
		ini << "[Umbrella]\nEnabled = 0\n";
	}

	HMODULE module = LoadLibraryA(argv[1]);
	if (!module) {
		std::printf("FAIL: LoadLibrary error %lu\n", GetLastError());
		return 1;
	}

	std::ifstream log(dir + "SDWet.log");
	std::stringstream text;
	text << log.rdbuf();
	const std::string contents = text.str();
	std::printf("%s", contents.c_str());

	const char* expected[] = {
		"Umbrella=0",
		"umbrella: off",
	};
	for (const char* line : expected) {
		if (contents.find(line) == std::string::npos) {
			std::printf("FAIL: log lacks \"%s\"\n", line);
			return 1;
		}
	}

	std::printf("PASS\n");
	return 0;
}

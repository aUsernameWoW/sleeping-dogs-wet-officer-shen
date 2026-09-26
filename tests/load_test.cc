// Loads SDWet.asi into a process that isn't the game: it must not crash, must write its default ini and
// must report both game functions missing. argv[1] = path to the .asi (build.ps1 passes a sandbox copy).

#include <Windows.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char** argv)
{
	if (argc < 2) {
		std::printf("usage: load_test <SDWet.asi>\n");
		return 2;
	}

	HMODULE module = LoadLibraryA(argv[1]);
	if (!module) {
		std::printf("FAIL: LoadLibrary error %lu\n", GetLastError());
		return 1;
	}

	std::string dir = argv[1];
	dir = dir.substr(0, dir.find_last_of("\\/") + 1);

	std::ifstream ini(dir + "SDWet.ini");
	if (!ini) {
		std::printf("FAIL: no default SDWet.ini written\n");
		return 1;
	}

	std::ifstream log(dir + "SDWet.log");
	std::stringstream text;
	text << log.rdbuf();
	const std::string contents = text.str();
	std::printf("%s", contents.c_str());

	const char* expected[] = {
		"SDWet loaded (WetLook=1 Shine=0.050 Gloss=0.100",
		"Illusion::StageShader::LoadShader: 0 matches",
		"LoadShader MISSING, wet look off",
		"ApplyWetnessOrSweatTask::Begin: 0 matches",
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

// Runs the patch over every shader in the installed game's data\shaders\shaders.temp.bin: checksums must
// verify, exactly the _WS character permutations must be patched, and D3D11 (WARP) must accept each result.
// Skips (exit 0) where the game isn't installed, e.g. in CI. Game folder: %SDDE_DIR% or the default below.

#include "../core/dxbc.cc"

#include <Windows.h>
#include <d3d11.h>

#include <cctype>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>

#pragma comment(lib, "d3d11.lib")

int main()
{
	char dir[MAX_PATH] = "D:\\SteamLibrary\\steamapps\\common\\SleepingDogsDefinitiveEdition";
	GetEnvironmentVariableA("SDDE_DIR", dir, sizeof(dir));
	std::ifstream file(std::string(dir) + "\\data\\shaders\\shaders.temp.bin", std::ios::binary);
	if (!file) {
		std::printf("SKIP: no shaders.temp.bin under %s\n", dir);
		return 0;
	}
	const std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

	ID3D11Device* device = nullptr;
	if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, nullptr))) {
		std::printf("FAIL: no WARP device\n");
		return 1;
	}

	int shaders = 0, badChecksums = 0, failed = 0, rejected = 0;
	std::set<std::string> patchedNames;
	const dxbc::WetDefaults defaults;
	for (size_t at = 0; at + 32 <= data.size(); ++at) {
		if (std::memcmp(&data[at], "DXBC", 4) != 0) {
			continue;
		}
		const uint32_t size = dxbc::Read32(&data[at + 24]);
		if (size < 32 || at + size > data.size()) {
			continue;
		}
		++shaders;
		const uint8_t* blob = &data[at];
		uint8_t sum[16];
		dxbc::Checksum(blob, size, sum);
		badChecksums += std::memcmp(sum, blob + 4, 16) != 0;

		// The ShaderBinary header right before the bytecode holds the name, e.g. "HK_CHARACTER_DR_WS.PSBIN".
		const std::string header(reinterpret_cast<const char*>(&data[at > 128 ? at - 128 : 0]), at > 128 ? 128 : at);
		std::string name;
		if (const size_t end = header.rfind("SBIN"); end != std::string::npos) {
			size_t begin = end;
			while (begin > 0 && (std::isalnum(static_cast<unsigned char>(header[begin - 1])) || header[begin - 1] == '_' || header[begin - 1] == '.')) {
				--begin;
			}
			name = header.substr(begin, end + 4 - begin);
		}

		std::vector<uint8_t> out;
		std::string why;
		switch (dxbc::PatchWetShader(blob, size, defaults, out, why)) {
		case dxbc::Result::Patched: {
			patchedNames.insert(name);
			ID3D11PixelShader* ps = nullptr;
			if (FAILED(device->CreatePixelShader(out.data(), out.size(), nullptr, &ps))) {
				++rejected;
				std::printf("rejected: %s\n", name.c_str());
			}
			else {
				ps->Release();
			}
			break;
		}
		case dxbc::Result::Failed:
			++failed;
			std::printf("failed: %s: %s\n", name.c_str(), why.c_str());
			break;
		case dxbc::Result::NotWet:
			// _WS on HK_ROAD etc. is the wet road surface, not a character.
			if (name.find("CHARACTER") != std::string::npos && name.find("_WS") != std::string::npos && name.find(".PSBIN") != std::string::npos) {
				++failed;
				std::printf("missed: %s: %s\n", name.c_str(), why.c_str());
			}
			break;
		}
	}
	device->Release();

	for (const std::string& name : patchedNames) {
		if (name.find("_WS") == std::string::npos || name.find("CHARACTER") == std::string::npos) {
			++failed;
			std::printf("patched a shader that isn't a _WS character permutation: %s\n", name.c_str());
		}
	}
	std::printf("%d shaders, %d bad checksums, %zu patched, %d failed, %d rejected by D3D11\n", shaders, badChecksums, patchedNames.size(), failed, rejected);
	if (shaders < 2000 || badChecksums || failed || rejected || patchedNames.size() != 28) {
		std::printf("FAIL\n");
		return 1;
	}
	std::printf("PASS\n");
	return 0;
}

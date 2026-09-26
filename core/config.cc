#include "config.hh"

#include <Windows.h>

#include <cstdio>
#include <cwchar>

Config gConfig;

namespace config
{
	static std::wstring gPath;

	static constexpr char kDefaultIni[] =
		"; SDWet 配置 / configuration\n"
		"; 1 = 开启 (on), 0 = 关闭 (off)\n"
		"\n"
		"[Wet]\n"
		"; 淋雨或出汗时让衣服和皮肤显得湿润。改了要重启游戏才生效。\n"
		"; Make clothes and skin look wet in the rain (and sweaty). Takes effect after restarting the game.\n"
		"WetLook = 1\n"
		"\n"
		"; 湿了以后的反光强度。0 = 游戏原样，0.05 = 默认，0.1 以上接近塑料感。\n"
		"; 自带湿身效果的衣服（比如西裤）不受这两项影响。\n"
		"; How much wet clothes and skin shine. 0 = the game as is, 0.05 = default, 0.1+ looks like plastic.\n"
		"; Clothes that already have a wet look of their own (e.g. slacks) keep it.\n"
		"Shine = 0.05\n"
		"\n"
		"; 额外的湿润光泽。 / Extra wet gloss.\n"
		"Gloss = 0.10\n"
		"\n"
		"; 游泳上岸后湿透、打伞后变干（游戏本来就有这些动作，但 DE 里它们找不到角色，所以没效果）。\n"
		"; Wet after swimming, dried under an umbrella: the game's own actions, which miss the character in the DE.\n"
		"ActionWetness = 1\n"
		"\n"
		"[Debug]\n"
		"; 在 .asi 旁边写 SDWet.log。 / Write SDWet.log.\n"
		"Logging = 1\n"
		"\n"
		"; 把游戏设置湿度/汗水的动作（游泳、打伞、剧情）写进日志。\n"
		"; Log the actions that set wetness or sweat (swimming, umbrellas, scripted scenes).\n"
		"LogWetnessTracks = 0\n";

	static std::wstring ReadString(const wchar_t* section, const wchar_t* key)
	{
		wchar_t text[64] = {};
		GetPrivateProfileStringW(section, key, L"", text, ARRAYSIZE(text), gPath.c_str());
		return text;
	}

	static bool ReadBool(const wchar_t* section, const wchar_t* key, bool fallback)
	{
		return GetPrivateProfileIntW(section, key, fallback ? 1 : 0, gPath.c_str()) != 0;
	}

	static float ReadFloat(const wchar_t* section, const wchar_t* key, float fallback)
	{
		const std::wstring text = ReadString(section, key);
		wchar_t* end = nullptr;
		const float value = std::wcstof(text.c_str(), &end);
		return end != text.c_str() ? value : fallback;
	}

	void Load(const std::wstring& dir)
	{
		gPath = dir + L"\\SDWet.ini";

		if (GetFileAttributesW(gPath.c_str()) == INVALID_FILE_ATTRIBUTES)
		{
			FILE* file = nullptr;
			if (_wfopen_s(&file, gPath.c_str(), L"wb") == 0 && file)
			{
				fwrite(kDefaultIni, 1, sizeof(kDefaultIni) - 1, file);
				fclose(file);
			}
		}

		gConfig.mWetLook = ReadBool(L"Wet", L"WetLook", gConfig.mWetLook);
		gConfig.mShine = ReadFloat(L"Wet", L"Shine", gConfig.mShine);
		gConfig.mGloss = ReadFloat(L"Wet", L"Gloss", gConfig.mGloss);
		gConfig.mActionWetness = ReadBool(L"Wet", L"ActionWetness", gConfig.mActionWetness);
		gConfig.mLogging = ReadBool(L"Debug", L"Logging", gConfig.mLogging);
		gConfig.mLogWetnessTracks = ReadBool(L"Debug", L"LogWetnessTracks", gConfig.mLogWetnessTracks);
	}
}

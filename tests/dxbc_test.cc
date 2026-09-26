// The shader patch on a stand-in for the game's wet/sweat pixel shaders, compiled here with D3DCompile:
// checksum, recognition, D3D11 accepting the patched bytecode, and what it renders on WARP with and without a
// wet mask in the specular map. argv[1] (the .asi) is unused.

#include "../core/dxbc.cc"

#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include <cmath>
#include <cstdio>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

static int gFailures = 0;

#define CHECK(cond, ...)                                                                      \
	do {                                                                                      \
		if (!(cond)) {                                                                        \
			std::printf("FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond);                       \
			std::printf(__VA_ARGS__);                                                         \
			std::printf("\n");                                                                \
			++gFailures;                                                                      \
		}                                                                                     \
	} while (0)

// Same bindings as HK_CHARACTER_DR_WS: cbSceneryInstance {ColourTint, SIColourTint, Mask, Value0}, texSpecular.
static const char kWetShader[] = R"(
struct SceneryInstance { float4 ColourTint; float4 SIColourTint; float4 Mask; float4 Value0; };
cbuffer cbSceneryInstance : register(b0) { SceneryInstance cbSceneryInstance; }
Texture2D texDiffuse : register(t0);
Texture2D texSpecular : register(t3);
SamplerState sDiffuse : register(s0);
SamplerState sSpecular : register(s3);
float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	float wet = max(cbSceneryInstance.Mask.z, cbSceneryInstance.Mask.y);
	float4 spec = texSpecular.Sample(sSpecular, uv);
	return float4(wet * spec.x * 10 + spec.y, spec.z * wet, spec.x, spec.z) + texDiffuse.Sample(sDiffuse, uv).w * 0;
}
)";

// Reads only the charred amount (Mask.w), like the permutations without _WS.
static const char kDryShader[] = R"(
struct SceneryInstance { float4 ColourTint; float4 SIColourTint; float4 Mask; float4 Value0; };
cbuffer cbSceneryInstance : register(b0) { SceneryInstance cbSceneryInstance; }
Texture2D texSpecular : register(t3);
SamplerState sSpecular : register(s3);
float4 main(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target
{
	return texSpecular.Sample(sSpecular, uv) * cbSceneryInstance.Mask.w;
}
)";

static const char kVertexShader[] = R"(
void main(uint id : SV_VertexID, out float4 pos : SV_Position, out float2 uv : TEXCOORD0)
{
	uv = float2((id << 1) & 2, id & 2);
	pos = float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}
)";

static std::vector<uint8_t> Compile(const char* source, const char* profile)
{
	ID3DBlob* code = nullptr;
	ID3DBlob* errors = nullptr;
	const HRESULT hr = D3DCompile(source, std::strlen(source), nullptr, nullptr, nullptr, "main", profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
	if (FAILED(hr)) {
		std::printf("compile error: %s\n", errors ? static_cast<const char*>(errors->GetBufferPointer()) : "?");
		std::exit(1);
	}
	const auto* p = static_cast<const uint8_t*>(code->GetBufferPointer());
	std::vector<uint8_t> out(p, p + code->GetBufferSize());
	code->Release();
	return out;
}

struct Renderer
{
	ID3D11Device* mDevice = nullptr;
	ID3D11DeviceContext* mContext = nullptr;
	ID3D11VertexShader* mVs = nullptr;

	bool Init()
	{
		if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &mDevice, nullptr, &mContext))) {
			return false;
		}
		const std::vector<uint8_t> vs = Compile(kVertexShader, "vs_4_0");
		return SUCCEEDED(mDevice->CreateVertexShader(vs.data(), vs.size(), nullptr, &mVs));
	}

	// Renders one pixel with `ps`, the given specular texel and cbSceneryInstance.Mask.
	bool Run(ID3D11PixelShader* ps, const float spec[4], const float mask[4], float out[4])
	{
		ID3D11Texture2D* tex = nullptr;
		ID3D11ShaderResourceView* srv = nullptr;
		ID3D11Texture2D* target = nullptr;
		ID3D11RenderTargetView* rtv = nullptr;
		ID3D11Texture2D* staging = nullptr;
		ID3D11Buffer* cb = nullptr;
		ID3D11SamplerState* sampler = nullptr;

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = desc.Height = 1;
		desc.MipLevels = desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		desc.SampleDesc.Count = 1;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		D3D11_SUBRESOURCE_DATA init = { spec, 16, 16 };
		bool ok = SUCCEEDED(mDevice->CreateTexture2D(&desc, &init, &tex)) && SUCCEEDED(mDevice->CreateShaderResourceView(tex, nullptr, &srv));

		desc.BindFlags = D3D11_BIND_RENDER_TARGET;
		ok = ok && SUCCEEDED(mDevice->CreateTexture2D(&desc, nullptr, &target)) && SUCCEEDED(mDevice->CreateRenderTargetView(target, nullptr, &rtv));
		desc.BindFlags = 0;
		desc.Usage = D3D11_USAGE_STAGING;
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		ok = ok && SUCCEEDED(mDevice->CreateTexture2D(&desc, nullptr, &staging));

		float constants[16] = {};
		std::memcpy(constants + 8, mask, 16);
		D3D11_BUFFER_DESC cbDesc = { sizeof(constants), D3D11_USAGE_DEFAULT, D3D11_BIND_CONSTANT_BUFFER };
		D3D11_SUBRESOURCE_DATA cbInit = { constants };
		ok = ok && SUCCEEDED(mDevice->CreateBuffer(&cbDesc, &cbInit, &cb));

		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		ok = ok && SUCCEEDED(mDevice->CreateSamplerState(&samplerDesc, &sampler));

		if (ok) {
			ID3D11ShaderResourceView* srvs[4] = { srv, nullptr, nullptr, srv };
			ID3D11SamplerState* samplers[4] = { sampler, sampler, sampler, sampler };
			const D3D11_VIEWPORT viewport = { 0, 0, 1, 1, 0, 1 };
			mContext->OMSetRenderTargets(1, &rtv, nullptr);
			mContext->RSSetViewports(1, &viewport);
			mContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			mContext->VSSetShader(mVs, nullptr, 0);
			mContext->PSSetShader(ps, nullptr, 0);
			mContext->PSSetShaderResources(0, 4, srvs);
			mContext->PSSetSamplers(0, 4, samplers);
			mContext->PSSetConstantBuffers(0, 1, &cb);
			mContext->Draw(3, 0);
			mContext->CopyResource(staging, target);
			D3D11_MAPPED_SUBRESOURCE mapped;
			ok = SUCCEEDED(mContext->Map(staging, 0, D3D11_MAP_READ, 0, &mapped));
			if (ok) {
				std::memcpy(out, mapped.pData, 16);
				mContext->Unmap(staging, 0);
			}
		}

		for (IUnknown* object : { static_cast<IUnknown*>(tex), static_cast<IUnknown*>(srv), static_cast<IUnknown*>(target), static_cast<IUnknown*>(rtv),
				 static_cast<IUnknown*>(staging), static_cast<IUnknown*>(cb), static_cast<IUnknown*>(sampler) }) {
			if (object) {
				object->Release();
			}
		}
		return ok;
	}
};

static bool Near(const float a[4], const float b[4])
{
	for (int i = 0; i < 4; ++i) {
		if (std::fabs(a[i] - b[i]) > 1e-4f) {
			return false;
		}
	}
	return true;
}

int main()
{
	const std::vector<uint8_t> wet = Compile(kWetShader, "ps_4_0");
	const std::vector<uint8_t> dry = Compile(kDryShader, "ps_4_0");

	uint8_t sum[16];
	dxbc::Checksum(wet.data(), wet.size(), sum);
	CHECK(std::memcmp(sum, wet.data() + 4, 16) == 0, "checksum differs from the compiler's");

	const dxbc::WetDefaults defaults{ 0.05f, 0.1f };
	std::vector<uint8_t> patched, unused;
	std::string why;
	CHECK(dxbc::PatchWetShader(dry.data(), dry.size(), defaults, unused, why) == dxbc::Result::NotWet, "dry shader: %s", why.c_str());
	CHECK(dxbc::PatchWetShader(reinterpret_cast<const uint8_t*>(kWetShader), 64, defaults, unused, why) == dxbc::Result::NotWet, "garbage: %s", why.c_str());
	CHECK(dxbc::PatchWetShader(wet.data(), wet.size(), defaults, patched, why) == dxbc::Result::Patched, "wet shader: %s", why.c_str());
	std::printf("patched: %zu -> %zu bytes (%s)\n", wet.size(), patched.size(), why.c_str());

	// Truncated or corrupted input must be rejected, not read out of bounds.
	for (size_t length = 0; length < wet.size(); length += 7) {
		std::vector<uint8_t> cut(wet.begin(), wet.begin() + length);
		CHECK(dxbc::PatchWetShader(cut.data(), cut.size(), defaults, unused, why) != dxbc::Result::Patched, "truncated to %zu", length);
	}
	for (size_t at = 32; at < wet.size(); at += 13) {
		std::vector<uint8_t> bad = wet;
		bad[at] ^= 0xA5;
		dxbc::PatchWetShader(bad.data(), bad.size(), defaults, unused, why);
	}

	ID3DBlob* disassembly = nullptr;
	if (SUCCEEDED(D3DDisassemble(patched.data(), patched.size(), 0, nullptr, &disassembly))) {
		const char* text = static_cast<const char*>(disassembly->GetBufferPointer());
		CHECK(std::strstr(text, "movc") && std::strstr(text, "l(0.050000,0,0.100000,0)"), "fallback not in the disassembly");
		disassembly->Release();
	}

	ID3D11ShaderReflection* before = nullptr;
	ID3D11ShaderReflection* after = nullptr;
	if (SUCCEEDED(D3DReflect(wet.data(), wet.size(), IID_PPV_ARGS(&before))) && SUCCEEDED(D3DReflect(patched.data(), patched.size(), IID_PPV_ARGS(&after)))) {
		D3D11_SHADER_DESC a, b;
		before->GetDesc(&a);
		after->GetDesc(&b);
		CHECK(b.InstructionCount == a.InstructionCount + 3 && b.TempRegisterCount == a.TempRegisterCount + 1 && b.BoundResources == a.BoundResources,
			"reflection: %u/%u instructions, %u/%u temps", a.InstructionCount, b.InstructionCount, a.TempRegisterCount, b.TempRegisterCount);
		before->Release();
		after->Release();
	}
	else {
		CHECK(false, "D3DReflect failed on the patched shader");
	}

	Renderer renderer;
	if (!renderer.Init()) {
		std::printf("FAIL: no WARP device\n");
		return 1;
	}
	ID3D11PixelShader* original = nullptr;
	ID3D11PixelShader* fixed = nullptr;
	CHECK(SUCCEEDED(renderer.mDevice->CreatePixelShader(wet.data(), wet.size(), nullptr, &original)), "original rejected");
	CHECK(SUCCEEDED(renderer.mDevice->CreatePixelShader(patched.data(), patched.size(), nullptr, &fixed)), "patched shader rejected by D3D11");
	if (!original || !fixed) {
		return 1;
	}

	const float wetMask[4] = { 0, 0, 1, 0 };   // soaked
	const float noMask[4] = { 0, 0.5f, 0, 1 };   // DE clothing: only glossiness in the specular map
	const float ownMask[4] = { 0.2f, 0.5f, 0.3f, 1 }; // an item with its own wet mask
	float out[4];

	CHECK(renderer.Run(original, noMask, wetMask, out), "render");
	const float dryLook[4] = { 0.5f, 0, 0, 0 };
	CHECK(Near(out, dryLook), "original without a mask: %g %g %g %g", out[0], out[1], out[2], out[3]);

	CHECK(renderer.Run(fixed, noMask, wetMask, out), "render");
	const float fallback[4] = { 1.0f, 0.1f, 0.05f, 0.1f };
	CHECK(Near(out, fallback), "patched without a mask: %g %g %g %g", out[0], out[1], out[2], out[3]);

	CHECK(renderer.Run(fixed, ownMask, wetMask, out), "render");
	const float own[4] = { 2.5f, 0.3f, 0.2f, 0.3f };
	CHECK(Near(out, own), "patched with its own mask: %g %g %g %g", out[0], out[1], out[2], out[3]);

	const float dryMask[4] = { 0, 0, 0, 0 };
	CHECK(renderer.Run(fixed, noMask, dryMask, out), "render");
	const float dryFallback[4] = { 0.5f, 0, 0.05f, 0.1f };
	CHECK(Near(out, dryFallback), "patched while dry: %g %g %g %g", out[0], out[1], out[2], out[3]);

	original->Release();
	fixed->Release();

	if (gFailures) {
		return 1;
	}
	std::printf("PASS\n");
	return 0;
}

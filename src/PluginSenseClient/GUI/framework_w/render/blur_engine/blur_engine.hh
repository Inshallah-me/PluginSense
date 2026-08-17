#pragma once

#define DX11_RELEASE(x) if (x) { x->Release(); x = nullptr; }

const char blur_shader_x[] = R"(
Texture2D inputTexture;
SamplerState textureSampler : register(s0);

cbuffer PS_Blur_Constants : register(b0) {
    float pixel_x; 
    float pixel_y;
    float screen_width;
    float screen_height;
    float blur_scale;
    float blur_weights[400];
};

float4 main(float4 texCoord : SV_Position, float4 inputCol : COLOR) : SV_Target
{
    float4 result;
    float2 pixel_size = float2(pixel_x, pixel_y);
    for (int x = -blur_scale + 1; x < blur_scale; x++) {
        result += inputTexture.Sample(textureSampler, float2(texCoord.x + x, texCoord.y) * pixel_size) * blur_weights[abs(x)];
    }
    result.a = 1.f;
    result *= inputCol;
    return result;
}
)";

const char blur_shader_y[] = R"(
Texture2D inputTexture;
SamplerState textureSampler : register(s0);

cbuffer PS_Blur_Constants : register(b0) {
    float pixel_x; 
    float pixel_y;
    float screen_width;
    float screen_height;
    float blur_scale;
    float blur_weights[400];
};

float4 main(float4 texCoord : SV_Position, float4 inputCol : COLOR) : SV_Target
{
    float4 result;
    float2 pixel_size = float2(pixel_x, pixel_y);
    for (int y = -blur_scale + 1; y < blur_scale; y++) {
        result += inputTexture.Sample(textureSampler, float2(texCoord.x, texCoord.y + y) * pixel_size) * blur_weights[abs(y)];
    }
    result.a = 1.f;
    result *= inputCol;
    return result;
}
)";

namespace engine {
	class c_base_shader {
	protected:
		struct BaseShaderBackup {
			ID3D11PixelShader* m_pShader = nullptr;
			ID3D11ClassInstance* m_pClassInstances = nullptr;
			UINT					m_nClassInstances = 0;

			ID3D11Buffer* m_pConstantBuffer = nullptr;
		};

		BaseShaderBackup	m_BaseBackupData;

		std::vector<ID3D11PixelShader*>	m_Passes;
		int					m_nCurrentPass = 0;
		ID3D11Buffer* m_pConstantBuffer = nullptr;
		int					m_nConstantBufferSize = 0;

		static void _begin(const ImDrawList* list, const ImDrawCmd* cmd);
		static void _end(const ImDrawList* list, const ImDrawCmd* cmd);
		static void _updateConstantBuffer(const ImDrawList* list, const ImDrawCmd* cmd);
		static void _setCurrentPass(const ImDrawList* list, const ImDrawCmd* cmd);

	public:
		c_base_shader(int constant_buffer_size = 0, void* buffer_init = nullptr);
		~c_base_shader();

		void addPass(const std::string& source, const std::string& main_function = "main");
		void setCurrentPass(int pass);
		void begin();
		void end();
		void updateConstantBuffer(void* buffer);
	};

	class c_blur_shader : public c_base_shader {
	private:
		struct BlurShaderBackup {
			ID3D11RenderTargetView* m_pRenderTarget = nullptr;
			ID3D11DepthStencilView* m_pDepthStencilView = nullptr;
		};

		math::c_vector_2d m_vecScreenSize;

		BlurShaderBackup    m_BlurShaderBackup;
		ID3D11Texture2D* m_pScreenTexture = nullptr; // for RTV
		ID3D11Texture2D* m_pTextureCopy = nullptr; // for SRV
		ID3D11ShaderResourceView* m_pShaderResourceView = nullptr;
		ID3D11RenderTargetView* m_pRenderTargetView = nullptr;

		static void _begin(const ImDrawList* list, const ImDrawCmd* cmd);
		static void _end(const ImDrawList* list, const ImDrawCmd* cmd);
		static void _pushScreenTexture(const ImDrawList* list, const ImDrawCmd* cmd);
		static void _popScreenTexture(const ImDrawList* list, const ImDrawCmd* cmd);
		static void _captureScreen(const ImDrawList* list, const ImDrawCmd* cmd);

	public:
		c_blur_shader(int constant_buffer_size = 0, void* buffer_init = nullptr) : c_base_shader(constant_buffer_size, buffer_init) {};
		~c_blur_shader();

		void begin();
		void end();
		void pushScreenTexture();
		void popScreenTexture();
		void captureScreen();
	};
}
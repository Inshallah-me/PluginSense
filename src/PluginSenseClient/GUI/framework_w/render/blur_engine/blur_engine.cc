#include "../../includes.hh"

#include <d3dcompiler.h>
#pragma comment(lib, "D3DCompiler.lib")

using namespace engine;

struct ConstantBufferUpdate_t {
	c_base_shader* shader;
	void* data;
};

c_base_shader::c_base_shader(int constant_buffer_size, void* buffer_init) {
	m_nConstantBufferSize = constant_buffer_size;

	if (m_nConstantBufferSize != 0) {
		D3D11_BUFFER_DESC cbDesc;
		ZeroMemory(&cbDesc, sizeof(D3D11_BUFFER_DESC));
		cbDesc.ByteWidth = static_cast<UINT>(std::ceil(constant_buffer_size / 16.f) * 16.f); // size of constant buffer should factor of 16
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

		D3D11_SUBRESOURCE_DATA InitData;
		InitData.pSysMem = buffer_init;
		InitData.SysMemPitch = 0;
		InitData.SysMemSlicePitch = 0;

		// Create the buffer.
		HRESULT hr = framework::g_overlay->m_device->CreateBuffer(&cbDesc, (buffer_init != nullptr ? &InitData : nullptr), &m_pConstantBuffer);
	}
}

c_base_shader::~c_base_shader() {
	for (int i = 0; i < m_nConstantBufferSize; i++)
		DX11_RELEASE(m_Passes[i]);
	DX11_RELEASE(m_pConstantBuffer);
}

void c_base_shader::addPass(const std::string& source, const std::string& main_function) {
	ID3DBlob* code = nullptr;
	ID3DBlob* error = nullptr;
	D3DCompile(source.c_str(), source.size(), nullptr, nullptr, nullptr, main_function.c_str(), "ps_5_0", 0, 0, &code, &error);

	if (error != nullptr) {
		char* data = new char[error->GetBufferSize()];
		memcpy(data, error->GetBufferPointer(), error->GetBufferSize());
		throw std::exception(data);
		return;
	}

	framework::g_overlay->m_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &m_Passes.emplace_back());
}

void c_base_shader::_setCurrentPass(const ImDrawList* list, const ImDrawCmd* cmd) {
	g_render->get_ctx()->PSSetShader(reinterpret_cast<ID3D11PixelShader*>(cmd->UserCallbackData), nullptr, 0);
}

void c_base_shader::_begin(const ImDrawList* list, const ImDrawCmd* cmd) {
	c_base_shader* shader = reinterpret_cast<c_base_shader*>(cmd->UserCallbackData);

	g_render->get_ctx()->PSGetShader(&shader->m_BaseBackupData.m_pShader, &shader->m_BaseBackupData.m_pClassInstances, &shader->m_BaseBackupData.m_nClassInstances);

	if (shader->m_pConstantBuffer) {
		g_render->get_ctx()->PSGetConstantBuffers(0, 1, &shader->m_BaseBackupData.m_pConstantBuffer);
		g_render->get_ctx()->PSSetConstantBuffers(0, 1, &shader->m_pConstantBuffer);
	}

	if (shader->m_Passes.size() == 1)
		g_render->get_ctx()->PSSetShader(shader->m_Passes[0], nullptr, 0);
}

void c_base_shader::_end(const ImDrawList* list, const ImDrawCmd* cmd) {
	c_base_shader* shader = reinterpret_cast<c_base_shader*>(cmd->UserCallbackData);

	g_render->get_ctx()->PSSetShader(shader->m_BaseBackupData.m_pShader, &shader->m_BaseBackupData.m_pClassInstances, std::min<UINT>(shader->m_BaseBackupData.m_nClassInstances, 1U));
	if (shader->m_BaseBackupData.m_pConstantBuffer)
		g_render->get_ctx()->PSSetConstantBuffers(0, 1, &shader->m_BaseBackupData.m_pConstantBuffer);

	DX11_RELEASE(shader->m_BaseBackupData.m_pConstantBuffer);
	DX11_RELEASE(shader->m_BaseBackupData.m_pShader);
	DX11_RELEASE(shader->m_BaseBackupData.m_pClassInstances);

	shader->m_BaseBackupData.m_nClassInstances = 0;
}

void c_base_shader::_updateConstantBuffer(const ImDrawList* list, const ImDrawCmd* cmd) {
	auto req = reinterpret_cast<ConstantBufferUpdate_t*>(cmd->UserCallbackData);

	D3D11_MAPPED_SUBRESOURCE map;
	g_render->get_ctx()->Map(req->shader->m_pConstantBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
	memcpy(map.pData, req->data, req->shader->m_nConstantBufferSize);
	g_render->get_ctx()->Unmap(req->shader->m_pConstantBuffer, 0);

	_aligned_free(req->data);
	delete req;
}

void c_base_shader::updateConstantBuffer(void* data) {
	if (!m_pConstantBuffer)
		return;

	void* copy_data = _aligned_malloc(m_nConstantBufferSize, 16);
	if (!copy_data)
		return;

	memcpy(copy_data, data, m_nConstantBufferSize);
	auto req = new ConstantBufferUpdate_t;
	req->data = copy_data;
	req->shader = this;
	g_render->draw_list()->AddCallback(_updateConstantBuffer, req);
}

void c_base_shader::setCurrentPass(int pass) {
	g_render->draw_list()->AddCallback(_setCurrentPass, m_Passes[pass]);
}

void c_base_shader::begin() {
	g_render->draw_list()->AddCallback(_begin, this);
}

void c_base_shader::end() {
	g_render->draw_list()->AddCallback(_end, this);
}

c_blur_shader::~c_blur_shader() {
	c_base_shader::~c_base_shader();
	DX11_RELEASE(m_pScreenTexture);
	DX11_RELEASE(m_pTextureCopy);
	DX11_RELEASE(m_pRenderTargetView);
}

void c_blur_shader::_begin(const ImDrawList* list, const ImDrawCmd* cmd) {
	c_blur_shader* shader = reinterpret_cast<c_blur_shader*>(cmd->UserCallbackData);

	g_render->get_ctx()->PSGetShader(&shader->m_BaseBackupData.m_pShader, &shader->m_BaseBackupData.m_pClassInstances, &shader->m_BaseBackupData.m_nClassInstances);
	g_render->get_ctx()->PSGetConstantBuffers(0, 1, &shader->m_BaseBackupData.m_pConstantBuffer);
	g_render->get_ctx()->OMGetRenderTargets(1, &shader->m_BlurShaderBackup.m_pRenderTarget, &shader->m_BlurShaderBackup.m_pDepthStencilView);

	if (shader->m_pConstantBuffer)
		g_render->get_ctx()->PSSetConstantBuffers(0, 1, &shader->m_pConstantBuffer);

	ID3D11Texture2D* screen_buffer = nullptr;
	framework::g_overlay->m_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&screen_buffer);

	if (!shader->m_pScreenTexture || !shader->m_pTextureCopy || !shader->m_pRenderTargetView ||
		(shader->m_vecScreenSize.x != ImGui::GetIO().DisplaySize.x || shader->m_vecScreenSize.y != ImGui::GetIO().DisplaySize.y)) {
		DX11_RELEASE(shader->m_pRenderTargetView);
		DX11_RELEASE(shader->m_pShaderResourceView);
		DX11_RELEASE(shader->m_pScreenTexture);
		DX11_RELEASE(shader->m_pTextureCopy);

		shader->m_vecScreenSize = math::c_vector_2d(ImGui::GetIO().DisplaySize.x, ImGui::GetIO().DisplaySize.y);

		D3D11_TEXTURE2D_DESC desc;
		screen_buffer->GetDesc(&desc);
		framework::g_overlay->m_device->CreateTexture2D(&desc, nullptr, &shader->m_pScreenTexture);
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		framework::g_overlay->m_device->CreateTexture2D(&desc, nullptr, &shader->m_pTextureCopy);
		framework::g_overlay->m_device->CreateShaderResourceView(shader->m_pTextureCopy, nullptr, &shader->m_pShaderResourceView);
		framework::g_overlay->m_device->CreateRenderTargetView(shader->m_pScreenTexture, nullptr, &shader->m_pRenderTargetView);
	}

	g_render->get_ctx()->OMSetRenderTargets(1, &shader->m_pRenderTargetView, nullptr);

	g_render->get_ctx()->CopyResource(shader->m_pTextureCopy, screen_buffer);
	g_render->get_ctx()->CopyResource(shader->m_pScreenTexture, screen_buffer);

	g_render->override_texture(shader->m_pShaderResourceView);

	screen_buffer->Release();
}

void c_blur_shader::_end(const ImDrawList* list, const ImDrawCmd* cmd) {
	c_base_shader::_end(list, cmd);

	auto shader = reinterpret_cast<c_blur_shader*>(cmd->UserCallbackData);

	g_render->get_ctx()->OMSetRenderTargets(1, &shader->m_BlurShaderBackup.m_pRenderTarget, shader->m_BlurShaderBackup.m_pDepthStencilView);

	DX11_RELEASE(shader->m_BlurShaderBackup.m_pRenderTarget);
	DX11_RELEASE(shader->m_BlurShaderBackup.m_pDepthStencilView);
}

void ImDX11BindTexture(ID3D11DeviceContext* ctx, UINT startSlot, UINT numViews, ID3D11ShaderResourceView** views) {
	if (!g_render->override_resource)
		return ctx->PSSetShaderResources(startSlot, numViews, views);

	ctx->PSSetShaderResources(0, 1, &g_render->override_resource);
}

void c_blur_shader::_pushScreenTexture(const ImDrawList* list, const ImDrawCmd* cmd) {
	auto shader = reinterpret_cast<c_blur_shader*>(cmd->UserCallbackData);

	g_render->override_texture(shader->m_pShaderResourceView);
}

void c_blur_shader::_popScreenTexture(const ImDrawList* list, const ImDrawCmd* cmd) {
	g_render->override_texture(nullptr);
}

void c_blur_shader::_captureScreen(const ImDrawList* list, const ImDrawCmd* cmd) {
	auto shader = reinterpret_cast<c_blur_shader*>(cmd->UserCallbackData);

	g_render->get_ctx()->CopyResource(shader->m_pTextureCopy, shader->m_pScreenTexture);
}

void c_blur_shader::begin() {
	g_render->draw_list()->AddCallback(_begin, this);
}

void c_blur_shader::end() {
	g_render->draw_list()->AddCallback(_end, this);
}

void c_blur_shader::pushScreenTexture() {
	g_render->draw_list()->AddCallback(_pushScreenTexture, this);
}

void c_blur_shader::popScreenTexture() {
	g_render->draw_list()->AddCallback(_popScreenTexture, this);
}

void c_blur_shader::captureScreen() {
	g_render->draw_list()->AddCallback(_captureScreen, this);
}

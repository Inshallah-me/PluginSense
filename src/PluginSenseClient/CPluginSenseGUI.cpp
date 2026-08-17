#include "CPluginSenseGUI.hpp"

#include <ShlObj_core.h>
#include <windowsx.h>

#include <ImGui/imgui_impl_win32.h>
#include <ImGui/imgui_impl_dx11.h>

#include <DllLauncher.hpp>
#include <Common/Helpers/StringHelper.hpp>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Interface/IEngineToClient.hpp>

#include <CS2/Hook/Hook_IsRelativeMouseMode.hpp>
#include <CS2/Hook/Hook_EnableCursor.hpp>

#include <PluginSenseClient/CPluginSenseClient.hpp>
#include <PluginSenseClient/GUI/CPluginSenseMenu.hpp>
#include <framework_w/includes.hh>
#include <PluginSenseClient/Settings/Settings.hpp>
#include <PluginSenseClient/Settings/MenuState.hpp>

static CPluginSenseGUI g_PluginSenseGUI{};

IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hwnd , UINT msg , WPARAM wParam , LPARAM lParam );

namespace
{
	auto IsMouseMenuKey( const int key ) -> bool
	{
		return key == VK_LBUTTON || key == VK_RBUTTON || key == VK_MBUTTON || key == VK_XBUTTON1 || key == VK_XBUTTON2;
	}

	auto IsMenuKeyMessage( UINT uMsg , WPARAM wParam ) -> bool
	{
		const int key = vars::menuKey;
		if ( key <= 0 || key > 255 )
			return false;

		if ( uMsg == WM_KEYUP )
			return !IsMouseMenuKey( key ) && static_cast<int>( wParam ) == key;

		return false;
	}

	auto SetSystemCursorVisible( const bool visible ) -> void
	{
		if ( visible )
		{
			while ( ShowCursor( TRUE ) < 0 ) {}
			SetCursor( LoadCursor( nullptr , IDC_ARROW ) );
		}
		else
		{
			while ( ShowCursor( FALSE ) >= 0 ) {}
		}
	}

	auto ApplyGameCursorState( const bool menuOpen , const bool gameRelativeMode ) -> void
	{
		auto& io = ImGui::GetIO();
		io.MouseDrawCursor = false;
		io.WantCaptureMouse = menuOpen;
		io.WantCaptureKeyboard = menuOpen && ImGui::IsAnyItemActive();

		if ( !SDK::Interfaces::InputSystem() )
			return;

		static bool savedCursorInput = true;

		if ( menuOpen )
		{
			savedCursorInput = EnableCursor_Input;
			SetSystemCursorVisible( true );
			ClipCursor( nullptr );
			__try
			{
				if ( EnableCursor_o )
					EnableCursor_o( SDK::Interfaces::InputSystem() , false );
				if ( IsRelativeMouseMode_o )
					IsRelativeMouseMode_o( SDK::Interfaces::InputSystem() , false );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {}
		}
		else
		{
			if ( savedCursorInput )
				SetSystemCursorVisible( true );
			else
				SetSystemCursorVisible( false );

			ClipCursor( nullptr );
			__try
			{
				if ( EnableCursor_o )
					EnableCursor_o( SDK::Interfaces::InputSystem() , savedCursorInput );
				if ( IsRelativeMouseMode_o )
					IsRelativeMouseMode_o( SDK::Interfaces::InputSystem() , gameRelativeMode );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) {}
		}
	}

	auto IsMouseCoordinateMessage( const UINT msg ) -> bool
	{
		switch ( msg )
		{
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
		case WM_XBUTTONDOWN:
		case WM_XBUTTONUP:
		case WM_XBUTTONDBLCLK:
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
			return true;
		default:
			return false;
		}
	}

	auto IsKeyboardMessage( const UINT msg ) -> bool
	{
		return ( msg >= WM_KEYFIRST && msg <= WM_KEYLAST ) ||
			msg == WM_CHAR || msg == WM_SYSCHAR || msg == WM_UNICHAR || msg == WM_INPUT;
	}

	auto MakeKeyUpLParam( const WPARAM key ) -> LPARAM
	{
		const UINT scanCode = MapVirtualKeyA( static_cast<UINT>( key ) , MAPVK_VK_TO_VSC );
		return static_cast<LPARAM>( 1u | ( scanCode << 16 ) | ( 1u << 30 ) | ( 1u << 31 ) );
	}

	auto ReleaseHeldGameplayKeys( HWND hwnd , WNDPROC originalWndProc ) -> void
	{
		if ( !hwnd || !originalWndProc )
			return;

		constexpr WPARAM keyboardKeys[] = {
			'W' , 'A' , 'S' , 'D' , 'E' , 'R' ,
			VK_SPACE ,
			VK_CONTROL , VK_LCONTROL , VK_RCONTROL ,
			VK_SHIFT , VK_LSHIFT , VK_RSHIFT ,
			VK_MENU , VK_LMENU , VK_RMENU
		};

		for ( const WPARAM key : keyboardKeys )
		{
			if ( GetAsyncKeyState( static_cast<int>( key ) ) & 0x8000 )
				CallWindowProcA( originalWndProc , hwnd , WM_KEYUP , key , MakeKeyUpLParam( key ) );
		}

		POINT pos{};
		if ( GetCursorPos( &pos ) )
			ScreenToClient( hwnd , &pos );
		const LPARAM mouseLParam = MAKELPARAM( pos.x , pos.y );

		if ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 )
			CallWindowProcA( originalWndProc , hwnd , WM_LBUTTONUP , 0 , mouseLParam );
		if ( GetAsyncKeyState( VK_RBUTTON ) & 0x8000 )
			CallWindowProcA( originalWndProc , hwnd , WM_RBUTTONUP , 0 , mouseLParam );
		if ( GetAsyncKeyState( VK_MBUTTON ) & 0x8000 )
			CallWindowProcA( originalWndProc , hwnd , WM_MBUTTONUP , 0 , mouseLParam );
		if ( GetAsyncKeyState( VK_XBUTTON1 ) & 0x8000 )
			CallWindowProcA( originalWndProc , hwnd , WM_XBUTTONUP , MAKEWPARAM( 0 , XBUTTON1 ) , mouseLParam );
		if ( GetAsyncKeyState( VK_XBUTTON2 ) & 0x8000 )
			CallWindowProcA( originalWndProc , hwnd , WM_XBUTTONUP , MAKEWPARAM( 0 , XBUTTON2 ) , mouseLParam );
	}

	auto GetMouseScale( HWND hwnd , const ImVec2& backBufferSize ) -> ImVec2
	{
		RECT rect{};
		if ( !GetClientRect( hwnd , &rect ) )
			return ImVec2( 1.f , 1.f );

		const float clientWidth = static_cast<float>( rect.right - rect.left );
		const float clientHeight = static_cast<float>( rect.bottom - rect.top );
		if ( clientWidth <= 0.f || clientHeight <= 0.f || backBufferSize.x <= 0.f || backBufferSize.y <= 0.f )
			return ImVec2( 1.f , 1.f );

		return ImVec2( backBufferSize.x / clientWidth , backBufferSize.y / clientHeight );
	}

	auto ScaleMouseLParam( HWND hwnd , LPARAM lParam , const ImVec2& backBufferSize ) -> LPARAM
	{
		const auto scale = GetMouseScale( hwnd , backBufferSize );
		if ( scale.x == 1.f && scale.y == 1.f )
			return lParam;

		const int x = static_cast<int>( static_cast<float>( GET_X_LPARAM( lParam ) ) * scale.x );
		const int y = static_cast<int>( static_cast<float>( GET_Y_LPARAM( lParam ) ) * scale.y );
		return MAKELPARAM( x , y );
	}

	auto SubmitScaledMousePosition( HWND hwnd , const ImVec2& backBufferSize ) -> void
	{
		POINT pos{};
		if ( !GetCursorPos( &pos ) )
			return;

		if ( !ScreenToClient( hwnd , &pos ) )
			return;

		const auto scale = GetMouseScale( hwnd , backBufferSize );
		ImGui::GetIO().AddMousePosEvent( static_cast<float>( pos.x ) * scale.x , static_cast<float>( pos.y ) * scale.y );
	}
}

auto CPluginSenseGUI::OnInit( IDXGISwapChain* pSwapChain ) -> void
{
	DXGI_SWAP_CHAIN_DESC SwapChainDesc;

	if ( FAILED( pSwapChain->GetDevice( IID_PPV_ARGS( &m_pDevice ) ) ) )
	{
		DEV_LOG( "[error] CPluginSenseGUI::OnInit: #1\n" );
		return;
	}

	m_pDevice->GetImmediateContext( &m_pDeviceContext );

	if ( FAILED( pSwapChain->GetDesc( &SwapChainDesc ) ) )
	{
		DEV_LOG( "[error] CPluginSenseGUI::OnInit: #2\n" );
		return;
	}

	m_hCS2Window = SwapChainDesc.OutputWindow;
	m_vecBackBufferSize = ImVec2( static_cast<float>( SwapChainDesc.BufferDesc.Width ) , static_cast<float>( SwapChainDesc.BufferDesc.Height ) );

	m_pImGuiContext = ImGui::CreateContext();

	if ( !m_pFreeType_Font )
		m_pFreeType_Font = new FreeTypeBuild();

	ImGui::SetCurrentContext( m_pImGuiContext );

	ImGui::GetIO().IniFilename = nullptr;
	ImGui::GetIO().LogFilename = "";

	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	ImGui::GetIO().MouseDrawCursor = false;

	ImGui_ImplWin32_Init( m_hCS2Window );
	ImGui_ImplDX11_Init( m_pDevice , m_pDeviceContext );

	InitFont();
	GetPluginSenseMenu()->OnGuiInitialized( pSwapChain , m_pDevice , m_pDeviceContext );
	UpdateStyle();

	m_WndProc_o = (WNDPROC)SetWindowLongPtrA( m_hCS2Window , GWLP_WNDPROC , (LONG_PTR)GUI_WndProc );

	m_bInit = true;

	OnReopenGUI();
}

auto CPluginSenseGUI::OnDestroy() -> void
{
	SetWindowLongPtrA( m_hCS2Window , GWLP_WNDPROC , (LONG_PTR)GetPluginSenseGUI()->m_WndProc_o );

	ApplyGameCursorState( false , m_bMainActive );
	m_bVisible = false;
	ImGui::GetIO().MouseDrawCursor = false;

	if ( m_pFreeType_Font )
	{
		delete m_pFreeType_Font;
		m_pFreeType_Font = nullptr;
	}

	ClearRenderTargetView();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	m_bInit = false;
}

auto CPluginSenseGUI::InitFont() -> void
{
	ImGuiIO& io = ImGui::GetIO();

	static const ImWchar TahomaRanges[] =
	{
		0x0020, 0xFFFC,
		0,
	};

	wchar_t* szWindowsFontPath = nullptr;

	if ( SHGetKnownFolderPath( FOLDERID_Fonts , 0 , 0 , &szWindowsFontPath ) == S_OK )
	{
		std::wstring TahomaFont = std::wstring( szWindowsFontPath ) + L"\\tahoma.ttf";
		io.Fonts->AddFontFromFileTTF( unicode_to_utf8( TahomaFont ).c_str() , 15.f , nullptr , TahomaRanges );
	}

	CoTaskMemFree( szWindowsFontPath );
}

void CPluginSenseGUI::OnPresent( IDXGISwapChain* pSwapChain )
{
	if ( !m_bInit )
		OnInit( pSwapChain );
	else
		OnRender( pSwapChain );
}

void CPluginSenseGUI::OnResizeBuffers( IDXGISwapChain* pSwapChain )
{
	if ( !m_bInit )
		return;

	m_bInit = false;

	// 切换分辨率时 ImGui context 会重建,清掉 framework 残留的焦点栈 / hovered /
	// 点击消费状态,否则切换后 can_interact 判定失败,控件点击全部失效。
	framework::g_ctx->m_focus_stack.clear();
	framework::g_ctx->m_hovered = nullptr;
	framework::g_ctx->m_click_consumed = false;
	framework::g_ctx->m_dragging = false;
	framework::g_ctx->m_modal_owner = nullptr;
	framework::g_ctx->m_focus_took = nullptr;
	framework::g_ctx->m_focus_took_by_popup = nullptr;

	ClearRenderTargetView();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();

	if ( m_pFreeType_Font )
	{
		delete m_pFreeType_Font;
		m_pFreeType_Font = nullptr;
	}

	g_render.reset();
	g_render = std::make_unique<c_render>();
}

void CPluginSenseGUI::OnResizeBuffersPost( IDXGISwapChain* pSwapChain )
{
	DXGI_SWAP_CHAIN_DESC SwapChainDesc;
	if ( FAILED( pSwapChain->GetDesc( &SwapChainDesc ) ) )
		return;

	m_vecBackBufferSize = ImVec2(
		static_cast<float>( SwapChainDesc.BufferDesc.Width ),
		static_cast<float>( SwapChainDesc.BufferDesc.Height ) );

	ID3D11Texture2D* pBackBuffer = nullptr;
	pSwapChain->GetBuffer( 0 , IID_PPV_ARGS( &pBackBuffer ) );

	if ( pBackBuffer )
	{
		m_pDevice->CreateRenderTargetView( pBackBuffer , nullptr , &m_pRenderTargetView );
		pBackBuffer->Release();
	}

	m_pImGuiContext = ImGui::CreateContext();

	if ( !m_pFreeType_Font )
		m_pFreeType_Font = new FreeTypeBuild();

	ImGui::SetCurrentContext( m_pImGuiContext );

	ImGui::GetIO().IniFilename = nullptr;
	ImGui::GetIO().LogFilename = "";
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
	ImGui::GetIO().MouseDrawCursor = false;

	ImGui_ImplWin32_Init( m_hCS2Window );
	ImGui_ImplDX11_Init( m_pDevice , m_pDeviceContext );

	InitFont();
	GetPluginSenseMenu()->OnGuiInitialized( pSwapChain , m_pDevice , m_pDeviceContext );
	UpdateStyle();

	m_bInit = true;
}
 
void CPluginSenseGUI::OnRender( IDXGISwapChain* pSwapChain )
{
	DXGI_SWAP_CHAIN_DESC swapChainDesc{};
	if ( SUCCEEDED( pSwapChain->GetDesc( &swapChainDesc ) ) )
		m_vecBackBufferSize = ImVec2( static_cast<float>( swapChainDesc.BufferDesc.Width ) , static_cast<float>( swapChainDesc.BufferDesc.Height ) );

	if ( m_pFreeType_Font && m_pFreeType_Font->PreNewFrame() )
	{
		ImGui_ImplDX11_InvalidateDeviceObjects();
		ImGui_ImplDX11_CreateDeviceObjects();
	}
	else
	{
		if ( !m_pRenderTargetView )
		{
			ID3D11Texture2D* pBackBuffer = nullptr;

			pSwapChain->GetBuffer( 0 , IID_PPV_ARGS( &pBackBuffer ) );

			D3D11_RENDER_TARGET_VIEW_DESC RenderTargetDesc;

			memset( &RenderTargetDesc , 0 , sizeof( RenderTargetDesc ) );

			RenderTargetDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			RenderTargetDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;

			if ( pBackBuffer )
			{
				m_pDevice->CreateRenderTargetView( pBackBuffer , &RenderTargetDesc , &m_pRenderTargetView );
				pBackBuffer->Release();
			}
		}

		ImGui::SetCurrentContext( m_pImGuiContext );

		if ( m_pMainRenderTarget ) { m_pMainRenderTarget->Release(); m_pMainRenderTarget = nullptr; }
			m_pDeviceContext->OMGetRenderTargets( 1 , &m_pMainRenderTarget , 0 );
			m_pDeviceContext->OMSetRenderTargets( 1 , &m_pRenderTargetView , 0 );

		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();

		if ( m_vecBackBufferSize.x > 0.f && m_vecBackBufferSize.y > 0.f )
			ImGui::GetIO().DisplaySize = m_vecBackBufferSize;

		SubmitScaledMousePosition( m_hCS2Window , m_vecBackBufferSize );

		ImGui::NewFrame();

		auto& io = ImGui::GetIO();
		io.MouseDrawCursor = false;
		io.WantCaptureMouse = m_bVisible;
		io.WantCaptureKeyboard = m_bVisible && ImGui::IsAnyItemActive();

		GetPluginSenseClient()->OnRender();

		ImGui::EndFrame();
		ImGui::Render();

		ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData() );

		m_pDeviceContext->OMSetRenderTargets( 1 , &m_pMainRenderTarget , 0 );
	}
}

auto CPluginSenseGUI::OnReopenGUI() -> void
{
	const bool opening = !m_bVisible;
	if ( opening )
		ReleaseHeldGameplayKeys( m_hCS2Window , m_WndProc_o );

	m_bVisible = !m_bVisible;
	ApplyGameCursorState( m_bVisible , m_bMainActive );
}

LRESULT WINAPI CPluginSenseGUI::GUI_WndProc( HWND hwnd , UINT uMsg , WPARAM wParam , LPARAM lParam )
{
	if ( uMsg == WM_QUIT || uMsg == WM_CLOSE || uMsg == WM_DESTROY )
	{
		GetDllLauncher()->OnDestroy();
		return true;
	}

	if ( GetPluginSenseGUI()->m_bInit )
	{
		if ( IsMenuKeyMessage( uMsg , wParam ) )
		{
			if ( vars::menuKeySuppress == static_cast<int>( wParam ) )
			{
				vars::menuKeySuppress = 0;
				return true;
			}

			GetPluginSenseGUI()->OnReopenGUI();
		}

		LPARAM imguiLParam = lParam;
		if ( IsMouseCoordinateMessage( uMsg ) )
			imguiLParam = ScaleMouseLParam( hwnd , lParam , GetPluginSenseGUI()->GetBackBufferSize() );

		if ( GetPluginSenseGUI()->IsVisible() )
		{
			ImGui_ImplWin32_WndProcHandler( hwnd , uMsg , wParam , imguiLParam );

			if ( uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST )
				return true;

			if ( IsKeyboardMessage( uMsg ) )
				return true;
		}
	}

	return CallWindowProcA( GetPluginSenseGUI()->m_WndProc_o , hwnd , uMsg , wParam , lParam );
}

auto CPluginSenseGUI::SetIndigoStyle() -> void
{
	auto& style = ImGui::GetStyle();
	auto& colors = style.Colors;

	float roundness = 2.0f;

	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.WindowMinSize = ImVec2( 75.f , 50.f );
	style.FramePadding = ImVec2( 5 , 5 );
	style.ItemSpacing = ImVec2( 6 , 5 );
	style.ItemInnerSpacing = ImVec2( 2 , 4 );
	style.Alpha = 1.0f;
	style.WindowRounding = 0.f;
	style.FrameRounding = roundness;
	style.PopupRounding = 0;
	style.PopupBorderSize = 1.f;
	style.IndentSpacing = 6.0f;
	style.ColumnsMinSpacing = 50.0f;
	style.GrabMinSize = 14.0f;
	style.GrabRounding = roundness;
	style.ScrollbarSize = 12.0f;
	style.ScrollbarRounding = roundness;

	style.AntiAliasedFill = true;
	style.AntiAliasedLines = true;
	style.AntiAliasedLinesUseTex = true;

	colors[ImGuiCol_Text] = ImVec4( 1.00f , 1.00f , 1.00f , 1.00f );
	colors[ImGuiCol_TextDisabled] = ImVec4( 0.50f , 0.50f , 0.50f , 1.00f );

	colors[ImGuiCol_WindowBg] = ImVec4( 0.20f , 0.23f , 0.31f , 1.00f );
	colors[ImGuiCol_ChildBg] = ImVec4( 0.20f , 0.23f , 0.31f , 1.00f );
	colors[ImGuiCol_PopupBg] = ImVec4( 0.20f , 0.23f , 0.31f , 1.00f );

	colors[ImGuiCol_Border] = ImVec4( 0.f , 0.f , 0.f , 1.00f );
	colors[ImGuiCol_BorderShadow] = ImVec4( 0.00f , 0.00f , 0.00f , 0.00f );

	colors[ImGuiCol_FrameBg] = ImVec4( 0.25f , 0.28f , 0.38f , 1.00f );
	colors[ImGuiCol_FrameBgHovered] = ImVec4( 0.25f , 0.28f , 0.38f , 1.00f );
	colors[ImGuiCol_FrameBgActive] = ImVec4( 0.25f , 0.28f , 0.38f , 1.00f );

	colors[ImGuiCol_TitleBg] = ImVec4( 0.00f , 0.43f , 1.00f , 1.00f );
	colors[ImGuiCol_TitleBgActive] = ImVec4( 0.00f , 0.55f , 1.00f , 1.00f );
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4( 0.10f , 0.69f , 1.00f , 1.00f );

	colors[ImGuiCol_MenuBarBg] = ImVec4( 0.25f , 0.28f , 0.38f , 1.00f );

	colors[ImGuiCol_ScrollbarBg] = ImVec4( 0.00f , 0.00f , 0.00f , 0.00f );
	colors[ImGuiCol_ScrollbarGrab] = ImVec4( 0.39f , 0.44f , 0.56f , 1.00f );
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4( 0.12f , 0.43f , 1.00f , 1.00f );
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4( 0.00f , 0.55f , 1.00f , 1.00f );

	colors[ImGuiCol_CheckMark] = ImVec4( 0.00f , 0.55f , 1.00f , 1.00f );

	colors[ImGuiCol_SliderGrab] = ImVec4( 0.00f , 0.55f , 1.00f , 1.00f );
	colors[ImGuiCol_SliderGrabActive] = ImVec4( 0.10f , 0.69f , 1.00f , 1.00f );

	colors[ImGuiCol_Button] = ImVec4( 0.25f , 0.28f , 0.38f , 1.00f );
	colors[ImGuiCol_ButtonHovered] = ImVec4( 0.12f , 0.43f , 1.00f , 1.00f );
	colors[ImGuiCol_ButtonActive] = ImVec4( 0.00f , 0.55f , 1.00f , 1.00f );

	colors[ImGuiCol_Header] = ImVec4( 0.00f , 0.43f , 1.00f , 1.00f );
	colors[ImGuiCol_HeaderHovered] = ImVec4( 0.00f , 0.55f , 1.00f , 1.00f );
	colors[ImGuiCol_HeaderActive] = ImVec4( 0.00f , 0.43f , 1.00f , 1.00f );

	colors[ImGuiCol_Separator] = ImVec4( 0.43f , 0.43f , 0.50f , 0.50f );
	colors[ImGuiCol_SeparatorHovered] = ImVec4( 0.10f , 0.40f , 0.75f , 0.78f );
	colors[ImGuiCol_SeparatorActive] = ImVec4( 0.10f , 0.40f , 0.75f , 1.00f );

	colors[ImGuiCol_ResizeGrip] = ImVec4( 0.26f , 0.59f , 0.98f , 0.25f );
	colors[ImGuiCol_ResizeGripHovered] = ImVec4( 0.26f , 0.59f , 0.98f , 0.67f );
	colors[ImGuiCol_ResizeGripActive] = ImVec4( 0.26f , 0.59f , 0.98f , 0.95f );

	colors[ImGuiCol_Tab] = ImVec4( 0.00f , 0.50f , 1.00f , 1.00f );
	colors[ImGuiCol_TabHovered] = ImVec4( 0.12f , 0.69f , 1.00f , 1.00f );

	colors[ImGuiCol_TabActive] = ImVec4( 0.12f , 0.69f , 1.00f , 1.00f );
	colors[ImGuiCol_TabUnfocused] = ImVec4( 0.07f , 0.10f , 0.15f , 0.97f );
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4( 0.14f , 0.26f , 0.42f , 1.00f );

	colors[ImGuiCol_PlotLines] = ImVec4( 0.61f , 0.61f , 0.61f , 1.00f );
	colors[ImGuiCol_PlotLinesHovered] = ImVec4( 1.00f , 0.43f , 0.35f , 1.00f );
	colors[ImGuiCol_PlotHistogram] = ImVec4( 0.10f , 0.69f , 1.00f , 1.00f );
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4( 1.00f , 0.60f , 0.00f , 1.00f );

	colors[ImGuiCol_TextSelectedBg] = ImVec4( 0.26f , 0.59f , 0.98f , 0.35f );
	colors[ImGuiCol_DragDropTarget] = ImVec4( 1.00f , 1.00f , 0.00f , 0.90f );

	colors[ImGuiCol_NavHighlight] = ImVec4( 0.26f , 0.59f , 0.98f , 1.00f );
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4( 1.00f , 1.00f , 1.00f , 0.70f );
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4( 0.80f , 0.80f , 0.80f , 0.20f );

	colors[ImGuiCol_ModalWindowDimBg] = ImVec4( 0.80f , 0.80f , 0.80f , 0.35f );
}

auto CPluginSenseGUI::SetVermillionStyle() -> void
{
	auto& style = ImGui::GetStyle();
	auto& colors = style.Colors;

	float roundness = 2.0f;

	style.WindowBorderSize = 1.0f;
	style.FrameBorderSize = 1.0f;
	style.WindowMinSize = ImVec2( 75.f , 50.f );
	style.FramePadding = ImVec2( 5 , 5 );
	style.ItemSpacing = ImVec2( 6 , 5 );
	style.ItemInnerSpacing = ImVec2( 2 , 4 );
	style.Alpha = 1.0f;
	style.WindowRounding = 0.f;
	style.FrameRounding = roundness;
	style.PopupRounding = 0;
	style.PopupBorderSize = 1.f;
	style.IndentSpacing = 6.0f;
	style.ColumnsMinSpacing = 50.0f;
	style.GrabMinSize = 14.0f;
	style.GrabRounding = roundness;
	style.ScrollbarSize = 12.0f;
	style.ScrollbarRounding = roundness;

	style.AntiAliasedFill = true;
	style.AntiAliasedLines = true;
	style.AntiAliasedLinesUseTex = true;

	colors[ImGuiCol_Text] = ImVec4( 1.00f , 1.00f , 1.00f , 0.75f );
	colors[ImGuiCol_TextDisabled] = ImVec4( 1.00f , 0.18f , 0.29f , 0.78f );

	colors[ImGuiCol_WindowBg] = ImVec4( 0.17f , 0.20f , 0.25f , 1.00f );
	colors[ImGuiCol_ChildBg] = ImVec4( 0.20f , 0.22f , 0.27f , 0.57f );
	colors[ImGuiCol_PopupBg] = ImVec4( 0.17f , 0.20f , 0.25f , 1.00f );

	colors[ImGuiCol_Border] = ImVec4( 0.00f , 0.00f , 0.00f , 1.00f );
	colors[ImGuiCol_BorderShadow] = ImVec4( 0.00f , 0.00f , 0.00f , 0.00f );

	colors[ImGuiCol_FrameBg] = ImVec4( 0.37f , 0.36f , 0.46f , 0.24f );
	colors[ImGuiCol_FrameBgHovered] = ImVec4( 0.78f , 0.18f , 0.29f , 0.78f );
	colors[ImGuiCol_FrameBgActive] = ImVec4( 0.78f , 0.18f , 0.29f , 1.00f );

	colors[ImGuiCol_TitleBg] = ImVec4( 0.20f , 0.22f , 0.27f , 1.00f );
	colors[ImGuiCol_TitleBgActive] = ImVec4( 0.78f , 0.18f , 0.29f , 1.00f );
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4( 0.20f , 0.22f , 0.27f , 0.75f );

	colors[ImGuiCol_MenuBarBg] = ImVec4( 0.20f , 0.22f , 0.27f , 0.47f );

	colors[ImGuiCol_ScrollbarBg] = ImVec4( 0.20f , 0.22f , 0.27f , 1.00f );
	colors[ImGuiCol_ScrollbarGrab] = ImVec4( 0.09f , 0.15f , 0.16f , 1.00f );
	colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4( 0.78f , 0.18f , 0.29f , 0.78f );
	colors[ImGuiCol_ScrollbarGrabActive] = ImVec4( 0.78f , 0.18f , 0.29f , 1.00f );

	colors[ImGuiCol_CheckMark] = ImVec4( 0.71f , 0.18f , 0.29f , 1.00f );

	colors[ImGuiCol_SliderGrab] = ImVec4( 0.78f , 0.18f , 0.29f , 0.37f );
	colors[ImGuiCol_SliderGrabActive] = ImVec4( 0.92f , 0.18f , 0.29f , 1.00f );

	colors[ImGuiCol_Button] = ImVec4( 0.65f , 0.18f , 0.29f , 1.00f );
	colors[ImGuiCol_ButtonHovered] = ImVec4( 0.78f , 0.18f , 0.29f , 0.86f );
	colors[ImGuiCol_ButtonActive] = ImVec4( 0.78f , 0.18f , 0.29f , 1.00f );

	colors[ImGuiCol_Header] = ImVec4( 0.78f , 0.18f , 0.29f , 0.76f );
	colors[ImGuiCol_HeaderHovered] = ImVec4( 0.78f , 0.18f , 0.29f , 0.86f );
	colors[ImGuiCol_HeaderActive] = ImVec4( 0.78f , 0.18f , 0.29f , 1.00f );

	colors[ImGuiCol_Separator] = ImVec4( 0.15f , 0.00f , 0.00f , 0.35f );
	colors[ImGuiCol_SeparatorHovered] = ImVec4( 0.78f , 0.18f , 0.29f , 0.59f );
	colors[ImGuiCol_SeparatorActive] = ImVec4( 0.78f , 0.18f , 0.29f , 1.00f );

	colors[ImGuiCol_ResizeGrip] = ImVec4( 0.78f , 0.18f , 0.29f , 0.63f );
	colors[ImGuiCol_ResizeGripHovered] = ImVec4( 0.78f , 0.18f , 0.29f , 0.78f );
	colors[ImGuiCol_ResizeGripActive] = ImVec4( 0.78f , 0.18f , 0.29f , 1.00f );

	colors[ImGuiCol_Tab] = ImVec4( 0.78f , 0.18f , 0.29f , 0.76f );
	colors[ImGuiCol_TabHovered] = ImVec4( 0.78f , 0.18f , 0.29f , 0.86f );
	colors[ImGuiCol_TabActive] = ImVec4( 0.78f , 0.18f , 0.29f , 1.00f );
	colors[ImGuiCol_TabUnfocused] = ImVec4( 0.07f , 0.10f , 0.15f , 0.97f );
	colors[ImGuiCol_TabUnfocusedActive] = ImVec4( 0.14f , 0.26f , 0.42f , 1.00f );

	colors[ImGuiCol_PlotLines] = ImVec4( 0.78f , 0.93f , 0.89f , 0.63f );
	colors[ImGuiCol_PlotLinesHovered] = ImVec4( 0.92f , 0.18f , 0.29f , 1.00f );
	colors[ImGuiCol_PlotHistogram] = ImVec4( 0.86f , 0.93f , 0.89f , 0.63f );
	colors[ImGuiCol_PlotHistogramHovered] = ImVec4( 0.92f , 0.18f , 0.29f , 1.00f );

	colors[ImGuiCol_TextSelectedBg] = ImVec4( 0.92f , 0.18f , 0.29f , 0.43f );
	colors[ImGuiCol_DragDropTarget] = ImVec4( 1.00f , 1.00f , 0.00f , 0.90f );

	colors[ImGuiCol_NavHighlight] = ImVec4( 0.45f , 0.45f , 0.90f , 0.80f );
	colors[ImGuiCol_NavWindowingHighlight] = ImVec4( 1.00f , 1.00f , 1.00f , 0.70f );
	colors[ImGuiCol_NavWindowingDimBg] = ImVec4( 0.80f , 0.80f , 0.80f , 0.20f );

	colors[ImGuiCol_ModalWindowDimBg] = ImVec4( 0.80f , 0.80f , 0.80f , 0.35f );
}

auto CPluginSenseGUI::SetClassicSteamStyle() -> void
{
	ImGuiStyle& style = ImGui::GetStyle();

	style.Alpha = 1.0f;
	style.DisabledAlpha = 0.6000000238418579f;
	style.WindowPadding = ImVec2( 8.0f , 8.0f );
	style.WindowRounding = 0.0f;
	style.WindowBorderSize = 1.0f;
	style.WindowMinSize = ImVec2( 32.0f , 32.0f );
	style.WindowTitleAlign = ImVec2( 0.0f , 0.5f );
	style.WindowMenuButtonPosition = ImGuiDir_Left;
	style.ChildRounding = 0.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupRounding = 0.0f;
	style.PopupBorderSize = 1.0f;
	style.FramePadding = ImVec2( 4.0f , 3.0f );
	style.FrameRounding = 0.0f;
	style.FrameBorderSize = 1.0f;
	style.ItemSpacing = ImVec2( 8.0f , 4.0f );
	style.ItemInnerSpacing = ImVec2( 4.0f , 4.0f );
	style.CellPadding = ImVec2( 4.0f , 2.0f );
	style.IndentSpacing = 21.0f;
	style.ColumnsMinSpacing = 6.0f;
	style.ScrollbarSize = 14.0f;
	style.ScrollbarRounding = 0.0f;
	style.GrabMinSize = 10.0f;
	style.GrabRounding = 0.0f;
	style.TabRounding = 0.0f;
	style.TabBorderSize = 0.0f;
	style.TabCloseButtonMinWidthUnselected = 0.0f;
	style.ColorButtonPosition = ImGuiDir_Right;
	style.ButtonTextAlign = ImVec2( 0.5f , 0.5f );
	style.SelectableTextAlign = ImVec2( 0.0f , 0.0f );

	style.Colors[ImGuiCol_Text] = ImVec4( 1.0f , 1.0f , 1.0f , 1.0f );
	style.Colors[ImGuiCol_TextDisabled] = ImVec4( 0.4980392158031464f , 0.4980392158031464f , 0.4980392158031464f , 1.0f );
	style.Colors[ImGuiCol_WindowBg] = ImVec4( 0.2862745225429535f , 0.3372549116611481f , 0.2588235437870026f , 1.0f );
	style.Colors[ImGuiCol_ChildBg] = ImVec4( 0.2862745225429535f , 0.3372549116611481f , 0.2588235437870026f , 1.0f );
	style.Colors[ImGuiCol_PopupBg] = ImVec4( 0.239215686917305f , 0.2666666805744171f , 0.2000000029802322f , 1.0f );
	style.Colors[ImGuiCol_Border] = ImVec4( 0.5372549295425415f , 0.5686274766921997f , 0.5098039507865906f , 0.5f );
	style.Colors[ImGuiCol_BorderShadow] = ImVec4( 0.1372549086809158f , 0.1568627506494522f , 0.1098039224743843f , 0.5199999809265137f );
	style.Colors[ImGuiCol_FrameBg] = ImVec4( 0.239215686917305f , 0.2666666805744171f , 0.2000000029802322f , 1.0f );
	style.Colors[ImGuiCol_FrameBgHovered] = ImVec4( 0.2666666805744171f , 0.2980392277240753f , 0.2274509817361832f , 1.0f );
	style.Colors[ImGuiCol_FrameBgActive] = ImVec4( 0.2980392277240753f , 0.3372549116611481f , 0.2588235437870026f , 1.0f );
	style.Colors[ImGuiCol_TitleBg] = ImVec4( 0.239215686917305f , 0.2666666805744171f , 0.2000000029802322f , 1.0f );
	style.Colors[ImGuiCol_TitleBgActive] = ImVec4( 0.2862745225429535f , 0.3372549116611481f , 0.2588235437870026f , 1.0f );
	style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4( 0.0f , 0.0f , 0.0f , 0.5099999904632568f );
	style.Colors[ImGuiCol_MenuBarBg] = ImVec4( 0.239215686917305f , 0.2666666805744171f , 0.2000000029802322f , 1.0f );
	style.Colors[ImGuiCol_ScrollbarBg] = ImVec4( 0.3490196168422699f , 0.4196078479290009f , 0.3098039329051971f , 1.0f );
	style.Colors[ImGuiCol_ScrollbarGrab] = ImVec4( 0.2784313857555389f , 0.3176470696926117f , 0.239215686917305f , 1.0f );
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4( 0.2470588237047195f , 0.2980392277240753f , 0.2196078449487686f , 1.0f );
	style.Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4( 0.2274509817361832f , 0.2666666805744171f , 0.2078431397676468f , 1.0f );
	style.Colors[ImGuiCol_CheckMark] = ImVec4( 0.5882353186607361f , 0.5372549295425415f , 0.1764705926179886f , 1.0f );
	style.Colors[ImGuiCol_SliderGrab] = ImVec4( 0.3490196168422699f , 0.4196078479290009f , 0.3098039329051971f , 1.0f );
	style.Colors[ImGuiCol_SliderGrabActive] = ImVec4( 0.5372549295425415f , 0.5686274766921997f , 0.5098039507865906f , 0.5f );
	style.Colors[ImGuiCol_Button] = ImVec4( 0.2862745225429535f , 0.3372549116611481f , 0.2588235437870026f , 0.4000000059604645f );
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4( 0.3490196168422699f , 0.4196078479290009f , 0.3098039329051971f , 1.0f );
	style.Colors[ImGuiCol_ButtonActive] = ImVec4( 0.5372549295425415f , 0.5686274766921997f , 0.5098039507865906f , 0.5f );
	style.Colors[ImGuiCol_Header] = ImVec4( 0.3490196168422699f , 0.4196078479290009f , 0.3098039329051971f , 1.0f );
	style.Colors[ImGuiCol_HeaderHovered] = ImVec4( 0.3490196168422699f , 0.4196078479290009f , 0.3098039329051971f , 0.6000000238418579f );
	style.Colors[ImGuiCol_HeaderActive] = ImVec4( 0.5372549295425415f , 0.5686274766921997f , 0.5098039507865906f , 0.5f );
	style.Colors[ImGuiCol_Separator] = ImVec4( 0.1372549086809158f , 0.1568627506494522f , 0.1098039224743843f , 1.0f );
	style.Colors[ImGuiCol_SeparatorHovered] = ImVec4( 0.5372549295425415f , 0.5686274766921997f , 0.5098039507865906f , 1.0f );
	style.Colors[ImGuiCol_SeparatorActive] = ImVec4( 0.5882353186607361f , 0.5372549295425415f , 0.1764705926179886f , 1.0f );
	style.Colors[ImGuiCol_ResizeGrip] = ImVec4( 0.1882352977991104f , 0.2274509817361832f , 0.1764705926179886f , 0.0f );
	style.Colors[ImGuiCol_ResizeGripHovered] = ImVec4( 0.5372549295425415f , 0.5686274766921997f , 0.5098039507865906f , 1.0f );
	style.Colors[ImGuiCol_ResizeGripActive] = ImVec4( 0.5882353186607361f , 0.5372549295425415f , 0.1764705926179886f , 1.0f );
	style.Colors[ImGuiCol_Tab] = ImVec4( 0.3490196168422699f , 0.4196078479290009f , 0.3098039329051971f , 1.0f );
	style.Colors[ImGuiCol_TabHovered] = ImVec4( 0.5372549295425415f , 0.5686274766921997f , 0.5098039507865906f , 0.7799999713897705f );
	style.Colors[ImGuiCol_TabActive] = ImVec4( 0.5882353186607361f , 0.5372549295425415f , 0.1764705926179886f , 1.0f );
	style.Colors[ImGuiCol_TabUnfocused] = ImVec4( 0.239215686917305f , 0.2666666805744171f , 0.2000000029802322f , 1.0f );
	style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4( 0.3490196168422699f , 0.4196078479290009f , 0.3098039329051971f , 1.0f );
	style.Colors[ImGuiCol_PlotLines] = ImVec4( 0.6078431606292725f , 0.6078431606292725f , 0.6078431606292725f , 1.0f );
	style.Colors[ImGuiCol_PlotLinesHovered] = ImVec4( 0.5882353186607361f , 0.5372549295425415f , 0.1764705926179886f , 1.0f );
	style.Colors[ImGuiCol_PlotHistogram] = ImVec4( 1.0f , 0.7764706015586853f , 0.2784313857555389f , 1.0f );
	style.Colors[ImGuiCol_PlotHistogramHovered] = ImVec4( 1.0f , 0.6000000238418579f , 0.0f , 1.0f );
	style.Colors[ImGuiCol_TableHeaderBg] = ImVec4( 0.1882352977991104f , 0.1882352977991104f , 0.2000000029802322f , 1.0f );
	style.Colors[ImGuiCol_TableBorderStrong] = ImVec4( 0.3098039329051971f , 0.3098039329051971f , 0.3490196168422699f , 1.0f );
	style.Colors[ImGuiCol_TableBorderLight] = ImVec4( 0.2274509817361832f , 0.2274509817361832f , 0.2470588237047195f , 1.0f );
	style.Colors[ImGuiCol_TableRowBg] = ImVec4( 0.0f , 0.0f , 0.0f , 0.0f );
	style.Colors[ImGuiCol_TableRowBgAlt] = ImVec4( 1.0f , 1.0f , 1.0f , 0.05999999865889549f );
	style.Colors[ImGuiCol_TextSelectedBg] = ImVec4( 0.5882353186607361f , 0.5372549295425415f , 0.1764705926179886f , 1.0f );
	style.Colors[ImGuiCol_DragDropTarget] = ImVec4( 0.729411780834198f , 0.6666666865348816f , 0.239215686917305f , 1.0f );
	style.Colors[ImGuiCol_NavHighlight] = ImVec4( 0.5882353186607361f , 0.5372549295425415f , 0.1764705926179886f , 1.0f );
	style.Colors[ImGuiCol_NavWindowingHighlight] = ImVec4( 1.0f , 1.0f , 1.0f , 0.699999988079071f );
	style.Colors[ImGuiCol_NavWindowingDimBg] = ImVec4( 0.800000011920929f , 0.800000011920929f , 0.800000011920929f , 0.2000000029802322f );
	style.Colors[ImGuiCol_ModalWindowDimBg] = ImVec4( 0.800000011920929f , 0.800000011920929f , 0.800000011920929f , 0.3499999940395355f );
}

auto CPluginSenseGUI::UpdateStyle() -> void
{
	ImGui::SetCurrentContext( m_pImGuiContext );

	if ( Settings::Menu::MenuStyle == EPluginSenseGuiStyle_t::PLUGINSENSE_GUI_STYLE_INDIGO )
		SetIndigoStyle();
	else if ( Settings::Menu::MenuStyle == EPluginSenseGuiStyle_t::PLUGINSENSE_GUI_STYLE_VERMILLION )
		SetVermillionStyle();
	else if ( Settings::Menu::MenuStyle == EPluginSenseGuiStyle_t::PLUGINSENSE_GUI_STYLE_CLASSIC_STEAM )
		SetClassicSteamStyle();
}

bool CPluginSenseGUI::FreeTypeBuild::PreNewFrame()
{
	if ( !WantRebuild )
		return false;

	ImFontAtlas* atlas = ImGui::GetIO().Fonts;

	for ( int n = 0; n < atlas->Sources.Size; n++ )
		( (ImFontConfig*)&atlas->Sources[n] )->RasterizerMultiply = RasterizerMultiply;

#ifdef IMGUI_ENABLE_FREETYPE
	if ( BuildMode == FontBuildMode::FreeType )
	{
		atlas->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
		atlas->FontBuilderFlags = FreeTypeBuilderFlags;
	}
#endif

	atlas->Build();
	WantRebuild = false;

	return true;
}

auto CPluginSenseGUI::ClearRenderTargetView() -> void
{
	if ( m_pRenderTargetView )
	{
		m_pRenderTargetView->Release();
		m_pRenderTargetView = nullptr;
	}
	if ( m_pMainRenderTarget )
	{
		m_pMainRenderTarget->Release();
		m_pMainRenderTarget = nullptr;
	}
}

auto GetPluginSenseGUI() -> CPluginSenseGUI*
{
	return &g_PluginSenseGUI;
}

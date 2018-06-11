#include "Editor.h"
#include <WindowsX.h>

using namespace std;
using namespace DirectX;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return WZRDEditor::GetEditor()->MsgProc(hwnd, msg, wParam, lParam);
}

WZRDEditor* WZRDEditor::m_editor = nullptr;
WZRDEditor* WZRDEditor::GetEditor()
{
	return m_editor;
}

//WZRDRenderer* WZRDEditor::GetRenderer() {
//	return m_renderer;
//}

AbstractRenderer* WZRDEditor::GetAppRenderer() {
	return m_appRenderer;
}

//WZRDEditor::WZRDEditor(HINSTANCE hInstance, WZRDRenderer* renderer)
//	: m_editorInstance(hInstance), m_renderer(renderer)
//{
//	// Only one editor can be constructed.
//	assert(m_editor == nullptr);
//	m_editor = this;
//}

WZRDEditor::WZRDEditor(HINSTANCE hInstance, AbstractRenderer* renderer)
	: m_editorInstance(hInstance), m_appRenderer(renderer)
{
	assert(m_editor == nullptr);
	m_editor = this;
}

//WZRDEditor::~WZRDEditor()
//{
//	if (md3dDevice != nullptr)
//		FlushCommandQueue();
//}

float WZRDEditor::AspectRatio() const {
	return static_cast<float>(m_clientWidth) / m_clientHeight;
}

HINSTANCE WZRDEditor::EditorInst()const
{
	return m_editorInstance;
}

HWND WZRDEditor::MainWnd()
{
	return m_mainWindow;
}

int WZRDEditor::Run()
{
	MSG msg = { 0 };

	m_gameTimer.Reset();

	while (msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		// Otherwise, do animation/game stuff.
		else
		{
			m_gameTimer.Tick();

			if (!m_editorPaused)
			{
				m_appRenderer->update(m_gameTimer);
				m_appRenderer->render();
			}
			else
			{
				Sleep(100);
			}
		}
	}

	return (int)msg.wParam;
}

bool WZRDEditor::InitMainWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hInstance = m_editorInstance;
	wc.hIcon = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszMenuName = 0;
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, m_clientWidth, m_clientHeight};
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);
	int width = R.right - R.left;
	int height = R.bottom - R.top;


	m_mainWindow = CreateWindow(L"MainWnd", L"WZRD Editor",
		WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, m_editorInstance, 0);

	if (!m_mainWindow)
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	this->m_appRenderer->outputWindow = m_mainWindow;

	ShowWindow(m_mainWindow, SW_SHOW);
	UpdateWindow(m_mainWindow);

	return true;
}

LRESULT WZRDEditor::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		// WM_ACTIVATE is sent when the window is activated or deactivated.  
		// We pause the game when the window is deactivated and unpause it 
		// when it becomes active.  
	//case WM_ACTIVATE:
	//	if (LOWORD(wParam) == WA_INACTIVE)
	//	{
	//		mAppPaused = true;
	//		mTimer.Stop();
	//	}
	//	else
	//	{
	//		mAppPaused = false;
	//		mTimer.Start();
	//	}
	//	return 0;

		// WM_SIZE is sent when the user resizes the window.  
	//case WM_SIZE:
	//	// Save the new client area dimensions.
	//	m_clientWidth = LOWORD(lParam);
	//	m_clientHeight = HIWORD(lParam);
	//	if (md3dDevice)
	//	{
	//		if (wParam == SIZE_MINIMIZED)
	//		{
	//			mAppPaused = true;
	//			mMinimized = true;
	//			mMaximized = false;
	//		}
	//		else if (wParam == SIZE_MAXIMIZED)
	//		{
	//			mAppPaused = false;
	//			mMinimized = false;
	//			mMaximized = true;
	//			OnResize();
	//		}
	//		else if (wParam == SIZE_RESTORED)
	//		{

	//			// Restoring from minimized state?
	//			if (mMinimized)
	//			{
	//				mAppPaused = false;
	//				mMinimized = false;
	//				OnResize();
	//			}

	//			// Restoring from maximized state?
	//			else if (mMaximized)
	//			{
	//				mAppPaused = false;
	//				mMaximized = false;
	//				OnResize();
	//			}
	//			else if (mResizing)
	//			{
	//				// If user is dragging the resize bars, we do not resize 
	//				// the buffers here because as the user continuously 
	//				// drags the resize bars, a stream of WM_SIZE messages are
	//				// sent to the window, and it would be pointless (and slow)
	//				// to resize for each WM_SIZE message received from dragging
	//				// the resize bars.  So instead, we reset after the user is 
	//				// done resizing the window and releases the resize bars, which 
	//				// sends a WM_EXITSIZEMOVE message.
	//			}
	//			else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
	//			{
	//				OnResize();
	//			}
	//		}
	//	}
	//	return 0;

		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	//case WM_ENTERSIZEMOVE:
	//	mAppPaused = true;
	//	mResizing = true;
	//	mTimer.Stop();
	//	return 0;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	//case WM_EXITSIZEMOVE:
	//	mAppPaused = false;
	//	mResizing = false;
	//	mTimer.Start();
	//	OnResize();
	//	return 0;

		// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

		// The WM_MENUCHAR message is sent when a menu is active and the user presses 
		// a key that does not correspond to any mnemonic or accelerator key. 
	//case WM_MENUCHAR:
	//	// Don't beep when we alt-enter.
	//	return MAKELRESULT(0, MNC_CLOSE);

		// Catch this message so to prevent the window from becoming too small.
	//case WM_GETMINMAXINFO:
	//	((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
	//	((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
	//	return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		GetAppRenderer()->OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		GetAppRenderer()->OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		GetAppRenderer()->OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	//case WM_KEYUP:
	//	if (wParam == VK_ESCAPE)
	//	{
	//		PostQuitMessage(0);
	//	}
	//	else if ((int)wParam == VK_F2)
	//		Set4xMsaaState(!m4xMsaaState);

	//	return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}
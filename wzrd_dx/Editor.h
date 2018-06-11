#pragma once

#include <windows.h>
#include "Utilities.h"
//#include "Renderer.h"
#include "GameTimer.h"
#include "App.h"

class WZRDEditor {
public:
	//WZRDEditor(HINSTANCE hInstance, WZRDRenderer* renderer);
	WZRDEditor(HINSTANCE hInstance, AbstractRenderer* renderer);
	int Run();
	bool InitMainWindow();
	static WZRDEditor* GetEditor();
	//WZRDRenderer* GetRenderer();
	AbstractRenderer* GetAppRenderer();
	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	HINSTANCE EditorInst() const;
	HWND MainWnd();
	GameTimer m_gameTimer;
	float AspectRatio() const;

private:
	HWND m_mainWindow = nullptr;
	static WZRDEditor* m_editor;
	//WZRDRenderer* m_renderer;
	HINSTANCE m_editorInstance = nullptr;

	AbstractRenderer* m_appRenderer;


	bool m_editorPaused = false;

	int m_clientWidth = 1600;
	int m_clientHeight = 1200;

};
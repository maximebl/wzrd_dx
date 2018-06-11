#pragma once
#include <windows.h>
#include "Renderer.h"
#include "Editor.h"
#include "BoxApp.h"
#include "ShapesApp.h"
#include "MirrorApp.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	try
	{
		//WZRDRenderer renderer;
		//WZRDEditor editor(hInstance, &renderer);
		//
		//if (!editor.InitMainWindow())
		//	return 0;

		//if (!editor.GetRenderer()->InitDirect3D())
		//	return 0;

		//editor.GetRenderer()->Resize(editor.AspectRatio());

		//// switch on what to init here

		//if (!editor.GetRenderer()->InitBoxApp())
		//	return 0;

		ShapesApp shapesApp;
		//MirrorApp mirrorApp;
		WZRDEditor editor(hInstance, &shapesApp);

		if (!editor.InitMainWindow()) {
			return 0;
		}

		if (!editor.GetAppRenderer()->InitDirect3D()) {
			return 0;
		}

		editor.GetAppRenderer()->resize(editor.AspectRatio());

		if (!editor.GetAppRenderer()->init()) {
			return 0;
		}

		return editor.Run();
	}
	catch (std::exception &e)
	{
		std::string error = std::string(e.what());
		std::wstring result = std::wstring(error.begin(), error.end());
		LPCWSTR sw = result.c_str();

		MessageBox(nullptr, sw, L"HR Failed", MB_OK);
		return 0;
	}
}
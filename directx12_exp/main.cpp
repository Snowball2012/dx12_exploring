#include <Windows.h>
#include <crtdbg.h>

#include <d3d12.h>

#include "DXLayer.h"

// mostly winapi stuff 

// debug helpers
// todo: move from main.cpp
#ifdef _DEBUG
	#define NOTIMPL _ASSERT_EXPR(false, L"not implemented yet") 
#else
	#define NOTIMPL
#endif

// global winapi vars
// todo: organize this mess

// name of the window (not the title)
LPCTSTR WindowName = L"Dx12ExpApp";

// title of the window
LPCTSTR WindowTitle = L"DirectX12 exploring";

namespace
{
	// callback function for windows messages
	LRESULT CALLBACK WndProc(HWND hwnd,
		UINT msg,
		WPARAM wParam,
		LPARAM lParam)
	{
		switch (msg)
		{

		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE) {
				if (MessageBox(0, L"Are you sure you want to exit?",
					L"Really?", MB_YESNO | MB_ICONQUESTION) == IDYES)
					DestroyWindow(hwnd);
			}
			return 0;

		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProc(hwnd,
			msg,
			wParam,
			lParam);
	}

	bool InitializeWindow(HINSTANCE hInstance, int nCmdShow, bool is_fullscreen, int& width, int& height, HWND& hwnd)
	{
		if (is_fullscreen)
		{
			HMONITOR hmon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFO mi = { sizeof(mi) };
			GetMonitorInfo(hmon, &mi);

			width = mi.rcMonitor.right - mi.rcMonitor.left;
			height = mi.rcMonitor.bottom - mi.rcMonitor.top;
		}

		WNDCLASSEX wc;
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc;
		wc.cbClsExtra = NULL;
		wc.cbWndExtra = NULL;
		wc.hInstance = hInstance;
		wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
		wc.hCursor = LoadCursor(NULL, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
		wc.lpszMenuName = NULL;
		wc.lpszClassName = WindowName;
		wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

		if (!RegisterClassEx(&wc))
		{
			MessageBox(NULL, L"Error registering window class",
				L"Error", MB_OK | MB_ICONERROR);
			return false;
		}

		hwnd = CreateWindowEx(NULL,
			WindowName,
			WindowTitle,
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT, CW_USEDEFAULT,
			width, height,
			NULL,
			NULL,
			hInstance,
			NULL);

		if (!hwnd)
		{
			MessageBox(NULL, L"Error creating window",
				L"Error", MB_OK | MB_ICONERROR);
			return false;
		}

		if (is_fullscreen)
		{
			SetWindowLong(hwnd, GWL_STYLE, 0);
		}

		ShowWindow(hwnd, nCmdShow);
		UpdateWindow(hwnd);

		return true;
	}

	void MainLoop()
	{
		MSG msg;
		ZeroMemory(&msg, sizeof(MSG));

		while (true)
		{
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
					break;

				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				// run game code
			}
		}
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	int width = 800;
	int height = 600;
	bool is_fullscreen = false;

	HWND hWnd = NULL;

	if (!InitializeWindow(hInstance, nCmdShow, is_fullscreen, width, height, hWnd))
	{
		MessageBox(0, L"Window initialization failed", L"Error", MB_OK);
		return 0;
	}

	if (!DXLayer::InitD3D(hWnd, width, height, is_fullscreen))
	{
		MessageBox(0, L"Failed to initialize D3D12", L"Error", MB_OK);
		DXLayer::Cleanup();
		return 1;
	}

	DXLayer::WaitForPreviousFrame();

	CloseHandle(DXLayer::fence_event);

	MainLoop();
}
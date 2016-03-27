#include "stdafx.h"
#include "framework.h"

Application::Application (int window_width, int window_height, const char *window_caption):
	is_minimized (false)
{
	d3d12.width = window_width;
	d3d12.height = window_height;
	try
	{
		InitLog ();
		InitWindow (window_caption);
	}
	catch (std::exception err)
	{
		throw framework_err ("Application initialization error");
	}
	Log ("Application initialized successfully");
}

Application::~Application ()
{
	d3d12.Destroy ();
	Log ("Finishing application");
	CloseLog ();
}

void Application::Run ()
{
	try
	{
		MSG msg;
		while (true)
		{
			if (PeekMessage (&msg, d3d12.hWnd, 0, 0, PM_REMOVE))
			{
				TranslateMessage (&msg);
				DispatchMessage (&msg);
			}

			if (msg.message == WM_QUIT)
				break;

			if (!is_minimized)
			{
				d3d12.Update ();
				d3d12.Render ();
			}
		}
	}
	catch (framework_err err)
	{
		throw framework_err ("Error occured while processing main loop");
	}
}

void Application::InitWindow (const char *caption)
{
	TCHAR WinName[] = _T ("Direct3D 12 Framework");
	HINSTANCE hInstance = GetModuleHandle (NULL);
	WNDCLASS wc = {};
	wc.hInstance = hInstance;
	wc.lpszClassName = WinName;
	wc.lpfnWndProc = WndProc;
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.hIcon = LoadIcon (NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor (NULL, IDC_ARROW);
	wc.lpszMenuName = NULL;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;

	if (!RegisterClass (&wc))
		throw framework_err ("Can not register window class");

	RECT windowRect = { 0, 0, d3d12.width, d3d12.height };
	AdjustWindowRect (&windowRect, WS_OVERLAPPEDWINDOW, false);

	d3d12.hWnd = CreateWindow (WinName,
							   CA2T (caption),
							   WS_OVERLAPPEDWINDOW,
							   CW_USEDEFAULT,
							   CW_USEDEFAULT,
							   windowRect.right - windowRect.left,
							   windowRect.bottom - windowRect.top,
							   NULL,
							   NULL,
							   hInstance,
							   this);

	if (!d3d12.hWnd)
		throw framework_err ("Can not create window");
	
	try
	{
		d3d12.Init ();
	}
	catch (framework_err err)
	{
		throw framework_err("Can not initialize Direct3D 12");
	}
	ShowWindow (d3d12.hWnd, SW_SHOW);

	Log ("Window created successfully");
}

LRESULT CALLBACK Application::WndProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Application *app = reinterpret_cast<Application*>(GetWindowLongPtr (hWnd, GWLP_USERDATA));
	switch (msg)
	{
	case WM_CREATE:
	{
		LPCREATESTRUCT create_struct = reinterpret_cast<LPCREATESTRUCT>(lParam);
		SetWindowLongPtr (hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create_struct->lpCreateParams));
		break;
	}
	case WM_CLOSE:
		PostQuitMessage (0);
		break;
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			PostQuitMessage (0);
		break;
	case WM_SIZE:
		if (app)
		{
			RECT windowRect = {};
			GetClientRect (hWnd, &windowRect);
			app->d3d12.Resize (windowRect.right - windowRect.left,
							   windowRect.bottom - windowRect.top);
			if (wParam == SIZE_MINIMIZED)
				app->is_minimized = true;
			else
				app->is_minimized = false;
		}
		break;
		
	default:
		return DefWindowProc (hWnd, msg, wParam, lParam);
	}
	return 0;
}
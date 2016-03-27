#pragma once
#include "stdafx.h"
#include "graphics.h"
#include "errors.h"

class Application
{
public:
	Application (int window_width, int window_height, const char *window_caption);
	~Application ();
	void Run ();

	Graphics d3d12;
	bool is_minimized;
private:
	void InitWindow (const char *caption);

	static LRESULT CALLBACK WndProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
};
#include "stdafx.h"
#include "framework.h"

const int width = 1280;
const int height = 720;
const char caption[] = "Direct3D 12 Application";

int WINAPI WinMain (HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmd, int mode)
{
	try
	{
		Application app (width, height, caption);
		app.Run ();
	}
	catch (std::exception err)
	{
		PrintMessage ("An error occured. See log.txt for more information");
		return 1;
	}
	catch (...)
	{
		PrintMessage ("Unknown error occured. The program will be terminated");
		return 1;
	}
	return 0;
}
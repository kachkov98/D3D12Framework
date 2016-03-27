#pragma once

#include <Windows.h>
#include <wrl.h>
#include <tchar.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "d3dcompiler.lib")

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <atlstr.h>

#include <exception>
#include <string>
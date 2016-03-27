#pragma once
#include "stdafx.h"

#define THROWIFFAILED(func, str) if (FAILED(func)) throw framework_err (str)

void PrintMessage (const char *str);

void InitLog ();
void Log (const char *format_str, ...);
void CloseLog ();

class framework_err:public std::exception
{
public:
	framework_err (const char *str) throw () :
		err_msg (str)
	{
		Log ("%s", str);
	};
	virtual ~framework_err () throw ()
	{
	};
	virtual const char *what ()
	{
		return err_msg.c_str ();
	}
private:
	std::string err_msg;
};
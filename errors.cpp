#include "stdafx.h"
#include "errors.h"

static FILE *f;

void PrintMessage (const char *str)
{
	MessageBoxA (NULL, str, "Framework information", MB_OK);
}

void InitLog ()
{
	if (fopen_s (&f, "log.txt", "w"))
	{
		PrintMessage ("Can not open log file!");
		exit (EXIT_FAILURE);
	}
}

void CloseLog ()
{
	fclose (f);
}

void Log (const char *format_str, ...)
{
	time_t rawtime;
	tm t;
	time (&rawtime);
	char time_str[40];
	localtime_s (&t, &rawtime);
	strftime (time_str, 40, "[%D %T] ", &t);
	fputs (time_str, f);

	va_list args;
	va_start (args, format_str);
	vfprintf (f, format_str, args);
	fputc ('\n', f);
	va_end (args);
	fflush (f);
}
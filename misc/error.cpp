/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1998 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "platform/mono.h"
#include "misc/error.h"
#include "platform/platform.h"

#ifdef WIN32
#include <Windows.h>
#include <debugapi.h>
#else
#include <signal.h>
#endif

#if defined(CHOCOLATE_USE_LOCALIZED_PATHS)
#define MAX_MSG_LEN 65536
#else
#define MAX_MSG_LEN 256
#endif

#undef Int3 //Uncomment on dev environments where an Int3 might not produce any message

int initialized = 0;

char exit_message[MAX_MSG_LEN] = "";
char warn_message[MAX_MSG_LEN];

//takes string in register, calls printf with string on stack
void warn_printf(const char* s)
{
	printf("%s\n", s);
}

void (*warn_func)(const char* s) = warn_printf;

//provides a function to call with warning messages
void set_warn_func(void (*f)(const char* s))
{
	warn_func = f;
}

//uninstall warning function - install default printf
void clear_warn_func(void (*f)(const char* s))
{
	warn_func = warn_printf;
}

void set_exit_message(const char* fmt, ...)
{
	va_list arglist;
	int len;

	va_start(arglist, fmt);
	len = vsnprintf(exit_message, MAX_MSG_LEN, fmt, arglist);
	va_end(arglist);

	if (len == -1 || len > MAX_MSG_LEN) Error("Message too long in set_exit_message (len=%d, max=%d)", len, MAX_MSG_LEN);

}

void _Assert(int expr, const char* expr_text, const char* filename, int linenum)
{
#ifndef NDEBUG
	if (!(expr))
	{
		Error("Assertion failed: %s, file %s, line %d", expr_text, filename, linenum);
	}
#endif
}

void Int3()
{
#ifndef NDEBUG
	printf("Int3 tripped\n");
# ifdef WIN32
	DebugBreak();
# else
	raise(SIGTRAP);
# endif
#endif
}

void print_exit_message(void)
{
	if (*exit_message)
		printf("%s\n", exit_message);
}

//terminates with error code 1, printing message
void Error(const char* fmt, ...) 
{
	va_list arglist;

	strcpy(exit_message, "\nError: ");

	va_start(arglist, fmt);
	size_t len = strlen(exit_message);
	vsnprintf(exit_message + len, MAX_MSG_LEN - len, fmt, arglist);
	va_end(arglist);

	if (!initialized) print_exit_message();

	plat_display_error(exit_message);

	#ifndef NDEBUG
	Int3();
	#else
	exit(1);
	#endif
}

//print out warning message to user
void Warning(const char* fmt, ...)
{
	va_list arglist;

	if (warn_func == NULL)
		return;

	strcpy(warn_message, "Warning: ");

	va_start(arglist, fmt);
	size_t len = strlen(warn_message);
	vsnprintf(warn_message + len, MAX_MSG_LEN - len, fmt, arglist);
	va_end(arglist);

	mprintf((0, "%s\n", warn_message));
	(*warn_func)(warn_message);

}

//initialize error handling system, and set default message. returns 0=ok
int error_init(const char* fmt, ...)
{
	va_list arglist;
	int len;

	atexit(print_exit_message);		//last thing at exit is print message

	if (fmt != NULL) {
		va_start(arglist, fmt);
		len = vsnprintf(exit_message, MAX_MSG_LEN, fmt, arglist);
		va_end(arglist);
		if (len == -1 || len > MAX_MSG_LEN) Error("Message too long in error_init (len=%d, max=%d)", len, MAX_MSG_LEN);
	}

	initialized = 1;

	return 0;
}

/*
 * win32.c
 * - utility functions for cvs under win32
 *
 */

#include <ctype.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <config.h>

unsigned sleep(unsigned seconds)
{
	Sleep(1000*seconds);
	return 0;
}

#if 0
/* This is available from the WinSock library.  */
int gethostname(char* name, int namelen)
{
	DWORD dw = namelen;
	BOOL ret = GetComputerName(name, &dw);
	namelen = dw;
	return (ret) ? 0 : -1;
}
#endif

char* win32getlogin()
{
	static char name[256];
	DWORD dw = 256;
	GetUserName(name, &dw);
	return name;
}


pid_t
getpid ()
{
    return (pid_t) GetCurrentProcessId();
}

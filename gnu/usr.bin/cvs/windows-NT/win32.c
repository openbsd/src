/*
 * win32.c
 * - utility functions for cvs under win32
 *
 */

#include <ctype.h>
#include <stdio.h>
#include <conio.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <config.h>
#include <winsock.h>
#include <stdlib.h>

#include "cvs.h"

void
init_winsock ()
{
    WSADATA data;

    if (WSAStartup (MAKEWORD (1, 1), &data))
    {
	fprintf (stderr, "cvs: unable to initialize winsock\n");
	exit (1);
    }
}

void
wnt_cleanup (void)
{
    if (WSACleanup ())
    {
#ifdef SERVER_ACTIVE
	if (server_active || error_use_protocol)
	    /* FIXME: how are we supposed to report errors?  As of now
	       (Sep 98), error() can in turn call us (if it is out of
	       memory) and in general is built on top of lots of
	       stuff.  */
	    ;
	else
#endif
	    fprintf (stderr, "cvs: cannot WSACleanup: %s\n",
		     sock_strerror (WSAGetLastError ()));
    }
}

unsigned sleep(unsigned seconds)
{
	Sleep(1000*seconds);
	return 0;
}

#if 0

/* WinSock has a gethostname.  But note that WinSock gethostname may
   want to talk to the network, which is kind of bogus in the
   non-client/server case.  I'm not sure I can think of any obvious
   solution.  Most of the ways I can think of to figure out whether
   to call gethostname or GetComputerName seem kind of kludgey, and/or
   might result in picking the name in a potentially confusing way
   (I'm not sure exactly how the name(s) are set).  */

int gethostname(char* name, int namelen)
{
	DWORD dw = namelen;
	BOOL ret = GetComputerName(name, &dw);
	namelen = dw;
	return (ret) ? 0 : -1;
}
#endif

char *win32getlogin()
{
    static char name[256];
    DWORD dw = 256;
    GetUserName (name, &dw);
    if (name[0] == '\0')
	return NULL;
    else
	return name;
}


pid_t
getpid ()
{
    return (pid_t) GetCurrentProcessId();
}

char *
getpass (const char *prompt)
{
    static char pwd_buf[128];
    size_t i;

    fputs (prompt, stderr);
    fflush (stderr);
    for (i = 0; i < sizeof (pwd_buf) - 1; ++i)
    {
	pwd_buf[i] = _getch ();
	if (pwd_buf[i] == '\r')
	    break;
    }
    pwd_buf[i] = '\0';
    fputs ("\n", stderr);
    return pwd_buf;
}

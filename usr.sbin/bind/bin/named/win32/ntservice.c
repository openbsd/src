/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: ntservice.c,v 1.3.2.1 2001/09/04 19:40:20 gson Exp $ */

#include <config.h>
#include <stdio.h>

#include <isc/app.h>
#include <isc/log.h>

#include <named/globals.h>
#include <named/ntservice.h>
#include <named/main.h>
#include <named/server.h>

/* Handle to SCM for updating service status */
static SERVICE_STATUS_HANDLE hServiceStatus = 0;
static int foreground = FALSE;
static char ConsoleTitle[128];

/*
 * Forward declarations
 */
void ServiceControl(DWORD dwCtrlCode);
void GetArgs(int *, char ***, char ***);
int main(int, char *[], char *[]); /* From ns_main.c */

/*
 * Here we change the entry point for the executable to bindmain() from main()
 * This allows us to invoke as a service or from the command line easily.
 */
#pragma comment(linker, "/entry:bindmain")

/*
 * This is the entry point for the executable 
 * We can now call main() explicitly or via StartServiceCtrlDispatcher()
 * as we need to.
 */
int bindmain()
{
	int rc,
	i = 1;

	int argc;
	char **envp, **argv;

	/*
	 * We changed the entry point function, so we must initialize argv,
	 * etc. ourselves.  Ick.
	 */
	GetArgs(&argc, &argv, &envp);

	/* Command line users should put -f in the options */
	while (argv[i]) {
		if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "-g")) {
			foreground = TRUE;
			break;
		}
		i++;
	}

	if (foreground) {
		/* run in console window */
		exit(main(argc, argv, envp));
	} else {
		/* Start up as service */
		char *SERVICE_NAME = BIND_SERVICE_NAME;

		SERVICE_TABLE_ENTRY dispatchTable[] = {
			{ TEXT(SERVICE_NAME), (LPSERVICE_MAIN_FUNCTION)main },
			{ NULL, NULL }
		};

		rc = StartServiceCtrlDispatcher(dispatchTable);
		if (!rc) {
			fprintf(stderr, "Use -f to run from the command line.\n");
			exit(GetLastError());
		}
	}
	exit(0);
}

/*
 * Initialize the Service by registering it.
 */
void
ntservice_init() {
	if (!foreground) {
		/* Register handler with the SCM */
		hServiceStatus = RegisterServiceCtrlHandler(BIND_SERVICE_NAME,
					(LPHANDLER_FUNCTION)ServiceControl);
		if (!hServiceStatus) {
			ns_main_earlyfatal(
				"could not register service control handler");
			UpdateSCM(SERVICE_STOPPED);
			exit(1);
		}
		UpdateSCM(SERVICE_RUNNING);
	} else {
		strcpy(ConsoleTitle, "BIND Version ");
		strcat(ConsoleTitle, VERSION);
		SetConsoleTitle(ConsoleTitle);
	}
}

void
ntservice_shutdown() {
	UpdateSCM(SERVICE_STOPPED);
}

/* 
 * ServiceControl(): Handles requests from the SCM and passes them on
 * to named.
 */
void
ServiceControl(DWORD dwCtrlCode) {
	/* Handle the requested control code */
	switch(dwCtrlCode) {
        case SERVICE_CONTROL_INTERROGATE:
		UpdateSCM(0);
		break;

        case SERVICE_CONTROL_STOP:
		ns_server_flushonshutdown(ns_g_server, ISC_TRUE);
		isc_app_shutdown();
		UpdateSCM(SERVICE_STOPPED);
		break;
        default:
		break;
	}
}

/*
 * Tell the Service Control Manager the state of the service.
 */
void UpdateSCM(DWORD state) {
	SERVICE_STATUS ss;
	static DWORD dwState = SERVICE_STOPPED;

	if (hServiceStatus) {
		if (state)
			dwState = state;

		memset(&ss, 0, sizeof(SERVICE_STATUS));
		ss.dwServiceType |= SERVICE_WIN32_OWN_PROCESS;
		ss.dwCurrentState = dwState;
		ss.dwControlsAccepted = SERVICE_ACCEPT_STOP |
					SERVICE_ACCEPT_SHUTDOWN;
		ss.dwCheckPoint = 0;
		ss.dwServiceSpecificExitCode = 0;
		ss.dwWin32ExitCode = NO_ERROR;
		ss.dwWaitHint = dwState == SERVICE_STOP_PENDING ? 10000 : 1000;

		if (!SetServiceStatus(hServiceStatus, &ss)) {
			ss.dwCurrentState = SERVICE_STOPPED;
			SetServiceStatus(hServiceStatus, &ss);
		}
	}
}

/*
 * C-runtime stuff used to initialize the app and
 * get argv, argc, envp.
 */

typedef struct 
{
	int newmode;
} _startupinfo;

_CRTIMP void __cdecl __set_app_type(int);
_CRTIMP void __cdecl __getmainargs(int *, char ***, char ***, int,
				   _startupinfo *);
void __cdecl _setargv(void);

#ifdef _M_IX86
/* Pentium FDIV adjustment */
extern int _adjust_fdiv;
extern int * _imp___adjust_fdiv;
/* Floating point precision */
extern void _setdefaultprecision();
#endif

extern int _newmode;		/* malloc new() handler mode */
extern int _dowildcard;		/* passed to __getmainargs() */

typedef void (__cdecl *_PVFV)(void);
extern void __cdecl _initterm(_PVFV *, _PVFV *);
extern _PVFV *__onexitbegin;
extern _PVFV *__onexitend;
extern _CRTIMP char **__initenv;

/*
 * Do the work that mainCRTStartup() would normally do
 */
void GetArgs(int *argc, char ***argv, char ***envp)
{
	_startupinfo startinfo;
    
	/*
	 * Set the app type to Console (check CRT/SRC/INTERNAL.H:
	 * #define _CONSOLE_APP 1)
	 */
	__set_app_type(1);
	
	/* Mark this module as an EXE file */
	__onexitbegin = __onexitend = (_PVFV *)(-1);

	startinfo.newmode = _newmode;
	__getmainargs(argc, argv, envp, _dowildcard, &startinfo);
	__initenv = *envp;

#ifdef _M_IX86
	_adjust_fdiv = * _imp___adjust_fdiv;
	_setdefaultprecision();
#endif
}

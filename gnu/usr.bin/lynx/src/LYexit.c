/*
 * $LynxId: LYexit.c,v 1.36 2013/10/24 09:16:37 tom Exp $
 *
 *	Copyright (c) 1994, University of Kansas, All Rights Reserved
 *	(most of this file was rewritten in 1996 and 2004).
 */
#include <HTUtils.h>
#include <LYexit.h>
#include <HTAlert.h>
#ifndef VMS
#include <LYGlobalDefs.h>
#include <LYUtils.h>
#include <LYSignal.h>
#include <LYMainLoop.h>
#endif /* !VMS */
#include <LYStrings.h>
#include <LYClean.h>

/*
 * Flag for outofmem macro.  - FM
 */
BOOL LYOutOfMemory = FALSE;

/*
 * Stack of functions to call upon exit.
 */
static void (*callstack[ATEXITSIZE]) (void);
static int topOfStack = 0;

/*
 * Capture "atexit()" calls for our own accounting.
 */
int LYatexit(void (*function) (void))
{
    int result = 0;

    if (topOfStack >= ATEXITSIZE) {
	CTRACE((tfp, "(LY)atexit: Too many functions, ignoring one!\n"));
	result = -1;
    } else {
	int n;
	BOOLEAN found = FALSE;

	for (n = 0; n < topOfStack; ++n) {
	    if (callstack[n] == function) {
		found = TRUE;
		break;
	    }
	}
	if (!found) {
	    callstack[topOfStack++] = function;
	}
    }
    return result;
}

/*
 * Purpose:		Call the functions registered with LYatexit
 * Arguments:		void
 * Return Value:	void
 * Remarks/Portability/Dependencies/Restrictions:
 * Revision History:
 *	06-15-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
static void LYCompleteExit(void)
{
    /*
     * Just loop through registered functions.  This is reentrant if more exits
     * occur in the registered functions.
     */
    while (--topOfStack >= 0) {
	callstack[topOfStack] ();
    }
}

/*
 * Purpose:		Terminates program, reports memory not freed.
 * Arguments:		status	Exit code.
 * Return Value:	void
 * Remarks/Portability/Dependencies/Restrictions:
 *	Function calls stdlib.h exit
 * Revision History:
 *	06-15-94	created Lynx 2-3-1 Garrett Arch Blythe
 */
void LYexit(int status)
{
#ifndef VMS			/*  On VMS, the VMSexit() handler does these. - FM */
#ifdef _WINDOWS
    DeleteCriticalSection(&critSec_DNS);
    DeleteCriticalSection(&critSec_READ);

    WSACleanup();
#endif
    if (LYOutOfMemory == TRUE) {
	/*
	 * Ignore further interrupts.  - FM
	 */
#ifndef NOSIGHUP
	(void) signal(SIGHUP, SIG_IGN);
#endif /* NOSIGHUP */
	(void) signal(SIGTERM, SIG_IGN);
	(void) signal(SIGINT, SIG_IGN);
#ifndef __linux__
#ifdef SIGBUS
	(void) signal(SIGBUS, SIG_IGN);
#endif /* SIGBUS */
#endif /* !__linux__ */
	(void) signal(SIGSEGV, SIG_IGN);
	(void) signal(SIGILL, SIG_IGN);

	/*
	 * Flush all messages.  - FM
	 */
	fflush(stderr);
	fflush(stdout);

	/*
	 * Deal with curses, if on, and clean up.  - FM
	 */
	if (LYCursesON) {
	    LYSleepAlert();
	}
	cleanup_sig(0);
#ifndef __linux__
#ifdef SIGBUS
	signal(SIGBUS, SIG_DFL);
#endif /* SIGBUS */
#endif /* !__linux__ */
	signal(SIGSEGV, SIG_DFL);
	signal(SIGILL, SIG_DFL);
    }
#endif /* !VMS */

    /*
     * Close syslog before doing atexit-cleanup, since it may use a string
     * that would be freed there.
     */
#ifdef SYSLOG_REQUESTED_URLS
    LYCloselog();
#endif

    /*
     * Do functions registered with LYatexit.  - GAB
     */
    LYCompleteExit();

    LYCloseCmdLogfile();

#ifdef exit
/*  Make sure we use stdlib exit and not LYexit. - GAB
*/
#undef exit
#endif /* exit */

    cleanup_files();		/* if someone starts with LYNXfoo: page */
#ifndef VMS			/*  On VMS, the VMSexit() handler does these. - FM */
    fflush(stderr);
    if (LYOutOfMemory == TRUE) {
	LYOutOfMemory = FALSE;
	printf("\r\n%s\r\n\r\n", MEMORY_EXHAUSTED_ABORT);
	fflush(stdout);
    }
    LYCloseTracelog();
#endif /* !VMS */
    show_alloc();

#if defined(NCURSES_VERSION) && defined(LY_FIND_LEAKS)
#if defined(HAVE__NC_FREE_AND_EXIT)
    _nc_free_and_exit(status);
#elif defined(HAVE__NC_FREEALL)
    _nc_freeall();
#endif
#endif /* NCURSES_VERSION */

    exit(status);
}

void outofmem(const char *fname,
	      const char *func)
{
    fprintf(stderr, "\n\n\n%s %s: %s\n", fname, func, MEMORY_EXHAUSTED_ABORTING);
    LYOutOfMemory = TRUE;
    LYexit(-1);
}

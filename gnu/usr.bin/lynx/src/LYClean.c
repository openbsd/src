/* $LynxId: LYClean.c,v 1.38 2008/02/11 00:50:19 Paul.B.Mahol Exp $ */
#include <HTUtils.h>
#include <LYCurses.h>
#include <LYUtils.h>
#include <LYSignal.h>
#include <LYClean.h>
#include <LYMainLoop.h>
#include <LYGlobalDefs.h>
#include <LYTraversal.h>
#include <LYHistory.h>
#include <LYCookie.h>
#include <LYSession.h>
#include <UCAuto.h>
#include <HTAlert.h>

#include <LYexit.h>
#include <LYLeaks.h>

#ifdef DJGPP
extern void sig_handler_watt(int);
#endif /* DJGPP */

#ifdef VMS
BOOLEAN HadVMSInterrupt = FALSE;
#endif /* VMS */

/*
 * Interrupt handler.  Stop curses and exit gracefully.
 */
void cleanup_sig(int sig)
{
#ifdef IGNORE_CTRL_C
    if (sig == SIGINT) {
	/*
	 * Need to rearm the signal.
	 */
#ifdef DJGPP
	if (wathndlcbrk) {
	    sig_handler_watt(sig);	/* Use WATT-32 signal handler */
	}
#endif /* DJGPP */
	signal(SIGINT, cleanup_sig);
	sigint = TRUE;
#ifdef DJGPP
	_eth_release();
	_eth_init();
#endif /* DJGPP */
	return;
    }
#endif /* IGNORE_CTRL_C */

#ifdef VMS
    if (!dump_output_immediately) {
	int c;

	/*
	 * Reassert the AST.
	 */
	(void) signal(SIGINT, cleanup_sig);
	if (!LYCursesON)
	    return;

	/*
	 * Refresh screen to get rid of "cancel" message, then query.
	 */
	lynx_force_repaint();
	LYrefresh();

	/*
	 * Ask if exit is intended.
	 */
	if (LYQuitDefaultYes == TRUE) {
	    c = HTConfirmDefault(REALLY_EXIT, YES);
	} else {
	    c = HTConfirmDefault(REALLY_EXIT, NO);
	}
	HadVMSInterrupt = TRUE;
	if (LYQuitDefaultYes == TRUE) {
	    if (c == NO) {
		return;
	    }
	} else if (c != YES) {
	    return;
	}
    }
#endif /* VMS */

    /*
     * Ignore further interrupts.  - mhc:  11/2/91
     */
#ifndef NOSIGHUP
    (void) signal(SIGHUP, SIG_IGN);
#endif /* NOSIGHUP */

#ifdef VMS
    /*
     * Use ttclose() from cleanup() for VMS if not dumping.
     */
    if (dump_output_immediately)
#else /* Unix: */
    (void) signal(SIGINT, SIG_IGN);
#endif /* VMS */

    (void) signal(SIGTERM, SIG_IGN);

    if (traversal)
	dump_traversal_history();

#ifndef NOSIGHUP
    if (sig != SIGHUP) {
#endif /* NOSIGHUP */

	if (!dump_output_immediately) {
	    /*
	     * cleanup() also calls cleanup_files().
	     */
	    cleanup();
	}
	if (sig != 0) {
	    SetOutputMode(O_TEXT);
	    printf("\n\n%s %d\n\n",
		   gettext("Exiting via interrupt:"),
		   sig);
	    fflush(stdout);
	}
#ifndef NOSIGHUP
    } else {
#ifdef USE_SESSIONS
	/*
	 * Wondering is this right place and time to do it.
	 * We need this, for example it is usefull to save session
	 * if user closed lynx in non standard way, like closing
	 * xterm window or in worst one like crash.
	 */
	SaveSession();
#endif /* USE_SESSIONS */
	cleanup_files();
    }
#endif /* NOSIGHUP */
    if (sig != 0) {
	exit_immediately(EXIT_SUCCESS);
    } else {
	reset_signals();
    }
}

/*
 * Called by Interrupt handler or at quit time.  Erases the temporary files
 * that lynx created.
 */
void cleanup_files(void)
{
    LYCleanupTemp();
    FREE(lynx_temp_space);
}

void cleanup(void)
{
#ifdef VMS
    extern BOOLEAN DidCleanup;
#endif /* VMS */

    /*
     * Cleanup signals - just in case.  Ignore further interrupts.  - mhc: 
     * 11/2/91
     */
#ifndef NOSIGHUP
    (void) signal(SIGHUP, SIG_IGN);
#endif /* NOSIGHUP */
    (void) signal(SIGTERM, SIG_IGN);

#ifndef VMS			/* use ttclose() from cleanup() for VMS */
    (void) signal(SIGINT, SIG_IGN);
#endif /* !VMS */

    if (LYCursesON) {
	LYmove(LYlines - 1, 0);
	LYclrtoeol();

	lynx_stop_all_colors();
	LYrefresh();

	stop_curses();
    }
#ifdef EXP_CHARTRANS_AUTOSWITCH
    /*
     * Currently implemented only for LINUX:  Restore original font.
     */
    UCChangeTerminalCodepage(-1, (LYUCcharset *) 0);
#endif /* EXP_CHARTRANS_AUTOSWITCH */

#ifdef USE_PERSISTENT_COOKIES
    /*
     * This can go right here for now.  We need to work up a better place
     * to save cookies for the next release, preferably whenever a new
     * persistent cookie is received or used.  Some sort of protocol to
     * handle two processes writing to the cookie file needs to be worked
     * out as well.
     */
    if (persistent_cookies)
	LYStoreCookies(LYCookieSaveFile);
#endif
#ifdef USE_SESSIONS
    /*
     * Wondering is this right place and time to do it.
     */
    SaveSession();
#endif /* USE_SESSIONS */

    cleanup_files();
#ifdef VMS
    ttclose();
    DidCleanup = TRUE;
#endif /* VMS */

    /*
     * If we're looking at memory leaks, hang onto the trace file, since there
     * is no memory freed in this function, and it is a nuisance to not be able
     * to trace the cleanup activity -TD
     */
#ifndef LY_FIND_LEAKS
    LYCloseTracelog();
#endif
}

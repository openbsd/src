#include "HTUtils.h"
#include "tcp.h"
#include "LYCurses.h"
#include "LYUtils.h"
#include "LYSignal.h"
#include "LYClean.h"
#include "LYGlobalDefs.h"
#include "LYStrings.h"
#include "LYTraversal.h"
#include "UCAuto.h"

#include "LYexit.h"
#include "LYLeaks.h"

#define FREE(x) if (x) {free(x); x = NULL;}

#ifdef VMS
BOOLEAN HadVMSInterrupt = FALSE;
#endif /* VMS */

/*
 *  Interrupt handler.	Stop curses and exit gracefully.
 */
PUBLIC void cleanup_sig ARGS1(
	int,		sig)
{

#ifdef IGNORE_CTRL_C
    if (sig == SIGINT)	{
    /*
     *	Need to rearm the signal.
     */
    signal(SIGINT, cleanup_sig);
    sigint = TRUE;
    return;
    }
#endif /* IGNORE_CTRL_C */

#ifdef VMS
    if (!dump_output_immediately) {
	int c;

	/*
	 *  Reassert the AST.
	 */
	(void) signal(SIGINT, cleanup_sig);
	HadVMSInterrupt = TRUE;
	if (!LYCursesON)
	    return;

	/*
	 *  Refresh screen to get rid of "cancel" message, then query.
	 */
	lynx_force_repaint();
	refresh();

	/*
	 *  Ask if exit is intended.
	 */
	if (LYQuitDefaultYes == TRUE) {
	    _statusline(REALLY_EXIT_Y);
	} else {
	    _statusline(REALLY_EXIT_N);
	}
	c = LYgetch();
	if (LYQuitDefaultYes == TRUE) {
	    if (TOUPPER(c) == 'N' ||
		c == 7) {
		return;
	    }
	} else if (TOUPPER(c) != 'Y') {
	    return;
	}
    }
#endif /* VMS */

    /*
     *	Ignore further interrupts. - mhc: 11/2/91
     */
#ifndef NOSIGHUP
    (void) signal(SIGHUP, SIG_IGN);
#endif /* NOSIGHUP */

#ifdef VMS
    /*
     *	Use ttclose() from cleanup() for VMS if not dumping.
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
	     *	cleanup() also calls cleanup_files().
	     */
	    cleanup();
	}
	if (sig != 0) {
	    printf("\r\nExiting via interrupt: %d\r\n", sig);
	    fflush(stdout);
	}
#ifndef NOSIGHUP
    } else {
	cleanup_files();
    }
#endif /* NOSIGHUP */

#ifndef NOSIGHUP
	 (void) signal(SIGHUP, SIG_DFL);
#endif /* NOSIGHUP */
    (void) signal(SIGTERM, SIG_DFL);
#ifndef VMS
    (void) signal(SIGINT, SIG_DFL);
#endif /* !VMS */
#ifdef SIGTSTP
    if (no_suspend)
	(void) signal(SIGTSTP, SIG_DFL);
#endif /* SIGTSTP */
    if (sig != 0) {
	exit(0);
    }
}

/*
 *  Called by Interrupt handler or at quit time.
 *  Erases the temporary files that lynx created
 *  temporary files are removed by tempname
 *  which created them.
 */
PUBLIC void cleanup_files NOARGS
{
    char filename[256];

    tempname(filename, REMOVE_FILES);

    if (rmdir(lynx_temp_space))
	perror("Could not remove the temp-directory");

    FREE(lynx_temp_space);
}

PUBLIC void cleanup NOARGS
{
    int i;
#ifdef VMS
    extern BOOLEAN DidCleanup;
#endif /* VMS */

    /*
     *	Cleanup signals - just in case.
     *	Ignore further interrupts. - mhc: 11/2/91
     */
#ifndef NOSIGHUP
    (void) signal(SIGHUP, SIG_IGN);
#endif /* NOSIGHUP */
    (void) signal (SIGTERM, SIG_IGN);

#ifndef VMS  /* use ttclose() from cleanup() for VMS */
    (void) signal (SIGINT, SIG_IGN);
#endif /* !VMS */

    if (LYCursesON) {
	move(LYlines-1, 0);
	clrtoeol();

	lynx_stop_all_colors ();
	refresh();

	stop_curses();
    }

#ifdef EXP_CHARTRANS_AUTOSWITCH
#ifdef LINUX
    /*
     *	Currently implemented only for LINUX: Restore original font.
     */
    UCChangeTerminalCodepage(-1, (LYUCcharset*)0);
#endif /* LINUX */
#endif /* EXP_CHARTRANS_AUTOSWITCH */

    cleanup_files();
    for (i = 0; i < nhist; i++) {
	FREE(history[i].title);
	FREE(history[i].address);
	FREE(history[i].post_data);
	FREE(history[i].post_content_type);
	FREE(history[i].bookmark);
    }
    nhist = 0;
#ifdef VMS
    ttclose();
    DidCleanup = TRUE;
#endif /* VMS */

    fflush(stdout);
    fflush(stderr);
    if (LYTraceLogFP != NULL) {
	fclose(LYTraceLogFP);
	LYTraceLogFP = NULL;
#if !defined(VMS) || (defined(VMS) && !defined(VAXC) && !defined(UCX))
	*stderr = LYOrigStderr;
#endif /* !VMS || (VMS && !VAXC && !UCX) */
    }
}

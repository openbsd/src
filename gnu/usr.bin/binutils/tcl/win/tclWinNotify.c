/* 
 * tclWinNotify.c --
 *
 *	This file contains Windows-specific procedures for the notifier,
 *	which is the lowest-level part of the Tcl event loop.  This file
 *	works together with ../generic/tclNotify.c.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinNotify.c 1.8 96/02/29 09:19:08
 */

#include "tclInt.h"
#include "tclPort.h"
#include <winsock.h>

/*
 * The following variable is a backdoor for use by Tk.  It is set when
 * Tk needs to process events on the Tcl event queue without reentering
 * the system event loop.  Tk uses it to flush the Tcl event queue.
 */

static int ignoreEvents = 0;

/*
 *----------------------------------------------------------------------
 *
 * TclWinFlushEvents --
 *
 *	This function is a special purpose hack to allow Tk to
 *	process queued Window events during a recursive event loop
 *	without looking for new events on the system event queue.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Services any pending Tcl events and calls idle handlers.
 *
 *----------------------------------------------------------------------
 */

void
TclWinFlushEvents()
{
    ignoreEvents = 1;
    while (Tcl_DoOneEvent(TCL_DONT_WAIT|TCL_WINDOW_EVENTS|TCL_IDLE_EVENTS)) {
    }
    ignoreEvents = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WatchFile --
 *
 *	Arrange for Tcl_DoOneEvent to include this file in the masks
 *	for the next call to select.  This procedure is invoked by
 *	event sources, which are in turn invoked by Tcl_DoOneEvent
 *	before it invokes select.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	
 *	The notifier will generate a file event when the I/O channel
 *	given by fd next becomes ready in the way indicated by mask.
 *	If fd is already registered then the old mask will be replaced
 *	with the new one.  Once the event is sent, the notifier will
 *	not send any more events about the fd until the next call to
 *	Tcl_NotifyFile. 
 *
 *----------------------------------------------------------------------
 */

void
Tcl_WatchFile(file, mask)
    Tcl_File file;		/* Opaque identifier for a stream. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions to wait for
				 * in select. */
{
    int type;

    (void) Tcl_GetFileInfo(file, &type);

    if (type == TCL_WIN_SOCKET) {
	TclWinWatchSocket(file, mask);
    } else if (type == TCL_WIN_FILE) {
	Tcl_Time timeout = { 0, 0 };

	/*
	 * Files are always ready under Windows, so we just set a
	 * 0 timeout.
	 */

	Tcl_SetMaxBlockTime(&timeout);
    } else if (type == TCL_WIN_PIPE) {
	/*
	 * We don't support waiting on pipes yet.
	 */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FileReady --
 *
 *	Indicates what conditions (readable, writable, etc.) were
 *	present on a file the last time the notifier invoked select.
 *	This procedure is typically invoked by event sources to see
 *	if they should queue events.
 *
 * Results:
 *	The return value is 0 if none of the conditions specified by mask
 *	was true for fd the last time the system checked.  If any of the
 *	conditions were true, then the return value is a mask of those
 *	that were true.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_FileReady(file, mask)
    Tcl_File file;	/* File handle for a stream. */
    int mask;			/* OR'ed combination of TCL_READABLE,
				 * TCL_WRITABLE, and TCL_EXCEPTION:
				 * indicates conditions caller cares about. */
{
    int type;

    (void) Tcl_GetFileInfo(file, &type);

    if (type == TCL_WIN_SOCKET) {
	return TclWinSocketReady(file, mask);
    } else if (type == TCL_WIN_FILE) {
	/*
	 * Under Windows, files are always ready, so we just return the
	 * mask that was passed in.
	 */

	return mask;
    }
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitForEvent --
 *
 *	This procedure does the lowest level wait for events in a
 *	platform-specific manner.  It uses information provided by
 *	previous calls to Tcl_WatchFile, plus the timePtr argument,
 *	to determine what to wait for and how long to wait.
 *
 * Results:
 *	The return value is normally TCL_OK.  However, if there are
 *	no events to wait for (e.g. no files and no timers) so that
 *	the procedure would block forever, then it returns TCL_ERROR.
 *
 * Side effects:
 *	May put the process to sleep for a while, depending on timePtr.
 *	When this procedure returns, an event of interest to the application
 *	has probably, but not necessarily, occurred.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitForEvent(timePtr)
    Tcl_Time *timePtr;		/* Specifies the maximum amount of time
				 * that this procedure should block before
				 * returning.  The time is given as an
				 * interval, not an absolute wakeup time.
				 * NULL means block forever. */
{
    MSG msg;
    int foundEvent = 1;

    /*
     * If we are ignoring events from the system, just return immediately.
     */

    if (ignoreEvents) {
	return TCL_OK;
    }

    /*
     * Set up the asynchronous select handlers for any sockets we
     * are watching.
     */

    TclWinNotifySocket();

    /*
     * Look for an event, setting a timer so we don't block forever.
     */

    if (timePtr != NULL) {
	UINT ms;
	ms = timePtr->sec * 1000;
	ms += timePtr->usec / 1000;

	if (ms > 0) {
	    UINT timerHandle = SetTimer(NULL, 0, ms, NULL);
	    GetMessage(&msg, NULL, 0, 0);
	    KillTimer(NULL, timerHandle);
	} else {

	    /*
	     * If the timeout is too small, we just poll.
	     */

	    foundEvent = PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
	}
    } else {
	GetMessage(&msg, NULL, 0, 0);
    }

    /*
     * Dispatch the message, if we found one.  If we are exiting, be
     * sure to inform Tcl so we can clean up properly.
     */

    if (foundEvent) {
	if (msg.message == WM_QUIT) {
	    Tcl_Exit(0);
	}
	TranslateMessage(&msg);
	DispatchMessage(&msg);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_Sleep --
 *
 *	Delay execution for the specified number of milliseconds.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Time passes.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_Sleep(ms)
    int ms;			/* Number of milliseconds to sleep. */
{
    Sleep(ms);
}

/* 
 * tkUnixEvent.c --
 *
 *	This file implements an event source for X displays for the
 *	UNIX version of Tk.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUnixEvent.c 1.5 96/02/15 18:55:30
 */

#include "tkInt.h"
#include "tkUnixInt.h"
#include <signal.h>

/*
 * Prototypes for procedures that are referenced only in this file:
 */

static void		DisplayCheckProc _ANSI_ARGS_((ClientData clientData,
			    int flags));
static void		DisplaySetupProc _ANSI_ARGS_((ClientData clientData,
			    int flags));

/*
 *----------------------------------------------------------------------
 *
 * DisplaySetupProc --
 *
 *	This procedure is part of the event source for UNIX X displays.
 *	It is invoked by Tcl_DoOneEvent before it calls select to check
 *	for events on all displays.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tells the notifier which files should be waited for.
 *
 *----------------------------------------------------------------------
 */

static void
DisplaySetupProc(clientData, flags)
    ClientData clientData;		/* Not used. */
    int flags;				/* Flags passed to Tk_DoOneEvent:
					 * if it doesn't include
					 * TCL_WINDOW_EVENTS then we do
					 * nothing. */
{
    TkDisplay *dispPtr;
    static Tcl_Time dontBlock = {0, 0};

    if (!(flags & TCL_WINDOW_EVENTS)) {
	return;
    }

    for (dispPtr = tkDisplayList; dispPtr != NULL; dispPtr = dispPtr->nextPtr) {
	Tcl_File handle;
	XFlush(dispPtr->display);
	if (XQLength(dispPtr->display) > 0) {
	    Tcl_SetMaxBlockTime(&dontBlock);
	}
	handle = Tcl_GetFile(
	    (ClientData)ConnectionNumber(dispPtr->display), TCL_UNIX_FD);
	Tcl_WatchFile(handle, TCL_READABLE);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * DisplayCheckProc --
 *
 *	This procedure is the second part of the "event source" for
 *	X displays.  It is invoked by Tcl_DoOneEvent after it calls
 *	select (or whatever it uses to wait for events).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Makes entries on the Tcl event queue for all the events available
 *	from all the displays.
 *
 *----------------------------------------------------------------------
 */

static void
DisplayCheckProc(clientData, flags)
    ClientData clientData;		/* Not used. */
    int flags;				/* Flags passed to Tk_DoOneEvent:
					 * if it doesn't include 
					 * TCL_WINDOW_EVENTS then we do
					 * nothing. */
{
    TkDisplay *dispPtr;
    XEvent event;
    int numFound;

    if (!(flags & TCL_WINDOW_EVENTS)) {
	return;
    }

    for (dispPtr = tkDisplayList; dispPtr != NULL; dispPtr = dispPtr->nextPtr) {
	Tcl_File handle;
	/*
	 * Note: we should not need to do a flush of the output queues before
	 * calling XEventsQueued because it was done by DisplaySetupProc.
	 */

	handle = Tcl_GetFile(
	    (ClientData) ConnectionNumber(dispPtr->display), TCL_UNIX_FD);
	if (Tcl_FileReady(handle, TCL_READABLE) != 0) {
	    numFound = XEventsQueued(dispPtr->display, QueuedAfterReading);
	    if (numFound == 0) {
	
		/*
		 * Things are very tricky if there aren't any events readable
		 * at this point (after all, there was supposedly data
		 * available on the connection).  A couple of things could
		 * have occurred:
		 * 
		 * One possibility is that there were only error events in the
		 * input from the server.  If this happens, we should return
		 * (we don't want to go to sleep in XNextEvent below, since
		 * this would block out other sources of input to the
		 * process).
		 *
		 * Another possibility is that our connection to the server
		 * has been closed.  This will not necessarily be detected in
		 * XEventsQueued (!!), so if we just return then there will be
		 * an infinite loop.  To detect such an error, generate a NoOp
		 * protocol request to exercise the connection to the server,
		 * then return.  However, must disable SIGPIPE while sending
		 * the request, or else the process will die from the signal
		 * and won't invoke the X error function to print a nice (?!)
		 * message.
		 */
	
		void (*oldHandler)();
	
		oldHandler = (void (*)()) signal(SIGPIPE, SIG_IGN);
		XNoOp(dispPtr->display);
		XFlush(dispPtr->display);
		(void) signal(SIGPIPE, oldHandler);
	    }
	} else {
	    numFound = XQLength(dispPtr->display);
	}
    
	/*
	 * Transfer events from the X event queue to the Tk event queue.
	 */

	while (numFound > 0) {
	    XNextEvent(dispPtr->display, &event);
	    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
	    numFound--;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateXEventSource --
 *
 *	This procedure is called during Tk initialization to create
 *	the event source for X Window events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new event source is created.
 *
 *----------------------------------------------------------------------
 */

void
TkCreateXEventSource()
{
    static int initialized = 0;

    if (!initialized) {
	Tcl_CreateEventSource(DisplaySetupProc, DisplayCheckProc,
		(ClientData) NULL);
	initialized = 1;
    }
}

/* 
 * tkXId.c --
 *
 *	This file provides a replacement function for the default X
 *	resource allocator (_XAllocID).  The problem with the default
 *	allocator is that it never re-uses ids, which causes long-lived
 *	applications to crash when X resource identifiers wrap around.
 *	The replacement functions in this file re-use old identifiers
 *	to prevent this problem.
 *
 *	The code in this file is based on similar implementations by
 *	George C. Kaplan and Michael Hoegeman.
 *
 * Copyright (c) 1993 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkXId.c 1.16 96/02/28 21:56:40
 */

/*
 * The definition below is needed on some systems so that we can access
 * the resource_alloc field of Display structures in order to replace
 * the resource allocator.
 */

#define XLIB_ILLEGAL_ACCESS 1

#include "tkInt.h"
#include "tkPort.h"

/*
 * A structure of the following type is used to hold one or more
 * available resource identifiers.  There is a list of these structures
 * for each display.
 */

#define IDS_PER_STACK 10
typedef struct TkIdStack {
    XID ids[IDS_PER_STACK];		/* Array of free identifiers. */
    int numUsed;			/* Indicates how many of the entries
					 * in ids are currently in use. */
    TkDisplay *dispPtr;			/* Display to which ids belong. */
    struct TkIdStack *nextPtr;		/* Next bunch of free identifiers
					 * for the same display. */
} TkIdStack;

/*
 * Forward declarations for procedures defined in this file:
 */

static XID		AllocXId _ANSI_ARGS_((Display *display));
static Tk_RestrictAction CheckRestrictProc _ANSI_ARGS_((
			    ClientData clientData, XEvent *eventPtr));
static void		WindowIdCleanup _ANSI_ARGS_((ClientData clientData));
static void		WindowIdCleanup2 _ANSI_ARGS_((ClientData clientData));

/*
 *----------------------------------------------------------------------
 *
 * TkInitXId --
 *
 *	This procedure is called to initialize the id allocator for
 *	a given display.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The official allocator for the display is set up to be Tk_AllocXID.
 *
 *----------------------------------------------------------------------
 */

void
TkInitXId(dispPtr)
    TkDisplay *dispPtr;			/* Tk's information about the
					 * display. */
{
    dispPtr->idStackPtr = NULL;
    dispPtr->defaultAllocProc = dispPtr->display->resource_alloc;
    dispPtr->display->resource_alloc = AllocXId;
    dispPtr->windowStackPtr = NULL;
    dispPtr->idCleanupScheduled = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * AllocXId --
 *
 *	This procedure is invoked by Xlib as the resource allocator
 *	for a display.
 *
 * Results:
 *	The return value is an X resource identifier that isn't currently
 *	in use.
 *
 * Side effects:
 *	The identifier is removed from the stack of free identifiers,
 *	if it was previously on the stack.
 *
 *----------------------------------------------------------------------
 */

static XID
AllocXId(display)
    Display *display;			/* Display for which to allocate. */
{
    TkDisplay *dispPtr;
    TkIdStack *stackPtr;

    /*
     * Find Tk's information about the display.
     */

    dispPtr = TkGetDisplay(display);
    
    /*
     * If the topmost chunk on the stack is empty then free it.  Then
     * check for a free id on the stack and return it if it exists.
     */

    stackPtr = dispPtr->idStackPtr;
    if (stackPtr != NULL) {
	while (stackPtr->numUsed == 0) {
	    dispPtr->idStackPtr = stackPtr->nextPtr;
	    ckfree((char *) stackPtr);
	    stackPtr = dispPtr->idStackPtr;
	    if (stackPtr == NULL) {
		goto defAlloc;
	    }
	}
	stackPtr->numUsed--;
	return stackPtr->ids[stackPtr->numUsed];
    }

    /*
     * No free ids in the stack:  just get one from the default
     * allocator.
     */

    defAlloc:
    return (*dispPtr->defaultAllocProc)(display);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FreeXId --
 *
 *	This procedure is called to indicate that an X resource identifier
 *	is now free.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The identifier is added to the stack of free identifiers for its
 *	display, so that it can be re-used.
 *
 *----------------------------------------------------------------------
 */

void
Tk_FreeXId(display, xid)
    Display *display;			/* Display for which xid was
					 * allocated. */
    XID xid;				/* Identifier that is no longer
					 * in use. */
{
    TkDisplay *dispPtr;
    TkIdStack *stackPtr;

    /*
     * Find Tk's information about the display.
     */

    dispPtr = TkGetDisplay(display);

    /*
     * Add a new chunk to the stack if the current chunk is full.
     */
    
    stackPtr = dispPtr->idStackPtr;
    if ((stackPtr == NULL) || (stackPtr->numUsed >= IDS_PER_STACK)) {
	stackPtr = (TkIdStack *) ckalloc(sizeof(TkIdStack));
	stackPtr->numUsed = 0;
	stackPtr->dispPtr = dispPtr;
	stackPtr->nextPtr = dispPtr->idStackPtr;
	dispPtr->idStackPtr = stackPtr;
    }

    /*
     * Add the id to the current chunk.
     */

    stackPtr->ids[stackPtr->numUsed] = xid;
    stackPtr->numUsed++;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFreeWindowId --
 *
 *	This procedure is invoked instead of TkFreeXId for window ids.
 *	See below for the reason why.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The id given by w will eventually be freed, so that it can be
 *	reused for other resources.
 *
 * Design:
 *	Freeing window ids is very tricky because there could still be
 *	events pending for a window in the event queue (or even in the
 *	server) at the time the window is destroyed.  If the window
 *	id were to get reused immediately for another window, old
 *	events could "drop in" on the new window, causing unexpected
 *	behavior.
 *
 *	Thus we have to wait to re-use a window id until we know that
 *	there are no events left for it.  Right now this is done in
 *	two steps.  First, we wait until we know that the server
 *	has seen the XDestroyWindow request, so we can be sure that
 *	it won't generate more events for the window and that any
 *	existing events are in our queue.  Second, we make sure that
 *	there are no events whatsoever in our queue (this is conservative
 *	but safe).
 *
 * 	The first step is done by remembering the request id of the
 *	XDestroyWindow request and using LastKnownRequestProcessed to
 *	see what events the server has processed.  If multiple windows
 *	get destroyed at about the same time, we just remember the
 *	most recent request number for any of them (again, conservative
 *	but safe).
 *
 *	There are a few other complications as well.  When Tk destroys a
 *	sub-tree of windows, it only issues a single XDestroyWindow call,
 *	at the very end for the root of the subtree.  We can't free any of
 *	the window ids until the final XDestroyWindow call.  To make sure
 *	that this happens, we have to keep track of deletions in progress,
 *	hence the need for the "destroyCount" field of the display.
 *
 *	One final problem.  Some servers, like Sun X11/News servers still
 *	seem to have problems with ids getting reused too quickly.  I'm
 *	not completely sure why this is a problem, but delaying the
 *	recycling of ids appears to eliminate it.  Therefore, we wait
 *	an additional few seconds, even after "the coast is clear"
 *	before reusing the ids.
 *
 *----------------------------------------------------------------------
 */

void
TkFreeWindowId(dispPtr, w)
    TkDisplay *dispPtr;		/* Display that w belongs to. */
    Window w;			/* X identifier for window on dispPtr. */
{
    TkIdStack *stackPtr;

    /*
     * Put the window id on a separate stack of window ids, rather
     * than the main stack, so it won't get reused right away.  Add
     * a new chunk to the stack if the current chunk is full.
     */

    stackPtr = dispPtr->windowStackPtr;
    if ((stackPtr == NULL) || (stackPtr->numUsed >= IDS_PER_STACK)) {
	stackPtr = (TkIdStack *) ckalloc(sizeof(TkIdStack));
	stackPtr->numUsed = 0;
	stackPtr->dispPtr = dispPtr;
	stackPtr->nextPtr = dispPtr->windowStackPtr;
	dispPtr->windowStackPtr = stackPtr;
    }

    /*
     * Add the id to the current chunk.
     */

    stackPtr->ids[stackPtr->numUsed] = w;
    stackPtr->numUsed++;

    /*
     * Schedule a call to WindowIdCleanup if one isn't already
     * scheduled.
     */

    if (!dispPtr->idCleanupScheduled) {
	dispPtr->idCleanupScheduled = 1;
	Tcl_CreateTimerHandler(100, WindowIdCleanup, (ClientData *) dispPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * WindowIdCleanup --
 *
 *	See if we can now free up all the accumulated ids of
 *	deleted windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If it's safe to move the window ids back to the main free
 *	list, we schedule this to happen after a few mores seconds
 *	of delay.  If it's not safe to move them yet, a timer handler
 *	gets invoked to try again later.
 *
 *----------------------------------------------------------------------
 */

static void
WindowIdCleanup(clientData)
    ClientData clientData;	/* Pointer to TkDisplay for display */
{
    TkDisplay *dispPtr = (TkDisplay *) clientData;
    int anyEvents, delta;
    Tk_RestrictProc *oldProc;
    ClientData oldData;

    dispPtr->idCleanupScheduled = 0;

    /*
     * See if it's safe to recycle the window ids.  It's safe if:
     * (a) no deletions are in progress.
     * (b) the server has seen all of the requests up to the last
     *     XDestroyWindow request.
     * (c) there are no events in the event queue; the only way to
     *     test for this right now is to create a restrict proc that
     *     will filter the events, then call Tcl_DoOneEvent to see if
     *	   the procedure gets invoked.
     */

    if (dispPtr->destroyCount > 0) {
	goto tryAgain;
    }
    delta = LastKnownRequestProcessed(dispPtr->display)
	    - dispPtr->lastDestroyRequest;
    if (delta < 0) {
	XSync(dispPtr->display, False);
    }
    anyEvents = 0;
    oldProc = Tk_RestrictEvents(CheckRestrictProc, (ClientData) &anyEvents,
	    &oldData);
    Tcl_DoOneEvent(TCL_DONT_WAIT|TCL_WINDOW_EVENTS);
    Tk_RestrictEvents(oldProc, oldData, &oldData);
    if (anyEvents) {
	goto tryAgain;
    }

    /*
     * These ids look safe to recycle, but we still need to delay a bit
     * more (see comments for TkFreeWindowId).  Schedule the final freeing.
     */

    if (dispPtr->windowStackPtr != NULL) {
	Tcl_CreateTimerHandler(5000, WindowIdCleanup2,
		(ClientData) dispPtr->windowStackPtr);
	dispPtr->windowStackPtr = NULL;
    }
    return;

    /*
     * It's still not safe to free up the ids.  Try again a bit later.
     */

    tryAgain:
    dispPtr->idCleanupScheduled = 1;
    Tcl_CreateTimerHandler(500, WindowIdCleanup, (ClientData *) dispPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * WindowIdCleanup2 --
 *
 *	This procedure is the last one in the chain that recycles
 *	window ids.  It takes all of the ids indicated by its
 *	argument and adds them back to the main id free list.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Window ids get added to the main free list for their display.
 *
 *----------------------------------------------------------------------
 */

static void
WindowIdCleanup2(clientData)
    ClientData clientData;	/* Pointer to TkIdStack list. */
{
    TkIdStack *stackPtr = (TkIdStack *) clientData;
    TkIdStack *lastPtr;

    lastPtr = stackPtr;
    while (lastPtr->nextPtr != NULL) {
	lastPtr = lastPtr->nextPtr;
    }
    lastPtr->nextPtr = stackPtr->dispPtr->idStackPtr;
    stackPtr->dispPtr->idStackPtr = stackPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * CheckRestrictProc --
 *
 *	This procedure is a restrict procedure, called by Tcl_DoOneEvent
 *	to filter X events.  All it does is to set a flag to indicate
 *	that there are X events present.
 *
 * Results:
 *	Sets the integer pointed to by the argument, then returns
 *	TK_DEFER_EVENT.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tk_RestrictAction
CheckRestrictProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to flag to set. */
    XEvent *eventPtr;		/* Event to filter;  not used. */
{
    int *flag = (int *) clientData;
    *flag = 1;
    return TK_DEFER_EVENT;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetPixmap --
 *
 *	Same as the XCreatePixmap procedure except that it manages
 *	resource identifiers better.
 *
 * Results:
 *	Returns a new pixmap.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Pixmap
Tk_GetPixmap(display, d, width, height, depth)
    Display *display;		/* Display for new pixmap. */
    Drawable d;			/* Drawable where pixmap will be used. */
    int width, height;		/* Dimensions of pixmap. */
    int depth;			/* Bits per pixel for pixmap. */
{
    return XCreatePixmap(display, d, (unsigned) width, (unsigned) height,
	    (unsigned) depth);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_FreePixmap --
 *
 *	Same as the XFreePixmap procedure except that it also marks
 *	the resource identifier as free.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The pixmap is freed in the X server and its resource identifier
 *	is saved for re-use.
 *
 *----------------------------------------------------------------------
 */

void
Tk_FreePixmap(display, pixmap)
    Display *display;		/* Display for which pixmap was allocated. */
    Pixmap pixmap;		/* Identifier for pixmap. */
{
    XFreePixmap(display, pixmap);
    Tk_FreeXId(display, (XID) pixmap);
}

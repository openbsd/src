/* 
 * tkUnixDraw.c --
 *
 *	This file contains X specific drawing routines.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUnixDraw.c 1.7 96/02/15 18:55:26
 */

#include "tkPort.h"
#include "tkInt.h"

/*
 * The following structure is used to pass information to
 * ScrollRestrictProc from TkScrollWindow.
 */

typedef struct ScrollInfo {
    int done;			/* Flag is 0 until filtering is done. */
    Display *display;		/* Display to filter. */
    Window window;		/* Window to filter. */
    TkRegion region;		/* Region into which damage is accumulated. */
    int dx, dy;			/* Amount by which window was shifted. */
} ScrollInfo;

/*
 * Forward declarations for procedures declared later in this file:
 */

static Tk_RestrictAction	ScrollRestrictProc _ANSI_ARGS_((
    				    ClientData arg, XEvent *eventPtr));

/*
 *----------------------------------------------------------------------
 *
 * TkScrollWindow --
 *
 *	Scroll a rectangle of the specified window and accumulate
 *	damage information in the specified Region.
 *
 * Results:
 *	Returns 0 if no damage additional damage was generated.  Sets
 *	damageRgn to contain the damaged areas and returns 1 if
 *	GraphicsExpose events were detected.
 *
 * Side effects:
 *	Scrolls the bits in the window and enters the event loop
 *	looking for damage events.
 *
 *----------------------------------------------------------------------
 */

int
TkScrollWindow(tkwin, gc, x, y, width, height, dx, dy, damageRgn)
    Tk_Window tkwin;		/* The window to be scrolled. */
    GC gc;			/* GC for window to be scrolled. */
    int x, y, width, height;	/* Position rectangle to be scrolled. */
    int dx, dy;			/* Distance rectangle should be moved. */
    TkRegion damageRgn;		/* Region to accumulate damage in. */
{
    Tk_RestrictProc *oldProc;
    ClientData oldArg, dummy;
    ScrollInfo info;
    
    XCopyArea(Tk_Display(tkwin), Tk_WindowId(tkwin), Tk_WindowId(tkwin), gc,
	    x, y, (unsigned int) width, (unsigned int) height, x + dx, y + dy);

    info.done = 0;
    info.window = Tk_WindowId(tkwin);
    info.display = Tk_Display(tkwin);
    info.region = damageRgn;
    info.dx = dx;
    info.dy = dy;

    /*
     * Sync the event stream so all of the expose events will be on the
     * X event queue before we start filtering.  This avoids busy waiting
     * while we filter events.
     */

    XSync(info.display, False);
    oldProc = Tk_RestrictEvents(ScrollRestrictProc, (ClientData) &info,
	    &oldArg);
    while (!info.done) {
	Tcl_DoOneEvent(TCL_WINDOW_EVENTS|TCL_DONT_WAIT);
    }
    Tk_RestrictEvents(oldProc, oldArg, &dummy);

    return XEmptyRegion((Region) damageRgn) ? 0 : 1;
}

/*
 *----------------------------------------------------------------------
 *
 * ScrollRestrictProc --
 *
 *	A Tk_RestrictProc used by TkScrollWindow to gather up Expose
 *	information into a single damage region.  It accumulates damage
 *	events on the specified window until a NoExpose or the last
 *	GraphicsExpose event is detected.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Discards Expose events after accumulating damage information
 *	for a particular window.
 *
 *----------------------------------------------------------------------
 */

static Tk_RestrictAction
ScrollRestrictProc(arg, eventPtr)
    ClientData arg;
    XEvent *eventPtr;
{
    ScrollInfo *info = (ScrollInfo *) arg;
    XRectangle rect;

    /*
     * Defer events which aren't for the specified window.
     */

    if (info->done || (eventPtr->xany.display != info->display)
	    || (eventPtr->xany.window != info->window)) {
	return TK_DEFER_EVENT;
    }

    if (eventPtr->type == NoExpose) {
	info->done = 1;
    } else if (eventPtr->type == GraphicsExpose) {
	rect.x = eventPtr->xgraphicsexpose.x;
	rect.y = eventPtr->xgraphicsexpose.y;
	rect.width = eventPtr->xgraphicsexpose.width;
	rect.height = eventPtr->xgraphicsexpose.height;
	XUnionRectWithRegion(&rect, (Region) info->region,
		(Region) info->region);

	if (eventPtr->xgraphicsexpose.count == 0) {
	    info->done = 1;
	}
    } else if (eventPtr->type == Expose) {

	/*
	 * This case is tricky.  This event was already queued before
	 * the XCopyArea was issued.  If this area overlaps the area
	 * being copied, then some of the copied area may be invalid.
	 * The easiest way to handle this case is to mark both the
	 * original area and the shifted area as damaged.
	 */

	rect.x = eventPtr->xexpose.x;
	rect.y = eventPtr->xexpose.y;
	rect.width = eventPtr->xexpose.width;
	rect.height = eventPtr->xexpose.height;
	XUnionRectWithRegion(&rect, (Region) info->region,
		(Region) info->region);
	rect.x += info->dx;
	rect.y += info->dy;
	XUnionRectWithRegion(&rect, (Region) info->region,
		(Region) info->region);
    } else {
	return TK_DEFER_EVENT;
    }
    return TK_DISCARD_EVENT;
}


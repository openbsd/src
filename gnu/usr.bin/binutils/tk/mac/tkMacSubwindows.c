/* 
 * tkMacSubwindows.c --
 *
 *	Implements subwindows for the macintosh version of Tk.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacSubwindows.c 1.42 96/03/07 18:42:04
 */

#include "tkInt.h"
#include "X.h"
#include "Xlib.h"
#include <stdio.h>

#include <Windows.h>
#include <QDOffscreen.h>
#include "tkMacInt.h"

#define TOPLEVEL_MAGIC ((unsigned int) 0x77890231)

void TkMacWMInitBounds _ANSI_ARGS_((TkWindow *winPtr, Rect *geometry));
static void MacMoveWindow _ANSI_ARGS_((WindowRef window, int x, int y));
static void UpdateOffsets _ANSI_ARGS_((TkWindow *winPtr, int deltaX, int deltaY));


/*
 *----------------------------------------------------------------------
 *
 * TkMacGetXWindow --
 *
 *	Returns the X window Id associated with the given WindowRef.
 *
 * Results:
 *	The window id is returned.  None is returned if not a Tk window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Window
TkMacGetXWindow(macWinPtr)
    WindowRef macWinPtr;
{
    MacDrawable *macDrawable;

    if (macWinPtr == NULL) {
	return None;
    }
    macDrawable = (MacDrawable *) GetWRefCon(macWinPtr);
    if (macDrawable->magic == TOPLEVEL_MAGIC) {
	return (Window) macDrawable;
    } else {
	return None;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMakeWindow --
 *
 *	Creates an X Window (Mac subwindow).
 *
 * Results:
 *	The window id is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Window
TkMakeWindow(winPtr, parent)
    TkWindow *winPtr;
    Window parent;
{
    Rect geometry;
    MacDrawable *macWin;
    XEvent event;

    /*
     * Allocate sub window
     */
    macWin = (MacDrawable *) ckalloc(sizeof(MacDrawable));
    if (macWin == NULL) {
	winPtr->privatePtr = NULL;
	return None;
    }
    macWin->winPtr = winPtr;
    winPtr->privatePtr = macWin;
    macWin->clipRgn = NewRgn();
    macWin->aboveClipRgn = NewRgn();
    macWin->referenceCount = 0;
    macWin->flags = TK_CLIP_INVALID;
    macWin->scrollWinPtr = NULL;

    if (Tk_IsTopLevel(macWin->winPtr)) {
	WindowRef newWindow = NULL;
	short windowType;

	TkMacWMInitBounds(winPtr, &geometry);
	
	if (winPtr->atts.override_redirect == true ||
		TkGetTransientMaster(macWin->winPtr) != None) {
	    windowType = plainDBox;
	} else {
	    windowType = documentProc;
	}

	newWindow = NewCWindow(NULL, &geometry, "\ptemp", false, 
		windowType, (WindowRef) -1, true, 0);
	if (newWindow == NULL) {
	    return None;
	}

	MacMoveWindow(newWindow, (int) geometry.left, (int) geometry.top);
	SetWRefCon(newWindow, (long) macWin);
	SetPort((GrafPtr) newWindow);

	macWin->flags |= TK_NEVER_MAPPED;
    	macWin->magic = TOPLEVEL_MAGIC;
	macWin->portPtr = (GWorldPtr) newWindow;
	macWin->toplevel = macWin;
	macWin->xOff = 0;
	macWin->yOff = 0;
    } else {
    	macWin->magic = 0;
	macWin->portPtr = NULL;
	macWin->xOff = winPtr->parentPtr->privatePtr->xOff +
	    winPtr->parentPtr->changes.border_width +
	    winPtr->changes.x;
	macWin->yOff = winPtr->parentPtr->privatePtr->yOff +
	    winPtr->parentPtr->changes.border_width +
	    winPtr->changes.y;
	macWin->toplevel = winPtr->parentPtr->privatePtr->toplevel;
    }

    macWin->toplevel->referenceCount++;
    
    /* 
     * TODO: need general solution for visibility events.
     */
    event.xany.serial = Tk_Display(winPtr)->request;
    event.xany.send_event = False;
    event.xany.display = Tk_Display(winPtr);
	
    event.xvisibility.type = VisibilityNotify;
    event.xvisibility.window = (Window) macWin;;
    event.xvisibility.state = VisibilityUnobscured;
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);

    return (Window) macWin;
}

/*
 *----------------------------------------------------------------------
 *
 * XDestroyWindow --
 *
 *	Dealocates the given X Window.
 *
 * Results:
 *	The window id is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void 
XDestroyWindow(Display *display, Window window)
{
    MacDrawable *macWin = (MacDrawable *) window;
    GWorldPtr destPort;

    /*
     * Remove any dangling pointers that may exist if
     * the window we are deleting is being tracked by
     * the grab code.
     */
    TkMacPointerDeadWindow(macWin->winPtr);
    TkMacSetScrollbarGrow(macWin->winPtr, false);
    destPort = TkMacGetDrawablePort(window);
    macWin->toplevel->referenceCount--;
    
    if (Tk_IsTopLevel(macWin->winPtr)) {
	DisposeRgn(macWin->clipRgn);
	DisposeRgn(macWin->aboveClipRgn);
	DisposeWindow((WindowRef) destPort);
	macWin->portPtr = NULL;
	if (macWin->toplevel->referenceCount == 0) {
	    ckfree((char *) macWin->toplevel);
	}
    } else {
	if (destPort != NULL) {
	    SetGWorld(destPort, NULL);
	    InvalRgn(macWin->aboveClipRgn); /* TODO: this may not be valid */
	}
	if (macWin->winPtr->parentPtr != NULL) {
	    InvalClipRgns(macWin->winPtr->parentPtr);
	}
	DisposeRgn(macWin->clipRgn);
	DisposeRgn(macWin->aboveClipRgn);
	if (macWin->toplevel->referenceCount == 0) {
	    ckfree((char *) macWin->toplevel);
	}
	ckfree((char *) macWin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XMapWindow --
 *
 *	Map the given X Window to the screen.  See X window documentation 
 *  for more details.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The subwindow or toplevel may appear on the screen.
 *
 *----------------------------------------------------------------------
 */

void 
XMapWindow(display, window)
    Display *display;
    Window window;
{
    MacDrawable *macWin = (MacDrawable *) window;
    XEvent event;
    GWorldPtr destPort;

    destPort = TkMacGetDrawablePort(window);

    if (Tk_IsTopLevel(macWin->winPtr)) {
	ShowWindow((WindowRef) destPort);

	/*
	 * If this is the first time this toplevel is to be
	 * mapped we need to send a configure event for the
	 * window.
	 */
	if (macWin->flags | TK_NEVER_MAPPED) {
	    Point where = {0, 0};
	    
	    macWin->flags &= ~TK_NEVER_MAPPED;
	    LocalToGlobal(&where);
	    TkGenWMMoveRequestEvent((Tk_Window) macWin->winPtr,
		    where.h, where.v);
	}

	/* 
	 * We only need to send the MapNotify event
	 * for toplevel windows.
	 */
	event.xany.serial = display->request;
	event.xany.send_event = False;
	event.xany.display = display;
	
	event.xmap.window = window;
	event.xmap.type = MapNotify;
	event.xmap.event = window;
	event.xmap.override_redirect = macWin->winPtr->atts.override_redirect;
	Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    } else {
	InvalClipRgns(macWin->winPtr->parentPtr);
    }

    /* 
     * Generate damage for that area of the window 
     */
    SetGWorld(destPort, NULL);
    TkMacUpdateClipRgn(macWin->winPtr);
    InvalRgn(macWin->aboveClipRgn);
}

/*
 *----------------------------------------------------------------------
 *
 * XUnmapWindow --
 *
 *	Unmap the given X Window to the screen.  See X window
 *	documentation for more details.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The subwindow or toplevel may be removed from the screen.
 *
 *----------------------------------------------------------------------
 */

void 
XUnmapWindow(display, window)
    Display *display;
    Window window;
{
    MacDrawable *macWin = (MacDrawable *) window;
    XEvent event;
    GWorldPtr destPort;

    destPort = TkMacGetDrawablePort(window);

    if (Tk_IsTopLevel(macWin->winPtr)) {
	HideWindow((WindowRef) destPort);

	/* 
	 * We only need to send the UnmapNotify event
	 * for toplevel windows.
	 */
	event.xany.serial = display->request;
	event.xany.send_event = False;
	event.xany.display = display;
	
	event.xunmap.type = UnmapNotify;
	event.xunmap.window = window;
	event.xunmap.event = window;
	event.xunmap.from_configure = false;
	Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    } else {
	/* 
	 * Generate damage for that area of the window.
	 */
	SetGWorld(destPort, NULL);
	InvalRgn(macWin->aboveClipRgn); /* TODO: may not be valid */
	InvalClipRgns(macWin->winPtr->parentPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XResizeWindow --
 *
 *	Resize a given X window.  See X windows documentation for
 *	further details.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void 
XResizeWindow(display, window, width, height)
    Display *display;
    Window window; 
    unsigned int width;
    unsigned int height;
{
    MacDrawable *macWin = (MacDrawable *) window;
    GWorldPtr destPort;

    destPort = TkMacGetDrawablePort(window);

    display->request++;
    SetPort((GrafPtr) destPort);
    if (Tk_IsTopLevel(macWin->winPtr)) {
	/* 
	 * NOTE: we are not adding the new space to the update
	 * regoin.  It is currently assumed that Tk will need
	 * to completely redraw anway.
	 */
	SizeWindow((WindowRef) destPort, (short) width, (short) height, false);
	InvalRgn(macWin->clipRgn);
	InvalClipRgns(macWin->winPtr);
    } else {
	/* TODO: update all xOff & yOffs */
	int deltaX, deltaY;
	MacDrawable *macParent = macWin->winPtr->parentPtr->privatePtr;

	InvalClipRgns(macWin->winPtr->parentPtr);

	deltaX = - macWin->xOff;
	deltaY = - macWin->yOff;
	deltaX += macParent->xOff +
	    macWin->winPtr->parentPtr->changes.border_width +
	    macWin->winPtr->changes.x;
	deltaY += macParent->yOff +
	    macWin->winPtr->parentPtr->changes.border_width +
	    macWin->winPtr->changes.y;
	UpdateOffsets(macWin->winPtr, deltaX, deltaY);
    }
    TkGenWMResizeRequestEvent((Tk_Window) macWin->winPtr, 
	    macWin->winPtr->changes.width, macWin->winPtr->changes.height);
}

/*
 *----------------------------------------------------------------------
 *
 * XMoveResizeWindow --
 *
 *	Move or resize a given X window.  See X windows documentation
 *	for further details.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void 
XMoveResizeWindow(Display *display, Window window, 
	int x, int y,
	unsigned int width, unsigned int height)
{	
    MacDrawable *macWin = (MacDrawable *) window;
    GWorldPtr destPort;

    destPort = TkMacGetDrawablePort(window);

    SetPort((GrafPtr) destPort);
    if (Tk_IsTopLevel(macWin->winPtr)) {
	/* 
	 * NOTE: we are not adding the new space to the update
	 * regoin.  It is currently assumed that Tk will need
	 * to completely redraw anway.
	 */
	SizeWindow((WindowRef) destPort, (short) width, (short) height, false);
	MacMoveWindow((WindowRef) destPort, x, y);

	/* TODO: is the following right? */
	InvalRgn(macWin->clipRgn);
	InvalClipRgns(macWin->winPtr);
	TkGenWMMoveRequestEvent((Tk_Window) macWin->winPtr, x, y);
    } else {
	int deltaX, deltaY;
	Rect bounds;
	MacDrawable *macParent = macWin->winPtr->parentPtr->privatePtr;
	if (macParent == NULL) {
	    return; /* TODO: Probably should be a panic */
	}

	if (!EmptyRgn(macWin->clipRgn)) {
	    InvalRgn(macWin->clipRgn);
	}
	InvalClipRgns(macWin->winPtr->parentPtr);

	deltaX = - macWin->xOff;
	deltaY = - macWin->yOff;
	deltaX += macParent->xOff +
	    macWin->winPtr->parentPtr->changes.border_width +
	    macWin->winPtr->changes.x;
	deltaY += macParent->yOff +
	    macWin->winPtr->parentPtr->changes.border_width +
	    macWin->winPtr->changes.y;
		
	UpdateOffsets(macWin->winPtr, deltaX, deltaY);
	TkMacWinBounds(macWin->winPtr, &bounds);
	InvalRect(&bounds);
	TkGenWMMoveRequestEvent((Tk_Window) macWin->winPtr, 
	    macWin->winPtr->changes.x, macWin->winPtr->changes.y);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XMoveWindow --
 *
 *	Move a given X window.  See X windows documentation for further
 *  details.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void 
XMoveWindow(display, window, x, y)
    Display* display;
    Window window;
    int x;
    int y;
{
    MacDrawable *macWin = (MacDrawable *) window;
    GWorldPtr destPort;

    destPort = TkMacGetDrawablePort(window);

    SetPort((GrafPtr) destPort);
    if (Tk_IsTopLevel(macWin->winPtr)) {
	/* 
	 * NOTE: we are not adding the new space to the update
	 * regoin.  It is currently assumed that Tk will need
	 * to completely redraw anway.
	 */
	MacMoveWindow((WindowRef) destPort, x, y);

	/* TODO: is the following right? */
	InvalRgn(macWin->clipRgn);
	InvalClipRgns(macWin->winPtr);
	TkGenWMMoveRequestEvent((Tk_Window) macWin->winPtr, x, y);
    } else {
	int deltaX, deltaY;
	Rect bounds;
	MacDrawable *macParent = macWin->winPtr->parentPtr->privatePtr;
	if (macParent == NULL) {
	    return; /* TODO: Probably should be a panic */
	}

	if (!EmptyRgn(macWin->clipRgn)) {
	    InvalRgn(macWin->clipRgn);
	}
	InvalClipRgns(macWin->winPtr->parentPtr);

	deltaX = - macWin->xOff;
	deltaY = - macWin->yOff;
	deltaX += macParent->xOff +
	    macWin->winPtr->parentPtr->changes.border_width +
	    macWin->winPtr->changes.x;
	deltaY += macParent->yOff +
	    macWin->winPtr->parentPtr->changes.border_width +
	    macWin->winPtr->changes.y;
		
	UpdateOffsets(macWin->winPtr, deltaX, deltaY);
	TkMacWinBounds(macWin->winPtr, &bounds);
	InvalRect(&bounds);
	TkGenWMMoveRequestEvent((Tk_Window) macWin->winPtr, 
	    macWin->winPtr->changes.x, macWin->winPtr->changes.y);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XRaiseWindow --
 *
 *	Change the stacking order of a window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the stacking order of the specified window.
 *
 *----------------------------------------------------------------------
 */

void 
XRaiseWindow(display, window)
    Display* display;
    Window window;
{
    MacDrawable *macWin = (MacDrawable *) window;
    
    display->request++;
    if (Tk_IsTopLevel(macWin->winPtr)) {
	TkWmRestackToplevel(macWin->winPtr, Above, NULL);
    } else {
    	/* TODO: this should generate damage */
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XConfigureWindow --
 *
 *	Change the size, position, stacking, or border of the specified
 *	window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the attributes of the specified window.  Note that we
 *	ignore the passed in values and use the values stored in the
 *	TkWindow data structure.
 *
 *----------------------------------------------------------------------
 */

void
XConfigureWindow(display, w, value_mask, values)
    Display* display;
    Window w;
    unsigned int value_mask;
    XWindowChanges* values;
{
    MacDrawable *macWin = (MacDrawable *) w;
    TkWindow *winPtr = macWin->winPtr;

    display->request++;

    /*
     * Change the shape and/or position of the window.
     */

    if (value_mask & (CWX|CWY|CWWidth|CWHeight)) {
	XMoveResizeWindow(display, w, winPtr->changes.x, winPtr->changes.y,
		winPtr->changes.width, winPtr->changes.height);
    }

    /*
     * Change the stacking order of the window.  Tk actuall keeps all
     * the information we need for stacking order.  All we need to do
     * is make sure the clipping regions get updated and generate damage
     * that will ensure things get drawn correctly.
     */

    if (value_mask & CWStackMode) {
	Rect bounds;
	GWorldPtr destPort;
	
	destPort = TkMacGetDrawablePort(w);
	SetPort((GrafPtr) destPort);
	InvalClipRgns(winPtr->parentPtr);
	TkMacWinBounds(winPtr, &bounds);
	InvalRect(&bounds);
    } 

    /* TkGenWMMoveRequestEvent(macWin->winPtr, 
	    macWin->winPtr->changes.x, macWin->winPtr->changes.y); */
}

/*
 *----------------------------------------------------------------------
 *
 *  TkMacUpdateClipRgn --
 *
 *	This function updates the cliping regions for a given window
 *	and all of its children.  Once updated the TK_CLIP_INVALID flag
 *	in the subwindow data structure is unset.  The TK_CLIP_INVALID 
 *	flag should always be unset before any drawing is attempted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The clip regions for the window and its children are updated.
 *
 *----------------------------------------------------------------------
 */

void
TkMacUpdateClipRgn(winPtr)
    TkWindow *winPtr;
{
    RgnHandle rgn, tempRgn;
    int x, y;
    TkWindow *win2Ptr;

    if (winPtr == NULL) {
	return;
    }
    
    if (winPtr->privatePtr->flags & TK_CLIP_INVALID) {
	rgn = winPtr->privatePtr->aboveClipRgn;
	tempRgn = NewRgn();
		
	/* 
	 * Start with a region defined by the window bounds.
	 */
	x = winPtr->privatePtr->xOff;
	y = winPtr->privatePtr->yOff;
	SetRectRgn(rgn, (short) x, (short) y,
		(short) (winPtr->changes.width  + x), 
		(short) (winPtr->changes.height + y));

	/* 
	 * Clip away the area of any windows that may obscure this
	 * window.  First, clip to the parents visable clip region.
	 * Second, clip away any siblings that are higher in the
	 * stacking order.
	 */
	if (!Tk_IsTopLevel(winPtr)) {
	    TkMacUpdateClipRgn(winPtr->parentPtr);
	    SectRgn(rgn, 
		    winPtr->parentPtr->privatePtr->aboveClipRgn, rgn);
				
	    win2Ptr = winPtr->nextPtr;
	    while (win2Ptr != NULL) {
		if (Tk_IsTopLevel(win2Ptr) || !Tk_IsMapped(win2Ptr)) {
		    win2Ptr = win2Ptr->nextPtr;
		    continue;
		}
		x = win2Ptr->privatePtr->xOff;
		y = win2Ptr->privatePtr->yOff;
		SetRectRgn(tempRgn, (short) x, (short) y,
			(short) (win2Ptr->changes.width  + x), 
			(short) (win2Ptr->changes.height + y));
		DiffRgn(rgn, tempRgn, rgn);
							  
		win2Ptr = win2Ptr->nextPtr;
	    }
	}
		
	/* 
	 * The final clip region is the aboveClip region (or visable
	 * region) minus all the children of this window.
	 */
	rgn = winPtr->privatePtr->clipRgn;
	CopyRgn(winPtr->privatePtr->aboveClipRgn, rgn);
		
	win2Ptr = winPtr->childList;
	while (win2Ptr != NULL) {
	    if (Tk_IsTopLevel(win2Ptr) || !Tk_IsMapped(win2Ptr)) {
		win2Ptr = win2Ptr->nextPtr;
		continue;
	    }
	    x = win2Ptr->privatePtr->xOff;
	    y = win2Ptr->privatePtr->yOff;
	    SetRectRgn(tempRgn, (short) x, (short) y,
		    (short) (win2Ptr->changes.width  + x), 
		    (short) (win2Ptr->changes.height + y));
	    DiffRgn(rgn, tempRgn, rgn);
							  
	    win2Ptr = win2Ptr->nextPtr;
	}
		
	DisposeRgn(tempRgn);
	winPtr->privatePtr->flags &= ~TK_CLIP_INVALID;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacVisableClipRgn --
 *
 *	This function returnd the Macintosh cliping region for the 
 *	given window.  A NULL Rgn means the window is not visable.
 *
 * Results:
 *	The region.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

RgnHandle
TkMacVisableClipRgn(winPtr)
    TkWindow *winPtr;
{
    if (winPtr->privatePtr->flags & TK_CLIP_INVALID) {
	TkMacUpdateClipRgn(winPtr);
    }

    return winPtr->privatePtr->clipRgn;
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacGetDrawablePort --
 *
 *	This function returns the Graphics Port for a given X drawable.
 *
 * Results:
 *	A GWorld pointer.  Either an off screen pixmap or a Window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

GWorldPtr
TkMacGetDrawablePort(drawable)
    Drawable drawable;
{
    MacDrawable *macWin = (MacDrawable *) drawable;
    
    if (macWin->clipRgn == NULL) {
	return macWin->portPtr;
    }
    
    return macWin->toplevel->portPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * InvalClipRgns --
 *
 *	This function invalidates the clipping regions for a given
 *	window and all of its children.  This function should be
 *	called whenever changes are made to subwindows that would
 *	effect the size or position of windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cliping regions for the window and its children are
 *	mark invalid.  (Make sure they are valid before drawing.)
 *
 *----------------------------------------------------------------------
 */

void
InvalClipRgns(winPtr)
    TkWindow *winPtr;
{
    TkWindow *childPtr;
	
    /* 
     * If already marked we can stop because all 
     * decendants will also already be marked.
     */
    if (winPtr->privatePtr->flags & TK_CLIP_INVALID) {
	return;
    }
	
    winPtr->privatePtr->flags |= TK_CLIP_INVALID;
	
    /* 
     * Invalidate clip regions for all children & 
     * thier decendants - unless the child is a toplevel.
     */
    childPtr = winPtr->childList;
    while (childPtr != NULL) {
	if (!Tk_IsTopLevel(childPtr)) {
	    InvalClipRgns(childPtr);
	}
	childPtr = childPtr->nextPtr;
    }
}


void
TkMacWinBounds(winPtr, bounds)
    TkWindow *winPtr;
    Rect *bounds;
{
    bounds->left = (short) winPtr->privatePtr->xOff;
    bounds->top = (short) winPtr->privatePtr->yOff;
    bounds->right = (short) (winPtr->privatePtr->xOff +
	    winPtr->changes.width);
    bounds->bottom = (short) (winPtr->privatePtr->yOff +
	    winPtr->changes.height);
}

/*
 *----------------------------------------------------------------------
 *
 * MacMoveWindow --
 *
 *	A replacement for the Macintosh MoveWindow function.  This
 *	function adjusts the inputs to MoveWindow to offset the root of 
 *	the window system.  This has two effects: 1) the orgin is just
 *	below the menu bar, and 2) coords refer to the window dressing
 *	rather than the top of the content.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void 
MacMoveWindow(WindowRef window, int x, int y)
{
    int offset;

    /*
     * Currently, the title bar height is determined from
     * this constant table.  However, it would be better to
     * to be dynamic.  Unfortunantly, I do not know of a way
     * to get the title bar height that doesn't require the
     * the window to be visable on the screen.
     */

    switch (GetWVariant(window)) {
	case 0:
	case 4:
	case 8:
	case 12:
	    offset = 18;
	    break;
	case 2:
	case 3:
	    offset = 0;
    }
    /* offset += GetMBarHeight(); */
    MoveWindow(window, (short) x, (short) (y + offset), false);
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateOffsets --
 *
 *	Updates the X & Y offsets of the given TkWindow from the
 *	TopLevel it is a decendant of.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The xOff & yOff fields for the Mac window datastructure
 *	is updated to the proper offset.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateOffsets(winPtr, deltaX, deltaY)
    TkWindow *winPtr;
    int deltaX, deltaY;
{
    TkWindow *childPtr;

    winPtr->privatePtr->xOff += deltaX;
    winPtr->privatePtr->yOff += deltaY;

    childPtr = winPtr->childList;
    while (childPtr != NULL) {
	if (!Tk_IsTopLevel(childPtr)) {
	    UpdateOffsets(childPtr, deltaX, deltaY);
	}
	childPtr = childPtr->nextPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XCreatePixmap --
 *
 *	Creates an in memory drawing surface.
 *
 * Results:
 *	Returns a handle to a new pixmap.
 *
 * Side effects:
 *	Allocates a new Macintosh GWorld.
 *
 *----------------------------------------------------------------------
 */

Pixmap
XCreatePixmap(display, d, width, height, depth)
    Display *display;		/* Display for new pixmap. */
    Drawable d;			/* Drawable where pixmap will be used. */
    unsigned int width, height;	/* Dimensions of pixmap. */
    unsigned int depth;		/* Bits per pixel for pixmap. */
{
    QDErr err;
    GWorldPtr gWorld;
    Rect bounds;
    MacDrawable *macPix;
    PixMapHandle pixels;
    
    display->request++;
    macPix = (MacDrawable *) ckalloc(sizeof(MacDrawable));
    macPix->winPtr = NULL;
    macPix->xOff = 0;
    macPix->yOff = 0;
    macPix->clipRgn = NULL;
    macPix->aboveClipRgn = NULL;
    macPix->magic = 0;
    macPix->referenceCount = 0;
    macPix->toplevel = NULL;
    macPix->flags = 0;
    macPix->scrollWinPtr = NULL;

    bounds.top = bounds.left = 0;
    bounds.right = (short) width;
    bounds.bottom = (short) height;
    err = NewGWorld(&gWorld, 0, &bounds, NULL, NULL, 0);
    if (err != noErr) {
        panic("NewGWorld failed in XCreatePixmap");
    }

    /*
     * TODO: This is a short term solution.  We should Lock 
     * the pixels only when we need to draw.
     */
    pixels = GetGWorldPixMap(gWorld);
    LockPixels(pixels);
    macPix->portPtr = gWorld;

    return (Pixmap) macPix;
}

/*
 *----------------------------------------------------------------------
 *
 * XFreePixmap --
 *
 *	Release the resources associated with a pixmap.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Deletes the Macintosh GWorld created by XCreatePixmap.
 *
 *----------------------------------------------------------------------
 */

void 
XFreePixmap(display, pixmap)
    Display *display;		/* Display. */
    Pixmap pixmap;     		/* Pixmap to destroy */
{
    MacDrawable *macPix = (MacDrawable *) pixmap;
    PixMapHandle pixels;

    display->request++;
    pixels = GetGWorldPixMap(macPix->portPtr);
    UnlockPixels(pixels);
    DisposeGWorld(macPix->portPtr);
    ckfree((char *) macPix);
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacSetScrollbarGrow --
 *
 *	Sets a flag for a toplevel window indicating that the passed
 *	Tk scrollbar window will display the grow region for the 
 *	toplevel window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A flag is set int windows toplevel parent.
 *
 *----------------------------------------------------------------------
 */

void 
TkMacSetScrollbarGrow(winPtr, flag)
    TkWindow *winPtr;		/* Tk scrollbar window. */
    int flag;			/* Boolean value true or false. */
{
    if (flag) {
	winPtr->privatePtr->toplevel->flags |= TK_SCROLLBAR_GROW;
	winPtr->privatePtr->toplevel->scrollWinPtr = winPtr;
    } else if (winPtr->privatePtr->toplevel->scrollWinPtr == winPtr) {
	winPtr->privatePtr->toplevel->flags &= ~TK_SCROLLBAR_GROW;
	winPtr->privatePtr->toplevel->scrollWinPtr = NULL;	
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacGetScrollbarGrowWindow --
 *
 *	Tests to see if a given window's toplevel window contains a
 *	scrollbar that will draw the GrowIcon for the window.
 *
 * Results:
 *	Boolean value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkWindow * 
TkMacGetScrollbarGrowWindow(winPtr)
    TkWindow *winPtr;	/* Tk window. */
{
    if (winPtr == NULL) return NULL;
    return winPtr->privatePtr->toplevel->scrollWinPtr;
}

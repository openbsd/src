/* 
 * tkWinWindow.c --
 *
 *	Xlib emulation routines for Windows related to creating,
 *	displaying and destroying windows.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinWindow.c 1.13 96/04/11 17:52:20
 */

#include "tkWinInt.h"

/*
 * Forward declarations for procedures defined in this file:
 */

static void		NotifyVisibility _ANSI_ARGS_((XEvent *eventPtr,
			    TkWindow *winPtr));
static void 		StackWindow _ANSI_ARGS_((Window w, Window sibling,
			    int stack_mode));

/*
 *----------------------------------------------------------------------
 *
 * TkMakeWindow --
 *
 *	Creates a Windows window object based on the current attributes
 *	of the specified TkWindow.
 *
 * Results:
 *	Returns a pointer to a new TkWinDrawable cast to a Window.
 *
 * Side effects:
 *	Creates a new window.
 *
 *----------------------------------------------------------------------
 */

Window
TkMakeWindow(winPtr, parent)
    TkWindow *winPtr;
    Window parent;
{
    HWND parentWin;
    TkWinDrawable *twdPtr;
    int style;
    

    twdPtr = (TkWinDrawable*) ckalloc(sizeof(TkWinDrawable));
    if (twdPtr == NULL) {
	return None;
    }

    twdPtr->type = TWD_WINDOW;
    twdPtr->window.winPtr = winPtr;

    if (parent != None) {
	parentWin = TkWinGetHWND(parent);
	style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    } else {
	parentWin = NULL;
	style = WS_POPUP | WS_CLIPCHILDREN;
    }

    twdPtr->window.handle = CreateWindow(TK_WIN_CHILD_CLASS_NAME, "", style,
	    winPtr->changes.x, winPtr->changes.y,
	    winPtr->changes.width, winPtr->changes.height,
	    parentWin, NULL, TkWinGetAppInstance(), twdPtr);

    if (twdPtr->window.handle == NULL) {
	ckfree((char *) twdPtr);
	twdPtr = NULL;
    }

    return (Window)twdPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * XDestroyWindow --
 *
 *	Destroys the given window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sends the WM_DESTROY message to the window and then destroys
 *	it the Win32 resources associated with the window.
 *
 *----------------------------------------------------------------------
 */

void
XDestroyWindow(display, w)
    Display* display;
    Window w;
{
    TkWinDrawable *twdPtr = (TkWinDrawable *)w;
    TkWindow *winPtr = TkWinGetWinPtr(w);
    HWND hwnd = TkWinGetHWND(w);

    display->request++;

    /*
     * Remove references to the window in the pointer module, and 
     * then remove the backpointer from the drawable.
     */

    TkWinPointerDeadWindow(winPtr);
    twdPtr->window.winPtr = NULL;

    /*
     * Don't bother destroying the window if we are going to destroy
     * the parent later.  Also if the window has already been destroyed
     * then we need to free the drawable now.
     */

    if (!hwnd) {
	ckfree((char *)twdPtr);
    } else if (!(winPtr->flags & TK_PARENT_DESTROYED)) {
	DestroyWindow(hwnd);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XMapWindow --
 *
 *	Cause the given window to become visible.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Causes the window state to change, and generates a MapNotify
 *	event.
 *
 *----------------------------------------------------------------------
 */

void
XMapWindow(display, w)
    Display* display;
    Window w;
{
    XEvent event;
    TkWindow *parentPtr;
    TkWindow *winPtr = TkWinGetWinPtr(w);

    display->request++;

    ShowWindow(TkWinGetHWND(w), SW_SHOWNORMAL);
    winPtr->flags |= TK_MAPPED;

    event.type = MapNotify;
    event.xmap.serial = display->request;
    event.xmap.send_event = False;
    event.xmap.display = display;
    event.xmap.event = winPtr->window;
    event.xmap.window = winPtr->window;
    event.xmap.override_redirect = winPtr->atts.override_redirect;
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);

    /*
     * Check to see if this window is visible now.  If all of the parent
     * windows up to the first toplevel are mapped, then this window and
     * its mapped children have just become visible.
     */

    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	for (parentPtr = winPtr->parentPtr; ;
	        parentPtr = parentPtr->parentPtr) {
	    if ((parentPtr == NULL) || !(parentPtr->flags & TK_MAPPED)) {
		return;
	    }
	    if (parentPtr->flags & TK_TOP_LEVEL) {
		break;
	    }
	}
    }

    /*
     * Generate VisibilityNotify events for this window and its mapped
     * children.
     */

    event.type = VisibilityNotify;
    event.xvisibility.state = VisibilityUnobscured;
    NotifyVisibility(&event, winPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * NotifyVisibility --
 *
 *	This function recursively notifies the mapped children of the
 *	specified window of a change in visibility.  Note that we don't
 *	properly report the visibility state, since Windows does not
 *	provide that info.  The eventPtr argument must point to an event
 *	that has been completely initialized except for the window slot.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Generates lots of events.
 *
 *----------------------------------------------------------------------
 */

static void
NotifyVisibility(eventPtr, winPtr)
    XEvent *eventPtr;		/* Initialized VisibilityNotify event. */
    TkWindow *winPtr;		/* Window to notify. */
{
    eventPtr->xvisibility.window = winPtr->window;
    Tk_QueueWindowEvent(eventPtr, TCL_QUEUE_TAIL);
    for (winPtr = winPtr->childList; winPtr != NULL;
	    winPtr = winPtr->nextPtr) {
	if (winPtr->flags & TK_MAPPED) {
	    NotifyVisibility(eventPtr, winPtr);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * XUnmapWindow --
 *
 *	Cause the given window to become invisible.
 *
 * Results:
 *	None
 *
 * Side effects:
 *	Causes the window state to change, and generates an UnmapNotify
 *	event.
 *
 *----------------------------------------------------------------------
 */

void
XUnmapWindow(display, w)
    Display* display;
    Window w;
{
    XEvent event;
    TkWindow *winPtr = TkWinGetWinPtr(w);

    display->request++;

    ShowWindow(TkWinGetHWND(w), SW_HIDE);
    winPtr->flags &= ~TK_MAPPED;

    event.type = UnmapNotify;
    event.xunmap.serial = display->request;
    event.xunmap.send_event = False;
    event.xunmap.display = display;
    event.xunmap.event = winPtr->window;
    event.xunmap.window = winPtr->window;
    event.xunmap.from_configure = False;
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
}

/*
 *----------------------------------------------------------------------
 *
 * XMoveResizeWindow --
 *
 *	Move and resize a window relative to its parent.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Repositions and resizes the specified window.
 *
 *----------------------------------------------------------------------
 */

void
XMoveResizeWindow(display, w, x, y, width, height)
    Display* display;
    Window w;
    int x;			/* Position relative to parent. */
    int y;
    unsigned int width;
    unsigned int height;
{
    display->request++;
    MoveWindow(TkWinGetHWND(w), x, y, width, height, TRUE);
}

/*
 *----------------------------------------------------------------------
 *
 * XMoveWindow --
 *
 *	Move a window relative to its parent.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Repositions the specified window.
 *
 *----------------------------------------------------------------------
 */

void
XMoveWindow(display, w, x, y)
    Display* display;
    Window w;
    int x;
    int y;
{
    TkWindow *winPtr = TkWinGetWinPtr(w);

    display->request++;

    MoveWindow(TkWinGetHWND(w), x, y, winPtr->changes.width,
	    winPtr->changes.height, TRUE);
}

/*
 *----------------------------------------------------------------------
 *
 * XResizeWindow --
 *
 *	Resize a window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resizes the specified window.
 *
 *----------------------------------------------------------------------
 */

void
XResizeWindow(display, w, width, height)
    Display* display;
    Window w;
    unsigned int width;
    unsigned int height;
{
    TkWindow *winPtr = TkWinGetWinPtr(w);

    display->request++;

    MoveWindow(TkWinGetHWND(w), winPtr->changes.x, winPtr->changes.y, width,
	    height, TRUE);
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
XRaiseWindow(display, w)
    Display* display;
    Window w;
{
    HWND window = TkWinGetHWND(w);

    display->request++;
    SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0,
	    SWP_NOMOVE | SWP_NOSIZE);
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
    TkWindow *winPtr = TkWinGetWinPtr(w);
    HWND window = TkWinGetHWND(w);
    HWND insertAfter;

    display->request++;

    /*
     * Change the shape and/or position of the window.
     */

    if (value_mask & (CWX|CWY|CWWidth|CWHeight)) {
	MoveWindow(window, winPtr->changes.x, winPtr->changes.y,
		winPtr->changes.width, winPtr->changes.height, TRUE);
    }

    /*
     * Change the stacking order of the window.
     */

    if (value_mask & CWStackMode) {
	if ((value_mask & CWSibling) && (values->sibling != None)) {
	    HWND sibling = TkWinGetHWND(values->sibling);

	    /*
	     * Windows doesn't support the Above mode, so we insert the
	     * window just below the sibling and then swap them.
	     */

	    if (values->stack_mode == Above) {
		SetWindowPos(window, sibling, 0, 0, 0, 0,
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		insertAfter = window;
		window = sibling;
	    } else {
		insertAfter = sibling;
	    }
	} else {
	    insertAfter = (values->stack_mode == Above) ? HWND_TOP
		: HWND_BOTTOM;
	}
		
	SetWindowPos(window, insertAfter, 0, 0, 0, 0,
		SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
    } 
}

/*
 *----------------------------------------------------------------------
 *
 * XClearWindow --
 *
 *	Clears the entire window to the current background color.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Erases the current contents of the window.
 *
 *----------------------------------------------------------------------
 */

void
XClearWindow(display, w)
    Display* display;
    Window w;
{
    RECT rc;
    HBRUSH brush;
    HPALETTE oldPalette, palette;
    TkWindow *winPtr;
    HWND hwnd = TkWinGetHWND(w);
    HDC dc = GetDC(hwnd);

    palette = TkWinGetPalette(display->screens[0].cmap);
    oldPalette = SelectPalette(dc, palette, FALSE);

    display->request++;

    winPtr = TkWinGetWinPtr(w);
    brush = CreateSolidBrush(winPtr->atts.background_pixel);
    GetWindowRect(hwnd, &rc);
    rc.right = rc.right - rc.left;
    rc.bottom = rc.bottom - rc.top;
    rc.left = rc.top = 0;
    FillRect(dc, &rc, brush);

    DeleteObject(brush);
    SelectPalette(dc, oldPalette, TRUE);
    ReleaseDC(hwnd, dc);
}

/*
 *----------------------------------------------------------------------
 *
 * XChangeWindowAttributes --
 *
 *	This function is called when the attributes on a window are
 *	updated.  Since Tk maintains all of the window state, the only
 *	relevant value is the cursor.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May cause the mouse position to be updated.
 *
 *----------------------------------------------------------------------
 */

void
XChangeWindowAttributes(display, w, valueMask, attributes)
    Display* display;
    Window w;
    unsigned long valueMask;
    XSetWindowAttributes* attributes;
{
    if (valueMask & CWCursor) {
	XDefineCursor(display, w, attributes->cursor);
    }
}

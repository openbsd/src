/* 
 * tkWinPointer.c --
 *
 *	Windows specific mouse tracking code.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinPointer.c 1.14 96/02/15 18:56:05
 */

#include "tkWinInt.h"

/*
 * Check for enter/leave events every MOUSE_TIMER_INTERVAL milliseconds.
 */

#define MOUSE_TIMER_INTERVAL 250

/*
 * Mask that selects any of the state bits corresponding to buttons,
 * plus masks that select individual buttons' bits:
 */

#define ALL_BUTTONS \
	(Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask)
static unsigned int buttonStates[] = {
    Button1Mask, Button2Mask, Button3Mask, Button4Mask, Button5Mask
};

/*
 * Declarations of static variables used in the grab module.
 */

static int captured;		/* 1 if mouse events outside of Tk windows
				 * will be reported, else 0. */
static TkWindow *grabWinPtr;	/* Window that defines the top of the grab
				 * tree in a global grab. */
static TkWindow *keyboardWinPtr;/* Current keyboard grab window. */
static TkWindow *restrictWinPtr;
				/* Window to which all mouse
				   events will be reported. */

/*
 * Declarations of static variables used in mouse position tracking.
 */

static POINT lastMousePos;	/* Last known mouse position. */
static HWND lastMouseWindow;	/* Last known mouse window. */
static TkWindow *lastMouseWinPtr;
				/* Last window mouse was seen in.  Used to
				 * detect Enter/Leave events. */
static Tcl_TimerToken mouseTimer;
				/* Handle to the latest mouse timer. */
static int mouseTimerSet;	/* Non-zero if the mouse timer is active. */

/*
 * Forward declarations of procedures used in this file.
 */

static void		InitializeCrossingEvent _ANSI_ARGS_((
    			    XEvent* eventPtr, TkWindow *winPtr,
			    long x, long y));
static void		MouseTimerProc _ANSI_ARGS_((ClientData clientData));
static int		UpdateMousePosition _ANSI_ARGS_((HWND hwnd,
			    TkWindow *winPtr, long x, long y));

/*
 *----------------------------------------------------------------------
 *
 * TkWinPointerInit --
 *
 *	Initialize the mouse pointer module.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Initializes various static variables.
 *
 *----------------------------------------------------------------------
 */

void
TkWinPointerInit()
{
    captured = 0;
    grabWinPtr = NULL;
    keyboardWinPtr = NULL;
    restrictWinPtr = NULL;

    mouseTimerSet = 0;
    GetCursorPos(&lastMousePos);
    lastMouseWindow = WindowFromPoint(lastMousePos);
    lastMouseWinPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinPointerDeadWindow --
 *
 *	Clean up pointer module state when a window is destroyed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May change the grab module settings.
 *
 *----------------------------------------------------------------------
 */

void
TkWinPointerDeadWindow(winPtr)
    TkWindow *winPtr;
{
    if (winPtr == lastMouseWinPtr) {
	lastMouseWinPtr = NULL;
    }
    if (winPtr == grabWinPtr) {
	grabWinPtr = NULL;
    }
    if (winPtr == restrictWinPtr) {
	restrictWinPtr = NULL;
    }
    if (!(restrictWinPtr || grabWinPtr)) {
	captured = 0;
	ReleaseCapture();
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinPointerEvent --
 *
 *	This procedure is called for each pointer-related event,
 *	before the event is queued.  It simulates X style automatic
 *	grabs so that button release events are not lost.  It also
 *	updates the pointer position so enter/leave events will be
 *	correctly generated.
 *
 * Results:
 *	Returns 0 if the event should be discarded.
 *
 * Side effects:
 *	Changes the current mouse capture window. 
 *
 *----------------------------------------------------------------------
 */

void
TkWinPointerEvent(eventPtr, winPtr)
    XEvent *eventPtr;		/* Event to process */
    TkWindow *winPtr;		/* Window to which event was reported. */
{
    POINT pos;
    HWND hwnd;
    TkWinDrawable *twdPtr;

    /*
     * If the mouse is captured, Windows will report all pointer
     * events to the capture window.  So, we need to determine which
     * window the mouse is really over and change the event.  Note
     * that the computed hwnd may point to a window not owned by Tk,
     * or a toplevel decorative frame, so winPtr can be NULL.
     */

    if (captured) {
	pos.x = eventPtr->xmotion.x_root;
	pos.y = eventPtr->xmotion.y_root;
	hwnd = WindowFromPoint(pos);
	twdPtr = TkWinGetDrawableFromHandle(hwnd);
	if (twdPtr && (twdPtr->type == TWD_WINDOW)) {
	    winPtr = TkWinGetWinPtr(twdPtr);
	} else {
	    winPtr = NULL;
	}
    } else {
	hwnd = TkWinGetHWND(Tk_WindowId(winPtr));
    }

    switch (eventPtr->type) {
	case MotionNotify: 

	    /*
	     * If updating the mouse position caused an enter or leave
	     * event to be generated, we discard the motion event.
	     */

	    if (UpdateMousePosition(hwnd, winPtr, eventPtr->xmotion.x_root,
		    eventPtr->xmotion.y_root)) {
		return;
	    }
	    break;

	case ButtonPress:

	    /*
	     * Set mouse capture and the restrict window if we are
	     * currently unrestricted.  However, If this is not the
	     * first button pressed and we are already grabbed, do not
	     * change anything.
	     */

	    if (!restrictWinPtr) {
		if (!grabWinPtr) {
		    /*
		     * Mouse was ungrabbed, so set a button grab.
		     */

		    restrictWinPtr = winPtr;
		    captured = 1;
		    SetCapture(hwnd);
		} else if ((eventPtr->xmotion.state & ALL_BUTTONS) == 0) {

		    /*
		     * Mouse was grabbed, but not in a button grab.
		     * Make sure the new restrict window is inside the
		     * current grab tree.
		     */

		    if (TkPositionInTree(winPtr, grabWinPtr)
			    == TK_GRAB_IN_TREE) {
			restrictWinPtr = winPtr;
		    } else {
			restrictWinPtr = grabWinPtr;
		    }
		    captured = 1;
		    SetCapture(TkWinGetHWND(Tk_WindowId(restrictWinPtr)));
		}
	    }
	    break;

	case ButtonRelease:

	    /*
	     * Release the mouse capture when the last button is
	     * released and we aren't in a global grab.
	     */
		    
	    if ((eventPtr->xbutton.state & ALL_BUTTONS)
		    == buttonStates[eventPtr->xbutton.button - Button1]) {
		if (!grabWinPtr) {
		    captured = 0;
		    ReleaseCapture();
		}

		/*
		 * If we are releasing a restrict window, then we need
		 * to send the button event followed by mouse motion from
		 * the restrict window the the current mouse position.
		 */

		if (restrictWinPtr) {
		    if (Tk_WindowId(restrictWinPtr) != eventPtr->xany.window) {
			TkChangeEventWindow(eventPtr, restrictWinPtr);
		    }
		    Tk_QueueWindowEvent(eventPtr, TCL_QUEUE_TAIL);
		    lastMouseWinPtr = restrictWinPtr;
		    restrictWinPtr = NULL;
		    UpdateMousePosition(hwnd, winPtr, eventPtr->xmotion.x_root,
			    eventPtr->xmotion.y_root);
		    return;
		}
	    }
	    break;
    }

    /*
     * If a restrict window is set, make sure the pointer event is reported
     * relative to that window.  Otherwise, if a global grab is in effect
     * then events outside of window managed by Tk should be reported to the
     * grab window.
     */

    if (restrictWinPtr) {
	winPtr = restrictWinPtr;
    } else if (grabWinPtr && !winPtr) {
	winPtr = grabWinPtr;
    }

    /*
     * If the target window has changed, update the coordinates in the event.
     */

    if (winPtr && Tk_WindowId(winPtr) != eventPtr->xany.window) {
	TkChangeEventWindow(eventPtr, winPtr);
    }
    Tk_QueueWindowEvent(eventPtr, TCL_QUEUE_TAIL);
}

/*
 *----------------------------------------------------------------------
 *
 * XGrabPointer --
 *
 *	Capture the mouse so event are reported outside of toplevels.
 *	Note that this is a very limited implementation that only
 *	supports GrabModeAsync and owner_events True.
 *
 * Results:
 *	Always returns GrabSuccess.
 *
 * Side effects:
 *	Turns on mouse capture, sets the global grab pointer, and
 *	clears any window restrictions.
 *
 *----------------------------------------------------------------------
 */

int
XGrabPointer(display, grab_window, owner_events, event_mask, pointer_mode,
	keyboard_mode, confine_to, cursor, time)
    Display* display;
    Window grab_window;
    Bool owner_events;
    unsigned int event_mask;
    int pointer_mode;
    int keyboard_mode;
    Window confine_to;
    Cursor cursor;
    Time time;
{
    HWND hwnd = TkWinGetHWND(grab_window);
    grabWinPtr = TkWinGetWinPtr(grab_window);
    captured = 1;
    restrictWinPtr = NULL;
    SetCapture(hwnd);
    if (TkPositionInTree(lastMouseWinPtr, grabWinPtr) == TK_GRAB_IN_TREE) {
	TkWinUpdateCursor(lastMouseWinPtr);
    } else {
	TkWinUpdateCursor(grabWinPtr);
    }
    return GrabSuccess;
}

/*
 *----------------------------------------------------------------------
 *
 * XUngrabPointer --
 *
 *	Release the current grab.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Releases the mouse capture.
 *
 *----------------------------------------------------------------------
 */

void
XUngrabPointer(display, time)
    Display* display;
    Time time;
{
    captured = 0;
    grabWinPtr = NULL;
    restrictWinPtr = NULL;
    ReleaseCapture();
    TkWinUpdateCursor(lastMouseWinPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * XGrabKeyboard --
 *
 *	Simulates a keyboard grab by setting the focus.
 *
 * Results:
 *	Always returns GrabSuccess.
 *
 * Side effects:
 *	Sets the keyboard focus to the specified window.
 *
 *----------------------------------------------------------------------
 */

int
XGrabKeyboard(display, grab_window, owner_events, pointer_mode,
	keyboard_mode, time)
    Display* display;
    Window grab_window;
    Bool owner_events;
    int pointer_mode;
    int keyboard_mode;
    Time time;
{
    keyboardWinPtr = TkWinGetWinPtr(grab_window);
    return GrabSuccess;
}

/*
 *----------------------------------------------------------------------
 *
 * XUngrabKeyboard --
 *
 *	Releases the simulated keyboard grab.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the keyboard focus back to the value before the grab.
 *
 *----------------------------------------------------------------------
 */

void
XUngrabKeyboard(display, time)
    Display* display;
    Time time;
{
    keyboardWinPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * InitializeCrossingEvent --
 *
 *	Initializes the common fields for enter/leave events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Fills in the specified event structure.
 *
 *----------------------------------------------------------------------
 */

static void
InitializeCrossingEvent(eventPtr, winPtr, x, y)
    XEvent* eventPtr;		/* Event structure to initialize. */
    TkWindow *winPtr;		/* Window to make event relative to. */
    long x, y;			/* Root coords of event. */
{
    eventPtr->xcrossing.serial = LastKnownRequestProcessed(winPtr->display);
    eventPtr->xcrossing.send_event = 0;
    eventPtr->xcrossing.display = winPtr->display;
    eventPtr->xcrossing.root = RootWindow(winPtr->display, winPtr->screenNum);
    eventPtr->xcrossing.time = TkCurrentTime(winPtr->dispPtr);
    eventPtr->xcrossing.x_root = x;
    eventPtr->xcrossing.y_root = y;
    eventPtr->xcrossing.state = TkWinGetModifierState(WM_MOUSEMOVE, 0, 0);
    eventPtr->xcrossing.mode = NotifyNormal;
    eventPtr->xcrossing.focus = False;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateMousePosition --
 *
 *	Update the current mouse window and position, and generate
 *	any enter/leave events that are needed.  Will schedule a
 *	timer to check the mouse position if the pointer is still
 *	inside a Tk window.
 *
 * Results:
 *	Returns 1 if enter/leave events were generated.
 *
 * Side effects:
 *	May generate enter/leave events and schedule a timer.
 *
 *----------------------------------------------------------------------
 */

int
UpdateMousePosition(hwnd, winPtr, x, y)
    HWND hwnd;			/* current mouse window */
    TkWindow *winPtr;		/* current Tk window (or NULL)  */
    long x;			/* current mouse position in */
    long y;			/* root coordinates */
{
    int crossed = 0;		/* 1 if mouse crossed a window boundary */
    TkWindow *cursorWinPtr;

    if (winPtr != lastMouseWinPtr) {
	if (restrictWinPtr) {
	    int newPos, oldPos;

	    newPos = TkPositionInTree(winPtr, restrictWinPtr);
	    oldPos = TkPositionInTree(lastMouseWinPtr, restrictWinPtr);

	    /*
	     * Check if the mouse crossed into or out of the restrict
	     * window.  If so, we need to generate an Enter or Leave event.
	     */

	    if ((newPos != oldPos) && ((newPos == TK_GRAB_IN_TREE)
		    || (oldPos == TK_GRAB_IN_TREE))) {
		XEvent event;

		InitializeCrossingEvent(&event, restrictWinPtr, x, y);
		if (newPos == TK_GRAB_IN_TREE) {
		    event.type = EnterNotify;
		} else {
		    event.type = LeaveNotify;
		}
		if ((oldPos == TK_GRAB_ANCESTOR)
			|| (newPos == TK_GRAB_ANCESTOR)) {
		    event.xcrossing.detail = NotifyAncestor;
		} else {
		    event.xcrossing.detail = NotifyVirtual;
		}
		TkChangeEventWindow(&event, restrictWinPtr);
		Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
	    }

	} else {
	    TkWindow *targetPtr;

	    if ((lastMouseWinPtr == NULL)
		|| (lastMouseWinPtr->window == None)) {
		targetPtr = winPtr;
	    } else {
		targetPtr = lastMouseWinPtr;
	    }

	    if (targetPtr && (targetPtr->window != None)) {
		XEvent event;

		/*
		 * Generate appropriate Enter/Leave events.
		 */

		InitializeCrossingEvent(&event, targetPtr, x, y);

		TkInOutEvents(&event, lastMouseWinPtr, winPtr, LeaveNotify,
			EnterNotify, TCL_QUEUE_TAIL);

		if (TkPositionInTree(winPtr, grabWinPtr) == TK_GRAB_IN_TREE) {
		    cursorWinPtr = winPtr;
		} else {
		    cursorWinPtr = grabWinPtr;
		}
		crossed = 1;
	    }
	}
	lastMouseWinPtr = winPtr;
    }

    /*
     * Make sure the cursor reflects the current mouse position.
     */

    if (restrictWinPtr) {
	cursorWinPtr = restrictWinPtr;
    } else if (grabWinPtr) {
	cursorWinPtr = (TkPositionInTree(winPtr, grabWinPtr)
		== TK_GRAB_IN_TREE) ? winPtr : grabWinPtr;
    } else {
	cursorWinPtr = winPtr;
    }
    TkWinUpdateCursor(cursorWinPtr);

    lastMouseWindow = hwnd;
    lastMousePos.x = x;
    lastMousePos.y = y;

    /*
     * Ensure the mouse timer is set if we are still inside a Tk window.
     */

    if (winPtr != NULL && !mouseTimerSet) {
	mouseTimerSet = 1;
	mouseTimer = Tcl_CreateTimerHandler(MOUSE_TIMER_INTERVAL,
		MouseTimerProc, NULL);
    }

    return crossed;
}

/*
 *----------------------------------------------------------------------
 *
 * MouseTimerProc --
 *
 *	Check the current mouse position and look for enter/leave 
 *	events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May schedule a new timer and/or generate enter/leave events.
 *
 *----------------------------------------------------------------------
 */

void
MouseTimerProc(clientData)
    ClientData clientData;
{
    POINT pos;
    HWND hwnd;
    TkWinDrawable *twdPtr;
    TkWindow *winPtr;

    mouseTimerSet = 0;

    /*
     * Get the current mouse position and window.  Don't do anything
     * if the mouse hasn't moved since the last time we looked.
     */

    GetCursorPos(&pos);
    if (pos.x == lastMousePos.y && pos.y == lastMousePos.y) {
	hwnd = lastMouseWindow;
    } else {
	hwnd = WindowFromPoint(pos);
    }

    /*
     * Check to see if the current window is managed by Tk.
     */

    if (hwnd == lastMouseWindow) {
	winPtr = lastMouseWinPtr;
    } else {
	twdPtr = TkWinGetDrawableFromHandle(hwnd);
	if (twdPtr && (twdPtr->type == TWD_WINDOW)) {
	    winPtr = TkWinGetWinPtr(twdPtr);
	} else {
	    winPtr = NULL;
	}
    }

    /*
     * Generate enter/leave events.
     */

    UpdateMousePosition(hwnd, winPtr, pos.x, pos.y);
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetPointerCoords --
 *
 *	Fetch the position of the mouse pointer.
 *
 * Results:
 *	*xPtr and *yPtr are filled in with the root coordinates
 *	of the mouse pointer for the display.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkGetPointerCoords(tkwin, xPtr, yPtr)
    Tk_Window tkwin;		/* Window that identifies screen on which
				 * lookup is to be done. */
    int *xPtr, *yPtr;		/* Store pointer coordinates here. */
{
    DWORD msgPos;
    POINTS rootPoint;

    msgPos = GetMessagePos();
    rootPoint = MAKEPOINTS(msgPos);
    *xPtr = rootPoint.x;
    *yPtr = rootPoint.y;
}

/*
 *----------------------------------------------------------------------
 *
 * XQueryPointer --
 *
 *	Check the current state of the mouse.  This is not a complete
 *	implementation of this function.  It only computes the root
 *	coordinates and the current mask.
 *
 * Results:
 *	Sets root_x_return, root_y_return, and mask_return.  Returns
 *	true on success.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
XQueryPointer(display, w, root_return, child_return, root_x_return,
	root_y_return, win_x_return, win_y_return, mask_return)
    Display* display;
    Window w;
    Window* root_return;
    Window* child_return;
    int* root_x_return;
    int* root_y_return;
    int* win_x_return;
    int* win_y_return;
    unsigned int* mask_return;
{
    TkGetPointerCoords(NULL, root_x_return, root_y_return);
    *mask_return = TkWinGetModifierState(WM_MOUSEMOVE, 0, 0);    
    return True;
}

/*
 *----------------------------------------------------------------------
 *
 * XGetInputFocus --
 *
 *	Retrieves the current keyboard focus window.
 *
 * Results:
 *	Returns the current focus window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
XGetInputFocus(display, focus_return, revert_to_return)
    Display *display;
    Window *focus_return;
    int *revert_to_return;
{
    HWND hwnd = GetFocus();
    TkWinDrawable *twdPtr = TkWinGetDrawableFromHandle(hwnd);

    /*
     * The focus window may be a Tk window or a window manager decorative
     * frame.
     */

    if (twdPtr) {
	*focus_return = Tk_WindowId(TkWinGetWinPtr(twdPtr));
    } else {
	*focus_return = NULL;
    }
    *revert_to_return = RevertToParent;
}

/*
 *----------------------------------------------------------------------
 *
 * XSetInputFocus --
 *
 *	Set the current focus window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Changes the keyboard focus and causes the selected window to
 *	be activated.
 *
 *----------------------------------------------------------------------
 */

void
XSetInputFocus(display, focus, revert_to, time)
    Display* display;
    Window focus;
    int revert_to;
    Time time;
{
    HWND hwnd = TkWinGetHWND(focus);
    SetFocus(hwnd);
}

/*
 *----------------------------------------------------------------------
 *
 * XDefineCursor --
 *
 *	This function is called to update the cursor on a window.
 *	Since the mouse might be in the specified window, we need to
 *	check the specified window against the current mouse position
 *	and grab state.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May update the cursor.
 *
 *----------------------------------------------------------------------
 */

void
XDefineCursor(display, w, cursor)
    Display* display;
    Window w;
    Cursor cursor;
{
    TkWindow *winPtr = TkWinGetWinPtr(w);

    if (restrictWinPtr) {

	/*
	 * If there is a restrict window, then we only update the cursor
	 * if the restrict window is the window being modified.
	 */

	if (winPtr == restrictWinPtr) {
	    goto update;
	}
    } else if (grabWinPtr) {

	/*
	 * If a grab is in effect, then we only update the cursor if the mouse
	 * pointer is outside the grab tree and the specified window is the
	 * grab window, or the pointer is inside the grab tree and the
	 * specified window is also the pointer window.
	 */

	if (TkPositionInTree(lastMouseWinPtr, grabWinPtr) == TK_GRAB_IN_TREE) {
	    if (winPtr == lastMouseWinPtr) {
		goto update;
	    }
	} else if (winPtr == grabWinPtr) {
	    goto update;
	}
    } else {

	/*
	 * Otherwise, we only update the cursor if the specified window
	 * contains the mouse pointer.
	 */

	if (winPtr == lastMouseWinPtr) {
	    goto update;
	}
    }
    return;

update:
    TkWinUpdateCursor(winPtr);
}


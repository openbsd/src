/* 
 * tkMacWindowMgr.c --
 *
 *	Implements common window manager functions for the Macintosh.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacWindowMgr.c 1.26 96/04/03 15:26:29
 */

#include <Events.h>
#include <Dialogs.h>
#include <EPPC.h>
#include <Windows.h>
#include <ToolUtils.h>
#include <DiskInit.h>
#include <Desk.h>
#include <LowMem.h>

#include "tkInt.h"
#include "tkPort.h"
#include "tkMacInt.h"

#define TK_DEFAULT_ABOUT 128

/*
 * Declarations of static variables used in this file.
 */

static int	 gCaptured = 0;		 /* 1 if mouse events outside of Tk 
				 	  * windows will be reported, else 0 */
static Tk_Window gGrabWinPtr = NULL;     /* Window that defines the top of the
				          * grab tree in a global grab. */
static Tk_Window gKeyboardWinPtr = NULL; /* Current keyboard grab window. */
static Point 	 gLastPointerPos;	 /* Last known position of mouse. */
static Tk_Window gLastWinPtr = NULL;     /* The last window the mouse was in */
static Tk_Window gRestrictWinPtr = NULL; /* Window to which all mouse
				          * events will be reported. */
static RgnHandle gDamageRgn = NULL;	 /* Damage region used for handling
					  * screen updates. */
static int gAppInFront = true;		 /* Boolean variable for determining 
					  * if we are the frontmost app. */
/*
 * Forward declarations of procedures used in this file.
 */

static unsigned int ButtonKeyState _ANSI_ARGS_((void));
static int 	CheckEventsAvail _ANSI_ARGS_((void));
static int 	GenerateActivateEvents _ANSI_ARGS_((EventRecord *macEvent));
static int	GenerateKeyEvent _ANSI_ARGS_((EventRecord *macEvent));
static int	GenerateUpdateEvent _ANSI_ARGS_((EventRecord *macEvent));
static int 	GenerateEnterLeave _ANSI_ARGS_((Tk_Window tkwin,
			long x, int y));
static int 	GenerateMotion _ANSI_ARGS_((Tk_Window tkwin,
			Point whereLocal, Point whereGlobal));
static void 	GenerateUpdates _ANSI_ARGS_((RgnHandle updateRgn,
			TkWindow *winPtr));
static int 	GeneratePollingEvents _ANSI_ARGS_((void));	
static int	WindowManagerMouse _ANSI_ARGS_((EventRecord *theEvent));
static void	InitializeCrossingEvent _ANSI_ARGS_((XEvent *eventPtr,
			TkWindow *winPtr, long x, long y));
static void 	UpdateCursor _ANSI_ARGS_((TkWindow *winPtr,
			WindowRef whichwindow, Point whereLocal));

void TkGenWMMoveRequestEvent(Tk_Window tkwin, int x, int y);


/*
 *----------------------------------------------------------------------
 *
 * WindowManagerMouse --
 *
 *	This function determines if a button event is a "Window Manager"
 *	function or an event that should be passed to Tk's event
 *	queue.
 *
 * Results:
 *	Return true if event was placed on Tk's event queue.
 *
 * Side effects:
 *	Depends on where the button event occurs.
 *
 *----------------------------------------------------------------------
 */

static int
WindowManagerMouse(eventPtr)
    EventRecord *eventPtr;
{
    WindowRef whichWindow, frontWindow;
    Window window;
    Tk_Window tkwin;
    Point where, where2;
				
    
    frontWindow = FrontWindow();

    /* 
     * The window manager only needs to know about mouse down events.
     */
    if (eventPtr->what == mouseUp) {
	return TkGenerateButtonEvent(eventPtr->where.h, eventPtr->where.v, 
		ButtonKeyState());
    }
	
    switch (FindWindow(eventPtr->where, &whichWindow)) {
	case inSysWindow:
	    SystemClick(eventPtr, (GrafPort *) whichWindow);
	    return false;
	case inDrag:
	    if (whichWindow != frontWindow) {
		if (!(eventPtr->modifiers & cmdKey)) {
		    SelectWindow(whichWindow);
		}
	    }
	    
	    SetPort((GrafPort *) whichWindow);
	    where.h = where.v = 0;
	    LocalToGlobal(&where);
	    DragWindow(whichWindow, eventPtr->where,
		    &qd.screenBits.bounds);
			
	    where2.h = where2.v = 0;
	    LocalToGlobal(&where2);
	    if (EqualPt(where, where2)) {
		return false;
	    }

	    window = TkMacGetXWindow(whichWindow);
	    tkwin = Tk_IdToWindow(tkDisplayList->display, window);
	    TkGenWMMoveRequestEvent(tkwin, where2.h, where2.v);
	    return true;
	case inGrow:
	case inContent:
	    if (whichWindow != frontWindow ) {
		SelectWindow(whichWindow);
		SetPort((GrafPort *) whichWindow);
		return false;
	    } else {
		/*
		 * Generally the content region is the domain of Tk
		 * sub-windows.  However, one exception is the grow
		 * region.  A button down in this area will be handled
		 * by the window manager.  Note: this means that Tk 
		 * may not get button down events in this area!
		 */

		if (TkMacGrowToplevel(whichWindow, eventPtr->where) == true) {
		    return true;
		} else {
		    return TkGenerateButtonEvent(eventPtr->where.h,
			    eventPtr->where.v, ButtonKeyState());
		}
	    }
	case inGoAway:
	    if (TrackGoAway( whichWindow, eventPtr->where)) {
		Window window;
		Tk_Window tkwin;

		window = TkMacGetXWindow(whichWindow);
		tkwin = Tk_IdToWindow(tkDisplayList->display, window);
		if (tkwin == NULL) {
		    return false;
		}
		TkGenWMDestroyEvent(tkwin);
		return true;
	    }
	    return false;
	case inMenuBar:
	    {
		KeyMap theKeys;

		GetKeys(theKeys);
		TkMacHandleMenuSelect(MenuSelect(eventPtr->where),
			theKeys[1] & 4);
		return true; /* TODO: may not be on event on queue. */
	    }
	default:
	    return false;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkAboutDlg --
 *
 *	Displays the default Tk About box.  This code uses Macintosh
 *	resources to define the content of the About Box.
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
TkAboutDlg()
{
    DialogPtr aboutDlog;
    short itemHit = -9;
	
    aboutDlog = GetNewDialog(128, NULL, (void*)(-1));
	
    if (!aboutDlog) {
	return;
    }
	
    SelectWindow((WindowRef) aboutDlog);
	
    while (itemHit != 1) {
	ModalDialog( NULL, &itemHit);
    }
    DisposDialog(aboutDlog);
    aboutDlog = NULL;
	
    SelectWindow(FrontWindow());

    return;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateUpdateEvent --
 *
 *	Given a Macintosh update event this function generates all the
 *	X update events needed by Tk.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.  
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

static int
GenerateUpdateEvent(macEvent)
    EventRecord *macEvent;	/* Incoming Mac event */
{
    WindowRef macWindow = (WindowRef)macEvent->message;
    Window window;
    register TkWindow *winPtr;
	
    window = TkMacGetXWindow(macWindow);
    winPtr = (TkWindow *) Tk_IdToWindow(tkDisplayList->display, window);

    if (gDamageRgn == NULL) {
	gDamageRgn = NewRgn();
    }

    /*
     * After the call to BeginUpdate the visable region (visRgn) of the 
     * window is equal to the intersection of the real visable region and
     * the update region for this event.  We use this region in all of our
     * calculations.
     */

    if (winPtr != NULL) {
	BeginUpdate(macWindow);
	GenerateUpdates(macWindow->visRgn, winPtr);
	EndUpdate(macWindow);
	return true;
    }
	
    return false;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateUpdates --
 *
 *	Given a Macintosh update region and a Tk window this function
 *	geneates a X damage event for the window if it is within the
 *	update region.  The function will then recursivly have each
 *	damaged window generate damage events for its child windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

static void
GenerateUpdates(updateRgn, winPtr)
    RgnHandle updateRgn;
    TkWindow *winPtr;
{
    TkWindow *childPtr;
    XEvent event;
    Rect bounds;

    TkMacWinBounds(winPtr, &bounds);
	
    if (bounds.top > (*updateRgn)->rgnBBox.bottom ||
	    (*updateRgn)->rgnBBox.top > bounds.bottom ||
	    bounds.left > (*updateRgn)->rgnBBox.right ||
	    (*updateRgn)->rgnBBox.left > bounds.right ||
	    !RectInRgn(&bounds, updateRgn)) {
	return;
    }

    event.xany.serial = Tk_Display(winPtr)->request;
    event.xany.send_event = false;
    event.xany.window = Tk_WindowId(winPtr);
    event.xany.display = Tk_Display(winPtr);
	
    event.type = Expose;

    /* 
     * Compute the bounding box of the area that the damage occured in.
     */

    /*
     * CopyRgn(TkMacVisableClipRgn(winPtr), rgn);
     * TODO: this call doesn't work doing resizes!!!
     */
    RectRgn(gDamageRgn, &bounds);
    SectRgn(gDamageRgn, updateRgn, gDamageRgn);
    OffsetRgn(gDamageRgn, -bounds.left, -bounds.top);
    event.xexpose.x = (**gDamageRgn).rgnBBox.left;
    event.xexpose.y = (**gDamageRgn).rgnBBox.top;
    event.xexpose.width = (**gDamageRgn).rgnBBox.right -
	(**gDamageRgn).rgnBBox.left;
    event.xexpose.height = (**gDamageRgn).rgnBBox.bottom -
	(**gDamageRgn).rgnBBox.top;
    event.xexpose.count = 0;
    
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);

    for (childPtr = winPtr->childList; childPtr != NULL;
				       childPtr = childPtr->nextPtr) {
	if (!Tk_IsMapped(childPtr) || Tk_IsTopLevel(childPtr)) {
	    continue;
	}

	GenerateUpdates(updateRgn, childPtr);
    }

    return;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGenerateButtonEvent --
 *
 *	Given a global x & y position and the button key status this 
 *	procedure generates the appropiate X button event.  It also 
 *	handles the state changes needed to implement implicit grabs.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue.
 *	Grab state may also change.
 *
 *----------------------------------------------------------------------
 */

int
TkGenerateButtonEvent(x, y, state)
    int x;		/* X location of mouse */
    int y;		/* Y location of mouse */
    unsigned int state;	/* Button Key state suitable for X event */
{
    WindowRef whichWin, frontWin;
    Point where;
    Tk_Window tkwin;
    Window window;
    XEvent event;
    Rect bounds;

    /* 
     * ButtonDown events will always occur in the front
     * window.  ButtonUp events, however, may occur anywhere
     * on the screen.  ButtonUp events should only be sent
     * to Tk if in the front window or during an implicit grab.
     */
    where.h = x;
    where.v = y;
    FindWindow(where, &whichWin);
    frontWin = FrontWindow();
			
    if ((frontWin == NULL) || (frontWin != whichWin && gRestrictWinPtr == NULL)) {
	return false;
    }

    event.xany.send_event = False;
    event.xbutton.x_root = where.h;
    event.xbutton.y_root = where.v;
    event.xbutton.subwindow = None;
    event.xbutton.same_screen = true;

    GlobalToLocal(&where);
    window = TkMacGetXWindow(whichWin);
    tkwin = Tk_IdToWindow(tkDisplayList->display, window);
    
    if (tkwin == NULL) {
    	tkwin = gRestrictWinPtr;
    } else {
	tkwin = Tk_TopCoordsToWindow(tkwin, where.h, where.v, 
	    &event.xbutton.x, &event.xbutton.y);
    }

    if (TkPositionInTree((TkWindow *) tkwin, (TkWindow *) gGrabWinPtr) !=
		TK_GRAB_IN_TREE) {
	    tkwin = gGrabWinPtr;
    }
	
    TkMacWinBounds((TkWindow *) tkwin, &bounds);		
    event.xbutton.x = where.h - bounds.left;
    event.xbutton.y = where.v - bounds.top;

    event.xany.serial = Tk_Display(tkwin)->request;
    event.xany.display = Tk_Display(tkwin);
    event.xbutton.root = XRootWindow(Tk_Display(tkwin), 0);
    event.xbutton.window = Tk_WindowId(tkwin);
    event.xbutton.state = state;
    event.xbutton.button = Button1;

    /*
     * Button events will also start or end an implicit grab.  Do
     * different things wether we generate up or down mouse events.
     */
    if (state & Button1Mask) {
	event.xany.type = ButtonPress;
	/*
	 * Set mouse capture and the restrict window if we are
	 * currently unrestricted.
	 */

	if (!gRestrictWinPtr) {
	    if (!gGrabWinPtr) {
		gRestrictWinPtr = tkwin;
		gCaptured = 1;
	    } else {

		/*
		 * Make sure the new restrict window is inside the
		 * current grab tree.
		 */

		if (TkPositionInTree((TkWindow *) tkwin, (TkWindow *)
			gGrabWinPtr) > 0) {
		    gRestrictWinPtr = tkwin;
		} else {
		    gRestrictWinPtr = gGrabWinPtr;
		}
		gCaptured = 1;
	    }
	}
    } else {
	event.xany.type = ButtonRelease;

	/*
	 * The function ButtonKeyState which is used to passed the
	 * state into this function will have incorrect information
	 * during a ButtonRelease event.  It will report that no
	 * button is depressed - which is true.  However, during a
	 * ButtonRelease event the state needs to include the button
	 * that *was* depressed.  There for we need to set the
	 * Button1Mask here.
	 */
	event.xbutton.state |= Button1Mask;
	
	/*
	 * Release the mouse capture when the last button is
	 * released and we aren't in a global grab.
	 */

	if (gGrabWinPtr == NULL) {
	    gCaptured = 0;
	}
		
	/*
	 * If we are releasing a restrict window, then we need
	 * to send the button event followed by mouse motion from
	 * the restrict window the the current mouse position.
	 */

	if (gRestrictWinPtr) {
	    if (Tk_WindowId(gRestrictWinPtr) != event.xbutton.window) {
		TkChangeEventWindow(&event, (TkWindow *) gRestrictWinPtr);
	    }
	    gLastWinPtr = gRestrictWinPtr;
	    gRestrictWinPtr = NULL;
	}
	
    }

    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    return true;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateActivateEvents --
 *
 *	Generate Activate/Deactivate and FocusIn/FocusOut events from
 *	a Macintosh Activate event.  Note, the activate-on-foreground
 *	bit must be set in the SIZE flags to ensure we get 
 *	Activate/Deactivate in addition to Susspend/Resume events.
 *
 * Results:
 *	Returns true if events were generate.
 *
 * Side effects:
 *	Queue events on Tk's event queue.
 *
 *----------------------------------------------------------------------
 */

static int
GenerateActivateEvents(macEvent)
    EventRecord *macEvent;	/* Incoming Mac event */
{
    XEvent event;
    Tk_Window tkwin;
    Window window;

    window = TkMacGetXWindow((WindowRef) macEvent->message);
    tkwin = Tk_IdToWindow(tkDisplayList->display, window);
    if (tkwin == NULL) {
	return false;
    }

    /* 
     * Generate Activate and Deactivate events.  This event
     * is sent to every subwindow in a toplevel window.
     */
    if (macEvent->modifiers & activeFlag) {
	event.xany.type = ActivateNotify;
    } else {
	event.xany.type = DeactivateNotify;
    }

    event.xany.serial = tkDisplayList->display->request;
    event.xany.send_event = False;
    event.xany.display = tkDisplayList->display;
    event.xany.window = window;

    TkQueueEventForAllChildren(tkwin, &event);

    /* 
     * Generate FocusIn and FocusOut events.  This event
     * is only sent to the toplevel window.
     */
    if (macEvent->modifiers & activeFlag) {
	event.xany.type = FocusIn;
    } else {
	event.xany.type = FocusOut;
    }

    event.xany.serial = tkDisplayList->display->request;
    event.xany.send_event = False;
    event.xfocus.display = tkDisplayList->display;
    event.xfocus.window = window;
    event.xfocus.mode = NotifyNormal;
    event.xfocus.detail = NotifyDetailNone;

    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    return true;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateKeyEvent --
 *
 *	Given Macintosh keyUp, keyDown & autoKey events this function
 *	generates the appropiate X key events.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

static int
GenerateKeyEvent(macEvent)
    EventRecord *macEvent;	/* Incoming Mac event */
{
    WindowRef frontWin;
    Window window;
    Point where;
    Tk_Window tkwin;
    XEvent event;

    frontWin = FrontWindow();

    /* TODO: it doesn't look like we handle Keyboard grabs. */
    window = TkMacGetXWindow(frontWin);
    tkwin = Tk_IdToWindow(tkDisplayList->display, window);
    if (tkwin == NULL) {
	return false;
    }
    where.v = macEvent->where.v;
    where.h = macEvent->where.h;

    event.xany.send_event = False;
    event.xkey.same_screen = true;
    event.xkey.subwindow = None;
    event.xkey.time = GenerateTime();
    event.xkey.x_root = where.h;
    event.xkey.y_root = where.v;

    GlobalToLocal(&where);
    tkwin = Tk_TopCoordsToWindow(tkwin, where.h, where.v, 
	    &event.xkey.x, &event.xkey.y);
    event.xkey.keycode = macEvent->message;

    event.xany.serial = Tk_Display(tkwin)->request;
    event.xkey.window = Tk_WindowId(tkwin);
    event.xkey.display = Tk_Display(tkwin);
    event.xkey.root = XRootWindow(Tk_Display(tkwin), 0);
    event.xkey.state = ButtonKeyState();

    if (macEvent->what == keyDown) {
	event.xany.type = KeyPress;
	Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    } else if (macEvent->what == keyUp) {
	event.xany.type = KeyRelease;
	Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    } else {
	/*
	 * Autokey events send multiple XKey events.
	 *
	 * Note: the last KeyRelease will always be missed with
	 * this scheme.  However, most Tk scripts don't look for
	 * KeyUp events so we should be OK.
	 */
	event.xany.type = KeyRelease;
	Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
	event.xany.type = KeyPress;
	Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
    }
    return true;
}

/*
 *----------------------------------------------------------------------
 *
 * GeneratePollingEvents --
 *
 *	This function polls the mouse position and generates X Motion,
 *	Enter & Leave events.  The cursor is also updated at this
 *	time.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.  
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue.
 *	The cursor may be changed.
 *
 *----------------------------------------------------------------------
 */

static int
GeneratePollingEvents()
{
    Tk_Window tkwin, rootwin;
    Window window;
    WindowRef whichwindow, frontWin;
    Point whereLocal, whereGlobal;
    Boolean inContentRgn;
    short part;
    int local_x, local_y;
    int generatedEvents = false;
    
    /*
     * First we get the current mouse position and determine
     * what Tk window the mouse is over (if any).
     */
    frontWin = FrontWindow();
    if (frontWin == NULL) {
	return false;
    }
    SetPort((GrafPort *) frontWin);
   
    GetMouse(&whereLocal);
    whereGlobal = whereLocal;
    LocalToGlobal(&whereGlobal);
	
    part = FindWindow(whereGlobal, &whichwindow);
    inContentRgn = (part == inContent || part == inGrow);

    if ((frontWin != whichwindow) || !inContentRgn) {
	tkwin = NULL;
    } else {
	window = TkMacGetXWindow(whichwindow);
	rootwin = Tk_IdToWindow(tkDisplayList->display, window);
	if (rootwin == NULL) {
	    tkwin = NULL;
	} else {
	    tkwin = Tk_TopCoordsToWindow(rootwin, whereLocal.h, whereLocal.v, 
		    &local_x, &local_y);
	}
    }
     
    /* 
     * Then check to see if the window the mouse is positioned
     * over has changed.  Generate enter and leave events if it
     * has.  We can also generate the cursor at this time.
     */

    generatedEvents = GenerateEnterLeave(tkwin, whereGlobal.h, whereGlobal.v);
    UpdateCursor((TkWindow *) gLastWinPtr, whichwindow, whereLocal);
    
    /*
     * Now we generate motion events - assuming the mouse moved.
     */

    if (EqualPt(gLastPointerPos, whereGlobal)) {
	return generatedEvents;
    } else {
	generatedEvents |= GenerateMotion(tkwin, whereLocal, whereGlobal);
	gLastPointerPos = whereGlobal;
    }

    return generatedEvents;
}

/*
 *----------------------------------------------------------------------
 *
 * ButtonKeyState --
 *
 *	Returns the current state of the button & modifier keys.
 *
 * Results:
 *	A bitwise inclusive OR of a subset of the following:
 *	Button1Mask, ShiftMask, LockMask, ControlMask, Mod?Mask,
 *	Mod?Mask.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static unsigned int
ButtonKeyState()
{
    unsigned int state = 0;
    KeyMap theKeys;

    if (Button()) {
	state |= Button1Mask;
    }

    GetKeys(theKeys);

    if (theKeys[1] & 2) {
	state |= LockMask;
    }

    if (theKeys[1] & 1) {
	state |= ShiftMask;
    }

    if (theKeys[1] & 8) {
	state |= ControlMask;
    }

    if (theKeys[1] & 32768) {
	state |= Mod1Mask;		/* command key */
    }

    if (theKeys[1] & 4) {
	state |= Mod2Mask;		/* option key */
    }

    return state;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateCursor --
 *
 *	This function updates the cursor based on the current window the
 *  	the cursor is over.  It also generates the resize cursor for the
 *  	resize region in the lower right hand corner of the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor may change shape.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateCursor(winPtr, whichwindow, whereLocal)
    TkWindow *winPtr;
    WindowRef whichwindow;
    Point whereLocal;
{
    WindowRef frontWin;
    CursHandle cursor;

    if (gAppInFront == false) {
	return;
    }
    
    /*
     * The cursor will change during an implicit grab only under
     * a few special cases - such as bindings.
     */
    if (gRestrictWinPtr != NULL) {
	if (TkPositionInTree(winPtr, (TkWindow *) gRestrictWinPtr) ==
		TK_GRAB_IN_TREE) {
	    TkUpdateCursor(winPtr);
	} else {
	    TkUpdateCursor((TkWindow *) gRestrictWinPtr);
	}
	return;
    }

    /*
     * The cursor should be the arrow if outside the active window.
     */
    frontWin = FrontWindow();
    if (frontWin != whichwindow) {	
	TkUpdateCursor(NULL);
	return;
    }

    /*
     * One special case is the grow region.  Because a Tk window may
     * not have allocated space for the grow region the grow region
     * floats above the rest of the Tk window.  This is shown by
     * changing the cursor over the grow region.  This is not needed
     * if the window is not resizable or a scrollbar is growing the
     * drag region for the window.
     */
    if ((gGrabWinPtr == NULL) && TkMacResizable(winPtr) && 
	    (TkMacGetScrollbarGrowWindow(winPtr) == NULL)) {
	if (whereLocal.h > (whichwindow->portRect.right - 16) &&
		whereLocal.v > (whichwindow->portRect.bottom - 16)) {
	    cursor = (CursHandle) GetNamedResource('CURS', "\presize");
	    SetCursor(*cursor);
	    return;
	}
    }

    /*
     * Set the cursor to the value set for the given window.  If
     * the value is None - set to the arrow cursor
     */
    TkUpdateCursor(winPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateMotion --
 *
 *	Given a Tk window and the current mouse position this
 *	function will generate the appropiate X Motion events.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.  
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

static int
GenerateMotion(tkwin, whereLocal, whereGlobal)
    Tk_Window tkwin;		/* current Tk window (or NULL)  */
    Point whereLocal;		/* current mouse position local coords */
    Point whereGlobal;		/* current mouse position global coords */
{
    XEvent event;
    int local_x, local_y;
    Rect bounds;
    
    /* 
     * Mouse moved events generated only when mouse is in the
     * content of the front window.  We also need to take into
     * account any grabs that may be in effect.
     */
     
    if (gRestrictWinPtr) {
	tkwin = gRestrictWinPtr;
    } else if (gGrabWinPtr && !tkwin) {
	tkwin = gGrabWinPtr;
    }

    if (tkwin == NULL) {
	return false;
    }
    
    TkMacWinBounds((TkWindow *) tkwin, &bounds);		
    local_x = whereLocal.h - bounds.left;
    local_y = whereLocal.v - bounds.top;

    event.xany.type = MotionNotify;
    event.xany.serial = Tk_Display(tkwin)->request;
    event.xany.send_event = False;
    event.xany.display = Tk_Display(tkwin);
    event.xmotion.window = Tk_WindowId(tkwin);
    event.xmotion.root = XRootWindow(Tk_Display(tkwin), 0);
    event.xmotion.state = ButtonKeyState();
    event.xmotion.subwindow = None;
    event.xmotion.time = GenerateTime();
    event.xmotion.x_root = whereGlobal.h;
    event.xmotion.y_root = whereGlobal.v;
    event.xmotion.x = local_x;
    event.xmotion.y = local_y;
    event.xmotion.same_screen = true;
    event.xmotion.is_hint = NotifyNormal;
    Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);

    return true;
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
    eventPtr->xcrossing.serial = Tk_Display(winPtr)->request;
    eventPtr->xcrossing.send_event = 0;
    eventPtr->xcrossing.display = winPtr->display;
    eventPtr->xcrossing.root = RootWindow(winPtr->display,
	    winPtr->screenNum);
    eventPtr->xcrossing.time = GenerateTime();
    eventPtr->xcrossing.x_root = x;
    eventPtr->xcrossing.y_root = y;
    eventPtr->xcrossing.state = ButtonKeyState();
    eventPtr->xcrossing.mode = NotifyNormal;
    eventPtr->xcrossing.focus = False;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateEnterLeave --
 *
 *	Given a Tk window and the current mouse position this
 *	function will generate the appropiate X Enter and Leave
 *	events.
 *
 * Results:
 *	True if event(s) are generated - false otherwise.  
 *
 * Side effects:
 *	Additional events may be place on the Tk event queue.
 *
 *----------------------------------------------------------------------
 */

static int
GenerateEnterLeave(tkwin, x, y)
    Tk_Window tkwin;		/* current Tk window (or NULL)  */
    long x;			/* current mouse position in */
    long y;			/* root coordinates */
{
    int crossed = 0;		/* 1 if mouse crossed a window boundary */

    if (tkwin != gLastWinPtr) {
	if (gRestrictWinPtr) {
	    int newPos, oldPos;

	    newPos = TkPositionInTree((TkWindow *) tkwin, (TkWindow *) gRestrictWinPtr);
	    oldPos = TkPositionInTree((TkWindow *) gLastWinPtr, (TkWindow *) gRestrictWinPtr);

	    /*
	     * Check if the mouse crossed into or out of the restrict
	     * window.  If so, we need to generate an Enter or Leave event.
	     */

	    if ((newPos != oldPos) && ((newPos == TK_GRAB_IN_TREE)
		    || (oldPos == TK_GRAB_IN_TREE))) {
		XEvent event;

		InitializeCrossingEvent(&event, gRestrictWinPtr, x, y);
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
		TkChangeEventWindow(&event, (TkWindow *) gRestrictWinPtr);
		Tk_QueueWindowEvent(&event, TCL_QUEUE_TAIL);
	    }

	} else {
	    Tk_Window targetPtr;

	    if ((gLastWinPtr == NULL)
		|| (Tk_WindowId(gLastWinPtr) == None)) {
		targetPtr = tkwin;
	    } else {
		targetPtr = gLastWinPtr;
	    }

	    if (targetPtr && (Tk_WindowId(targetPtr) != None)) {
		XEvent event;

		/*
		 * Generate appropriate Enter/Leave events.
		 */

		InitializeCrossingEvent(&event, targetPtr, x, y);

		TkInOutEvents(&event, (TkWindow *) gLastWinPtr, (TkWindow *) tkwin, LeaveNotify,
			EnterNotify, TCL_QUEUE_TAIL);

		crossed = 1;
	    }
	}
	gLastWinPtr = tkwin;
    }

    return crossed;
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
    gCaptured = 1;
    gGrabWinPtr = Tk_IdToWindow(display, grab_window);
    gRestrictWinPtr = NULL;
    if (TkPositionInTree((TkWindow *) gLastWinPtr, (TkWindow *) gGrabWinPtr) !=
	    TK_GRAB_IN_TREE) {
	TkUpdateCursor((TkWindow *) gGrabWinPtr);
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
    gCaptured = 0;
    gGrabWinPtr = NULL;
    gRestrictWinPtr = NULL;
    TkUpdateCursor((TkWindow *) gLastWinPtr);
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
    gKeyboardWinPtr = Tk_IdToWindow(display, grab_window);
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
    gKeyboardWinPtr = NULL;
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
    Point where;

    GetMouse(&where);
    LocalToGlobal(&where);
    *root_x_return = where.h;
    *root_y_return = where.v;
    *mask_return = ButtonKeyState();    
    return True;
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateTime --
 *
 *	Returns the total number of ticks from startup  This function
 *	is used to generate the time of generated X events.
 *
 * Results:
 *	Returns the current time (ticks from startup).
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Time
GenerateTime()
{
    return (Time) LMGetTicks();
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacPointerDeadWindow --
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
TkMacPointerDeadWindow(winPtr)
    TkWindow *winPtr;
{
    if ((Tk_Window) winPtr == gLastWinPtr) {
	gLastWinPtr = NULL;
    }
    if ((Tk_Window) winPtr == gGrabWinPtr) {
	gGrabWinPtr = NULL;
    }
    if ((Tk_Window) winPtr == gRestrictWinPtr) {
	gRestrictWinPtr = NULL;
    }
    if (!(gRestrictWinPtr || gGrabWinPtr)) {
	gCaptured = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMacConvertEvent --
 *
 *	This function converts a Macintosh event into zero or more
 *	Tcl events.
 *
 * Results:
 *	Returns 1 if event added to Tcl queue, 0 otherwse.
 *
 * Side effects:
 *	May add events to Tcl's event queue.
 *
 *----------------------------------------------------------------------
 */

int
TkMacConvertEvent(eventPtr)
    EventRecord *eventPtr;
{
    int eventFound = false;
    
    switch (eventPtr->what) {
	case nullEvent:
	    if (GeneratePollingEvents()) {
		eventFound = true;
	    }
	    break;
	case updateEvt:
	    if (GenerateUpdateEvent(eventPtr)) {
		eventFound = true;
	    }
	    break;
	case mouseDown:
	case mouseUp:
	    if (WindowManagerMouse(eventPtr)) {
		eventFound = true;
	    }
	    break;
	case autoKey:
	case keyDown:
	    /*
	     * Handle menu-key events here.  If it is *not*
	     * a menu key - just fall through to handle as a
	     * normal key event.
	     */
	    if ((eventPtr->modifiers & cmdKey) == cmdKey) {
		long menuResult = MenuKey(eventPtr->message & charCodeMask);
		
		if (HiWord(menuResult) != 0) {
		    TkMacHandleMenuSelect(menuResult, false);
		    break;
		}
	    }
	case keyUp:
	    eventFound |= GenerateKeyEvent(eventPtr);
	    break;
	case activateEvt:
	    eventFound |= GenerateActivateEvents(eventPtr);
	    break;
	case kHighLevelEvent:
	    TkMacDoHLEvent(eventPtr);
	    /* TODO: should return true if events were placed on event queue. */
	    break;
	case osEvt:
	    /*
	     * Do clipboard conversion.
	     */
	    switch ((eventPtr->message & osEvtMessageMask) >> 24) {
		case mouseMovedMessage:
		    if (GeneratePollingEvents()) {
			eventFound = true;
		    }
		    break;
		case suspendResumeMessage:
		    if (eventPtr->message & resumeFlag) {
			if (eventPtr->message & convertClipboardFlag) {
			    TkResumeClipboard();
			}
		    } else {
			TkSuspendClipboard();
		    }
		    gAppInFront = (eventPtr->message & resumeFlag);
		    break;
	    }
	    break;
	case diskEvt:
	    /* 
	     * Disk insertion. 
	     */
	    if (HiWord(eventPtr->message) != noErr) {
		Point pt;
			
		DILoad();
		pt.v = pt.h = 120;	  /* parameter ignored in sys 7 */
		DIBadMount(pt, eventPtr->message);
		DIUnload();
	    }
	    break;
    }
    
    return eventFound;
}

/*
 *----------------------------------------------------------------------
 *
 * CheckEventsAvail --
 *
 *	Checks to see if events are available on the Macintosh queue.
 *	This function looks for both queued events (eg. key & button)
 *	and generated events (update).
 *
 * Results:
 *	True is events exist, false otherwise.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
CheckEventsAvail()
{
    QHdrPtr evPtr;
    WindowPeek macWinPtr;
    
    evPtr = GetEvQHdr();
    if (evPtr->qHead != NULL) {
	return true;
    }
    
    macWinPtr = (WindowPeek) FrontWindow();
    while (macWinPtr != NULL) {
	if (!EmptyRgn(macWinPtr->updateRgn)) {
	    return true;
	}
	macWinPtr = macWinPtr->nextWindow;
    }
    return false;
}


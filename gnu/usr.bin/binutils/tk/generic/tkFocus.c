/* 
 * tkFocus.c --
 *
 *	This file contains procedures that manage the input
 *	focus for Tk.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkFocus.c 1.27 96/02/15 18:53:29
 */

#include "tkInt.h"
#include "tkPort.h"

/*
 * For each top-level window that has ever received the focus, there
 * is a record of the following type:
 */

typedef struct TkFocusInfo {
    TkWindow *topLevelPtr;	/* Information about top-level window. */
    TkWindow *focusWinPtr;	/* The next time the focus comes to this
				 * top-level, it will be given to this
				 * window. */
    struct TkFocusInfo *nextPtr;/* Next in list of all focus records for
				 * a given application. */
} FocusInfo;

static int focusDebug = 0;

/*
 * The following magic value is stored in the "send_event" field of
 * FocusIn and FocusOut events that are generated in this file.  This
 * allows us to separate "real" events coming from the server from
 * those that we generated.
 */

#define GENERATED_EVENT_MAGIC ((Bool) 0x547321ac)

/*
 * Forward declarations for procedures defined in this file:
 */


static void		ChangeXFocus _ANSI_ARGS_((TkWindow *topLevelPtr,
			    int focus));
static void		FocusMapProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		GenerateFocusEvents _ANSI_ARGS_((TkWindow *sourcePtr,
			    TkWindow *destPtr));
static void		SetFocus _ANSI_ARGS_((TkWindow *winPtr, int force));

/*
 *--------------------------------------------------------------
 *
 * Tk_FocusCmd --
 *
 *	This procedure is invoked to process the "focus" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
Tk_FocusCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr = (TkWindow *) clientData;
    TkWindow *newPtr, *focusWinPtr, *topLevelPtr;
    FocusInfo *focusPtr;
    char c;
    size_t length;

    /*
     * If invoked with no arguments, just return the current focus window.
     */

    if (argc == 1) {
	focusWinPtr = TkGetFocus(winPtr);
	if (focusWinPtr != NULL) {
	    interp->result = focusWinPtr->pathName;
	}
	return TCL_OK;
    }

    /*
     * If invoked with a single argument beginning with "." then focus
     * on that window.
     */

    if (argc == 2) {
	if (argv[1][0] == 0) {
	    return TCL_OK;
	}
	if (argv[1][0] == '.') {
	    newPtr = (TkWindow *) Tk_NameToWindow(interp, argv[1], tkwin);
	    if (newPtr == NULL) {
		return TCL_ERROR;
	    }
	    if (!(newPtr->flags & TK_ALREADY_DEAD)) {
		SetFocus(newPtr, 0);
	    }
	    return TCL_OK;
	}
    }

    length = strlen(argv[1]);
    c = argv[1][1];
    if ((c == 'd') && (strncmp(argv[1], "-displayof", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " -displayof window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	newPtr = (TkWindow *) Tk_NameToWindow(interp, argv[2], tkwin);
	if (newPtr == NULL) {
	    return TCL_ERROR;
	}
	newPtr = TkGetFocus(newPtr);
	if (newPtr != NULL) {
	    interp->result = newPtr->pathName;
	}
    } else if ((c == 'f') && (strncmp(argv[1], "-force", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " -force window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argv[2][0] == 0) {
	    return TCL_OK;
	}
	newPtr = (TkWindow *) Tk_NameToWindow(interp, argv[2], tkwin);
	if (newPtr == NULL) {
	    return TCL_ERROR;
	}
	SetFocus(newPtr, 1);
    } else if ((c == 'l') && (strncmp(argv[1], "-lastfor", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " -lastfor window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	newPtr = (TkWindow *) Tk_NameToWindow(interp, argv[2], tkwin);
	if (newPtr == NULL) {
	    return TCL_ERROR;
	}
	for (topLevelPtr = newPtr; topLevelPtr != NULL;
		topLevelPtr = topLevelPtr->parentPtr)  {
	    if (topLevelPtr->flags & TK_TOP_LEVEL) {
		for (focusPtr = newPtr->mainPtr->focusPtr; focusPtr != NULL;
			focusPtr = focusPtr->nextPtr) {
		    if (focusPtr->topLevelPtr == topLevelPtr) {
			interp->result = focusPtr->focusWinPtr->pathName;
			return TCL_OK;
		    }
		}
		interp->result = topLevelPtr->pathName;
		return TCL_OK;
	    }
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be -displayof, -force, or -lastfor", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TkFocusFilterEvent --
 *
 *	This procedure is invoked by Tk_HandleEvent when it encounters
 *	a FocusIn, FocusOut, Enter, or Leave event.
 *
 * Results:
 *	A return value of 1 means that Tk_HandleEvent should process
 *	the event normally (i.e. event handlers should be invoked).
 *	A return value of 0 means that this event should be ignored.
 *
 * Side effects:
 *	Additional events may be generated, and the focus may switch.
 *
 *--------------------------------------------------------------
 */

int
TkFocusFilterEvent(winPtr, eventPtr)
    TkWindow *winPtr;		/* Window that focus event is directed to. */
    XEvent *eventPtr;		/* FocusIn or FocusOut event. */
{
    /*
     * Design notes: the window manager and X server work together to
     * transfer the focus among top-level windows.  This procedure takes
     * care of transferring the focus from a top-level window to the
     * actual window within that top-level that has the focus.  We
     * do this by synthesizing X events to move the focus around.  None
     * of the FocusIn and FocusOut events generated by X are ever used
     * outside of this procedure;  only the synthesized events get through
     * to the rest of the application.  At one point (e.g. Tk4.0b1) Tk
     * used to call X to move the focus from a top-level to one of its
     * descendants, then just pass through the events generated by X.
     * This approach didn't work very well, for a variety of reasons.
     * For example, if X generates the events they go at the back of
     * the event queue, which could cause problems if other things
     * have already happened, such as moving the focus to yet another
     * window.
     */

    FocusInfo *focusPtr;
    TkDisplay *dispPtr = winPtr->dispPtr;
    TkWindow *newFocusPtr;
    int retValue, delta;

    /*
     * If this was a generated event, just turn off the generated
     * flag and pass the event through.
     */

    if (eventPtr->xfocus.send_event == GENERATED_EVENT_MAGIC) {
	eventPtr->xfocus.send_event = 0;
	return 1;
    }

    /*
     * This was not a generated event.  We'll return 1 (so that the
     * event will be processed) if it's an Enter or Leave event, and
     * 0 (so that the event won't be processed) if it's a FocusIn or
     * FocusOut event.  Also, skip NotifyPointer, NotifyPointerRoot,
     * and NotifyInferior focus events immediately; they're not
     * useful and tend to cause confusion.
     */

    if ((eventPtr->type == FocusIn) || (eventPtr->type == FocusOut)) {
	retValue = 0;
	if ((eventPtr->xfocus.detail == NotifyPointer)
		|| (eventPtr->xfocus.detail == NotifyPointerRoot)
		|| (eventPtr->xfocus.detail == NotifyInferior)) {
	    return retValue;
	}
    } else {
	retValue = 1;
	if (eventPtr->xcrossing.detail == NotifyInferior) {
	    return retValue;
	}
    }

    /*
     * If winPtr isn't a top-level window than just ignore the event.
     */

    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	return retValue;
    }

    /*
     * If there is a grab in effect and this window is outside the
     * grabbed tree, then ignore the event.
     */

    if (TkGrabState(winPtr) == TK_GRAB_EXCLUDED)  {
	return retValue;
    }

    /*
     * Find the FocusInfo structure for the window, and make a new one
     * if there isn't one already.
     */

    for (focusPtr = winPtr->mainPtr->focusPtr; focusPtr != NULL;
	    focusPtr = focusPtr->nextPtr) {
	if (focusPtr->topLevelPtr == winPtr) {
	    break;
	}
    }
    if (focusPtr == NULL) {
	focusPtr = (FocusInfo *) ckalloc(sizeof(FocusInfo));
	focusPtr->topLevelPtr = focusPtr->focusWinPtr = winPtr;
	focusPtr->nextPtr = winPtr->mainPtr->focusPtr;
	winPtr->mainPtr->focusPtr = focusPtr;
    }

    /*
     * It is possible that there were outstanding FocusIn and FocusOut
     * events on their way to us at the time the focus was changed
     * internally with the "focus" command.  If so, these events could
     * potentially cause us to lose the focus (switch it to the window
     * of the last FocusIn event) even though the focus change occurred
     * after those events.  The following code detects this and puts
     * the focus back to the place where it was rightfully set.
     */

    newFocusPtr = focusPtr->focusWinPtr;
    delta = eventPtr->xfocus.serial - winPtr->mainPtr->focusSerial;
    if (focusDebug) {
	printf("check event serial %d, delta %d\n",
		(int) eventPtr->xfocus.serial, delta);
    }
    if ((delta < 0) && (winPtr->mainPtr->lastFocusPtr != NULL)) {
	newFocusPtr = winPtr->mainPtr->lastFocusPtr;
	if (focusDebug) {
	    printf("reverting to %s instead of %s\n", newFocusPtr->pathName,
		    focusPtr->focusWinPtr->pathName);
	}
    }

    if (eventPtr->type == FocusIn) {
	GenerateFocusEvents(dispPtr->focusWinPtr, newFocusPtr);
	dispPtr->focusWinPtr = newFocusPtr;
	dispPtr->implicitWinPtr = NULL;
	if (focusDebug) {
	    printf("Focussed on %s\n", newFocusPtr->pathName);
	}
    } else if (eventPtr->type == FocusOut) {
	GenerateFocusEvents(dispPtr->focusWinPtr, (TkWindow *) NULL);
	dispPtr->focusWinPtr = NULL;
	dispPtr->implicitWinPtr = NULL;
	if (focusDebug) {
	    printf("Unfocussed from %s, detail %d\n", winPtr->pathName,
		    eventPtr->xfocus.detail);
	}
    } else if (eventPtr->type == EnterNotify) {
	/*
	 * If there is no window manager, or if the window manager isn't
	 * moving the focus around (e.g. the disgusting "NoTitleFocus"
	 * option has been selected in twm), then we won't get FocusIn
	 * or FocusOut events.  Instead, the "focus" field will be set
	 * in an Enter event to indicate that we've already got the focus
	 * when then mouse enters the window (even though we didn't get
	 * a FocusIn event).  Watch for this and grab the focus when it
	 * happens.
	 */

	if (eventPtr->xcrossing.focus && (dispPtr->focusWinPtr == NULL)) {
	    GenerateFocusEvents(dispPtr->focusWinPtr, newFocusPtr);
	    dispPtr->focusWinPtr = newFocusPtr;
	    dispPtr->implicitWinPtr = winPtr;
	    if (focusDebug) {
		printf("Focussed implicitly on %s\n",
			newFocusPtr->pathName);
	    }
	}
    } else if (eventPtr->type == LeaveNotify) {
	/*
	 * If the pointer just left a window for which we automatically
	 * claimed the focus on enter, generate FocusOut events.  Note:
	 * dispPtr->implicitWinPtr may not be the same as
	 * dispPtr->focusWinPtr (e.g. because the "focus" command was
	 * used to redirect the focus after it arrived at
	 * dispPtr->implicitWinPtr)!!
	 */

	if (dispPtr->implicitWinPtr == winPtr) {
	    GenerateFocusEvents(dispPtr->focusWinPtr, (TkWindow *) NULL);
	    dispPtr->focusWinPtr = NULL;
	    dispPtr->implicitWinPtr = NULL;
	    if (focusDebug) {
		printf("Defocussed implicitly\n");
	    }
	}
    }
    return retValue;
}

/*
 *----------------------------------------------------------------------
 *
 * SetFocus --
 *
 *	This procedure is invoked to change the focus window for a
 *	given display in a given application.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Event handlers may be invoked to process the change of
 *	focus.
 *
 *----------------------------------------------------------------------
 */

static void
SetFocus(winPtr, force)
    TkWindow *winPtr;		/* Window that is to be the new focus for
				 * its display and application. */
    int force;			/* If non-zero, set the X focus to this
				 * window even if the application doesn't
				 * currently have the X focus. */
{
    TkDisplay *dispPtr = winPtr->dispPtr;
    FocusInfo *focusPtr;
    TkWindow *topLevelPtr, *topLevelPtr2;

    if (winPtr == dispPtr->focusWinPtr) {
	return;
    }

    /*
     * Find the top-level window for winPtr, then find (or create)
     * a record for the top-level.
     */

    for (topLevelPtr = winPtr; ; topLevelPtr = topLevelPtr->parentPtr)  {
	if (topLevelPtr == NULL) {
	    /*
	     * The window is being deleted.  No point in worrying about
	     * giving it the focus.
	     */

	    return;
	}
	if (topLevelPtr->flags & TK_TOP_LEVEL) {
	    break;
	}
    }
    for (focusPtr = winPtr->mainPtr->focusPtr; focusPtr != NULL;
	    focusPtr = focusPtr->nextPtr) {
	if (focusPtr->topLevelPtr == topLevelPtr) {
	    break;
	}
    }
    if (focusPtr == NULL) {
	focusPtr = (FocusInfo *) ckalloc(sizeof(FocusInfo));
	focusPtr->topLevelPtr = topLevelPtr;
	focusPtr->nextPtr = winPtr->mainPtr->focusPtr;
	winPtr->mainPtr->focusPtr = focusPtr;
    }

    /*
     * Reset the focus, but only if the application already has the
     * input focus or "force" has been specified.
     */

    focusPtr->focusWinPtr = winPtr;
    Tk_MakeWindowExist((Tk_Window) winPtr);
    if (force || ((dispPtr->focusWinPtr != NULL)
	    && (dispPtr->focusWinPtr->mainPtr == winPtr->mainPtr))) {
	/*
	 * Reset the focus in X if it has changed top-levels and if the
	 * new top-level isn't override-redirect (the only reason to
	 * change the X focus is so that the window manager can redecorate
	 * the focus window, but if it's override-redirect then it won't
	 * be decorated anyway;  also, changing the focus to menus causes
	 * all sorts of problems with olvwm:  the focus gets lost if
	 * keyboard traversal is used to move among menus.
	 */

	if (dispPtr->focusWinPtr != NULL) {
	    for (topLevelPtr2 = dispPtr->focusWinPtr;
		    (topLevelPtr2 != NULL)
			&& !(topLevelPtr2->flags & TK_TOP_LEVEL);
		    topLevelPtr2 = topLevelPtr2->parentPtr)  {
		/* Empty loop body. */
	    }
	} else {
	    topLevelPtr2 = NULL;
	}
	if ((topLevelPtr2 != topLevelPtr)
		&& !(topLevelPtr->atts.override_redirect)) {
	    if (dispPtr->focusOnMapPtr != NULL) {
		Tk_DeleteEventHandler((Tk_Window) dispPtr->focusOnMapPtr,
			StructureNotifyMask, FocusMapProc,
			(ClientData) dispPtr->focusOnMapPtr);
		dispPtr->focusOnMapPtr = NULL;
	    }
	    if (topLevelPtr->flags & TK_MAPPED) {
		ChangeXFocus(topLevelPtr, force);
	    } else {
		/*
		 * The window isn't mapped, so we can't give it the focus
		 * right now.  Create an event handler that will give it
		 * the focus as soon as it is mapped.
		 */

		Tk_CreateEventHandler((Tk_Window) topLevelPtr,
			StructureNotifyMask, FocusMapProc,
			(ClientData) topLevelPtr);
		dispPtr->focusOnMapPtr = topLevelPtr;
		dispPtr->forceFocus = force;
	    }
	}
	GenerateFocusEvents(dispPtr->focusWinPtr, winPtr);
	dispPtr->focusWinPtr = winPtr;
    }

    /*
     * Remember the current serial number for the X server and issue
     * a dummy server request.  This marks the position at which we
     * changed the focus, so we can distinguish FocusIn and FocusOut
     * events on either side of the mark.
     */

    winPtr->mainPtr->lastFocusPtr = winPtr;
    winPtr->mainPtr->focusSerial = NextRequest(winPtr->display);
    XNoOp(winPtr->display);
    if (focusDebug) {
	printf("focus marking for %s at %d\n", winPtr->pathName,
		(int) winPtr->mainPtr->focusSerial);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetFocus --
 *
 *	Given a window, this procedure returns the current focus
 *	window for its application and display.
 *
 * Results:
 *	The return value is a pointer to the window that currently
 *	has the input focus for the specified application and
 *	display, or NULL if none.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

TkWindow *
TkGetFocus(winPtr)
    TkWindow *winPtr;		/* Window that selects an application
				 * and a display. */
{
    TkWindow *focusWinPtr;

    focusWinPtr = winPtr->dispPtr->focusWinPtr;
    if ((focusWinPtr != NULL) && (focusWinPtr->mainPtr == winPtr->mainPtr)) {
	return focusWinPtr;
    }
    return (TkWindow *) NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFocusDeadWindow --
 *
 *	This procedure is invoked when it is determined that
 *	a window is dead.  It cleans up focus-related information
 *	about the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Various things get cleaned up and recycled.
 *
 *----------------------------------------------------------------------
 */

void
TkFocusDeadWindow(winPtr)
    register TkWindow *winPtr;		/* Information about the window
					 * that is being deleted. */
{
    FocusInfo *focusPtr, *prevPtr;
    TkDisplay *dispPtr = winPtr->dispPtr;

    /*
     * Search for focus records that refer to this window either as
     * the top-level window or the current focus window.
     */

    for (prevPtr = NULL, focusPtr = winPtr->mainPtr->focusPtr;
	    focusPtr != NULL;
	    prevPtr = focusPtr, focusPtr = focusPtr->nextPtr) {
	if (winPtr == focusPtr->topLevelPtr) {
	    /*
	     * The top-level window is the one being deleted: free
	     * the focus record and release the focus back to PointerRoot
	     * if we acquired it implicitly.
	     */

	    if (dispPtr->implicitWinPtr == winPtr) {
		if (focusDebug) {
		    printf("releasing focus to root after %s died\n",
			    focusPtr->topLevelPtr->pathName);
		}
		dispPtr->implicitWinPtr = NULL;
		dispPtr->focusWinPtr = NULL;
	    }
	    if (dispPtr->focusWinPtr == focusPtr->focusWinPtr) {
		dispPtr->focusWinPtr = NULL;
	    }
	    if (dispPtr->focusOnMapPtr == focusPtr->topLevelPtr) {
		dispPtr->focusOnMapPtr = NULL;
	    }
	    if (prevPtr == NULL) {
		winPtr->mainPtr->focusPtr = focusPtr->nextPtr;
	    } else {
		prevPtr->nextPtr = focusPtr->nextPtr;
	    }
	    ckfree((char *) focusPtr);
	    break;
	} else if (winPtr == focusPtr->focusWinPtr) {
	    /*
	     * The deleted window had the focus for its top-level:
	     * move the focus to the top-level itself.
	     */

	    focusPtr->focusWinPtr = focusPtr->topLevelPtr;
	    if ((dispPtr->focusWinPtr == winPtr)
		    && !(focusPtr->topLevelPtr->flags & TK_ALREADY_DEAD)) {
		if (focusDebug) {
		    printf("forwarding focus to %s after %s died\n",
			    focusPtr->topLevelPtr->pathName, winPtr->pathName);
		}
		GenerateFocusEvents(dispPtr->focusWinPtr,
			focusPtr->topLevelPtr);
		dispPtr->focusWinPtr = focusPtr->topLevelPtr;
	    }
	    break;
	}
    }

    if (winPtr->mainPtr->lastFocusPtr == winPtr) {
	winPtr->mainPtr->lastFocusPtr = NULL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GenerateFocusEvents --
 *
 *	This procedure is called to create FocusIn and FocusOut events to
 *	move the input focus from one window to another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	FocusIn and FocusOut events are generated.
 *
 *----------------------------------------------------------------------
 */

static void
GenerateFocusEvents(sourcePtr, destPtr)
    TkWindow *sourcePtr;	/* Window that used to have the focus (may
				 * be NULL). */
    TkWindow *destPtr;		/* New window to have the focus (may be
				 * NULL). */

{
    XEvent event;
    TkWindow *winPtr;

    winPtr = sourcePtr;
    if (winPtr == NULL) {
	winPtr = destPtr;
	if (winPtr == NULL) {
	    return;
	}
    }

    event.xfocus.serial = LastKnownRequestProcessed(winPtr->display);
    event.xfocus.send_event = GENERATED_EVENT_MAGIC;
    event.xfocus.display = winPtr->display;
    event.xfocus.mode = NotifyNormal;
    TkInOutEvents(&event, sourcePtr, destPtr, FocusOut, FocusIn,
	    TCL_QUEUE_MARK);
}

/*
 *----------------------------------------------------------------------
 *
 * ChangeXFocus --
 *
 *	This procedure is invoked to move the official X focus from
 *	one top-level to another.  We do this when the application
 *	changes the focus window from one top-level to another, in
 *	order to notify the window manager so that it can highlight
 *	the new focus top-level.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The official X focus window changes;  the application's focus
 *	window isn't changed by this procedure.
 *
 *----------------------------------------------------------------------
 */

static void
ChangeXFocus(topLevelPtr, force)
    TkWindow *topLevelPtr;	/* Top-level window that is to receive
				 * the X focus. */
    int force;			/* Non-zero means claim the focus even
				 * if it didn't originally belong to
				 * topLevelPtr's application. */
{
    TkDisplay *dispPtr = topLevelPtr->dispPtr;
    TkWindow *winPtr;
    Window focusWindow;
    int dummy;
    Tk_ErrorHandler errHandler;

    /*
     * If the focus was received implicitly, then there's no advantage
     * in setting an explicit focus;  just return.
     */

    if (dispPtr->implicitWinPtr != NULL) {
	return;
    }

    /*
     * Check to make sure that the focus is still in one of the
     * windows of this application.  Furthermore, grab the server
     * to make sure that the focus doesn't change in the middle
     * of this operation.
     */

    if (!focusDebug) {
	XGrabServer(dispPtr->display);
    }
    if (!force) {
	XGetInputFocus(dispPtr->display, &focusWindow, &dummy);
	winPtr = (TkWindow *) Tk_IdToWindow(dispPtr->display, focusWindow);
	if ((winPtr == NULL) || (winPtr->mainPtr != topLevelPtr->mainPtr)) {
	    goto done;
	}
    }

    /*
     * Tell X to change the focus.  Ignore errors that occur when changing
     * the focus:  it is still possible that the window we're focussing
     * to could have gotten unmapped, which will generate an error.
     */

    errHandler = Tk_CreateErrorHandler(dispPtr->display, -1, -1, -1,
	    (Tk_ErrorProc *) NULL, (ClientData) NULL);
    XSetInputFocus(dispPtr->display, topLevelPtr->window, RevertToParent,
	    CurrentTime);
    Tk_DeleteErrorHandler(errHandler);
    if (focusDebug) {
	printf("Set X focus to %s\n", topLevelPtr->pathName);
    }

    done:
    if (!focusDebug) {
	XUngrabServer(dispPtr->display);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * FocusMapProc --
 *
 *	This procedure is called as an event handler for StructureNotify
 *	events, if a window receives the focus at a time when its
 *	toplevel isn't mapped.  The procedure is needed because X
 *	won't allow the focus to be set to an unmapped window;  we
 *	detect when the toplevel is mapped and set the focus to it then.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If this is a map event, the focus gets set to the toplevel
 *	given by clientData.
 *
 *----------------------------------------------------------------------
 */

static void
FocusMapProc(clientData, eventPtr)
    ClientData clientData;	/* Toplevel window. */
    XEvent *eventPtr;		/* Information about event. */
{
    TkWindow *winPtr = (TkWindow *) clientData;
    TkDisplay *dispPtr = winPtr->dispPtr;

    if (eventPtr->type == MapNotify) {
	ChangeXFocus(winPtr, dispPtr->forceFocus);
	Tk_DeleteEventHandler((Tk_Window) winPtr, StructureNotifyMask,
		FocusMapProc, clientData);
	dispPtr->focusOnMapPtr = NULL;
    }
}

/* 
 * tkEvent.c --
 *
 *	This file provides basic low-level facilities for managing
 *	X events in Tk.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkEvent.c 1.16 96/03/29 17:56:54
 */

#include "tkPort.h"
#include "tkInt.h"
#include <signal.h>

/*
 * There's a potential problem if a handler is deleted while it's
 * current (i.e. its procedure is executing), since Tk_HandleEvent
 * will need to read the handler's "nextPtr" field when the procedure
 * returns.  To handle this problem, structures of the type below
 * indicate the next handler to be processed for any (recursively
 * nested) dispatches in progress.  The nextHandler fields get
 * updated if the handlers pointed to are deleted.  Tk_HandleEvent
 * also needs to know if the entire window gets deleted;  the winPtr
 * field is set to zero if that particular window gets deleted.
 */

typedef struct InProgress {
    XEvent *eventPtr;		 /* Event currently being handled. */
    TkWindow *winPtr;		 /* Window for event.  Gets set to None if
				  * window is deleted while event is being
				  * handled. */
    TkEventHandler *nextHandler; /* Next handler in search. */
    struct InProgress *nextPtr;	 /* Next higher nested search. */
} InProgress;

static InProgress *pendingPtr = NULL;
				/* Topmost search in progress, or
				 * NULL if none. */

/*
 * For each call to Tk_CreateGenericHandler, an instance of the following
 * structure will be created.  All of the active handlers are linked into a
 * list.
 */

typedef struct GenericHandler {
    Tk_GenericProc *proc;	/* Procedure to dispatch on all X events. */
    ClientData clientData;	/* Client data to pass to procedure. */
    int deleteFlag;		/* Flag to set when this handler is deleted. */
    struct GenericHandler *nextPtr;
				/* Next handler in list of all generic
				 * handlers, or NULL for end of list. */
} GenericHandler;

static GenericHandler *genericList = NULL;
				/* First handler in the list, or NULL. */
static GenericHandler *lastGenericPtr = NULL;
				/* Last handler in list. */

/*
 * There's a potential problem if Tk_HandleEvent is entered recursively.
 * A handler cannot be deleted physically until we have returned from
 * calling it.  Otherwise, we're looking at unallocated memory in advancing to
 * its `next' entry.  We deal with the problem by using the `delete flag' and
 * deleting handlers only when it's known that there's no handler active.
 *
 * The following variable has a non-zero value when a handler is active.
 */

static int genericHandlersActive = 0;

/*
 * The following structure is used for queueing X-style events on the
 * Tcl event queue.
 */

typedef struct TkWindowEvent {
    Tcl_Event header;		/* Standard information for all events. */
    XEvent event;		/* The X event. */
} TkWindowEvent;

/*
 * Array of event masks corresponding to each X event:
 */

static unsigned long eventMasks[] = {
    0,
    0,
    KeyPressMask,			/* KeyPress */
    KeyReleaseMask,			/* KeyRelease */
    ButtonPressMask,			/* ButtonPress */
    ButtonReleaseMask,			/* ButtonRelease */
    PointerMotionMask|PointerMotionHintMask|ButtonMotionMask
	    |Button1MotionMask|Button2MotionMask|Button3MotionMask
	    |Button4MotionMask|Button5MotionMask,
					/* MotionNotify */
    EnterWindowMask,			/* EnterNotify */
    LeaveWindowMask,			/* LeaveNotify */
    FocusChangeMask,			/* FocusIn */
    FocusChangeMask,			/* FocusOut */
    KeymapStateMask,			/* KeymapNotify */
    ExposureMask,			/* Expose */
    ExposureMask,			/* GraphicsExpose */
    ExposureMask,			/* NoExpose */
    VisibilityChangeMask,		/* VisibilityNotify */
    SubstructureNotifyMask,		/* CreateNotify */
    StructureNotifyMask,		/* DestroyNotify */
    StructureNotifyMask,		/* UnmapNotify */
    StructureNotifyMask,		/* MapNotify */
    SubstructureRedirectMask,		/* MapRequest */
    StructureNotifyMask,		/* ReparentNotify */
    StructureNotifyMask,		/* ConfigureNotify */
    SubstructureRedirectMask,		/* ConfigureRequest */
    StructureNotifyMask,		/* GravityNotify */
    ResizeRedirectMask,			/* ResizeRequest */
    StructureNotifyMask,		/* CirculateNotify */
    SubstructureRedirectMask,		/* CirculateRequest */
    PropertyChangeMask,			/* PropertyNotify */
    0,					/* SelectionClear */
    0,					/* SelectionRequest */
    0,					/* SelectionNotify */
    ColormapChangeMask,			/* ColormapNotify */
    0,					/* ClientMessage */
    0,					/* Mapping Notify */
    ActivateMask,			/* ActivateNotify */
    ActivateMask			/* DeactivateNotify */
};

/*
 * If someone has called Tk_RestrictEvents, the information below
 * keeps track of it.
 */

static Tk_RestrictProc *restrictProc;
				/* Procedure to call.  NULL means no
				 * restrictProc is currently in effect. */
static ClientData restrictArg;	/* Argument to pass to restrictProc. */

/*
 * Prototypes for procedures that are only referenced locally within
 * this file.
 */

static void		DelayedMotionProc _ANSI_ARGS_((ClientData clientData));
static int		WindowEventProc _ANSI_ARGS_((Tcl_Event *evPtr,
			    int flags));

/*
 *--------------------------------------------------------------
 *
 * Tk_CreateEventHandler --
 *
 *	Arrange for a given procedure to be invoked whenever
 *	events from a given class occur in a given window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	From now on, whenever an event of the type given by
 *	mask occurs for token and is processed by Tk_HandleEvent,
 *	proc will be called.  See the manual entry for details
 *	of the calling sequence and return value for proc.
 *
 *--------------------------------------------------------------
 */

void
Tk_CreateEventHandler(token, mask, proc, clientData)
    Tk_Window token;		/* Token for window in which to
				 * create handler. */
    unsigned long mask;		/* Events for which proc should
				 * be called. */
    Tk_EventProc *proc;		/* Procedure to call for each
				 * selected event */
    ClientData clientData;	/* Arbitrary data to pass to proc. */
{
    register TkEventHandler *handlerPtr;
    register TkWindow *winPtr = (TkWindow *) token;
    int found;

    /*
     * Skim through the list of existing handlers to (a) compute the
     * overall event mask for the window (so we can pass this new
     * value to the X system) and (b) see if there's already a handler
     * declared with the same callback and clientData (if so, just
     * change the mask).  If no existing handler matches, then create
     * a new handler.
     */

    found = 0;
    if (winPtr->handlerList == NULL) {
	handlerPtr = (TkEventHandler *) ckalloc(
		(unsigned) sizeof(TkEventHandler));
	winPtr->handlerList = handlerPtr;
	goto initHandler;
    } else {
	for (handlerPtr = winPtr->handlerList; ;
		handlerPtr = handlerPtr->nextPtr) {
	    if ((handlerPtr->proc == proc)
		    && (handlerPtr->clientData == clientData)) {
		handlerPtr->mask = mask;
		found = 1;
	    }
	    if (handlerPtr->nextPtr == NULL) {
		break;
	    }
	}
    }

    /*
     * Create a new handler if no matching old handler was found.
     */

    if (!found) {
	handlerPtr->nextPtr = (TkEventHandler *)
		ckalloc(sizeof(TkEventHandler));
	handlerPtr = handlerPtr->nextPtr;
	initHandler:
	handlerPtr->mask = mask;
	handlerPtr->proc = proc;
	handlerPtr->clientData = clientData;
	handlerPtr->nextPtr = NULL;
    }

    /*
     * No need to call XSelectInput:  Tk always selects on all events
     * for all windows (needed to support bindings on classes and "all").
     */
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeleteEventHandler --
 *
 *	Delete a previously-created handler.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there existed a handler as described by the
 *	parameters, the handler is deleted so that proc
 *	will not be invoked again.
 *
 *--------------------------------------------------------------
 */

void
Tk_DeleteEventHandler(token, mask, proc, clientData)
    Tk_Window token;		/* Same as corresponding arguments passed */
    unsigned long mask;		/* previously to Tk_CreateEventHandler. */
    Tk_EventProc *proc;
    ClientData clientData;
{
    register TkEventHandler *handlerPtr;
    register InProgress *ipPtr;
    TkEventHandler *prevPtr;
    register TkWindow *winPtr = (TkWindow *) token;

    /*
     * Find the event handler to be deleted, or return
     * immediately if it doesn't exist.
     */

    for (handlerPtr = winPtr->handlerList, prevPtr = NULL; ;
	    prevPtr = handlerPtr, handlerPtr = handlerPtr->nextPtr) {
	if (handlerPtr == NULL) {
	    return;
	}
	if ((handlerPtr->mask == mask) && (handlerPtr->proc == proc)
		&& (handlerPtr->clientData == clientData)) {
	    break;
	}
    }

    /*
     * If Tk_HandleEvent is about to process this handler, tell it to
     * process the next one instead.
     */

    for (ipPtr = pendingPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	if (ipPtr->nextHandler == handlerPtr) {
	    ipPtr->nextHandler = handlerPtr->nextPtr;
	}
    }

    /*
     * Free resources associated with the handler.
     */

    if (prevPtr == NULL) {
	winPtr->handlerList = handlerPtr->nextPtr;
    } else {
	prevPtr->nextPtr = handlerPtr->nextPtr;
    }
    ckfree((char *) handlerPtr);


    /*
     * No need to call XSelectInput:  Tk always selects on all events
     * for all windows (needed to support bindings on classes and "all").
     */
}

/*--------------------------------------------------------------
 *
 * Tk_CreateGenericHandler --
 *
 *	Register a procedure to be called on each X event, regardless
 *	of display or window.  Generic handlers are useful for capturing
 *	events that aren't associated with windows, or events for windows
 *	not managed by Tk.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	From now on, whenever an X event is given to Tk_HandleEvent,
 *	invoke proc, giving it clientData and the event as arguments.
 *
 *--------------------------------------------------------------
 */

void
Tk_CreateGenericHandler(proc, clientData)
     Tk_GenericProc *proc;	/* Procedure to call on every event. */
     ClientData clientData;	/* One-word value to pass to proc. */
{
    GenericHandler *handlerPtr;
    
    handlerPtr = (GenericHandler *) ckalloc (sizeof (GenericHandler));
    
    handlerPtr->proc = proc;
    handlerPtr->clientData = clientData;
    handlerPtr->deleteFlag = 0;
    handlerPtr->nextPtr = NULL;
    if (genericList == NULL) {
	genericList = handlerPtr;
    } else {
	lastGenericPtr->nextPtr = handlerPtr;
    }
    lastGenericPtr = handlerPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeleteGenericHandler --
 *
 *	Delete a previously-created generic handler.
 *
 * Results:
 *	None.
 *
 * Side Effects:
 *	If there existed a handler as described by the parameters,
 *	that handler is logically deleted so that proc will not be
 *	invoked again.  The physical deletion happens in the event
 *	loop in Tk_HandleEvent.
 *
 *--------------------------------------------------------------
 */

void
Tk_DeleteGenericHandler(proc, clientData)
     Tk_GenericProc *proc;
     ClientData clientData;
{
    GenericHandler * handler;
    
    for (handler = genericList; handler; handler = handler->nextPtr) {
	if ((handler->proc == proc) && (handler->clientData == clientData)) {
	    handler->deleteFlag = 1;
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_HandleEvent --
 *
 *	Given an event, invoke all the handlers that have
 *	been registered for the event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the handlers.
 *
 *--------------------------------------------------------------
 */

void
Tk_HandleEvent(eventPtr)
    XEvent *eventPtr;		/* Event to dispatch. */
{
    register TkEventHandler *handlerPtr;
    register GenericHandler *genericPtr;
    register GenericHandler *genPrevPtr;
    TkWindow *winPtr;
    unsigned long mask;
    InProgress ip;
    Window handlerWindow;
    TkDisplay *dispPtr;
    Tcl_Interp *interp = (Tcl_Interp *) NULL;

    /* 
     * Next, invoke all the generic event handlers (those that are
     * invoked for all events).  If a generic event handler reports that
     * an event is fully processed, go no further.
     */

    for (genPrevPtr = NULL, genericPtr = genericList;  genericPtr != NULL; ) {
	if (genericPtr->deleteFlag) {
	    if (!genericHandlersActive) {
		GenericHandler *tmpPtr;

		/*
		 * This handler needs to be deleted and there are no
		 * calls pending through the handler, so now is a safe
		 * time to delete it.
		 */

		tmpPtr = genericPtr->nextPtr;
		if (genPrevPtr == NULL) {
		    genericList = tmpPtr;
		} else {
		    genPrevPtr->nextPtr = tmpPtr;
		}
		if (tmpPtr == NULL) {
		    lastGenericPtr = genPrevPtr;
		}
		(void) ckfree((char *) genericPtr);
		genericPtr = tmpPtr;
		continue;
	    }
	} else {
	    int done;

	    genericHandlersActive++;
	    done = (*genericPtr->proc)(genericPtr->clientData, eventPtr);
	    genericHandlersActive--;
	    if (done) {
		return;
	    }
	}
	genPrevPtr = genericPtr;
	genericPtr = genPrevPtr->nextPtr;
    }

    /*
     * If the event is a MappingNotify event, find its display and
     * refresh the keyboard mapping information for the display.
     * After that there's nothing else to do with the event, so just
     * quit.
     */

    if (eventPtr->type == MappingNotify) {
	dispPtr = TkGetDisplay(eventPtr->xmapping.display);
	if (dispPtr != NULL) {
	    XRefreshKeyboardMapping(&eventPtr->xmapping);
	    dispPtr->bindInfoStale = 1;
	}
	return;
    }

    /*
     * Events selected by StructureNotify require special handling.
     * They look the same as those selected by SubstructureNotify.
     * The only difference is whether the "event" and "window" fields
     * are the same.  Compare the two fields and convert StructureNotify
     * to SubstructureNotify if necessary.
     */

    handlerWindow = eventPtr->xany.window;
    mask = eventMasks[eventPtr->xany.type];
    if (mask == StructureNotifyMask) {
	if (eventPtr->xmap.event != eventPtr->xmap.window) {
	    mask = SubstructureNotifyMask;
	    handlerWindow = eventPtr->xmap.event;
	}
    }
    winPtr = (TkWindow *) Tk_IdToWindow(eventPtr->xany.display, handlerWindow);
    if (winPtr == NULL) {

	/*
	 * There isn't a TkWindow structure for this window.
	 * However, if the event is a PropertyNotify event then call
	 * the selection manager (it deals beneath-the-table with
	 * certain properties).
	 */

	if (eventPtr->type == PropertyNotify) {
	    TkSelPropProc(eventPtr);
	}
	return;
    }

    if (winPtr->mainPtr != NULL) {

        /*
         * Protect interpreter for this window from possible deletion
         * while we are dealing with the event for this window. Thus,
         * widget writers do not have to worry about protecting the
         * interpreter in their own code.
         */
        
        interp = winPtr->mainPtr->interp;
        Tcl_Preserve((ClientData) interp);
        
	/*
	 * Call focus-related code to look at FocusIn, FocusOut, Enter,
	 * and Leave events;  depending on its return value, ignore the
	 * event.
	 */
    
	if ((mask & (FocusChangeMask|EnterWindowMask|LeaveWindowMask))
		&& !TkFocusFilterEvent(winPtr, eventPtr)) {
            Tcl_Release((ClientData) interp);
	    return;
	}
    
	/*
	 * Redirect KeyPress and KeyRelease events to the focus window,
	 * or ignore them entirely if there is no focus window.  Map the
	 * x and y coordinates to make sense in the context of the focus
	 * window, if possible (make both -1 if the map-from and map-to
	 * windows don't share the same screen).
	 */
    
	if (mask & (KeyPressMask|KeyReleaseMask)) {
	    TkWindow *focusPtr;
	    int winX, winY, focusX, focusY;
    
	    winPtr->dispPtr->lastEventTime = eventPtr->xkey.time;
	    focusPtr = TkGetFocus(winPtr);
	    if (focusPtr == NULL) {
                Tcl_Release((ClientData) interp);
		return;
	    }
	    if ((focusPtr->display != winPtr->display)
		    || (focusPtr->screenNum != winPtr->screenNum)) {
		eventPtr->xkey.x = -1;
		eventPtr->xkey.y = -1;
	    } else {
		Tk_GetRootCoords((Tk_Window) winPtr, &winX, &winY);
		Tk_GetRootCoords((Tk_Window) focusPtr, &focusX, &focusY);
		eventPtr->xkey.x -= focusX - winX;
		eventPtr->xkey.y -= focusY - winY;
	    }
	    eventPtr->xkey.window = focusPtr->window;
	    winPtr = focusPtr;
	}
    
	/*
	 * Call a grab-related procedure to do special processing on
	 * pointer events.
	 */
    
	if (mask & (ButtonPressMask|ButtonReleaseMask|PointerMotionMask
		|EnterWindowMask|LeaveWindowMask)) {
	    if (mask & (ButtonPressMask|ButtonReleaseMask)) {
		winPtr->dispPtr->lastEventTime = eventPtr->xbutton.time;
	    } else if (mask & PointerMotionMask) {
		winPtr->dispPtr->lastEventTime = eventPtr->xmotion.time;
	    } else {
		winPtr->dispPtr->lastEventTime = eventPtr->xcrossing.time;
	    }
	    if (TkPointerEvent(eventPtr, winPtr) == 0) {
                goto done;
	    }
	}
    }

#ifdef TK_USE_INPUT_METHODS
    /*
     * Pass the event to the input method(s), if there are any, and
     * discard the event if the input method(s) insist.  Create the
     * input context for the window if it hasn't already been done
     * (XFilterEvent needs this context).
     */

    if (!(winPtr->flags & TK_CHECKED_IC)) {
	if (winPtr->dispPtr->inputMethod != NULL) {
	    winPtr->inputContext = XCreateIC(
		    winPtr->dispPtr->inputMethod, XNInputStyle,
		    XIMPreeditNothing|XIMStatusNothing,
		    XNClientWindow, winPtr->window,
		    XNFocusWindow, winPtr->window, NULL);
	}
	winPtr->flags |= TK_CHECKED_IC;
    }
    if (XFilterEvent(eventPtr, None)) {
        goto done;
    }
#endif /* TK_USE_INPUT_METHODS */

    /*
     * For events where it hasn't already been done, update the current
     * time in the display.
     */

    if (eventPtr->type == PropertyNotify) {
	winPtr->dispPtr->lastEventTime = eventPtr->xproperty.time;
    }

    /*
     * There's a potential interaction here with Tk_DeleteEventHandler.
     * Read the documentation for pendingPtr.
     */

    ip.eventPtr = eventPtr;
    ip.winPtr = winPtr;
    ip.nextHandler = NULL;
    ip.nextPtr = pendingPtr;
    pendingPtr = &ip;
    if (mask == 0) {
	if ((eventPtr->type == SelectionClear)
		|| (eventPtr->type == SelectionRequest)
		|| (eventPtr->type == SelectionNotify)) {
	    TkSelEventProc((Tk_Window) winPtr, eventPtr);
	} else if ((eventPtr->type == ClientMessage)
		&& (eventPtr->xclient.message_type ==
		    Tk_InternAtom((Tk_Window) winPtr, "WM_PROTOCOLS"))) {
	    TkWmProtocolEventProc(winPtr, eventPtr);
	}
    } else {
	for (handlerPtr = winPtr->handlerList; handlerPtr != NULL; ) {
	    if ((handlerPtr->mask & mask) != 0) {
		ip.nextHandler = handlerPtr->nextPtr;
		(*(handlerPtr->proc))(handlerPtr->clientData, eventPtr);
		handlerPtr = ip.nextHandler;
	    } else {
		handlerPtr = handlerPtr->nextPtr;
	    }
	}

	/*
	 * Pass the event to the "bind" command mechanism.  But, don't
	 * do this for SubstructureNotify events.  The "bind" command
	 * doesn't support them anyway, and it's easier to filter out
	 * these events here than in the lower-level procedures.
	 */

	if ((ip.winPtr != None) && (mask != SubstructureNotifyMask)) {
	    TkBindEventProc(winPtr, eventPtr);
	}
    }
    pendingPtr = ip.nextPtr;
done:

    /*
     * Release the interpreter for this window so that it can be potentially
     * deleted if requested.
     */
    
    if (interp != (Tcl_Interp *) NULL) {
        Tcl_Release((ClientData) interp);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkEventDeadWindow --
 *
 *	This procedure is invoked when it is determined that
 *	a window is dead.  It cleans up event-related information
 *	about the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Various things get cleaned up and recycled.
 *
 *--------------------------------------------------------------
 */

void
TkEventDeadWindow(winPtr)
    TkWindow *winPtr;		/* Information about the window
				 * that is being deleted. */
{
    register TkEventHandler *handlerPtr;
    register InProgress *ipPtr;

    /*
     * While deleting all the handlers, be careful to check for
     * Tk_HandleEvent being about to process one of the deleted
     * handlers.  If it is, tell it to quit (all of the handlers
     * are being deleted).
     */

    while (winPtr->handlerList != NULL) {
	handlerPtr = winPtr->handlerList;
	winPtr->handlerList = handlerPtr->nextPtr;
	for (ipPtr = pendingPtr; ipPtr != NULL; ipPtr = ipPtr->nextPtr) {
	    if (ipPtr->nextHandler == handlerPtr) {
		ipPtr->nextHandler = NULL;
	    }
	    if (ipPtr->winPtr == winPtr) {
		ipPtr->winPtr = None;
	    }
	}
	ckfree((char *) handlerPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkCurrentTime --
 *
 *	Try to deduce the current time.  "Current time" means the time
 *	of the event that led to the current code being executed, which
 *	means the time in the most recently-nested invocation of
 *	Tk_HandleEvent.
 *
 * Results:
 *	The return value is the time from the current event, or
 *	CurrentTime if there is no current event or if the current
 *	event contains no time.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Time
TkCurrentTime(dispPtr)
    TkDisplay *dispPtr;		/* Display for which the time is desired. */
{
    register XEvent *eventPtr;

    if (pendingPtr == NULL) {
	return dispPtr->lastEventTime;
    }
    eventPtr = pendingPtr->eventPtr;
    switch (eventPtr->type) {
	case ButtonPress:
	case ButtonRelease:
	    return eventPtr->xbutton.time;
	case KeyPress:
	case KeyRelease:
	    return eventPtr->xkey.time;
	case MotionNotify:
	    return eventPtr->xmotion.time;
	case EnterNotify:
	case LeaveNotify:
	    return eventPtr->xcrossing.time;
	case PropertyNotify:
	    return eventPtr->xproperty.time;
    }
    return dispPtr->lastEventTime;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_RestrictEvents --
 *
 *	This procedure is used to globally restrict the set of events
 *	that will be dispatched.  The restriction is done by filtering
 *	all incoming X events through a procedure that determines
 *	whether they are to be processed immediately, deferred, or
 *	discarded.
 *
 * Results:
 *	The return value is the previous restriction procedure in effect,
 *	if there was one, or NULL if there wasn't.
 *
 * Side effects:
 *	From now on, proc will be called to determine whether to process,
 *	defer or discard each incoming X event.
 *
 *----------------------------------------------------------------------
 */

Tk_RestrictProc *
Tk_RestrictEvents(proc, arg, prevArgPtr)
    Tk_RestrictProc *proc;	/* Procedure to call for each incoming
				 * event. */
    ClientData arg;		/* Arbitrary argument to pass to proc. */
    ClientData *prevArgPtr;	/* Place to store information about previous
				 * argument. */
{
    Tk_RestrictProc *prev;

    prev = restrictProc;
    *prevArgPtr = restrictArg;
    restrictProc = proc;
    restrictArg = arg;
    return prev;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_QueueWindowEvent --
 *
 *	Given an X-style window event, this procedure adds it to the
 *	Tcl event queue at the given position.  This procedure also
 *	performs mouse motion event collapsing if possible.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Adds stuff to the event queue, which will eventually be
 *	processed.
 *
 *----------------------------------------------------------------------
 */

void
Tk_QueueWindowEvent(eventPtr, position)
    XEvent *eventPtr;			/* Event to add to queue.  This
					 * procedures copies it before adding
					 * it to the queue. */
    Tcl_QueuePosition position;		/* Where to put it on the queue:
					 * TCL_QUEUE_TAIL, TCL_QUEUE_HEAD,
					 * or TCL_QUEUE_MARK. */
{
    TkWindowEvent *wevPtr;
    TkDisplay *dispPtr;

    /*
     * Find our display structure for the event's display.
     */

    for (dispPtr = tkDisplayList; ; dispPtr = dispPtr->nextPtr) {
	if (dispPtr == NULL) {
	    return;
	}
	if (dispPtr->display == eventPtr->xany.display) {
	    break;
	}
    }

    if ((dispPtr->delayedMotionPtr != NULL) && (position == TCL_QUEUE_TAIL)) {
	if ((eventPtr->type == MotionNotify) && (eventPtr->xmotion.window
		== dispPtr->delayedMotionPtr->event.xmotion.window)) {
	    /*
	     * The new event is a motion event in the same window as the
	     * saved motion event.  Just replace the saved event with the
	     * new one.
	     */

	    dispPtr->delayedMotionPtr->event = *eventPtr;
	    return;
	} else if ((eventPtr->type != GraphicsExpose)
		&& (eventPtr->type != NoExpose)
		&& (eventPtr->type != Expose)) {
	    /*
	     * The new event may conflict with the saved motion event.  Queue
	     * the saved motion event now so that it will be processed before
	     * the new event.
	     */

	    Tcl_QueueEvent(&dispPtr->delayedMotionPtr->header, position);
	    dispPtr->delayedMotionPtr = NULL;
	    Tcl_CancelIdleCall(DelayedMotionProc, (ClientData) dispPtr);
	}
    }

    wevPtr = (TkWindowEvent *) ckalloc(sizeof(TkWindowEvent));
    wevPtr->header.proc = WindowEventProc;
    wevPtr->event = *eventPtr;
    if ((eventPtr->type == MotionNotify) && (position == TCL_QUEUE_TAIL)) {
	/*
	 * The new event is a motion event so don't queue it immediately;
	 * save it around in case another motion event arrives that it can
	 * be collapsed with.
	 */

	if (dispPtr->delayedMotionPtr != NULL) {
	    panic("Tk_QueueWindowEvent found unexpected delayed motion event");
	}
	dispPtr->delayedMotionPtr = wevPtr;
	Tcl_DoWhenIdle(DelayedMotionProc, (ClientData) dispPtr);
    } else {
	Tcl_QueueEvent(&wevPtr->header, position);
    }
}

/*
 *---------------------------------------------------------------------------
 *
 * TkQueueEventForAllChildren --
 *
 *	Given an XEvent, recursively queue the event for this window and
 *	all non-toplevel children of the given window.  
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Events queued.
 *
 *---------------------------------------------------------------------------
 */

void
TkQueueEventForAllChildren(tkwin, eventPtr)
    Tk_Window tkwin;	    /* Window to which event is sent. */
    XEvent *eventPtr;	    /* The event to be sent. */
{
    TkWindow *winPtr, *childPtr;

    winPtr = (TkWindow *) tkwin;
    eventPtr->xany.window = winPtr->window;
    Tk_QueueWindowEvent(eventPtr, TCL_QUEUE_TAIL);
    
    childPtr = winPtr->childList;
    while (childPtr != NULL) {
	if (!Tk_IsTopLevel(childPtr)) {
	    TkQueueEventForAllChildren((Tk_Window) childPtr, eventPtr);
	}
	childPtr = childPtr->nextPtr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * WindowEventProc --
 *
 *	This procedure is called by Tcl_DoOneEvent when a window event
 *	reaches the front of the event queue.  This procedure is responsible
 *	for actually handling the event.
 *
 * Results:
 *	Returns 1 if the event was handled, meaning it should be removed
 *	from the queue.  Returns 0 if the event was not handled, meaning
 *	it should stay on the queue.  The event isn't handled if the
 *	TCL_WINDOW_EVENTS bit isn't set in flags, if a restrict proc
 *	prevents the event from being handled.
 *
 * Side effects:
 *	Whatever the event handlers for the event do.
 *
 *----------------------------------------------------------------------
 */

static int
WindowEventProc(evPtr, flags)
    Tcl_Event *evPtr;		/* Event to service. */
    int flags;			/* Flags that indicate what events to
				 * handle, such as TCL_WINDOW_EVENTS. */
{
    TkWindowEvent *wevPtr = (TkWindowEvent *) evPtr;
    Tk_RestrictAction result;

    if (!(flags & TCL_WINDOW_EVENTS)) {
	return 0;
    }
    if (restrictProc != NULL) {
	result = (*restrictProc)(restrictArg, &wevPtr->event);
	if (result != TK_PROCESS_EVENT) {
	    if (result == TK_DEFER_EVENT) {
		return 0;
	    } else {
		/*
		 * TK_DELETE_EVENT: return and say we processed the event,
		 * even though we didn't do anything at all.
		 */
		return 1;
	    }
	}
    }
    Tk_HandleEvent(&wevPtr->event);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * DelayedMotionProc --
 *
 *	This procedure is invoked as an idle handler when a mouse motion
 *	event has been delayed.  It queues the delayed event so that it
 *	will finally be serviced.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The delayed mouse motion event gets added to the Tcl event
 *	queue for servicing.
 *
 *----------------------------------------------------------------------
 */

static void
DelayedMotionProc(clientData)
    ClientData clientData;	/* Pointer to display containing a delayed
				 * motion event to be serviced. */
{
    TkDisplay *dispPtr = (TkDisplay *) clientData;

    if (dispPtr->delayedMotionPtr == NULL) {
	panic("DelayedMotionProc found no delayed mouse motion event");
    }
    Tcl_QueueEvent(&dispPtr->delayedMotionPtr->header, TCL_QUEUE_TAIL);
    dispPtr->delayedMotionPtr = NULL;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_MainLoop --
 *
 *	Call Tcl_DoOneEvent over and over again in an infinite
 *	loop as long as there exist any main windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arbitrary;  depends on handlers for events.
 *
 *--------------------------------------------------------------
 */

void
Tk_MainLoop()
{
    while (Tk_GetNumMainWindows() > 0) {
	Tcl_DoOneEvent(0);
    }
}

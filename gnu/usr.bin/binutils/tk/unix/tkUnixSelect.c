/* 
 * tkUnixSelect.c --
 *
 *	This file contains X specific routines for manipulating 
 *	selections.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUnixSelect.c 1.5 96/03/29 14:14:31
 */

#include "tkInt.h"
#include "tkSelect.h"

/*
 * When handling INCR-style selection retrievals, the selection owner
 * uses the following data structure to communicate between the
 * ConvertSelection procedure and TkSelPropProc.
 */

typedef struct IncrInfo {
    TkWindow *winPtr;		/* Window that owns selection. */
    Atom selection;		/* Selection that is being retrieved. */
    Atom *multAtoms;		/* Information about conversions to
				 * perform:  one or more pairs of
				 * (target, property).  This either
				 * points to a retrieved  property (for
				 * MULTIPLE retrievals) or to a static
				 * array. */
    unsigned long numConversions;
				/* Number of entries in offsets (same as
				 * # of pairs in multAtoms). */
    int *offsets;		/* One entry for each pair in
				 * multAtoms;  -1 means all data has
				 * been transferred for this
				 * conversion.  -2 means only the
				 * final zero-length transfer still
				 * has to be done.  Otherwise it is the
				 * offset of the next chunk of data
				 * to transfer.  This array is malloc-ed. */
    int numIncrs;		/* Number of entries in offsets that
				 * aren't -1 (i.e. # of INCR-mode transfers
				 * not yet completed). */
    Tcl_TimerToken timeout;	/* Token for timer procedure. */
    int idleTime;		/* Number of seconds since we heard
				 * anything from the selection
				 * requestor. */
    Window reqWindow;		/* Requestor's window id. */
    Time time;			/* Timestamp corresponding to
				 * selection at beginning of request;
				 * used to abort transfer if selection
				 * changes. */
    struct IncrInfo *nextPtr;	/* Next in list of all INCR-style
				 * retrievals currently pending. */
} IncrInfo;

static IncrInfo *pendingIncrs = NULL;
				/* List of all incr structures
				 * currently active. */

/*
 * Largest property that we'll accept when sending or receiving the
 * selection:
 */

#define MAX_PROP_WORDS 100000

static TkSelRetrievalInfo *pendingRetrievals = NULL;
				/* List of all retrievals currently
				 * being waited for. */

/*
 * Forward declarations for procedures defined in this file:
 */

static void		ConvertSelection _ANSI_ARGS_((TkWindow *winPtr,
			    XSelectionRequestEvent *eventPtr));
static void		IncrTimeoutProc _ANSI_ARGS_((ClientData clientData));
static char *		SelCvtFromX _ANSI_ARGS_((long *propPtr, int numValues,
			    Atom type, Tk_Window tkwin));
static long *		SelCvtToX _ANSI_ARGS_((char *string, Atom type,
			    Tk_Window tkwin, int *numLongsPtr));
static int		SelectionSize _ANSI_ARGS_((TkSelHandler *selPtr));
static void		SelRcvIncrProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		SelTimeoutProc _ANSI_ARGS_((ClientData clientData));

/*
 *----------------------------------------------------------------------
 *
 * TkSelGetSelection --
 *
 *	Retrieve the specified selection from another process.
 *
 * Results:
 *	The return value is a standard Tcl return value.
 *	If an error occurs (such as no selection exists)
 *	then an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkSelGetSelection(interp, tkwin, selection, target, proc, clientData)
    Tcl_Interp *interp;		/* Interpreter to use for reporting
				 * errors. */
    Tk_Window tkwin;		/* Window on whose behalf to retrieve
				 * the selection (determines display
				 * from which to retrieve). */
    Atom selection;		/* Selection to retrieve. */
    Atom target;		/* Desired form in which selection
				 * is to be returned. */
    Tk_GetSelProc *proc;	/* Procedure to call to process the
				 * selection, once it has been retrieved. */
    ClientData clientData;	/* Arbitrary value to pass to proc. */
{
    TkSelRetrievalInfo retr;
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkDisplay *dispPtr = winPtr->dispPtr;

    /*
     * The selection is owned by some other process.  To
     * retrieve it, first record information about the retrieval
     * in progress.  Use an internal window as the requestor.
     */

    retr.interp = interp;
    if (dispPtr->clipWindow == NULL) {
	int result;

	result = TkClipInit(interp, dispPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }
    retr.winPtr = (TkWindow *) dispPtr->clipWindow;
    retr.selection = selection;
    retr.property = selection;
    retr.target = target;
    retr.proc = proc;
    retr.clientData = clientData;
    retr.result = -1;
    retr.idleTime = 0;
    retr.nextPtr = pendingRetrievals;
    pendingRetrievals = &retr;

    /*
     * Initiate the request for the selection.  Note:  can't use
     * TkCurrentTime for the time.  If we do, and this application hasn't
     * received any X events in a long time, the current time will be way
     * in the past and could even predate the time when the selection was
     * made;  if this happens, the request will be rejected.
     */

    XConvertSelection(winPtr->display, retr.selection, retr.target,
	    retr.property, retr.winPtr->window, CurrentTime);

    /*
     * Enter a loop processing X events until the selection
     * has been retrieved and processed.  If no response is
     * received within a few seconds, then timeout.
     */

    retr.timeout = Tcl_CreateTimerHandler(1000, SelTimeoutProc,
	    (ClientData) &retr);
    while (retr.result == -1) {
	Tcl_DoOneEvent(0);
    }
    Tcl_DeleteTimerHandler(retr.timeout);

    /*
     * Unregister the information about the selection retrieval
     * in progress.
     */

    if (pendingRetrievals == &retr) {
	pendingRetrievals = retr.nextPtr;
    } else {
	TkSelRetrievalInfo *retrPtr;

	for (retrPtr = pendingRetrievals; retrPtr != NULL;
		retrPtr = retrPtr->nextPtr) {
	    if (retrPtr->nextPtr == &retr) {
		retrPtr->nextPtr = retr.nextPtr;
		break;
	    }
	}
    }
    return retr.result;
}

/*
 *----------------------------------------------------------------------
 *
 * TkSelPropProc --
 *
 *	This procedure is invoked when property-change events
 *	occur on windows not known to the toolkit.  Its function
 *	is to implement the sending side of the INCR selection
 *	retrieval protocol when the selection requestor deletes
 *	the property containing a part of the selection.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the property that is receiving the selection was just
 *	deleted, then a new piece of the selection is fetched and
 *	placed in the property, until eventually there's no more
 *	selection to fetch.
 *
 *----------------------------------------------------------------------
 */

void
TkSelPropProc(eventPtr)
    register XEvent *eventPtr;		/* X PropertyChange event. */
{
    register IncrInfo *incrPtr;
    int i, format;
    Atom target, formatType;
    register TkSelHandler *selPtr;
    long buffer[TK_SEL_WORDS_AT_ONCE];
    int numItems;
    char *propPtr;
    Tk_ErrorHandler errorHandler;

    /*
     * See if this event announces the deletion of a property being
     * used for an INCR transfer.  If so, then add the next chunk of
     * data to the property.
     */

    if (eventPtr->xproperty.state != PropertyDelete) {
	return;
    }
    for (incrPtr = pendingIncrs; incrPtr != NULL;
	    incrPtr = incrPtr->nextPtr) {
	if (incrPtr->reqWindow != eventPtr->xproperty.window) {
	    continue;
	}
	for (i = 0; i < incrPtr->numConversions; i++) {
	    if ((eventPtr->xproperty.atom != incrPtr->multAtoms[2*i + 1])
		    || (incrPtr->offsets[i] == -1)){
		continue;
	    }
	    target = incrPtr->multAtoms[2*i];
	    incrPtr->idleTime = 0;
	    for (selPtr = incrPtr->winPtr->selHandlerList; ;
		    selPtr = selPtr->nextPtr) {
		if (selPtr == NULL) {
		    incrPtr->multAtoms[2*i + 1] = None;
		    incrPtr->offsets[i] = -1;
		    incrPtr->numIncrs --;
		    return;
		}
		if ((selPtr->target == target)
			&& (selPtr->selection == incrPtr->selection)) {
		    formatType = selPtr->format;
		    if (incrPtr->offsets[i] == -2) {
			numItems = 0;
			((char *) buffer)[0] = 0;
		    } else {
			TkSelInProgress ip;
			ip.selPtr = selPtr;
			ip.nextPtr = pendingPtr;
			pendingPtr = &ip;
			numItems = (*selPtr->proc)(selPtr->clientData,
				incrPtr->offsets[i], (char *) buffer,
				TK_SEL_BYTES_AT_ONCE);
			pendingPtr = ip.nextPtr;
			if (ip.selPtr == NULL) {
			    /*
			     * The selection handler deleted itself.
			     */

			    return;
			}
			if (numItems > TK_SEL_BYTES_AT_ONCE) {
			    panic("selection handler returned too many bytes");
			} else {
			    if (numItems < 0) {
				numItems = 0;
			    }
			}
			((char *) buffer)[numItems] = '\0';
		    }
		    if (numItems < TK_SEL_BYTES_AT_ONCE) {
			if (numItems <= 0) {
			    incrPtr->offsets[i] = -1;
			    incrPtr->numIncrs--;
			} else {
			    incrPtr->offsets[i] = -2;
			}
		    } else {
			incrPtr->offsets[i] += numItems;
		    }
		    if (formatType == XA_STRING) {
			propPtr = (char *) buffer;
			format = 8;
		    } else {
			propPtr = (char *) SelCvtToX((char *) buffer,
				formatType, (Tk_Window) incrPtr->winPtr,
				&numItems);
			format = 32;
		    }
		    errorHandler = Tk_CreateErrorHandler(
			    eventPtr->xproperty.display, -1, -1, -1,
			    (int (*)()) NULL, (ClientData) NULL);
		    XChangeProperty(eventPtr->xproperty.display,
			    eventPtr->xproperty.window,
			    eventPtr->xproperty.atom, formatType,
			    format, PropModeReplace,
			    (unsigned char *) propPtr, numItems);
		    Tk_DeleteErrorHandler(errorHandler);
		    if (propPtr != (char *) buffer) {
			ckfree(propPtr);
		    }
		    return;
		}
	    }
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkSelEventProc --
 *
 *	This procedure is invoked whenever a selection-related
 *	event occurs.  It does the lion's share of the work
 *	in implementing the selection protocol.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Lots:  depends on the type of event.
 *
 *--------------------------------------------------------------
 */

void
TkSelEventProc(tkwin, eventPtr)
    Tk_Window tkwin;		/* Window for which event was
				 * targeted. */
    register XEvent *eventPtr;	/* X event:  either SelectionClear,
				 * SelectionRequest, or
				 * SelectionNotify. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;
    TkDisplay *dispPtr = winPtr->dispPtr;
    Tcl_Interp *interp;

    /*
     * Case #1: SelectionClear events.
     */

    if (eventPtr->type == SelectionClear) {
	TkSelClearSelection(tkwin, eventPtr);
    }

    /*
     * Case #2: SelectionNotify events.  Call the relevant procedure
     * to handle the incoming selection.
     */

    if (eventPtr->type == SelectionNotify) {
	register TkSelRetrievalInfo *retrPtr;
	char *propInfo;
	Atom type;
	int format, result;
	unsigned long numItems, bytesAfter;

	for (retrPtr = pendingRetrievals; ; retrPtr = retrPtr->nextPtr) {
	    if (retrPtr == NULL) {
		return;
	    }
	    if ((retrPtr->winPtr == winPtr)
		    && (retrPtr->selection == eventPtr->xselection.selection)
		    && (retrPtr->target == eventPtr->xselection.target)
		    && (retrPtr->result == -1)) {
		if (retrPtr->property == eventPtr->xselection.property) {
		    break;
		}
		if (eventPtr->xselection.property == None) {
		    Tcl_SetResult(retrPtr->interp, (char *) NULL, TCL_STATIC);
		    Tcl_AppendResult(retrPtr->interp,
			    Tk_GetAtomName(tkwin, retrPtr->selection),
			    " selection doesn't exist or form \"",
			    Tk_GetAtomName(tkwin, retrPtr->target),
			    "\" not defined", (char *) NULL);
		    retrPtr->result = TCL_ERROR;
		    return;
		}
	    }
	}

	propInfo = NULL;
	result = XGetWindowProperty(eventPtr->xselection.display,
		eventPtr->xselection.requestor, retrPtr->property,
		0, MAX_PROP_WORDS, False, (Atom) AnyPropertyType,
		&type, &format, &numItems, &bytesAfter,
		(unsigned char **) &propInfo);
	if ((result != Success) || (type == None)) {
	    return;
	}
	if (bytesAfter != 0) {
	    Tcl_SetResult(retrPtr->interp, "selection property too large",
		TCL_STATIC);
	    retrPtr->result = TCL_ERROR;
	    XFree(propInfo);
	    return;
	}
	if ((type == XA_STRING) || (type == dispPtr->textAtom)
		|| (type == dispPtr->compoundTextAtom)) {
	    if (format != 8) {
		sprintf(retrPtr->interp->result,
		    "bad format for string selection: wanted \"8\", got \"%d\"",
		    format);
		retrPtr->result = TCL_ERROR;
		return;
	    }
            interp = retrPtr->interp;
            Tcl_Preserve((ClientData) interp);
	    retrPtr->result = (*retrPtr->proc)(retrPtr->clientData,
		    interp, propInfo);
            Tcl_Release((ClientData) interp);
	} else if (type == dispPtr->incrAtom) {

	    /*
	     * It's a !?#@!?!! INCR-style reception.  Arrange to receive
	     * the selection in pieces, using the ICCCM protocol, then
	     * hang around until either the selection is all here or a
	     * timeout occurs.
	     */

	    retrPtr->idleTime = 0;
	    Tk_CreateEventHandler(tkwin, PropertyChangeMask, SelRcvIncrProc,
		    (ClientData) retrPtr);
	    XDeleteProperty(Tk_Display(tkwin), Tk_WindowId(tkwin),
		    retrPtr->property);
	    while (retrPtr->result == -1) {
		Tcl_DoOneEvent(0);
	    }
	    Tk_DeleteEventHandler(tkwin, PropertyChangeMask, SelRcvIncrProc,
		    (ClientData) retrPtr);
	} else {
	    char *string;

	    if (format != 32) {
		sprintf(retrPtr->interp->result,
		    "bad format for selection: wanted \"32\", got \"%d\"",
		    format);
		retrPtr->result = TCL_ERROR;
		return;
	    }
	    string = SelCvtFromX((long *) propInfo, (int) numItems, type,
		    (Tk_Window) winPtr);
            interp = retrPtr->interp;
            Tcl_Preserve((ClientData) interp);
	    retrPtr->result = (*retrPtr->proc)(retrPtr->clientData,
		    interp, string);
            Tcl_Release((ClientData) interp);
	    ckfree(string);
	}
	XFree(propInfo);
	return;
    }

    /*
     * Case #3: SelectionRequest events.  Call ConvertSelection to
     * do the dirty work.
     */

    if (eventPtr->type == SelectionRequest) {
	ConvertSelection(winPtr, &eventPtr->xselectionrequest);
	return;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SelTimeoutProc --
 *
 *	This procedure is invoked once every second while waiting for
 *	the selection to be returned.  After a while it gives up and
 *	aborts the selection retrieval.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new timer callback is created to call us again in another
 *	second, unless time has expired, in which case an error is
 *	recorded for the retrieval.
 *
 *----------------------------------------------------------------------
 */

static void
SelTimeoutProc(clientData)
    ClientData clientData;		/* Information about retrieval
					 * in progress. */
{
    register TkSelRetrievalInfo *retrPtr = (TkSelRetrievalInfo *) clientData;

    /*
     * Make sure that the retrieval is still in progress.  Then
     * see how long it's been since any sort of response was received
     * from the other side.
     */

    if (retrPtr->result != -1) {
	return;
    }
    retrPtr->idleTime++;
    if (retrPtr->idleTime >= 5) {

	/*
	 * Use a careful procedure to store the error message, because
	 * the result could already be partially filled in with a partial
	 * selection return.
	 */

	Tcl_SetResult(retrPtr->interp, "selection owner didn't respond",
		TCL_STATIC);
	retrPtr->result = TCL_ERROR;
    } else {
	retrPtr->timeout = Tcl_CreateTimerHandler(1000, SelTimeoutProc,
	    (ClientData) retrPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConvertSelection --
 *
 *	This procedure is invoked to handle SelectionRequest events.
 *	It responds to the requests, obeying the ICCCM protocols.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Properties are created for the selection requestor, and a
 *	SelectionNotify event is generated for the selection
 *	requestor.  In the event of long selections, this procedure
 *	implements INCR-mode transfers, using the ICCCM protocol.
 *
 *----------------------------------------------------------------------
 */

static void
ConvertSelection(winPtr, eventPtr)
    TkWindow *winPtr;			/* Window that received the
					 * conversion request;  may not be
					 * selection's current owner, be we
					 * set it to the current owner. */
    register XSelectionRequestEvent *eventPtr;
					/* Event describing request. */
{
    XSelectionEvent reply;		/* Used to notify requestor that
					 * selection info is ready. */
    int multiple;			/* Non-zero means a MULTIPLE request
					 * is being handled. */
    IncrInfo incr;			/* State of selection conversion. */
    Atom singleInfo[2];			/* incr.multAtoms points here except
					 * for multiple conversions. */
    int i;
    Tk_ErrorHandler errorHandler;
    TkSelectionInfo *infoPtr;
    TkSelInProgress ip;

    errorHandler = Tk_CreateErrorHandler(eventPtr->display, -1, -1,-1,
	    (int (*)()) NULL, (ClientData) NULL);

    /*
     * Initialize the reply event.
     */

    reply.type = SelectionNotify;
    reply.serial = 0;
    reply.send_event = True;
    reply.display = eventPtr->display;
    reply.requestor = eventPtr->requestor;
    reply.selection = eventPtr->selection;
    reply.target = eventPtr->target;
    reply.property = eventPtr->property;
    if (reply.property == None) {
	reply.property = reply.target;
    }
    reply.time = eventPtr->time;

    for (infoPtr = winPtr->dispPtr->selectionInfoPtr; infoPtr != NULL;
	    infoPtr = infoPtr->nextPtr) {
	if (infoPtr->selection == eventPtr->selection)
	    break;
    }
    if (infoPtr == NULL) {
	goto refuse;
    }
    winPtr = (TkWindow *) infoPtr->owner;

    /*
     * Figure out which kind(s) of conversion to perform.  If handling
     * a MULTIPLE conversion, then read the property describing which
     * conversions to perform.
     */

    incr.winPtr = winPtr;
    incr.selection = eventPtr->selection;
    if (eventPtr->target != winPtr->dispPtr->multipleAtom) {
	multiple = 0;
	singleInfo[0] = reply.target;
	singleInfo[1] = reply.property;
	incr.multAtoms = singleInfo;
	incr.numConversions = 1;
    } else {
	Atom type;
	int format, result;
	unsigned long bytesAfter;

	multiple = 1;
	incr.multAtoms = NULL;
	if (eventPtr->property == None) {
	    goto refuse;
	}
	result = XGetWindowProperty(eventPtr->display,
		eventPtr->requestor, eventPtr->property,
		0, MAX_PROP_WORDS, False, XA_ATOM,
		&type, &format, &incr.numConversions, &bytesAfter,
		(unsigned char **) &incr.multAtoms);
	if ((result != Success) || (bytesAfter != 0) || (format != 32)
		|| (type == None)) {
	    if (incr.multAtoms != NULL) {
		XFree((char *) incr.multAtoms);
	    }
	    goto refuse;
	}
	incr.numConversions /= 2;		/* Two atoms per conversion. */
    }

    /*
     * Loop through all of the requested conversions, and either return
     * the entire converted selection, if it can be returned in a single
     * bunch, or return INCR information only (the actual selection will
     * be returned below).
     */

    incr.offsets = (int *) ckalloc((unsigned)
	    (incr.numConversions*sizeof(int)));
    incr.numIncrs = 0;
    for (i = 0; i < incr.numConversions; i++) {
	Atom target, property, type;
	long buffer[TK_SEL_WORDS_AT_ONCE];
	register TkSelHandler *selPtr;
	int numItems, format;
	char *propPtr;

	target = incr.multAtoms[2*i];
	property = incr.multAtoms[2*i + 1];
	incr.offsets[i] = -1;

	for (selPtr = winPtr->selHandlerList; selPtr != NULL;
		selPtr = selPtr->nextPtr) {
	    if ((selPtr->target == target)
		    && (selPtr->selection == eventPtr->selection)) {
		break;
	    }
	}

	if (selPtr == NULL) {
	    /*
	     * Nobody seems to know about this kind of request.  If
	     * it's of a sort that we can handle without any help, do
	     * it.  Otherwise mark the request as an errror.
	     */

	    numItems = TkSelDefaultSelection(infoPtr, target, (char *) buffer,
		    TK_SEL_BYTES_AT_ONCE, &type);
	    if (numItems < 0) {
		incr.multAtoms[2*i + 1] = None;
		continue;
	    }
	} else {
	    ip.selPtr = selPtr;
	    ip.nextPtr = pendingPtr;
	    pendingPtr = &ip;
	    type = selPtr->format;
	    numItems = (*selPtr->proc)(selPtr->clientData, 0,
		    (char *) buffer, TK_SEL_BYTES_AT_ONCE);
	    pendingPtr = ip.nextPtr;
	    if ((ip.selPtr == NULL) || (numItems < 0)) {
		incr.multAtoms[2*i + 1] = None;
		continue;
	    }
	    if (numItems > TK_SEL_BYTES_AT_ONCE) {
		panic("selection handler returned too many bytes");
	    }
	    ((char *) buffer)[numItems] = '\0';
	}

	/*
	 * Got the selection;  store it back on the requestor's property.
	 */

	if (numItems == TK_SEL_BYTES_AT_ONCE) {
	    /*
	     * Selection is too big to send at once;  start an
	     * INCR-mode transfer.
	     */

	    incr.numIncrs++;
	    type = winPtr->dispPtr->incrAtom;
	    buffer[0] = SelectionSize(selPtr);
	    if (buffer[0] == 0) {
		incr.multAtoms[2*i + 1] = None;
		continue;
	    }
	    numItems = 1;
	    propPtr = (char *) buffer;
	    format = 32;
	    incr.offsets[i] = 0;
	} else if (type == XA_STRING) {
	    propPtr = (char *) buffer;
	    format = 8;
	} else {
	    propPtr = (char *) SelCvtToX((char *) buffer,
		    type, (Tk_Window) winPtr, &numItems);
	    format = 32;
	}
	XChangeProperty(reply.display, reply.requestor,
		property, type, format, PropModeReplace,
		(unsigned char *) propPtr, numItems);
	if (propPtr != (char *) buffer) {
	    ckfree(propPtr);
	}
    }

    /*
     * Send an event back to the requestor to indicate that the
     * first stage of conversion is complete (everything is done
     * except for long conversions that have to be done in INCR
     * mode).
     */

    if (incr.numIncrs > 0) {
	XSelectInput(reply.display, reply.requestor, PropertyChangeMask);
	incr.timeout = Tcl_CreateTimerHandler(1000, IncrTimeoutProc,
	    (ClientData) &incr);
	incr.idleTime = 0;
	incr.reqWindow = reply.requestor;
	incr.time = infoPtr->time;
	incr.nextPtr = pendingIncrs;
	pendingIncrs = &incr;
    }
    if (multiple) {
	XChangeProperty(reply.display, reply.requestor, reply.property,
		XA_ATOM, 32, PropModeReplace,
		(unsigned char *) incr.multAtoms,
		(int) incr.numConversions*2);
    } else {

	/*
	 * Not a MULTIPLE request.  The first property in "multAtoms"
	 * got set to None if there was an error in conversion.
	 */

	reply.property = incr.multAtoms[1];
    }
    XSendEvent(reply.display, reply.requestor, False, 0, (XEvent *) &reply);
    Tk_DeleteErrorHandler(errorHandler);

    /*
     * Handle any remaining INCR-mode transfers.  This all happens
     * in callbacks to TkSelPropProc, so just wait until the number
     * of uncompleted INCR transfers drops to zero.
     */

    if (incr.numIncrs > 0) {
	IncrInfo *incrPtr2;

	while (incr.numIncrs > 0) {
	    Tcl_DoOneEvent(0);
	}
	Tcl_DeleteTimerHandler(incr.timeout);
	errorHandler = Tk_CreateErrorHandler(winPtr->display,
		-1, -1,-1, (int (*)()) NULL, (ClientData) NULL);
	XSelectInput(reply.display, reply.requestor, 0L);
	Tk_DeleteErrorHandler(errorHandler);
	if (pendingIncrs == &incr) {
	    pendingIncrs = incr.nextPtr;
	} else {
	    for (incrPtr2 = pendingIncrs; incrPtr2 != NULL;
		    incrPtr2 = incrPtr2->nextPtr) {
		if (incrPtr2->nextPtr == &incr) {
		    incrPtr2->nextPtr = incr.nextPtr;
		    break;
		}
	    }
	}
    }

    /*
     * All done.  Cleanup and return.
     */

    ckfree((char *) incr.offsets);
    if (multiple) {
	XFree((char *) incr.multAtoms);
    }
    return;

    /*
     * An error occurred.  Send back a refusal message.
     */

    refuse:
    reply.property = None;
    XSendEvent(reply.display, reply.requestor, False, 0, (XEvent *) &reply);
    Tk_DeleteErrorHandler(errorHandler);
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * SelRcvIncrProc --
 *
 *	This procedure handles the INCR protocol on the receiving
 *	side.  It is invoked in response to property changes on
 *	the requestor's window (which hopefully are because a new
 *	chunk of the selection arrived).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a new piece of selection has arrived, a procedure is
 *	invoked to deal with that piece.  When the whole selection
 *	is here, a flag is left for the higher-level procedure that
 *	initiated the selection retrieval.
 *
 *----------------------------------------------------------------------
 */

static void
SelRcvIncrProc(clientData, eventPtr)
    ClientData clientData;		/* Information about retrieval. */
    register XEvent *eventPtr;		/* X PropertyChange event. */
{
    register TkSelRetrievalInfo *retrPtr = (TkSelRetrievalInfo *) clientData;
    char *propInfo;
    Atom type;
    int format, result;
    unsigned long numItems, bytesAfter;
    Tcl_Interp *interp;

    if ((eventPtr->xproperty.atom != retrPtr->property)
	    || (eventPtr->xproperty.state != PropertyNewValue)
	    || (retrPtr->result != -1)) {
	return;
    }
    propInfo = NULL;
    result = XGetWindowProperty(eventPtr->xproperty.display,
	    eventPtr->xproperty.window, retrPtr->property, 0, MAX_PROP_WORDS,
	    True, (Atom) AnyPropertyType, &type, &format, &numItems,
	    &bytesAfter, (unsigned char **) &propInfo);
    if ((result != Success) || (type == None)) {
	return;
    }
    if (bytesAfter != 0) {
	Tcl_SetResult(retrPtr->interp, "selection property too large",
		TCL_STATIC);
	retrPtr->result = TCL_ERROR;
	goto done;
    }
    if (numItems == 0) {
	retrPtr->result = TCL_OK;
    } else if ((type == XA_STRING)
	    || (type == retrPtr->winPtr->dispPtr->textAtom)
	    || (type == retrPtr->winPtr->dispPtr->compoundTextAtom)) {
	if (format != 8) {
	    Tcl_SetResult(retrPtr->interp, (char *) NULL, TCL_STATIC);
	    sprintf(retrPtr->interp->result,
		"bad format for string selection: wanted \"8\", got \"%d\"",
		format);
	    retrPtr->result = TCL_ERROR;
	    goto done;
	}
        interp = retrPtr->interp;
        Tcl_Preserve((ClientData) interp);
	result = (*retrPtr->proc)(retrPtr->clientData, interp, propInfo);
        Tcl_Release((ClientData) interp);
	if (result != TCL_OK) {
	    retrPtr->result = result;
	}
    } else {
	char *string;

	if (format != 32) {
	    Tcl_SetResult(retrPtr->interp, (char *) NULL, TCL_STATIC);
	    sprintf(retrPtr->interp->result,
		"bad format for selection: wanted \"32\", got \"%d\"",
		format);
	    retrPtr->result = TCL_ERROR;
	    goto done;
	}
	string = SelCvtFromX((long *) propInfo, (int) numItems, type,
		(Tk_Window) retrPtr->winPtr);
        interp = retrPtr->interp;
        Tcl_Preserve((ClientData) interp);
	result = (*retrPtr->proc)(retrPtr->clientData, interp, string);
        Tcl_Release((ClientData) interp);
	if (result != TCL_OK) {
	    retrPtr->result = result;
	}
	ckfree(string);
    }

    done:
    XFree(propInfo);
    retrPtr->idleTime = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * SelectionSize --
 *
 *	This procedure is called when the selection is too large to
 *	send in a single buffer;  it computes the total length of
 *	the selection in bytes.
 *
 * Results:
 *	The return value is the number of bytes in the selection
 *	given by selPtr.
 *
 * Side effects:
 *	The selection is retrieved from its current owner (this is
 *	the only way to compute its size).
 *
 *----------------------------------------------------------------------
 */

static int
SelectionSize(selPtr)
    TkSelHandler *selPtr;	/* Information about how to retrieve
				 * the selection whose size is wanted. */
{
    char buffer[TK_SEL_BYTES_AT_ONCE+1];
    int size, chunkSize;
    TkSelInProgress ip;

    size = TK_SEL_BYTES_AT_ONCE;
    ip.selPtr = selPtr;
    ip.nextPtr = pendingPtr;
    pendingPtr = &ip;
    do {
	chunkSize = (*selPtr->proc)(selPtr->clientData, size,
			(char *) buffer, TK_SEL_BYTES_AT_ONCE);
	if (ip.selPtr == NULL) {
	    size = 0;
	    break;
	}
	size += chunkSize;
    } while (chunkSize == TK_SEL_BYTES_AT_ONCE);
    pendingPtr = ip.nextPtr;
    return size;
}

/*
 *----------------------------------------------------------------------
 *
 * IncrTimeoutProc --
 *
 *	This procedure is invoked once a second while sending the
 *	selection to a requestor in INCR mode.  After a while it
 *	gives up and aborts the selection operation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new timeout gets registered so that this procedure gets
 *	called again in another second, unless too many seconds
 *	have elapsed, in which case incrPtr is marked as "all done".
 *
 *----------------------------------------------------------------------
 */

static void
IncrTimeoutProc(clientData)
    ClientData clientData;		/* Information about INCR-mode
					 * selection retrieval for which
					 * we are selection owner. */
{
    register IncrInfo *incrPtr = (IncrInfo *) clientData;

    incrPtr->idleTime++;
    if (incrPtr->idleTime >= 5) {
	incrPtr->numIncrs = 0;
    } else {
	incrPtr->timeout = Tcl_CreateTimerHandler(1000, IncrTimeoutProc,
		(ClientData) incrPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SelCvtToX --
 *
 *	Given a selection represented as a string (the normal Tcl form),
 *	convert it to the ICCCM-mandated format for X, depending on
 *	the type argument.  This procedure and SelCvtFromX are inverses.
 *
 * Results:
 *	The return value is a malloc'ed buffer holding a value
 *	equivalent to "string", but formatted as for "type".  It is
 *	the caller's responsibility to free the string when done with
 *	it.  The word at *numLongsPtr is filled in with the number of
 *	32-bit words returned in the result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static long *
SelCvtToX(string, type, tkwin, numLongsPtr)
    char *string;		/* String representation of selection. */
    Atom type;			/* Atom specifying the X format that is
				 * desired for the selection.  Should not
				 * be XA_STRING (if so, don't bother calling
				 * this procedure at all). */
    Tk_Window tkwin;		/* Window that governs atom conversion. */
    int *numLongsPtr;		/* Number of 32-bit words contained in the
				 * result. */
{
    register char *p;
    char *field;
    int numFields;
    long *propPtr, *longPtr;
#define MAX_ATOM_NAME_LENGTH 100
    char atomName[MAX_ATOM_NAME_LENGTH+1];

    /*
     * The string is assumed to consist of fields separated by spaces.
     * The property gets generated by converting each field to an
     * integer number, in one of two ways:
     * 1. If type is XA_ATOM, convert each field to its corresponding
     *	  atom.
     * 2. If type is anything else, convert each field from an ASCII number
     *    to a 32-bit binary number.
     */

    numFields = 1;
    for (p = string; *p != 0; p++) {
	if (isspace(UCHAR(*p))) {
	    numFields++;
	}
    }
    propPtr = (long *) ckalloc((unsigned) numFields*sizeof(long));

    /*
     * Convert the fields one-by-one.
     */

    for (longPtr = propPtr, *numLongsPtr = 0, p = string;
	    ; longPtr++, (*numLongsPtr)++) {
	while (isspace(UCHAR(*p))) {
	    p++;
	}
	if (*p == 0) {
	    break;
	}
	field = p;
	while ((*p != 0) && !isspace(UCHAR(*p))) {
	    p++;
	}
	if (type == XA_ATOM) {
	    int length;

	    length = p - field;
	    if (length > MAX_ATOM_NAME_LENGTH) {
		length = MAX_ATOM_NAME_LENGTH;
	    }
	    strncpy(atomName, field, (unsigned) length);
	    atomName[length] = 0;
	    *longPtr = (long) Tk_InternAtom(tkwin, atomName);
	} else {
	    char *dummy;

	    *longPtr = strtol(field, &dummy, 0);
	}
    }
    return propPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * SelCvtFromX --
 *
 *	Given an X property value, formatted as a collection of 32-bit
 *	values according to "type" and the ICCCM conventions, convert
 *	the value to a string suitable for manipulation by Tcl.  This
 *	procedure is the inverse of SelCvtToX.
 *
 * Results:
 *	The return value is the string equivalent of "property".  It is
 *	malloc-ed and should be freed by the caller when no longer
 *	needed.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
SelCvtFromX(propPtr, numValues, type, tkwin)
    register long *propPtr;	/* Property value from X. */
    int numValues;		/* Number of 32-bit values in property. */
    Atom type;			/* Type of property  Should not be
				 * XA_STRING (if so, don't bother calling
				 * this procedure at all). */
    Tk_Window tkwin;		/* Window to use for atom conversion. */
{
    char *result;
    int resultSpace, curSize, fieldSize;
    char *atomName;

    /*
     * Convert each long in the property to a string value, which is
     * either the name of an atom (if type is XA_ATOM) or a hexadecimal
     * string.  Make an initial guess about the size of the result, but
     * be prepared to enlarge the result if necessary.
     */

    resultSpace = 12*numValues+1;
    curSize = 0;
    atomName = "";	/* Not needed, but eliminates compiler warning. */
    result = (char *) ckalloc((unsigned) resultSpace);
    *result  = '\0';
    for ( ; numValues > 0; propPtr++, numValues--) {
	if (type == XA_ATOM) {
	    atomName = Tk_GetAtomName(tkwin, (Atom) *propPtr);
	    fieldSize = strlen(atomName) + 1;
	} else {
	    fieldSize = 12;
	}
	if (curSize+fieldSize >= resultSpace) {
	    char *newResult;

	    resultSpace *= 2;
	    if (curSize+fieldSize >= resultSpace) {
		resultSpace = curSize + fieldSize + 1;
	    }
	    newResult = (char *) ckalloc((unsigned) resultSpace);
	    strncpy(newResult, result, (unsigned) curSize);
	    ckfree(result);
	    result = newResult;
	}
	if (curSize != 0) {
	    result[curSize] = ' ';
	    curSize++;
	}
	if (type == XA_ATOM) {
	    strcpy(result+curSize, atomName);
	} else {
	    sprintf(result+curSize, "0x%x", (unsigned int) *propPtr);
	}
	curSize += strlen(result+curSize);
    }
    return result;
}

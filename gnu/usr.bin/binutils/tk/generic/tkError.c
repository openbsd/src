/* 
 * tkError.c --
 *
 *	This file provides a high-performance mechanism for
 *	selectively dealing with errors that occur in talking
 *	to the X server.  This is useful, for example, when
 *	communicating with a window that may not exist.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkError.c 1.20 96/02/15 18:53:17
 */

#include "tkPort.h"
#include "tkInt.h"

/*
 * The default X error handler gets saved here, so that it can
 * be invoked if an error occurs that we can't handle.
 */

static int	(*defaultHandler) _ANSI_ARGS_((Display *display,
		    XErrorEvent *eventPtr)) = NULL;


/*
 * Forward references to procedures declared later in this file:
 */

static int	ErrorProc _ANSI_ARGS_((Display *display,
		    XErrorEvent *errEventPtr));

/*
 *--------------------------------------------------------------
 *
 * Tk_CreateErrorHandler --
 *
 *	Arrange for all a given procedure to be invoked whenever
 *	certain errors occur.
 *
 * Results:
 *	The return value is a token identifying the handler;
 *	it must be passed to Tk_DeleteErrorHandler to delete the
 *	handler.
 *
 * Side effects:
 *	If an X error occurs that matches the error, request,
 *	and minor arguments, then errorProc will be invoked.
 *	ErrorProc should have the following structure:
 *
 *	int
 *	errorProc(clientData, errorEventPtr)
 *	    caddr_t clientData;
 *	    XErrorEvent *errorEventPtr;
 *	{
 *	}
 *
 *	The clientData argument will be the same as the clientData
 *	argument to this procedure, and errorEvent will describe
 *	the error.  If errorProc returns 0, it means that it
 *	completely "handled" the error:  no further processing
 *	should be done.  If errorProc returns 1, it means that it
 *	didn't know how to deal with the error, so we should look
 *	for other error handlers, or invoke the default error
 *	handler if no other handler returns zero.  Handlers are
 *	invoked in order of age:  youngest handler first.
 *
 *	Note:  errorProc will only be called for errors associated
 *	with X requests made AFTER this call, but BEFORE the handler
 *	is deleted by calling Tk_DeleteErrorHandler.
 *
 *--------------------------------------------------------------
 */

Tk_ErrorHandler
Tk_CreateErrorHandler(display, error, request, minorCode, errorProc, clientData)
    Display *display;		/* Display for which to handle
				 * errors. */
    int error;			/* Consider only errors with this
				 * error_code (-1 means consider
				 * all errors). */
    int request;		/* Consider only errors with this
				 * major request code (-1 means
				 * consider all major codes). */
    int minorCode;		/* Consider only errors with this
				 * minor request code (-1 means
				 * consider all minor codes). */
    Tk_ErrorProc *errorProc;	/* Procedure to invoke when a
				 * matching error occurs.  NULL means
				 * just ignore matching errors. */
    ClientData clientData;	/* Arbitrary value to pass to
				 * errorProc. */
{
    register TkErrorHandler *errorPtr;
    register TkDisplay *dispPtr;

    /*
     * Find the display.  If Tk doesn't know about this display then
     * it's an error:  panic.
     */

    dispPtr = TkGetDisplay(display);
    if (dispPtr == NULL) {
	panic("Unknown display passed to Tk_CreateErrorHandler");
    }

    /*
     * Make sure that X calls us whenever errors occur.
     */

    if (defaultHandler == NULL) {
	defaultHandler = XSetErrorHandler(ErrorProc);
    }

    /*
     * Create the handler record.
     */

    errorPtr = (TkErrorHandler *) ckalloc(sizeof(TkErrorHandler));
    errorPtr->dispPtr = dispPtr;
    errorPtr->firstRequest = NextRequest(display);
    errorPtr->lastRequest = (unsigned) -1;
    errorPtr->error = error;
    errorPtr->request = request;
    errorPtr->minorCode = minorCode;
    errorPtr->errorProc = errorProc;
    errorPtr->clientData = clientData;
    errorPtr->nextPtr = dispPtr->errorPtr;
    dispPtr->errorPtr = errorPtr;

    return (Tk_ErrorHandler) errorPtr;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DeleteErrorHandler --
 *
 *	Do not use an error handler anymore.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The handler denoted by the "handler" argument will not
 *	be invoked for any X errors associated with requests
 *	made after this call.  However, if errors arrive later
 *	for requests made BEFORE this call, then the handler
 *	will still be invoked.  Call XSync if you want to be
 *	sure that all outstanding errors have been received
 *	and processed.
 *
 *--------------------------------------------------------------
 */

void
Tk_DeleteErrorHandler(handler)
    Tk_ErrorHandler handler;	/* Token for handler to delete;
				 * was previous return value from
				 * Tk_CreateErrorHandler. */
{
    register TkErrorHandler *errorPtr = (TkErrorHandler *) handler;
    register TkDisplay *dispPtr = errorPtr->dispPtr;

    errorPtr->lastRequest = NextRequest(dispPtr->display) - 1;

    /*
     * Every once-in-a-while, cleanup handlers that are no longer
     * active.  We probably won't be able to free the handler that
     * was just deleted (need to wait for any outstanding requests to
     * be processed by server), but there may be previously-deleted
     * handlers that are now ready for garbage collection.  To reduce
     * the cost of the cleanup, let a few dead handlers pile up, then
     * clean them all at once.  This adds a bit of overhead to errors
     * that might occur while the dead handlers are hanging around,
     * but reduces the overhead of scanning the list to clean up
     * (particularly if there are many handlers that stay around
     * forever).
     */

    dispPtr->deleteCount += 1;
    if (dispPtr->deleteCount >= 10) {
	register TkErrorHandler *prevPtr;
	TkErrorHandler *nextPtr;
	int lastSerial;

	dispPtr->deleteCount = 0;
	lastSerial = LastKnownRequestProcessed(dispPtr->display);
	errorPtr = dispPtr->errorPtr;
	for (errorPtr = dispPtr->errorPtr, prevPtr = NULL;
		errorPtr != NULL;  errorPtr = nextPtr) {
	    nextPtr = errorPtr->nextPtr;
	    if ((errorPtr->lastRequest != -1)
		    && (errorPtr->lastRequest <= lastSerial)) {
		if (prevPtr == NULL) {
		    dispPtr->errorPtr = nextPtr;
		} else {
		    prevPtr->nextPtr = nextPtr;
		}
		ckfree((char *) errorPtr);
		continue;
	    }
	    prevPtr = errorPtr;
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * ErrorProc --
 *
 *	This procedure is invoked by the X system when error
 *	events arrive.
 *
 * Results:
 *	If it returns, the return value is zero.  However,
 *	it is possible that one of the error handlers may
 *	just exit.
 *
 * Side effects:
 *	This procedure does two things.  First, it uses the
 *	serial #  in the error event to eliminate handlers whose
 *	expiration serials are now in the past.  Second, it
 *	invokes any handlers that want to deal with the error.
 *
 *--------------------------------------------------------------
 */

static int
ErrorProc(display, errEventPtr)
    Display *display;			/* Display for which error
					 * occurred. */
    register XErrorEvent *errEventPtr;	/* Information about error. */
{
    register TkDisplay *dispPtr;
    register TkErrorHandler *errorPtr;

    /*
     * See if we know anything about the display.  If not, then
     * invoke the default error handler.
     */

    dispPtr = TkGetDisplay(display);
    if (dispPtr == NULL) {
	goto couldntHandle;
    }

    /*
     * Otherwise invoke any relevant handlers for the error, in order.
     */

    for (errorPtr = dispPtr->errorPtr; errorPtr != NULL;
	    errorPtr = errorPtr->nextPtr) {
	if ((errorPtr->firstRequest > errEventPtr->serial)
		|| ((errorPtr->error != -1)
		    && (errorPtr->error != errEventPtr->error_code))
		|| ((errorPtr->request != -1)
		    && (errorPtr->request != errEventPtr->request_code))
		|| ((errorPtr->minorCode != -1)
		    && (errorPtr->minorCode != errEventPtr->minor_code))
		|| ((errorPtr->lastRequest != -1)
		    && (errorPtr->lastRequest < errEventPtr->serial))) {
	    continue;
	}
	if (errorPtr->errorProc == NULL) {
	    return 0;
	} else {
	    if ((*errorPtr->errorProc)(errorPtr->clientData,
		    errEventPtr) == 0) {
		return 0;
	    }
	}
    }

    /*
     * See if the error is a BadWindow error.  If so, and it refers
     * to a window that still exists in our window table, then ignore
     * the error.  Errors like this can occur if a window owned by us
     * is deleted by someone externally, like a window manager.  We'll
     * ignore the errors at least long enough to clean up internally and
     * remove the entry from the window table.
     */

    if ((errEventPtr->error_code == BadWindow) && (Tk_IdToWindow(display,
	    (Window) errEventPtr->resourceid) != NULL)) {
	return 0;
    }

    /*
     * We couldn't handle the error.  Use the default handler.
     */

    couldntHandle:
    return (*defaultHandler)(display, errEventPtr);
}

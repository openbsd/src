/* 
 * tkWinClipboard.c --
 *
 *	This file contains functions for managing the clipboard.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWinClipboard.c 1.5 96/02/15 18:56:07
 */

#include "tkWinInt.h"
#include "tkSelect.h"


/*
 *----------------------------------------------------------------------
 *
 * TkSelGetSelection --
 *
 *	Retrieve the specified selection from another process.  For
 *	now, only fetching XA_STRING from CLIPBOARD is supported.
 *	Eventually other types should be allowed.
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
    char *data, *buffer, *destPtr;
    HGLOBAL handle;
    int result, length;

    if ((selection == Tk_InternAtom(tkwin, "CLIPBOARD"))
	    && (target == XA_STRING)) {
	if (OpenClipboard(NULL)) {
	    handle = GetClipboardData(CF_TEXT);
	    if (handle != NULL) {
		data = GlobalLock(handle);
		length = strlen(data);
		buffer = ckalloc(length+1);
		destPtr = buffer;
		while (*data != '\0') {
		    if (*data != '\r') {
			*destPtr = *data;
			destPtr++;
		    }
		    data++;
		}
		*destPtr = '\0';
		GlobalUnlock(handle);
		CloseClipboard();
		result = (*proc)(clientData, interp, buffer);
		ckfree(buffer);
		return result;
	    }
	    CloseClipboard();
	}
    }

    Tcl_AppendResult(interp, Tk_GetAtomName(tkwin, selection),
	" selection doesn't exist or form \"", Tk_GetAtomName(tkwin, target),
	"\" not defined", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * TkSetSelectionOwner --
 *
 *	This function claims ownership of the specified selection.
 *	If the selection is CLIPBOARD, then we empty the system
 *	clipboard.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Empties the system clipboard, and claims ownership.
 *
 *----------------------------------------------------------------------
 */

void
XSetSelectionOwner(display, selection, owner, time)
    Display* display;
    Atom selection;
    Window owner;
    Time time;
{
    HWND hwnd = owner ? TkWinGetHWND(owner) : NULL;
    Tk_Window tkwin;

    /*
     * This is a gross hack because the Tk_InternAtom interface is broken.
     * It expects a Tk_Window, even though it only needs a Tk_Display.
     */

    tkwin = (Tk_Window)tkMainWindowList->winPtr;

    if (selection == Tk_InternAtom(tkwin, "CLIPBOARD")) {

	/*
	 * Only claim and empty the clipboard if we aren't already the
	 * owner of the clipboard.
	 */

	if (GetClipboardOwner() != hwnd) {
	    OpenClipboard(hwnd);
	    EmptyClipboard();
	    SetClipboardData(CF_TEXT, NULL);
	    CloseClipboard();
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWinClipboardRender --
 *
 *	This function supplies the contents of the clipboard in
 *	response to a WM_RENDERFORMAT message.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets the contents of the clipboard.
 *
 *----------------------------------------------------------------------
 */

void
TkWinClipboardRender(winPtr, format)
    TkWindow *winPtr;
    UINT format;
{
    TkClipboardTarget *targetPtr;
    TkClipboardBuffer *cbPtr;
    TkDisplay *dispPtr = winPtr->dispPtr;
    HGLOBAL handle;
    char *buffer;
    int length;

    for (targetPtr = dispPtr->clipTargetPtr; targetPtr != NULL;
	    targetPtr = targetPtr->nextPtr) {
	if (targetPtr->type == XA_STRING)
	    break;
    }
    length = 0;
    if (targetPtr != NULL) {
	for (cbPtr = targetPtr->firstBufferPtr; cbPtr != NULL;
		cbPtr = cbPtr->nextPtr) {
	    length += cbPtr->length;
	}
    }
    handle = GlobalAlloc(GMEM_MOVEABLE|GMEM_DDESHARE, length+1);
    if (!handle) {
	return;
    }
    buffer = GlobalLock(handle);
    if (targetPtr != NULL) {
	for (cbPtr = targetPtr->firstBufferPtr; cbPtr != NULL;
		cbPtr = cbPtr->nextPtr) {
	    strncpy(buffer, cbPtr->buffer, cbPtr->length);
	    buffer += cbPtr->length;
	}
    }
    *buffer = '\0';
    GlobalUnlock(handle);
    SetClipboardData(CF_TEXT, handle);
    return;
}

/*
 *----------------------------------------------------------------------
 *
 * TkSelUpdateClipboard --
 *
 *	This function is called to force the clipboard to be updated
 *	after new data is added.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Clears the current contents of the clipboard.
 *
 *----------------------------------------------------------------------
 */

void
TkSelUpdateClipboard(winPtr, targetPtr)
    TkWindow *winPtr;
    TkClipboardTarget *targetPtr;
{
    HWND hwnd = TkWinGetHWND(winPtr->window);

    OpenClipboard(hwnd);
    EmptyClipboard();
    SetClipboardData(CF_TEXT, NULL);
    CloseClipboard();
}

/*
 *--------------------------------------------------------------
 *
 * TkSelEventProc --
 *
 *	This procedure is invoked whenever a selection-related
 *	event occurs. 
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
    if (eventPtr->type == SelectionClear) {
	TkSelClearSelection(tkwin, eventPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkSelPropProc --
 *
 *	This procedure is invoked when property-change events
 *	occur on windows not known to the toolkit.  This is a stub
 *	function under Windows.
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
TkSelPropProc(eventPtr)
    register XEvent *eventPtr;		/* X PropertyChange event. */
{
}

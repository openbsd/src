/*
 * tkClipboard.c --
 *
 * 	This file manages the clipboard for the Tk toolkit,
 * 	maintaining a collection of data buffers that will be
 * 	supplied on demand to requesting applications.
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkClipboard.c 1.14 96/02/15 18:52:37
 */

#include "tkInt.h"
#include "tkPort.h"
#include "tkSelect.h"

/*
 * Prototypes for procedures used only in this file:
 */

static int		ClipboardAppHandler _ANSI_ARGS_((ClientData clientData,
			    int offset, char *buffer, int maxBytes));
static int		ClipboardHandler _ANSI_ARGS_((ClientData clientData,
			    int offset, char *buffer, int maxBytes));
static int		ClipboardWindowHandler _ANSI_ARGS_((
			    ClientData clientData, int offset, char *buffer,
			    int maxBytes));
static void		ClipboardLostSel _ANSI_ARGS_((ClientData clientData));

/*
 *----------------------------------------------------------------------
 *
 * ClipboardHandler --
 *
 *	This procedure acts as selection handler for the
 *	clipboard manager.  It extracts the required chunk of
 *	data from the buffer chain for a given selection target.
 *
 * Results:
 *	The return value is a count of the number of bytes
 *	actually stored at buffer.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ClipboardHandler(clientData, offset, buffer, maxBytes)
    ClientData clientData;	/* Information about data to fetch. */
    int offset;			/* Return selection bytes starting at this
				 * offset. */
    char *buffer;		/* Place to store converted selection. */
    int maxBytes;		/* Maximum # of bytes to store at buffer. */
{
    TkClipboardTarget *targetPtr = (TkClipboardTarget*) clientData;
    TkClipboardBuffer *cbPtr;
    char *srcPtr, *destPtr;
    int count = 0;
    int scanned = 0;
    size_t length, freeCount;

    /*
     * Skip to buffer containing offset byte
     */

    for (cbPtr = targetPtr->firstBufferPtr; ; cbPtr = cbPtr->nextPtr) {
	if (cbPtr == NULL) {
	    return 0;
	}
	if (scanned + cbPtr->length > offset) {
	    break;
	}
	scanned += cbPtr->length;
    }

    /*
     * Copy up to maxBytes or end of list, switching buffers as needed.
     */

    freeCount = maxBytes;
    srcPtr = cbPtr->buffer + (offset - scanned);
    destPtr = buffer;
    length = cbPtr->length - (offset - scanned);
    while (1) {
	if (length > freeCount) {
	    strncpy(destPtr, srcPtr, freeCount);
	    return maxBytes;
	} else {
	    strncpy(destPtr, srcPtr, length);
	    destPtr += length;
	    count += length;
	    freeCount -= length;
	}
	cbPtr = cbPtr->nextPtr;
	if (cbPtr == NULL) {
	    break;
	}
	srcPtr = cbPtr->buffer;
	length = cbPtr->length;
    }
    return count;
}

/*
 *----------------------------------------------------------------------
 *
 * ClipboardAppHandler --
 *
 *	This procedure acts as selection handler for retrievals of type
 *	TK_APPLICATION.  It returns the name of the application that
 *	owns the clipboard.  Note:  we can't use the default Tk
 *	selection handler for this selection type, because the clipboard
 *	window isn't a "real" window and doesn't have the necessary
 *	information.
 *
 * Results:
 *	The return value is a count of the number of bytes
 *	actually stored at buffer.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ClipboardAppHandler(clientData, offset, buffer, maxBytes)
    ClientData clientData;	/* Pointer to TkDisplay structure. */
    int offset;			/* Return selection bytes starting at this
				 * offset. */
    char *buffer;		/* Place to store converted selection. */
    int maxBytes;		/* Maximum # of bytes to store at buffer. */
{
    TkDisplay *dispPtr = (TkDisplay *) clientData;
    size_t length;
    char *p;

    p = dispPtr->clipboardAppPtr->winPtr->nameUid;
    length = strlen(p);
    length -= offset;
    if (length <= 0) {
	return 0;
    }
    if (length > maxBytes) {
	length = maxBytes;
    }
    strncpy(buffer, p, length);
    return length;
}

/*
 *----------------------------------------------------------------------
 *
 * ClipboardWindowHandler --
 *
 *	This procedure acts as selection handler for retrievals of
 *	type TK_WINDOW.  Since the clipboard doesn't correspond to
 *	any particular window, we just return ".".  We can't use Tk's
 *	default handler for this selection type, because the clipboard
 *	window isn't a valid window.
 *
 * Results:
 *	The return value is 1, the number of non-null bytes stored
 *	at buffer.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ClipboardWindowHandler(clientData, offset, buffer, maxBytes)
    ClientData clientData;	/* Not used. */
    int offset;			/* Return selection bytes starting at this
				 * offset. */
    char *buffer;		/* Place to store converted selection. */
    int maxBytes;		/* Maximum # of bytes to store at buffer. */
{
    buffer[0] = '.';
    buffer[1] = 0;
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * ClipboardLostSel --
 *
 *	This procedure is invoked whenever clipboard ownership is
 *	claimed by another window.  It just sets a flag so that we
 *	know the clipboard was taken away.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The clipboard is marked as inactive.
 *
 *----------------------------------------------------------------------
 */

static void
ClipboardLostSel(clientData)
    ClientData clientData;		/* Pointer to TkDisplay structure. */
{
    TkDisplay *dispPtr = (TkDisplay*) clientData;

    dispPtr->clipboardActive = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ClipboardClear --
 *
 *	Take control of the clipboard and clear out the previous
 *	contents.  This procedure must be invoked before any
 *	calls to Tk_AppendToClipboard.
 *
 * Results:
 *	A standard Tcl result.  If an error occurs, an error message is
 *	left in interp->result.
 *
 * Side effects:
 *	From now on, requests for the CLIPBOARD selection will be
 *	directed to the clipboard manager routines associated with
 *	clipWindow for the display of tkwin.  In order to guarantee
 *	atomicity, no event handling should occur between
 *	Tk_ClipboardClear and the following Tk_AppendToClipboard
 *	calls.  This procedure may cause a user-defined LostSel command 
 * 	to be invoked when the CLIPBOARD is claimed, so any calling
 *	function should be reentrant at the point Tk_ClipboardClear is
 *	invoked.
 *
 *----------------------------------------------------------------------
 */

int
Tk_ClipboardClear(interp, tkwin)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tk_Window tkwin;		/* Window in application that is clearing
				 * clipboard;  identifies application and
				 * display. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkDisplay *dispPtr = winPtr->dispPtr;
    TkClipboardTarget *targetPtr, *nextTargetPtr;
    TkClipboardBuffer *cbPtr, *nextCbPtr;

    if (dispPtr->clipWindow == NULL) {
	int result;

	result = TkClipInit(interp, dispPtr);
	if (result != TCL_OK) {
	    return result;
	}
    }

    /*
     * Discard any existing clipboard data and delete the selection
     * handler(s) associated with that data.
     */

    for (targetPtr = dispPtr->clipTargetPtr; targetPtr != NULL;
	    targetPtr = nextTargetPtr) {
	for (cbPtr = targetPtr->firstBufferPtr; cbPtr != NULL;
		cbPtr = nextCbPtr) {
	    ckfree(cbPtr->buffer);
	    nextCbPtr = cbPtr->nextPtr;
	    ckfree((char *) cbPtr);
	}
	nextTargetPtr = targetPtr->nextPtr;
	Tk_DeleteSelHandler(dispPtr->clipWindow, dispPtr->clipboardAtom,
		targetPtr->type);
	ckfree((char *) targetPtr);
    }
    dispPtr->clipTargetPtr = NULL;

    /*
     * Reclaim the clipboard selection if we lost it.
     */

    if (!dispPtr->clipboardActive) {
	Tk_OwnSelection(dispPtr->clipWindow, dispPtr->clipboardAtom,
		ClipboardLostSel, (ClientData) dispPtr);
	dispPtr->clipboardActive = 1;
    }
    dispPtr->clipboardAppPtr = winPtr->mainPtr;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ClipboardAppend --
 *
 * 	Append a buffer of data to the clipboard.  The first buffer of
 *	a given type determines the format for that type.  Any successive
 *	appends to that type must have the same format or an error will
 *	be returned.  Tk_ClipboardClear must be called before a sequence
 *	of Tk_ClipboardAppend calls can be issued.  In order to guarantee
 *	atomicity, no event handling should occur between Tk_ClipboardClear
 *	and the following Tk_AppendToClipboard calls.
 *
 * Results:
 *	A standard Tcl result.  If an error is returned, an error message
 *	is left in interp->result.
 *
 * Side effects:
 * 	The specified buffer will be copied onto the end of the clipboard.
 *	The clipboard maintains a list of buffers which will be used to
 *	supply the data for a selection get request.  The first time a given
 *	type is appended, Tk_ClipboardAppend will register a selection
 * 	handler of the appropriate type.
 *
 *----------------------------------------------------------------------
 */

int
Tk_ClipboardAppend(interp, tkwin, type, format, buffer)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tk_Window tkwin;		/* Window that selects a display. */
    Atom type;			/* The desired conversion type for this
				 * clipboard item, e.g. STRING or LENGTH. */
    Atom format;		/* Format in which the selection
				 * information should be returned to
				 * the requestor. */
    char* buffer;		/* NULL terminated string containing the data
				 * to be added to the clipboard. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkDisplay *dispPtr = winPtr->dispPtr;
    TkClipboardTarget *targetPtr;
    TkClipboardBuffer *cbPtr;

    /*
     * If this application doesn't already own the clipboard, clear
     * the clipboard.  If we don't own the clipboard selection, claim it.
     */

    if (dispPtr->clipboardAppPtr != winPtr->mainPtr) {
	Tk_ClipboardClear(interp, tkwin);
    } else if (!dispPtr->clipboardActive) {
	Tk_OwnSelection(dispPtr->clipWindow, dispPtr->clipboardAtom,
		ClipboardLostSel, (ClientData) dispPtr);
	dispPtr->clipboardActive = 1;
    }

    /*
     * Check to see if the specified target is already present on the
     * clipboard.  If it isn't, we need to create a new target; otherwise,
     * we just append the new buffer to the clipboard list.
     */

    for (targetPtr = dispPtr->clipTargetPtr; targetPtr != NULL;
	    targetPtr = targetPtr->nextPtr) {
	if (targetPtr->type == type)
	    break;
    }
    if (targetPtr == NULL) {
	targetPtr = (TkClipboardTarget*) ckalloc(sizeof(TkClipboardTarget));
	targetPtr->type = type;
	targetPtr->format = format;
	targetPtr->firstBufferPtr = targetPtr->lastBufferPtr = NULL;
	targetPtr->nextPtr = dispPtr->clipTargetPtr;
	dispPtr->clipTargetPtr = targetPtr;
	Tk_CreateSelHandler(dispPtr->clipWindow, dispPtr->clipboardAtom,
		type, ClipboardHandler, (ClientData) targetPtr, format);
    } else if (targetPtr->format != format) {
	Tcl_AppendResult(interp, "format \"", Tk_GetAtomName(tkwin, format),
		"\" does not match current format \"",
		Tk_GetAtomName(tkwin, targetPtr->format),"\" for ",
		Tk_GetAtomName(tkwin, type), (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Append a new buffer to the buffer chain.
     */

    cbPtr = (TkClipboardBuffer*) ckalloc(sizeof(TkClipboardBuffer));
    cbPtr->nextPtr = NULL;
    if (targetPtr->lastBufferPtr != NULL) {
	targetPtr->lastBufferPtr->nextPtr = cbPtr;
    } else {
	targetPtr->firstBufferPtr = cbPtr;
    }
    targetPtr->lastBufferPtr = cbPtr;

    cbPtr->length = strlen(buffer);
    cbPtr->buffer = (char *) ckalloc((unsigned) (cbPtr->length + 1));
    strcpy(cbPtr->buffer, buffer);

    TkSelUpdateClipboard((TkWindow*)(dispPtr->clipWindow), targetPtr);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_ClipboardCmd --
 *
 *	This procedure is invoked to process the "clipboard" Tcl
 *	command.  See the user documentation for details on what
 *	it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tk_ClipboardCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    char *path = NULL;
    size_t length;
    int count;
    char c;
    char **args;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "append", length) == 0)) {
	Atom target, format;
	char *targetName = NULL;
	char *formatName = NULL;

	for (count = argc-2, args = argv+2; count > 1; count -= 2, args += 2) {
	    if (args[0][0] != '-') {
		break;
	    }
	    c = args[0][1];
	    length = strlen(args[0]);
	    if ((c == '-') && (length == 2)) {
		args++;
		count--;
		break;
	    }
	    if ((c == 'd') && (strncmp(args[0], "-displayof", length) == 0)) {
		path = args[1];
	    } else if ((c == 'f')
		    && (strncmp(args[0], "-format", length) == 0)) {
		formatName = args[1];
	    } else if ((c == 't')
		    && (strncmp(args[0], "-type", length) == 0)) {
		targetName = args[1];
	    } else {
		Tcl_AppendResult(interp, "unknown option \"", args[0],
			"\"", (char *) NULL);
		return TCL_ERROR;
	    }
	}
	if (count != 1) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " append ?options? data\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (path != NULL) {
	    tkwin = Tk_NameToWindow(interp, path, tkwin);
	}
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	if (targetName != NULL) {
	    target = Tk_InternAtom(tkwin, targetName);
	} else {
	    target = XA_STRING;
	}
	if (formatName != NULL) {
	    format = Tk_InternAtom(tkwin, formatName);
	} else {
	    format = XA_STRING;
	}
	return Tk_ClipboardAppend(interp, tkwin, target, format, args[0]);
    } else if ((c == 'c') && (strncmp(argv[1], "clear", length) == 0)) {
	for (count = argc-2, args = argv+2; count > 0; count -= 2, args += 2) {
	    if (args[0][0] != '-') {
		break;
	    }
	    if (count < 2) {
		Tcl_AppendResult(interp, "value for \"", *args,
			"\" missing", (char *) NULL);
		return TCL_ERROR;
	    }
	    c = args[0][1];
	    length = strlen(args[0]);
	    if ((c == 'd') && (strncmp(args[0], "-displayof", length) == 0)) {
		path = args[1];
	    } else {
		Tcl_AppendResult(interp, "unknown option \"", args[0],
			"\"", (char *) NULL);
		return TCL_ERROR;
	    }
	}
	if (count > 0) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " clear ?options?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (path != NULL) {
	    tkwin = Tk_NameToWindow(interp, path, tkwin);
	}
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
	return Tk_ClipboardClear(interp, tkwin);
    } else {
	sprintf(interp->result,
		"bad option \"%.50s\": must be clear or append",
		argv[1]);
	return TCL_ERROR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkClipInit --
 *
 *	This procedure is called to initialize the window for claiming
 *	clipboard ownership and for receiving selection get results.  This
 *	function is called from tkSelect.c as well as tkClipboard.c.
 *
 * Results:
 *	The result is a standard Tcl return value, which is normally TCL_OK.
 *	If an error occurs then an error message is left in interp->result
 *	and TCL_ERROR is returned.
 *
 * Side effects:
 *	Sets up the clipWindow and related data structures.
 *
 *----------------------------------------------------------------------
 */

int
TkClipInit(interp, dispPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error
				 * reporting. */
    register TkDisplay *dispPtr;/* Display to initialize. */
{
    XSetWindowAttributes atts;

    dispPtr->clipTargetPtr = NULL;
    dispPtr->clipboardActive = 0;
    dispPtr->clipboardAppPtr = NULL;
    
    /*
     * Create the window used for clipboard ownership and selection retrieval,
     * and set up an event handler for it.
     */

    dispPtr->clipWindow = Tk_CreateWindow(interp, (Tk_Window) NULL,
	    "_clip", DisplayString(dispPtr->display));
    if (dispPtr->clipWindow == NULL) {
	return TCL_ERROR;
    }
    atts.override_redirect = True;
    Tk_ChangeWindowAttributes(dispPtr->clipWindow, CWOverrideRedirect, &atts);
    Tk_MakeWindowExist(dispPtr->clipWindow);

    if (dispPtr->multipleAtom == None) {
	/*
	 * Need to invoke selection initialization to make sure that
	 * atoms we depend on below are defined.
	 */

	TkSelInit(dispPtr->clipWindow);
    }

    /*
     * Create selection handlers for types TK_APPLICATION and TK_WINDOW
     * on this window.  Can't use the default handlers for these types
     * because this isn't a full-fledged window.
     */

    Tk_CreateSelHandler(dispPtr->clipWindow, dispPtr->clipboardAtom,
	    dispPtr->applicationAtom, ClipboardAppHandler,
	    (ClientData) dispPtr, XA_STRING);
    Tk_CreateSelHandler(dispPtr->clipWindow, dispPtr->clipboardAtom,
	    dispPtr->windowAtom, ClipboardWindowHandler,
	    (ClientData) dispPtr, XA_STRING);
    return TCL_OK;
}

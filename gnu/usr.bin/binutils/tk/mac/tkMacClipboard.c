/*
 * tkMacClipboard.c --
 *
 * 	This file manages the clipboard for the Tk toolkit,
 * 	it is much like the generic version with a few small changes
 *	and support for the Macintosh scrap.  Eventually, this file
 *	will be specialized for the Mac & will use the generic 
 *	clipboard code
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacClipboard.c 1.10 96/02/15 18:55:43
 */

#include "tkInt.h"
#include "tkPort.h"
#include "tkMacInt.h"

#include <Scrap.h>
#include <Events.h>

/*
 * The clipboard contains a list of buffers of various types and formats.
 * All of the buffers of a given type will be returned in sequence when the
 * CLIPBOARD selection is retrieved.  All buffers of a given type on the
 * same clipboard must have the same format.  The TkClipboardTarget structure
 * is used to record the information about a chain of buffers of the same
 * type.
 */

typedef struct ClipboardBuffer {
    char *buffer;			/* Null terminated data buffer. */
    long length;			/* Length of string in buffer. */
    struct ClipboardBuffer *nextPtr;	/* Next in list of buffers.  NULL
					 * means end of list . */
} ClipboardBuffer;

typedef struct TkClipboardTarget {
    Atom type;				/* Type conversion supported. */
    Atom format;			/* Representation used for data. */
    ClipboardBuffer *firstBufferPtr;	/* First in list of data buffers. */
    ClipboardBuffer *lastBufferPtr;	/* Last in list of clipboard buffers.
					 * Used to speed up appends. */
    struct TkClipboardTarget *nextPtr;	/* Next in list of targets on
					 * clipboard.  NULL means end of
					 * list. */
} TkClipboardTarget;

static int globalScrapChanged = false;
static int localScrapChanged = false;
char * localScrap = NULL;

/*
 * Prototypes for procedures used only in this file:
 */

static int		ClipboardHandler _ANSI_ARGS_((ClientData clientData,
			    int offset, char *buffer, int maxBytes));
static void		ClipboardLostSel _ANSI_ARGS_((ClientData clientData));
static int		SelGetProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *portion));

/*
 *--------------------------------------------------------------
 *
 * SelGetProc --
 *
 *	This procedure is invoked to process pieces of the selection
 *	as they arrive during "selection get" commands.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side effects:
 *	Bytes get appended to the dynamic string pointed to by the
 *	clientData argument.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
SelGetProc(clientData, interp, portion)
    ClientData clientData;	/* Dynamic string holding partially
				 * assembled selection. */
    Tcl_Interp *interp;		/* Interpreter used for error
				 * reporting (not used). */
    char *portion;		/* New information to be appended. */
{
    Tcl_DStringAppend((Tcl_DString *) clientData, portion, -1);
    return TCL_OK;
}

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
    ClipboardBuffer *cbPtr;
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
    ClipboardBuffer *cbPtr, *nextCbPtr;

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

    /* TODO: is there a way to get rid of this? */
    localScrapChanged = true;
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
    ClipboardBuffer *cbPtr;

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

    cbPtr = (ClipboardBuffer*) ckalloc(sizeof(ClipboardBuffer));
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

    /* TODO: is there a way to get rid of this? */
    localScrapChanged = true;
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

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkSuspendClipboard --
 *
 *	Handle clipboard conversion as required by the suppend event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The local scrap is moved to the global scrap.
 *
 *----------------------------------------------------------------------
 */

void
TkSuspendClipboard()
{
    Tcl_DString selBytes;
    Atom selection;
    int result;
    Tcl_Interp *dummy;
    Tk_Window tkwin;
    char *ptr;

    if (localScrapChanged == true) {
    	/*
    	 * Move the local scrap to the global scrap.
    	 */
	localScrapChanged = false;

	Tcl_DStringInit(&selBytes);
	tkwin = (Tk_Window) tkMainWindowList->winPtr;
	selection = Tk_InternAtom(tkwin, "CLIPBOARD");
	dummy = Tcl_CreateInterp();
	/* TODO: this only supports strings */
	result = Tk_GetSelection(dummy, tkwin, selection,
		XA_STRING, SelGetProc, (ClientData) &selBytes);
	Tcl_DeleteInterp(dummy);
	
	if (result == TCL_OK) {
	    ZeroScrap();
	    /* 
	     * Convert newlines to carrige returns
	     */
	    for (ptr = selBytes.string; *ptr != NULL; ptr++) {
	    	if (*ptr == '\n') {
	    	    *ptr = '\r';
	    	}
	    }
	    PutScrap((long) (strlen(selBytes.string) + 1),
		    'TEXT', selBytes.string);
	    Tcl_DStringFree(&selBytes);
	} else {
	    ZeroScrap();
	    Tcl_DStringFree(&selBytes);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkResumeClipboard --
 *
 *	Handle clipboard conversion as required by the resume event.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The global scrap is check to see it has changed.
 *
 *----------------------------------------------------------------------
 */

void
TkResumeClipboard()
{
    Handle handle;
    long length, offset = 0;
    Tk_Window tkwin;
    Tcl_Interp *dummy;
    
    globalScrapChanged = true;
    handle = NewHandle(1);
    length = GetScrap(handle, 'TEXT', &offset);
    if (length > 0) {
	tkwin = (Tk_Window) tkMainWindowList->winPtr;
	SetHandleSize(handle, (Size) length + 1);
	HLock(handle);
	(*handle)[length] = '\0';
	dummy = Tcl_CreateInterp();
	Tk_ClipboardClear(dummy, tkwin);
	Tk_ClipboardAppend(dummy, tkwin, XA_STRING, XA_STRING, *handle);
	Tcl_DeleteInterp(dummy);
	HUnlock(handle);
	localScrapChanged = false;
    }
    DisposeHandle(handle);
}

/* 
 * tkTextWind.c --
 *
 *	This file contains code that allows arbitrary windows to be
 *	nested inside text widgets.  It also implements the "window"
 *	widget command for texts.
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkTextWind.c 1.13 96/02/15 18:53:02
 */

#include "tk.h"
#include "tkText.h"
#include "tkPort.h"

/*
 * The following structure is the official type record for the
 * embedded window geometry manager:
 */

static void		EmbWinRequestProc _ANSI_ARGS_((ClientData clientData,
			    Tk_Window tkwin));
static void		EmbWinLostSlaveProc _ANSI_ARGS_((ClientData clientData,
			    Tk_Window tkwin));

static Tk_GeomMgr textGeomType = {
    "text",			/* name */
    EmbWinRequestProc,		/* requestProc */
    EmbWinLostSlaveProc,	/* lostSlaveProc */
};

/*
 * Definitions for alignment values:
 */

#define ALIGN_BOTTOM		0
#define ALIGN_CENTER		1
#define ALIGN_TOP		2
#define ALIGN_BASELINE		3

/*
 * Macro that determines the size of an embedded window segment:
 */

#define EW_SEG_SIZE ((unsigned) (Tk_Offset(TkTextSegment, body) \
	+ sizeof(TkTextEmbWindow)))

/*
 * Prototypes for procedures defined in this file:
 */

static int		AlignParseProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Tk_Window tkwin, char *value,
			    char *widgRec, int offset));
static char *		AlignPrintProc _ANSI_ARGS_((ClientData clientData,
			    Tk_Window tkwin, char *widgRec, int offset,
			    Tcl_FreeProc **freeProcPtr));
static TkTextSegment *	EmbWinCleanupProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));
static void		EmbWinCheckProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr));
static void		EmbWinBboxProc _ANSI_ARGS_((TkTextDispChunk *chunkPtr,
			    int index, int y, int lineHeight, int baseline,
			    int *xPtr, int *yPtr, int *widthPtr,
			    int *heightPtr));
static int		EmbWinConfigure _ANSI_ARGS_((TkText *textPtr,
			    TkTextSegment *ewPtr, int argc, char **argv));
static void		EmbWinDelayedUnmap _ANSI_ARGS_((
			    ClientData clientData));
static int		EmbWinDeleteProc _ANSI_ARGS_((TkTextSegment *segPtr,
			    TkTextLine *linePtr, int treeGone));
static void		EmbWinDisplayProc _ANSI_ARGS_((
			    TkTextDispChunk *chunkPtr, int x, int y,
			    int lineHeight, int baseline, Display *display,
			    Drawable dst, int screenY));
static int		EmbWinLayoutProc _ANSI_ARGS_((TkText *textPtr,
			    TkTextIndex *indexPtr, TkTextSegment *segPtr,
			    int offset, int maxX, int maxChars,
			    int noCharsYet, Tk_Uid wrapMode,
			    TkTextDispChunk *chunkPtr));
static void		EmbWinStructureProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		EmbWinUndisplayProc _ANSI_ARGS_((TkText *textPtr,
			    TkTextDispChunk *chunkPtr));

/*
 * The following structure declares the "embedded window" segment type.
 */

static Tk_SegType tkTextEmbWindowType = {
    "window",					/* name */
    0,						/* leftGravity */
    (Tk_SegSplitProc *) NULL,			/* splitProc */
    EmbWinDeleteProc,				/* deleteProc */
    EmbWinCleanupProc,				/* cleanupProc */
    (Tk_SegLineChangeProc *) NULL,		/* lineChangeProc */
    EmbWinLayoutProc,				/* layoutProc */
    EmbWinCheckProc				/* checkProc */
};

/*
 * Information used for parsing window configuration options:
 */

static Tk_CustomOption alignOption = {AlignParseProc, AlignPrintProc,
	(ClientData) NULL};

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_CUSTOM, "-align", (char *) NULL, (char *) NULL,
	"center", 0, TK_CONFIG_DONT_SET_DEFAULT, &alignOption},
    {TK_CONFIG_STRING, "-create", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextEmbWindow, create),
	TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-padx", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(TkTextEmbWindow, padX),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_INT, "-pady", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(TkTextEmbWindow, padY),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-stretch", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(TkTextEmbWindow, stretch),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_WINDOW, "-window", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TkTextEmbWindow, tkwin),
	TK_CONFIG_DONT_SET_DEFAULT|TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 *--------------------------------------------------------------
 *
 * TkTextWindowCmd --
 *
 *	This procedure implements the "window" widget command
 *	for text widgets.  See the user documentation for details
 *	on what it does.
 *
 * Results:
 *	A standard Tcl result or error.
 *
 * Side effects:
 *	See the user documentation.
 *
 *--------------------------------------------------------------
 */

int
TkTextWindowCmd(textPtr, interp, argc, argv)
    register TkText *textPtr;	/* Information about text widget. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings.  Someone else has already
				 * parsed this command enough to know that
				 * argv[1] is "window". */
{
    int c;
    size_t length;
    register TkTextSegment *ewPtr;

    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " window option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[2][0];
    length = strlen(argv[2]);
    if ((strncmp(argv[2], "cget", length) == 0) && (length >= 2)) {
	TkTextIndex index;
	TkTextSegment *ewPtr;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " window cget index option\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (TkTextGetIndex(interp, textPtr, argv[3], &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	ewPtr = TkTextIndexToSeg(&index, (int *) NULL);
	if (ewPtr->typePtr != &tkTextEmbWindowType) {
	    Tcl_AppendResult(interp, "no embedded window at index \"",
		    argv[3], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	return Tk_ConfigureValue(interp, textPtr->tkwin, configSpecs,
		(char *) &ewPtr->body.ew, argv[4], 0);
    } else if ((strncmp(argv[2], "configure", length) == 0) && (length >= 2)) {
	TkTextIndex index;
	TkTextSegment *ewPtr;

	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " window configure index ?option value ...?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (TkTextGetIndex(interp, textPtr, argv[3], &index) != TCL_OK) {
	    return TCL_ERROR;
	}
	ewPtr = TkTextIndexToSeg(&index, (int *) NULL);
	if (ewPtr->typePtr != &tkTextEmbWindowType) {
	    Tcl_AppendResult(interp, "no embedded window at index \"",
		    argv[3], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 4) {
	    return Tk_ConfigureInfo(interp, textPtr->tkwin, configSpecs,
		    (char *) &ewPtr->body.ew, (char *) NULL, 0);
	} else if (argc == 5) {
	    return Tk_ConfigureInfo(interp, textPtr->tkwin, configSpecs,
		    (char *) &ewPtr->body.ew, argv[4], 0);
	} else {
	    TkTextChanged(textPtr, &index, &index);
	    return EmbWinConfigure(textPtr, ewPtr, argc-4, argv+4);
	}
    } else if ((strncmp(argv[2], "create", length) == 0) && (length >= 2)) {
	TkTextIndex index;
	int lineIndex;

	/*
	 * Add a new window.  Find where to put the new window, and
	 * mark that position for redisplay.
	 */

	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " window create index ?option value ...?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (TkTextGetIndex(interp, textPtr, argv[3], &index) != TCL_OK) {
	    return TCL_ERROR;
	}

	/*
	 * Don't allow insertions on the last (dummy) line of the text.
	 */
    
	lineIndex = TkBTreeLineIndex(index.linePtr);
	if (lineIndex == TkBTreeNumLines(textPtr->tree)) {
	    lineIndex--;
	    TkTextMakeIndex(textPtr->tree, lineIndex, 1000000, &index);
	}

	/*
	 * Create the new window segment and initialize it.
	 */

	ewPtr = (TkTextSegment *) ckalloc(EW_SEG_SIZE);
	ewPtr->typePtr = &tkTextEmbWindowType;
	ewPtr->size = 1;
	ewPtr->body.ew.textPtr = textPtr;
	ewPtr->body.ew.linePtr = NULL;
	ewPtr->body.ew.tkwin = NULL;
	ewPtr->body.ew.create = NULL;
	ewPtr->body.ew.align = ALIGN_CENTER;
	ewPtr->body.ew.padX = ewPtr->body.ew.padY = 0;
	ewPtr->body.ew.stretch = 0;
	ewPtr->body.ew.chunkCount = 0;
	ewPtr->body.ew.displayed = 0;

	/*
	 * Link the segment into the text widget, then configure it (delete
	 * it again if the configuration fails).
	 */

	TkTextChanged(textPtr, &index, &index);
	TkBTreeLinkSegment(ewPtr, &index);
	if (EmbWinConfigure(textPtr, ewPtr, argc-4, argv+4) != TCL_OK) {
	    TkTextIndex index2;

	    TkTextIndexForwChars(&index, 1, &index2);
	    TkBTreeDeleteChars(&index, &index2);
	    return TCL_ERROR;
	}
    } else if (strncmp(argv[2], "names", length) == 0) {
	Tcl_HashSearch search;
	Tcl_HashEntry *hPtr;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " window names\"", (char *) NULL);
	    return TCL_ERROR;
	}
	for (hPtr = Tcl_FirstHashEntry(&textPtr->windowTable, &search);
		hPtr != NULL; hPtr = Tcl_NextHashEntry(&search)) {
	    Tcl_AppendElement(interp,
		    Tcl_GetHashKey(&textPtr->markTable, hPtr));
	}
    } else {
	Tcl_AppendResult(interp, "bad window option \"", argv[2],
		"\": must be cget, configure, create, or names",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinConfigure --
 *
 *	This procedure is called to handle configuration options
 *	for an embedded window, using an argc/argv list.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message..
 *
 * Side effects:
 *	Configuration information for the embedded window changes,
 *	such as alignment, stretching, or name of the embedded
 *	window.
 *
 *--------------------------------------------------------------
 */

static int
EmbWinConfigure(textPtr, ewPtr, argc, argv)
    TkText *textPtr;		/* Information about text widget that
				 * contains embedded window. */
    TkTextSegment *ewPtr;	/* Embedded window to be configured. */
    int argc;			/* Number of strings in argv. */
    char **argv;		/* Array of strings describing configuration
				 * options. */
{
    Tk_Window oldWindow;
    Tcl_HashEntry *hPtr;
    int new;

    oldWindow = ewPtr->body.ew.tkwin;
    if (Tk_ConfigureWidget(textPtr->interp, textPtr->tkwin, configSpecs,
	    argc, argv, (char *) &ewPtr->body.ew, TK_CONFIG_ARGV_ONLY)
	    != TCL_OK) {
	return TCL_ERROR;
    }
    if (oldWindow != ewPtr->body.ew.tkwin) {
	if (oldWindow != NULL) {
	    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&textPtr->windowTable,
		    Tk_PathName(oldWindow)));
	    Tk_DeleteEventHandler(oldWindow, StructureNotifyMask,
		    EmbWinStructureProc, (ClientData) ewPtr);
	    Tk_ManageGeometry(oldWindow, (Tk_GeomMgr *) NULL,
		    (ClientData) NULL);
	    if (textPtr->tkwin != Tk_Parent(oldWindow)) {
		Tk_UnmaintainGeometry(oldWindow, textPtr->tkwin);
	    } else {
		Tk_UnmapWindow(oldWindow);
	    }
	}
	if (ewPtr->body.ew.tkwin != NULL) {
	    Tk_Window ancestor, parent;

	    /*
	     * Make sure that the text is either the parent of the
	     * embedded window or a descendant of that parent.  Also,
	     * don't allow a top-level window to be managed inside
	     * a text.
	     */

	    parent = Tk_Parent(ewPtr->body.ew.tkwin);
	    for (ancestor = textPtr->tkwin; ;
		    ancestor = Tk_Parent(ancestor)) {
		if (ancestor == parent) {
		    break;
		}
		if (Tk_IsTopLevel(ancestor)) {
		    badMaster:
		    Tcl_AppendResult(textPtr->interp, "can't embed ",
			    Tk_PathName(ewPtr->body.ew.tkwin), " in ",
			    Tk_PathName(textPtr->tkwin), (char *) NULL);
		    ewPtr->body.ew.tkwin = NULL;
		    return TCL_ERROR;
		}
	    }
	    if (Tk_IsTopLevel(ewPtr->body.ew.tkwin)
		    || (ewPtr->body.ew.tkwin == textPtr->tkwin)) {
		goto badMaster;
	    }

	    /*
	     * Take over geometry management for the window, plus create
	     * an event handler to find out when it is deleted.
	     */

	    Tk_ManageGeometry(ewPtr->body.ew.tkwin, &textGeomType,
		    (ClientData) ewPtr);
	    Tk_CreateEventHandler(ewPtr->body.ew.tkwin, StructureNotifyMask,
		    EmbWinStructureProc, (ClientData) ewPtr);

	    /*
	     * Special trick!  Must enter into the hash table *after*
	     * calling Tk_ManageGeometry:  if the window was already managed
	     * elsewhere in this text, the Tk_ManageGeometry call will cause
	     * the entry to be removed, which could potentially lose the new
	     * entry.
	     */

	    hPtr = Tcl_CreateHashEntry(&textPtr->windowTable,
		    Tk_PathName(ewPtr->body.ew.tkwin), &new);
	    Tcl_SetHashValue(hPtr, ewPtr);

	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * AlignParseProc --
 *
 *	This procedure is invoked by Tk_ConfigureWidget during
 *	option processing to handle "-align" options for embedded
 *	windows.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	The alignment for the embedded window may change.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
AlignParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;		/* Not used.*/
    Tcl_Interp *interp;			/* Used for reporting errors. */
    Tk_Window tkwin;			/* Window for text widget. */
    char *value;			/* Value of option. */
    char *widgRec;			/* Pointer to TkTextEmbWindow
					 * structure. */
    int offset;				/* Offset into item (ignored). */
{
    register TkTextEmbWindow *embPtr = (TkTextEmbWindow *) widgRec;

    if (strcmp(value, "baseline") == 0) {
	embPtr->align = ALIGN_BASELINE;
    } else if (strcmp(value, "bottom") == 0) {
	embPtr->align = ALIGN_BOTTOM;
    } else if (strcmp(value, "center") == 0) {
	embPtr->align = ALIGN_CENTER;
    } else if (strcmp(value, "top") == 0) {
	embPtr->align = ALIGN_TOP;
    } else {
	Tcl_AppendResult(interp, "bad alignment \"", value,
		"\": must be baseline, bottom, center, or top",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * AlignPrintProc --
 *
 *	This procedure is invoked by the Tk configuration code
 *	to produce a printable string for the "-align" configuration
 *	option for embedded windows.
 *
 * Results:
 *	The return value is a string describing the embedded
 *	window's current alignment.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static char *
AlignPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;		/* Ignored. */
    Tk_Window tkwin;			/* Window for text widget. */
    char *widgRec;			/* Pointer to TkTextEmbWindow
					 * structure. */
    int offset;				/* Ignored. */
    Tcl_FreeProc **freeProcPtr;		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    switch (((TkTextEmbWindow *) widgRec)->align) {
	case ALIGN_BASELINE:
	    return "baseline";
	case ALIGN_BOTTOM:
	    return "bottom";
	case ALIGN_CENTER:
	    return "center";
	case ALIGN_TOP:
	    return "top";
	default:
	    return "??";
    }
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinStructureProc --
 *
 *	This procedure is invoked by the Tk event loop whenever
 *	StructureNotify events occur for a window that's embedded
 *	in a text widget.  This procedure's only purpose is to
 *	clean up when windows are deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is disassociated from the window segment, and
 *	the portion of the text is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinStructureProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to record describing window item. */
    XEvent *eventPtr;		/* Describes what just happened. */
{
    register TkTextSegment *ewPtr = (TkTextSegment *) clientData;
    TkTextIndex index;

    if (eventPtr->type != DestroyNotify) {
	return;
    }

    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&ewPtr->body.ew.textPtr->windowTable,
	    Tk_PathName(ewPtr->body.ew.tkwin)));
    ewPtr->body.ew.tkwin = NULL;
    index.tree = ewPtr->body.ew.textPtr->tree;
    index.linePtr = ewPtr->body.ew.linePtr;
    index.charIndex = TkTextSegToOffset(ewPtr, ewPtr->body.ew.linePtr);
    TkTextChanged(ewPtr->body.ew.textPtr, &index, &index);
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinRequestProc --
 *
 *	This procedure is invoked whenever a window that's associated
 *	with a window canvas item changes its requested dimensions.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The size and location on the screen of the window may change,
 *	depending on the options specified for the window item.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
EmbWinRequestProc(clientData, tkwin)
    ClientData clientData;		/* Pointer to record for window item. */
    Tk_Window tkwin;			/* Window that changed its desired
					 * size. */
{
    TkTextSegment *ewPtr = (TkTextSegment *) clientData;
    TkTextIndex index;

    index.tree = ewPtr->body.ew.textPtr->tree;
    index.linePtr = ewPtr->body.ew.linePtr;
    index.charIndex = TkTextSegToOffset(ewPtr, ewPtr->body.ew.linePtr);
    TkTextChanged(ewPtr->body.ew.textPtr, &index, &index);
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinLostSlaveProc --
 *
 *	This procedure is invoked by the Tk geometry manager when
 *	a slave window managed by a text widget is claimed away
 *	by another geometry manager.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is disassociated from the window segment, and
 *	the portion of the text is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinLostSlaveProc(clientData, tkwin)
    ClientData clientData;	/* Pointer to record describing window item. */
    Tk_Window tkwin;		/* Window that was claimed away by another
				 * geometry manager. */
{
    register TkTextSegment *ewPtr = (TkTextSegment *) clientData;
    TkTextIndex index;

    Tk_DeleteEventHandler(ewPtr->body.ew.tkwin, StructureNotifyMask,
	    EmbWinStructureProc, (ClientData) ewPtr);
    Tcl_CancelIdleCall(EmbWinDelayedUnmap, (ClientData) ewPtr);
    if (ewPtr->body.ew.textPtr->tkwin != Tk_Parent(tkwin)) {
	Tk_UnmaintainGeometry(tkwin, ewPtr->body.ew.textPtr->tkwin);
    } else {
	Tk_UnmapWindow(tkwin);
    }
    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&ewPtr->body.ew.textPtr->windowTable,
	    Tk_PathName(ewPtr->body.ew.tkwin)));
    ewPtr->body.ew.tkwin = NULL;
    index.tree = ewPtr->body.ew.textPtr->tree;
    index.linePtr = ewPtr->body.ew.linePtr;
    index.charIndex = TkTextSegToOffset(ewPtr, ewPtr->body.ew.linePtr);
    TkTextChanged(ewPtr->body.ew.textPtr, &index, &index);
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinDeleteProc --
 *
 *	This procedure is invoked by the text B-tree code whenever
 *	an embedded window lies in a range of characters being deleted.
 *
 * Results:
 *	Returns 0 to indicate that the deletion has been accepted.
 *
 * Side effects:
 *	The embedded window is deleted, if it exists, and any resources
 *	associated with it are released.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
EmbWinDeleteProc(ewPtr, linePtr, treeGone)
    TkTextSegment *ewPtr;		/* Segment being deleted. */
    TkTextLine *linePtr;		/* Line containing segment. */
    int treeGone;			/* Non-zero means the entire tree is
					 * being deleted, so everything must
					 * get cleaned up. */
{
    Tcl_HashEntry *hPtr;

    if (ewPtr->body.ew.tkwin != NULL) {
	hPtr = Tcl_FindHashEntry(&ewPtr->body.ew.textPtr->windowTable,
		Tk_PathName(ewPtr->body.ew.tkwin));
	if (hPtr != NULL) {
	    /*
	     * (It's possible for there to be no hash table entry for this
	     * window, if an error occurred while creating the window segment
	     * but before the window got added to the table)
	     */

	    Tcl_DeleteHashEntry(hPtr);
	}

	/*
	 * Delete the event handler for the window before destroying
	 * the window, so that EmbWinStructureProc doesn't get called
	 * (we'll already do everything that it would have done, and
	 * it will just get confused).
	 */

	Tk_DeleteEventHandler(ewPtr->body.ew.tkwin, StructureNotifyMask,
		EmbWinStructureProc, (ClientData) ewPtr);
	Tk_DestroyWindow(ewPtr->body.ew.tkwin);
    }
    Tcl_CancelIdleCall(EmbWinDelayedUnmap, (ClientData) ewPtr);
    Tk_FreeOptions(configSpecs, (char *) &ewPtr->body.ew,
	    ewPtr->body.ew.textPtr->display, 0);
    ckfree((char *) ewPtr);
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinCleanupProc --
 *
 *	This procedure is invoked by the B-tree code whenever a
 *	segment containing an embedded window is moved from one
 *	line to another.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The linePtr field of the segment gets updated.
 *
 *--------------------------------------------------------------
 */

static TkTextSegment *
EmbWinCleanupProc(ewPtr, linePtr)
    TkTextSegment *ewPtr;		/* Mark segment that's being moved. */
    TkTextLine *linePtr;		/* Line that now contains segment. */
{
    ewPtr->body.ew.linePtr = linePtr;
    return ewPtr;
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinLayoutProc --
 *
 *	This procedure is the "layoutProc" for embedded window
 *	segments.
 *
 * Results:
 *	1 is returned to indicate that the segment should be
 *	displayed.  The chunkPtr structure is filled in.
 *
 * Side effects:
 *	None, except for filling in chunkPtr.
 *
 *--------------------------------------------------------------
 */

	/*ARGSUSED*/
static int
EmbWinLayoutProc(textPtr, indexPtr, ewPtr, offset, maxX, maxChars,
	noCharsYet, wrapMode, chunkPtr)
    TkText *textPtr;		/* Text widget being layed out. */
    TkTextIndex *indexPtr;	/* Identifies first character in chunk. */
    TkTextSegment *ewPtr;	/* Segment corresponding to indexPtr. */
    int offset;			/* Offset within segPtr corresponding to
				 * indexPtr (always 0). */
    int maxX;			/* Chunk must not occupy pixels at this
				 * position or higher. */
    int maxChars;		/* Chunk must not include more than this
				 * many characters. */
    int noCharsYet;		/* Non-zero means no characters have been
				 * assigned to this line yet. */
    Tk_Uid wrapMode;		/* Wrap mode to use for line: tkTextCharUid,
				 * tkTextNoneUid, or tkTextWordUid. */
    register TkTextDispChunk *chunkPtr;
				/* Structure to fill in with information
				 * about this chunk.  The x field has already
				 * been set by the caller. */
{
    int width, height;

    if (offset != 0) {
	panic("Non-zero offset in EmbWinLayoutProc");
    }

    if ((ewPtr->body.ew.tkwin == NULL) && (ewPtr->body.ew.create != NULL)) {
	int code, new;
	Tcl_DString name;
	Tk_Window ancestor;
	Tcl_HashEntry *hPtr;

	/*
	 * The window doesn't currently exist.  Create it by evaluating
	 * the creation script.  The script must return the window's
	 * path name:  look up that name to get back to the window
	 * token.  Then register ourselves as the geometry manager for
	 * the window.
	 */

	code = Tcl_GlobalEval(textPtr->interp, ewPtr->body.ew.create);
	if (code != TCL_OK) {
	    createError:
	    Tcl_BackgroundError(textPtr->interp);
	    goto gotWindow;
	}
	Tcl_DStringInit(&name);
	Tcl_DStringAppend(&name, textPtr->interp->result, -1);
	Tcl_ResetResult(textPtr->interp);
	ewPtr->body.ew.tkwin = Tk_NameToWindow(textPtr->interp,
		Tcl_DStringValue(&name), textPtr->tkwin);
	if (ewPtr->body.ew.tkwin == NULL) {
	    goto createError;
	}
	for (ancestor = textPtr->tkwin; ;
		ancestor = Tk_Parent(ancestor)) {
	    if (ancestor == Tk_Parent(ewPtr->body.ew.tkwin)) {
		break;
	    }
	    if (Tk_IsTopLevel(ancestor)) {
		badMaster:
		Tcl_AppendResult(textPtr->interp, "can't embed ",
			Tk_PathName(ewPtr->body.ew.tkwin), " relative to ",
			Tk_PathName(textPtr->tkwin), (char *) NULL);
		Tcl_BackgroundError(textPtr->interp);
		ewPtr->body.ew.tkwin = NULL;
		goto gotWindow;
	    }
	}
	if (Tk_IsTopLevel(ewPtr->body.ew.tkwin)
		|| (textPtr->tkwin == ewPtr->body.ew.tkwin)) {
	    goto badMaster;
	}
	Tk_ManageGeometry(ewPtr->body.ew.tkwin, &textGeomType,
		(ClientData) ewPtr);
	Tk_CreateEventHandler(ewPtr->body.ew.tkwin, StructureNotifyMask,
		EmbWinStructureProc, (ClientData) ewPtr);

	/*
	 * Special trick!  Must enter into the hash table *after*
	 * calling Tk_ManageGeometry:  if the window was already managed
	 * elsewhere in this text, the Tk_ManageGeometry call will cause
	 * the entry to be removed, which could potentially lose the new
	 * entry.
	 */

	hPtr = Tcl_CreateHashEntry(&textPtr->windowTable,
		Tk_PathName(ewPtr->body.ew.tkwin), &new);
	Tcl_SetHashValue(hPtr, ewPtr);
    }

    /*
     * See if there's room for this window on this line.
     */

    gotWindow:
    if (ewPtr->body.ew.tkwin == NULL) {
	width = 0;
	height = 0;
    } else {
	width = Tk_ReqWidth(ewPtr->body.ew.tkwin) + 2*ewPtr->body.ew.padX;
	height = Tk_ReqHeight(ewPtr->body.ew.tkwin) + 2*ewPtr->body.ew.padY;
    }
    if ((width > (maxX - chunkPtr->x))
	    && !noCharsYet && (textPtr->wrapMode != tkTextNoneUid)) {
	return 0;
    }

    /*
     * Fill in the chunk structure.
     */

    chunkPtr->displayProc = EmbWinDisplayProc;
    chunkPtr->undisplayProc = EmbWinUndisplayProc;
    chunkPtr->measureProc = (Tk_ChunkMeasureProc *) NULL;
    chunkPtr->bboxProc = EmbWinBboxProc;
    chunkPtr->numChars = 1;
    if (ewPtr->body.ew.align == ALIGN_BASELINE) {
	chunkPtr->minAscent = height - ewPtr->body.ew.padY;
	chunkPtr->minDescent = ewPtr->body.ew.padY;
	chunkPtr->minHeight = 0;
    } else {
	chunkPtr->minAscent = 0;
	chunkPtr->minDescent = 0;
	chunkPtr->minHeight = height;
    }
    chunkPtr->width = width;
    chunkPtr->breakIndex = -1;
    chunkPtr->breakIndex = 1;
    chunkPtr->clientData = (ClientData) ewPtr;
    ewPtr->body.ew.chunkCount += 1;
    return 1;
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinCheckProc --
 *
 *	This procedure is invoked by the B-tree code to perform
 *	consistency checks on embedded windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The procedure panics if it detects anything wrong with
 *	the embedded window.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinCheckProc(ewPtr, linePtr)
    TkTextSegment *ewPtr;		/* Segment to check. */
    TkTextLine *linePtr;		/* Line containing segment. */
{
    if (ewPtr->nextPtr == NULL) {
	panic("EmbWinCheckProc: embedded window is last segment in line");
    }
    if (ewPtr->size != 1) {
	panic("EmbWinCheckProc: embedded window has size %d", ewPtr->size);
    }
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinDisplayProc --
 *
 *	This procedure is invoked by the text displaying code
 *	when it is time to actually draw an embedded window
 *	chunk on the screen.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The embedded window gets moved to the correct location
 *	and mapped onto the screen.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinDisplayProc(chunkPtr, x, y, lineHeight, baseline, display, dst, screenY)
    TkTextDispChunk *chunkPtr;		/* Chunk that is to be drawn. */
    int x;				/* X-position in dst at which to
					 * draw this chunk (differs from
					 * the x-position in the chunk because
					 * of scrolling). */
    int y;				/* Top of rectangular bounding box
					 * for line: tells where to draw this
					 * chunk in dst (x-position is in
					 * the chunk itself). */
    int lineHeight;			/* Total height of line. */
    int baseline;			/* Offset of baseline from y. */
    Display *display;			/* Display to use for drawing. */
    Drawable dst;			/* Pixmap or window in which to draw */
    int screenY;			/* Y-coordinate in text window that
					 * corresponds to y. */
{
    TkTextSegment *ewPtr = (TkTextSegment *) chunkPtr->clientData;
    int lineX, windowX, windowY, width, height;
    Tk_Window tkwin;

    tkwin = ewPtr->body.ew.tkwin;
    if (tkwin == NULL) {
	return;
    }
    if ((x + chunkPtr->width) <= 0) {
	/*
	 * The window is off-screen;  just unmap it.
	 */

	if (ewPtr->body.ew.textPtr->tkwin != Tk_Parent(tkwin)) {
	    Tk_UnmaintainGeometry(tkwin, ewPtr->body.ew.textPtr->tkwin);
	} else {
	    Tk_UnmapWindow(tkwin);
	}
	return;
    }

    /*
     * Compute the window's location and size in the text widget, taking
     * into account the align and stretch values for the window.
     */

    EmbWinBboxProc(chunkPtr, 0, screenY, lineHeight, baseline, &lineX,
	    &windowY, &width, &height);
    windowX = lineX - chunkPtr->x + x;

    if (ewPtr->body.ew.textPtr->tkwin == Tk_Parent(tkwin)) {
	if ((windowX != Tk_X(tkwin)) || (windowY != Tk_Y(tkwin))
		|| (Tk_ReqWidth(tkwin) != Tk_Width(tkwin))
		|| (height != Tk_Height(tkwin))) {
	    Tk_MoveResizeWindow(tkwin, windowX, windowY, width, height);
	}
	Tk_MapWindow(tkwin);
    } else {
	Tk_MaintainGeometry(tkwin, ewPtr->body.ew.textPtr->tkwin,
		windowX, windowY, width, height);
    }

    /*
     * Mark the window as displayed so that it won't get unmapped.
     */

    ewPtr->body.ew.displayed = 1;
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinUndisplayProc --
 *
 *	This procedure is called when the chunk for an embedded
 *	window is no longer going to be displayed.  It arranges
 *	for the window associated with the chunk to be unmapped.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is scheduled for unmapping.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinUndisplayProc(textPtr, chunkPtr)
    TkText *textPtr;			/* Overall information about text
					 * widget. */
    TkTextDispChunk *chunkPtr;		/* Chunk that is about to be freed. */
{
    TkTextSegment *ewPtr = (TkTextSegment *) chunkPtr->clientData;

    ewPtr->body.ew.chunkCount--;
    if (ewPtr->body.ew.chunkCount == 0) {
	/*
	 * Don't unmap the window immediately, since there's a good chance
	 * that it will immediately be redisplayed, perhaps even in the
	 * same place.  Instead, schedule the window to be unmapped later;
	 * the call to EmbWinDelayedUnmap will be cancelled in the likely
	 * event that the unmap becomes unnecessary.
	 */

	ewPtr->body.ew.displayed = 0;
	Tcl_DoWhenIdle(EmbWinDelayedUnmap, (ClientData) ewPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinBboxProc --
 *
 *	This procedure is called to compute the bounding box of
 *	the area occupied by an embedded window.
 *
 * Results:
 *	There is no return value.  *xPtr and *yPtr are filled in
 *	with the coordinates of the upper left corner of the
 *	window, and *widthPtr and *heightPtr are filled in with
 *	the dimensions of the window in pixels.  Note:  not all
 *	of the returned bbox is necessarily visible on the screen
 *	(the rightmost part might be off-screen to the right,
 *	and the bottommost part might be off-screen to the bottom).
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinBboxProc(chunkPtr, index, y, lineHeight, baseline, xPtr, yPtr,
	widthPtr, heightPtr)
    TkTextDispChunk *chunkPtr;		/* Chunk containing desired char. */
    int index;				/* Index of desired character within
					 * the chunk. */
    int y;				/* Topmost pixel in area allocated
					 * for this line. */
    int lineHeight;			/* Total height of line. */
    int baseline;			/* Location of line's baseline, in
					 * pixels measured down from y. */
    int *xPtr, *yPtr;			/* Gets filled in with coords of
					 * character's upper-left pixel. */
    int *widthPtr;			/* Gets filled in with width of
					 * character, in pixels. */
    int *heightPtr;			/* Gets filled in with height of
					 * character, in pixels. */
{
    TkTextSegment *ewPtr = (TkTextSegment *) chunkPtr->clientData;
    Tk_Window tkwin;

    tkwin = ewPtr->body.ew.tkwin;
    if (tkwin != NULL) {
	*widthPtr = Tk_ReqWidth(tkwin);
	*heightPtr = Tk_ReqHeight(tkwin);
    } else {
	*widthPtr = 0;
	*heightPtr = 0;
    }
    *xPtr = chunkPtr->x + ewPtr->body.ew.padX;
    if (ewPtr->body.ew.stretch) {
	if (ewPtr->body.ew.align == ALIGN_BASELINE) {
	    *heightPtr = baseline - ewPtr->body.ew.padY;
	} else {
	    *heightPtr = lineHeight - 2*ewPtr->body.ew.padY;
	}
    }
    switch (ewPtr->body.ew.align) {
	case ALIGN_BOTTOM:
	    *yPtr = y + (lineHeight - *heightPtr - ewPtr->body.ew.padY);
	    break;
	case ALIGN_CENTER:
	    *yPtr = y + (lineHeight - *heightPtr)/2;
	    break;
	case ALIGN_TOP:
	    *yPtr = y + ewPtr->body.ew.padY;
	    break;
	case ALIGN_BASELINE:
	    *yPtr = y + (baseline - *heightPtr);
	    break;
    }
}

/*
 *--------------------------------------------------------------
 *
 * EmbWinDelayedUnmap --
 *
 *	This procedure is an idle handler that does the actual
 *	work of unmapping an embedded window.  See the comment
 *	in EmbWinUndisplayProc for details.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window gets unmapped, unless its chunk reference count
 *	has become non-zero again.
 *
 *--------------------------------------------------------------
 */

static void
EmbWinDelayedUnmap(clientData)
    ClientData clientData;		/* Token for the window to
					 * be unmapped. */
{
    TkTextSegment *ewPtr = (TkTextSegment *) clientData;

    if (!ewPtr->body.ew.displayed && (ewPtr->body.ew.tkwin != NULL)) {
	if (ewPtr->body.ew.textPtr->tkwin != Tk_Parent(ewPtr->body.ew.tkwin)) {
	    Tk_UnmaintainGeometry(ewPtr->body.ew.tkwin,
		    ewPtr->body.ew.textPtr->tkwin);
	} else {
	    Tk_UnmapWindow(ewPtr->body.ew.tkwin);
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkTextWindowIndex --
 *
 *	Given the name of an embedded window within a text widget,
 *	returns an index corresponding to the window's position
 *	in the text.
 *
 * Results:
 *	The return value is 1 if there is an embedded window by
 *	the given name in the text widget, 0 otherwise.  If the
 *	window exists, *indexPtr is filled in with its index.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
TkTextWindowIndex(textPtr, name, indexPtr)
    TkText *textPtr;		/* Text widget containing window. */
    char *name;			/* Name of window. */
    TkTextIndex *indexPtr;	/* Index information gets stored here. */
{
    Tcl_HashEntry *hPtr;
    TkTextSegment *ewPtr;

    hPtr = Tcl_FindHashEntry(&textPtr->windowTable, name);
    if (hPtr == NULL) {
	return 0;
    }
    ewPtr = (TkTextSegment *) Tcl_GetHashValue(hPtr);
    indexPtr->tree = textPtr->tree;
    indexPtr->linePtr = ewPtr->body.ew.linePtr;
    indexPtr->charIndex = TkTextSegToOffset(ewPtr, indexPtr->linePtr);
    return 1;
}

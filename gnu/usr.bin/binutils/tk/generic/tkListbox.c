/* 
 * tkListbox.c --
 *
 *	This module implements listbox widgets for the Tk
 *	toolkit.  A listbox displays a collection of strings,
 *	one per line, and provides scrolling and selection.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkListbox.c 1.108 96/03/29 17:56:32
 */

#include "tkPort.h"
#include "default.h"
#include "tkInt.h"

/*
 * One record of the following type is kept for each element
 * associated with a listbox widget:
 */

typedef struct Element {
    int textLength;		/* # non-NULL characters in text. */
    int lBearing;		/* Distance from first character's
				 * origin to left edge of character. */
    int pixelWidth;		/* Total width of element in pixels (including
				 * left bearing and right bearing). */
    int selected;		/* 1 means this item is selected, 0 means
				 * it isn't. */
    struct Element *nextPtr;	/* Next in list of all elements of this
				 * listbox, or NULL for last element. */
    char text[4];		/* Characters of this element, NULL-
				 * terminated.  The actual space allocated
				 * here will be as large as needed (> 4,
				 * most likely).  Must be the last field
				 * of the record. */
} Element;

#define ElementSize(stringLength) \
	((unsigned) (sizeof(Element) - 3 + stringLength))

/*
 * A data structure of the following type is kept for each listbox
 * widget managed by this file:
 */

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the listbox.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Display *display;		/* Display containing widget.  Used, among
				 * other things, so that resources can be
				 * freed even after tkwin has gone away. */
    Tcl_Interp *interp;		/* Interpreter associated with listbox. */
    Tcl_Command widgetCmd;	/* Token for listbox's widget command. */
    int numElements;		/* Total number of elements in this listbox. */
    Element *firstPtr;		/* First in list of elements (NULL if no
				 * elements). */
    Element *lastPtr;		/* Last in list of elements (NULL if no
				 * elements). */

    /*
     * Information used when displaying widget:
     */

    Tk_3DBorder normalBorder;	/* Used for drawing border around whole
				 * window, plus used for background. */
    int borderWidth;		/* Width of 3-D border around window. */
    int relief;			/* 3-D effect: TK_RELIEF_RAISED, etc. */
    int highlightWidth;		/* Width in pixels of highlight to draw
				 * around widget when it has the focus.
				 * <= 0 means don't draw a highlight. */
    XColor *highlightBgColorPtr;
				/* Color for drawing traversal highlight
				 * area when highlight is off. */
    XColor *highlightColorPtr;	/* Color for drawing traversal highlight. */
    int inset;			/* Total width of all borders, including
				 * traversal highlight and 3-D border.
				 * Indicates how much interior stuff must
				 * be offset from outside edges to leave
				 * room for borders. */
    XFontStruct *fontPtr;	/* Information about text font, or NULL. */
    XColor *fgColorPtr;		/* Text color in normal mode. */
    GC textGC;			/* For drawing normal text. */
    Tk_3DBorder selBorder;	/* Borders and backgrounds for selected
				 * elements. */
    int selBorderWidth;		/* Width of border around selection. */
    XColor *selFgColorPtr;	/* Foreground color for selected elements. */
    GC selTextGC;		/* For drawing selected text. */
    int width;			/* Desired width of window, in characters. */
    int height;			/* Desired height of window, in lines. */
    int lineHeight;		/* Number of pixels allocated for each line
				 * in display. */
    int topIndex;		/* Index of top-most element visible in
				 * window. */
    int fullLines;		/* Number of lines that fit are completely
				 * visible in window.  There may be one
				 * additional line at the bottom that is
				 * partially visible. */
    int partialLine;		/* 0 means that the window holds exactly
				 * fullLines lines.  1 means that there is
				 * one additional line that is partially
				 * visble. */
    int setGrid;		/* Non-zero means pass gridding information
				 * to window manager. */

    /*
     * Information to support horizontal scrolling:
     */

    int maxWidth;		/* Width (in pixels) of widest string in
				 * listbox. */
    int xScrollUnit;		/* Number of pixels in one "unit" for
				 * horizontal scrolling (window scrolls
				 * horizontally in increments of this size).
				 * This is an average character size. */
    int xOffset;		/* The left edge of each string in the
				 * listbox is offset to the left by this
				 * many pixels (0 means no offset, positive
				 * means there is an offset). */

    /*
     * Information about what's selected or active, if any.
     */

    Tk_Uid selectMode;		/* Selection style: single, browse, multiple,
				 * or extended.  This value isn't used in C
				 * code, but the Tcl bindings use it. */
    int numSelected;		/* Number of elements currently selected. */
    int selectAnchor;		/* Fixed end of selection (i.e. element
				 * at which selection was started.) */
    int exportSelection;	/* Non-zero means tie internal listbox
				 * to X selection. */
    int active;			/* Index of "active" element (the one that
				 * has been selected by keyboard traversal).
				 * -1 means none. */

    /*
     * Information for scanning:
     */

    int scanMarkX;		/* X-position at which scan started (e.g.
				 * button was pressed here). */
    int scanMarkY;		/* Y-position at which scan started (e.g.
				 * button was pressed here). */
    int scanMarkXOffset;	/* Value of "xOffset" field when scan
				 * started. */
    int scanMarkYIndex;		/* Index of line that was at top of window
				 * when scan started. */

    /*
     * Miscellaneous information:
     */

    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    char *yScrollCmd;		/* Command prefix for communicating with
				 * vertical scrollbar.  NULL means no command
				 * to issue.  Malloc'ed. */
    char *xScrollCmd;		/* Command prefix for communicating with
				 * horizontal scrollbar.  NULL means no command
				 * to issue.  Malloc'ed. */
    int flags;			/* Various flag bits:  see below for
				 * definitions. */
} Listbox;

/*
 * Flag bits for listboxes:
 *
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * UPDATE_V_SCROLLBAR:		Non-zero means vertical scrollbar needs
 *				to be updated.
 * UPDATE_H_SCROLLBAR:		Non-zero means horizontal scrollbar needs
 *				to be updated.
 * GOT_FOCUS:			Non-zero means this widget currently
 *				has the input focus.
 */

#define REDRAW_PENDING		1
#define UPDATE_V_SCROLLBAR	2
#define UPDATE_H_SCROLLBAR	4
#define GOT_FOCUS		8

/*
 * Information used for argv parsing:
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_LISTBOX_BG_COLOR, Tk_Offset(Listbox, normalBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_LISTBOX_BG_MONO, Tk_Offset(Listbox, normalBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_LISTBOX_BORDER_WIDTH, Tk_Offset(Listbox, borderWidth), 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_LISTBOX_CURSOR, Tk_Offset(Listbox, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-exportselection", "exportSelection",
	"ExportSelection", DEF_LISTBOX_EXPORT_SELECTION,
	Tk_Offset(Listbox, exportSelection), 0},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_FONT, "-font", "font", "Font",
	DEF_LISTBOX_FONT, Tk_Offset(Listbox, fontPtr), 0},
    {TK_CONFIG_COLOR, "-foreground", "foreground", "Foreground",
	DEF_LISTBOX_FG, Tk_Offset(Listbox, fgColorPtr), 0},
    {TK_CONFIG_INT, "-height", "height", "Height",
	DEF_LISTBOX_HEIGHT, Tk_Offset(Listbox, height), 0},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_LISTBOX_HIGHLIGHT_BG,
	Tk_Offset(Listbox, highlightBgColorPtr), 0},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_LISTBOX_HIGHLIGHT, Tk_Offset(Listbox, highlightColorPtr), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_LISTBOX_HIGHLIGHT_WIDTH, Tk_Offset(Listbox, highlightWidth), 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_LISTBOX_RELIEF, Tk_Offset(Listbox, relief), 0},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_LISTBOX_SELECT_COLOR, Tk_Offset(Listbox, selBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_LISTBOX_SELECT_MONO, Tk_Offset(Listbox, selBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_PIXELS, "-selectborderwidth", "selectBorderWidth", "BorderWidth",
	DEF_LISTBOX_SELECT_BD, Tk_Offset(Listbox, selBorderWidth), 0},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_LISTBOX_SELECT_FG_COLOR, Tk_Offset(Listbox, selFgColorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_LISTBOX_SELECT_FG_MONO, Tk_Offset(Listbox, selFgColorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_UID, "-selectmode", "selectMode", "SelectMode",
	DEF_LISTBOX_SELECT_MODE, Tk_Offset(Listbox, selectMode), 0},
    {TK_CONFIG_BOOLEAN, "-setgrid", "setGrid", "SetGrid",
	DEF_LISTBOX_SET_GRID, Tk_Offset(Listbox, setGrid), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_LISTBOX_TAKE_FOCUS, Tk_Offset(Listbox, takeFocus),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_INT, "-width", "width", "Width",
	DEF_LISTBOX_WIDTH, Tk_Offset(Listbox, width), 0},
    {TK_CONFIG_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
	DEF_LISTBOX_SCROLL_COMMAND, Tk_Offset(Listbox, xScrollCmd),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
	DEF_LISTBOX_SCROLL_COMMAND, Tk_Offset(Listbox, yScrollCmd),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ChangeListboxOffset _ANSI_ARGS_((Listbox *listPtr,
			    int offset));
static void		ChangeListboxView _ANSI_ARGS_((Listbox *listPtr,
			    int index));
static int		ConfigureListbox _ANSI_ARGS_((Tcl_Interp *interp,
			    Listbox *listPtr, int argc, char **argv,
			    int flags));
static void		DeleteEls _ANSI_ARGS_((Listbox *listPtr, int first,
			    int last));
static void		DestroyListbox _ANSI_ARGS_((char *memPtr));
static void		DisplayListbox _ANSI_ARGS_((ClientData clientData));
static int		GetListboxIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    Listbox *listPtr, char *string, int numElsOK,
			    int *indexPtr));
static void		InsertEls _ANSI_ARGS_((Listbox *listPtr, int index,
			    int argc, char **argv));
static void		ListboxCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		ListboxComputeGeometry _ANSI_ARGS_((Listbox *listPtr,
			    int fontChanged, int maxIsStale, int updateGrid));
static void		ListboxEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static int		ListboxFetchSelection _ANSI_ARGS_((
			    ClientData clientData, int offset, char *buffer,
			    int maxBytes));
static void		ListboxLostSelection _ANSI_ARGS_((
			    ClientData clientData));
static void		ListboxRedrawRange _ANSI_ARGS_((Listbox *listPtr,
			    int first, int last));
static void		ListboxScanTo _ANSI_ARGS_((Listbox *listPtr,
			    int x, int y));
static void		ListboxSelect _ANSI_ARGS_((Listbox *listPtr,
			    int first, int last, int select));
static void		ListboxUpdateHScrollbar _ANSI_ARGS_((Listbox *listPtr));
static void		ListboxUpdateVScrollbar _ANSI_ARGS_((Listbox *listPtr));
static int		ListboxWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		NearestListboxElement _ANSI_ARGS_((Listbox *listPtr,
			    int y));

/*
 *--------------------------------------------------------------
 *
 * Tk_ListboxCmd --
 *
 *	This procedure is invoked to process the "listbox" Tcl
 *	command.  See the user documentation for details on what
 *	it does.
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
Tk_ListboxCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    register Listbox *listPtr;
    Tk_Window new;
    Tk_Window tkwin = (Tk_Window) clientData;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    new = Tk_CreateWindowFromPath(interp, tkwin, argv[1], (char *) NULL);
    if (new == NULL) {
	return TCL_ERROR;
    }

    /*
     * Initialize the fields of the structure that won't be initialized
     * by ConfigureListbox, or that ConfigureListbox requires to be
     * initialized already (e.g. resource pointers).
     */

    listPtr = (Listbox *) ckalloc(sizeof(Listbox));
    listPtr->tkwin = new;
    listPtr->display = Tk_Display(new);
    listPtr->interp = interp;
    listPtr->widgetCmd = Tcl_CreateCommand(interp,
	    Tk_PathName(listPtr->tkwin), ListboxWidgetCmd,
	    (ClientData) listPtr, ListboxCmdDeletedProc);
    listPtr->numElements = 0;
    listPtr->firstPtr = NULL;
    listPtr->lastPtr = NULL;
    listPtr->normalBorder = NULL;
    listPtr->borderWidth = 0;
    listPtr->relief = TK_RELIEF_RAISED;
    listPtr->highlightWidth = 0;
    listPtr->highlightBgColorPtr = NULL;
    listPtr->highlightColorPtr = NULL;
    listPtr->inset = 0;
    listPtr->fontPtr = NULL;
    listPtr->fgColorPtr = NULL;
    listPtr->textGC = None;
    listPtr->selBorder = NULL;
    listPtr->selBorderWidth = 0;
    listPtr->selFgColorPtr = None;
    listPtr->selTextGC = None;
    listPtr->width = 0;
    listPtr->height = 0;
    listPtr->lineHeight = 0;
    listPtr->topIndex = 0;
    listPtr->fullLines = 1;
    listPtr->partialLine = 0;
    listPtr->setGrid = 0;
    listPtr->maxWidth = 0;
    listPtr->xScrollUnit = 0;
    listPtr->xOffset = 0;
    listPtr->selectMode = NULL;
    listPtr->numSelected = 0;
    listPtr->selectAnchor = 0;
    listPtr->exportSelection = 1;
    listPtr->active = 0;
    listPtr->scanMarkX = 0;
    listPtr->scanMarkY = 0;
    listPtr->scanMarkXOffset = 0;
    listPtr->scanMarkYIndex = 0;
    listPtr->cursor = None;
    listPtr->takeFocus = NULL;
    listPtr->xScrollCmd = NULL;
    listPtr->yScrollCmd = NULL;
    listPtr->flags = 0;

    Tk_SetClass(listPtr->tkwin, "Listbox");
    Tk_CreateEventHandler(listPtr->tkwin,
	    ExposureMask|StructureNotifyMask|FocusChangeMask,
	    ListboxEventProc, (ClientData) listPtr);
    Tk_CreateSelHandler(listPtr->tkwin, XA_PRIMARY, XA_STRING,
	    ListboxFetchSelection, (ClientData) listPtr, XA_STRING);
    if (ConfigureListbox(interp, listPtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    interp->result = Tk_PathName(listPtr->tkwin);
    return TCL_OK;

    error:
    Tk_DestroyWindow(listPtr->tkwin);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * ListboxWidgetCmd --
 *
 *	This procedure is invoked to process the Tcl command
 *	that corresponds to a widget managed by this module.
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

static int
ListboxWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Information about listbox widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register Listbox *listPtr = (Listbox *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) listPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "activate", length) == 0)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " activate index\"",
		    (char *) NULL);
	    goto error;
	}
	ListboxRedrawRange(listPtr, listPtr->active, listPtr->active);
	if (GetListboxIndex(interp, listPtr, argv[2], 0, &index)
		!= TCL_OK) {
	    goto error;
	}
	listPtr->active = index;
	ListboxRedrawRange(listPtr, listPtr->active, listPtr->active);
    } else if ((c == 'b') && (strncmp(argv[1], "bbox", length) == 0)) {
	int index, x, y, i;
	Element *elPtr;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " bbox index\"", (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	for (i = 0, elPtr = listPtr->firstPtr; i < index;
		i++, elPtr = elPtr->nextPtr) {
	    /* Empty loop body. */
	}
	if ((index >= listPtr->topIndex) && (index < listPtr->numElements)
		    && (index < (listPtr->topIndex + listPtr->fullLines
		    + listPtr->partialLine))) {
	    x = listPtr->inset - listPtr->xOffset;
	    y = ((index - listPtr->topIndex)*listPtr->lineHeight)
		    + listPtr->inset + listPtr->selBorderWidth;
	    sprintf(interp->result, "%d %d %d %d", x, y, elPtr->pixelWidth,
		    listPtr->fontPtr->ascent + listPtr->fontPtr->descent);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Tk_ConfigureValue(interp, listPtr->tkwin, configSpecs,
		(char *) listPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, listPtr->tkwin, configSpecs,
		    (char *) listPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, listPtr->tkwin, configSpecs,
		    (char *) listPtr, argv[2], 0);
	} else {
	    result = ConfigureListbox(interp, listPtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "curselection", length) == 0)
	    && (length >= 2)) {
	int i, count;
	char index[20];
	Element *elPtr;

	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " curselection\"",
		    (char *) NULL);
	    goto error;
	}
	count = 0;
	for (i = 0, elPtr = listPtr->firstPtr; elPtr != NULL;
		i++, elPtr = elPtr->nextPtr) {
	    if (elPtr->selected) {
		sprintf(index, "%d", i);
		Tcl_AppendElement(interp, index);
		count++;
	    }
	}
	if (count != listPtr->numSelected) {
	    panic("ListboxWidgetCmd: selection count incorrect");
	}
    } else if ((c == 'd') && (strncmp(argv[1], "delete", length) == 0)) {
	int first, last;

	if ((argc < 3) || (argc > 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " delete firstIndex ?lastIndex?\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 0, &first) != TCL_OK) {
	    goto error;
	}
	if (argc == 3) {
	    last = first;
	} else {
	    if (GetListboxIndex(interp, listPtr, argv[3], 0, &last) != TCL_OK) {
		goto error;
	    }
	}
	DeleteEls(listPtr, first, last);
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	int first, last, i;
	Element *elPtr;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get first ?last?\"", (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 0, &first) != TCL_OK) {
	    goto error;
	}
	if ((argc == 4) && (GetListboxIndex(interp, listPtr, argv[3],
		0, &last) != TCL_OK)) {
	    goto error;
	}
	for (elPtr = listPtr->firstPtr, i = 0; i < first;
		i++, elPtr = elPtr->nextPtr) {
	    /* Empty loop body. */
	}
	if (elPtr != NULL) {
	    if (argc == 3) {
		interp->result = elPtr->text;
	    } else {
		for (  ; i <= last; i++, elPtr = elPtr->nextPtr) {
		    Tcl_AppendElement(interp, elPtr->text);
		}
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "index", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " index index\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 1, &index)
		!= TCL_OK) {
	    goto error;
	}
	sprintf(interp->result, "%d", index);
    } else if ((c == 'i') && (strncmp(argv[1], "insert", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " insert index ?element element ...?\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 1, &index)
		!= TCL_OK) {
	    goto error;
	}
	InsertEls(listPtr, index, argc-3, argv+3);
    } else if ((c == 'n') && (strncmp(argv[1], "nearest", length) == 0)) {
	int index, y;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " nearest y\"", (char *) NULL);
	    goto error;
	}
	if (Tcl_GetInt(interp, argv[2], &y) != TCL_OK) {
	    goto error;
	}
	index = NearestListboxElement(listPtr, y);
	sprintf(interp->result, "%d", index);
    } else if ((c == 's') && (length >= 2)
	    && (strncmp(argv[1], "scan", length) == 0)) {
	int x, y;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " scan mark|dragto x y\"", (char *) NULL);
	    goto error;
	}
	if ((Tcl_GetInt(interp, argv[3], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[4], &y) != TCL_OK)) {
	    goto error;
	}
	if ((argv[2][0] == 'm')
		&& (strncmp(argv[2], "mark", strlen(argv[2])) == 0)) {
	    listPtr->scanMarkX = x;
	    listPtr->scanMarkY = y;
	    listPtr->scanMarkXOffset = listPtr->xOffset;
	    listPtr->scanMarkYIndex = listPtr->topIndex;
	} else if ((argv[2][0] == 'd')
		&& (strncmp(argv[2], "dragto", strlen(argv[2])) == 0)) {
	    ListboxScanTo(listPtr, x, y);
	} else {
	    Tcl_AppendResult(interp, "bad scan option \"", argv[2],
		    "\": must be mark or dragto", (char *) NULL);
	    goto error;
	}
    } else if ((c == 's') && (strncmp(argv[1], "see", length) == 0)
	    && (length >= 3)) {
	int index, diff;
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " see index\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[2], 0, &index) != TCL_OK) {
	    goto error;
	}
	diff = listPtr->topIndex-index;
	if (diff > 0) {
	    if (diff <= (listPtr->fullLines/3)) {
		ChangeListboxView(listPtr, index);
	    } else {
		ChangeListboxView(listPtr, index - (listPtr->fullLines-1)/2);
	    }
	} else {
	    diff = index - (listPtr->topIndex + listPtr->fullLines - 1);
	    if (diff > 0) {
		if (diff <= (listPtr->fullLines/3)) {
		    ChangeListboxView(listPtr, listPtr->topIndex + diff);
		} else {
		    ChangeListboxView(listPtr,
			    index - (listPtr->fullLines-1)/2);
		}
	    }
	}
    } else if ((c == 's') && (length >= 3)
	    && (strncmp(argv[1], "selection", length) == 0)) {
	int first, last;

	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " selection option index ?index?\"",
		    (char *) NULL);
	    goto error;
	}
	if (GetListboxIndex(interp, listPtr, argv[3], 0, &first) != TCL_OK) {
	    goto error;
	}
	if (argc == 5) {
	    if (GetListboxIndex(interp, listPtr, argv[4], 0, &last) != TCL_OK) {
		goto error;
	    }
	} else {
	    last = first;
	}
	length = strlen(argv[2]);
	c = argv[2][0];
	if ((c == 'a') && (strncmp(argv[2], "anchor", length) == 0)) {
	    if (argc != 4) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " selection anchor index\"", (char *) NULL);
		goto error;
	    }
	    listPtr->selectAnchor = first;
	} else if ((c == 'c') && (strncmp(argv[2], "clear", length) == 0)) {
	    ListboxSelect(listPtr, first, last, 0);
	} else if ((c == 'i') && (strncmp(argv[2], "includes", length) == 0)) {
	    int i;
	    Element *elPtr;
    
	    if (argc != 4) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " selection includes index\"", (char *) NULL);
		goto error;
	    }
	    for (elPtr = listPtr->firstPtr, i = 0; i < first;
		    i++, elPtr = elPtr->nextPtr) {
		/* Empty loop body. */
	    }
	    if ((elPtr != NULL) && (elPtr->selected)) {
		interp->result = "1";
	    } else {
		interp->result = "0";
	    }
	} else if ((c == 's') && (strncmp(argv[2], "set", length) == 0)) {
	    ListboxSelect(listPtr, first, last, 1);
	} else {
	    Tcl_AppendResult(interp, "bad selection option \"", argv[2],
		    "\": must be anchor, clear, includes, or set",
		    (char *) NULL);
	    goto error;
	}
    } else if ((c == 's') && (length >= 2)
	    && (strncmp(argv[1], "size", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " size\"", (char *) NULL);
	    goto error;
	}
	sprintf(interp->result, "%d", listPtr->numElements);
    } else if ((c == 'x') && (strncmp(argv[1], "xview", length) == 0)) {
	int index, count, type, windowWidth, windowUnits;
	int offset = 0;		/* Initialized to stop gcc warnings. */
	double fraction, fraction2;

	windowWidth = Tk_Width(listPtr->tkwin)
	    - 2*(listPtr->inset + listPtr->selBorderWidth);
	if (argc == 2) {
	    if (listPtr->maxWidth == 0) {
		interp->result = "0 1";
	    } else {
		fraction = listPtr->xOffset/((double) listPtr->maxWidth);
		fraction2 = (listPtr->xOffset + windowWidth)
			/((double) listPtr->maxWidth);
		if (fraction2 > 1.0) {
		    fraction2 = 1.0;
		}
		sprintf(interp->result, "%g %g", fraction, fraction2);
	    }
	} else if (argc == 3) {
	    if (Tcl_GetInt(interp, argv[2], &index) != TCL_OK) {
		goto error;
	    }
	    ChangeListboxOffset(listPtr, index*listPtr->xScrollUnit);
	} else {
	    type = Tk_GetScrollInfo(interp, argc, argv, &fraction, &count);
	    switch (type) {
		case TK_SCROLL_ERROR:
		    goto error;
		case TK_SCROLL_MOVETO:
		    offset = fraction*listPtr->maxWidth + 0.5;
		    break;
		case TK_SCROLL_PAGES:
		    windowUnits = windowWidth/listPtr->xScrollUnit;
		    if (windowUnits > 2) {
			offset = listPtr->xOffset
				+ count*listPtr->xScrollUnit*(windowUnits-2);
		    } else {
			offset = listPtr->xOffset + count*listPtr->xScrollUnit;
		    }
		    break;
		case TK_SCROLL_UNITS:
		    offset = listPtr->xOffset + count*listPtr->xScrollUnit;
		    break;
	    }
	    ChangeListboxOffset(listPtr, offset);
	}
    } else if ((c == 'y') && (strncmp(argv[1], "yview", length) == 0)) {
	int index, count, type;
	double fraction, fraction2;

	if (argc == 2) {
	    if (listPtr->numElements == 0) {
		interp->result = "0 1";
	    } else {
		fraction = listPtr->topIndex/((double) listPtr->numElements);
		fraction2 = (listPtr->topIndex+listPtr->fullLines)
			/((double) listPtr->numElements);
		if (fraction2 > 1.0) {
		    fraction2 = 1.0;
		}
		sprintf(interp->result, "%g %g", fraction, fraction2);
	    }
	} else if (argc == 3) {
	    if (GetListboxIndex(interp, listPtr, argv[2], 0, &index)
		    != TCL_OK) {
		goto error;
	    }
	    ChangeListboxView(listPtr, index);
	} else {
	    type = Tk_GetScrollInfo(interp, argc, argv, &fraction, &count);
	    switch (type) {
		case TK_SCROLL_ERROR:
		    goto error;
		case TK_SCROLL_MOVETO:
		    index = listPtr->numElements*fraction + 0.5;
		    break;
		case TK_SCROLL_PAGES:
		    if (listPtr->fullLines > 2) {
			index = listPtr->topIndex
				+ count*(listPtr->fullLines-2);
		    } else {
			index = listPtr->topIndex + count;
		    }
		    break;
		case TK_SCROLL_UNITS:
		    index = listPtr->topIndex + count;
		    break;
	    }
	    ChangeListboxView(listPtr, index);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be activate, bbox, cget, configure, ",
		"curselection, delete, get, index, insert, nearest, ",
		"scan, see, selection, size, ",
		"xview, or yview", (char *) NULL);
	goto error;
    }
    Tcl_Release((ClientData) listPtr);
    return result;

    error:
    Tcl_Release((ClientData) listPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyListbox --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a listbox at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the listbox is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyListbox(memPtr)
    char *memPtr;	/* Info about listbox widget. */
{
    register Listbox *listPtr = (Listbox *) memPtr;
    register Element *elPtr, *nextPtr;

    /*
     * Free up all of the list elements.
     */

    for (elPtr = listPtr->firstPtr; elPtr != NULL; ) {
	nextPtr = elPtr->nextPtr;
	ckfree((char *) elPtr);
	elPtr = nextPtr;
    }

    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (listPtr->textGC != None) {
	Tk_FreeGC(listPtr->display, listPtr->textGC);
    }
    if (listPtr->selTextGC != None) {
	Tk_FreeGC(listPtr->display, listPtr->selTextGC);
    }
    Tk_FreeOptions(configSpecs, (char *) listPtr, listPtr->display, 0);
    ckfree((char *) listPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureListbox --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or reconfigure)
 *	a listbox widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for listPtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureListbox(interp, listPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    register Listbox *listPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    XGCValues gcValues;
    GC new;
    int oldExport;

    oldExport = listPtr->exportSelection;
    if (Tk_ConfigureWidget(interp, listPtr->tkwin, configSpecs,
	    argc, argv, (char *) listPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few options need special processing, such as setting the
     * background from a 3-D border.
     */

    Tk_SetBackgroundFromBorder(listPtr->tkwin, listPtr->normalBorder);

    if (listPtr->highlightWidth < 0) {
	listPtr->highlightWidth = 0;
    }
    listPtr->inset = listPtr->highlightWidth + listPtr->borderWidth;

    gcValues.foreground = listPtr->fgColorPtr->pixel;
    gcValues.font = listPtr->fontPtr->fid;
    gcValues.graphics_exposures = False;
    new = Tk_GetGC(listPtr->tkwin, GCForeground|GCFont|GCGraphicsExposures,
	    &gcValues);
    if (listPtr->textGC != None) {
	Tk_FreeGC(listPtr->display, listPtr->textGC);
    }
    listPtr->textGC = new;

    gcValues.foreground = listPtr->selFgColorPtr->pixel;
    gcValues.font = listPtr->fontPtr->fid;
    new = Tk_GetGC(listPtr->tkwin, GCForeground|GCFont, &gcValues);
    if (listPtr->selTextGC != None) {
	Tk_FreeGC(listPtr->display, listPtr->selTextGC);
    }
    listPtr->selTextGC = new;

    /*
     * Claim the selection if we've suddenly started exporting it and
     * there is a selection to export.
     */

    if (listPtr->exportSelection && !oldExport
	    && (listPtr->numSelected != 0)) {
	Tk_OwnSelection(listPtr->tkwin, XA_PRIMARY, ListboxLostSelection,
		(ClientData) listPtr);
    }

    /*
     * Register the desired geometry for the window and arrange for
     * the window to be redisplayed.
     */

    ListboxComputeGeometry(listPtr, 1, 1, 1);
    listPtr->flags |= UPDATE_V_SCROLLBAR|UPDATE_H_SCROLLBAR;
    ListboxRedrawRange(listPtr, 0, listPtr->numElements-1);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayListbox --
 *
 *	This procedure redraws the contents of a listbox window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information appears on the screen.
 *
 *--------------------------------------------------------------
 */

static void
DisplayListbox(clientData)
    ClientData clientData;	/* Information about window. */
{
    register Listbox *listPtr = (Listbox *) clientData;
    register Tk_Window tkwin = listPtr->tkwin;
    register Element *elPtr;
    GC gc;
    int i, limit, x, y, width, prevSelected;
    int left, right;			/* Non-zero values here indicate
					 * that the left or right edge of
					 * the listbox is off-screen. */
    Pixmap pixmap;

    listPtr->flags &= ~REDRAW_PENDING;
    if (listPtr->flags & UPDATE_V_SCROLLBAR) {
	ListboxUpdateVScrollbar(listPtr);
    }
    if (listPtr->flags & UPDATE_H_SCROLLBAR) {
	ListboxUpdateHScrollbar(listPtr);
    }
    listPtr->flags &= ~(REDRAW_PENDING|UPDATE_V_SCROLLBAR|UPDATE_H_SCROLLBAR);
    if ((listPtr->tkwin == NULL) || !Tk_IsMapped(tkwin)) {
	return;
    }

    /*
     * Redrawing is done in a temporary pixmap that is allocated
     * here and freed at the end of the procedure.  All drawing is
     * done to the pixmap, and the pixmap is copied to the screen
     * at the end of the procedure.  This provides the smoothest
     * possible visual effects (no flashing on the screen).
     */

    pixmap = Tk_GetPixmap(listPtr->display, Tk_WindowId(tkwin),
	    Tk_Width(tkwin), Tk_Height(tkwin), Tk_Depth(tkwin));
    Tk_Fill3DRectangle(tkwin, pixmap, listPtr->normalBorder, 0, 0,
	    Tk_Width(tkwin), Tk_Height(tkwin), 0, TK_RELIEF_FLAT);

    /*
     * Iterate through all of the elements of the listbox, displaying each
     * in turn.  Selected elements use a different GC and have a raised
     * background.
     */

    limit = listPtr->topIndex + listPtr->fullLines + listPtr->partialLine - 1;
    if (limit >= listPtr->numElements) {
	limit = listPtr->numElements-1;
    }
    left = right = 0;
    if (listPtr->xOffset > 0) {
	left = listPtr->selBorderWidth+1;
    }
    if ((listPtr->maxWidth - listPtr->xOffset) > (Tk_Width(listPtr->tkwin)
	    - 2*(listPtr->inset + listPtr->selBorderWidth)))  {
	right = listPtr->selBorderWidth+1;
    }
    prevSelected = 0;
    for (elPtr = listPtr->firstPtr, i = 0; (elPtr != NULL) && (i <= limit);
	    prevSelected = elPtr->selected, elPtr = elPtr->nextPtr, i++) {
	if (i < listPtr->topIndex) {
	    continue;
	}
	x = listPtr->inset;
	y = ((i - listPtr->topIndex) * listPtr->lineHeight) 
		+ listPtr->inset;
	gc = listPtr->textGC;
	if (elPtr->selected) {
	    gc = listPtr->selTextGC;
	    width = Tk_Width(tkwin) - 2*listPtr->inset;
	    Tk_Fill3DRectangle(tkwin, pixmap, listPtr->selBorder, x, y,
		    width, listPtr->lineHeight, 0, TK_RELIEF_FLAT);

	    /*
	     * Draw beveled edges around the selection, if there are visible
	     * edges next to this element.  Special considerations:
	     * 1. The left and right bevels may not be visible if horizontal
	     *    scrolling is enabled (the "left" and "right" variables
	     *    are zero to indicate that the corresponding bevel is
	     *    visible).
	     * 2. Top and bottom bevels are only drawn if this is the
	     *    first or last seleted item.
	     * 3. If the left or right bevel isn't visible, then the "left"
	     *    and "right" variables, computed above, have non-zero values
	     *    that extend the top and bottom bevels so that the mitered
	     *    corners are off-screen.
	     */

	    if (left == 0) {
		Tk_3DVerticalBevel(tkwin, pixmap, listPtr->selBorder,
			x, y, listPtr->selBorderWidth, listPtr->lineHeight,
			1, TK_RELIEF_RAISED);
	    }
	    if (right == 0) {
		Tk_3DVerticalBevel(tkwin, pixmap, listPtr->selBorder,
			x + width - listPtr->selBorderWidth, y,
			listPtr->selBorderWidth, listPtr->lineHeight,
			0, TK_RELIEF_RAISED);
	    }
	    if (!prevSelected) {
		Tk_3DHorizontalBevel(tkwin, pixmap, listPtr->selBorder,
			x-left, y, width+left+right, listPtr->selBorderWidth,
			1, 1, 1, TK_RELIEF_RAISED);
	    }
	    if ((elPtr->nextPtr == NULL) || !elPtr->nextPtr->selected) {
		Tk_3DHorizontalBevel(tkwin, pixmap, listPtr->selBorder, x-left,
			y + listPtr->lineHeight - listPtr->selBorderWidth,
			width+left+right, listPtr->selBorderWidth, 0, 0, 0,
			TK_RELIEF_RAISED);
	    }
	}
	y += listPtr->fontPtr->ascent + listPtr->selBorderWidth;
	x = listPtr->inset + listPtr->selBorderWidth - elPtr->lBearing
		- listPtr->xOffset;
	XDrawString(listPtr->display, pixmap, gc, x, y,
		elPtr->text, elPtr->textLength);

	/*
	 * If this is the active element, underline it.
	 */

	if ((i == listPtr->active) && (listPtr->flags & GOT_FOCUS)) {
	    XFillRectangle(listPtr->display, pixmap, gc,
		    listPtr->inset + listPtr->selBorderWidth
			- listPtr->xOffset,
		    y + listPtr->fontPtr->descent - 1,
		    (unsigned) elPtr->pixelWidth, 1);
	}
    }

    /*
     * Redraw the border for the listbox to make sure that it's on top
     * of any of the text of the listbox entries.
     */

    Tk_Draw3DRectangle(tkwin, pixmap, listPtr->normalBorder,
	    listPtr->highlightWidth, listPtr->highlightWidth,
	    Tk_Width(tkwin) - 2*listPtr->highlightWidth,
	    Tk_Height(tkwin) - 2*listPtr->highlightWidth,
	    listPtr->borderWidth, listPtr->relief);
    if (listPtr->highlightWidth > 0) {
	GC gc;

	if (listPtr->flags & GOT_FOCUS) {
	    gc = Tk_GCForColor(listPtr->highlightColorPtr, pixmap);
	} else {
	    gc = Tk_GCForColor(listPtr->highlightBgColorPtr, pixmap);
	}
	Tk_DrawFocusHighlight(tkwin, gc, listPtr->highlightWidth, pixmap);
    }
    XCopyArea(listPtr->display, pixmap, Tk_WindowId(tkwin),
	    listPtr->textGC, 0, 0, (unsigned) Tk_Width(tkwin),
	    (unsigned) Tk_Height(tkwin), 0, 0);
    Tk_FreePixmap(listPtr->display, pixmap);
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxComputeGeometry --
 *
 *	This procedure is invoked to recompute geometry information
 *	such as the sizes of the elements and the overall dimensions
 *	desired for the listbox.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Geometry information is updated and a new requested size is
 *	registered for the widget.  Internal border and gridding
 *	information is also set.
 *
 *----------------------------------------------------------------------
 */

static void
ListboxComputeGeometry(listPtr, fontChanged, maxIsStale, updateGrid)
    Listbox *listPtr;		/* Listbox whose geometry is to be
				 * recomputed. */
    int fontChanged;		/* Non-zero means the font may have changed
				 * so per-element width information also
				 * has to be computed. */
    int maxIsStale;		/* Non-zero means the "maxWidth" field may
				 * no longer be up-to-date and must
				 * be recomputed.  If fontChanged is 1 then
				 * this must be 1. */
    int updateGrid;		/* Non-zero means call Tk_SetGrid or
				 * Tk_UnsetGrid to update gridding for
				 * the window. */
{
    register Element *elPtr;
    int dummy, fontHeight, width, height, pixelWidth, pixelHeight;
    XCharStruct bbox;

    if (fontChanged  || maxIsStale) {
	listPtr->xScrollUnit = XTextWidth(listPtr->fontPtr, "0", 1);
	listPtr->maxWidth = 0;
	for (elPtr = listPtr->firstPtr; elPtr != NULL; elPtr = elPtr->nextPtr) {
	    if (fontChanged) {
		XTextExtents(listPtr->fontPtr, elPtr->text, elPtr->textLength,
			&dummy, &dummy, &dummy, &bbox);
		elPtr->lBearing = bbox.lbearing;
		elPtr->pixelWidth = bbox.rbearing - bbox.lbearing;
	    }
	    if (elPtr->pixelWidth > listPtr->maxWidth) {
		listPtr->maxWidth = elPtr->pixelWidth;
	    }
	}
    }

    fontHeight = listPtr->fontPtr->ascent + listPtr->fontPtr->descent;
    listPtr->lineHeight = fontHeight + 1 + 2*listPtr->selBorderWidth;
    width = listPtr->width;
    if (width <= 0) {
	width = (listPtr->maxWidth + listPtr->xScrollUnit - 1)
		/listPtr->xScrollUnit;
	if (width < 1) {
	    width = 1;
	}
    }
    pixelWidth = width*listPtr->xScrollUnit + 2*listPtr->inset
	    + 2*listPtr->selBorderWidth;
    height = listPtr->height;
    if (listPtr->height <= 0) {
	height = listPtr->numElements;
	if (height < 1) {
	    height = 1;
	}
    }
    pixelHeight = height*listPtr->lineHeight + 2*listPtr->inset;
    Tk_GeometryRequest(listPtr->tkwin, pixelWidth, pixelHeight);
    Tk_SetInternalBorder(listPtr->tkwin, listPtr->inset);
    if (updateGrid) {
	if (listPtr->setGrid) {
	    Tk_SetGrid(listPtr->tkwin, width, height, listPtr->xScrollUnit,
		    listPtr->lineHeight);
	} else {
	    Tk_UnsetGrid(listPtr->tkwin);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * InsertEls --
 *
 *	Add new elements to a listbox widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	New information gets added to listPtr;  it will be redisplayed
 *	soon, but not immediately.
 *
 *----------------------------------------------------------------------
 */

static void
InsertEls(listPtr, index, argc, argv)
    register Listbox *listPtr;	/* Listbox that is to get the new
				 * elements. */
    int index;			/* Add the new elements before this
				 * element. */
    int argc;			/* Number of new elements to add. */
    char **argv;		/* New elements (one per entry). */
{
    register Element *prevPtr, *newPtr;
    int length, dummy, i, oldMaxWidth;
    XCharStruct bbox;

    /*
     * Find the element before which the new ones will be inserted.
     */

    if (index <= 0) {
	index = 0;
    }
    if (index > listPtr->numElements) {
	index = listPtr->numElements;
    }
    if (index == 0) {
	prevPtr = NULL;
    } else if (index == listPtr->numElements) {
          prevPtr = listPtr->lastPtr;
    } else {
	for (prevPtr = listPtr->firstPtr, i = index - 1; i > 0; i--) {
	    prevPtr = prevPtr->nextPtr;
	}
    }

    /*
     * For each new element, create a record, initialize it, and link
     * it into the list of elements.
     */

    oldMaxWidth = listPtr->maxWidth;
    for (i = argc ; i > 0; i--, argv++, prevPtr = newPtr) {
	length = strlen(*argv);
	newPtr = (Element *) ckalloc(ElementSize(length));
	newPtr->textLength = length;
	strcpy(newPtr->text, *argv);
	XTextExtents(listPtr->fontPtr, newPtr->text, newPtr->textLength,
		&dummy, &dummy, &dummy, &bbox);
	newPtr->lBearing = bbox.lbearing;
	newPtr->pixelWidth = bbox.rbearing - bbox.lbearing;
	if (newPtr->pixelWidth > listPtr->maxWidth) {
	    listPtr->maxWidth = newPtr->pixelWidth;
	}
	newPtr->selected = 0;
	if (prevPtr == NULL) {
	    newPtr->nextPtr = listPtr->firstPtr;
	    listPtr->firstPtr = newPtr;
	} else {
	    newPtr->nextPtr = prevPtr->nextPtr;
	    prevPtr->nextPtr = newPtr;
	}
    }
    if ((prevPtr != NULL) && (prevPtr->nextPtr == NULL)) {
	listPtr->lastPtr = prevPtr;
    }
    listPtr->numElements += argc;

    /*
     * Update the selection and other indexes to account for the
     * renumbering that has just occurred.  Then arrange for the new
     * information to be displayed.
     */

    if (index <= listPtr->selectAnchor) {
	listPtr->selectAnchor += argc;
    }
    if (index < listPtr->topIndex) {
	listPtr->topIndex += argc;
    }
    if (index <= listPtr->active) {
	listPtr->active += argc;
	if ((listPtr->active >= listPtr->numElements)
		&& (listPtr->numElements > 0)) {
	    listPtr->active = listPtr->numElements-1;
	}
    }
    listPtr->flags |= UPDATE_V_SCROLLBAR;
    if (listPtr->maxWidth != oldMaxWidth) {
	listPtr->flags |= UPDATE_H_SCROLLBAR;
    }
    ListboxComputeGeometry(listPtr, 0, 0, 0);
    ListboxRedrawRange(listPtr, index, listPtr->numElements-1);
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteEls --
 *
 *	Remove one or more elements from a listbox widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Memory gets freed, the listbox gets modified and (eventually)
 *	redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteEls(listPtr, first, last)
    register Listbox *listPtr;	/* Listbox widget to modify. */
    int first;			/* Index of first element to delete. */
    int last;			/* Index of last element to delete. */
{
    register Element *prevPtr, *elPtr;
    int count, i, widthChanged;

    /*
     * Adjust the range to fit within the existing elements of the
     * listbox, and make sure there's something to delete.
     */

    if (first < 0) {
	first = 0;
    }
    if (last >= listPtr->numElements) {
	last = listPtr->numElements-1;
    }
    count = last + 1 - first;
    if (count <= 0) {
	return;
    }

    /*
     * Find the element just before the ones to delete.
     */

    if (first == 0) {
	prevPtr = NULL;
    } else {
	for (i = first-1, prevPtr = listPtr->firstPtr; i > 0; i--) {
	    prevPtr = prevPtr->nextPtr;
	}
    }

    /*
     * Delete the requested number of elements.
     */

    widthChanged = 0;
    for (i = count; i > 0; i--) {
	if (prevPtr == NULL) {
	    elPtr = listPtr->firstPtr;
	    listPtr->firstPtr = elPtr->nextPtr;
	    if (listPtr->firstPtr == NULL) {
		listPtr->lastPtr = NULL;
	    }
	} else {
	    elPtr = prevPtr->nextPtr;
	    prevPtr->nextPtr = elPtr->nextPtr;
	    if (prevPtr->nextPtr == NULL) {
		listPtr->lastPtr = prevPtr;
	    }
	}
	if (elPtr->pixelWidth == listPtr->maxWidth) {
	    widthChanged = 1;
	}
	if (elPtr->selected) {
	    listPtr->numSelected -= 1;
	}
	ckfree((char *) elPtr);
    }
    listPtr->numElements -= count;

    /*
     * Update the selection and viewing information to reflect the change
     * in the element numbering, and redisplay to slide information up over
     * the elements that were deleted.
     */

    if (first <= listPtr->selectAnchor) {
	listPtr->selectAnchor -= count;
	if (listPtr->selectAnchor < first) {
	    listPtr->selectAnchor = first;
	}
    }
    if (first <= listPtr->topIndex) {
	listPtr->topIndex -= count;
	if (listPtr->topIndex < first) {
	    listPtr->topIndex = first;
	}
    }
    if (listPtr->topIndex > (listPtr->numElements - listPtr->fullLines)) {
	listPtr->topIndex = listPtr->numElements - listPtr->fullLines;
	if (listPtr->topIndex < 0) {
	    listPtr->topIndex = 0;
	}
    }
    if (listPtr->active > last) {
	listPtr->active -= count;
    } else if (listPtr->active >= first) {
	listPtr->active = first;
	if ((listPtr->active >= listPtr->numElements)
		&& (listPtr->numElements > 0)) {
	    listPtr->active = listPtr->numElements-1;
	}
    }
    listPtr->flags |= UPDATE_V_SCROLLBAR;
    ListboxComputeGeometry(listPtr, 0, widthChanged, 0);
    if (widthChanged) {
	listPtr->flags |= UPDATE_H_SCROLLBAR;
    }
    ListboxRedrawRange(listPtr, first, listPtr->numElements-1);
}

/*
 *--------------------------------------------------------------
 *
 * ListboxEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on listboxes.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the window gets deleted, internal structures get
 *	cleaned up.  When it gets exposed, it is redisplayed.
 *
 *--------------------------------------------------------------
 */

static void
ListboxEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Listbox *listPtr = (Listbox *) clientData;

    if (eventPtr->type == Expose) {
	ListboxRedrawRange(listPtr,
		NearestListboxElement(listPtr, eventPtr->xexpose.y),
		NearestListboxElement(listPtr, eventPtr->xexpose.y
		+ eventPtr->xexpose.height));
    } else if (eventPtr->type == DestroyNotify) {
	if (listPtr->setGrid) {
	    Tk_UnsetGrid(listPtr->tkwin);
	}
	if (listPtr->tkwin != NULL) {
	    listPtr->tkwin = NULL;
	    Tcl_DeleteCommand(listPtr->interp,
		    Tcl_GetCommandName(listPtr->interp, listPtr->widgetCmd));
	}
	if (listPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayListbox, (ClientData) listPtr);
	}
	Tcl_EventuallyFree((ClientData) listPtr, DestroyListbox);
    } else if (eventPtr->type == ConfigureNotify) {
	int vertSpace;

	vertSpace = Tk_Height(listPtr->tkwin) - 2*listPtr->inset;
	listPtr->fullLines = vertSpace / listPtr->lineHeight;
	if ((listPtr->fullLines*listPtr->lineHeight) < vertSpace) {
	    listPtr->partialLine = 1;
	} else {
	    listPtr->partialLine = 0;
	}
	listPtr->flags |= UPDATE_V_SCROLLBAR|UPDATE_H_SCROLLBAR;
	ChangeListboxView(listPtr, listPtr->topIndex);
	ChangeListboxOffset(listPtr, listPtr->xOffset);

	/*
	 * Redraw the whole listbox.  It's hard to tell what needs
	 * to be redrawn (e.g. if the listbox has shrunk then we
	 * may only need to redraw the borders), so just redraw
	 * everything for safety.
	 */

	ListboxRedrawRange(listPtr, 0, listPtr->numElements-1);
    } else if (eventPtr->type == FocusIn) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    listPtr->flags |= GOT_FOCUS;
	    ListboxRedrawRange(listPtr, 0, listPtr->numElements-1);
	}
    } else if (eventPtr->type == FocusOut) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    listPtr->flags &= ~GOT_FOCUS;
	    ListboxRedrawRange(listPtr, 0, listPtr->numElements-1);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxCmdDeletedProc --
 *
 *	This procedure is invoked when a widget command is deleted.  If
 *	the widget isn't already in the process of being destroyed,
 *	this command destroys it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The widget is destroyed.
 *
 *----------------------------------------------------------------------
 */

static void
ListboxCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Listbox *listPtr = (Listbox *) clientData;
    Tk_Window tkwin = listPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	listPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *--------------------------------------------------------------
 *
 * GetListboxIndex --
 *
 *	Parse an index into a listbox and return either its value
 *	or an error.
 *
 * Results:
 *	A standard Tcl result.  If all went well, then *indexPtr is
 *	filled in with the index (into listPtr) corresponding to
 *	string.  Otherwise an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetListboxIndex(interp, listPtr, string, numElsOK, indexPtr)
    Tcl_Interp *interp;		/* For error messages. */
    Listbox *listPtr;		/* Listbox for which the index is being
				 * specified. */
    char *string;		/* Specifies an element in the listbox. */
    int numElsOK;		/* 0 means the return value must be less
				 * less than the number of entries in
				 * the listbox;  1 means it may also be
				 * equal to the number of entries. */
    int *indexPtr;		/* Where to store converted index. */
{
    int c;
    size_t length;

    length = strlen(string);
    c = string[0];
    if ((c == 'a') && (strncmp(string, "active", length) == 0)
	    && (length >= 2)) {
	*indexPtr = listPtr->active;
    } else if ((c == 'a') && (strncmp(string, "anchor", length) == 0)
	    && (length >= 2)) {
	*indexPtr = listPtr->selectAnchor;
    } else if ((c == 'e') && (strncmp(string, "end", length) == 0)) {
	*indexPtr = listPtr->numElements;
    } else if (c == '@') {
	int x, y;
	char *p, *end;

	p = string+1;
	x = strtol(p, &end, 0);
	if ((end == p) || (*end != ',')) {
	    goto badIndex;
	}
	p = end+1;
	y = strtol(p, &end, 0);
	if ((end == p) || (*end != 0)) {
	    goto badIndex;
	}
	*indexPtr = NearestListboxElement(listPtr, y);
    } else {
	if (Tcl_GetInt(interp, string, indexPtr) != TCL_OK) {
	    Tcl_ResetResult(interp);
	    goto badIndex;
	}
    }
    if (numElsOK) {
	if (*indexPtr > listPtr->numElements) {
	    *indexPtr = listPtr->numElements;
	}
    } else if (*indexPtr >= listPtr->numElements) {
	*indexPtr = listPtr->numElements-1;
    }
    if (*indexPtr < 0) {
	*indexPtr = 0;
    }
    return TCL_OK;

    badIndex:
    Tcl_AppendResult(interp, "bad listbox index \"", string,
	    "\": must be active, anchor, end, @x,y, or a number",
	    (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * ChangeListboxView --
 *
 *	Change the view on a listbox widget so that a given element
 *	is displayed at the top.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	What's displayed on the screen is changed.  If there is a
 *	scrollbar associated with this widget, then the scrollbar
 *	is instructed to change its display too.
 *
 *----------------------------------------------------------------------
 */

static void
ChangeListboxView(listPtr, index)
    register Listbox *listPtr;		/* Information about widget. */
    int index;				/* Index of element in listPtr
					 * that should now appear at the
					 * top of the listbox. */
{
    if (index >= (listPtr->numElements - listPtr->fullLines)) {
	index = listPtr->numElements - listPtr->fullLines;
    }
    if (index < 0) {
	index = 0;
    }
    if (listPtr->topIndex != index) {
	listPtr->topIndex = index;
	if (!(listPtr->flags & REDRAW_PENDING)) {
	    Tcl_DoWhenIdle(DisplayListbox, (ClientData) listPtr);
	    listPtr->flags |= REDRAW_PENDING;
	}
	listPtr->flags |= UPDATE_V_SCROLLBAR;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ChangListboxOffset --
 *
 *	Change the horizontal offset for a listbox.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The listbox may be redrawn to reflect its new horizontal
 *	offset.
 *
 *----------------------------------------------------------------------
 */

static void
ChangeListboxOffset(listPtr, offset)
    register Listbox *listPtr;		/* Information about widget. */
    int offset;				/* Desired new "xOffset" for
					 * listbox. */
{
    int maxOffset;

    /*
     * Make sure that the new offset is within the allowable range, and
     * round it off to an even multiple of xScrollUnit.
     */

    maxOffset = listPtr->maxWidth + (listPtr->xScrollUnit-1)
	    - (Tk_Width(listPtr->tkwin) - 2*listPtr->inset
	    - 2*listPtr->selBorderWidth - listPtr->xScrollUnit);
    if (offset > maxOffset) {
	offset = maxOffset;
    }
    if (offset < 0) {
	offset = 0;
    }
    offset -= offset%listPtr->xScrollUnit;
    if (offset != listPtr->xOffset) {
	listPtr->xOffset = offset;
	listPtr->flags |= UPDATE_H_SCROLLBAR;
	ListboxRedrawRange(listPtr, 0, listPtr->numElements);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxScanTo --
 *
 *	Given a point (presumably of the curent mouse location)
 *	drag the view in the window to implement the scan operation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The view in the window may change.
 *
 *----------------------------------------------------------------------
 */

static void
ListboxScanTo(listPtr, x, y)
    register Listbox *listPtr;		/* Information about widget. */
    int x;				/* X-coordinate to use for scan
					 * operation. */
    int y;				/* Y-coordinate to use for scan
					 * operation. */
{
    int newTopIndex, newOffset, maxIndex, maxOffset;

    maxIndex = listPtr->numElements - listPtr->fullLines;
    maxOffset = listPtr->maxWidth + (listPtr->xScrollUnit-1)
	    - (Tk_Width(listPtr->tkwin) - 2*listPtr->inset
	    - 2*listPtr->selBorderWidth - listPtr->xScrollUnit);

    /*
     * Compute new top line for screen by amplifying the difference
     * between the current position and the place where the scan
     * started (the "mark" position).  If we run off the top or bottom
     * of the list, then reset the mark point so that the current
     * position continues to correspond to the edge of the window.
     * This means that the picture will start dragging as soon as the
     * mouse reverses direction (without this reset, might have to slide
     * mouse a long ways back before the picture starts moving again).
     */

    newTopIndex = listPtr->scanMarkYIndex
	    - (10*(y - listPtr->scanMarkY))/listPtr->lineHeight;
    if (newTopIndex > maxIndex) {
	newTopIndex = listPtr->scanMarkYIndex = maxIndex;
	listPtr->scanMarkY = y;
    } else if (newTopIndex < 0) {
	newTopIndex = listPtr->scanMarkYIndex = 0;
	listPtr->scanMarkY = y;
    }
    ChangeListboxView(listPtr, newTopIndex);

    /*
     * Compute new left edge for display in a similar fashion by amplifying
     * the difference between the current position and the place where the
     * scan started.
     */

    newOffset = listPtr->scanMarkXOffset - (10*(x - listPtr->scanMarkX));
    if (newOffset > maxOffset) {
	newOffset = listPtr->scanMarkXOffset = maxOffset;
	listPtr->scanMarkX = x;
    } else if (newOffset < 0) {
	newOffset = listPtr->scanMarkXOffset = 0;
	listPtr->scanMarkX = x;
    }
    ChangeListboxOffset(listPtr, newOffset);
}

/*
 *----------------------------------------------------------------------
 *
 * NearestListboxElement --
 *
 *	Given a y-coordinate inside a listbox, compute the index of
 *	the element under that y-coordinate (or closest to that
 *	y-coordinate).
 *
 * Results:
 *	The return value is an index of an element of listPtr.  If
 *	listPtr has no elements, then 0 is always returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
NearestListboxElement(listPtr, y)
    register Listbox *listPtr;		/* Information about widget. */
    int y;				/* Y-coordinate in listPtr's window. */
{
    int index;

    index = (y - listPtr->inset)/listPtr->lineHeight;
    if (index >= (listPtr->fullLines + listPtr->partialLine)) {
	index = listPtr->fullLines + listPtr->partialLine - 1;
    }
    if (index < 0) {
	index = 0;
    }
    index += listPtr->topIndex;
    if (index >= listPtr->numElements) {
	index = listPtr->numElements-1;
    }
    return index;
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxSelect --
 *
 *	Select or deselect one or more elements in a listbox..
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	All of the elements in the range between first and last are
 *	marked as either selected or deselected, depending on the
 *	"select" argument.  Any items whose state changes are redisplayed.
 *	The selection is claimed from X when the number of selected
 *	elements changes from zero to non-zero.
 *
 *----------------------------------------------------------------------
 */

static void
ListboxSelect(listPtr, first, last, select)
    register Listbox *listPtr;		/* Information about widget. */
    int first;				/* Index of first element to
					 * select or deselect. */
    int last;				/* Index of last element to
					 * select or deselect. */
    int select;				/* 1 means select items, 0 means
					 * deselect them. */
{
    int i, firstRedisplay, lastRedisplay, increment, oldCount;
    Element *elPtr;

    if (last < first) {
	i = first;
	first = last;
	last = i;
    }
    if (first >= listPtr->numElements) {
	return;
    }
    oldCount = listPtr->numSelected;
    firstRedisplay = -1;
    increment = select ? 1 : -1;
    for (i = 0, elPtr = listPtr->firstPtr; i < first;
	    i++, elPtr = elPtr->nextPtr) {
	/* Empty loop body. */
    }
    for ( ; i <= last; i++, elPtr = elPtr->nextPtr) {
	if (elPtr->selected == select) {
	    continue;
	}
	listPtr->numSelected += increment;
	elPtr->selected = select;
	if (firstRedisplay < 0) {
	    firstRedisplay = i;
	}
	lastRedisplay = i;
    }
    if (firstRedisplay >= 0) {
	ListboxRedrawRange(listPtr, first, last);
    }
    if ((oldCount == 0) && (listPtr->numSelected > 0)
	    && (listPtr->exportSelection)) {
	Tk_OwnSelection(listPtr->tkwin, XA_PRIMARY, ListboxLostSelection,
		(ClientData) listPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxFetchSelection --
 *
 *	This procedure is called back by Tk when the selection is
 *	requested by someone.  It returns part or all of the selection
 *	in a buffer provided by the caller.
 *
 * Results:
 *	The return value is the number of non-NULL bytes stored
 *	at buffer.  Buffer is filled (or partially filled) with a
 *	NULL-terminated string containing part or all of the selection,
 *	as given by offset and maxBytes.  The selection is returned
 *	as a Tcl list with one list element for each element in the
 *	listbox.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ListboxFetchSelection(clientData, offset, buffer, maxBytes)
    ClientData clientData;		/* Information about listbox widget. */
    int offset;				/* Offset within selection of first
					 * byte to be returned. */
    char *buffer;			/* Location in which to place
					 * selection. */
    int maxBytes;			/* Maximum number of bytes to place
					 * at buffer, not including terminating
					 * NULL character. */
{
    register Listbox *listPtr = (Listbox *) clientData;
    register Element *elPtr;
    Tcl_DString selection;
    int length, count, needNewline;

    if (!listPtr->exportSelection) {
	return -1;
    }

    /*
     * Use a dynamic string to accumulate the contents of the selection.
     */

    needNewline = 0;
    Tcl_DStringInit(&selection);
    for (elPtr = listPtr->firstPtr; elPtr != NULL; elPtr = elPtr->nextPtr) {
	if (elPtr->selected) {
	    if (needNewline) {
		Tcl_DStringAppend(&selection, "\n", 1);
	    }
	    Tcl_DStringAppend(&selection, elPtr->text, elPtr->textLength);
	    needNewline = 1;
	}
    }

    length = Tcl_DStringLength(&selection);
    if (length == 0) {
	return -1;
    }

    /*
     * Copy the requested portion of the selection to the buffer.
     */

    count = length - offset;
    if (count <= 0) {
	count = 0;
    } else {
	if (count > maxBytes) {
	    count = maxBytes;
	}
	memcpy((VOID *) buffer,
		(VOID *) (Tcl_DStringValue(&selection) + offset),
		(size_t) count);
    }
    buffer[count] = '\0';
    Tcl_DStringFree(&selection);
    return count;
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxLostSelection --
 *
 *	This procedure is called back by Tk when the selection is
 *	grabbed away from a listbox widget.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The existing selection is unhighlighted, and the window is
 *	marked as not containing a selection.
 *
 *----------------------------------------------------------------------
 */

static void
ListboxLostSelection(clientData)
    ClientData clientData;		/* Information about listbox widget. */
{
    register Listbox *listPtr = (Listbox *) clientData;

    if ((listPtr->exportSelection) && (listPtr->numElements > 0)) {
	ListboxSelect(listPtr, 0, listPtr->numElements-1, 0);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxRedrawRange --
 *
 *	Ensure that a given range of elements is eventually redrawn on
 *	the display (if those elements in fact appear on the display).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets redisplayed.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
ListboxRedrawRange(listPtr, first, last)
    register Listbox *listPtr;		/* Information about widget. */
    int first;				/* Index of first element in list
					 * that needs to be redrawn. */
    int last;				/* Index of last element in list
					 * that needs to be redrawn.  May
					 * be less than first;
					 * these just bracket a range. */
{
    if ((listPtr->tkwin == NULL) || !Tk_IsMapped(listPtr->tkwin)
	    || (listPtr->flags & REDRAW_PENDING)) {
	return;
    }
    Tcl_DoWhenIdle(DisplayListbox, (ClientData) listPtr);
    listPtr->flags |= REDRAW_PENDING;
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxUpdateVScrollbar --
 *
 *	This procedure is invoked whenever information has changed in
 *	a listbox in a way that would invalidate a vertical scrollbar
 *	display.  If there is an associated scrollbar, then this command
 *	updates it by invoking a Tcl command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A Tcl command is invoked, and an additional command may be
 *	invoked to process errors in the command.
 *
 *----------------------------------------------------------------------
 */

static void
ListboxUpdateVScrollbar(listPtr)
    register Listbox *listPtr;		/* Information about widget. */
{
    char string[100];
    double first, last;
    int result;
    Tcl_Interp *interp;

    if (listPtr->yScrollCmd == NULL) {
	return;
    }
    if (listPtr->numElements == 0) {
	first = 0.0;
	last = 1.0;
    } else {
	first = listPtr->topIndex/((double) listPtr->numElements);
	last = (listPtr->topIndex+listPtr->fullLines)
		/((double) listPtr->numElements);
	if (last > 1.0) {
	    last = 1.0;
	}
    }
    sprintf(string, " %g %g", first, last);

    /*
     * We must hold onto the interpreter from the listPtr because the data
     * at listPtr might be freed as a result of the Tcl_VarEval.
     */
    
    interp = listPtr->interp;
    Tcl_Preserve((ClientData) interp);
    result = Tcl_VarEval(interp, listPtr->yScrollCmd, string,
	    (char *) NULL);
    if (result != TCL_OK) {
	Tcl_AddErrorInfo(interp,
		"\n    (vertical scrolling command executed by listbox)");
	Tcl_BackgroundError(interp);
    }
    Tcl_Release((ClientData) interp);
}

/*
 *----------------------------------------------------------------------
 *
 * ListboxUpdateHScrollbar --
 *
 *	This procedure is invoked whenever information has changed in
 *	a listbox in a way that would invalidate a horizontal scrollbar
 *	display.  If there is an associated horizontal scrollbar, then
 *	this command updates it by invoking a Tcl command.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A Tcl command is invoked, and an additional command may be
 *	invoked to process errors in the command.
 *
 *----------------------------------------------------------------------
 */

static void
ListboxUpdateHScrollbar(listPtr)
    register Listbox *listPtr;		/* Information about widget. */
{
    char string[60];
    int result, windowWidth;
    double first, last;
    Tcl_Interp *interp;

    if (listPtr->xScrollCmd == NULL) {
	return;
    }
    windowWidth = Tk_Width(listPtr->tkwin) - 2*(listPtr->inset
	    + listPtr->selBorderWidth);
    if (listPtr->maxWidth == 0) {
	first = 0;
	last = 1.0;
    } else {
	first = listPtr->xOffset/((double) listPtr->maxWidth);
	last = (listPtr->xOffset + windowWidth)
		/((double) listPtr->maxWidth);
	if (last > 1.0) {
	    last = 1.0;
	}
    }
    sprintf(string, " %g %g", first, last);

    /*
     * We must hold onto the interpreter because the data referred to at
     * listPtr might be freed as a result of the call to Tcl_VarEval.
     */
    
    interp = listPtr->interp;
    Tcl_Preserve((ClientData) interp);
    result = Tcl_VarEval(interp, listPtr->xScrollCmd, string,
	    (char *) NULL);
    if (result != TCL_OK) {
	Tcl_AddErrorInfo(interp,
		"\n    (horizontal scrolling command executed by listbox)");
	Tcl_BackgroundError(interp);
    }
    Tcl_Release((ClientData) interp);
}

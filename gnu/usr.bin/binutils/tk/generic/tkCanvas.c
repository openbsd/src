/* 
 * tkCanvas.c --
 *
 *	This module implements canvas widgets for the Tk toolkit.
 *	A canvas displays a background and a collection of graphical
 *	objects such as rectangles, lines, and texts.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkCanvas.c 1.119 96/03/21 11:26:39
 */

#include "default.h"
#include "tkInt.h"
#include "tkPort.h"
#include "tkCanvas.h"

/*
 * See tkCanvas.h for key data structures used to implement canvases.
 */

/*
 * The structure defined below is used to keep track of a tag search
 * in progress.  Only the "prevPtr" field should be accessed by anyone
 * other than StartTagSearch and NextItem.
 */

typedef struct TagSearch {
    TkCanvas *canvasPtr;	/* Canvas widget being searched. */
    Tk_Uid tag;			/* Tag to search for.   0 means return
				 * all items. */
    Tk_Item *prevPtr;		/* Item just before last one found (or NULL
				 * if last one found was first in the item
				 * list of canvasPtr). */
    Tk_Item *currentPtr;	/* Pointer to last item returned. */
    int searchOver;		/* Non-zero means NextItem should always
				 * return NULL. */
} TagSearch;

/*
 * Information used for argv parsing.
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_CANVAS_BG_COLOR, Tk_Offset(TkCanvas, bgBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_CANVAS_BG_MONO, Tk_Offset(TkCanvas, bgBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_CANVAS_BORDER_WIDTH, Tk_Offset(TkCanvas, borderWidth), 0},
    {TK_CONFIG_DOUBLE, "-closeenough", "closeEnough", "CloseEnough",
	DEF_CANVAS_CLOSE_ENOUGH, Tk_Offset(TkCanvas, closeEnough), 0},
    {TK_CONFIG_BOOLEAN, "-confine", "confine", "Confine",
	DEF_CANVAS_CONFINE, Tk_Offset(TkCanvas, confine), 0},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_CANVAS_CURSOR, Tk_Offset(TkCanvas, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-height", "height", "Height",
	DEF_CANVAS_HEIGHT, Tk_Offset(TkCanvas, height), 0},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_CANVAS_HIGHLIGHT_BG,
	Tk_Offset(TkCanvas, highlightBgColorPtr), 0},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_CANVAS_HIGHLIGHT, Tk_Offset(TkCanvas, highlightColorPtr), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_CANVAS_HIGHLIGHT_WIDTH, Tk_Offset(TkCanvas, highlightWidth), 0},
    {TK_CONFIG_BORDER, "-insertbackground", "insertBackground", "Foreground",
	DEF_CANVAS_INSERT_BG, Tk_Offset(TkCanvas, textInfo.insertBorder), 0},
    {TK_CONFIG_PIXELS, "-insertborderwidth", "insertBorderWidth", "BorderWidth",
	DEF_CANVAS_INSERT_BD_COLOR,
	Tk_Offset(TkCanvas, textInfo.insertBorderWidth), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_PIXELS, "-insertborderwidth", "insertBorderWidth", "BorderWidth",
	DEF_CANVAS_INSERT_BD_MONO,
	Tk_Offset(TkCanvas, textInfo.insertBorderWidth), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_INT, "-insertofftime", "insertOffTime", "OffTime",
	DEF_CANVAS_INSERT_OFF_TIME, Tk_Offset(TkCanvas, insertOffTime), 0},
    {TK_CONFIG_INT, "-insertontime", "insertOnTime", "OnTime",
	DEF_CANVAS_INSERT_ON_TIME, Tk_Offset(TkCanvas, insertOnTime), 0},
    {TK_CONFIG_PIXELS, "-insertwidth", "insertWidth", "InsertWidth",
	DEF_CANVAS_INSERT_WIDTH, Tk_Offset(TkCanvas, textInfo.insertWidth), 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_CANVAS_RELIEF, Tk_Offset(TkCanvas, relief), 0},
    {TK_CONFIG_STRING, "-scrollregion", "scrollRegion", "ScrollRegion",
	DEF_CANVAS_SCROLL_REGION, Tk_Offset(TkCanvas, regionString),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_CANVAS_SELECT_COLOR, Tk_Offset(TkCanvas, textInfo.selBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-selectbackground", "selectBackground", "Foreground",
	DEF_CANVAS_SELECT_MONO, Tk_Offset(TkCanvas, textInfo.selBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_PIXELS, "-selectborderwidth", "selectBorderWidth", "BorderWidth",
	DEF_CANVAS_SELECT_BD_COLOR,
	Tk_Offset(TkCanvas, textInfo.selBorderWidth), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_PIXELS, "-selectborderwidth", "selectBorderWidth", "BorderWidth",
	DEF_CANVAS_SELECT_BD_MONO, Tk_Offset(TkCanvas, textInfo.selBorderWidth),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_CANVAS_SELECT_FG_COLOR, Tk_Offset(TkCanvas, textInfo.selFgColorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-selectforeground", "selectForeground", "Background",
	DEF_CANVAS_SELECT_FG_MONO, Tk_Offset(TkCanvas, textInfo.selFgColorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_CANVAS_TAKE_FOCUS, Tk_Offset(TkCanvas, takeFocus),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-width", "width", "Width",
	DEF_CANVAS_WIDTH, Tk_Offset(TkCanvas, width), 0},
    {TK_CONFIG_STRING, "-xscrollcommand", "xScrollCommand", "ScrollCommand",
	DEF_CANVAS_X_SCROLL_CMD, Tk_Offset(TkCanvas, xScrollCmd),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-xscrollincrement", "xScrollIncrement",
	"ScrollIncrement",
	DEF_CANVAS_X_SCROLL_INCREMENT, Tk_Offset(TkCanvas, xScrollIncrement),
	0},
    {TK_CONFIG_STRING, "-yscrollcommand", "yScrollCommand", "ScrollCommand",
	DEF_CANVAS_Y_SCROLL_CMD, Tk_Offset(TkCanvas, yScrollCmd),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-yscrollincrement", "yScrollIncrement",
	"ScrollIncrement",
	DEF_CANVAS_Y_SCROLL_INCREMENT, Tk_Offset(TkCanvas, yScrollIncrement),
	0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * List of all the item types known at present:
 */

static Tk_ItemType *typeList = NULL;	/* NULL means initialization hasn't
					 * been done yet. */

/*
 * Standard item types provided by Tk:
 */

extern Tk_ItemType tkArcType, tkBitmapType, tkImageType, tkLineType;
extern Tk_ItemType tkOvalType, tkPolygonType;
extern Tk_ItemType tkRectangleType, tkTextType, tkWindowType;

/*
 * Various Tk_Uid's used by this module (set up during initialization):
 */

static Tk_Uid allUid = NULL;
static Tk_Uid currentUid = NULL;

/*
 * Statistics counters:
 */

static int numIdSearches;
static int numSlowSearches;

/*
 * Prototypes for procedures defined later in this file:
 */

static void		CanvasBindProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		CanvasBlinkProc _ANSI_ARGS_((ClientData clientData));
static void		CanvasCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		CanvasDoEvent _ANSI_ARGS_((TkCanvas *canvasPtr,
			    XEvent *eventPtr));
static void		CanvasEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static int		CanvasFetchSelection _ANSI_ARGS_((
			    ClientData clientData, int offset,
			    char *buffer, int maxBytes));
static Tk_Item *	CanvasFindClosest _ANSI_ARGS_((TkCanvas *canvasPtr,
			    double coords[2]));
static void		CanvasFocusProc _ANSI_ARGS_((TkCanvas *canvasPtr,
			    int gotFocus));
static void		CanvasLostSelection _ANSI_ARGS_((
			    ClientData clientData));
static void		CanvasSelectTo _ANSI_ARGS_((TkCanvas *canvasPtr,
			    Tk_Item *itemPtr, int index));
static void		CanvasSetOrigin _ANSI_ARGS_((TkCanvas *canvasPtr,
			    int xOrigin, int yOrigin));
static void		CanvasUpdateScrollbars _ANSI_ARGS_((
			    TkCanvas *canvasPtr));
static int		CanvasWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static int		ConfigureCanvas _ANSI_ARGS_((Tcl_Interp *interp,
			    TkCanvas *canvasPtr, int argc, char **argv,
			    int flags));
static void		DestroyCanvas _ANSI_ARGS_((char *memPtr));
static void		DisplayCanvas _ANSI_ARGS_((ClientData clientData));
static void		DoItem _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Item *itemPtr, Tk_Uid tag));
static int		FindItems _ANSI_ARGS_((Tcl_Interp *interp,
			    TkCanvas *canvasPtr, int argc, char **argv,
			    char *newTag, char *cmdName, char *option));
static int		FindArea _ANSI_ARGS_((Tcl_Interp *interp,
			    TkCanvas *canvasPtr, char **argv, Tk_Uid uid,
			    int enclosed));
static double		GridAlign _ANSI_ARGS_((double coord, double spacing));
static void		InitCanvas _ANSI_ARGS_((void));
static Tk_Item *	NextItem _ANSI_ARGS_((TagSearch *searchPtr));
static void		PickCurrentItem _ANSI_ARGS_((TkCanvas *canvasPtr,
			    XEvent *eventPtr));
static void		PrintScrollFractions _ANSI_ARGS_((int screen1,
			    int screen2, int object1, int object2,
			    char *string));
static void		RelinkItems _ANSI_ARGS_((TkCanvas *canvasPtr,
			    char *tag, Tk_Item *prevPtr));
static Tk_Item *	StartTagSearch _ANSI_ARGS_((TkCanvas *canvasPtr,
			    char *tag, TagSearch *searchPtr));

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasCmd --
 *
 *	This procedure is invoked to process the "canvas" Tcl
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
Tk_CanvasCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    TkCanvas *canvasPtr;
    Tk_Window new;

    if (typeList == NULL) {
	InitCanvas();
    }

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
     * Initialize fields that won't be initialized by ConfigureCanvas,
     * or which ConfigureCanvas expects to have reasonable values
     * (e.g. resource pointers).
     */

    canvasPtr = (TkCanvas *) ckalloc(sizeof(TkCanvas));
    canvasPtr->tkwin = new;
    canvasPtr->display = Tk_Display(new);
    canvasPtr->interp = interp;
    canvasPtr->widgetCmd = Tcl_CreateCommand(interp,
	    Tk_PathName(canvasPtr->tkwin), CanvasWidgetCmd,
	    (ClientData) canvasPtr, CanvasCmdDeletedProc);
    canvasPtr->firstItemPtr = NULL;
    canvasPtr->lastItemPtr = NULL;
    canvasPtr->borderWidth = 0;
    canvasPtr->bgBorder = NULL;
    canvasPtr->relief = TK_RELIEF_FLAT;
    canvasPtr->highlightWidth = 0;
    canvasPtr->highlightBgColorPtr = NULL;
    canvasPtr->highlightColorPtr = NULL;
    canvasPtr->inset = 0;
    canvasPtr->pixmapGC = None;
    canvasPtr->width = None;
    canvasPtr->height = None;
    canvasPtr->confine = 0;
    canvasPtr->textInfo.selBorder = NULL;
    canvasPtr->textInfo.selBorderWidth = 0;
    canvasPtr->textInfo.selFgColorPtr = NULL;
    canvasPtr->textInfo.selItemPtr = NULL;
    canvasPtr->textInfo.selectFirst = -1;
    canvasPtr->textInfo.selectLast = -1;
    canvasPtr->textInfo.anchorItemPtr = NULL;
    canvasPtr->textInfo.selectAnchor = 0;
    canvasPtr->textInfo.insertBorder = NULL;
    canvasPtr->textInfo.insertWidth = 0;
    canvasPtr->textInfo.insertBorderWidth = 0;
    canvasPtr->textInfo.focusItemPtr = NULL;
    canvasPtr->textInfo.gotFocus = 0;
    canvasPtr->textInfo.cursorOn = 0;
    canvasPtr->insertOnTime = 0;
    canvasPtr->insertOffTime = 0;
    canvasPtr->insertBlinkHandler = (Tcl_TimerToken) NULL;
    canvasPtr->xOrigin = canvasPtr->yOrigin = 0;
    canvasPtr->drawableXOrigin = canvasPtr->drawableYOrigin = 0;
    canvasPtr->bindingTable = NULL;
    canvasPtr->currentItemPtr = NULL;
    canvasPtr->newCurrentPtr = NULL;
    canvasPtr->closeEnough = 0.0;
    canvasPtr->pickEvent.type = LeaveNotify;
    canvasPtr->pickEvent.xcrossing.x = 0;
    canvasPtr->pickEvent.xcrossing.y = 0;
    canvasPtr->state = 0;
    canvasPtr->xScrollCmd = NULL;
    canvasPtr->yScrollCmd = NULL;
    canvasPtr->scrollX1 = 0;
    canvasPtr->scrollY1 = 0;
    canvasPtr->scrollX2 = 0;
    canvasPtr->scrollY2 = 0;
    canvasPtr->regionString = NULL;
    canvasPtr->xScrollIncrement = 0;
    canvasPtr->yScrollIncrement = 0;
    canvasPtr->scanX = 0;
    canvasPtr->scanXOrigin = 0;
    canvasPtr->scanY = 0;
    canvasPtr->scanYOrigin = 0;
    canvasPtr->hotPtr = NULL;
    canvasPtr->hotPrevPtr = NULL;
    canvasPtr->cursor = None;
    canvasPtr->takeFocus = NULL;
    canvasPtr->pixelsPerMM = WidthOfScreen(Tk_Screen(new));
    canvasPtr->pixelsPerMM /= WidthMMOfScreen(Tk_Screen(new));
    canvasPtr->flags = 0;
    canvasPtr->nextId = 1;
    canvasPtr->psInfoPtr = NULL;

    Tk_SetClass(canvasPtr->tkwin, "Canvas");
    Tk_CreateEventHandler(canvasPtr->tkwin,
	    ExposureMask|StructureNotifyMask|FocusChangeMask,
	    CanvasEventProc, (ClientData) canvasPtr);
    Tk_CreateEventHandler(canvasPtr->tkwin, KeyPressMask|KeyReleaseMask
	    |ButtonPressMask|ButtonReleaseMask|EnterWindowMask
	    |LeaveWindowMask|PointerMotionMask, CanvasBindProc,
	    (ClientData) canvasPtr);
    Tk_CreateSelHandler(canvasPtr->tkwin, XA_PRIMARY, XA_STRING,
	    CanvasFetchSelection, (ClientData) canvasPtr, XA_STRING);
    if (ConfigureCanvas(interp, canvasPtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    interp->result = Tk_PathName(canvasPtr->tkwin);
    return TCL_OK;

    error:
    Tk_DestroyWindow(canvasPtr->tkwin);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * CanvasWidgetCmd --
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
CanvasWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Information about canvas
					 * widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    TkCanvas *canvasPtr = (TkCanvas *) clientData;
    size_t length;
    int c, result;
    Tk_Item *itemPtr = NULL;		/* Initialization needed only to
					 * prevent compiler warning. */
    TagSearch search;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) canvasPtr);
    result = TCL_OK;
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "addtag", length) == 0)) {
	if (argc < 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " addtags tag searchCommand ?arg arg ...?\"",
		    (char *) NULL);
	    goto error;
	}
	result = FindItems(interp, canvasPtr, argc-3, argv+3, argv[2], argv[0],
		" addtag tag");
    } else if ((c == 'b') && (strncmp(argv[1], "bbox", length) == 0)
	    && (length >= 2)) {
	int i, gotAny;
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;	/* Initializations needed
						 * only to prevent compiler
						 * warnings. */

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " bbox tagOrId ?tagOrId ...?\"",
		    (char *) NULL);
	    goto error;
	}
	gotAny = 0;
	for (i = 2; i < argc; i++) {
	    for (itemPtr = StartTagSearch(canvasPtr, argv[i], &search);
		    itemPtr != NULL; itemPtr = NextItem(&search)) {
		if ((itemPtr->x1 >= itemPtr->x2)
			|| (itemPtr->y1 >= itemPtr->y2)) {
		    continue;
		}
		if (!gotAny) {
		    x1 = itemPtr->x1;
		    y1 = itemPtr->y1;
		    x2 = itemPtr->x2;
		    y2 = itemPtr->y2;
		    gotAny = 1;
		} else {
		    if (itemPtr->x1 < x1) {
			x1 = itemPtr->x1;
		    }
		    if (itemPtr->y1 < y1) {
			y1 = itemPtr->y1;
		    }
		    if (itemPtr->x2 > x2) {
			x2 = itemPtr->x2;
		    }
		    if (itemPtr->y2 > y2) {
			y2 = itemPtr->y2;
		    }
		}
	    }
	}
	if (gotAny) {
	    sprintf(interp->result, "%d %d %d %d", x1, y1, x2, y2);
	}
    } else if ((c == 'b') && (strncmp(argv[1], "bind", length) == 0)
	    && (length >= 2)) {
	ClientData object;

	if ((argc < 3) || (argc > 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " bind tagOrId ?sequence? ?command?\"",
		    (char *) NULL);
	    goto error;
	}

	/*
	 * Figure out what object to use for the binding (individual
	 * item vs. tag).
	 */

	object = 0;
	if (isdigit(UCHAR(argv[2][0]))) {
	    int id;
	    char *end;

	    id = strtoul(argv[2], &end, 0);
	    if (*end != 0) {
		goto bindByTag;
	    }
	    for (itemPtr = canvasPtr->firstItemPtr; itemPtr != NULL;
		    itemPtr = itemPtr->nextPtr) {
		if (itemPtr->id == id) {
		    object = (ClientData) itemPtr;
		    break;
		}
	    }
	    if (object == 0) {
		Tcl_AppendResult(interp, "item \"", argv[2],
			"\" doesn't exist", (char *) NULL);
		goto error;
	    }
	} else {
	    bindByTag:
	    object = (ClientData) Tk_GetUid(argv[2]);
	}

	/*
	 * Make a binding table if the canvas doesn't already have
	 * one.
	 */

	if (canvasPtr->bindingTable == NULL) {
	    canvasPtr->bindingTable = Tk_CreateBindingTable(interp);
	}

	if (argc == 5) {
	    int append = 0;
	    unsigned long mask;

	    if (argv[4][0] == 0) {
		result = Tk_DeleteBinding(interp, canvasPtr->bindingTable,
			object, argv[3]);
		goto done;
	    }
	    if (argv[4][0] == '+') {
		argv[4]++;
		append = 1;
	    }
	    mask = Tk_CreateBinding(interp, canvasPtr->bindingTable,
		    object, argv[3], argv[4], append);
	    if (mask == 0) {
		goto error;
	    }
	    if (mask & (unsigned) ~(ButtonMotionMask|Button1MotionMask
		    |Button2MotionMask|Button3MotionMask|Button4MotionMask
		    |Button5MotionMask|ButtonPressMask|ButtonReleaseMask
		    |EnterWindowMask|LeaveWindowMask|KeyPressMask
		    |KeyReleaseMask|PointerMotionMask)) {
		Tk_DeleteBinding(interp, canvasPtr->bindingTable,
			object, argv[3]);
		Tcl_ResetResult(interp);
		Tcl_AppendResult(interp, "requested illegal events; ",
			"only key, button, motion, and enter/leave ",
			"events may be used", (char *) NULL);
		goto error;
	    }
	} else if (argc == 4) {
	    char *command;
    
	    command = Tk_GetBinding(interp, canvasPtr->bindingTable,
		    object, argv[3]);
	    if (command == NULL) {
		goto error;
	    }
	    interp->result = command;
	} else {
	    Tk_GetAllBindings(interp, canvasPtr->bindingTable, object);
	}
    } else if ((c == 'c') && (strcmp(argv[1], "canvasx") == 0)) {
	int x;
	double grid;

	if ((argc < 3) || (argc > 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " canvasx screenx ?gridspacing?\"",
		    (char *) NULL);
	    goto error;
	}
	if (Tk_GetPixels(interp, canvasPtr->tkwin, argv[2], &x) != TCL_OK) {
	    goto error;
	}
	if (argc == 4) {
	    if (Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr, argv[3],
		    &grid) != TCL_OK) {
		goto error;
	    }
	} else {
	    grid = 0.0;
	}
	x += canvasPtr->xOrigin;
	Tcl_PrintDouble(interp, GridAlign((double) x, grid), interp->result);
    } else if ((c == 'c') && (strcmp(argv[1], "canvasy") == 0)) {
	int y;
	double grid;

	if ((argc < 3) || (argc > 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " canvasy screeny ?gridspacing?\"",
		    (char *) NULL);
	    goto error;
	}
	if (Tk_GetPixels(interp, canvasPtr->tkwin, argv[2], &y) != TCL_OK) {
	    goto error;
	}
	if (argc == 4) {
	    if (Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr,
		    argv[3], &grid) != TCL_OK) {
		goto error;
	    }
	} else {
	    grid = 0.0;
	}
	y += canvasPtr->yOrigin;
	Tcl_PrintDouble(interp, GridAlign((double) y, grid), interp->result);
    } else if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Tk_ConfigureValue(interp, canvasPtr->tkwin, configSpecs,
		(char *) canvasPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 3)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, canvasPtr->tkwin, configSpecs,
		    (char *) canvasPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, canvasPtr->tkwin, configSpecs,
		    (char *) canvasPtr, argv[2], 0);
	} else {
	    result = ConfigureCanvas(interp, canvasPtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "coords", length) == 0)
	    && (length >= 3)) {
	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " coords tagOrId ?x y x y ...?\"",
		    (char *) NULL);
	    goto error;
	}
	itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
	if (itemPtr != NULL) {
	    if (argc != 3) {
		Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
			itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	    }
	    if (itemPtr->typePtr->coordProc != NULL) {
		result = (*itemPtr->typePtr->coordProc)(interp,
			(Tk_Canvas) canvasPtr, itemPtr, argc-3, argv+3);
	    }
	    if (argc != 3) {
		Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
			itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	    }
	}
    } else if ((c == 'c') && (strncmp(argv[1], "create", length) == 0)
	    && (length >= 2)) {
	Tk_ItemType *typePtr;
	Tk_ItemType *matchPtr = NULL;
	Tk_Item *itemPtr;

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " create type ?arg arg ...?\"", (char *) NULL);
	    goto error;
	}
	c = argv[2][0];
	length = strlen(argv[2]);
	for (typePtr = typeList; typePtr != NULL; typePtr = typePtr->nextPtr) {
	    if ((c == typePtr->name[0])
		    && (strncmp(argv[2], typePtr->name, length) == 0)) {
		if (matchPtr != NULL) {
		    badType:
		    Tcl_AppendResult(interp,
			    "unknown or ambiguous item type \"",
			    argv[2], "\"", (char *) NULL);
		    goto error;
		}
		matchPtr = typePtr;
	    }
	}
	if (matchPtr == NULL) {
	    goto badType;
	}
	typePtr = matchPtr;
	itemPtr = (Tk_Item *) ckalloc((unsigned) typePtr->itemSize);
	itemPtr->id = canvasPtr->nextId;
	canvasPtr->nextId++;
	itemPtr->tagPtr = itemPtr->staticTagSpace;
	itemPtr->tagSpace = TK_TAG_SPACE;
	itemPtr->numTags = 0;
	itemPtr->typePtr = typePtr;
	if ((*typePtr->createProc)(interp, (Tk_Canvas) canvasPtr,
		itemPtr, argc-3, argv+3) != TCL_OK) {
	    ckfree((char *) itemPtr);
	    goto error;
	}
	itemPtr->nextPtr = NULL;
	canvasPtr->hotPtr = itemPtr;
	canvasPtr->hotPrevPtr = canvasPtr->lastItemPtr;
	if (canvasPtr->lastItemPtr == NULL) {
	    canvasPtr->firstItemPtr = itemPtr;
	} else {
	    canvasPtr->lastItemPtr->nextPtr = itemPtr;
	}
	canvasPtr->lastItemPtr = itemPtr;
	Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	canvasPtr->flags |= REPICK_NEEDED;
	sprintf(interp->result, "%d", itemPtr->id);
    } else if ((c == 'd') && (strncmp(argv[1], "dchars", length) == 0)
	    && (length >= 2)) {
	int first, last;

	if ((argc != 4) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " dchars tagOrId first ?last?\"",
		    (char *) NULL);
	    goto error;
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    if ((itemPtr->typePtr->indexProc == NULL)
		    || (itemPtr->typePtr->dCharsProc == NULL)) {
		continue;
	    }
	    if ((*itemPtr->typePtr->indexProc)(interp, (Tk_Canvas) canvasPtr,
		    itemPtr, argv[3], &first) != TCL_OK) {
		goto error;
	    }
	    if (argc == 5) {
		if ((*itemPtr->typePtr->indexProc)(interp,
			(Tk_Canvas) canvasPtr, itemPtr, argv[4], &last)
			!= TCL_OK) {
		    goto error;
		}
	    } else {
		last = first;
	    }

	    /*
	     * Redraw both item's old and new areas:  it's possible
	     * that a delete could result in a new area larger than
	     * the old area.
	     */

	    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		    itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	    (*itemPtr->typePtr->dCharsProc)((Tk_Canvas) canvasPtr,
		    itemPtr, first, last);
	    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		    itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "delete", length) == 0)
	    && (length >= 2)) {
	int i;

	for (i = 2; i < argc; i++) {
	    for (itemPtr = StartTagSearch(canvasPtr, argv[i], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
		Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
			itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
		if (canvasPtr->bindingTable != NULL) {
		    Tk_DeleteAllBindings(canvasPtr->bindingTable,
			    (ClientData) itemPtr);
		}
		(*itemPtr->typePtr->deleteProc)((Tk_Canvas) canvasPtr, itemPtr,
			canvasPtr->display);
		if (itemPtr->tagPtr != itemPtr->staticTagSpace) {
		    ckfree((char *) itemPtr->tagPtr);
		}
		if (search.prevPtr == NULL) {
		    canvasPtr->firstItemPtr = itemPtr->nextPtr;
		    if (canvasPtr->firstItemPtr == NULL) {
			canvasPtr->lastItemPtr = NULL;
		    }
		} else {
		    search.prevPtr->nextPtr = itemPtr->nextPtr;
		}
		if (canvasPtr->lastItemPtr == itemPtr) {
		    canvasPtr->lastItemPtr = search.prevPtr;
		}
		ckfree((char *) itemPtr);
		if (itemPtr == canvasPtr->currentItemPtr) {
		    canvasPtr->currentItemPtr = NULL;
		    canvasPtr->flags |= REPICK_NEEDED;
		}
		if (itemPtr == canvasPtr->newCurrentPtr) {
		    canvasPtr->newCurrentPtr = NULL;
		    canvasPtr->flags |= REPICK_NEEDED;
		}
		if (itemPtr == canvasPtr->textInfo.focusItemPtr) {
		    canvasPtr->textInfo.focusItemPtr = NULL;
		}
		if (itemPtr == canvasPtr->textInfo.selItemPtr) {
		    canvasPtr->textInfo.selItemPtr = NULL;
		}
		if ((itemPtr == canvasPtr->hotPtr)
			|| (itemPtr == canvasPtr->hotPrevPtr)) {
		    canvasPtr->hotPtr = NULL;
		}
	    }
	}
    } else if ((c == 'd') && (strncmp(argv[1], "dtag", length) == 0)
	    && (length >= 2)) {
	Tk_Uid tag;
	int i;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " dtag tagOrId ?tagToDelete?\"",
		    (char *) NULL);
	    goto error;
	}
	if (argc == 4) {
	    tag = Tk_GetUid(argv[3]);
	} else {
	    tag = Tk_GetUid(argv[2]);
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    for (i = itemPtr->numTags-1; i >= 0; i--) {
		if (itemPtr->tagPtr[i] == tag) {
		    itemPtr->tagPtr[i] = itemPtr->tagPtr[itemPtr->numTags-1];
		    itemPtr->numTags--;
		}
	    }
	}
    } else if ((c == 'f') && (strncmp(argv[1], "find", length) == 0)
	    && (length >= 2)) {
	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " find searchCommand ?arg arg ...?\"",
		    (char *) NULL);
	    goto error;
	}
	result = FindItems(interp, canvasPtr, argc-2, argv+2, (char *) NULL,
		argv[0]," find");
    } else if ((c == 'f') && (strncmp(argv[1], "focus", length) == 0)
	    && (length >= 2)) {
	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " focus ?tagOrId?\"",
		    (char *) NULL);
	    goto error;
	}
	itemPtr = canvasPtr->textInfo.focusItemPtr;
	if (argc == 2) {
	    if (itemPtr != NULL) {
		sprintf(interp->result, "%d", itemPtr->id);
	    }
	    goto done;
	}
	if ((itemPtr != NULL) && (canvasPtr->textInfo.gotFocus)) {
	    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		    itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	}
	if (argv[2][0] == 0) {
	    canvasPtr->textInfo.focusItemPtr = NULL;
	    goto done;
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    if (itemPtr->typePtr->icursorProc != NULL) {
		break;
	    }
	}
	if (itemPtr == NULL) {
	    goto done;
	}
	canvasPtr->textInfo.focusItemPtr = itemPtr;
	if (canvasPtr->textInfo.gotFocus) {
	    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		    itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	}
    } else if ((c == 'g') && (strncmp(argv[1], "gettags", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " gettags tagOrId\"", (char *) NULL);
	    goto error;
	}
	itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
	if (itemPtr != NULL) {
	    int i;
	    for (i = 0; i < itemPtr->numTags; i++) {
		Tcl_AppendElement(interp, (char *) itemPtr->tagPtr[i]);
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "icursor", length) == 0)
	    && (length >= 2)) {
	int index;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " icursor tagOrId index\"",
		    (char *) NULL);
	    goto error;
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    if ((itemPtr->typePtr->indexProc == NULL)
		    || (itemPtr->typePtr->icursorProc == NULL)) {
		goto done;
	    }
	    if ((*itemPtr->typePtr->indexProc)(interp, (Tk_Canvas) canvasPtr,
		    itemPtr, argv[3], &index) != TCL_OK) {
		goto error;
	    }
	    (*itemPtr->typePtr->icursorProc)((Tk_Canvas) canvasPtr, itemPtr,
		    index);
	    if ((itemPtr == canvasPtr->textInfo.focusItemPtr)
		    && (canvasPtr->textInfo.cursorOn)) {
		Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
			itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "index", length) == 0)
	    && (length >= 3)) {
	int index;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " index tagOrId string\"",
		    (char *) NULL);
	    goto error;
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    if (itemPtr->typePtr->indexProc != NULL) {
		break;
	    }
	}
	if (itemPtr == NULL) {
	    Tcl_AppendResult(interp, "can't find an indexable item \"",
		    argv[2], "\"", (char *) NULL);
	    goto error;
	}
	if ((*itemPtr->typePtr->indexProc)(interp, (Tk_Canvas) canvasPtr,
		itemPtr, argv[3], &index) != TCL_OK) {
	    goto error;
	}
	sprintf(interp->result, "%d", index);
    } else if ((c == 'i') && (strncmp(argv[1], "insert", length) == 0)
	    && (length >= 3)) {
	int beforeThis;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " insert tagOrId beforeThis string\"",
		    (char *) NULL);
	    goto error;
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    if ((itemPtr->typePtr->indexProc == NULL)
		    || (itemPtr->typePtr->insertProc == NULL)) {
		continue;
	    }
	    if ((*itemPtr->typePtr->indexProc)(interp, (Tk_Canvas) canvasPtr,
		    itemPtr, argv[3], &beforeThis) != TCL_OK) {
		goto error;
	    }

	    /*
	     * Redraw both item's old and new areas:  it's possible
	     * that an insertion could result in a new area either
	     * larger or smaller than the old area.
	     */

	    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		    itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	    (*itemPtr->typePtr->insertProc)((Tk_Canvas) canvasPtr,
		    itemPtr, beforeThis, argv[4]);
	    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr, itemPtr->x1,
		    itemPtr->y1, itemPtr->x2, itemPtr->y2);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "itemcget", length) == 0)
	    && (length >= 6)) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " itemcget tagOrId option\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
	if (itemPtr != NULL) {
	    result = Tk_ConfigureValue(canvasPtr->interp, canvasPtr->tkwin,
		    itemPtr->typePtr->configSpecs, (char *) itemPtr,
		    argv[3], 0);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "itemconfigure", length) == 0)
	    && (length >= 6)) {
	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " itemconfigure tagOrId ?option value ...?\"",
		    (char *) NULL);
	    goto error;
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    if (argc == 3) {
		result = Tk_ConfigureInfo(canvasPtr->interp, canvasPtr->tkwin,
			itemPtr->typePtr->configSpecs, (char *) itemPtr,
			(char *) NULL, 0);
	    } else if (argc == 4) {
		result = Tk_ConfigureInfo(canvasPtr->interp, canvasPtr->tkwin,
			itemPtr->typePtr->configSpecs, (char *) itemPtr,
			argv[3], 0);
	    } else {
		Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
			itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
		result = (*itemPtr->typePtr->configProc)(interp,
			(Tk_Canvas) canvasPtr, itemPtr, argc-3, argv+3,
			TK_CONFIG_ARGV_ONLY);
		Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
			itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
		canvasPtr->flags |= REPICK_NEEDED;
	    }
	    if ((result != TCL_OK) || (argc < 5)) {
		break;
	    }
	}
    } else if ((c == 'l') && (strncmp(argv[1], "lower", length) == 0)) {
	Tk_Item *prevPtr;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " lower tagOrId ?belowThis?\"",
		    (char *) NULL);
	    goto error;
	}

	/*
	 * First find the item just after which we'll insert the
	 * named items.
	 */

	if (argc == 3) {
	    prevPtr = NULL;
	} else {
	    prevPtr = StartTagSearch(canvasPtr, argv[3], &search);
	    if (prevPtr != NULL) {
		prevPtr = search.prevPtr;
	    } else {
		Tcl_AppendResult(interp, "tag \"", argv[3],
			"\" doesn't match any items", (char *) NULL);
		goto error;
	    }
	}
	RelinkItems(canvasPtr, argv[2], prevPtr);
    } else if ((c == 'm') && (strncmp(argv[1], "move", length) == 0)) {
	double xAmount, yAmount;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " move tagOrId xAmount yAmount\"",
		    (char *) NULL);
	    goto error;
	}
	if ((Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr, argv[3],
		&xAmount) != TCL_OK) || (Tk_CanvasGetCoord(interp,
		(Tk_Canvas) canvasPtr, argv[4], &yAmount) != TCL_OK)) {
	    goto error;
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		    itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	    (void) (*itemPtr->typePtr->translateProc)((Tk_Canvas) canvasPtr,
		    itemPtr,  xAmount, yAmount);
	    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		    itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	    canvasPtr->flags |= REPICK_NEEDED;
	}
    } else if ((c == 'p') && (strncmp(argv[1], "postscript", length) == 0)) {
	result = TkCanvPostscriptCmd(canvasPtr, interp, argc, argv);
    } else if ((c == 'r') && (strncmp(argv[1], "raise", length) == 0)) {
	Tk_Item *prevPtr;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " raise tagOrId ?aboveThis?\"",
		    (char *) NULL);
	    goto error;
	}

	/*
	 * First find the item just after which we'll insert the
	 * named items.
	 */

	if (argc == 3) {
	    prevPtr = canvasPtr->lastItemPtr;
	} else {
	    prevPtr = NULL;
	    for (itemPtr = StartTagSearch(canvasPtr, argv[3], &search);
		    itemPtr != NULL; itemPtr = NextItem(&search)) {
		prevPtr = itemPtr;
	    }
	    if (prevPtr == NULL) {
		Tcl_AppendResult(interp, "tagOrId \"", argv[3],
			"\" doesn't match any items", (char *) NULL);
		goto error;
	    }
	}
	RelinkItems(canvasPtr, argv[2], prevPtr);
    } else if ((c == 's') && (strncmp(argv[1], "scale", length) == 0)
	    && (length >= 3)) {
	double xOrigin, yOrigin, xScale, yScale;

	if (argc != 7) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " scale tagOrId xOrigin yOrigin xScale yScale\"",
		    (char *) NULL);
	    goto error;
	}
	if ((Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr,
		    argv[3], &xOrigin) != TCL_OK)
		|| (Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr,
		    argv[4], &yOrigin) != TCL_OK)
		|| (Tcl_GetDouble(interp, argv[5], &xScale) != TCL_OK)
		|| (Tcl_GetDouble(interp, argv[6], &yScale) != TCL_OK)) {
	    goto error;
	}
	if ((xScale == 0.0) || (yScale == 0.0)) {
	    interp->result = "scale factor cannot be zero";
	    goto error;
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		    itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	    (void) (*itemPtr->typePtr->scaleProc)((Tk_Canvas) canvasPtr,
		    itemPtr, xOrigin, yOrigin, xScale, yScale);
	    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		    itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
	    canvasPtr->flags |= REPICK_NEEDED;
	}
    } else if ((c == 's') && (strncmp(argv[1], "scan", length) == 0)
	    && (length >= 3)) {
	int x, y;

	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " scan mark|dragto x y\"", (char *) NULL);
	    goto error;
	}
	if ((Tcl_GetInt(interp, argv[3], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[4], &y) != TCL_OK)){
	    goto error;
	}
	if ((argv[2][0] == 'm')
		&& (strncmp(argv[2], "mark", strlen(argv[2])) == 0)) {
	    canvasPtr->scanX = x;
	    canvasPtr->scanXOrigin = canvasPtr->xOrigin;
	    canvasPtr->scanY = y;
	    canvasPtr->scanYOrigin = canvasPtr->yOrigin;
	} else if ((argv[2][0] == 'd')
		&& (strncmp(argv[2], "dragto", strlen(argv[2])) == 0)) {
	    int newXOrigin, newYOrigin, tmp;

	    /*
	     * Compute a new view origin for the canvas, amplifying the
	     * mouse motion.
	     */

	    tmp = canvasPtr->scanXOrigin - 10*(x - canvasPtr->scanX)
		    - canvasPtr->scrollX1;
	    newXOrigin = canvasPtr->scrollX1 + tmp;
	    tmp = canvasPtr->scanYOrigin - 10*(y - canvasPtr->scanY)
		    - canvasPtr->scrollY1;
	    newYOrigin = canvasPtr->scrollY1 + tmp;
	    CanvasSetOrigin(canvasPtr, newXOrigin, newYOrigin);
	} else {
	    Tcl_AppendResult(interp, "bad scan option \"", argv[2],
		    "\": must be mark or dragto", (char *) NULL);
	    goto error;
	}
    } else if ((c == 's') && (strncmp(argv[1], "select", length) == 0)
	    && (length >= 2)) {
	int index;

	if (argc < 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " select option ?tagOrId? ?arg?\"", (char *) NULL);
	    goto error;
	}
	if (argc >= 4) {
	    for (itemPtr = StartTagSearch(canvasPtr, argv[3], &search);
		    itemPtr != NULL; itemPtr = NextItem(&search)) {
		if ((itemPtr->typePtr->indexProc != NULL)
			&& (itemPtr->typePtr->selectionProc != NULL)){
		    break;
		}
	    }
	    if (itemPtr == NULL) {
		Tcl_AppendResult(interp,
			"can't find an indexable and selectable item \"",
			argv[3], "\"", (char *) NULL);
		goto error;
	    }
	}
	if (argc == 5) {
	    if ((*itemPtr->typePtr->indexProc)(interp, (Tk_Canvas) canvasPtr,
		    itemPtr, argv[4], &index) != TCL_OK) {
		goto error;
	    }
	}
	length = strlen(argv[2]);
	c = argv[2][0];
	if ((c == 'a') && (strncmp(argv[2], "adjust", length) == 0)) {
	    if (argc != 5) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " select adjust tagOrId index\"",
			(char *) NULL);
		goto error;
	    }
	    if (canvasPtr->textInfo.selItemPtr == itemPtr) {
		if (index < (canvasPtr->textInfo.selectFirst
			+ canvasPtr->textInfo.selectLast)/2) {
		    canvasPtr->textInfo.selectAnchor =
			    canvasPtr->textInfo.selectLast + 1;
		} else {
		    canvasPtr->textInfo.selectAnchor =
			    canvasPtr->textInfo.selectFirst;
		}
	    }
	    CanvasSelectTo(canvasPtr, itemPtr, index);
	} else if ((c == 'c') && (argv[2] != NULL)
		&& (strncmp(argv[2], "clear", length) == 0)) {
	    if (argc != 3) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " select clear\"", (char *) NULL);
		goto error;
	    }
	    if (canvasPtr->textInfo.selItemPtr != NULL) {
		Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
			canvasPtr->textInfo.selItemPtr->x1,
			canvasPtr->textInfo.selItemPtr->y1,
			canvasPtr->textInfo.selItemPtr->x2,
			canvasPtr->textInfo.selItemPtr->y2);
		canvasPtr->textInfo.selItemPtr = NULL;
	    }
	    goto done;
	} else if ((c == 'f') && (strncmp(argv[2], "from", length) == 0)) {
	    if (argc != 5) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " select from tagOrId index\"",
			(char *) NULL);
		goto error;
	    }
	    canvasPtr->textInfo.anchorItemPtr = itemPtr;
	    canvasPtr->textInfo.selectAnchor = index;
	} else if ((c == 'i') && (strncmp(argv[2], "item", length) == 0)) {
	    if (argc != 3) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " select item\"", (char *) NULL);
		goto error;
	    }
	    if (canvasPtr->textInfo.selItemPtr != NULL) {
		sprintf(interp->result, "%d",
			canvasPtr->textInfo.selItemPtr->id);
	    }
	} else if ((c == 't') && (strncmp(argv[2], "to", length) == 0)) {
	    if (argc != 5) {
		Tcl_AppendResult(interp, "wrong # args: should be \"",
			argv[0], " select to tagOrId index\"",
			(char *) NULL);
		goto error;
	    }
	    CanvasSelectTo(canvasPtr, itemPtr, index);
	} else {
	    Tcl_AppendResult(interp, "bad select option \"", argv[2],
		    "\": must be adjust, clear, from, item, or to",
		    (char *) NULL);
	    goto error;
	}
    } else if ((c == 't') && (strncmp(argv[1], "type", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " type tag\"", (char *) NULL);
	    goto error;
	}
	itemPtr = StartTagSearch(canvasPtr, argv[2], &search);
	if (itemPtr != NULL) {
	    interp->result = itemPtr->typePtr->name;
	}
    } else if ((c == 'x') && (strncmp(argv[1], "xview", length) == 0)) {
	int count, type;
	int newX = 0;		/* Initialization needed only to prevent
				 * gcc warnings. */
	double fraction;

	if (argc == 2) {
	    PrintScrollFractions(canvasPtr->xOrigin + canvasPtr->inset,
		    canvasPtr->xOrigin + Tk_Width(canvasPtr->tkwin)
		    - canvasPtr->inset, canvasPtr->scrollX1,
		    canvasPtr->scrollX2, interp->result);
	} else {
	    type = Tk_GetScrollInfo(interp, argc, argv, &fraction, &count);
	    switch (type) {
		case TK_SCROLL_ERROR:
		    goto error;
		case TK_SCROLL_MOVETO:
		    newX = canvasPtr->scrollX1 - canvasPtr->inset
			    + (int) (fraction * (canvasPtr->scrollX2
			    - canvasPtr->scrollX1) + 0.5);
		    break;
		case TK_SCROLL_PAGES:
		    newX = canvasPtr->xOrigin + count * .9
			    * (Tk_Width(canvasPtr->tkwin) - 2*canvasPtr->inset);
		    break;
		case TK_SCROLL_UNITS:
		    if (canvasPtr->xScrollIncrement > 0) {
			newX = canvasPtr->xOrigin
				+ count*canvasPtr->xScrollIncrement;
		    } else {
			newX = canvasPtr->xOrigin + count * .1
				* (Tk_Width(canvasPtr->tkwin)
				- 2*canvasPtr->inset);
		    }
		    break;
	    }
	    CanvasSetOrigin(canvasPtr, newX, canvasPtr->yOrigin);
	}
    } else if ((c == 'y') && (strncmp(argv[1], "yview", length) == 0)) {
	int count, type;
	int newY = 0;		/* Initialization needed only to prevent
				 * gcc warnings. */
	double fraction;

	if (argc == 2) {
	    PrintScrollFractions(canvasPtr->yOrigin + canvasPtr->inset,
		    canvasPtr->yOrigin + Tk_Height(canvasPtr->tkwin)
		    - canvasPtr->inset, canvasPtr->scrollY1,
		    canvasPtr->scrollY2, interp->result);
	} else {
	    type = Tk_GetScrollInfo(interp, argc, argv, &fraction, &count);
	    switch (type) {
		case TK_SCROLL_ERROR:
		    goto error;
		case TK_SCROLL_MOVETO:
		    newY = canvasPtr->scrollY1 - canvasPtr->inset
			    + (int) (fraction*(canvasPtr->scrollY2
			    - canvasPtr->scrollY1) + 0.5);
		    break;
		case TK_SCROLL_PAGES:
		    newY = canvasPtr->yOrigin + count * .9
			    * (Tk_Height(canvasPtr->tkwin)
			    - 2*canvasPtr->inset);
		    break;
		case TK_SCROLL_UNITS:
		    if (canvasPtr->yScrollIncrement > 0) {
			newY = canvasPtr->yOrigin
				+ count*canvasPtr->yScrollIncrement;
		    } else {
			newY = canvasPtr->yOrigin + count * .1
				* (Tk_Height(canvasPtr->tkwin)
				- 2*canvasPtr->inset);
		    }
		    break;
	    }
	    CanvasSetOrigin(canvasPtr, canvasPtr->xOrigin, newY);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be addtag, bbox, bind, ",
		"canvasx, canvasy, cget, configure, coords, create, ",
		"dchars, delete, dtag, find, focus, ",
		"gettags, icursor, index, insert, itemcget, itemconfigure, ",
		"lower, move, postscript, raise, scale, scan, ",
		"select, type, xview, or yview",
		(char *) NULL);  
	goto error;
    }
    done:
    Tcl_Release((ClientData) canvasPtr);
    return result;

    error:
    Tcl_Release((ClientData) canvasPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyCanvas --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a canvas at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the canvas is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyCanvas(memPtr)
    char *memPtr;		/* Info about canvas widget. */
{
    TkCanvas *canvasPtr = (TkCanvas *) memPtr;
    Tk_Item *itemPtr;

    /*
     * Free up all of the items in the canvas.
     */

    for (itemPtr = canvasPtr->firstItemPtr; itemPtr != NULL;
	    itemPtr = canvasPtr->firstItemPtr) {
	canvasPtr->firstItemPtr = itemPtr->nextPtr;
	(*itemPtr->typePtr->deleteProc)((Tk_Canvas) canvasPtr, itemPtr,
		canvasPtr->display);
	if (itemPtr->tagPtr != itemPtr->staticTagSpace) {
	    ckfree((char *) itemPtr->tagPtr);
	}
	ckfree((char *) itemPtr);
    }

    /*
     * Free up all the stuff that requires special handling,
     * then let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (canvasPtr->pixmapGC != None) {
	Tk_FreeGC(canvasPtr->display, canvasPtr->pixmapGC);
    }
    Tcl_DeleteTimerHandler(canvasPtr->insertBlinkHandler);
    if (canvasPtr->bindingTable != NULL) {
	Tk_DeleteBindingTable(canvasPtr->bindingTable);
    }
    Tk_FreeOptions(configSpecs, (char *) canvasPtr, canvasPtr->display, 0);
    ckfree((char *) canvasPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureCanvas --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a canvas widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for canvasPtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureCanvas(interp, canvasPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    TkCanvas *canvasPtr;	/* Information about widget;  may or may
				 * not already have values for some fields. */
    int argc;			/* Number of valid entries in argv. */
    char **argv;		/* Arguments. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    XGCValues gcValues;
    GC new;

    if (Tk_ConfigureWidget(interp, canvasPtr->tkwin, configSpecs,
	    argc, argv, (char *) canvasPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few options need special processing, such as setting the
     * background from a 3-D border and creating a GC for copying
     * bits to the screen.
     */

    Tk_SetBackgroundFromBorder(canvasPtr->tkwin, canvasPtr->bgBorder);

    if (canvasPtr->highlightWidth < 0) {
	canvasPtr->highlightWidth = 0;
    }
    canvasPtr->inset = canvasPtr->borderWidth + canvasPtr->highlightWidth;

    gcValues.function = GXcopy;
    gcValues.foreground = Tk_3DBorderColor(canvasPtr->bgBorder)->pixel;
    gcValues.graphics_exposures = False;
    new = Tk_GetGC(canvasPtr->tkwin,
	    GCFunction|GCForeground|GCGraphicsExposures, &gcValues);
    if (canvasPtr->pixmapGC != None) {
	Tk_FreeGC(canvasPtr->display, canvasPtr->pixmapGC);
    }
    canvasPtr->pixmapGC = new;

    /*
     * Reset the desired dimensions for the window.
     */

    Tk_GeometryRequest(canvasPtr->tkwin, canvasPtr->width + 2*canvasPtr->inset,
	    canvasPtr->height + 2*canvasPtr->inset);

    /*
     * Restart the cursor timing sequence in case the on-time or off-time
     * just changed.
     */

    if (canvasPtr->textInfo.gotFocus) {
	CanvasFocusProc(canvasPtr, 1);
    }

    /*
     * Recompute the scroll region.
     */

    canvasPtr->scrollX1 = 0;
    canvasPtr->scrollY1 = 0;
    canvasPtr->scrollX2 = 0;
    canvasPtr->scrollY2 = 0;
    if (canvasPtr->regionString != NULL) {
	int argc2;
	char **argv2;

	if (Tcl_SplitList(canvasPtr->interp, canvasPtr->regionString,
		&argc2, &argv2) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (argc2 != 4) {
	    Tcl_AppendResult(interp, "bad scrollRegion \"",
		    canvasPtr->regionString, "\"", (char *) NULL);
	    badRegion:
	    ckfree(canvasPtr->regionString);
	    ckfree((char *) argv2);
	    canvasPtr->regionString = NULL;
	    return TCL_ERROR;
	}
	if ((Tk_GetPixels(canvasPtr->interp, canvasPtr->tkwin,
		    argv2[0], &canvasPtr->scrollX1) != TCL_OK)
		|| (Tk_GetPixels(canvasPtr->interp, canvasPtr->tkwin,
		    argv2[1], &canvasPtr->scrollY1) != TCL_OK)
		|| (Tk_GetPixels(canvasPtr->interp, canvasPtr->tkwin,
		    argv2[2], &canvasPtr->scrollX2) != TCL_OK)
		|| (Tk_GetPixels(canvasPtr->interp, canvasPtr->tkwin,
		    argv2[3], &canvasPtr->scrollY2) != TCL_OK)) {
	    goto badRegion;
	}
	ckfree((char *) argv2);
    }

    /*
     * Reset the canvas's origin (this is a no-op unless confine
     * mode has just been turned on or the scroll region has changed).
     */

    CanvasSetOrigin(canvasPtr, canvasPtr->xOrigin, canvasPtr->yOrigin);
    canvasPtr->flags |= UPDATE_SCROLLBARS|REDRAW_BORDERS;
    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
	    canvasPtr->xOrigin, canvasPtr->yOrigin,
	    canvasPtr->xOrigin + Tk_Width(canvasPtr->tkwin),
	    canvasPtr->yOrigin + Tk_Height(canvasPtr->tkwin));
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayCanvas --
 *
 *	This procedure redraws the contents of a canvas window.
 *	It is invoked as a do-when-idle handler, so it only runs
 *	when there's nothing else for the application to do.
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
DisplayCanvas(clientData)
    ClientData clientData;	/* Information about widget. */
{
    TkCanvas *canvasPtr = (TkCanvas *) clientData;
    Tk_Window tkwin = canvasPtr->tkwin;
    Tk_Item *itemPtr;
    Pixmap pixmap;
    int screenX1, screenX2, screenY1, screenY2, width, height;

    if (canvasPtr->tkwin == NULL) {
	return;
    }
    if (!Tk_IsMapped(tkwin)) {
	goto done;
    }

    /*
     * Choose a new current item if that is needed (this could cause
     * event handlers to be invoked).
     */

    while (canvasPtr->flags & REPICK_NEEDED) {
	Tcl_Preserve((ClientData) canvasPtr);
	canvasPtr->flags &= ~REPICK_NEEDED;
	PickCurrentItem(canvasPtr, &canvasPtr->pickEvent);
	tkwin = canvasPtr->tkwin;
	Tcl_Release((ClientData) canvasPtr);
	if (tkwin == NULL) {
	    return;
	}
    }

    /*
     * Compute the intersection between the area that needs redrawing
     * and the area that's visible on the screen.
     */

    if ((canvasPtr->redrawX1 < canvasPtr->redrawX2)
	    && (canvasPtr->redrawY1 < canvasPtr->redrawY2)) {
	screenX1 = canvasPtr->xOrigin + canvasPtr->inset;
	screenY1 = canvasPtr->yOrigin + canvasPtr->inset;
	screenX2 = canvasPtr->xOrigin + Tk_Width(tkwin) - canvasPtr->inset;
	screenY2 = canvasPtr->yOrigin + Tk_Height(tkwin) - canvasPtr->inset;
	if (canvasPtr->redrawX1 > screenX1) {
	    screenX1 = canvasPtr->redrawX1;
	}
	if (canvasPtr->redrawY1 > screenY1) {
	    screenY1 = canvasPtr->redrawY1;
	}
	if (canvasPtr->redrawX2 < screenX2) {
	    screenX2 = canvasPtr->redrawX2;
	}
	if (canvasPtr->redrawY2 < screenY2) {
	    screenY2 = canvasPtr->redrawY2;
	}
	if ((screenX1 >= screenX2) || (screenY1 >= screenY2)) {
	    goto borders;
	}
    
	/*
	 * Redrawing is done in a temporary pixmap that is allocated
	 * here and freed at the end of the procedure.  All drawing
	 * is done to the pixmap, and the pixmap is copied to the
	 * screen at the end of the procedure. The temporary pixmap
	 * serves two purposes:
	 *
	 * 1. It provides a smoother visual effect (no clearing and
	 *    gradual redraw will be visible to users).
	 * 2. It allows us to redraw only the objects that overlap
	 *    the redraw area.  Otherwise incorrect results could
	 *	  occur from redrawing things that stick outside of
	 *	  the redraw area (we'd have to redraw everything in
	 *    order to make the overlaps look right).
	 *
	 * Some tricky points about the pixmap:
	 *
	 * 1. We only allocate a large enough pixmap to hold the
	 *    area that has to be redisplayed.  This saves time in
	 *    in the X server for large objects that cover much
	 *    more than the area being redisplayed:  only the area
	 *    of the pixmap will actually have to be redrawn.
	 * 2. Some X servers (e.g. the one for DECstations) have troubles
	 *    with characters that overlap an edge of the pixmap (on the
	 *    DEC servers, as of 8/18/92, such characters are drawn one
	 *    pixel too far to the right).  To handle this problem,
	 *    make the pixmap a bit larger than is absolutely needed
	 *    so that for normal-sized fonts the characters that overlap
	 *    the edge of the pixmap will be outside the area we care
	 *    about.
	 */
    
	canvasPtr->drawableXOrigin = screenX1 - 30;
	canvasPtr->drawableYOrigin = screenY1 - 30;
	pixmap = Tk_GetPixmap(Tk_Display(tkwin), Tk_WindowId(tkwin),
	    (screenX2 + 30 - canvasPtr->drawableXOrigin),
	    (screenY2 + 30 - canvasPtr->drawableYOrigin),
	    Tk_Depth(tkwin));
    
	/*
	 * Clear the area to be redrawn.
	 */
    
	width = screenX2 - screenX1;
	height = screenY2 - screenY1;
    
	XFillRectangle(Tk_Display(tkwin), pixmap, canvasPtr->pixmapGC,
		screenX1 - canvasPtr->drawableXOrigin,
		screenY1 - canvasPtr->drawableYOrigin, (unsigned int) width,
		(unsigned int) height);
    
	/*
	 * Scan through the item list, redrawing those items that need it.
	 * An item must be redraw if either (a) it intersects the smaller
	 * on-screen area or (b) it intersects the full canvas area and its
	 * type requests that it be redrawn always (e.g. so subwindows can
	 * be unmapped when they move off-screen).
	 */
    
	for (itemPtr = canvasPtr->firstItemPtr; itemPtr != NULL;
		itemPtr = itemPtr->nextPtr) {
	    if ((itemPtr->x1 >= screenX2)
		    || (itemPtr->y1 >= screenY2)
		    || (itemPtr->x2 < screenX1)
		    || (itemPtr->y2 < screenY1)) {
		if (!itemPtr->typePtr->alwaysRedraw
			|| (itemPtr->x1 >= canvasPtr->redrawX2)
			|| (itemPtr->y1 >= canvasPtr->redrawY2)
			|| (itemPtr->x2 < canvasPtr->redrawX1)
			|| (itemPtr->y2 < canvasPtr->redrawY1)) {
		    continue;
		}
	    }
	    (*itemPtr->typePtr->displayProc)((Tk_Canvas) canvasPtr, itemPtr,
		    canvasPtr->display, pixmap, screenX1, screenY1, width,
		    height);
	}
    
	/*
	 * Copy from the temporary pixmap to the screen, then free up
	 * the temporary pixmap.
	 */
    
	XCopyArea(Tk_Display(tkwin), pixmap, Tk_WindowId(tkwin),
		canvasPtr->pixmapGC,
		screenX1 - canvasPtr->drawableXOrigin,
		screenY1 - canvasPtr->drawableYOrigin,
		(unsigned) (screenX2 - screenX1),
		(unsigned) (screenY2 - screenY1),
		screenX1 - canvasPtr->xOrigin, screenY1 - canvasPtr->yOrigin);
	Tk_FreePixmap(Tk_Display(tkwin), pixmap);
    }

    /*
     * Draw the window borders, if needed.
     */

    borders:
    if (canvasPtr->flags & REDRAW_BORDERS) {
	canvasPtr->flags &= ~REDRAW_BORDERS;
	if (canvasPtr->borderWidth > 0) {
	    Tk_Draw3DRectangle(tkwin, Tk_WindowId(tkwin),
		    canvasPtr->bgBorder, canvasPtr->highlightWidth,
		    canvasPtr->highlightWidth,
		    Tk_Width(tkwin) - 2*canvasPtr->highlightWidth,
		    Tk_Height(tkwin) - 2*canvasPtr->highlightWidth,
		    canvasPtr->borderWidth, canvasPtr->relief);
	}
	if (canvasPtr->highlightWidth != 0) {
	    GC gc;
    
	    if (canvasPtr->textInfo.gotFocus) {
		gc = Tk_GCForColor(canvasPtr->highlightColorPtr,
			Tk_WindowId(tkwin));
	    } else {
		gc = Tk_GCForColor(canvasPtr->highlightBgColorPtr,
			Tk_WindowId(tkwin));
	    }
	    Tk_DrawFocusHighlight(tkwin, gc, canvasPtr->highlightWidth,
		    Tk_WindowId(tkwin));
	}
    }

    done:
    canvasPtr->flags &= ~REDRAW_PENDING;
    canvasPtr->redrawX1 = canvasPtr->redrawX2 = 0;
    canvasPtr->redrawY1 = canvasPtr->redrawY2 = 0;
    if (canvasPtr->flags & UPDATE_SCROLLBARS) {
	CanvasUpdateScrollbars(canvasPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * CanvasEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on canvases.
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
CanvasEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    TkCanvas *canvasPtr = (TkCanvas *) clientData;

    if (eventPtr->type == Expose) {
	int x, y;

	x = eventPtr->xexpose.x + canvasPtr->xOrigin;
	y = eventPtr->xexpose.y + canvasPtr->yOrigin;
	Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr, x, y,
		x + eventPtr->xexpose.width,
		y + eventPtr->xexpose.height);
	if ((eventPtr->xexpose.x < canvasPtr->inset)
		|| (eventPtr->xexpose.y < canvasPtr->inset)
		|| ((eventPtr->xexpose.x + eventPtr->xexpose.width)
		    > (Tk_Width(canvasPtr->tkwin) - canvasPtr->inset))
		|| ((eventPtr->xexpose.y + eventPtr->xexpose.height)
		    > (Tk_Height(canvasPtr->tkwin) - canvasPtr->inset))) {
	    canvasPtr->flags |= REDRAW_BORDERS;
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (canvasPtr->tkwin != NULL) {
	    canvasPtr->tkwin = NULL;
	    Tcl_DeleteCommand(canvasPtr->interp,
		    Tcl_GetCommandName(canvasPtr->interp,
		    canvasPtr->widgetCmd));
	}
	if (canvasPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayCanvas, (ClientData) canvasPtr);
	}
	Tcl_EventuallyFree((ClientData) canvasPtr, DestroyCanvas);
    } else if (eventPtr->type == ConfigureNotify) {
	canvasPtr->flags |= UPDATE_SCROLLBARS;

	/*
	 * The call below is needed in order to recenter the canvas if
	 * it's confined and its scroll region is smaller than the window.
	 */

	CanvasSetOrigin(canvasPtr, canvasPtr->xOrigin, canvasPtr->yOrigin);
	Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr, canvasPtr->xOrigin,
		canvasPtr->yOrigin,
		canvasPtr->xOrigin + Tk_Width(canvasPtr->tkwin),
		canvasPtr->yOrigin + Tk_Height(canvasPtr->tkwin));
	canvasPtr->flags |= REDRAW_BORDERS;
    } else if (eventPtr->type == FocusIn) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    CanvasFocusProc(canvasPtr, 1);
	}
    } else if (eventPtr->type == FocusOut) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    CanvasFocusProc(canvasPtr, 0);
	}
    } else if (eventPtr->type == UnmapNotify) {
	Tk_Item *itemPtr;

	/*
	 * Special hack:  if the canvas is unmapped, then must notify
	 * all items with "alwaysRedraw" set, so that they know that
	 * they are no longer displayed.
	 */

	for (itemPtr = canvasPtr->firstItemPtr; itemPtr != NULL;
		itemPtr = itemPtr->nextPtr) {
	    if (itemPtr->typePtr->alwaysRedraw) {
		(*itemPtr->typePtr->displayProc)((Tk_Canvas) canvasPtr,
			itemPtr, canvasPtr->display, None, 0, 0, 0, 0);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CanvasCmdDeletedProc --
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
CanvasCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    TkCanvas *canvasPtr = (TkCanvas *) clientData;
    Tk_Window tkwin = canvasPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	canvasPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasEventuallyRedraw --
 *
 *	Arrange for part or all of a canvas widget to redrawn at
 *	some convenient time in the future.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The screen will eventually be refreshed.
 *
 *--------------------------------------------------------------
 */

void
Tk_CanvasEventuallyRedraw(canvas, x1, y1, x2, y2)
    Tk_Canvas canvas;		/* Information about widget. */
    int x1, y1;			/* Upper left corner of area to redraw.
				 * Pixels on edge are redrawn. */
    int x2, y2;			/* Lower right corner of area to redraw.
				 * Pixels on edge are not redrawn. */
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    if ((x1 == x2) || (y1 == y2)) {
	return;
    }
    if (canvasPtr->flags & REDRAW_PENDING) {
	if (x1 <= canvasPtr->redrawX1) {
	    canvasPtr->redrawX1 = x1;
	}
	if (y1 <= canvasPtr->redrawY1) {
	    canvasPtr->redrawY1 = y1;
	}
	if (x2 >= canvasPtr->redrawX2) {
	    canvasPtr->redrawX2 = x2;
	}
	if (y2 >= canvasPtr->redrawY2) {
	    canvasPtr->redrawY2 = y2;
	}
    } else {
	canvasPtr->redrawX1 = x1;
	canvasPtr->redrawY1 = y1;
	canvasPtr->redrawX2 = x2;
	canvasPtr->redrawY2 = y2;
	Tcl_DoWhenIdle(DisplayCanvas, (ClientData) canvasPtr);
	canvasPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CreateItemType --
 *
 *	This procedure may be invoked to add a new kind of canvas
 *	element to the core item types supported by Tk.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	From now on, the new item type will be useable in canvas
 *	widgets (e.g. typePtr->name can be used as the item type
 *	in "create" widget commands).  If there was already a
 *	type with the same name as in typePtr, it is replaced with
 *	the new type.
 *
 *--------------------------------------------------------------
 */

void
Tk_CreateItemType(typePtr)
    Tk_ItemType *typePtr;		/* Information about item type;
					 * storage must be statically
					 * allocated (must live forever). */
{
    Tk_ItemType *typePtr2, *prevPtr;

    if (typeList == NULL) {
	InitCanvas();
    }

    /*
     * If there's already an item type with the given name, remove it.
     */

    for (typePtr2 = typeList, prevPtr = NULL; typePtr2 != NULL;
	    prevPtr = typePtr2, typePtr2 = typePtr2->nextPtr) {
	if (strcmp(typePtr2->name, typePtr->name) == 0) {
	    if (prevPtr == NULL) {
		typeList = typePtr2->nextPtr;
	    } else {
		prevPtr->nextPtr = typePtr2->nextPtr;
	    }
	    break;
	}
    }
    typePtr->nextPtr = typeList;
    typeList = typePtr;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetItemTypes --
 *
 *	This procedure returns a pointer to the list of all item
 *	types.
 *
 * Results:
 *	The return value is a pointer to the first in the list
 *	of item types currently supported by canvases.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_ItemType *
Tk_GetItemTypes()
{
    if (typeList == NULL) {
	InitCanvas();
    }
    return typeList;
}

/*
 *--------------------------------------------------------------
 *
 * InitCanvas --
 *
 *	This procedure is invoked to perform once-only-ever
 *	initialization for the module, such as setting up
 *	the type table.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
InitCanvas()
{
    if (typeList != NULL) {
	return;
    }
    typeList = &tkRectangleType;
    tkRectangleType.nextPtr = &tkTextType;
    tkTextType.nextPtr = &tkLineType;
    tkLineType.nextPtr = &tkPolygonType;
    tkPolygonType.nextPtr = &tkImageType;
    tkImageType.nextPtr = &tkOvalType;
    tkOvalType.nextPtr = &tkBitmapType;
    tkBitmapType.nextPtr = &tkArcType;
    tkArcType.nextPtr = &tkWindowType;
    tkWindowType.nextPtr = NULL;
    allUid = Tk_GetUid("all");
    currentUid = Tk_GetUid("current");
}

/*
 *--------------------------------------------------------------
 *
 * StartTagSearch --
 *
 *	This procedure is called to initiate an enumeration of
 *	all items in a given canvas that contain a given tag.
 *
 * Results:
 *	The return value is a pointer to the first item in
 *	canvasPtr that matches tag, or NULL if there is no
 *	such item.  The information at *searchPtr is initialized
 *	such that successive calls to NextItem will return
 *	successive items that match tag.
 *
 * Side effects:
 *	SearchPtr is linked into a list of searches in progress
 *	on canvasPtr, so that elements can safely be deleted
 *	while the search is in progress.  EndTagSearch must be
 *	called at the end of the search to unlink searchPtr from
 *	this list.
 *
 *--------------------------------------------------------------
 */

static Tk_Item *
StartTagSearch(canvasPtr, tag, searchPtr)
    TkCanvas *canvasPtr;		/* Canvas whose items are to be
					 * searched. */
    char *tag;				/* String giving tag value. */
    TagSearch *searchPtr;		/* Record describing tag search;
					 * will be initialized here. */
{
    int id;
    Tk_Item *itemPtr, *prevPtr;
    Tk_Uid *tagPtr;
    Tk_Uid uid;
    int count;

    /*
     * Initialize the search.
     */

    searchPtr->canvasPtr = canvasPtr;
    searchPtr->searchOver = 0;

    /*
     * Find the first matching item in one of several ways. If the tag
     * is a number then it selects the single item with the matching
     * identifier.  In this case see if the item being requested is the
     * hot item, in which case the search can be skipped.
     */

    if (isdigit(UCHAR(*tag))) {
	char *end;

	numIdSearches++;
	id = strtoul(tag, &end, 0);
	if (*end == 0) {
	    itemPtr = canvasPtr->hotPtr;
	    prevPtr = canvasPtr->hotPrevPtr;
	    if ((itemPtr == NULL) || (itemPtr->id != id) || (prevPtr == NULL)
		    || (prevPtr->nextPtr != itemPtr)) {
		numSlowSearches++;
		for (prevPtr = NULL, itemPtr = canvasPtr->firstItemPtr;
			itemPtr != NULL;
			prevPtr = itemPtr, itemPtr = itemPtr->nextPtr) {
		    if (itemPtr->id == id) {
			break;
		    }
		}
	    }
	    searchPtr->prevPtr = prevPtr;
	    searchPtr->searchOver = 1;
	    canvasPtr->hotPtr = itemPtr;
	    canvasPtr->hotPrevPtr = prevPtr;
	    return itemPtr;
	}
    }

    searchPtr->tag = uid = Tk_GetUid(tag);
    if (uid == allUid) {

	/*
	 * All items match.
	 */

	searchPtr->tag = NULL;
	searchPtr->prevPtr = NULL;
	searchPtr->currentPtr = canvasPtr->firstItemPtr;
	return canvasPtr->firstItemPtr;
    }

    /*
     * None of the above.  Search for an item with a matching tag.
     */

    for (prevPtr = NULL, itemPtr = canvasPtr->firstItemPtr; itemPtr != NULL;
	    prevPtr = itemPtr, itemPtr = itemPtr->nextPtr) {
	for (tagPtr = itemPtr->tagPtr, count = itemPtr->numTags;
		count > 0; tagPtr++, count--) {
	    if (*tagPtr == uid) {
		searchPtr->prevPtr = prevPtr;
		searchPtr->currentPtr = itemPtr;
		return itemPtr;
	    }
	}
    }
    searchPtr->prevPtr = prevPtr;
    searchPtr->searchOver = 1;
    return NULL;
}

/*
 *--------------------------------------------------------------
 *
 * NextItem --
 *
 *	This procedure returns successive items that match a given
 *	tag;  it should be called only after StartTagSearch has been
 *	used to begin a search.
 *
 * Results:
 *	The return value is a pointer to the next item that matches
 *	the tag specified to StartTagSearch, or NULL if no such
 *	item exists.  *SearchPtr is updated so that the next call
 *	to this procedure will return the next item.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static Tk_Item *
NextItem(searchPtr)
    TagSearch *searchPtr;		/* Record describing search in
					 * progress. */
{
    Tk_Item *itemPtr, *prevPtr;
    int count;
    Tk_Uid uid;
    Tk_Uid *tagPtr;

    /*
     * Find next item in list (this may not actually be a suitable
     * one to return), and return if there are no items left.
     */

    prevPtr = searchPtr->prevPtr;
    if (prevPtr == NULL) {
	itemPtr = searchPtr->canvasPtr->firstItemPtr;
    } else {
	itemPtr = prevPtr->nextPtr;
    }
    if ((itemPtr == NULL) || (searchPtr->searchOver)) {
	searchPtr->searchOver = 1;
	return NULL;
    }
    if (itemPtr != searchPtr->currentPtr) {
	/*
	 * The structure of the list has changed.  Probably the
	 * previously-returned item was removed from the list.
	 * In this case, don't advance prevPtr;  just return
	 * its new successor (i.e. do nothing here).
	 */
    } else {
	prevPtr = itemPtr;
	itemPtr = prevPtr->nextPtr;
    }

    /*
     * Handle special case of "all" search by returning next item.
     */

    uid = searchPtr->tag;
    if (uid == NULL) {
	searchPtr->prevPtr = prevPtr;
	searchPtr->currentPtr = itemPtr;
	return itemPtr;
    }

    /*
     * Look for an item with a particular tag.
     */

    for ( ; itemPtr != NULL; prevPtr = itemPtr, itemPtr = itemPtr->nextPtr) {
	for (tagPtr = itemPtr->tagPtr, count = itemPtr->numTags;
		count > 0; tagPtr++, count--) {
	    if (*tagPtr == uid) {
		searchPtr->prevPtr = prevPtr;
		searchPtr->currentPtr = itemPtr;
		return itemPtr;
	    }
	}
    }
    searchPtr->prevPtr = prevPtr;
    searchPtr->searchOver = 1;
    return NULL;
}

/*
 *--------------------------------------------------------------
 *
 * DoItem --
 *
 *	This is a utility procedure called by FindItems.  It
 *	either adds itemPtr's id to the result forming in interp,
 *	or it adds a new tag to itemPtr, depending on the value
 *	of tag.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If tag is NULL then itemPtr's id is added as a list element
 *	to interp->result;  otherwise tag is added to itemPtr's
 *	list of tags.
 *
 *--------------------------------------------------------------
 */

static void
DoItem(interp, itemPtr, tag)
    Tcl_Interp *interp;			/* Interpreter in which to (possibly)
					 * record item id. */
    Tk_Item *itemPtr;			/* Item to (possibly) modify. */
    Tk_Uid tag;				/* Tag to add to those already
					 * present for item, or NULL. */
{
    Tk_Uid *tagPtr;
    int count;

    /*
     * Handle the "add-to-result" case and return, if appropriate.
     */

    if (tag == NULL) {
	char msg[30];
	sprintf(msg, "%d", itemPtr->id);
	Tcl_AppendElement(interp, msg);
	return;
    }

    for (tagPtr = itemPtr->tagPtr, count = itemPtr->numTags;
	    count > 0; tagPtr++, count--) {
	if (tag == *tagPtr) {
	    return;
	}
    }

    /*
     * Grow the tag space if there's no more room left in the current
     * block.
     */

    if (itemPtr->tagSpace == itemPtr->numTags) {
	Tk_Uid *newTagPtr;

	itemPtr->tagSpace += 5;
	newTagPtr = (Tk_Uid *) ckalloc((unsigned)
		(itemPtr->tagSpace * sizeof(Tk_Uid)));
	memcpy((VOID *) newTagPtr, (VOID *) itemPtr->tagPtr,
		(itemPtr->numTags * sizeof(Tk_Uid)));
	if (itemPtr->tagPtr != itemPtr->staticTagSpace) {
	    ckfree((char *) itemPtr->tagPtr);
	}
	itemPtr->tagPtr = newTagPtr;
	tagPtr = &itemPtr->tagPtr[itemPtr->numTags];
    }

    /*
     * Add in the new tag.
     */

    *tagPtr = tag;
    itemPtr->numTags++;
}

/*
 *--------------------------------------------------------------
 *
 * FindItems --
 *
 *	This procedure does all the work of implementing the
 *	"find" and "addtag" options of the canvas widget command,
 *	which locate items that have certain features (location,
 *	tags, position in display list, etc.).
 *
 * Results:
 *	A standard Tcl return value.  If newTag is NULL, then a
 *	list of ids from all the items that match argc/argv is
 *	returned in interp->result.  If newTag is NULL, then
 *	the normal interp->result is an empty string.  If an error
 *	occurs, then interp->result will hold an error message.
 *
 * Side effects:
 *	If newTag is non-NULL, then all the items that match the
 *	information in argc/argv have that tag added to their
 *	lists of tags.
 *
 *--------------------------------------------------------------
 */

static int
FindItems(interp, canvasPtr, argc, argv, newTag, cmdName, option)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    TkCanvas *canvasPtr;		/* Canvas whose items are to be
					 * searched. */
    int argc;				/* Number of entries in argv.  Must be
					 * greater than zero. */
    char **argv;			/* Arguments that describe what items
					 * to search for (see user doc on
					 * "find" and "addtag" options). */
    char *newTag;			/* If non-NULL, gives new tag to set
					 * on all found items;  if NULL, then
					 * ids of found items are returned
					 * in interp->result. */
    char *cmdName;			/* Name of original Tcl command, for
					 * use in error messages. */
    char *option;			/* For error messages:  gives option
					 * from Tcl command and other stuff
					 * up to what's in argc/argv. */
{
    int c;
    size_t length;
    TagSearch search;
    Tk_Item *itemPtr;
    Tk_Uid uid;

    if (newTag != NULL) {
	uid = Tk_GetUid(newTag);
    } else {
	uid = NULL;
    }
    c = argv[0][0];
    length = strlen(argv[0]);
    if ((c == 'a') && (strncmp(argv[0], "above", length) == 0)
	    && (length >= 2)) {
	Tk_Item *lastPtr = NULL;
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    cmdName, option, " above tagOrId", (char *) NULL);
	    return TCL_ERROR;
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[1], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    lastPtr = itemPtr;
	}
	if ((lastPtr != NULL) && (lastPtr->nextPtr != NULL)) {
	    DoItem(interp, lastPtr->nextPtr, uid);
	}
    } else if ((c == 'a') && (strncmp(argv[0], "all", length) == 0)
	    && (length >= 2)) {
	if (argc != 1) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    cmdName, option, " all", (char *) NULL);
	    return TCL_ERROR;
	}

	for (itemPtr = canvasPtr->firstItemPtr; itemPtr != NULL;
		itemPtr = itemPtr->nextPtr) {
	    DoItem(interp, itemPtr, uid);
	}
    } else if ((c == 'b') && (strncmp(argv[0], "below", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    cmdName, option, " below tagOrId", (char *) NULL);
	    return TCL_ERROR;
	}
	itemPtr = StartTagSearch(canvasPtr, argv[1], &search);
	if (search.prevPtr != NULL) {
	    DoItem(interp, search.prevPtr, uid);
	}
    } else if ((c == 'c') && (strncmp(argv[0], "closest", length) == 0)) {
	double closestDist;
	Tk_Item *startPtr, *closestPtr;
	double coords[2], halo;
	int x1, y1, x2, y2;

	if ((argc < 3) || (argc > 5)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    cmdName, option, " closest x y ?halo? ?start?",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if ((Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr, argv[1],
		&coords[0]) != TCL_OK) || (Tk_CanvasGetCoord(interp,
		(Tk_Canvas) canvasPtr, argv[2], &coords[1]) != TCL_OK)) {
	    return TCL_ERROR;
	}
	if (argc > 3) {
	    if (Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr, argv[3],
		    &halo) != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (halo < 0.0) {
		Tcl_AppendResult(interp, "can't have negative halo value \"",
			argv[3], "\"", (char *) NULL);
		return TCL_ERROR;
	    }
	} else {
	    halo = 0.0;
	}

	/*
	 * Find the item at which to start the search.
	 */

	startPtr = canvasPtr->firstItemPtr;
	if (argc == 5) {
	    itemPtr = StartTagSearch(canvasPtr, argv[4], &search);
	    if (itemPtr != NULL) {
		startPtr = itemPtr;
	    }
	}

	/*
	 * The code below is optimized so that it can eliminate most
	 * items without having to call their item-specific procedures.
	 * This is done by keeping a bounding box (x1, y1, x2, y2) that
	 * an item's bbox must overlap if the item is to have any
	 * chance of being closer than the closest so far.
	 */

	itemPtr = startPtr;
	if (itemPtr == NULL) {
	    return TCL_OK;
	}
	closestDist = (*itemPtr->typePtr->pointProc)((Tk_Canvas) canvasPtr,
		itemPtr, coords) - halo;
	if (closestDist < 0.0) {
	    closestDist = 0.0;
	}
	while (1) {
	    double newDist;

	    /*
	     * Update the bounding box using itemPtr, which is the
	     * new closest item.
	     */

	    x1 = (coords[0] - closestDist - halo - 1);
	    y1 = (coords[1] - closestDist - halo - 1);
	    x2 = (coords[0] + closestDist + halo + 1);
	    y2 = (coords[1] + closestDist + halo + 1);
	    closestPtr = itemPtr;

	    /*
	     * Search for an item that beats the current closest one.
	     * Work circularly through the canvas's item list until
	     * getting back to the starting item.
	     */

	    while (1) {
		itemPtr = itemPtr->nextPtr;
		if (itemPtr == NULL) {
		    itemPtr = canvasPtr->firstItemPtr;
		}
		if (itemPtr == startPtr) {
		    DoItem(interp, closestPtr, uid);
		    return TCL_OK;
		}
		if ((itemPtr->x1 >= x2) || (itemPtr->x2 <= x1)
			|| (itemPtr->y1 >= y2) || (itemPtr->y2 <= y1)) {
		    continue;
		}
		newDist = (*itemPtr->typePtr->pointProc)((Tk_Canvas) canvasPtr,
			itemPtr, coords) - halo;
		if (newDist < 0.0) {
		    newDist = 0.0;
		}
		if (newDist <= closestDist) {
		    closestDist = newDist;
		    break;
		}
	    }
	}
    } else if ((c == 'e') && (strncmp(argv[0], "enclosed", length) == 0)) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    cmdName, option, " enclosed x1 y1 x2 y2", (char *) NULL);
	    return TCL_ERROR;
	}
	return FindArea(interp, canvasPtr, argv+1, uid, 1);
    } else if ((c == 'o') && (strncmp(argv[0], "overlapping", length) == 0)) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    cmdName, option, " overlapping x1 y1 x2 y2",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	return FindArea(interp, canvasPtr, argv+1, uid, 0);
    } else if ((c == 'w') && (strncmp(argv[0], "withtag", length) == 0)) {
		if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    cmdName, option, " withtag tagOrId", (char *) NULL);
	    return TCL_ERROR;
	}
	for (itemPtr = StartTagSearch(canvasPtr, argv[1], &search);
		itemPtr != NULL; itemPtr = NextItem(&search)) {
	    DoItem(interp, itemPtr, uid);
	}
    } else  {
	Tcl_AppendResult(interp, "bad search command \"", argv[0],
		"\": must be above, all, below, closest, enclosed, ",
		"overlapping, or withtag", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * FindArea --
 *
 *	This procedure implements area searches for the "find"
 *	and "addtag" options.
 *
 * Results:
 *	A standard Tcl return value.  If newTag is NULL, then a
 *	list of ids from all the items overlapping or enclosed
 *	by the rectangle given by argc is returned in interp->result.
 *	If newTag is NULL, then the normal interp->result is an
 *	empty string.  If an error occurs, then interp->result will
 *	hold an error message.
 *
 * Side effects:
 *	If uid is non-NULL, then all the items overlapping
 *	or enclosed by the area in argv have that tag added to
 *	their lists of tags.
 *
 *--------------------------------------------------------------
 */

static int
FindArea(interp, canvasPtr, argv, uid, enclosed)
    Tcl_Interp *interp;			/* Interpreter for error reporting
					 * and result storing. */
    TkCanvas *canvasPtr;		/* Canvas whose items are to be
					 * searched. */
    char **argv;			/* Array of four arguments that
					 * give the coordinates of the
					 * rectangular area to search. */
    Tk_Uid uid;				/* If non-NULL, gives new tag to set
					 * on all found items;  if NULL, then
					 * ids of found items are returned
					 * in interp->result. */
    int enclosed;			/* 0 means overlapping or enclosed
					 * items are OK, 1 means only enclosed
					 * items are OK. */
{
    double rect[4], tmp;
    int x1, y1, x2, y2;
    Tk_Item *itemPtr;

    if ((Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr, argv[0],
		&rect[0]) != TCL_OK)
	    || (Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr, argv[1],
		&rect[1]) != TCL_OK)
	    || (Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr, argv[2],
		&rect[2]) != TCL_OK)
	    || (Tk_CanvasGetCoord(interp, (Tk_Canvas) canvasPtr, argv[3],
		&rect[3]) != TCL_OK)) {
	return TCL_ERROR;
    }
    if (rect[0] > rect[2]) {
	tmp = rect[0]; rect[0] = rect[2]; rect[2] = tmp;
    }
    if (rect[1] > rect[3]) {
	tmp = rect[1]; rect[1] = rect[3]; rect[3] = tmp;
    }

    /*
     * Use an integer bounding box for a quick test, to avoid
     * calling item-specific code except for items that are close.
     */

    x1 = (rect[0]-1.0);
    y1 = (rect[1]-1.0);
    x2 = (rect[2]+1.0);
    y2 = (rect[3]+1.0);
    for (itemPtr = canvasPtr->firstItemPtr; itemPtr != NULL;
	    itemPtr = itemPtr->nextPtr) {
	if ((itemPtr->x1 >= x2) || (itemPtr->x2 <= x1)
		|| (itemPtr->y1 >= y2) || (itemPtr->y2 <= y1)) {
	    continue;
	}
	if ((*itemPtr->typePtr->areaProc)((Tk_Canvas) canvasPtr, itemPtr, rect)
		>= enclosed) {
	    DoItem(interp, itemPtr, uid);
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * RelinkItems --
 *
 *	Move one or more items to a different place in the
 *	display order for a canvas.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The items identified by "tag" are moved so that they
 *	are all together in the display list and immediately
 *	after prevPtr.  The order of the moved items relative
 *	to each other is not changed.
 *
 *--------------------------------------------------------------
 */

static void
RelinkItems(canvasPtr, tag, prevPtr)
    TkCanvas *canvasPtr;	/* Canvas to be modified. */
    char *tag;			/* Tag identifying items to be moved
				 * in the redisplay list. */
    Tk_Item *prevPtr;		/* Reposition the items so that they
				 * go just after this item (NULL means
				 * put at beginning of list). */
{
    Tk_Item *itemPtr;
    TagSearch search;
    Tk_Item *firstMovePtr, *lastMovePtr;

    /*
     * Find all of the items to be moved and remove them from
     * the list, making an auxiliary list running from firstMovePtr
     * to lastMovePtr.  Record their areas for redisplay.
     */

    firstMovePtr = lastMovePtr = NULL;
    for (itemPtr = StartTagSearch(canvasPtr, tag, &search);
	    itemPtr != NULL; itemPtr = NextItem(&search)) {
	if (itemPtr == prevPtr) {
	    /*
	     * Item after which insertion is to occur is being
	     * moved!  Switch to insert after its predecessor.
	     */

	    prevPtr = search.prevPtr;
	}
	if (search.prevPtr == NULL) {
	    canvasPtr->firstItemPtr = itemPtr->nextPtr;
	} else {
	    search.prevPtr->nextPtr = itemPtr->nextPtr;
	}
	if (canvasPtr->lastItemPtr == itemPtr) {
	    canvasPtr->lastItemPtr = search.prevPtr;
	}
	if (firstMovePtr == NULL) {
	    firstMovePtr = itemPtr;
	} else {
	    lastMovePtr->nextPtr = itemPtr;
	}
	lastMovePtr = itemPtr;
	Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr, itemPtr->x1, itemPtr->y1,
		itemPtr->x2, itemPtr->y2);
	canvasPtr->flags |= REPICK_NEEDED;
    }

    /*
     * Insert the list of to-be-moved items back into the canvas's
     * at the desired position.
     */

    if (firstMovePtr == NULL) {
	return;
    }
    if (prevPtr == NULL) {
	lastMovePtr->nextPtr = canvasPtr->firstItemPtr;
	canvasPtr->firstItemPtr = firstMovePtr;
    } else {
	lastMovePtr->nextPtr = prevPtr->nextPtr;
	prevPtr->nextPtr = firstMovePtr;
    }
    if (canvasPtr->lastItemPtr == prevPtr) {
	canvasPtr->lastItemPtr = lastMovePtr;
    }
}

/*
 *--------------------------------------------------------------
 *
 * CanvasBindProc --
 *
 *	This procedure is invoked by the Tk dispatcher to handle
 *	events associated with bindings on items.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the command invoked as part of the binding
 *	(if there was any).
 *
 *--------------------------------------------------------------
 */

static void
CanvasBindProc(clientData, eventPtr)
    ClientData clientData;		/* Pointer to canvas structure. */
    XEvent *eventPtr;			/* Pointer to X event that just
					 * happened. */
{
    TkCanvas *canvasPtr = (TkCanvas *) clientData;

    Tcl_Preserve((ClientData) canvasPtr);

    /*
     * This code below keeps track of the current modifier state in
     * canvasPtr>state.  This information is used to defer repicks of
     * the current item while buttons are down.
     */

    if ((eventPtr->type == ButtonPress) || (eventPtr->type == ButtonRelease)) {
	int mask;

	switch (eventPtr->xbutton.button) {
	    case Button1:
		mask = Button1Mask;
		break;
	    case Button2:
		mask = Button2Mask;
		break;
	    case Button3:
		mask = Button3Mask;
		break;
	    case Button4:
		mask = Button4Mask;
		break;
	    case Button5:
		mask = Button5Mask;
		break;
	    default:
		mask = 0;
		break;
	}

	/*
	 * For button press events, repick the current item using the
	 * button state before the event, then process the event.  For
	 * button release events, first process the event, then repick
	 * the current item using the button state *after* the event
	 * (the button has logically gone up before we change the
	 * current item).
	 */

	if (eventPtr->type == ButtonPress) {
	    /*
	     * On a button press, first repick the current item using
	     * the button state before the event, the process the event.
	     */

	    canvasPtr->state = eventPtr->xbutton.state;
	    PickCurrentItem(canvasPtr, eventPtr);
	    canvasPtr->state ^= mask;
	    CanvasDoEvent(canvasPtr, eventPtr);
	} else {
	    /*
	     * Button release: first process the event, with the button
	     * still considered to be down.  Then repick the current
	     * item under the assumption that the button is no longer down.
	     */

	    canvasPtr->state = eventPtr->xbutton.state;
	    CanvasDoEvent(canvasPtr, eventPtr);
	    eventPtr->xbutton.state ^= mask;
	    canvasPtr->state = eventPtr->xbutton.state;
	    PickCurrentItem(canvasPtr, eventPtr);
	    eventPtr->xbutton.state ^= mask;
	}
	goto done;
    } else if ((eventPtr->type == EnterNotify)
	    || (eventPtr->type == LeaveNotify)) {
	canvasPtr->state = eventPtr->xcrossing.state;
	PickCurrentItem(canvasPtr, eventPtr);
	goto done;
    } else if (eventPtr->type == MotionNotify) {
	canvasPtr->state = eventPtr->xmotion.state;
	PickCurrentItem(canvasPtr, eventPtr);
    }
    CanvasDoEvent(canvasPtr, eventPtr);

    done:
    Tcl_Release((ClientData) canvasPtr);
}

/*
 *--------------------------------------------------------------
 *
 * PickCurrentItem --
 *
 *	Find the topmost item in a canvas that contains a given
 *	location and mark the the current item.  If the current
 *	item has changed, generate a fake exit event on the old
 *	current item and a fake enter event on the new current
 *	item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The current item for canvasPtr may change.  If it does,
 *	then the commands associated with item entry and exit
 *	could do just about anything.  A binding script could
 *	delete the canvas, so callers should protect themselves
 *	with Tcl_Preserve and Tcl_Release.
 *
 *--------------------------------------------------------------
 */

static void
PickCurrentItem(canvasPtr, eventPtr)
    TkCanvas *canvasPtr;		/* Canvas widget in which to select
					 * current item. */
    XEvent *eventPtr;			/* Event describing location of
					 * mouse cursor.  Must be EnterWindow,
					 * LeaveWindow, ButtonRelease, or
					 * MotionNotify. */
{
    double coords[2];
    int buttonDown;

    /*
     * Check whether or not a button is down.  If so, we'll log entry
     * and exit into and out of the current item, but not entry into
     * any other item.  This implements a form of grabbing equivalent
     * to what the X server does for windows.
     */

    buttonDown = canvasPtr->state
	    & (Button1Mask|Button2Mask|Button3Mask|Button4Mask|Button5Mask);
    if (!buttonDown) {
	canvasPtr->flags &= ~LEFT_GRABBED_ITEM;
    }

    /*
     * Save information about this event in the canvas.  The event in
     * the canvas is used for two purposes:
     *
     * 1. Event bindings: if the current item changes, fake events are
     *    generated to allow item-enter and item-leave bindings to trigger.
     * 2. Reselection: if the current item gets deleted, can use the
     *    saved event to find a new current item.
     * Translate MotionNotify events into EnterNotify events, since that's
     * what gets reported to item handlers.
     */

    if (eventPtr != &canvasPtr->pickEvent) {
	if ((eventPtr->type == MotionNotify)
		|| (eventPtr->type == ButtonRelease)) {
	    canvasPtr->pickEvent.xcrossing.type = EnterNotify;
	    canvasPtr->pickEvent.xcrossing.serial = eventPtr->xmotion.serial;
	    canvasPtr->pickEvent.xcrossing.send_event
		    = eventPtr->xmotion.send_event;
	    canvasPtr->pickEvent.xcrossing.display = eventPtr->xmotion.display;
	    canvasPtr->pickEvent.xcrossing.window = eventPtr->xmotion.window;
	    canvasPtr->pickEvent.xcrossing.root = eventPtr->xmotion.root;
	    canvasPtr->pickEvent.xcrossing.subwindow = None;
	    canvasPtr->pickEvent.xcrossing.time = eventPtr->xmotion.time;
	    canvasPtr->pickEvent.xcrossing.x = eventPtr->xmotion.x;
	    canvasPtr->pickEvent.xcrossing.y = eventPtr->xmotion.y;
	    canvasPtr->pickEvent.xcrossing.x_root = eventPtr->xmotion.x_root;
	    canvasPtr->pickEvent.xcrossing.y_root = eventPtr->xmotion.y_root;
	    canvasPtr->pickEvent.xcrossing.mode = NotifyNormal;
	    canvasPtr->pickEvent.xcrossing.detail = NotifyNonlinear;
	    canvasPtr->pickEvent.xcrossing.same_screen
		    = eventPtr->xmotion.same_screen;
	    canvasPtr->pickEvent.xcrossing.focus = False;
	    canvasPtr->pickEvent.xcrossing.state = eventPtr->xmotion.state;
	} else  {
	    canvasPtr->pickEvent = *eventPtr;
	}
    }

    /*
     * If this is a recursive call (there's already a partially completed
     * call pending on the stack;  it's in the middle of processing a
     * Leave event handler for the old current item) then just return;
     * the pending call will do everything that's needed.
     */

    if (canvasPtr->flags & REPICK_IN_PROGRESS) {
	return;
    }

    /*
     * A LeaveNotify event automatically means that there's no current
     * object, so the check for closest item can be skipped.
     */

    coords[0] = canvasPtr->pickEvent.xcrossing.x + canvasPtr->xOrigin;
    coords[1] = canvasPtr->pickEvent.xcrossing.y + canvasPtr->yOrigin;
    if (canvasPtr->pickEvent.type != LeaveNotify) {
	canvasPtr->newCurrentPtr = CanvasFindClosest(canvasPtr, coords);
    } else {
	canvasPtr->newCurrentPtr = NULL;
    }

    if ((canvasPtr->newCurrentPtr == canvasPtr->currentItemPtr)
	    && !(canvasPtr->flags & LEFT_GRABBED_ITEM)) {
	/*
	 * Nothing to do:  the current item hasn't changed.
	 */

	return;
    }

    /*
     * Simulate a LeaveNotify event on the previous current item and
     * an EnterNotify event on the new current item.  Remove the "current"
     * tag from the previous current item and place it on the new current
     * item.
     */

    if ((canvasPtr->newCurrentPtr != canvasPtr->currentItemPtr)
	    && (canvasPtr->currentItemPtr != NULL)
	    && !(canvasPtr->flags & LEFT_GRABBED_ITEM)) {
	XEvent event;
	Tk_Item *itemPtr = canvasPtr->currentItemPtr;
	int i;

	event = canvasPtr->pickEvent;
	event.type = LeaveNotify;

	/*
	 * If the event's detail happens to be NotifyInferior the
	 * binding mechanism will discard the event.  To be consistent,
	 * always use NotifyAncestor.
	 */

	event.xcrossing.detail = NotifyAncestor;
	canvasPtr->flags |= REPICK_IN_PROGRESS;
	CanvasDoEvent(canvasPtr, &event);
	canvasPtr->flags &= ~REPICK_IN_PROGRESS;

	/*
	 * The check below is needed because there could be an event
	 * handler for <LeaveNotify> that deletes the current item.
	 */

	if ((itemPtr == canvasPtr->currentItemPtr) && !buttonDown) {
	    for (i = itemPtr->numTags-1; i >= 0; i--) {
		if (itemPtr->tagPtr[i] == currentUid) {
		    itemPtr->tagPtr[i] = itemPtr->tagPtr[itemPtr->numTags-1];
		    itemPtr->numTags--;
		    break;
		}
	    }
	}
    
	/*
	 * Note:  during CanvasDoEvent above, it's possible that
	 * canvasPtr->newCurrentPtr got reset to NULL because the
	 * item was deleted.
	 */
    }
    if ((canvasPtr->newCurrentPtr != canvasPtr->currentItemPtr) && buttonDown) {
	canvasPtr->flags |= LEFT_GRABBED_ITEM;
	return;
    }

    /*
     * Special note:  it's possible that canvasPtr->newCurrentPtr ==
     * canvasPtr->currentItemPtr here.  This can happen, for example,
     * if LEFT_GRABBED_ITEM was set.
     */

    canvasPtr->flags &= ~LEFT_GRABBED_ITEM;
    canvasPtr->currentItemPtr = canvasPtr->newCurrentPtr;
    if (canvasPtr->currentItemPtr != NULL) {
	XEvent event;

	DoItem((Tcl_Interp *) NULL, canvasPtr->currentItemPtr, currentUid);
	event = canvasPtr->pickEvent;
	event.type = EnterNotify;
	event.xcrossing.detail = NotifyAncestor;
	CanvasDoEvent(canvasPtr, &event);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CanvasFindClosest --
 *
 *	Given x and y coordinates, find the topmost canvas item that
 *	is "close" to the coordinates.
 *
 * Results:
 *	The return value is a pointer to the topmost item that is
 *	close to (x,y), or NULL if no item is close.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tk_Item *
CanvasFindClosest(canvasPtr, coords)
    TkCanvas *canvasPtr;		/* Canvas widget to search. */
    double coords[2];			/* Desired x,y position in canvas,
					 * not screen, coordinates.) */
{
    Tk_Item *itemPtr;
    Tk_Item *bestPtr;
    int x1, y1, x2, y2;

    x1 = coords[0] - canvasPtr->closeEnough;
    y1 = coords[1] - canvasPtr->closeEnough;
    x2 = coords[0] + canvasPtr->closeEnough;
    y2 = coords[1] + canvasPtr->closeEnough;

    bestPtr = NULL;
    for (itemPtr = canvasPtr->firstItemPtr; itemPtr != NULL;
	    itemPtr = itemPtr->nextPtr) {
	if ((itemPtr->x1 > x2) || (itemPtr->x2 < x1)
		|| (itemPtr->y1 > y2) || (itemPtr->y2 < y1)) {
	    continue;
	}
	if ((*itemPtr->typePtr->pointProc)((Tk_Canvas) canvasPtr,
		itemPtr, coords) <= canvasPtr->closeEnough) {
	    bestPtr = itemPtr;
	}
    }
    return bestPtr;
}

/*
 *--------------------------------------------------------------
 *
 * CanvasDoEvent --
 *
 *	This procedure is called to invoke binding processing
 *	for a new event that is associated with the current item
 *	for a canvas.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on the bindings for the canvas.  A binding script
 *	could delete the canvas, so callers should protect themselves
 *	with Tcl_Preserve and Tcl_Release.
 *
 *--------------------------------------------------------------
 */

static void
CanvasDoEvent(canvasPtr, eventPtr)
    TkCanvas *canvasPtr;		/* Canvas widget in which event
					 * occurred. */
    XEvent *eventPtr;			/* Real or simulated X event that
					 * is to be processed. */
{
#define NUM_STATIC 3
    ClientData staticObjects[NUM_STATIC];
    ClientData *objectPtr;
    int numObjects, i;
    Tk_Item *itemPtr;

    if (canvasPtr->bindingTable == NULL) {
	return;
    }

    itemPtr = canvasPtr->currentItemPtr;
    if ((eventPtr->type == KeyPress) || (eventPtr->type == KeyRelease)) {
	itemPtr = canvasPtr->textInfo.focusItemPtr;
    }
    if (itemPtr == NULL) {
	return;
    }

    /*
     * Set up an array with all the relevant objects for processing
     * this event.  The relevant objects are (a) the event's item,
     * (b) the tags associated with the event's item, and (c) the
     * tag "all".  If there are a lot of tags then malloc an array
     * to hold all of the objects.
     */

    numObjects = itemPtr->numTags + 2;
    if (numObjects <= NUM_STATIC) {
	objectPtr = staticObjects;
    } else {
	objectPtr = (ClientData *) ckalloc((unsigned)
		(numObjects * sizeof(ClientData)));
    }
    objectPtr[0] = (ClientData) allUid;
    for (i = itemPtr->numTags-1; i >= 0; i--) {
	objectPtr[i+1] = (ClientData) itemPtr->tagPtr[i];
    }
    objectPtr[itemPtr->numTags+1] = (ClientData) itemPtr;

    /*
     * Invoke the binding system, then free up the object array if
     * it was malloc-ed.
     */

    if (canvasPtr->tkwin != NULL) {
	Tk_BindEvent(canvasPtr->bindingTable, eventPtr, canvasPtr->tkwin,
		numObjects, objectPtr);
    }
    if (objectPtr != staticObjects) {
	ckfree((char *) objectPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CanvasBlinkProc --
 *
 *	This procedure is called as a timer handler to blink the
 *	insertion cursor off and on.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor gets turned on or off, redisplay gets invoked,
 *	and this procedure reschedules itself.
 *
 *----------------------------------------------------------------------
 */

static void
CanvasBlinkProc(clientData)
    ClientData clientData;	/* Pointer to record describing entry. */
{
    TkCanvas *canvasPtr = (TkCanvas *) clientData;

    if (!canvasPtr->textInfo.gotFocus || (canvasPtr->insertOffTime == 0)) {
	return;
    }
    if (canvasPtr->textInfo.cursorOn) {
	canvasPtr->textInfo.cursorOn = 0;
	canvasPtr->insertBlinkHandler = Tcl_CreateTimerHandler(
		canvasPtr->insertOffTime, CanvasBlinkProc,
		(ClientData) canvasPtr);
    } else {
	canvasPtr->textInfo.cursorOn = 1;
	canvasPtr->insertBlinkHandler = Tcl_CreateTimerHandler(
		canvasPtr->insertOnTime, CanvasBlinkProc,
		(ClientData) canvasPtr);
    }
    if (canvasPtr->textInfo.focusItemPtr != NULL) {
	Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		canvasPtr->textInfo.focusItemPtr->x1,
		canvasPtr->textInfo.focusItemPtr->y1,
		canvasPtr->textInfo.focusItemPtr->x2,
		canvasPtr->textInfo.focusItemPtr->y2);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CanvasFocusProc --
 *
 *	This procedure is called whenever a canvas gets or loses the
 *	input focus.  It's also called whenever the window is
 *	reconfigured while it has the focus.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor gets turned on or off.
 *
 *----------------------------------------------------------------------
 */

static void
CanvasFocusProc(canvasPtr, gotFocus)
    TkCanvas *canvasPtr;	/* Canvas that just got or lost focus. */
    int gotFocus;		/* 1 means window is getting focus, 0 means
				 * it's losing it. */
{
    Tcl_DeleteTimerHandler(canvasPtr->insertBlinkHandler);
    if (gotFocus) {
	canvasPtr->textInfo.gotFocus = 1;
	canvasPtr->textInfo.cursorOn = 1;
	if (canvasPtr->insertOffTime != 0) {
	    canvasPtr->insertBlinkHandler = Tcl_CreateTimerHandler(
		    canvasPtr->insertOffTime, CanvasBlinkProc,
		    (ClientData) canvasPtr);
	}
    } else {
	canvasPtr->textInfo.gotFocus = 0;
	canvasPtr->textInfo.cursorOn = 0;
	canvasPtr->insertBlinkHandler = (Tcl_TimerToken) NULL;
    }
    if (canvasPtr->textInfo.focusItemPtr != NULL) {
	Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		canvasPtr->textInfo.focusItemPtr->x1,
		canvasPtr->textInfo.focusItemPtr->y1,
		canvasPtr->textInfo.focusItemPtr->x2,
		canvasPtr->textInfo.focusItemPtr->y2);
    }
    if (canvasPtr->highlightWidth > 0) {
	canvasPtr->flags |= REDRAW_BORDERS;
	if (!(canvasPtr->flags & REDRAW_PENDING)) {
	    Tcl_DoWhenIdle(DisplayCanvas, (ClientData) canvasPtr);
	    canvasPtr->flags |= REDRAW_PENDING;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * CanvasSelectTo --
 *
 *	Modify the selection by moving its un-anchored end.  This could
 *	make the selection either larger or smaller.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The selection changes.
 *
 *----------------------------------------------------------------------
 */

static void
CanvasSelectTo(canvasPtr, itemPtr, index)
    TkCanvas *canvasPtr;	/* Information about widget. */
    Tk_Item *itemPtr;		/* Item that is to hold selection. */
    int index;			/* Index of element that is to become the
				 * "other" end of the selection. */
{
    int oldFirst, oldLast;
    Tk_Item *oldSelPtr;

    oldFirst = canvasPtr->textInfo.selectFirst;
    oldLast = canvasPtr->textInfo.selectLast;
    oldSelPtr = canvasPtr->textInfo.selItemPtr;

    /*
     * Grab the selection if we don't own it already.
     */

    if (canvasPtr->textInfo.selItemPtr == NULL) {
	Tk_OwnSelection(canvasPtr->tkwin, XA_PRIMARY, CanvasLostSelection,
		(ClientData) canvasPtr);
    } else if (canvasPtr->textInfo.selItemPtr != itemPtr) {
	Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		canvasPtr->textInfo.selItemPtr->x1,
		canvasPtr->textInfo.selItemPtr->y1,
		canvasPtr->textInfo.selItemPtr->x2,
		canvasPtr->textInfo.selItemPtr->y2);
    }
    canvasPtr->textInfo.selItemPtr = itemPtr;

    if (canvasPtr->textInfo.anchorItemPtr != itemPtr) {
	canvasPtr->textInfo.anchorItemPtr = itemPtr;
	canvasPtr->textInfo.selectAnchor = index;
    }
    if (canvasPtr->textInfo.selectAnchor <= index) {
	canvasPtr->textInfo.selectFirst = canvasPtr->textInfo.selectAnchor;
	canvasPtr->textInfo.selectLast = index;
    } else {
	canvasPtr->textInfo.selectFirst = index;
	canvasPtr->textInfo.selectLast = canvasPtr->textInfo.selectAnchor - 1;
    }
    if ((canvasPtr->textInfo.selectFirst != oldFirst)
	    || (canvasPtr->textInfo.selectLast != oldLast)
	    || (itemPtr != oldSelPtr)) {
	Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		itemPtr->x1, itemPtr->y1, itemPtr->x2, itemPtr->y2);
    }
}

/*
 *--------------------------------------------------------------
 *
 * CanvasFetchSelection --
 *
 *	This procedure is invoked by Tk to return part or all of
 *	the selection, when the selection is in a canvas widget.
 *	This procedure always returns the selection as a STRING.
 *
 * Results:
 *	The return value is the number of non-NULL bytes stored
 *	at buffer.  Buffer is filled (or partially filled) with a
 *	NULL-terminated string containing part or all of the selection,
 *	as given by offset and maxBytes.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
CanvasFetchSelection(clientData, offset, buffer, maxBytes)
    ClientData clientData;		/* Information about canvas widget. */
    int offset;				/* Offset within selection of first
					 * character to be returned. */
    char *buffer;			/* Location in which to place
					 * selection. */
    int maxBytes;			/* Maximum number of bytes to place
					 * at buffer, not including terminating
					 * NULL character. */
{
    TkCanvas *canvasPtr = (TkCanvas *) clientData;

    if (canvasPtr->textInfo.selItemPtr == NULL) {
	return -1;
    }
    if (canvasPtr->textInfo.selItemPtr->typePtr->selectionProc == NULL) {
	return -1;
    }
    return (*canvasPtr->textInfo.selItemPtr->typePtr->selectionProc)(
	    (Tk_Canvas) canvasPtr, canvasPtr->textInfo.selItemPtr, offset,
	    buffer, maxBytes);
}

/*
 *----------------------------------------------------------------------
 *
 * CanvasLostSelection --
 *
 *	This procedure is called back by Tk when the selection is
 *	grabbed away from a canvas widget.
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
CanvasLostSelection(clientData)
    ClientData clientData;		/* Information about entry widget. */
{
    TkCanvas *canvasPtr = (TkCanvas *) clientData;

    if (canvasPtr->textInfo.selItemPtr != NULL) {
	Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
		canvasPtr->textInfo.selItemPtr->x1,
		canvasPtr->textInfo.selItemPtr->y1,
		canvasPtr->textInfo.selItemPtr->x2,
		canvasPtr->textInfo.selItemPtr->y2);
    }
    canvasPtr->textInfo.selItemPtr = NULL;
}

/*
 *--------------------------------------------------------------
 *
 * GridAlign --
 *
 *	Given a coordinate and a grid spacing, this procedure
 *	computes the location of the nearest grid line to the
 *	coordinate.
 *
 * Results:
 *	The return value is the location of the grid line nearest
 *	to coord.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static double
GridAlign(coord, spacing)
    double coord;		/* Coordinate to grid-align. */
    double spacing;		/* Spacing between grid lines.   If <= 0
				 * then no alignment is done. */
{
    if (spacing <= 0.0) {
	return coord;
    }
    if (coord < 0) {
	return -((int) ((-coord)/spacing + 0.5)) * spacing;
    }
    return ((int) (coord/spacing + 0.5)) * spacing;
}

/*
 *----------------------------------------------------------------------
 *
 * PrintScrollFractions --
 *
 *	Given the range that's visible in the window and the "100%
 *	range" for what's in the canvas, print a string containing
 *	the scroll fractions.  This procedure is used for both x
 *	and y scrolling.
 *
 * Results:
 *	The memory pointed to by string is modified to hold
 *	two real numbers containing the scroll fractions (between
 *	0 and 1) corresponding to the other arguments.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
PrintScrollFractions(screen1, screen2, object1, object2, string)
    int screen1;		/* Lowest coordinate visible in the window. */
    int screen2;		/* Highest coordinate visible in the window. */
    int object1;		/* Lowest coordinate in the object. */
    int object2;		/* Highest coordinate in the object. */
    char *string;		/* Two real numbers get printed here.  Must
				 * have enough storage for two %g
				 * conversions. */
{
    double range, f1, f2;

    range = object2 - object1;
    if (range <= 0) {
	f1 = 0;
	f2 = 1.0;
    } else {
	f1 = (screen1 - object1)/range;
	if (f1 < 0) {
	    f1 = 0.0;
	}
	f2 = (screen2 - object1)/range;
	if (f2 > 1.0) {
	    f2 = 1.0;
	}
	if (f2 < f1) {
	    f2 = f1;
	}
    }
    sprintf(string, "%g %g", f1, f2);
}

/*
 *--------------------------------------------------------------
 *
 * CanvasUpdateScrollbars --
 *
 *	This procedure is invoked whenever a canvas has changed in
 *	a way that requires scrollbars to be redisplayed (e.g. the
 *	view in the canvas has changed).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there are scrollbars associated with the canvas, then
 *	their scrolling commands are invoked to cause them to
 *	redisplay.  If errors occur, additional Tcl commands may
 *	be invoked to process the errors.
 *
 *--------------------------------------------------------------
 */

static void
CanvasUpdateScrollbars(canvasPtr)
    TkCanvas *canvasPtr;		/* Information about canvas. */
{
    int result;
    char buffer[200];
    Tcl_Interp *interp;
    int xOrigin, yOrigin, inset, width, height, scrollX1, scrollX2,
        scrollY1, scrollY2;
    char *xScrollCmd, *yScrollCmd;

    /*
     * Save all the relevant values from the canvasPtr, because it might be
     * deleted as part of either of the two calls to Tcl_VarEval below.
     */
    
    interp = canvasPtr->interp;
    Tcl_Preserve((ClientData) interp);
    xScrollCmd = canvasPtr->xScrollCmd;
    if (xScrollCmd != (char *) NULL) {
        Tcl_Preserve((ClientData) xScrollCmd);
    }
    yScrollCmd = canvasPtr->yScrollCmd;
    if (yScrollCmd != (char *) NULL) {
        Tcl_Preserve((ClientData) yScrollCmd);
    }
    xOrigin = canvasPtr->xOrigin;
    yOrigin = canvasPtr->yOrigin;
    inset = canvasPtr->inset;
    width = Tk_Width(canvasPtr->tkwin);
    height = Tk_Height(canvasPtr->tkwin);
    scrollX1 = canvasPtr->scrollX1;
    scrollX2 = canvasPtr->scrollX2;
    scrollY1 = canvasPtr->scrollY1;
    scrollY2 = canvasPtr->scrollY2;
    canvasPtr->flags &= ~UPDATE_SCROLLBARS;
    if (canvasPtr->xScrollCmd != NULL) {
	PrintScrollFractions(xOrigin + inset, xOrigin + width - inset,
                scrollX1, scrollX2, buffer);
	result = Tcl_VarEval(interp, xScrollCmd, " ", buffer, (char *) NULL);
	if (result != TCL_OK) {
	    Tcl_BackgroundError(interp);
	}
	Tcl_ResetResult(interp);
        Tcl_Release((ClientData) xScrollCmd);
    }

    if (yScrollCmd != NULL) {
	PrintScrollFractions(yOrigin + inset, yOrigin + height - inset,
                scrollY1, scrollY2, buffer);
	result = Tcl_VarEval(interp, yScrollCmd, " ", buffer, (char *) NULL);
	if (result != TCL_OK) {
	    Tcl_BackgroundError(interp);
	}
	Tcl_ResetResult(interp);
        Tcl_Release((ClientData) yScrollCmd);
    }
    Tcl_Release((ClientData) interp);
}

/*
 *--------------------------------------------------------------
 *
 * CanvasSetOrigin --
 *
 *	This procedure is invoked to change the mapping between
 *	canvas coordinates and screen coordinates in the canvas
 *	window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The canvas will be redisplayed to reflect the change in
 *	view.  In addition, scrollbars will be updated if there
 *	are any.
 *
 *--------------------------------------------------------------
 */

static void
CanvasSetOrigin(canvasPtr, xOrigin, yOrigin)
    TkCanvas *canvasPtr;	/* Information about canvas. */
    int xOrigin;		/* New X origin for canvas (canvas x-coord
				 * corresponding to left edge of canvas
				 * window). */
    int yOrigin;		/* New Y origin for canvas (canvas y-coord
				 * corresponding to top edge of canvas
				 * window). */
{
    int left, right, top, bottom, delta;

    /*
     * If scroll increments have been set, round the window origin
     * to the nearest multiple of the increments.  Remember, the
     * origin is the place just inside the borders,  not the upper
     * left corner.
     */

    if (canvasPtr->xScrollIncrement > 0) {
	if (xOrigin >= 0) {
	    xOrigin += canvasPtr->xScrollIncrement/2;
	    xOrigin -= (xOrigin + canvasPtr->inset)
		    % canvasPtr->xScrollIncrement;
	} else {
	    xOrigin = (-xOrigin) + canvasPtr->xScrollIncrement/2;
	    xOrigin = -(xOrigin - (xOrigin - canvasPtr->inset)
		    % canvasPtr->xScrollIncrement);
	}
    }
    if (canvasPtr->yScrollIncrement > 0) {
	if (yOrigin >= 0) {
	    yOrigin += canvasPtr->yScrollIncrement/2;
	    yOrigin -= (yOrigin + canvasPtr->inset)
		    % canvasPtr->yScrollIncrement;
	} else {
	    yOrigin = (-yOrigin) + canvasPtr->yScrollIncrement/2;
	    yOrigin = -(yOrigin - (yOrigin - canvasPtr->inset)
		    % canvasPtr->yScrollIncrement);
	}
    }

    /*
     * Adjust the origin if necessary to keep as much as possible of the
     * canvas in the view.  The variables left, right, etc. keep track of
     * how much extra space there is on each side of the view before it
     * will stick out past the scroll region.  If one side sticks out past
     * the edge of the scroll region, adjust the view to bring that side
     * back to the edge of the scrollregion (but don't move it so much that
     * the other side sticks out now).  If scroll increments are in effect,
     * be sure to adjust only by full increments.
     */

    if ((canvasPtr->confine) && (canvasPtr->regionString != NULL)) {
	left = xOrigin + canvasPtr->inset - canvasPtr->scrollX1;
	right = canvasPtr->scrollX2
		- (xOrigin + Tk_Width(canvasPtr->tkwin) - canvasPtr->inset);
	top = yOrigin + canvasPtr->inset - canvasPtr->scrollY1;
	bottom = canvasPtr->scrollY2
		- (yOrigin + Tk_Height(canvasPtr->tkwin) - canvasPtr->inset);
	if ((left < 0) && (right > 0)) {
	    delta = (right > -left) ? -left : right;
	    if (canvasPtr->xScrollIncrement > 0) {
		delta -= delta % canvasPtr->xScrollIncrement;
	    }
	    xOrigin += delta;
	} else if ((right < 0) && (left > 0)) {
	    delta = (left > -right) ? -right : left;
	    if (canvasPtr->xScrollIncrement > 0) {
		delta -= delta % canvasPtr->xScrollIncrement;
	    }
	    xOrigin -= delta;
	}
	if ((top < 0) && (bottom > 0)) {
	    delta = (bottom > -top) ? -top : bottom;
	    if (canvasPtr->yScrollIncrement > 0) {
		delta -= delta % canvasPtr->yScrollIncrement;
	    }
	    yOrigin += delta;
	} else if ((bottom < 0) && (top > 0)) {
	    delta = (top > -bottom) ? -bottom : top;
	    if (canvasPtr->yScrollIncrement > 0) {
		delta -= delta % canvasPtr->yScrollIncrement;
	    }
	    yOrigin -= delta;
	}
    }

    if ((xOrigin == canvasPtr->xOrigin) && (yOrigin == canvasPtr->yOrigin)) {
	return;
    }

    /*
     * Tricky point: must redisplay not only everything that's visible
     * in the window's final configuration, but also everything that was
     * visible in the initial configuration.  This is needed because some
     * item types, like windows, need to know when they move off-screen
     * so they can explicitly undisplay themselves.
     */

    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
	    canvasPtr->xOrigin, canvasPtr->yOrigin,
	    canvasPtr->xOrigin + Tk_Width(canvasPtr->tkwin),
	    canvasPtr->yOrigin + Tk_Height(canvasPtr->tkwin));
    canvasPtr->xOrigin = xOrigin;
    canvasPtr->yOrigin = yOrigin;
    canvasPtr->flags |= UPDATE_SCROLLBARS;
    Tk_CanvasEventuallyRedraw((Tk_Canvas) canvasPtr,
	    canvasPtr->xOrigin, canvasPtr->yOrigin,
	    canvasPtr->xOrigin + Tk_Width(canvasPtr->tkwin),
	    canvasPtr->yOrigin + Tk_Height(canvasPtr->tkwin));
}

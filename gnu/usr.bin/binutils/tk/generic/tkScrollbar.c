/* 
 * tkScrollbar.c --
 *
 *	This module implements a scrollbar widgets for the Tk
 *	toolkit.  A scrollbar displays a slider and two arrows;
 *	mouse clicks on features within the scrollbar cause
 *	scrolling commands to be invoked.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkScrollbar.c 1.79 96/02/15 18:52:40
 */

#include "tkPort.h"
#include "default.h"
#include "tkInt.h"

/*
 * A data structure of the following type is kept for each scrollbar
 * widget managed by this file:
 */

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the scrollbar.  NULL
				 * means that the window has been destroyed
				 * but the data structures haven't yet been
				 * cleaned up.*/
    Display *display;		/* Display containing widget.  Used, among
				 * other things, so that resources can be
				 * freed even after tkwin has gone away. */
    Tcl_Interp *interp;		/* Interpreter associated with scrollbar. */
    Tcl_Command widgetCmd;	/* Token for scrollbar's widget command. */
    Tk_Uid orientUid;		/* Orientation for window ("vertical" or
				 * "horizontal"). */
    int vertical;		/* Non-zero means vertical orientation
				 * requested, zero means horizontal. */
    int width;			/* Desired narrow dimension of scrollbar,
				 * in pixels. */
    char *command;		/* Command prefix to use when invoking
				 * scrolling commands.  NULL means don't
				 * invoke commands.  Malloc'ed. */
    int commandSize;		/* Number of non-NULL bytes in command. */
    int repeatDelay;		/* How long to wait before auto-repeating
				 * on scrolling actions (in ms). */
    int repeatInterval;		/* Interval between autorepeats (in ms). */
    int jump;			/* Value of -jump option. */

    /*
     * Information used when displaying widget:
     */

    int borderWidth;		/* Width of 3-D borders. */
    Tk_3DBorder bgBorder;	/* Used for drawing background (all flat
				 * surfaces except for trough). */
    Tk_3DBorder activeBorder;	/* For drawing backgrounds when active (i.e.
				 * when mouse is positioned over element). */
    XColor *troughColorPtr;	/* Color for drawing trough. */
    GC troughGC;		/* For drawing trough. */
    GC copyGC;			/* Used for copying from pixmap onto screen. */
    int relief;			/* Indicates whether window as a whole is
				 * raised, sunken, or flat. */
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
    int elementBorderWidth;	/* Width of border to draw around elements
				 * inside scrollbar (arrows and slider).
				 * -1 means use borderWidth. */
    int arrowLength;		/* Length of arrows along long dimension of
				 * scrollbar, including space for a small gap
				 * between the arrow and the slider.
				 * Recomputed on window size changes. */
    int sliderFirst;		/* Pixel coordinate of top or left edge
				 * of slider area, including border. */
    int sliderLast;		/* Coordinate of pixel just after bottom
				 * or right edge of slider area, including
				 * border. */
    int activeField;		/* Names field to be displayed in active
				 * colors, such as TOP_ARROW, or 0 for
				 * no field. */
    int activeRelief;		/* Value of -activeRelief option: relief
				 * to use for active element. */

    /*
     * Information describing the application related to the scrollbar.
     * This information is provided by the application by invoking the
     * "set" widget command.  This information can now be provided in
     * two ways:  the "old" form (totalUnits, windowUnits, firstUnit,
     * and lastUnit), or the "new" form (firstFraction and lastFraction).
     * FirstFraction and lastFraction will always be valid, but
     * the old-style information is only valid if the NEW_STYLE_COMMANDS
     * flag is 0.
     */

    int totalUnits;		/* Total dimension of application, in
				 * units.  Valid only if the NEW_STYLE_COMMANDS
				 * flag isn't set. */
    int windowUnits;		/* Maximum number of units that can be
				 * displayed in the window at once.  Valid
				 * only if the NEW_STYLE_COMMANDS flag isn't
				 * set. */
    int firstUnit;		/* Number of last unit visible in
				 * application's window.  Valid only if the
				 * NEW_STYLE_COMMANDS flag isn't set. */
    int lastUnit;		/* Index of last unit visible in window.
				 * Valid only if the NEW_STYLE_COMMANDS
				 * flag isn't set. */
    double firstFraction;	/* Position of first visible thing in window,
				 * specified as a fraction between 0 and
				 * 1.0. */
    double lastFraction;	/* Position of last visible thing in window,
				 * specified as a fraction between 0 and
				 * 1.0. */

    /*
     * Miscellaneous information:
     */

    Tk_Cursor cursor;		/* Current cursor for window, or None. */
    char *takeFocus;		/* Value of -takefocus option;  not used in
				 * the C code, but used by keyboard traversal
				 * scripts.  Malloc'ed, but may be NULL. */
    int flags;			/* Various flags;  see below for
				 * definitions. */
} Scrollbar;

/*
 * Legal values for "activeField" field of Scrollbar structures.  These
 * are also the return values from the ScrollbarPosition procedure.
 */

#define OUTSIDE		0
#define TOP_ARROW	1
#define TOP_GAP		2
#define SLIDER		3
#define BOTTOM_GAP	4
#define BOTTOM_ARROW	5

/*
 * Flag bits for scrollbars:
 * 
 * REDRAW_PENDING:		Non-zero means a DoWhenIdle handler
 *				has already been queued to redraw
 *				this window.
 * NEW_STYLE_COMMANDS:		Non-zero means the new style of commands
 *				should be used to communicate with the
 *				widget:  ".t yview scroll 2 lines", instead
 *				of ".t yview 40", for example.
 * GOT_FOCUS:			Non-zero means this window has the input
 *				focus.
 */

#define REDRAW_PENDING		1
#define NEW_STYLE_COMMANDS	2
#define GOT_FOCUS		4

/*
 * Minimum slider length, in pixels (designed to make sure that the slider
 * is always easy to grab with the mouse).
 */

#define MIN_SLIDER_LENGTH	5

/*
 * Information used for argv parsing.
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_SCROLLBAR_ACTIVE_BG_COLOR, Tk_Offset(Scrollbar, activeBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-activebackground", "activeBackground", "Foreground",
	DEF_SCROLLBAR_ACTIVE_BG_MONO, Tk_Offset(Scrollbar, activeBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_RELIEF, "-activerelief", "activeRelief", "Relief",
	DEF_SCROLLBAR_ACTIVE_RELIEF, Tk_Offset(Scrollbar, activeRelief), 0},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_SCROLLBAR_BG_COLOR, Tk_Offset(Scrollbar, bgBorder),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	DEF_SCROLLBAR_BG_MONO, Tk_Offset(Scrollbar, bgBorder),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	DEF_SCROLLBAR_BORDER_WIDTH, Tk_Offset(Scrollbar, borderWidth), 0},
    {TK_CONFIG_STRING, "-command", "command", "Command",
	DEF_SCROLLBAR_COMMAND, Tk_Offset(Scrollbar, command),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_ACTIVE_CURSOR, "-cursor", "cursor", "Cursor",
	DEF_SCROLLBAR_CURSOR, Tk_Offset(Scrollbar, cursor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_PIXELS, "-elementborderwidth", "elementBorderWidth",
	"BorderWidth", DEF_SCROLLBAR_EL_BORDER_WIDTH,
	Tk_Offset(Scrollbar, elementBorderWidth), 0},
    {TK_CONFIG_COLOR, "-highlightbackground", "highlightBackground",
	"HighlightBackground", DEF_SCROLLBAR_HIGHLIGHT_BG,
	Tk_Offset(Scrollbar, highlightBgColorPtr), 0},
    {TK_CONFIG_COLOR, "-highlightcolor", "highlightColor", "HighlightColor",
	DEF_SCROLLBAR_HIGHLIGHT,
	Tk_Offset(Scrollbar, highlightColorPtr), 0},
    {TK_CONFIG_PIXELS, "-highlightthickness", "highlightThickness",
	"HighlightThickness",
	DEF_SCROLLBAR_HIGHLIGHT_WIDTH, Tk_Offset(Scrollbar, highlightWidth), 0},
    {TK_CONFIG_BOOLEAN, "-jump", "jump", "Jump",
	DEF_SCROLLBAR_JUMP, Tk_Offset(Scrollbar, jump), 0},
    {TK_CONFIG_UID, "-orient", "orient", "Orient",
	DEF_SCROLLBAR_ORIENT, Tk_Offset(Scrollbar, orientUid), 0},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	DEF_SCROLLBAR_RELIEF, Tk_Offset(Scrollbar, relief), 0},
    {TK_CONFIG_INT, "-repeatdelay", "repeatDelay", "RepeatDelay",
	DEF_SCROLLBAR_REPEAT_DELAY, Tk_Offset(Scrollbar, repeatDelay), 0},
    {TK_CONFIG_INT, "-repeatinterval", "repeatInterval", "RepeatInterval",
	DEF_SCROLLBAR_REPEAT_INTERVAL, Tk_Offset(Scrollbar, repeatInterval), 0},
    {TK_CONFIG_STRING, "-takefocus", "takeFocus", "TakeFocus",
	DEF_SCROLLBAR_TAKE_FOCUS, Tk_Offset(Scrollbar, takeFocus),
	TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-troughcolor", "troughColor", "Background",
	DEF_SCROLLBAR_TROUGH_COLOR, Tk_Offset(Scrollbar, troughColorPtr),
	TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_COLOR, "-troughcolor", "troughColor", "Background",
	DEF_SCROLLBAR_TROUGH_MONO, Tk_Offset(Scrollbar, troughColorPtr),
	TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_PIXELS, "-width", "width", "Width",
	DEF_SCROLLBAR_WIDTH, Tk_Offset(Scrollbar, width), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ComputeScrollbarGeometry _ANSI_ARGS_((
			    Scrollbar *scrollPtr));
static int		ConfigureScrollbar _ANSI_ARGS_((Tcl_Interp *interp,
			    Scrollbar *scrollPtr, int argc, char **argv,
			    int flags));
static void		DestroyScrollbar _ANSI_ARGS_((char *memPtr));
static void		DisplayScrollbar _ANSI_ARGS_((ClientData clientData));
static void		EventuallyRedraw _ANSI_ARGS_((Scrollbar *scrollPtr));
static void		ScrollbarCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static void		ScrollbarEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static int		ScrollbarPosition _ANSI_ARGS_((Scrollbar *scrollPtr,
			    int x, int y));
static int		ScrollbarWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *, int argc, char **argv));

/*
 *--------------------------------------------------------------
 *
 * Tk_ScrollbarCmd --
 *
 *	This procedure is invoked to process the "scrollbar" Tcl
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
Tk_ScrollbarCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    register Scrollbar *scrollPtr;
    Tk_Window new;

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
     * Initialize fields that won't be initialized by ConfigureScrollbar,
     * or which ConfigureScrollbar expects to have reasonable values
     * (e.g. resource pointers).
     */

    scrollPtr = (Scrollbar *) ckalloc(sizeof(Scrollbar));
    scrollPtr->tkwin = new;
    scrollPtr->display = Tk_Display(new);
    scrollPtr->interp = interp;
    scrollPtr->widgetCmd = Tcl_CreateCommand(interp,
	    Tk_PathName(scrollPtr->tkwin), ScrollbarWidgetCmd,
	    (ClientData) scrollPtr, ScrollbarCmdDeletedProc);
    scrollPtr->orientUid = NULL;
    scrollPtr->vertical = 0;
    scrollPtr->width = 0;
    scrollPtr->command = NULL;
    scrollPtr->commandSize = 0;
    scrollPtr->repeatDelay = 0;
    scrollPtr->repeatInterval = 0;
    scrollPtr->borderWidth = 0;
    scrollPtr->bgBorder = NULL;
    scrollPtr->activeBorder = NULL;
    scrollPtr->troughColorPtr = NULL;
    scrollPtr->troughGC = None;
    scrollPtr->copyGC = None;
    scrollPtr->relief = TK_RELIEF_FLAT;
    scrollPtr->highlightWidth = 0;
    scrollPtr->highlightBgColorPtr = NULL;
    scrollPtr->highlightColorPtr = NULL;
    scrollPtr->inset = 0;
    scrollPtr->elementBorderWidth = -1;
    scrollPtr->arrowLength = 0;
    scrollPtr->sliderFirst = 0;
    scrollPtr->sliderLast = 0;
    scrollPtr->activeField = 0;
    scrollPtr->activeRelief = TK_RELIEF_RAISED;
    scrollPtr->totalUnits = 0;
    scrollPtr->windowUnits = 0;
    scrollPtr->firstUnit = 0;
    scrollPtr->lastUnit = 0;
    scrollPtr->firstFraction = 0.0;
    scrollPtr->lastFraction = 0.0;
    scrollPtr->cursor = None;
    scrollPtr->takeFocus = NULL;
    scrollPtr->flags = 0;

    Tk_SetClass(scrollPtr->tkwin, "Scrollbar");
    Tk_CreateEventHandler(scrollPtr->tkwin,
	    ExposureMask|StructureNotifyMask|FocusChangeMask,
	    ScrollbarEventProc, (ClientData) scrollPtr);
    if (ConfigureScrollbar(interp, scrollPtr, argc-2, argv+2, 0) != TCL_OK) {
	goto error;
    }

    interp->result = Tk_PathName(scrollPtr->tkwin);
    return TCL_OK;

    error:
    Tk_DestroyWindow(scrollPtr->tkwin);
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * ScrollbarWidgetCmd --
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
ScrollbarWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Information about scrollbar
					 * widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    register Scrollbar *scrollPtr = (Scrollbar *) clientData;
    int result = TCL_OK;
    size_t length;
    int c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) scrollPtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "activate", length) == 0)) {
	if (argc == 2) {
	    switch (scrollPtr->activeField) {
		case TOP_ARROW:		interp->result = "arrow1";	break;
		case SLIDER:		interp->result = "slider";	break;
		case BOTTOM_ARROW:	interp->result = "arrow2";	break;
	    }
	    goto done;
	}
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " activate element\"", (char *) NULL);
	    goto error;
	}
	c = argv[2][0];
	length = strlen(argv[2]);
	if ((c == 'a') && (strcmp(argv[2], "arrow1") == 0)) {
	    scrollPtr->activeField = TOP_ARROW;
	} else if ((c == 'a') && (strcmp(argv[2], "arrow2") == 0)) {
	    scrollPtr->activeField = BOTTOM_ARROW;
	} else if ((c == 's') && (strncmp(argv[2], "slider", length) == 0)) {
	    scrollPtr->activeField = SLIDER;
	} else {
	    scrollPtr->activeField = OUTSIDE;
	}
	EventuallyRedraw(scrollPtr);
    } else if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Tk_ConfigureValue(interp, scrollPtr->tkwin, configSpecs,
		(char *) scrollPtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, scrollPtr->tkwin, configSpecs,
		    (char *) scrollPtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, scrollPtr->tkwin, configSpecs,
		    (char *) scrollPtr, argv[2], 0);
	} else {
	    result = ConfigureScrollbar(interp, scrollPtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "delta", length) == 0)) {
	int xDelta, yDelta, pixels, length;
	double fraction;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " delta xDelta yDelta\"", (char *) NULL);
	    goto error;
	}
	if ((Tcl_GetInt(interp, argv[2], &xDelta) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &yDelta) != TCL_OK)) {
	    goto error;
	}
	if (scrollPtr->vertical) {
	    pixels = yDelta;
	    length = Tk_Height(scrollPtr->tkwin) - 1
		    - 2*(scrollPtr->arrowLength + scrollPtr->inset);
	} else {
	    pixels = xDelta;
	    length = Tk_Width(scrollPtr->tkwin) - 1
		    - 2*(scrollPtr->arrowLength + scrollPtr->inset);
	}
	if (length == 0) {
	    fraction = 0.0;
	} else {
	    fraction = ((double) pixels / (double) length);
	}
	sprintf(interp->result, "%g", fraction);
    } else if ((c == 'f') && (strncmp(argv[1], "fraction", length) == 0)) {
	int x, y, pos, length;
	double fraction;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " fraction x y\"", (char *) NULL);
	    goto error;
	}
	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
	    goto error;
	}
	if (scrollPtr->vertical) {
	    pos = y - (scrollPtr->arrowLength + scrollPtr->inset);
	    length = Tk_Height(scrollPtr->tkwin) - 1
		    - 2*(scrollPtr->arrowLength + scrollPtr->inset);
	} else {
	    pos = x - (scrollPtr->arrowLength + scrollPtr->inset);
	    length = Tk_Width(scrollPtr->tkwin) - 1
		    - 2*(scrollPtr->arrowLength + scrollPtr->inset);
	}
	if (length == 0) {
	    fraction = 0.0;
	} else {
	    fraction = ((double) pos / (double) length);
	}
	if (fraction < 0) {
	    fraction = 0;
	} else if (fraction > 1.0) {
	    fraction = 1.0;
	}
	sprintf(interp->result, "%g", fraction);
    } else if ((c == 'g') && (strncmp(argv[1], "get", length) == 0)) {
	if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " get\"", (char *) NULL);
	    goto error;
	}
	if (scrollPtr->flags & NEW_STYLE_COMMANDS) {
	    char first[TCL_DOUBLE_SPACE], last[TCL_DOUBLE_SPACE];

	    Tcl_PrintDouble(interp, scrollPtr->firstFraction, first);
	    Tcl_PrintDouble(interp, scrollPtr->lastFraction, last);
	    Tcl_AppendResult(interp, first, " ", last, (char *) NULL);
	} else {
	    sprintf(interp->result, "%d %d %d %d", scrollPtr->totalUnits,
		    scrollPtr->windowUnits, scrollPtr->firstUnit,
		    scrollPtr->lastUnit);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "identify", length) == 0)) {
	int x, y, thing;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " identify x y\"", (char *) NULL);
	    goto error;
	}
	if ((Tcl_GetInt(interp, argv[2], &x) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[3], &y) != TCL_OK)) {
	    goto error;
	}
	thing = ScrollbarPosition(scrollPtr, x,y);
	switch (thing) {
	    case TOP_ARROW:	interp->result = "arrow1";	break;
	    case TOP_GAP:	interp->result = "trough1";	break;
	    case SLIDER:	interp->result = "slider";	break;
	    case BOTTOM_GAP:	interp->result = "trough2";	break;
	    case BOTTOM_ARROW:	interp->result = "arrow2";	break;
	}
    } else if ((c == 's') && (strncmp(argv[1], "set", length) == 0)) {
	int totalUnits, windowUnits, firstUnit, lastUnit;

	if (argc == 4) {
	    double first, last;

	    if (Tcl_GetDouble(interp, argv[2], &first) != TCL_OK) {
		goto error;
	    }
	    if (Tcl_GetDouble(interp, argv[3], &last) != TCL_OK) {
		goto error;
	    }
	    if (first < 0) {
		scrollPtr->firstFraction = 0;
	    } else if (first > 1.0) {
		scrollPtr->firstFraction = 1.0;
	    } else {
		scrollPtr->firstFraction = first;
	    }
	    if (last < scrollPtr->firstFraction) {
		scrollPtr->lastFraction = scrollPtr->firstFraction;
	    } else if (last > 1.0) {
		scrollPtr->lastFraction = 1.0;
	    } else {
		scrollPtr->lastFraction = last;
	    }
	    scrollPtr->flags |= NEW_STYLE_COMMANDS;
	} else if (argc == 6) {
	    if (Tcl_GetInt(interp, argv[2], &totalUnits) != TCL_OK) {
		goto error;
	    }
	    if (totalUnits < 0) {
		totalUnits = 0;
	    }
	    if (Tcl_GetInt(interp, argv[3], &windowUnits) != TCL_OK) {
		goto error;
	    }
	    if (windowUnits < 0) {
		windowUnits = 0;
	    }
	    if (Tcl_GetInt(interp, argv[4], &firstUnit) != TCL_OK) {
		goto error;
	    }
	    if (Tcl_GetInt(interp, argv[5], &lastUnit) != TCL_OK) {
		goto error;
	    }
	    if (totalUnits > 0) {
		if (lastUnit < firstUnit) {
		    lastUnit = firstUnit;
		}
	    } else {
		firstUnit = lastUnit = 0;
	    }
	    scrollPtr->totalUnits = totalUnits;
	    scrollPtr->windowUnits = windowUnits;
	    scrollPtr->firstUnit = firstUnit;
	    scrollPtr->lastUnit = lastUnit;
	    if (scrollPtr->totalUnits == 0) {
		scrollPtr->firstFraction = 0.0;
		scrollPtr->lastFraction = 1.0;
	    } else {
		scrollPtr->firstFraction = ((double) firstUnit)/totalUnits;
		scrollPtr->lastFraction = ((double) (lastUnit+1))/totalUnits;
	    }
	    scrollPtr->flags &= ~NEW_STYLE_COMMANDS;
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " set firstFraction lastFraction\" or \"",
		    argv[0],
		    " set totalUnits windowUnits firstUnit lastUnit\"",
		    (char *) NULL);
	    goto error;
	}
	ComputeScrollbarGeometry(scrollPtr);
	EventuallyRedraw(scrollPtr);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be activate, cget, configure, delta, fraction, ",
		"get, identify, or set", (char *) NULL);
	goto error;
    }
    done:
    Tcl_Release((ClientData) scrollPtr);
    return result;

    error:
    Tcl_Release((ClientData) scrollPtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyScrollbar --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a scrollbar at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the scrollbar is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyScrollbar(memPtr)
    char *memPtr;	/* Info about scrollbar widget. */
{
    register Scrollbar *scrollPtr = (Scrollbar *) memPtr;

    /*
     * Free up all the stuff that requires special handling, then
     * let Tk_FreeOptions handle all the standard option-related
     * stuff.
     */

    if (scrollPtr->troughGC != None) {
	Tk_FreeGC(scrollPtr->display, scrollPtr->troughGC);
    }
    if (scrollPtr->copyGC != None) {
	Tk_FreeGC(scrollPtr->display, scrollPtr->copyGC);
    }
    Tk_FreeOptions(configSpecs, (char *) scrollPtr, scrollPtr->display, 0);
    ckfree((char *) scrollPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureScrollbar --
 *
 *	This procedure is called to process an argv/argc list, plus
 *	the Tk option database, in order to configure (or
 *	reconfigure) a scrollbar widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for scrollPtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureScrollbar(interp, scrollPtr, argc, argv, flags)
    Tcl_Interp *interp;			/* Used for error reporting. */
    register Scrollbar *scrollPtr;	/* Information about widget;  may or
					 * may not already have values for
					 * some fields. */
    int argc;				/* Number of valid entries in argv. */
    char **argv;			/* Arguments. */
    int flags;				/* Flags to pass to
					 * Tk_ConfigureWidget. */
{
    size_t length;
    XGCValues gcValues;
    GC new;

    if (Tk_ConfigureWidget(interp, scrollPtr->tkwin, configSpecs,
	    argc, argv, (char *) scrollPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few options need special processing, such as parsing the
     * orientation or setting the background from a 3-D border.
     */

    length = strlen(scrollPtr->orientUid);
    if (strncmp(scrollPtr->orientUid, "vertical", length) == 0) {
	scrollPtr->vertical = 1;
    } else if (strncmp(scrollPtr->orientUid, "horizontal", length) == 0) {
	scrollPtr->vertical = 0;
    } else {
	Tcl_AppendResult(interp, "bad orientation \"", scrollPtr->orientUid,
		"\": must be vertical or horizontal", (char *) NULL);
	return TCL_ERROR;
    }

    if (scrollPtr->command != NULL) {
	scrollPtr->commandSize = strlen(scrollPtr->command);
    } else {
	scrollPtr->commandSize = 0;
    }

    Tk_SetBackgroundFromBorder(scrollPtr->tkwin, scrollPtr->bgBorder);

    gcValues.foreground = scrollPtr->troughColorPtr->pixel;
    new = Tk_GetGC(scrollPtr->tkwin, GCForeground, &gcValues);
    if (scrollPtr->troughGC != None) {
	Tk_FreeGC(scrollPtr->display, scrollPtr->troughGC);
    }
    scrollPtr->troughGC = new;
    if (scrollPtr->copyGC == None) {
	gcValues.graphics_exposures = False;
	scrollPtr->copyGC = Tk_GetGC(scrollPtr->tkwin, GCGraphicsExposures,
	    &gcValues);
    }

    /*
     * Register the desired geometry for the window (leave enough space
     * for the two arrows plus a minimum-size slider, plus border around
     * the whole window, if any).  Then arrange for the window to be
     * redisplayed.
     */

    ComputeScrollbarGeometry(scrollPtr);
    EventuallyRedraw(scrollPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayScrollbar --
 *
 *	This procedure redraws the contents of a scrollbar window.
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
DisplayScrollbar(clientData)
    ClientData clientData;	/* Information about window. */
{
    register Scrollbar *scrollPtr = (Scrollbar *) clientData;
    register Tk_Window tkwin = scrollPtr->tkwin;
    XPoint points[7];
    Tk_3DBorder border;
    int relief, width, elementBorderWidth;
    Pixmap pixmap;

    if ((scrollPtr->tkwin == NULL) || !Tk_IsMapped(tkwin)) {
	goto done;
    }

    if (scrollPtr->vertical) {
	width = Tk_Width(tkwin) - 2*scrollPtr->inset;
    } else {
	width = Tk_Height(tkwin) - 2*scrollPtr->inset;
    }
    elementBorderWidth = scrollPtr->elementBorderWidth;
    if (elementBorderWidth < 0) {
	elementBorderWidth = scrollPtr->borderWidth;
    }

    /*
     * In order to avoid screen flashes, this procedure redraws
     * the scrollbar in a pixmap, then copies the pixmap to the
     * screen in a single operation.  This means that there's no
     * point in time where the on-sreen image has been cleared.
     */

    pixmap = Tk_GetPixmap(scrollPtr->display, Tk_WindowId(tkwin),
	    Tk_Width(tkwin), Tk_Height(tkwin), Tk_Depth(tkwin));

    if (scrollPtr->highlightWidth != 0) {
	GC gc;

	if (scrollPtr->flags & GOT_FOCUS) {
	    gc = Tk_GCForColor(scrollPtr->highlightColorPtr, pixmap);
	} else {
	    gc = Tk_GCForColor(scrollPtr->highlightBgColorPtr, pixmap);
	}
	Tk_DrawFocusHighlight(tkwin, gc, scrollPtr->highlightWidth, pixmap);
    }
    Tk_Draw3DRectangle(tkwin, pixmap, scrollPtr->bgBorder,
	    scrollPtr->highlightWidth, scrollPtr->highlightWidth,
	    Tk_Width(tkwin) - 2*scrollPtr->highlightWidth,
	    Tk_Height(tkwin) - 2*scrollPtr->highlightWidth,
	    scrollPtr->borderWidth, scrollPtr->relief);
    XFillRectangle(scrollPtr->display, pixmap, scrollPtr->troughGC,
	    scrollPtr->inset, scrollPtr->inset,
	    (unsigned) (Tk_Width(tkwin) - 2*scrollPtr->inset),
	    (unsigned) (Tk_Height(tkwin) - 2*scrollPtr->inset));

    /*
     * Draw the top or left arrow.  The coordinates of the polygon
     * points probably seem odd, but they were carefully chosen with
     * respect to X's rules for filling polygons.  These point choices
     * cause the arrows to just fill the narrow dimension of the
     * scrollbar and be properly centered.
     */

    if (scrollPtr->activeField == TOP_ARROW) {
	border = scrollPtr->activeBorder;
	relief = scrollPtr->activeField == TOP_ARROW ? scrollPtr->activeRelief
		: TK_RELIEF_RAISED;
    } else {
	border = scrollPtr->bgBorder;
	relief = TK_RELIEF_RAISED;
    }
    if (scrollPtr->vertical) {
	points[0].x = scrollPtr->inset - 1;
	points[0].y = scrollPtr->arrowLength + scrollPtr->inset - 1;
	points[1].x = width + scrollPtr->inset;
	points[1].y = points[0].y;
	points[2].x = width/2 + scrollPtr->inset;
	points[2].y = scrollPtr->inset - 1;
	Tk_Fill3DPolygon(tkwin, pixmap, border, points, 3,
		elementBorderWidth, relief);
    } else {
	points[0].x = scrollPtr->arrowLength + scrollPtr->inset - 1;
	points[0].y = scrollPtr->inset - 1;
	points[1].x = scrollPtr->inset;
	points[1].y = width/2 + scrollPtr->inset;
	points[2].x = points[0].x;
	points[2].y = width + scrollPtr->inset;
	Tk_Fill3DPolygon(tkwin, pixmap, border, points, 3,
		elementBorderWidth, relief);
    }

    /*
     * Display the bottom or right arrow.
     */

    if (scrollPtr->activeField == BOTTOM_ARROW) {
	border = scrollPtr->activeBorder;
	relief = scrollPtr->activeField == BOTTOM_ARROW
		? scrollPtr->activeRelief : TK_RELIEF_RAISED;
    } else {
	border = scrollPtr->bgBorder;
	relief = TK_RELIEF_RAISED;
    }
    if (scrollPtr->vertical) {
	points[0].x = scrollPtr->inset;
	points[0].y = Tk_Height(tkwin) - scrollPtr->arrowLength
		- scrollPtr->inset + 1;
	points[1].x = width/2 + scrollPtr->inset;
	points[1].y = Tk_Height(tkwin) - scrollPtr->inset;
	points[2].x = width + scrollPtr->inset;
	points[2].y = points[0].y;
	Tk_Fill3DPolygon(tkwin, pixmap, border,
		points, 3, elementBorderWidth, relief);
    } else {
	points[0].x = Tk_Width(tkwin) - scrollPtr->arrowLength
		- scrollPtr->inset + 1;
	points[0].y = scrollPtr->inset - 1;
	points[1].x = points[0].x;
	points[1].y = width + scrollPtr->inset;
	points[2].x = Tk_Width(tkwin) - scrollPtr->inset;
	points[2].y = width/2 + scrollPtr->inset;
	Tk_Fill3DPolygon(tkwin, pixmap, border,
		points, 3, elementBorderWidth, relief);
    }

    /*
     * Display the slider.
     */

    if (scrollPtr->activeField == SLIDER) {
	border = scrollPtr->activeBorder;
	relief = scrollPtr->activeField == SLIDER ? scrollPtr->activeRelief
		: TK_RELIEF_RAISED;
    } else {
	border = scrollPtr->bgBorder;
	relief = TK_RELIEF_RAISED;
    }
    if (scrollPtr->vertical) {
	Tk_Fill3DRectangle(tkwin, pixmap, border,
		scrollPtr->inset, scrollPtr->sliderFirst,
		width, scrollPtr->sliderLast - scrollPtr->sliderFirst,
		elementBorderWidth, relief);
    } else {
	Tk_Fill3DRectangle(tkwin, pixmap, border,
		scrollPtr->sliderFirst, scrollPtr->inset,
		scrollPtr->sliderLast - scrollPtr->sliderFirst, width,
		elementBorderWidth, relief);
    }

    /*
     * Copy the information from the off-screen pixmap onto the screen,
     * then delete the pixmap.
     */

    XCopyArea(scrollPtr->display, pixmap, Tk_WindowId(tkwin),
	    scrollPtr->copyGC, 0, 0, (unsigned) Tk_Width(tkwin),
	    (unsigned) Tk_Height(tkwin), 0, 0);
    Tk_FreePixmap(scrollPtr->display, pixmap);

    done:
    scrollPtr->flags &= ~REDRAW_PENDING;
}

/*
 *--------------------------------------------------------------
 *
 * ScrollbarEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on scrollbars.
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
ScrollbarEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Scrollbar *scrollPtr = (Scrollbar *) clientData;

    if ((eventPtr->type == Expose) && (eventPtr->xexpose.count == 0)) {
	EventuallyRedraw(scrollPtr);
    } else if (eventPtr->type == DestroyNotify) {
	if (scrollPtr->tkwin != NULL) {
	    scrollPtr->tkwin = NULL;
	    Tcl_DeleteCommand(scrollPtr->interp,
		    Tcl_GetCommandName(scrollPtr->interp,
		    scrollPtr->widgetCmd));
	}
	if (scrollPtr->flags & REDRAW_PENDING) {
	    Tcl_CancelIdleCall(DisplayScrollbar, (ClientData) scrollPtr);
	}
	Tcl_EventuallyFree((ClientData) scrollPtr, DestroyScrollbar);
    } else if (eventPtr->type == ConfigureNotify) {
	ComputeScrollbarGeometry(scrollPtr);
	EventuallyRedraw(scrollPtr);
    } else if (eventPtr->type == FocusIn) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    scrollPtr->flags |= GOT_FOCUS;
	    if (scrollPtr->highlightWidth > 0) {
		EventuallyRedraw(scrollPtr);
	    }
	}
    } else if (eventPtr->type == FocusOut) {
	if (eventPtr->xfocus.detail != NotifyInferior) {
	    scrollPtr->flags &= ~GOT_FOCUS;
	    if (scrollPtr->highlightWidth > 0) {
		EventuallyRedraw(scrollPtr);
	    }
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ScrollbarCmdDeletedProc --
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
ScrollbarCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Scrollbar *scrollPtr = (Scrollbar *) clientData;
    Tk_Window tkwin = scrollPtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	scrollPtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeScrollbarGeometry --
 *
 *	After changes in a scrollbar's size or configuration, this
 *	procedure recomputes various geometry information used in
 *	displaying the scrollbar.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The scrollbar will be displayed differently.
 *
 *----------------------------------------------------------------------
 */

static void
ComputeScrollbarGeometry(scrollPtr)
    register Scrollbar *scrollPtr;	/* Scrollbar whose geometry may
					 * have changed. */
{
    int width, fieldLength;

    if (scrollPtr->highlightWidth < 0) {
	scrollPtr->highlightWidth = 0;
    }
    scrollPtr->inset = scrollPtr->highlightWidth + scrollPtr->borderWidth;
    width = (scrollPtr->vertical) ? Tk_Width(scrollPtr->tkwin)
	    : Tk_Height(scrollPtr->tkwin);
    scrollPtr->arrowLength = width - 2*scrollPtr->inset + 1;
    fieldLength = (scrollPtr->vertical ? Tk_Height(scrollPtr->tkwin)
	    : Tk_Width(scrollPtr->tkwin))
	    - 2*(scrollPtr->arrowLength + scrollPtr->inset);
    if (fieldLength < 0) {
	fieldLength = 0;
    }
    scrollPtr->sliderFirst = fieldLength*scrollPtr->firstFraction;
    scrollPtr->sliderLast = fieldLength*scrollPtr->lastFraction;

    /*
     * Adjust the slider so that some piece of it is always
     * displayed in the scrollbar and so that it has at least
     * a minimal width (so it can be grabbed with the mouse).
     */

    if (scrollPtr->sliderFirst > (fieldLength - 2*scrollPtr->borderWidth)) {
	scrollPtr->sliderFirst = fieldLength - 2*scrollPtr->borderWidth;
    }
    if (scrollPtr->sliderFirst < 0) {
	scrollPtr->sliderFirst = 0;
    }
    if (scrollPtr->sliderLast < (scrollPtr->sliderFirst
	    + MIN_SLIDER_LENGTH)) {
	scrollPtr->sliderLast = scrollPtr->sliderFirst + MIN_SLIDER_LENGTH;
    }
    if (scrollPtr->sliderLast > fieldLength) {
	scrollPtr->sliderLast = fieldLength;
    }
    scrollPtr->sliderFirst += scrollPtr->arrowLength + scrollPtr->inset;
    scrollPtr->sliderLast += scrollPtr->arrowLength + scrollPtr->inset;

    /*
     * Register the desired geometry for the window (leave enough space
     * for the two arrows plus a minimum-size slider, plus border around
     * the whole window, if any).  Then arrange for the window to be
     * redisplayed.
     */

    if (scrollPtr->vertical) {
	Tk_GeometryRequest(scrollPtr->tkwin,
		scrollPtr->width + 2*scrollPtr->inset,
		2*(scrollPtr->arrowLength + scrollPtr->borderWidth
		+ scrollPtr->inset));
    } else {
	Tk_GeometryRequest(scrollPtr->tkwin,
		2*(scrollPtr->arrowLength + scrollPtr->borderWidth
		+ scrollPtr->inset), scrollPtr->width + 2*scrollPtr->inset);
    }
    Tk_SetInternalBorder(scrollPtr->tkwin, scrollPtr->inset);
}

/*
 *--------------------------------------------------------------
 *
 * ScrollbarPosition --
 *
 *	Determine the scrollbar element corresponding to a
 *	given position.
 *
 * Results:
 *	One of TOP_ARROW, TOP_GAP, etc., indicating which element
 *	of the scrollbar covers the position given by (x, y).  If
 *	(x,y) is outside the scrollbar entirely, then OUTSIDE is
 *	returned.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
ScrollbarPosition(scrollPtr, x, y)
    register Scrollbar *scrollPtr;	/* Scrollbar widget record. */
    int x, y;				/* Coordinates within scrollPtr's
					 * window. */
{
    int length, width, tmp;

    if (scrollPtr->vertical) {
	length = Tk_Height(scrollPtr->tkwin);
	width = Tk_Width(scrollPtr->tkwin);
    } else {
	tmp = x;
	x = y;
	y = tmp;
	length = Tk_Width(scrollPtr->tkwin);
	width = Tk_Height(scrollPtr->tkwin);
    }

    if ((x < scrollPtr->inset) || (x >= (width - scrollPtr->inset))
	    || (y < scrollPtr->inset) || (y >= (length - scrollPtr->inset))) {
	return OUTSIDE;
    }

    /*
     * All of the calculations in this procedure mirror those in
     * DisplayScrollbar.  Be sure to keep the two consistent.
     */

    if (y < (scrollPtr->inset + scrollPtr->arrowLength)) {
	return TOP_ARROW;
    }
    if (y < scrollPtr->sliderFirst) {
	return TOP_GAP;
    }
    if (y < scrollPtr->sliderLast) {
	return SLIDER;
    }
    if (y >= (length - (scrollPtr->arrowLength + scrollPtr->inset))) {
	return BOTTOM_ARROW;
    }
    return BOTTOM_GAP;
}

/*
 *--------------------------------------------------------------
 *
 * EventuallyRedraw --
 *
 *	Arrange for one or more of the fields of a scrollbar
 *	to be redrawn.
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
EventuallyRedraw(scrollPtr)
    register Scrollbar *scrollPtr;	/* Information about widget. */
{
    if ((scrollPtr->tkwin == NULL) || (!Tk_IsMapped(scrollPtr->tkwin))) {
	return;
    }
    if ((scrollPtr->flags & REDRAW_PENDING) == 0) {
	Tcl_DoWhenIdle(DisplayScrollbar, (ClientData) scrollPtr);
	scrollPtr->flags |= REDRAW_PENDING;
    }
}

/* 
 * tkMacScrollbar.c --
 *
 *	This module implements the native Macintosh scrollbar widget 
 *	for the Tk toolkit.  A scrollbar displays a slider and two 
 *	arrows; mouse clicks on features within the scrollbar cause
 *	scrolling commands to be invoked.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacScrollbar.c 1.9 96/04/20 15:07:11
 */

#include "tkPort.h"
#include "default.h"
#include "tkInt.h"
#include <Controls.h>
#include "tkMacInt.h"
void SetUpClippingRgn _ANSI_ARGS_((Drawable drawable));

/*
 * The following definitions should really be in MacOS
 * header files.  They are included here as this is the only
 * file that needs the declarations.
 */
typedef	pascal void (*ThumbActionFunc)(void);

#if GENERATINGCFM
typedef UniversalProcPtr ThumbActionUPP;
#else
typedef ThumbActionFunc ThumbActionUPP;
#endif

enum {
	uppThumbActionProcInfo = kPascalStackBased
};

#if GENERATINGCFM
#define NewThumbActionProc(userRoutine)		\
		(ThumbActionUPP) NewRoutineDescriptor((ProcPtr)(userRoutine), uppThumbActionProcInfo, GetCurrentArchitecture())
#else
#define NewThumbActionProc(userRoutine)		\
		((ThumbActionUPP) (userRoutine))
#endif

/*
 * Change defines for Mac look & feel.
 * TODO: should be moved to tkDefaults.h
 */
#undef DEF_SCROLLBAR_WIDTH
#define DEF_SCROLLBAR_WIDTH		"15"
#undef DEF_SCROLLBAR_RELIEF
#define DEF_SCROLLBAR_RELIEF		"flat"
#undef DEF_SCROLLBAR_BORDER_WIDTH
#define DEF_SCROLLBAR_BORDER_WIDTH	"0"
#undef DEF_SCROLLBAR_HIGHLIGHT_WIDTH
#define DEF_SCROLLBAR_HIGHLIGHT_WIDTH	"0"

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
    Tk_TimerToken autoRepeat;	/* Token for auto-repeat that's
				 * currently in progress.  NULL means no
				 * auto-repeat in progress. */
    int flags;			/* Various flags;  see below for
				 * definitions. */

    /*
     * Mac specific fields.
     */
    ControlRef sbHandle;	/* Handle to the Scrollbar control struct. */
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
 * SCROLLBAR_GROW:		Non-zero means this window draws the grow
 *				region for the toplevel window.  Mac only.
 * ACTIVE:			Non-zero means this window is currently
 *				active (in the foreground).  Mac only.
 */

#define REDRAW_PENDING		1
#define NEW_STYLE_COMMANDS	2
#define GOT_FOCUS		4
#define SCROLLBAR_GROW		8
#define ACTIVE			16

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
static pascal void	ScrollbarActionProc _ANSI_ARGS_((ControlRef theControl,
			    ControlPartCode partCode));
static pascal void	ThumbActionProc _ANSI_ARGS_((void));
			    
/*
 * Globals uses locally in this file.
 */
static ControlActionUPP scrollActionProc = NULL; /* Pointer to func. */
static ThumbActionUPP thumbActionProc = NULL;    /* Pointer to func. */
static Scrollbar *activeScrollPtr = NULL;        /* Non-null when in thumb */
						 /* proc. */

/*
 *--------------------------------------------------------------
 *
 * Tk_MacScrollbarCmd --
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
Tk_MacScrollbarCmd(clientData, interp, argc, argv)
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
    scrollPtr->autoRepeat = NULL;
    scrollPtr->flags = 0;

    /*
     * Mac specific inits.
     */
    scrollPtr->sbHandle = NULL;

    Tk_SetClass(scrollPtr->tkwin, "MacScrollbar");
    Tk_CreateEventHandler(scrollPtr->tkwin,
	    ActivateMask|ExposureMask|StructureNotifyMask|FocusChangeMask|ButtonPressMask,
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

    /*
     * Free Macintosh control.
     */
    if (scrollPtr->sbHandle == NULL) {
        DisposeControl(scrollPtr->sbHandle);
	scrollPtr->sbHandle = NULL;
    }
        
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
    
    MacDrawable *macDraw;
    CGrafPtr saveWorld;
    GDHandle saveDevice;
    GWorldPtr destPort;
    double middle;
    WindowRef windowRef;
    int drawGrowRgn = false;
    int flushRight = false;
    int flushBottom = false;
    
    if ((scrollPtr->tkwin == NULL) || !Tk_IsMapped(tkwin)) {
	goto done;
    }

    /*
     * Draw the focus or any 3D relief we may have.
     */
    if (scrollPtr->highlightWidth != 0) {
	GC gc;

	if (scrollPtr->flags & GOT_FOCUS) {
	    gc = Tk_GCForColor(scrollPtr->highlightColorPtr,
		    Tk_WindowId(tkwin));
	} else {
	    gc = Tk_GCForColor(scrollPtr->highlightBgColorPtr,
		    Tk_WindowId(tkwin));
	}
	Tk_DrawFocusHighlight(tkwin, gc, scrollPtr->highlightWidth,
		Tk_WindowId(tkwin));
    }
    Tk_Draw3DRectangle(tkwin, Tk_WindowId(tkwin), scrollPtr->bgBorder,
	    scrollPtr->highlightWidth, scrollPtr->highlightWidth,
	    Tk_Width(tkwin) - 2*scrollPtr->highlightWidth,
	    Tk_Height(tkwin) - 2*scrollPtr->highlightWidth,
	    scrollPtr->borderWidth, scrollPtr->relief);

    /*
     * Set up port for drawing Macintosh control.
     */
    macDraw = (MacDrawable *) Tk_WindowId(tkwin);
    destPort = TkMacGetDrawablePort(Tk_WindowId(tkwin));
    GetGWorld(&saveWorld, &saveDevice);
    SetGWorld(destPort, NULL);
    SetUpClippingRgn(Tk_WindowId(tkwin));

    /*
     * Given the Tk parameters for the fractions of the start and
     * end of the thumb, the following calculation determines the
     * location for the fixed sized Macintosh thumb.
     */
    middle = scrollPtr->firstFraction / (scrollPtr->firstFraction +
	    (1.0 - scrollPtr->lastFraction));

    if (scrollPtr->sbHandle == NULL) {
        Rect r;
        
        r.left = r.top = 0;
        r.right = r.bottom = 1;
	scrollPtr->sbHandle = NewControl((WindowRef) destPort, &r, "\p",
		false, (short) (middle * 1000), 0, 1000,
		scrollBarProc, (SInt32) scrollPtr);

	/*
	 * If we are foremost than make us active.
	 */
	if ((WindowPtr) destPort == FrontWindow()) {
	    scrollPtr->flags |= ACTIVE;
	}
    }
    windowRef  = (**scrollPtr->sbHandle).contrlOwner;
    
    /*
     * We can't use the Macintosh commands SizeControl and MoveControl as these
     * calls will also cause a redraw which in our case will also cause
     * flicker.  To avoid this we adjust the control record directly.  The
     * Draw1Control command appears to just draw where ever the control says to
     * draw so this seems right.
     *
     * NOTE: changing the control record directly may not work when
     * Apple releases the Copland version of the MacOS in late 1996.
     */
     
    (**scrollPtr->sbHandle).contrlRect.left = macDraw->xOff + scrollPtr->inset;
    (**scrollPtr->sbHandle).contrlRect.top = macDraw->yOff + scrollPtr->inset;
    (**scrollPtr->sbHandle).contrlRect.right = macDraw->xOff + Tk_Width(tkwin)
	- scrollPtr->inset;
    (**scrollPtr->sbHandle).contrlRect.bottom = macDraw->yOff +
	Tk_Height(tkwin) - scrollPtr->inset;
    
    /*
     * Here is a lovely hack to draw the grow region of a window.
     */
    /* TODO: use accessor function!!! */
    if (windowRef->portRect.top == (**scrollPtr->sbHandle).contrlRect.top) {
	(**scrollPtr->sbHandle).contrlRect.top--;
    }
	
    if (windowRef->portRect.left == (**scrollPtr->sbHandle).contrlRect.left) {
	(**scrollPtr->sbHandle).contrlRect.left--;
    }
	
    if (windowRef->portRect.right == (**scrollPtr->sbHandle).contrlRect.right) {
	flushRight = true;
	(**scrollPtr->sbHandle).contrlRect.right++;
    }
	
    if (windowRef->portRect.bottom == (**scrollPtr->sbHandle).contrlRect.bottom) {
	flushBottom = true;
	(**scrollPtr->sbHandle).contrlRect.bottom++;
    }
	
    if (flushBottom && flushRight) {
	if (scrollPtr->vertical) {
	    (**scrollPtr->sbHandle).contrlRect.bottom -= 14;
	} else {
	    (**scrollPtr->sbHandle).contrlRect.right -= 14;
	}
	drawGrowRgn = true;
	TkMacSetScrollbarGrow((TkWindow *) tkwin, true);
    } else {
	TkMacSetScrollbarGrow((TkWindow *) tkwin, false);
    }
    
    /*
     * Set the thumb position in the scrollbar.
     */
    (**scrollPtr->sbHandle).contrlValue = (short) (middle * 1000);
    if ((**scrollPtr->sbHandle).contrlHilite == 0 || 
		(**scrollPtr->sbHandle).contrlHilite == 255) {
	if (scrollPtr->firstFraction == 0.0 &&
		scrollPtr->lastFraction == 1.0) {
	    (**scrollPtr->sbHandle).contrlHilite = 255;
	} else {
	    (**scrollPtr->sbHandle).contrlHilite = 0;
	}
    }
    if ((**scrollPtr->sbHandle).contrlVis != 255) {
	(**scrollPtr->sbHandle).contrlVis = 255;
    }
    
    if (scrollPtr->flags & ACTIVE) {
	Draw1Control(scrollPtr->sbHandle);
	if (drawGrowRgn) {
	    DrawGrowIcon(windowRef);
	}
    } else {
	(**scrollPtr->sbHandle).contrlHilite = 255;
	Draw1Control(scrollPtr->sbHandle);
	if (drawGrowRgn) {
	    DrawGrowIcon(windowRef);
	    Tk_Fill3DRectangle(tkwin, Tk_WindowId(tkwin), scrollPtr->bgBorder,
		Tk_Width(tkwin) - 13, Tk_Height(tkwin) - 13,
		Tk_Width(tkwin), Tk_Height(tkwin),
		0, TK_RELIEF_FLAT);
	}
    }
    
    SetGWorld(saveWorld, saveDevice);
     
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
    Tcl_Interp *interp;

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
	    Tk_CancelIdleCall(DisplayScrollbar, (ClientData) scrollPtr);
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
    } else if (eventPtr->type == UnmapNotify) {
	TkMacSetScrollbarGrow((TkWindow *) scrollPtr->tkwin, false);
    } else if (eventPtr->type == ActivateNotify) {
	scrollPtr->flags |= ACTIVE;
	EventuallyRedraw(scrollPtr);
    } else if (eventPtr->type == DeactivateNotify) {
	scrollPtr->flags &= ~ACTIVE;
	EventuallyRedraw(scrollPtr);
    } else if (eventPtr->type == ButtonPress) {
    	Point where;
    	Rect bounds;
    	int part, x, y, dummy;
    	unsigned int state;
	CGrafPtr saveWorld;
	GDHandle saveDevice;
	GWorldPtr destPort;
	Window dummyWin;

	/*
	 * To call Macintosh control routines we must have the port
	 * set to the window containing the control.  We will then test
	 * which part of the control was hit and act accordingly.
	 */
	destPort = TkMacGetDrawablePort(Tk_WindowId(scrollPtr->tkwin));
	GetGWorld(&saveWorld, &saveDevice);
	SetGWorld(destPort, NULL);
	SetUpClippingRgn(Tk_WindowId(scrollPtr->tkwin));

	TkMacWinBounds((TkWindow *) scrollPtr->tkwin, &bounds);		
    	where.h = eventPtr->xbutton.x + bounds.left;
    	where.v = eventPtr->xbutton.y + bounds.top;
	part = TestControl(scrollPtr->sbHandle, where);
	if (part == inThumb && scrollPtr->jump == false) {
	    /*
	     * Case 1: In thumb, no jump scrolling.  Call track control
	     * with the thumb action proc which will do most of the work.
	     * Set the global activeScrollPtr to the current control
	     * so the callback may have access to it.
	     */
    	    if (thumbActionProc == NULL) {
		thumbActionProc = NewThumbActionProc(ThumbActionProc);
	    }
	    activeScrollPtr = scrollPtr;
	    part = TrackControl(scrollPtr->sbHandle, where,
		    (ControlActionUPP) thumbActionProc);
	    activeScrollPtr = NULL;
	} else if (part == inThumb) {
	    /*
	     * Case 2: in thumb with jump scrolling.  Call TrackControl
	     * with a NULL action proc.  Use the new value of the control
	     * to set update the control.
	     */
	    part = TrackControl(scrollPtr->sbHandle, where, NULL);
	    if (part == inThumb) {
	    	double newFirstFraction, thumbWidth;
		Tcl_DString cmdString;
		char vauleString[TCL_DOUBLE_SPACE];

		/*
		 * The following calculation takes the new control
		 * value and maps it to what Tk needs for its variable
		 * thumb size representation.
		 */
		thumbWidth = scrollPtr->lastFraction
		     - scrollPtr->firstFraction;
		newFirstFraction = (1.0 - thumbWidth) *
		    ((double) GetControlValue(scrollPtr->sbHandle) / 1000.0);
		sprintf(vauleString, "%g", newFirstFraction);

		Tcl_DStringInit(&cmdString);
		Tcl_DStringAppend(&cmdString, scrollPtr->command,
			strlen(scrollPtr->command));
		Tcl_DStringAppendElement(&cmdString, "moveto");
		Tcl_DStringAppendElement(&cmdString, vauleString);
		Tcl_DStringAppend(&cmdString, "; update idletasks",
			strlen("; update idletasks"));
		
                interp = scrollPtr->interp;
                Tcl_Preserve((ClientData) interp);
		Tcl_GlobalEval(interp, cmdString.string);
                Tcl_Release((ClientData) interp);
		Tcl_DStringFree(&cmdString);		
	    }
	} else if (part != 0) {
	    /*
	     * Case 3: in any other part of the scrollbar.  We call
	     * TrackControl with the scrollActionProc which will do
	     * most all the work.
	     */
	    if (scrollActionProc == NULL) {
		scrollActionProc = NewControlActionProc(ScrollbarActionProc);
	    }
	    TrackControl(scrollPtr->sbHandle, where, scrollActionProc);
	    HiliteControl(scrollPtr->sbHandle, 0);
	}
	
	/*
	 * The TrackControl call will "eat" the ButtonUp event.  We now
	 * generate a ButtonUp event so Tk will unset implicit grabs etc.
	 */
	GetMouse(&where);
	XQueryPointer(NULL, None, &dummyWin, &dummyWin, &x,
	    &y, &dummy, &dummy, &state);
	TkGenerateButtonEvent(x, y, state);

	SetGWorld(saveWorld, saveDevice);
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

/* TODO: this should be Mac specific */

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
    ControlPartCode part;
    Point where;
    Rect bounds;

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

    TkMacWinBounds((TkWindow *) scrollPtr->tkwin, &bounds);		
    where.h = x + bounds.left;
    where.v = y + bounds.top;
    part = TestControl(scrollPtr->sbHandle, where);
    switch (part) {
    	case inUpButton:
	    return TOP_ARROW;
    	case inPageUp:
	    return TOP_GAP;
    	case inThumb:
	    return SLIDER;
    	case inPageDown:
	    return BOTTOM_GAP;
    	case inDownButton:
	    return BOTTOM_ARROW;
    	default:
	    return OUTSIDE;
    }
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
	Tk_DoWhenIdle(DisplayScrollbar, (ClientData) scrollPtr);
	scrollPtr->flags |= REDRAW_PENDING;
    }
}

/*
 *--------------------------------------------------------------
 *
 * ScrollbarActionProc --
 *
 *	Callback procedure used by the Macintosh toolbox call
 *	TrackControl.  This call will update the display while
 *	the scrollbar is being manipulated by the user.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May change the display.
 *
 *--------------------------------------------------------------
 */

static pascal void
ScrollbarActionProc(ControlRef theControl, ControlPartCode partCode)
    /* ControlRef theControl;	/* Handle to scrollbat control */
    /* ControlPartCode partCode;	/* Part of scrollbar that was "hit" */
{
    register Scrollbar *scrollPtr = (Scrollbar *) GetCRefCon(theControl);
    Tcl_DString cmdString;
    Tcl_Interp *interp;
    
    Tcl_DStringInit(&cmdString);
    switch (partCode) {
    	case inPageUp:
    	case inPageDown:
    	case inDownButton:
    	case inUpButton:
    	    if (partCode == inPageUp || partCode == inPageDown) {
    		Tcl_DStringAppendElement(&cmdString, "tkScrollByPages");
    	    } else {
    		Tcl_DStringAppendElement(&cmdString, "tkScrollByUnits");
    	    }
    	    Tcl_DStringAppendElement(&cmdString,
		    Tk_PathName(scrollPtr->tkwin));
    	    Tcl_DStringAppendElement(&cmdString, "hv");
    	    if (partCode == inPageUp || partCode == inUpButton) {
    		Tcl_DStringAppendElement(&cmdString, "-1");
    	    } else {
    		Tcl_DStringAppendElement(&cmdString, "1");
    	    }
    	    Tcl_DStringAppend(&cmdString, "; update idletasks",
		    strlen("; update idletasks"));
            interp = scrollPtr->interp;
            Tcl_Preserve((ClientData) interp);
    	    Tcl_GlobalEval(interp, cmdString.string);
            Tcl_Release((ClientData) interp);
    	    break;
    }
    Tcl_DStringFree(&cmdString);
}

/*
 *--------------------------------------------------------------
 *
 * ThumbActionProc --
 *
 *	Callback procedure used by the Macintosh toolbox call
 *	TrackControl.  This call is used to track the thumb of
 *	the scrollbar.  Unlike the ScrollbarActionProc function
 *	this function is called once and basically takes over
 *	tracking the scrollbar from the control.  This is done
 *	to avoid conflicts with what the control plans to draw.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	May change the display.
 *
 *--------------------------------------------------------------
 */

static pascal void
ThumbActionProc()
{
    register Scrollbar *scrollPtr = activeScrollPtr;
    Tcl_DString cmdString;
    Rect nullRect = {0,0,0,0};
    int origValue, trackBarPin;
    double thumbWidth, newFirstFraction, trackBarSize;
    char vauleString[40];
    Point currentPoint = { 0, 0 };
    Point lastPoint = { 0, 0 };
    Rect trackRect;
    Tcl_Interp *interp;
    
    if (scrollPtr == NULL) {
	return;
    }

    Tcl_DStringInit(&cmdString);
    
    /*
     * First compute values that will remain constant during the tracking
     * of the thumb.  The variable trackBarSize is the length of the scrollbar
     * minus the 2 arrows and half the width of the thumb on both sides
     * (3 * arrowLength).  The variable trackBarPin is the lower starting point
     * of the drag region.
     *
     * Note: the arrowLength is equal to the thumb width of a Mac scrollbar.
     */
    origValue = GetControlValue(scrollPtr->sbHandle);
    trackRect = (**scrollPtr->sbHandle).contrlRect;
    if (scrollPtr->vertical == true) {
	trackBarSize = (double) (trackRect.bottom - trackRect.top
		- (scrollPtr->arrowLength * 3));
	trackBarPin = trackRect.top + scrollPtr->arrowLength
	    + (scrollPtr->arrowLength / 2);
	InsetRect(&trackRect, -25, -113);
	
    } else {
	trackBarSize = (double) (trackRect.right - trackRect.left
		- (scrollPtr->arrowLength * 3));
	trackBarPin = trackRect.left + scrollPtr->arrowLength
	    + (scrollPtr->arrowLength / 2);
	InsetRect(&trackRect, -113, -25);
    }

    /*
     * Track the mouse while the button is held down.  If the mouse is moved,
     * we calculate the value that should be passed to the "command" part of
     * the scrollbar.
     */
    while (StillDown()) {
	GetMouse(&currentPoint);
	if (EqualPt(currentPoint, lastPoint)) {
	    continue;
	}
	lastPoint = currentPoint;

	/*
	 * Calculating this value is a little tricky.  We need to calculate a
	 * value for where the thumb would be in a Motif widget (variable
	 * thumb).  This value is what the "command" expects and is what will
	 * be resent to the scrollbar to update its value.
	 */
	thumbWidth = scrollPtr->lastFraction - scrollPtr->firstFraction;
	if (PtInRect(currentPoint, &trackRect)) {
	    if (scrollPtr->vertical == true) {
		newFirstFraction =  (1.0 - thumbWidth) *
		    ((double) (currentPoint.v - trackBarPin) / trackBarSize);
	    } else {
		newFirstFraction =  (1.0 - thumbWidth) *
		    ((double) (currentPoint.h - trackBarPin) / trackBarSize);
	    }
	} else {
	    newFirstFraction = ((double) origValue / 1000.0)
		* (1.0 - thumbWidth);
	}
	
	sprintf(vauleString, "%g", newFirstFraction);

	Tcl_DStringSetLength(&cmdString, 0);
	Tcl_DStringAppend(&cmdString, scrollPtr->command,
		strlen(scrollPtr->command));
	Tcl_DStringAppendElement(&cmdString, "moveto");
	Tcl_DStringAppendElement(&cmdString, vauleString);
	Tcl_DStringAppend(&cmdString, "; update idletasks",
		strlen("; update idletasks"));
    
        interp = scrollPtr->interp;
        Tcl_Preserve((ClientData) interp);
	Tcl_GlobalEval(interp, cmdString.string);
        Tcl_Release((ClientData) interp);
    }
    
    /*
     * This next bit of code is a bit of a hack - but needed.  The problem is
     * that the control wants to draw the drag outline if the control value
     * changes during the drag (which it does).  What we do here is change the
     * clip region to hide this drawing from the user.
     */
    ClipRect(&nullRect);
    
    Tcl_DStringFree(&cmdString);
    return;
}

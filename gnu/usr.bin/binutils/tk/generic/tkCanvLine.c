/* 
 * tkCanvLine.c --
 *
 *	This file implements line items for canvas widgets.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkCanvLine.c 1.43 96/02/15 18:52:30
 */

#include <stdio.h>
#include "tkInt.h"
#include "tkPort.h"

/*
 * The structure below defines the record for each line item.
 */

typedef struct LineItem  {
    Tk_Item header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;		/* Canvas containing item.  Needed for
				 * parsing arrow shapes. */
    int numPoints;		/* Number of points in line (always >= 2). */
    double *coordPtr;		/* Pointer to malloc-ed array containing
				 * x- and y-coords of all points in line.
				 * X-coords are even-valued indices, y-coords
				 * are corresponding odd-valued indices. If
				 * the line has arrowheads then the first
				 * and last points have been adjusted to refer
				 * to the necks of the arrowheads rather than
				 * their tips.  The actual endpoints are
				 * stored in the *firstArrowPtr and
				 * *lastArrowPtr, if they exist. */
    int width;			/* Width of line. */
    XColor *fg;			/* Foreground color for line. */
    Pixmap fillStipple;		/* Stipple bitmap for filling line. */
    int capStyle;		/* Cap style for line. */
    int joinStyle;		/* Join style for line. */
    GC gc;			/* Graphics context for filling line. */
    GC arrowGC;			/* Graphics context for drawing arrowheads. */
    Tk_Uid arrow;		/* Indicates whether or not to draw arrowheads:
				 * "none", "first", "last", or "both". */
    float arrowShapeA;		/* Distance from tip of arrowhead to center. */
    float arrowShapeB;		/* Distance from tip of arrowhead to trailing
				 * point, measured along shaft. */
    float arrowShapeC;		/* Distance of trailing points from outside
				 * edge of shaft. */
    double *firstArrowPtr;	/* Points to array of PTS_IN_ARROW points
				 * describing polygon for arrowhead at first
				 * point in line.  First point of arrowhead
				 * is tip.  Malloc'ed.  NULL means no arrowhead
				 * at first point. */
    double *lastArrowPtr;	/* Points to polygon for arrowhead at last
				 * point in line (PTS_IN_ARROW points, first
				 * of which is tip).  Malloc'ed.  NULL means
				 * no arrowhead at last point. */
    int smooth;			/* Non-zero means draw line smoothed (i.e.
				 * with Bezier splines). */
    int splineSteps;		/* Number of steps in each spline segment. */
} LineItem;

/*
 * Number of points in an arrowHead:
 */

#define PTS_IN_ARROW 6

/*
 * Prototypes for procedures defined in this file:
 */

static int		ArrowheadPostscript _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, LineItem *linePtr,
			    double *arrowPtr));
static void		ComputeLineBbox _ANSI_ARGS_((Tk_Canvas canvas,
			    LineItem *linePtr));
static int		ConfigureLine _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
			    char **argv, int flags));
static int		ConfigureArrows _ANSI_ARGS_((Tk_Canvas canvas,
			    LineItem *linePtr));
static int		CreateLine _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, struct Tk_Item *itemPtr,
			    int argc, char **argv));
static void		DeleteLine _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, Display *display));
static void		DisplayLine _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, Display *display, Drawable dst,
			    int x, int y, int width, int height));
static int		LineCoords _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr,
			    int argc, char **argv));
static int		LineToArea _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double *rectPtr));
static double		LineToPoint _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double *coordPtr));
static int		LineToPostscript _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, int prepass));
static int		ParseArrowShape _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, Tk_Window tkwin, char *value,
			    char *recordPtr, int offset));
static char *		PrintArrowShape _ANSI_ARGS_((ClientData clientData,
			    Tk_Window tkwin, char *recordPtr, int offset,
			    Tcl_FreeProc **freeProcPtr));
static void		ScaleLine _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double originX, double originY,
			    double scaleX, double scaleY));
static void		TranslateLine _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double deltaX, double deltaY));

/*
 * Information used for parsing configuration specs.  If you change any
 * of the default strings, be sure to change the corresponding default
 * values in CreateLine.
 */

static Tk_CustomOption arrowShapeOption = {ParseArrowShape,
	PrintArrowShape, (ClientData) NULL};
static Tk_CustomOption tagsOption = {Tk_CanvasTagsParseProc,
    Tk_CanvasTagsPrintProc, (ClientData) NULL
};

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_UID, "-arrow", (char *) NULL, (char *) NULL,
	"none", Tk_Offset(LineItem, arrow), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-arrowshape", (char *) NULL, (char *) NULL,
	"8 10 3", Tk_Offset(LineItem, arrowShapeA),
	TK_CONFIG_DONT_SET_DEFAULT, &arrowShapeOption},
    {TK_CONFIG_CAP_STYLE, "-capstyle", (char *) NULL, (char *) NULL,
	"butt", Tk_Offset(LineItem, capStyle), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_COLOR, "-fill", (char *) NULL, (char *) NULL,
	"black", Tk_Offset(LineItem, fg), TK_CONFIG_NULL_OK},
    {TK_CONFIG_JOIN_STYLE, "-joinstyle", (char *) NULL, (char *) NULL,
	"round", Tk_Offset(LineItem, joinStyle), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BOOLEAN, "-smooth", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(LineItem, smooth), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_INT, "-splinesteps", (char *) NULL, (char *) NULL,
	"12", Tk_Offset(LineItem, splineSteps), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BITMAP, "-stipple", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(LineItem, fillStipple), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
	(char *) NULL, 0, TK_CONFIG_NULL_OK, &tagsOption},
    {TK_CONFIG_PIXELS, "-width", (char *) NULL, (char *) NULL,
	"1", Tk_Offset(LineItem, width), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * The structures below defines the line item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkLineType = {
    "line",				/* name */
    sizeof(LineItem),			/* itemSize */
    CreateLine,				/* createProc */
    configSpecs,			/* configSpecs */
    ConfigureLine,			/* configureProc */
    LineCoords,				/* coordProc */
    DeleteLine,				/* deleteProc */
    DisplayLine,			/* displayProc */
    0,					/* alwaysRedraw */
    LineToPoint,			/* pointProc */
    LineToArea,				/* areaProc */
    LineToPostscript,			/* postscriptProc */
    ScaleLine,				/* scaleProc */
    TranslateLine,			/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL		/* nextPtr */
};

/*
 * The Tk_Uid's below refer to uids for the various arrow types:
 */

static Tk_Uid noneUid = NULL;
static Tk_Uid firstUid = NULL;
static Tk_Uid lastUid = NULL;
static Tk_Uid bothUid = NULL;

/*
 * The definition below determines how large are static arrays
 * used to hold spline points (splines larger than this have to
 * have their arrays malloc-ed).
 */

#define MAX_STATIC_POINTS 200

/*
 *--------------------------------------------------------------
 *
 * CreateLine --
 *
 *	This procedure is invoked to create a new line item in
 *	a canvas.
 *
 * Results:
 *	A standard Tcl return value.  If an error occurred in
 *	creating the item, then an error message is left in
 *	interp->result;  in this case itemPtr is left uninitialized,
 *	so it can be safely freed by the caller.
 *
 * Side effects:
 *	A new line item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreateLine(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    Tk_Canvas canvas;			/* Canvas to hold new item. */
    Tk_Item *itemPtr;			/* Record to hold new item;  header
					 * has been initialized by caller. */
    int argc;				/* Number of arguments in argv. */
    char **argv;			/* Arguments describing line. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    int i;

    if (argc < 4) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tk_PathName(Tk_CanvasTkwin(canvas)), " create ",
		itemPtr->typePtr->name, " x1 y1 x2 y2 ?x3 y3 ...? ?options?\"",
		(char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Carry out initialization that is needed to set defaults and to
     * allow proper cleanup after errors during the the remainder of
     * this procedure.
     */

    linePtr->canvas = canvas;
    linePtr->numPoints = 0;
    linePtr->coordPtr = NULL;
    linePtr->width = 1;
    linePtr->fg = None;
    linePtr->fillStipple = None;
    linePtr->capStyle = CapButt;
    linePtr->joinStyle = JoinRound;
    linePtr->gc = None;
    linePtr->arrowGC = None;
    if (noneUid == NULL) {
	noneUid = Tk_GetUid("none");
	firstUid = Tk_GetUid("first");
	lastUid = Tk_GetUid("last");
	bothUid = Tk_GetUid("both");
    }
    linePtr->arrow = noneUid;
    linePtr->arrowShapeA = 8.0;
    linePtr->arrowShapeB = 10.0;
    linePtr->arrowShapeC = 3.0;
    linePtr->firstArrowPtr = NULL;
    linePtr->lastArrowPtr = NULL;
    linePtr->smooth = 0;
    linePtr->splineSteps = 12;

    /*
     * Count the number of points and then parse them into a point
     * array.  Leading arguments are assumed to be points if they
     * start with a digit or a minus sign followed by a digit.
     */

    for (i = 4; i < (argc-1); i+=2) {
	if ((!isdigit(UCHAR(argv[i][0]))) &&
		((argv[i][0] != '-')
		|| ((argv[i][1] != '.') && !isdigit(UCHAR(argv[i][1]))))) {
	    break;
	}
    }
    if (LineCoords(interp, canvas, itemPtr, i, argv) != TCL_OK) {
	goto error;
    }
    if (ConfigureLine(interp, canvas, itemPtr, argc-i, argv+i, 0) == TCL_OK) {
	return TCL_OK;
    }

    error:
    DeleteLine(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * LineCoords --
 *
 *	This procedure is invoked to process the "coords" widget
 *	command on lines.  See the user documentation for details
 *	on what it does.
 *
 * Results:
 *	Returns TCL_OK or TCL_ERROR, and sets interp->result.
 *
 * Side effects:
 *	The coordinates for the given item may be changed.
 *
 *--------------------------------------------------------------
 */

static int
LineCoords(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item whose coordinates are to be
					 * read or modified. */
    int argc;				/* Number of coordinates supplied in
					 * argv. */
    char **argv;			/* Array of coordinates: x1, y1,
					 * x2, y2, ... */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    char buffer[TCL_DOUBLE_SPACE];
    int i, numPoints;

    if (argc == 0) {
	double *coordPtr;
	int numCoords;

	numCoords = 2*linePtr->numPoints;
	if (linePtr->firstArrowPtr != NULL) {
	    coordPtr = linePtr->firstArrowPtr;
	} else {
	    coordPtr = linePtr->coordPtr;
	}
	for (i = 0; i < numCoords; i++, coordPtr++) {
	    if (i == 2) {
		coordPtr = linePtr->coordPtr+2;
	    }
	    if ((linePtr->lastArrowPtr != NULL) && (i == (numCoords-2))) {
		coordPtr = linePtr->lastArrowPtr;
	    }
	    Tcl_PrintDouble(interp, *coordPtr, buffer);
	    Tcl_AppendElement(interp, buffer);
	}
    } else if (argc < 4) {
	Tcl_AppendResult(interp,
		"too few coordinates for line: must have at least 4",
		(char *) NULL);
	return TCL_ERROR;
    } else if (argc & 1) {
	Tcl_AppendResult(interp,
		"odd number of coordinates specified for line",
		(char *) NULL);
	return TCL_ERROR;
    } else {
	numPoints = argc/2;
	if (linePtr->numPoints != numPoints) {
	    if (linePtr->coordPtr != NULL) {
		ckfree((char *) linePtr->coordPtr);
	    }
	    linePtr->coordPtr = (double *) ckalloc((unsigned)
		    (sizeof(double) * argc));
	    linePtr->numPoints = numPoints;
	}
	for (i = argc-1; i >= 0; i--) {
	    if (Tk_CanvasGetCoord(interp, canvas, argv[i],
		    &linePtr->coordPtr[i]) != TCL_OK) {
		return TCL_ERROR;
	    }
	}

	/*
	 * Update arrowheads by throwing away any existing arrow-head
	 * information and calling ConfigureArrows to recompute it.
	 */

	if (linePtr->firstArrowPtr != NULL) {
	    ckfree((char *) linePtr->firstArrowPtr);
	    linePtr->firstArrowPtr = NULL;
	}
	if (linePtr->lastArrowPtr != NULL) {
	    ckfree((char *) linePtr->lastArrowPtr);
	    linePtr->lastArrowPtr = NULL;
	}
	if (linePtr->arrow != noneUid) {
	    ConfigureArrows(canvas, linePtr);
	}
	ComputeLineBbox(canvas, linePtr);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ConfigureLine --
 *
 *	This procedure is invoked to configure various aspects
 *	of a line item such as its background color.
 *
 * Results:
 *	A standard Tcl result code.  If an error occurs, then
 *	an error message is left in interp->result.
 *
 * Side effects:
 *	Configuration information, such as colors and stipple
 *	patterns, may be set for itemPtr.
 *
 *--------------------------------------------------------------
 */

static int
ConfigureLine(interp, canvas, itemPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tk_Canvas canvas;		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr;		/* Line item to reconfigure. */
    int argc;			/* Number of elements in argv.  */
    char **argv;		/* Arguments describing things to configure. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    XGCValues gcValues;
    GC newGC, arrowGC;
    unsigned long mask;
    Tk_Window tkwin;

    tkwin = Tk_CanvasTkwin(canvas);
    if (Tk_ConfigureWidget(interp, tkwin, configSpecs, argc, argv,
	    (char *) linePtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few of the options require additional processing, such as
     * graphics contexts.
     */

    if (linePtr->fg == NULL) {
	newGC = arrowGC = None;
    } else {
	gcValues.foreground = linePtr->fg->pixel;
	gcValues.join_style = linePtr->joinStyle;
	if (linePtr->width < 0) {
	    linePtr->width = 1;
	}
	gcValues.line_width = linePtr->width;
	mask = GCForeground|GCJoinStyle|GCLineWidth;
	if (linePtr->fillStipple != None) {
	    gcValues.stipple = linePtr->fillStipple;
	    gcValues.fill_style = FillStippled;
	    mask |= GCStipple|GCFillStyle;
	}
	if (linePtr->arrow == noneUid) {
	    gcValues.cap_style = linePtr->capStyle;
	    mask |= GCCapStyle;
	}
	newGC = Tk_GetGC(tkwin, mask, &gcValues);
	gcValues.line_width = 0;
	arrowGC = Tk_GetGC(tkwin, mask, &gcValues);
    }
    if (linePtr->gc != None) {
	Tk_FreeGC(Tk_Display(tkwin), linePtr->gc);
    }
    if (linePtr->arrowGC != None) {
	Tk_FreeGC(Tk_Display(tkwin), linePtr->arrowGC);
    }
    linePtr->gc = newGC;
    linePtr->arrowGC = arrowGC;

    /*
     * Keep spline parameters within reasonable limits.
     */

    if (linePtr->splineSteps < 1) {
	linePtr->splineSteps = 1;
    } else if (linePtr->splineSteps > 100) {
	linePtr->splineSteps = 100;
    }

    /*
     * Setup arrowheads, if needed.  If arrowheads are turned off,
     * restore the line's endpoints (they were shortened when the
     * arrowheads were added).
     */

    if ((linePtr->firstArrowPtr != NULL) && (linePtr->arrow != firstUid)
	    && (linePtr->arrow != bothUid)) {
	linePtr->coordPtr[0] = linePtr->firstArrowPtr[0];
	linePtr->coordPtr[1] = linePtr->firstArrowPtr[1];
	ckfree((char *) linePtr->firstArrowPtr);
	linePtr->firstArrowPtr = NULL;
    }
    if ((linePtr->lastArrowPtr != NULL) && (linePtr->arrow != lastUid)
	    && (linePtr->arrow != bothUid)) {
	int i;

	i = 2*(linePtr->numPoints-1);
	linePtr->coordPtr[i] = linePtr->lastArrowPtr[0];
	linePtr->coordPtr[i+1] = linePtr->lastArrowPtr[1];
	ckfree((char *) linePtr->lastArrowPtr);
	linePtr->lastArrowPtr = NULL;
    }
    if (linePtr->arrow != noneUid) {
	if ((linePtr->arrow != firstUid) && (linePtr->arrow != lastUid)
		&& (linePtr->arrow != bothUid)) {
	    Tcl_AppendResult(interp, "bad arrow spec \"",
		    linePtr->arrow, "\": must be none, first, last, or both",
		    (char *) NULL);
	    linePtr->arrow = noneUid;
	    return TCL_ERROR;
	}
	ConfigureArrows(canvas, linePtr);
    }

    /*
     * Recompute bounding box for line.
     */

    ComputeLineBbox(canvas, linePtr);

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DeleteLine --
 *
 *	This procedure is called to clean up the data structure
 *	associated with a line item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Resources associated with itemPtr are released.
 *
 *--------------------------------------------------------------
 */

static void
DeleteLine(canvas, itemPtr, display)
    Tk_Canvas canvas;			/* Info about overall canvas widget. */
    Tk_Item *itemPtr;			/* Item that is being deleted. */
    Display *display;			/* Display containing window for
					 * canvas. */
{
    LineItem *linePtr = (LineItem *) itemPtr;

    if (linePtr->coordPtr != NULL) {
	ckfree((char *) linePtr->coordPtr);
    }
    if (linePtr->fg != NULL) {
	Tk_FreeColor(linePtr->fg);
    }
    if (linePtr->fillStipple != None) {
	Tk_FreeBitmap(display, linePtr->fillStipple);
    }
    if (linePtr->gc != None) {
	Tk_FreeGC(display, linePtr->gc);
    }
    if (linePtr->arrowGC != None) {
	Tk_FreeGC(display, linePtr->arrowGC);
    }
    if (linePtr->firstArrowPtr != NULL) {
	ckfree((char *) linePtr->firstArrowPtr);
    }
    if (linePtr->lastArrowPtr != NULL) {
	ckfree((char *) linePtr->lastArrowPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ComputeLineBbox --
 *
 *	This procedure is invoked to compute the bounding box of
 *	all the pixels that may be drawn as part of a line.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The fields x1, y1, x2, and y2 are updated in the header
 *	for itemPtr.
 *
 *--------------------------------------------------------------
 */

static void
ComputeLineBbox(canvas, linePtr)
    Tk_Canvas canvas;			/* Canvas that contains item. */
    LineItem *linePtr;			/* Item whose bbos is to be
					 * recomputed. */
{
    double *coordPtr;
    int i;

    coordPtr = linePtr->coordPtr;
    linePtr->header.x1 = linePtr->header.x2 = *coordPtr;
    linePtr->header.y1 = linePtr->header.y2 = coordPtr[1];

    /*
     * Compute the bounding box of all the points in the line,
     * then expand in all directions by the line's width to take
     * care of butting or rounded corners and projecting or
     * rounded caps.  This expansion is an overestimate (worst-case
     * is square root of two over two) but it's simple.  Don't do
     * anything special for curves.  This causes an additional
     * overestimate in the bounding box, but is faster.
     */

    for (i = 1, coordPtr = linePtr->coordPtr+2; i < linePtr->numPoints;
	    i++, coordPtr += 2) {
	TkIncludePoint((Tk_Item *) linePtr, coordPtr);
    }
    linePtr->header.x1 -= linePtr->width;
    linePtr->header.x2 += linePtr->width;
    linePtr->header.y1 -= linePtr->width;
    linePtr->header.y2 += linePtr->width;

    /*
     * For mitered lines, make a second pass through all the points.
     * Compute the locations of the two miter vertex points and add
     * those into the bounding box.
     */

    if (linePtr->joinStyle == JoinMiter) {
	for (i = linePtr->numPoints, coordPtr = linePtr->coordPtr; i >= 3;
		i--, coordPtr += 2) {
	    double miter[4];
	    int j;
    
	    if (TkGetMiterPoints(coordPtr, coordPtr+2, coordPtr+4,
		    (double) linePtr->width, miter, miter+2)) {
		for (j = 0; j < 4; j += 2) {
		    TkIncludePoint((Tk_Item *) linePtr, miter+j);
		}
	    }
	}
    }

    /*
     * Add in the sizes of arrowheads, if any.
     */

    if (linePtr->arrow != noneUid) {
	if (linePtr->arrow != lastUid) {
	    for (i = 0, coordPtr = linePtr->firstArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkIncludePoint((Tk_Item *) linePtr, coordPtr);
	    }
	}
	if (linePtr->arrow != firstUid) {
	    for (i = 0, coordPtr = linePtr->lastArrowPtr; i < PTS_IN_ARROW;
		    i++, coordPtr += 2) {
		TkIncludePoint((Tk_Item *) linePtr, coordPtr);
	    }
	}
    }

    /*
     * Add one more pixel of fudge factor just to be safe (e.g.
     * X may round differently than we do).
     */

    linePtr->header.x1 -= 1;
    linePtr->header.x2 += 1;
    linePtr->header.y1 -= 1;
    linePtr->header.y2 += 1;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayLine --
 *
 *	This procedure is invoked to draw a line item in a given
 *	drawable.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	ItemPtr is drawn in drawable using the transformation
 *	information in canvas.
 *
 *--------------------------------------------------------------
 */

static void
DisplayLine(canvas, itemPtr, display, drawable, x, y, width, height)
    Tk_Canvas canvas;			/* Canvas that contains item. */
    Tk_Item *itemPtr;			/* Item to be displayed. */
    Display *display;			/* Display on which to draw item. */
    Drawable drawable;			/* Pixmap or window in which to draw
					 * item. */
    int x, y, width, height;		/* Describes region of canvas that
					 * must be redisplayed (not used). */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    XPoint staticPoints[MAX_STATIC_POINTS];
    XPoint *pointPtr;
    XPoint *pPtr;
    double *coordPtr;
    int i, numPoints;

    if (linePtr->gc == None) {
	return;
    }

    /*
     * Build up an array of points in screen coordinates.  Use a
     * static array unless the line has an enormous number of points;
     * in this case, dynamically allocate an array.  For smoothed lines,
     * generate the curve points on each redisplay.
     */

    if ((linePtr->smooth) && (linePtr->numPoints > 2)) {
	numPoints = 1 + linePtr->numPoints*linePtr->splineSteps;
    } else {
	numPoints = linePtr->numPoints;
    }

    if (numPoints <= MAX_STATIC_POINTS) {
	pointPtr = staticPoints;
    } else {
	pointPtr = (XPoint *) ckalloc((unsigned) (numPoints * sizeof(XPoint)));
    }

    if ((linePtr->smooth) && (linePtr->numPoints > 2)) {
	numPoints = TkMakeBezierCurve(canvas, linePtr->coordPtr,
		linePtr->numPoints, linePtr->splineSteps, pointPtr,
		(double *) NULL);
    } else {
	for (i = 0, coordPtr = linePtr->coordPtr, pPtr = pointPtr;
		i < linePtr->numPoints;  i += 1, coordPtr += 2, pPtr++) {
	    Tk_CanvasDrawableCoords(canvas, coordPtr[0], coordPtr[1],
		    &pPtr->x, &pPtr->y);
	}
    }

    /*
     * Display line, the free up line storage if it was dynamically
     * allocated.  If we're stippling, then modify the stipple offset
     * in the GC.  Be sure to reset the offset when done, since the
     * GC is supposed to be read-only.
     */

    if (linePtr->fillStipple != None) {
	Tk_CanvasSetStippleOrigin(canvas, linePtr->gc);
	Tk_CanvasSetStippleOrigin(canvas, linePtr->arrowGC);
    }
    XDrawLines(display, drawable, linePtr->gc, pointPtr, numPoints,
	    CoordModeOrigin);
    if (pointPtr != staticPoints) {
	ckfree((char *) pointPtr);
    }

    /*
     * Display arrowheads, if they are wanted.
     */

    if (linePtr->firstArrowPtr != NULL) {
	TkFillPolygon(canvas, linePtr->firstArrowPtr, PTS_IN_ARROW,
		display, drawable, linePtr->gc, NULL);
    }
    if (linePtr->lastArrowPtr != NULL) {
	TkFillPolygon(canvas, linePtr->lastArrowPtr, PTS_IN_ARROW,
		display, drawable, linePtr->gc, NULL);
    }
    if (linePtr->fillStipple != None) {
	XSetTSOrigin(display, linePtr->gc, 0, 0);
	XSetTSOrigin(display, linePtr->arrowGC, 0, 0);
    }
}

/*
 *--------------------------------------------------------------
 *
 * LineToPoint --
 *
 *	Computes the distance from a given point to a given
 *	line, in canvas units.
 *
 * Results:
 *	The return value is 0 if the point whose x and y coordinates
 *	are pointPtr[0] and pointPtr[1] is inside the line.  If the
 *	point isn't inside the line then the return value is the
 *	distance from the point to the line.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static double
LineToPoint(canvas, itemPtr, pointPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against point. */
    double *pointPtr;		/* Pointer to x and y coordinates. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    double *coordPtr, *linePoints;
    double staticSpace[2*MAX_STATIC_POINTS];
    double poly[10];
    double bestDist, dist;
    int numPoints, count;
    int changedMiterToBevel;	/* Non-zero means that a mitered corner
				 * had to be treated as beveled after all
				 * because the angle was < 11 degrees. */

    bestDist = 1.0e36;

    /*
     * Handle smoothed lines by generating an expanded set of points
     * against which to do the check.
     */

    if ((linePtr->smooth) && (linePtr->numPoints > 2)) {
	numPoints = 1 + linePtr->numPoints*linePtr->splineSteps;
	if (numPoints <= MAX_STATIC_POINTS) {
	    linePoints = staticSpace;
	} else {
	    linePoints = (double *) ckalloc((unsigned)
		    (2*numPoints*sizeof(double)));
	}
	numPoints = TkMakeBezierCurve(canvas, linePtr->coordPtr,
		linePtr->numPoints, linePtr->splineSteps, (XPoint *) NULL,
		linePoints);
    } else {
	numPoints = linePtr->numPoints;
	linePoints = linePtr->coordPtr;
    }

    /*
     * The overall idea is to iterate through all of the edges of
     * the line, computing a polygon for each edge and testing the
     * point against that polygon.  In addition, there are additional
     * tests to deal with rounded joints and caps.
     */

    changedMiterToBevel = 0;
    for (count = numPoints, coordPtr = linePoints; count >= 2;
	    count--, coordPtr += 2) {

	/*
	 * If rounding is done around the first point then compute
	 * the distance between the point and the point.
	 */

	if (((linePtr->capStyle == CapRound) && (count == numPoints))
		|| ((linePtr->joinStyle == JoinRound)
			&& (count != numPoints))) {
	    dist = hypot(coordPtr[0] - pointPtr[0], coordPtr[1] - pointPtr[1])
		    - linePtr->width/2.0;
	    if (dist <= 0.0) {
		bestDist = 0.0;
		goto done;
	    } else if (dist < bestDist) {
		bestDist = dist;
	    }
	}

	/*
	 * Compute the polygonal shape corresponding to this edge,
	 * consisting of two points for the first point of the edge
	 * and two points for the last point of the edge.
	 */

	if (count == numPoints) {
	    TkGetButtPoints(coordPtr+2, coordPtr, (double) linePtr->width,
		    linePtr->capStyle == CapProjecting, poly, poly+2);
	} else if ((linePtr->joinStyle == JoinMiter) && !changedMiterToBevel) {
	    poly[0] = poly[6];
	    poly[1] = poly[7];
	    poly[2] = poly[4];
	    poly[3] = poly[5];
	} else {
	    TkGetButtPoints(coordPtr+2, coordPtr, (double) linePtr->width, 0,
		    poly, poly+2);

	    /*
	     * If this line uses beveled joints, then check the distance
	     * to a polygon comprising the last two points of the previous
	     * polygon and the first two from this polygon;  this checks
	     * the wedges that fill the mitered joint.
	     */

	    if ((linePtr->joinStyle == JoinBevel) || changedMiterToBevel) {
		poly[8] = poly[0];
		poly[9] = poly[1];
		dist = TkPolygonToPoint(poly, 5, pointPtr);
		if (dist <= 0.0) {
		    bestDist = 0.0;
		    goto done;
		} else if (dist < bestDist) {
		    bestDist = dist;
		}
		changedMiterToBevel = 0;
	    }
	}
	if (count == 2) {
	    TkGetButtPoints(coordPtr, coordPtr+2, (double) linePtr->width,
		    linePtr->capStyle == CapProjecting, poly+4, poly+6);
	} else if (linePtr->joinStyle == JoinMiter) {
	    if (TkGetMiterPoints(coordPtr, coordPtr+2, coordPtr+4,
		    (double) linePtr->width, poly+4, poly+6) == 0) {
		changedMiterToBevel = 1;
		TkGetButtPoints(coordPtr, coordPtr+2, (double) linePtr->width,
			0, poly+4, poly+6);
	    }
	} else {
	    TkGetButtPoints(coordPtr, coordPtr+2, (double) linePtr->width, 0,
		    poly+4, poly+6);
	}
	poly[8] = poly[0];
	poly[9] = poly[1];
	dist = TkPolygonToPoint(poly, 5, pointPtr);
	if (dist <= 0.0) {
	    bestDist = 0.0;
	    goto done;
	} else if (dist < bestDist) {
	    bestDist = dist;
	}
    }

    /*
     * If caps are rounded, check the distance to the cap around the
     * final end point of the line.
     */

    if (linePtr->capStyle == CapRound) {
	dist = hypot(coordPtr[0] - pointPtr[0], coordPtr[1] - pointPtr[1])
		- linePtr->width/2.0;
	if (dist <= 0.0) {
	    bestDist = 0.0;
	    goto done;
	} else if (dist < bestDist) {
	    bestDist = dist;
	}
    }

    /*
     * If there are arrowheads, check the distance to the arrowheads.
     */

    if (linePtr->arrow != noneUid) {
	if (linePtr->arrow != lastUid) {
	    dist = TkPolygonToPoint(linePtr->firstArrowPtr, PTS_IN_ARROW,
		    pointPtr);
	    if (dist <= 0.0) {
		bestDist = 0.0;
		goto done;
	    } else if (dist < bestDist) {
		bestDist = dist;
	    }
	}
	if (linePtr->arrow != firstUid) {
	    dist = TkPolygonToPoint(linePtr->lastArrowPtr, PTS_IN_ARROW,
		    pointPtr);
	    if (dist <= 0.0) {
		bestDist = 0.0;
		goto done;
	    } else if (dist < bestDist) {
		bestDist = dist;
	    }
	}
    }

    done:
    if ((linePoints != staticSpace) && (linePoints != linePtr->coordPtr)) {
	ckfree((char *) linePoints);
    }
    return bestDist;
}

/*
 *--------------------------------------------------------------
 *
 * LineToArea --
 *
 *	This procedure is called to determine whether an item
 *	lies entirely inside, entirely outside, or overlapping
 *	a given rectangular area.
 *
 * Results:
 *	-1 is returned if the item is entirely outside the
 *	area, 0 if it overlaps, and 1 if it is entirely
 *	inside the given area.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
LineToArea(canvas, itemPtr, rectPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against line. */
    double *rectPtr;
{
    LineItem *linePtr = (LineItem *) itemPtr;
    double staticSpace[2*MAX_STATIC_POINTS];
    double *linePoints;
    double radius;
    int numPoints, result;

    radius = linePtr->width/2.0;

    /*
     * Handle smoothed lines by generating an expanded set of points
     * against which to do the check.
     */

    if ((linePtr->smooth) && (linePtr->numPoints > 2)) {
	numPoints = 1 + linePtr->numPoints*linePtr->splineSteps;
	if (numPoints <= MAX_STATIC_POINTS) {
	    linePoints = staticSpace;
	} else {
	    linePoints = (double *) ckalloc((unsigned)
		    (2*numPoints*sizeof(double)));
	}
	numPoints = TkMakeBezierCurve(canvas, linePtr->coordPtr,
		linePtr->numPoints, linePtr->splineSteps, (XPoint *) NULL,
		linePoints);
    } else {
	numPoints = linePtr->numPoints;
	linePoints = linePtr->coordPtr;
    }

    /*
     * Check the segments of the line.
     */

    result = TkThickPolyLineToArea(linePoints, numPoints, 
	    (double) linePtr->width, linePtr->capStyle, linePtr->joinStyle,
	    rectPtr);
    if (result == 0) {
	goto done;
    }

    /*
     * Check arrowheads, if any.
     */

    if (linePtr->arrow != noneUid) {
	if (linePtr->arrow != lastUid) {
	    if (TkPolygonToArea(linePtr->firstArrowPtr, PTS_IN_ARROW,
		    rectPtr) != result) {
		result = 0;
		goto done;
	    }
	}
	if (linePtr->arrow != firstUid) {
	    if (TkPolygonToArea(linePtr->lastArrowPtr, PTS_IN_ARROW,
		    rectPtr) != result) {
		result = 0;
		goto done;
	    }
	}
    }

    done:
    if ((linePoints != staticSpace) && (linePoints != linePtr->coordPtr)) {
	ckfree((char *) linePoints);
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * ScaleLine --
 *
 *	This procedure is invoked to rescale a line item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The line referred to by itemPtr is rescaled so that the
 *	following transformation is applied to all point
 *	coordinates:
 *		x' = originX + scaleX*(x-originX)
 *		y' = originY + scaleY*(y-originY)
 *
 *--------------------------------------------------------------
 */

static void
ScaleLine(canvas, itemPtr, originX, originY, scaleX, scaleY)
    Tk_Canvas canvas;			/* Canvas containing line. */
    Tk_Item *itemPtr;			/* Line to be scaled. */
    double originX, originY;		/* Origin about which to scale rect. */
    double scaleX;			/* Amount to scale in X direction. */
    double scaleY;			/* Amount to scale in Y direction. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    double *coordPtr;
    int i;

    /*
     * Delete any arrowheads before scaling all the points (so that
     * the end-points of the line get restored).
     */

    if (linePtr->firstArrowPtr != NULL) {
	linePtr->coordPtr[0] = linePtr->firstArrowPtr[0];
	linePtr->coordPtr[1] = linePtr->firstArrowPtr[1];
	ckfree((char *) linePtr->firstArrowPtr);
	linePtr->firstArrowPtr = NULL;
    }
    if (linePtr->lastArrowPtr != NULL) {
	int i;

	i = 2*(linePtr->numPoints-1);
	linePtr->coordPtr[i] = linePtr->lastArrowPtr[0];
	linePtr->coordPtr[i+1] = linePtr->lastArrowPtr[1];
	ckfree((char *) linePtr->lastArrowPtr);
	linePtr->lastArrowPtr = NULL;
    }
    for (i = 0, coordPtr = linePtr->coordPtr; i < linePtr->numPoints;
	    i++, coordPtr += 2) {
	coordPtr[0] = originX + scaleX*(*coordPtr - originX);
	coordPtr[1] = originY + scaleY*(coordPtr[1] - originY);
    }
    if (linePtr->arrow != noneUid) {
	ConfigureArrows(canvas, linePtr);
    }
    ComputeLineBbox(canvas, linePtr);
}

/*
 *--------------------------------------------------------------
 *
 * TranslateLine --
 *
 *	This procedure is called to move a line by a given amount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The position of the line is offset by (xDelta, yDelta), and
 *	the bounding box is updated in the generic part of the item
 *	structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslateLine(canvas, itemPtr, deltaX, deltaY)
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item that is being moved. */
    double deltaX, deltaY;		/* Amount by which item is to be
					 * moved. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    double *coordPtr;
    int i;

    for (i = 0, coordPtr = linePtr->coordPtr; i < linePtr->numPoints;
	    i++, coordPtr += 2) {
	coordPtr[0] += deltaX;
	coordPtr[1] += deltaY;
    }
    if (linePtr->firstArrowPtr != NULL) {
	for (i = 0, coordPtr = linePtr->firstArrowPtr; i < PTS_IN_ARROW;
		i++, coordPtr += 2) {
	    coordPtr[0] += deltaX;
	    coordPtr[1] += deltaY;
	}
    }
    if (linePtr->lastArrowPtr != NULL) {
	for (i = 0, coordPtr = linePtr->lastArrowPtr; i < PTS_IN_ARROW;
		i++, coordPtr += 2) {
	    coordPtr[0] += deltaX;
	    coordPtr[1] += deltaY;
	}
    }
    ComputeLineBbox(canvas, linePtr);
}

/*
 *--------------------------------------------------------------
 *
 * ParseArrowShape --
 *
 *	This procedure is called back during option parsing to
 *	parse arrow shape information.
 *
 * Results:
 *	The return value is a standard Tcl result:  TCL_OK means
 *	that the arrow shape information was parsed ok, and
 *	TCL_ERROR means it couldn't be parsed.
 *
 * Side effects:
 *	Arrow information in recordPtr is updated.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
ParseArrowShape(clientData, interp, tkwin, value, recordPtr, offset)
    ClientData clientData;	/* Not used. */
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tk_Window tkwin;		/* Not used. */
    char *value;		/* Textual specification of arrow shape. */
    char *recordPtr;		/* Pointer to item record in which to
				 * store arrow information. */
    int offset;			/* Offset of shape information in widget
				 * record. */
{
    LineItem *linePtr = (LineItem *) recordPtr;
    double a, b, c;
    int argc;
    char **argv = NULL;

    if (offset != Tk_Offset(LineItem, arrowShapeA)) {
	panic("ParseArrowShape received bogus offset");
    }

    if (Tcl_SplitList(interp, value, &argc, &argv) != TCL_OK) {
	syntaxError:
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "bad arrow shape \"", value,
		"\": must be list with three numbers", (char *) NULL);
	if (argv != NULL) {
	    ckfree((char *) argv);
	}
	return TCL_ERROR;
    }
    if (argc != 3) {
	goto syntaxError;
    }
    if ((Tk_CanvasGetCoord(interp, linePtr->canvas, argv[0], &a) != TCL_OK)
	    || (Tk_CanvasGetCoord(interp, linePtr->canvas, argv[1], &b)
		!= TCL_OK)
	    || (Tk_CanvasGetCoord(interp, linePtr->canvas, argv[2], &c)
		!= TCL_OK)) {
	goto syntaxError;
    }
    linePtr->arrowShapeA = a;
    linePtr->arrowShapeB = b;
    linePtr->arrowShapeC = c;
    ckfree((char *) argv);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * PrintArrowShape --
 *
 *	This procedure is a callback invoked by the configuration
 *	code to return a printable value describing an arrow shape.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

    /* ARGSUSED */
static char *
PrintArrowShape(clientData, tkwin, recordPtr, offset, freeProcPtr)
    ClientData clientData;	/* Not used. */
    Tk_Window tkwin;		/* Window associated with linePtr's widget. */
    char *recordPtr;		/* Pointer to item record containing current
				 * shape information. */
    int offset;			/* Offset of arrow information in record. */
    Tcl_FreeProc **freeProcPtr;	/* Store address of procedure to call to
				 * free string here. */
{
    LineItem *linePtr = (LineItem *) recordPtr;
    char *buffer;

    buffer = (char *) ckalloc(120);
    sprintf(buffer, "%.5g %.5g %.5g", linePtr->arrowShapeA,
	    linePtr->arrowShapeB, linePtr->arrowShapeC);
    *freeProcPtr = TCL_DYNAMIC;
    return buffer;
}

/*
 *--------------------------------------------------------------
 *
 * ConfigureArrows --
 *
 *	If arrowheads have been requested for a line, this
 *	procedure makes arrangements for the arrowheads.
 *
 * Results:
 *	Always returns TCL_OK.
 *
 * Side effects:
 *	Information in linePtr is set up for one or two arrowheads.
 *	the firstArrowPtr and lastArrowPtr polygons are allocated
 *	and initialized, if need be, and the end points of the line
 *	are adjusted so that a thick line doesn't stick out past
 *	the arrowheads.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
ConfigureArrows(canvas, linePtr)
    Tk_Canvas canvas;			/* Canvas in which arrows will be
					 * displayed (interp and tkwin
					 * fields are needed). */
    LineItem *linePtr;			/* Item to configure for arrows. */
{
    double *poly, *coordPtr;
    double dx, dy, length, sinTheta, cosTheta, temp;
    double fracHeight;			/* Line width as fraction of
					 * arrowhead width. */
    double backup;			/* Distance to backup end points
					 * so the line ends in the middle
					 * of the arrowhead. */
    double vertX, vertY;		/* Position of arrowhead vertex. */
    double shapeA, shapeB, shapeC;	/* Adjusted coordinates (see
					 * explanation below). */

    /*
     * The code below makes a tiny increase in the shape parameters
     * for the line.  This is a bit of a hack, but it seems to result
     * in displays that more closely approximate the specified parameters.
     * Without the adjustment, the arrows come out smaller than expected.
     */

    shapeA = linePtr->arrowShapeA + 0.001;
    shapeB = linePtr->arrowShapeB + 0.001;
    shapeC = linePtr->arrowShapeC + linePtr->width/2.0 + 0.001;

    /*
     * If there's an arrowhead on the first point of the line, compute
     * its polygon and adjust the first point of the line so that the
     * line doesn't stick out past the leading edge of the arrowhead.
     */

    fracHeight = (linePtr->width/2.0)/shapeC;
    backup = fracHeight*shapeB + shapeA*(1.0 - fracHeight)/2.0;
    if (linePtr->arrow != lastUid) {
	poly = linePtr->firstArrowPtr;
	if (poly == NULL) {
	    poly = (double *) ckalloc((unsigned)
		    (2*PTS_IN_ARROW*sizeof(double)));
	    poly[0] = poly[10] = linePtr->coordPtr[0];
	    poly[1] = poly[11] = linePtr->coordPtr[1];
	    linePtr->firstArrowPtr = poly;
	}
	dx = poly[0] - linePtr->coordPtr[2];
	dy = poly[1] - linePtr->coordPtr[3];
	length = hypot(dx, dy);
	if (length == 0) {
	    sinTheta = cosTheta = 0.0;
	} else {
	    sinTheta = dy/length;
	    cosTheta = dx/length;
	}
	vertX = poly[0] - shapeA*cosTheta;
	vertY = poly[1] - shapeA*sinTheta;
	temp = shapeC*sinTheta;
	poly[2] = poly[0] - shapeB*cosTheta + temp;
	poly[8] = poly[2] - 2*temp;
	temp = shapeC*cosTheta;
	poly[3] = poly[1] - shapeB*sinTheta - temp;
	poly[9] = poly[3] + 2*temp;
	poly[4] = poly[2]*fracHeight + vertX*(1.0-fracHeight);
	poly[5] = poly[3]*fracHeight + vertY*(1.0-fracHeight);
	poly[6] = poly[8]*fracHeight + vertX*(1.0-fracHeight);
	poly[7] = poly[9]*fracHeight + vertY*(1.0-fracHeight);

	/*
	 * Polygon done.  Now move the first point towards the second so
	 * that the corners at the end of the line are inside the
	 * arrowhead.
	 */

	linePtr->coordPtr[0] = poly[0] - backup*cosTheta;
	linePtr->coordPtr[1] = poly[1] - backup*sinTheta;
    }

    /*
     * Similar arrowhead calculation for the last point of the line.
     */

    if (linePtr->arrow != firstUid) {
	coordPtr = linePtr->coordPtr + 2*(linePtr->numPoints-2);
	poly = linePtr->lastArrowPtr;
	if (poly == NULL) {
	    poly = (double *) ckalloc((unsigned)
		    (2*PTS_IN_ARROW*sizeof(double)));
	    poly[0] = poly[10] = coordPtr[2];
	    poly[1] = poly[11] = coordPtr[3];
	    linePtr->lastArrowPtr = poly;
	}
	dx = poly[0] - coordPtr[0];
	dy = poly[1] - coordPtr[1];
	length = hypot(dx, dy);
	if (length == 0) {
	    sinTheta = cosTheta = 0.0;
	} else {
	    sinTheta = dy/length;
	    cosTheta = dx/length;
	}
	vertX = poly[0] - shapeA*cosTheta;
	vertY = poly[1] - shapeA*sinTheta;
	temp = shapeC*sinTheta;
	poly[2] = poly[0] - shapeB*cosTheta + temp;
	poly[8] = poly[2] - 2*temp;
	temp = shapeC*cosTheta;
	poly[3] = poly[1] - shapeB*sinTheta - temp;
	poly[9] = poly[3] + 2*temp;
	poly[4] = poly[2]*fracHeight + vertX*(1.0-fracHeight);
	poly[5] = poly[3]*fracHeight + vertY*(1.0-fracHeight);
	poly[6] = poly[8]*fracHeight + vertX*(1.0-fracHeight);
	poly[7] = poly[9]*fracHeight + vertY*(1.0-fracHeight);
	coordPtr[2] = poly[0] - backup*cosTheta;
	coordPtr[3] = poly[1] - backup*sinTheta;
    }

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * LineToPostscript --
 *
 *	This procedure is called to generate Postscript for
 *	line items.
 *
 * Results:
 *	The return value is a standard Tcl result.  If an error
 *	occurs in generating Postscript then an error message is
 *	left in interp->result, replacing whatever used
 *	to be there.  If no error occurs, then Postscript for the
 *	item is appended to the result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
LineToPostscript(interp, canvas, itemPtr, prepass)
    Tcl_Interp *interp;			/* Leave Postscript or error message
					 * here. */
    Tk_Canvas canvas;			/* Information about overall canvas. */
    Tk_Item *itemPtr;			/* Item for which Postscript is
					 * wanted. */
    int prepass;			/* 1 means this is a prepass to
					 * collect font information;  0 means
					 * final Postscript is being created. */
{
    LineItem *linePtr = (LineItem *) itemPtr;
    char buffer[200];
    char *style;

    if (linePtr->fg == NULL) {
	return TCL_OK;
    }

    /*
     * Generate a path for the line's center-line (do this differently
     * for straight lines and smoothed lines).
     */

    if ((!linePtr->smooth) || (linePtr->numPoints <= 2)) {
	Tk_CanvasPsPath(interp, canvas, linePtr->coordPtr, linePtr->numPoints);
    } else {
	if (linePtr->fillStipple == None) {
	    TkMakeBezierPostscript(interp, canvas, linePtr->coordPtr,
		    linePtr->numPoints);
	} else {
	    /*
	     * Special hack: Postscript printers don't appear to be able
	     * to turn a path drawn with "curveto"s into a clipping path
	     * without exceeding resource limits, so TkMakeBezierPostscript
	     * won't work for stippled curves.  Instead, generate all of
	     * the intermediate points here and output them into the
	     * Postscript file with "lineto"s instead.
	     */

	    double staticPoints[2*MAX_STATIC_POINTS];
	    double *pointPtr;
	    int numPoints;

	    numPoints = 1 + linePtr->numPoints*linePtr->splineSteps;
	    pointPtr = staticPoints;
	    if (numPoints > MAX_STATIC_POINTS) {
		pointPtr = (double *) ckalloc((unsigned)
			(numPoints * 2 * sizeof(double)));
	    }
	    numPoints = TkMakeBezierCurve(canvas, linePtr->coordPtr,
		    linePtr->numPoints, linePtr->splineSteps, (XPoint *) NULL,
		    pointPtr);
	    Tk_CanvasPsPath(interp, canvas, pointPtr, numPoints);
	    if (pointPtr != staticPoints) {
		ckfree((char *) pointPtr);
	    }
	}
    }

    /*
     * Set other line-drawing parameters and stroke out the line.
     */

    sprintf(buffer, "%d setlinewidth\n", linePtr->width);
    Tcl_AppendResult(interp, buffer, (char *) NULL);
    style = "0 setlinecap\n";
    if (linePtr->capStyle == CapRound) {
	style = "1 setlinecap\n";
    } else if (linePtr->capStyle == CapProjecting) {
	style = "2 setlinecap\n";
    }
    Tcl_AppendResult(interp, style, (char *) NULL);
    style = "0 setlinejoin\n";
    if (linePtr->joinStyle == JoinRound) {
	style = "1 setlinejoin\n";
    } else if (linePtr->joinStyle == JoinBevel) {
	style = "2 setlinejoin\n";
    }
    Tcl_AppendResult(interp, style, (char *) NULL);
    if (Tk_CanvasPsColor(interp, canvas, linePtr->fg) != TCL_OK) {
	return TCL_ERROR;
    };
    if (linePtr->fillStipple != None) {
	Tcl_AppendResult(interp, "StrokeClip ", (char *) NULL);
	if (Tk_CanvasPsStipple(interp, canvas, linePtr->fillStipple)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	Tcl_AppendResult(interp, "stroke\n", (char *) NULL);
    }

    /*
     * Output polygons for the arrowheads, if there are any.
     */

    if (linePtr->firstArrowPtr != NULL) {
	if (linePtr->fillStipple != None) {
	    Tcl_AppendResult(interp, "grestore gsave\n",
		    (char *) NULL);
	}
	if (ArrowheadPostscript(interp, canvas, linePtr,
		linePtr->firstArrowPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    if (linePtr->lastArrowPtr != NULL) {
	if (linePtr->fillStipple != None) {
	    Tcl_AppendResult(interp, "grestore gsave\n", (char *) NULL);
	}
	if (ArrowheadPostscript(interp, canvas, linePtr,
		linePtr->lastArrowPtr) != TCL_OK) {
	    return TCL_ERROR;
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ArrowheadPostscript --
 *
 *	This procedure is called to generate Postscript for
 *	an arrowhead for a line item.
 *
 * Results:
 *	The return value is a standard Tcl result.  If an error
 *	occurs in generating Postscript then an error message is
 *	left in interp->result, replacing whatever used
 *	to be there.  If no error occurs, then Postscript for the
 *	arrowhead is appended to the result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
ArrowheadPostscript(interp, canvas, linePtr, arrowPtr)
    Tcl_Interp *interp;			/* Leave Postscript or error message
					 * here. */
    Tk_Canvas canvas;			/* Information about overall canvas. */
    LineItem *linePtr;			/* Line item for which Postscript is
					 * being generated. */
    double *arrowPtr;			/* Pointer to first of five points
					 * describing arrowhead polygon. */
{
    Tk_CanvasPsPath(interp, canvas, arrowPtr, PTS_IN_ARROW);
    if (linePtr->fillStipple != None) {
	Tcl_AppendResult(interp, "clip ", (char *) NULL);
	if (Tk_CanvasPsStipple(interp, canvas, linePtr->fillStipple)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
	Tcl_AppendResult(interp, "fill\n", (char *) NULL);
    }
    return TCL_OK;
}

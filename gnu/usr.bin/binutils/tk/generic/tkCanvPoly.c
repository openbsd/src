/* 
 * tkCanvPoly.c --
 *
 *	This file implements polygon items for canvas widgets.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkCanvPoly.c 1.34 96/02/15 18:52:32
 */

#include <stdio.h>
#include "tkInt.h"
#include "tkPort.h"

/*
 * The structure below defines the record for each polygon item.
 */

typedef struct PolygonItem  {
    Tk_Item header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
    int numPoints;		/* Number of points in polygon (always >= 3).
				 * Polygon is always closed. */
    int pointsAllocated;	/* Number of points for which space is
				 * allocated at *coordPtr. */
    double *coordPtr;		/* Pointer to malloc-ed array containing
				 * x- and y-coords of all points in polygon.
				 * X-coords are even-valued indices, y-coords
				 * are corresponding odd-valued indices. */
    int width;			/* Width of outline. */
    XColor *outlineColor;	/* Color for outline. */
    GC outlineGC;		/* Graphics context for drawing outline. */
    XColor *fillColor;		/* Foreground color for polygon. */
    Pixmap fillStipple;		/* Stipple bitmap for filling polygon. */
    GC fillGC;			/* Graphics context for filling polygon. */
    int smooth;			/* Non-zero means draw shape smoothed (i.e.
				 * with Bezier splines). */
    int splineSteps;		/* Number of steps in each spline segment. */
} PolygonItem;

/*
 * Information used for parsing configuration specs:
 */

static Tk_CustomOption tagsOption = {Tk_CanvasTagsParseProc,
    Tk_CanvasTagsPrintProc, (ClientData) NULL
};

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_COLOR, "-fill", (char *) NULL, (char *) NULL,
	"black", Tk_Offset(PolygonItem, fillColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_COLOR, "-outline", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(PolygonItem, outlineColor), TK_CONFIG_NULL_OK},
    {TK_CONFIG_BOOLEAN, "-smooth", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(PolygonItem, smooth), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_INT, "-splinesteps", (char *) NULL, (char *) NULL,
	"12", Tk_Offset(PolygonItem, splineSteps), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BITMAP, "-stipple", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(PolygonItem, fillStipple), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
	(char *) NULL, 0, TK_CONFIG_NULL_OK, &tagsOption},
    {TK_CONFIG_PIXELS, "-width", (char *) NULL, (char *) NULL,
	"1", Tk_Offset(PolygonItem, width), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputePolygonBbox _ANSI_ARGS_((Tk_Canvas canvas,
			    PolygonItem *polyPtr));
static int		ConfigurePolygon _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
			    char **argv, int flags));
static int		CreatePolygon _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, struct Tk_Item *itemPtr,
			    int argc, char **argv));
static void		DeletePolygon _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr,  Display *display));
static void		DisplayPolygon _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, Display *display, Drawable dst,
			    int x, int y, int width, int height));
static int		PolygonCoords _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr,
			    int argc, char **argv));
static int		PolygonToArea _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double *rectPtr));
static double		PolygonToPoint _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double *pointPtr));
static int		PolygonToPostscript _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, int prepass));
static void		ScalePolygon _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double originX, double originY,
			    double scaleX, double scaleY));
static void		TranslatePolygon _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double deltaX, double deltaY));

/*
 * The structures below defines the polygon item type by means
 * of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkPolygonType = {
    "polygon",				/* name */
    sizeof(PolygonItem),		/* itemSize */
    CreatePolygon,			/* createProc */
    configSpecs,			/* configSpecs */
    ConfigurePolygon,			/* configureProc */
    PolygonCoords,			/* coordProc */
    DeletePolygon,			/* deleteProc */
    DisplayPolygon,			/* displayProc */
    0,					/* alwaysRedraw */
    PolygonToPoint,			/* pointProc */
    PolygonToArea,			/* areaProc */
    PolygonToPostscript,		/* postscriptProc */
    ScalePolygon,			/* scaleProc */
    TranslatePolygon,			/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL		/* nextPtr */
};

/*
 * The definition below determines how large are static arrays
 * used to hold spline points (splines larger than this have to
 * have their arrays malloc-ed).
 */

#define MAX_STATIC_POINTS 200

/*
 *--------------------------------------------------------------
 *
 * CreatePolygon --
 *
 *	This procedure is invoked to create a new polygon item in
 *	a canvas.
 *
 * Results:
 *	A standard Tcl return value.  If an error occurred in
 *	creating the item, then an error message is left in
 *	interp->result;  in this case itemPtr is
 *	left uninitialized, so it can be safely freed by the
 *	caller.
 *
 * Side effects:
 *	A new polygon item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreatePolygon(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    Tk_Canvas canvas;			/* Canvas to hold new item. */
    Tk_Item *itemPtr;			/* Record to hold new item;  header
					 * has been initialized by caller. */
    int argc;				/* Number of arguments in argv. */
    char **argv;			/* Arguments describing polygon. */
{
    PolygonItem *polyPtr = (PolygonItem *) itemPtr;
    int i;

    if (argc < 6) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tk_PathName(Tk_CanvasTkwin(canvas)), " create ",
		itemPtr->typePtr->name,
		" x1 y1 x2 y2 x3 y3 ?x4 y4 ...? ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Carry out initialization that is needed in order to clean
     * up after errors during the the remainder of this procedure.
     */

    polyPtr->numPoints = 0;
    polyPtr->pointsAllocated = 0;
    polyPtr->coordPtr = NULL;
    polyPtr->width = 1;
    polyPtr->outlineColor = NULL;
    polyPtr->outlineGC = None;
    polyPtr->fillColor = NULL;
    polyPtr->fillStipple = None;
    polyPtr->fillGC = None;
    polyPtr->smooth = 0;
    polyPtr->splineSteps = 12;

    /*
     * Count the number of points and then parse them into a point
     * array.  Leading arguments are assumed to be points if they
     * start with a digit or a minus sign followed by a digit.
     */

    for (i = 4; i < (argc-1); i+=2) {
	if ((!isdigit(UCHAR(argv[i][0]))) &&
		((argv[i][0] != '-') || (!isdigit(UCHAR(argv[i][1]))))) {
	    break;
	}
    }
    if (PolygonCoords(interp, canvas, itemPtr, i, argv) != TCL_OK) {
	goto error;
    }

    if (ConfigurePolygon(interp, canvas, itemPtr, argc-i, argv+i, 0)
	    == TCL_OK) {
	return TCL_OK;
    }

    error:
    DeletePolygon(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
    return TCL_ERROR;
}

/*
 *--------------------------------------------------------------
 *
 * PolygonCoords --
 *
 *	This procedure is invoked to process the "coords" widget
 *	command on polygons.  See the user documentation for details
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
PolygonCoords(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item whose coordinates are to be
					 * read or modified. */
    int argc;				/* Number of coordinates supplied in
					 * argv. */
    char **argv;			/* Array of coordinates: x1, y1,
					 * x2, y2, ... */
{
    PolygonItem *polyPtr = (PolygonItem *) itemPtr;
    char buffer[TCL_DOUBLE_SPACE];
    int i, numPoints;

    if (argc == 0) {
	for (i = 0; i < 2*polyPtr->numPoints; i++) {
	    Tcl_PrintDouble(interp, polyPtr->coordPtr[i], buffer);
	    Tcl_AppendElement(interp, buffer);
	}
    } else if (argc < 6) {
	Tcl_AppendResult(interp,
		"too few coordinates for polygon: must have at least 6",
		(char *) NULL);
	return TCL_ERROR;
    } else if (argc & 1) {
	Tcl_AppendResult(interp,
		"odd number of coordinates specified for polygon",
		(char *) NULL);
	return TCL_ERROR;
    } else {
	numPoints = argc/2;
	if (polyPtr->pointsAllocated <= numPoints) {
	    if (polyPtr->coordPtr != NULL) {
		ckfree((char *) polyPtr->coordPtr);
	    }

	    /*
	     * One extra point gets allocated here, just in case we have
	     * to add another point to close the polygon.
	     */

	    polyPtr->coordPtr = (double *) ckalloc((unsigned)
		    (sizeof(double) * (argc+2)));
	    polyPtr->pointsAllocated = numPoints+1;
	}
	for (i = argc-1; i >= 0; i--) {
	    if (Tk_CanvasGetCoord(interp, canvas, argv[i],
		    &polyPtr->coordPtr[i]) != TCL_OK) {
		return TCL_ERROR;
	    }
	}
	polyPtr->numPoints = numPoints;
    
	/*
	 * Close the polygon if it isn't already closed.
	 */
    
	if ((polyPtr->coordPtr[argc-2] != polyPtr->coordPtr[0])
		|| (polyPtr->coordPtr[argc-1] != polyPtr->coordPtr[1])) {
	    polyPtr->numPoints++;
	    polyPtr->coordPtr[argc] = polyPtr->coordPtr[0];
	    polyPtr->coordPtr[argc+1] = polyPtr->coordPtr[1];
	}
	ComputePolygonBbox(canvas, polyPtr);
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ConfigurePolygon --
 *
 *	This procedure is invoked to configure various aspects
 *	of a polygon item such as its background color.
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
ConfigurePolygon(interp, canvas, itemPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Tk_Canvas canvas;		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr;		/* Polygon item to reconfigure. */
    int argc;			/* Number of elements in argv.  */
    char **argv;		/* Arguments describing things to configure. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    PolygonItem *polyPtr = (PolygonItem *) itemPtr;
    XGCValues gcValues;
    GC newGC;
    unsigned long mask;
    Tk_Window tkwin;

    tkwin = Tk_CanvasTkwin(canvas);
    if (Tk_ConfigureWidget(interp, tkwin, configSpecs, argc, argv,
	    (char *) polyPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few of the options require additional processing, such as
     * graphics contexts.
     */

    if (polyPtr->width < 1) {
	polyPtr->width = 1;
    }
    if (polyPtr->outlineColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = polyPtr->outlineColor->pixel;
	gcValues.line_width = polyPtr->width;
	gcValues.cap_style = CapRound;
	gcValues.join_style = JoinRound;
	mask = GCForeground|GCLineWidth|GCCapStyle|GCJoinStyle;
	newGC = Tk_GetGC(tkwin, mask, &gcValues);
    }
    if (polyPtr->outlineGC != None) {
	Tk_FreeGC(Tk_Display(tkwin), polyPtr->outlineGC);
    }
    polyPtr->outlineGC = newGC;

    if (polyPtr->fillColor == NULL) {
	newGC = None;
    } else {
	gcValues.foreground = polyPtr->fillColor->pixel;
	mask = GCForeground;
	if (polyPtr->fillStipple != None) {
	    gcValues.stipple = polyPtr->fillStipple;
	    gcValues.fill_style = FillStippled;
	    mask |= GCStipple|GCFillStyle;
	}
	newGC = Tk_GetGC(tkwin, mask, &gcValues);
    }
    if (polyPtr->fillGC != None) {
	Tk_FreeGC(Tk_Display(tkwin), polyPtr->fillGC);
    }
    polyPtr->fillGC = newGC;

    /*
     * Keep spline parameters within reasonable limits.
     */

    if (polyPtr->splineSteps < 1) {
	polyPtr->splineSteps = 1;
    } else if (polyPtr->splineSteps > 100) {
	polyPtr->splineSteps = 100;
    }

    ComputePolygonBbox(canvas, polyPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DeletePolygon --
 *
 *	This procedure is called to clean up the data structure
 *	associated with a polygon item.
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
DeletePolygon(canvas, itemPtr, display)
    Tk_Canvas canvas;			/* Info about overall canvas widget. */
    Tk_Item *itemPtr;			/* Item that is being deleted. */
    Display *display;			/* Display containing window for
					 * canvas. */
{
    PolygonItem *polyPtr = (PolygonItem *) itemPtr;

    if (polyPtr->coordPtr != NULL) {
	ckfree((char *) polyPtr->coordPtr);
    }
    if (polyPtr->fillColor != NULL) {
	Tk_FreeColor(polyPtr->fillColor);
    }
    if (polyPtr->fillStipple != None) {
	Tk_FreeBitmap(display, polyPtr->fillStipple);
    }
    if (polyPtr->outlineColor != NULL) {
	Tk_FreeColor(polyPtr->outlineColor);
    }
    if (polyPtr->outlineGC != None) {
	Tk_FreeGC(display, polyPtr->outlineGC);
    }
    if (polyPtr->fillGC != None) {
	Tk_FreeGC(display, polyPtr->fillGC);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ComputePolygonBbox --
 *
 *	This procedure is invoked to compute the bounding box of
 *	all the pixels that may be drawn as part of a polygon.
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
ComputePolygonBbox(canvas, polyPtr)
    Tk_Canvas canvas;			/* Canvas that contains item. */
    PolygonItem *polyPtr;		/* Item whose bbox is to be
					 * recomputed. */
{
    double *coordPtr;
    int i;

    coordPtr = polyPtr->coordPtr;
    polyPtr->header.x1 = polyPtr->header.x2 = *coordPtr;
    polyPtr->header.y1 = polyPtr->header.y2 = coordPtr[1];

    for (i = 1, coordPtr = polyPtr->coordPtr+2; i < polyPtr->numPoints;
	    i++, coordPtr += 2) {
	TkIncludePoint((Tk_Item *) polyPtr, coordPtr);
    }

    /*
     * Expand bounding box in all directions to account for the outline,
     * which can stick out beyond the polygon.  Add one extra pixel of
     * fudge, just in case X rounds differently than we do.
     */

    i = (polyPtr->width+1)/2 + 1;
    polyPtr->header.x1 -= i;
    polyPtr->header.x2 += i;
    polyPtr->header.y1 -= i;
    polyPtr->header.y2 += i;
}

/*
 *--------------------------------------------------------------
 *
 * TkFillPolygon --
 *
 *	This procedure is invoked to convert a polygon to screen
 *	coordinates and display it using a particular GC.
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

void
TkFillPolygon(canvas, coordPtr, numPoints, display, drawable, gc, outlineGC)
    Tk_Canvas canvas;			/* Canvas whose coordinate system
					 * is to be used for drawing. */
    double *coordPtr;			/* Array of coordinates for polygon:
					 * x1, y1, x2, y2, .... */
    int numPoints;			/* Twice this many coordinates are
					 * present at *coordPtr. */
    Display *display;			/* Display on which to draw polygon. */
    Drawable drawable;			/* Pixmap or window in which to draw
					 * polygon. */
    GC gc;				/* Graphics context for drawing. */
    GC outlineGC;			/* If not None, use this to draw an
					 * outline around the polygon after
					 * filling it. */
{
    XPoint staticPoints[MAX_STATIC_POINTS];
    XPoint *pointPtr;
    XPoint *pPtr;
    int i;

    /*
     * Build up an array of points in screen coordinates.  Use a
     * static array unless the polygon has an enormous number of points;
     * in this case, dynamically allocate an array.
     */

    if (numPoints <= MAX_STATIC_POINTS) {
	pointPtr = staticPoints;
    } else {
	pointPtr = (XPoint *) ckalloc((unsigned) (numPoints * sizeof(XPoint)));
    }

    for (i = 0, pPtr = pointPtr; i < numPoints; i += 1, coordPtr += 2, pPtr++) {
	Tk_CanvasDrawableCoords(canvas, coordPtr[0], coordPtr[1], &pPtr->x,
		&pPtr->y);
    }

    /*
     * Display polygon, then free up polygon storage if it was dynamically
     * allocated.
     */

    if (gc != None) {
	XFillPolygon(display, drawable, gc, pointPtr, numPoints, Complex,
		CoordModeOrigin);
    }
    if (outlineGC != None) {
	XDrawLines(display, drawable, outlineGC, pointPtr,
	    numPoints, CoordModeOrigin);
    }
    if (pointPtr != staticPoints) {
	ckfree((char *) pointPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * DisplayPolygon --
 *
 *	This procedure is invoked to draw a polygon item in a given
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
DisplayPolygon(canvas, itemPtr, display, drawable, x, y, width, height)
    Tk_Canvas canvas;			/* Canvas that contains item. */
    Tk_Item *itemPtr;			/* Item to be displayed. */
    Display *display;			/* Display on which to draw item. */
    Drawable drawable;			/* Pixmap or window in which to draw
					 * item. */
    int x, y, width, height;		/* Describes region of canvas that
					 * must be redisplayed (not used). */
{
    PolygonItem *polyPtr = (PolygonItem *) itemPtr;

    if ((polyPtr->fillGC == None) && (polyPtr->outlineGC == None)) {
	return;
    }

    /*
     * If we're stippling then modify the stipple offset in the GC.  Be
     * sure to reset the offset when done, since the GC is supposed to be
     * read-only.
     */

    if ((polyPtr->fillStipple != None) && (polyPtr->fillGC != None)) {
	Tk_CanvasSetStippleOrigin(canvas, polyPtr->fillGC);
    }

    if (!polyPtr->smooth) {
	TkFillPolygon(canvas, polyPtr->coordPtr, polyPtr->numPoints,
		display, drawable, polyPtr->fillGC, polyPtr->outlineGC);
    } else {
	int numPoints;
	XPoint staticPoints[MAX_STATIC_POINTS];
	XPoint *pointPtr;

	/*
	 * This is a smoothed polygon.  Display using a set of generated
	 * spline points rather than the original points.
	 */

	numPoints = 1 + polyPtr->numPoints*polyPtr->splineSteps;
	if (numPoints <= MAX_STATIC_POINTS) {
	    pointPtr = staticPoints;
	} else {
	    pointPtr = (XPoint *) ckalloc((unsigned)
		    (numPoints * sizeof(XPoint)));
	}
	numPoints = TkMakeBezierCurve(canvas, polyPtr->coordPtr,
		polyPtr->numPoints, polyPtr->splineSteps, pointPtr,
		(double *) NULL);
	if (polyPtr->fillGC != None) {
	    XFillPolygon(display, drawable, polyPtr->fillGC, pointPtr,
		    numPoints, Complex, CoordModeOrigin);
	}
	if (polyPtr->outlineGC != None) {
	    XDrawLines(display, drawable, polyPtr->outlineGC, pointPtr,
		    numPoints, CoordModeOrigin);
	}
	if (pointPtr != staticPoints) {
	    ckfree((char *) pointPtr);
	}
    }
    if ((polyPtr->fillStipple != None) && (polyPtr->fillGC != None)) {
	XSetTSOrigin(display, polyPtr->fillGC, 0, 0);
    }
}

/*
 *--------------------------------------------------------------
 *
 * PolygonToPoint --
 *
 *	Computes the distance from a given point to a given
 *	polygon, in canvas units.
 *
 * Results:
 *	The return value is 0 if the point whose x and y coordinates
 *	are pointPtr[0] and pointPtr[1] is inside the polygon.  If the
 *	point isn't inside the polygon then the return value is the
 *	distance from the point to the polygon.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static double
PolygonToPoint(canvas, itemPtr, pointPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against point. */
    double *pointPtr;		/* Pointer to x and y coordinates. */
{
    PolygonItem *polyPtr = (PolygonItem *) itemPtr;
    double *coordPtr, distance;
    double staticSpace[2*MAX_STATIC_POINTS];
    int numPoints;

    if (!polyPtr->smooth) {
	distance = TkPolygonToPoint(polyPtr->coordPtr, polyPtr->numPoints,
		pointPtr);
    } else {
	/*
	 * Smoothed polygon.  Generate a new set of points and use them
	 * for comparison.
	 */
    
	numPoints = 1 + polyPtr->numPoints*polyPtr->splineSteps;
	if (numPoints <= MAX_STATIC_POINTS) {
	    coordPtr = staticSpace;
	} else {
	    coordPtr = (double *) ckalloc((unsigned)
		    (2*numPoints*sizeof(double)));
	}
	numPoints = TkMakeBezierCurve(canvas, polyPtr->coordPtr,
		polyPtr->numPoints, polyPtr->splineSteps, (XPoint *) NULL,
		coordPtr);
	distance = TkPolygonToPoint(coordPtr, numPoints, pointPtr);
	if (coordPtr != staticSpace) {
	    ckfree((char *) coordPtr);
	}
    }
    if (polyPtr->outlineColor != NULL) {
	distance -= polyPtr->width/2.0;
	if (distance < 0) {
	    distance = 0;
	}
    }
    return distance;
}

/*
 *--------------------------------------------------------------
 *
 * PolygonToArea --
 *
 *	This procedure is called to determine whether an item
 *	lies entirely inside, entirely outside, or overlapping
 *	a given rectangular area.
 *
 * Results:
 *	-1 is returned if the item is entirely outside the area
 *	given by rectPtr, 0 if it overlaps, and 1 if it is entirely
 *	inside the given area.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static int
PolygonToArea(canvas, itemPtr, rectPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against polygon. */
    double *rectPtr;		/* Pointer to array of four coordinates
				 * (x1, y1, x2, y2) describing rectangular
				 * area.  */
{
    PolygonItem *polyPtr = (PolygonItem *) itemPtr;
    double *coordPtr, rect2[4], halfWidth;
    double staticSpace[2*MAX_STATIC_POINTS];
    int numPoints, result;

    /*
     * Handle smoothed polygons by generating an expanded set of points
     * against which to do the check.
     */

    if (polyPtr->smooth) {
	numPoints = 1 + polyPtr->numPoints*polyPtr->splineSteps;
	if (numPoints <= MAX_STATIC_POINTS) {
	    coordPtr = staticSpace;
	} else {
	    coordPtr = (double *) ckalloc((unsigned)
		    (2*numPoints*sizeof(double)));
	}
	numPoints = TkMakeBezierCurve(canvas, polyPtr->coordPtr,
		polyPtr->numPoints, polyPtr->splineSteps, (XPoint *) NULL,
		coordPtr);
    } else {
	numPoints = polyPtr->numPoints;
	coordPtr = polyPtr->coordPtr;
    }

    if (polyPtr->width <= 1) {
	/*
	 * The outline of the polygon doesn't stick out, so we can
	 * do a simple check.
	 */

	result = TkPolygonToArea(coordPtr, numPoints, rectPtr);
    } else {
	/*
	 * The polygon has a wide outline, so the check is more complicated.
	 * First, check the line segments to see if they overlap the area.
	 */

	result = TkThickPolyLineToArea(coordPtr, numPoints, 
	    (double) polyPtr->width, CapRound, JoinRound, rectPtr);
	if (result >= 0) {
	    goto done;
	}

	/*
	 * There is no overlap between the polygon's outline and the
	 * rectangle.  This means either the rectangle is entirely outside
	 * the polygon or entirely inside.  To tell the difference,
	 * see whether the polygon (with 0 outline width) overlaps the
	 * rectangle bloated by half the outline width.
	 */

	halfWidth = polyPtr->width/2.0;
	rect2[0] = rectPtr[0] - halfWidth;
	rect2[1] = rectPtr[1] - halfWidth;
	rect2[2] = rectPtr[2] + halfWidth;
	rect2[3] = rectPtr[3] + halfWidth;
	if (TkPolygonToArea(coordPtr, numPoints, rect2) == -1) {
	    result = -1;
	} else {
	    result = 0;
	}
    }

    done:
    if ((coordPtr != staticSpace) && (coordPtr != polyPtr->coordPtr)) {
	ckfree((char *) coordPtr);
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * ScalePolygon --
 *
 *	This procedure is invoked to rescale a polygon item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The polygon referred to by itemPtr is rescaled so that the
 *	following transformation is applied to all point
 *	coordinates:
 *		x' = originX + scaleX*(x-originX)
 *		y' = originY + scaleY*(y-originY)
 *
 *--------------------------------------------------------------
 */

static void
ScalePolygon(canvas, itemPtr, originX, originY, scaleX, scaleY)
    Tk_Canvas canvas;			/* Canvas containing polygon. */
    Tk_Item *itemPtr;			/* Polygon to be scaled. */
    double originX, originY;		/* Origin about which to scale rect. */
    double scaleX;			/* Amount to scale in X direction. */
    double scaleY;			/* Amount to scale in Y direction. */
{
    PolygonItem *polyPtr = (PolygonItem *) itemPtr;
    double *coordPtr;
    int i;

    for (i = 0, coordPtr = polyPtr->coordPtr; i < polyPtr->numPoints;
	    i++, coordPtr += 2) {
	*coordPtr = originX + scaleX*(*coordPtr - originX);
	coordPtr[1] = originY + scaleY*(coordPtr[1] - originY);
    }
    ComputePolygonBbox(canvas, polyPtr);
}

/*
 *--------------------------------------------------------------
 *
 * TranslatePolygon --
 *
 *	This procedure is called to move a polygon by a given
 *	amount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The position of the polygon is offset by (xDelta, yDelta),
 *	and the bounding box is updated in the generic part of the
 *	item structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslatePolygon(canvas, itemPtr, deltaX, deltaY)
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item that is being moved. */
    double deltaX, deltaY;		/* Amount by which item is to be
					 * moved. */
{
    PolygonItem *polyPtr = (PolygonItem *) itemPtr;
    double *coordPtr;
    int i;

    for (i = 0, coordPtr = polyPtr->coordPtr; i < polyPtr->numPoints;
	    i++, coordPtr += 2) {
	*coordPtr += deltaX;
	coordPtr[1] += deltaY;
    }
    ComputePolygonBbox(canvas, polyPtr);
}

/*
 *--------------------------------------------------------------
 *
 * PolygonToPostscript --
 *
 *	This procedure is called to generate Postscript for
 *	polygon items.
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
PolygonToPostscript(interp, canvas, itemPtr, prepass)
    Tcl_Interp *interp;			/* Leave Postscript or error message
					 * here. */
    Tk_Canvas canvas;			/* Information about overall canvas. */
    Tk_Item *itemPtr;			/* Item for which Postscript is
					 * wanted. */
    int prepass;			/* 1 means this is a prepass to
					 * collect font information;  0 means
					 * final Postscript is being created. */
{
    char string[100];
    PolygonItem *polyPtr = (PolygonItem *) itemPtr;

    /*
     * Fill the area of the polygon.
     */

    if (polyPtr->fillColor != NULL) {
	if (!polyPtr->smooth) {
	    Tk_CanvasPsPath(interp, canvas, polyPtr->coordPtr,
		    polyPtr->numPoints);
	} else {
	    TkMakeBezierPostscript(interp, canvas, polyPtr->coordPtr,
		    polyPtr->numPoints);
	}
	if (Tk_CanvasPsColor(interp, canvas, polyPtr->fillColor) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (polyPtr->fillStipple != None) {
	    Tcl_AppendResult(interp, "eoclip ", (char *) NULL);
	    if (Tk_CanvasPsStipple(interp, canvas, polyPtr->fillStipple)
		    != TCL_OK) {
		return TCL_ERROR;
	    }
	    if (polyPtr->outlineColor != NULL) {
		Tcl_AppendResult(interp, "grestore gsave\n", (char *) NULL);
	    }
	} else {
	    Tcl_AppendResult(interp, "eofill\n", (char *) NULL);
	}
    }

    /*
     * Now draw the outline, if there is one.
     */

    if (polyPtr->outlineColor != NULL) {
	if (!polyPtr->smooth) {
	    Tk_CanvasPsPath(interp, canvas, polyPtr->coordPtr,
		polyPtr->numPoints);
	} else {
	    TkMakeBezierPostscript(interp, canvas, polyPtr->coordPtr,
		polyPtr->numPoints);
	}

	sprintf(string, "%d setlinewidth\n", polyPtr->width);
	Tcl_AppendResult(interp, string,
		"1 setlinecap\n1 setlinejoin\n", (char *) NULL);
	if (Tk_CanvasPsColor(interp, canvas, polyPtr->outlineColor)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	Tcl_AppendResult(interp, "stroke\n", (char *) NULL);
    }
    return TCL_OK;
}

/* 
 * tkCanvWind.c --
 *
 *	This file implements window items for canvas widgets.
 *
 * Copyright (c) 1992-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkCanvWind.c 1.25 96/02/17 16:59:13
 */

#include <stdio.h>
#include "tkInt.h"
#include "tkPort.h"
#include "tkCanvas.h"

/*
 * The structure below defines the record for each window item.
 */

typedef struct WindowItem  {
    Tk_Item header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
    double x, y;		/* Coordinates of positioning point for
				 * window. */
    Tk_Window tkwin;		/* Window associated with item.  NULL means
				 * window has been destroyed. */
    int width;			/* Width to use for window (<= 0 means use
				 * window's requested width). */
    int height;			/* Width to use for window (<= 0 means use
				 * window's requested width). */
    Tk_Anchor anchor;		/* Where to anchor window relative to
				 * (x,y). */
    Tk_Canvas canvas;		/* Canvas containing this item. */
} WindowItem;

/*
 * Information used for parsing configuration specs:
 */

static Tk_CustomOption tagsOption = {Tk_CanvasTagsParseProc,
    Tk_CanvasTagsPrintProc, (ClientData) NULL
};

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_ANCHOR, "-anchor", (char *) NULL, (char *) NULL,
	"center", Tk_Offset(WindowItem, anchor), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_PIXELS, "-height", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(WindowItem, height), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
	(char *) NULL, 0, TK_CONFIG_NULL_OK, &tagsOption},
    {TK_CONFIG_PIXELS, "-width", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(WindowItem, width), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_WINDOW, "-window", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(WindowItem, tkwin), TK_CONFIG_NULL_OK},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputeWindowBbox _ANSI_ARGS_((Tk_Canvas canvas,
			    WindowItem *winItemPtr));
static int		ConfigureWinItem _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
			    char **argv, int flags));
static int		CreateWinItem _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, struct Tk_Item *itemPtr,
			    int argc, char **argv));
static void		DeleteWinItem _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, Display *display));
static void		DisplayWinItem _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, Display *display, Drawable dst,
			    int x, int y, int width, int height));
static void		ScaleWinItem _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double originX, double originY,
			    double scaleX, double scaleY));
static void		TranslateWinItem _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double deltaX, double deltaY));
static int		WinItemCoords _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
			    char **argv));
static void		WinItemLostSlaveProc _ANSI_ARGS_((
			    ClientData clientData, Tk_Window tkwin));
static void		WinItemRequestProc _ANSI_ARGS_((ClientData clientData,
			    Tk_Window tkwin));
static void		WinItemStructureProc _ANSI_ARGS_((
			    ClientData clientData, XEvent *eventPtr));
static int		WinItemToArea _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double *rectPtr));
static double		WinItemToPoint _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double *pointPtr));

/*
 * The structure below defines the window item type by means of procedures
 * that can be invoked by generic item code.
 */

Tk_ItemType tkWindowType = {
    "window",				/* name */
    sizeof(WindowItem),			/* itemSize */
    CreateWinItem,			/* createProc */
    configSpecs,			/* configSpecs */
    ConfigureWinItem,			/* configureProc */
    WinItemCoords,			/* coordProc */
    DeleteWinItem,			/* deleteProc */
    DisplayWinItem,			/* displayProc */
    1,					/* alwaysRedraw */
    WinItemToPoint,			/* pointProc */
    WinItemToArea,			/* areaProc */
    (Tk_ItemPostscriptProc *) NULL,	/* postscriptProc */
    ScaleWinItem,			/* scaleProc */
    TranslateWinItem,			/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* cursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL		/* nextPtr */
};


/*
 * The structure below defines the official type record for the
 * placer:
 */

static Tk_GeomMgr canvasGeomType = {
    "canvas",				/* name */
    WinItemRequestProc,			/* requestProc */
    WinItemLostSlaveProc,		/* lostSlaveProc */
};

/*
 *--------------------------------------------------------------
 *
 * CreateWinItem --
 *
 *	This procedure is invoked to create a new window
 *	item in a canvas.
 *
 * Results:
 *	A standard Tcl return value.  If an error occurred in
 *	creating the item, then an error message is left in
 *	interp->result;  in this case itemPtr is
 *	left uninitialized, so it can be safely freed by the
 *	caller.
 *
 * Side effects:
 *	A new window item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreateWinItem(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    Tk_Canvas canvas;			/* Canvas to hold new item. */
    Tk_Item *itemPtr;			/* Record to hold new item;  header
					 * has been initialized by caller. */
    int argc;				/* Number of arguments in argv. */
    char **argv;			/* Arguments describing rectangle. */
{
    WindowItem *winItemPtr = (WindowItem *) itemPtr;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tk_PathName(Tk_CanvasTkwin(canvas)), " create ",
		itemPtr->typePtr->name, " x y ?options?\"",
		(char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Initialize item's record.
     */

    winItemPtr->tkwin = NULL;
    winItemPtr->width = 0;
    winItemPtr->height = 0;
    winItemPtr->anchor = TK_ANCHOR_CENTER;
    winItemPtr->canvas = canvas;

    /*
     * Process the arguments to fill in the item record.
     */

    if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &winItemPtr->x) != TCL_OK)
	    || (Tk_CanvasGetCoord(interp, canvas, argv[1],
		&winItemPtr->y) != TCL_OK)) {
	return TCL_ERROR;
    }

    if (ConfigureWinItem(interp, canvas, itemPtr, argc-2, argv+2, 0)
	    != TCL_OK) {
	DeleteWinItem(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * WinItemCoords --
 *
 *	This procedure is invoked to process the "coords" widget
 *	command on window items.  See the user documentation for
 *	details on what it does.
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
WinItemCoords(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item whose coordinates are to be
					 * read or modified. */
    int argc;				/* Number of coordinates supplied in
					 * argv. */
    char **argv;			/* Array of coordinates: x1, y1,
					 * x2, y2, ... */
{
    WindowItem *winItemPtr = (WindowItem *) itemPtr;
    char x[TCL_DOUBLE_SPACE], y[TCL_DOUBLE_SPACE];

    if (argc == 0) {
	Tcl_PrintDouble(interp, winItemPtr->x, x);
	Tcl_PrintDouble(interp, winItemPtr->y, y);
	Tcl_AppendResult(interp, x, " ", y, (char *) NULL);
    } else if (argc == 2) {
	if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &winItemPtr->x)
		!= TCL_OK) || (Tk_CanvasGetCoord(interp, canvas, argv[1],
		&winItemPtr->y) != TCL_OK)) {
	    return TCL_ERROR;
	}
	ComputeWindowBbox(canvas, winItemPtr);
    } else {
	sprintf(interp->result,
		"wrong # coordinates: expected 0 or 2, got %d", argc);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ConfigureWinItem --
 *
 *	This procedure is invoked to configure various aspects
 *	of a window item, such as its anchor position.
 *
 * Results:
 *	A standard Tcl result code.  If an error occurs, then
 *	an error message is left in interp->result.
 *
 * Side effects:
 *	Configuration information may be set for itemPtr.
 *
 *--------------------------------------------------------------
 */

static int
ConfigureWinItem(interp, canvas, itemPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tk_Canvas canvas;		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr;		/* Window item to reconfigure. */
    int argc;			/* Number of elements in argv.  */
    char **argv;		/* Arguments describing things to configure. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    WindowItem *winItemPtr = (WindowItem *) itemPtr;
    Tk_Window oldWindow;
    Tk_Window canvasTkwin;

    oldWindow = winItemPtr->tkwin;
    canvasTkwin = Tk_CanvasTkwin(canvas);
    if (Tk_ConfigureWidget(interp, canvasTkwin, configSpecs, argc, argv,
	    (char *) winItemPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few of the options require additional processing.
     */

    if (oldWindow != winItemPtr->tkwin) {
	if (oldWindow != NULL) {
	    Tk_DeleteEventHandler(oldWindow, StructureNotifyMask,
		    WinItemStructureProc, (ClientData) winItemPtr);
	    Tk_ManageGeometry(oldWindow, (Tk_GeomMgr *) NULL,
		    (ClientData) NULL);
	    Tk_UnmaintainGeometry(oldWindow, canvasTkwin);
	    Tk_UnmapWindow(oldWindow);
	}
	if (winItemPtr->tkwin != NULL) {
	    Tk_Window ancestor, parent;

	    /*
	     * Make sure that the canvas is either the parent of the
	     * window associated with the item or a descendant of that
	     * parent.  Also, don't allow a top-level window to be
	     * managed inside a canvas.
	     */

	    parent = Tk_Parent(winItemPtr->tkwin);
	    for (ancestor = canvasTkwin; ;
		    ancestor = Tk_Parent(ancestor)) {
		if (ancestor == parent) {
		    break;
		}
		if (((Tk_FakeWin *) (ancestor))->flags & TK_TOP_LEVEL) {
		    badWindow:
		    Tcl_AppendResult(interp, "can't use ",
			    Tk_PathName(winItemPtr->tkwin),
			    " in a window item of this canvas", (char *) NULL);
		    winItemPtr->tkwin = NULL;
		    return TCL_ERROR;
		}
	    }
	    if (((Tk_FakeWin *) (winItemPtr->tkwin))->flags & TK_TOP_LEVEL) {
		goto badWindow;
	    }
	    if (winItemPtr->tkwin == canvasTkwin) {
		goto badWindow;
	    }
	    Tk_CreateEventHandler(winItemPtr->tkwin, StructureNotifyMask,
		    WinItemStructureProc, (ClientData) winItemPtr);
	    Tk_ManageGeometry(winItemPtr->tkwin, &canvasGeomType,
		    (ClientData) winItemPtr);
	}
    }

    ComputeWindowBbox(canvas, winItemPtr);

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DeleteWinItem --
 *
 *	This procedure is called to clean up the data structure
 *	associated with a window item.
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
DeleteWinItem(canvas, itemPtr, display)
    Tk_Canvas canvas;			/* Overall info about widget. */
    Tk_Item *itemPtr;			/* Item that is being deleted. */
    Display *display;			/* Display containing window for
					 * canvas. */
{
    WindowItem *winItemPtr = (WindowItem *) itemPtr;
    Tk_Window canvasTkwin = Tk_CanvasTkwin(canvas);

    if (winItemPtr->tkwin != NULL) {
	Tk_DeleteEventHandler(winItemPtr->tkwin, StructureNotifyMask,
		WinItemStructureProc, (ClientData) winItemPtr);
	Tk_ManageGeometry(winItemPtr->tkwin, (Tk_GeomMgr *) NULL,
		(ClientData) NULL);
	if (canvasTkwin != Tk_Parent(winItemPtr->tkwin)) {
	    Tk_UnmaintainGeometry(winItemPtr->tkwin, canvasTkwin);
	}
	Tk_UnmapWindow(winItemPtr->tkwin);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ComputeWindowBbox --
 *
 *	This procedure is invoked to compute the bounding box of
 *	all the pixels that may be drawn as part of a window item.
 *	This procedure is where the child window's placement is
 *	computed.
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
ComputeWindowBbox(canvas, winItemPtr)
    Tk_Canvas canvas;			/* Canvas that contains item. */
    WindowItem *winItemPtr;		/* Item whose bbox is to be
					 * recomputed. */
{
    int width, height, x, y;

    x = winItemPtr->x + ((winItemPtr->x >= 0) ? 0.5 : - 0.5);
    y = winItemPtr->y + ((winItemPtr->y >= 0) ? 0.5 : - 0.5);

    if (winItemPtr->tkwin == NULL) {
	winItemPtr->header.x1 = winItemPtr->header.x2 = x;
	winItemPtr->header.y1 = winItemPtr->header.y2 = y;
	return;
    }

    /*
     * Compute dimensions of window.
     */

    width = winItemPtr->width;
    if (width <= 0) {
	width = Tk_ReqWidth(winItemPtr->tkwin);
	if (width <= 0) {
	    width = 1;
	}
    }
    height = winItemPtr->height;
    if (height <= 0) {
	height = Tk_ReqHeight(winItemPtr->tkwin);
	if (height <= 0) {
	    height = 1;
	}
    }

    /*
     * Compute location of window, using anchor information.
     */

    switch (winItemPtr->anchor) {
	case TK_ANCHOR_N:
	    x -= width/2;
	    break;
	case TK_ANCHOR_NE:
	    x -= width;
	    break;
	case TK_ANCHOR_E:
	    x -= width;
	    y -= height/2;
	    break;
	case TK_ANCHOR_SE:
	    x -= width;
	    y -= height;
	    break;
	case TK_ANCHOR_S:
	    x -= width/2;
	    y -= height;
	    break;
	case TK_ANCHOR_SW:
	    y -= height;
	    break;
	case TK_ANCHOR_W:
	    y -= height/2;
	    break;
	case TK_ANCHOR_NW:
	    break;
	case TK_ANCHOR_CENTER:
	    x -= width/2;
	    y -= height/2;
	    break;
    }

    /*
     * Store the information in the item header.
     */

    winItemPtr->header.x1 = x;
    winItemPtr->header.y1 = y;
    winItemPtr->header.x2 = x + width;
    winItemPtr->header.y2 = y + height;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayWinItem --
 *
 *	This procedure is invoked to "draw" a window item in a given
 *	drawable.  Since the window draws itself, we needn't do any
 *	actual redisplay here.  However, this procedure takes care
 *	of actually repositioning the child window so that it occupies
 *	the correct screen position.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The child window's position may get changed.  Note: this
 *	procedure gets called both when a window needs to be displayed
 *	and when it ceases to be visible on the screen (e.g. it was
 *	scrolled or moved off-screen or the enclosing canvas is
 *	unmapped).
 *
 *--------------------------------------------------------------
 */

static void
DisplayWinItem(canvas, itemPtr, display, drawable, regionX, regionY,
	regionWidth, regionHeight)
    Tk_Canvas canvas;			/* Canvas that contains item. */
    Tk_Item *itemPtr;			/* Item to be displayed. */
    Display *display;			/* Display on which to draw item. */
    Drawable drawable;			/* Pixmap or window in which to draw
					 * item. */
    int regionX, regionY, regionWidth, regionHeight;
					/* Describes region of canvas that
					 * must be redisplayed (not used). */
{
    WindowItem *winItemPtr = (WindowItem *) itemPtr;
    int width, height;
    short x, y;
    Tk_Window canvasTkwin = Tk_CanvasTkwin(canvas);

    if (winItemPtr->tkwin == NULL) {
	return;
    }

    Tk_CanvasWindowCoords(canvas, (double) winItemPtr->header.x1,
	    (double) winItemPtr->header.y1, &x, &y);
    width = winItemPtr->header.x2 - winItemPtr->header.x1;
    height = winItemPtr->header.y2 - winItemPtr->header.y1;

    /*
     * Reposition and map the window (but in different ways depending
     * on whether the canvas is the window's parent).
     */

    if (canvasTkwin == Tk_Parent(winItemPtr->tkwin)) {
	if ((x != Tk_X(winItemPtr->tkwin)) || (y != Tk_Y(winItemPtr->tkwin))
		|| (width != Tk_Width(winItemPtr->tkwin))
		|| (height != Tk_Height(winItemPtr->tkwin))) {
	    Tk_MoveResizeWindow(winItemPtr->tkwin, x, y, width, height);
	}
	Tk_MapWindow(winItemPtr->tkwin);
    } else {
	Tk_MaintainGeometry(winItemPtr->tkwin, canvasTkwin, x, y,
		width, height);
    }
}

/*
 *--------------------------------------------------------------
 *
 * WinItemToPoint --
 *
 *	Computes the distance from a given point to a given
 *	rectangle, in canvas units.
 *
 * Results:
 *	The return value is 0 if the point whose x and y coordinates
 *	are coordPtr[0] and coordPtr[1] is inside the window.  If the
 *	point isn't inside the window then the return value is the
 *	distance from the point to the window.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static double
WinItemToPoint(canvas, itemPtr, pointPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against point. */
    double *pointPtr;		/* Pointer to x and y coordinates. */
{
    WindowItem *winItemPtr = (WindowItem *) itemPtr;
    double x1, x2, y1, y2, xDiff, yDiff;

    x1 = winItemPtr->header.x1;
    y1 = winItemPtr->header.y1;
    x2 = winItemPtr->header.x2;
    y2 = winItemPtr->header.y2;

    /*
     * Point is outside rectangle.
     */

    if (pointPtr[0] < x1) {
	xDiff = x1 - pointPtr[0];
    } else if (pointPtr[0] >= x2)  {
	xDiff = pointPtr[0] + 1 - x2;
    } else {
	xDiff = 0;
    }

    if (pointPtr[1] < y1) {
	yDiff = y1 - pointPtr[1];
    } else if (pointPtr[1] >= y2)  {
	yDiff = pointPtr[1] + 1 - y2;
    } else {
	yDiff = 0;
    }

    return hypot(xDiff, yDiff);
}

/*
 *--------------------------------------------------------------
 *
 * WinItemToArea --
 *
 *	This procedure is called to determine whether an item
 *	lies entirely inside, entirely outside, or overlapping
 *	a given rectangle.
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

static int
WinItemToArea(canvas, itemPtr, rectPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against rectangle. */
    double *rectPtr;		/* Pointer to array of four coordinates
				 * (x1, y1, x2, y2) describing rectangular
				 * area.  */
{
    WindowItem *winItemPtr = (WindowItem *) itemPtr;

    if ((rectPtr[2] <= winItemPtr->header.x1)
	    || (rectPtr[0] >= winItemPtr->header.x2)
	    || (rectPtr[3] <= winItemPtr->header.y1)
	    || (rectPtr[1] >= winItemPtr->header.y2)) {
	return -1;
    }
    if ((rectPtr[0] <= winItemPtr->header.x1)
	    && (rectPtr[1] <= winItemPtr->header.y1)
	    && (rectPtr[2] >= winItemPtr->header.x2)
	    && (rectPtr[3] >= winItemPtr->header.y2)) {
	return 1;
    }
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * ScaleWinItem --
 *
 *	This procedure is invoked to rescale a rectangle or oval
 *	item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The rectangle or oval referred to by itemPtr is rescaled
 *	so that the following transformation is applied to all
 *	point coordinates:
 *		x' = originX + scaleX*(x-originX)
 *		y' = originY + scaleY*(y-originY)
 *
 *--------------------------------------------------------------
 */

static void
ScaleWinItem(canvas, itemPtr, originX, originY, scaleX, scaleY)
    Tk_Canvas canvas;			/* Canvas containing rectangle. */
    Tk_Item *itemPtr;			/* Rectangle to be scaled. */
    double originX, originY;		/* Origin about which to scale rect. */
    double scaleX;			/* Amount to scale in X direction. */
    double scaleY;			/* Amount to scale in Y direction. */
{
    WindowItem *winItemPtr = (WindowItem *) itemPtr;

    winItemPtr->x = originX + scaleX*(winItemPtr->x - originX);
    winItemPtr->y = originY + scaleY*(winItemPtr->y - originY);
    if (winItemPtr->width > 0) {
	winItemPtr->width = scaleY*winItemPtr->width;
    }
    if (winItemPtr->height > 0) {
	winItemPtr->height = scaleY*winItemPtr->height;
    }
    ComputeWindowBbox(canvas, winItemPtr);
}

/*
 *--------------------------------------------------------------
 *
 * TranslateWinItem --
 *
 *	This procedure is called to move a rectangle or oval by a
 *	given amount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The position of the rectangle or oval is offset by
 *	(xDelta, yDelta), and the bounding box is updated in the
 *	generic part of the item structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslateWinItem(canvas, itemPtr, deltaX, deltaY)
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item that is being moved. */
    double deltaX, deltaY;		/* Amount by which item is to be
					 * moved. */
{
    WindowItem *winItemPtr = (WindowItem *) itemPtr;

    winItemPtr->x += deltaX;
    winItemPtr->y += deltaY;
    ComputeWindowBbox(canvas, winItemPtr);
}

/*
 *--------------------------------------------------------------
 *
 * WinItemStructureProc --
 *
 *	This procedure is invoked whenever StructureNotify events
 *	occur for a window that's managed as part of a canvas window
 *	item.  This procudure's only purpose is to clean up when
 *	windows are deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is disassociated from the window item when it is
 *	deleted.
 *
 *--------------------------------------------------------------
 */

static void
WinItemStructureProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to record describing window item. */
    XEvent *eventPtr;		/* Describes what just happened. */
{
    WindowItem *winItemPtr = (WindowItem *) clientData;

    if (eventPtr->type == DestroyNotify) {
	winItemPtr->tkwin = NULL;
    }
}

/*
 *--------------------------------------------------------------
 *
 * WinItemRequestProc --
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

static void
WinItemRequestProc(clientData, tkwin)
    ClientData clientData;		/* Pointer to record for window item. */
    Tk_Window tkwin;			/* Window that changed its desired
					 * size. */
{
    WindowItem *winItemPtr = (WindowItem *) clientData;

    ComputeWindowBbox(winItemPtr->canvas, winItemPtr);
    DisplayWinItem(winItemPtr->canvas, (Tk_Item *) winItemPtr,
	    (Display *) NULL, (Drawable) None, 0, 0, 0, 0);
}

/*
 *--------------------------------------------------------------
 *
 * WinItemLostSlaveProc --
 *
 *	This procedure is invoked by Tk whenever some other geometry
 *	claims control over a slave that used to be managed by us.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Forgets all canvas-related information about the slave.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
WinItemLostSlaveProc(clientData, tkwin)
    ClientData clientData;	/* WindowItem structure for slave window that
				 * was stolen away. */
    Tk_Window tkwin;		/* Tk's handle for the slave window. */
{
    WindowItem *winItemPtr = (WindowItem *) clientData;
    Tk_Window canvasTkwin = Tk_CanvasTkwin(winItemPtr->canvas);

    Tk_DeleteEventHandler(winItemPtr->tkwin, StructureNotifyMask,
	    WinItemStructureProc, (ClientData) winItemPtr);
    if (canvasTkwin != Tk_Parent(winItemPtr->tkwin)) {
	Tk_UnmaintainGeometry(winItemPtr->tkwin, canvasTkwin);
    }
    Tk_UnmapWindow(winItemPtr->tkwin);
    winItemPtr->tkwin = NULL;
}

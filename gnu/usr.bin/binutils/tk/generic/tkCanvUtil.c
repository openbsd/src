/* 
 * tkCanvUtil.c --
 *
 *	This procedure contains a collection of utility procedures
 *	used by the implementations of various canvas item types.
 *
 * Copyright (c) 1994 Sun Microsystems, Inc.
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkCanvUtil.c 1.6 96/02/15 18:53:10
 */

#include "tk.h"
#include "tkCanvas.h"
#include "tkPort.h"


/*
 *----------------------------------------------------------------------
 *
 * Tk_CanvasTkwin --
 *
 *	Given a token for a canvas, this procedure returns the
 *	widget that represents the canvas.
 *
 * Results:
 *	The return value is a handle for the widget.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
Tk_CanvasTkwin(canvas)
    Tk_Canvas canvas;			/* Token for the canvas. */
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    return canvasPtr->tkwin;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CanvasDrawableCoords --
 *
 *	Given an (x,y) coordinate pair within a canvas, this procedure
 *	returns the corresponding coordinates at which the point should
 *	be drawn in the drawable used for display.
 *
 * Results:
 *	There is no return value.  The values at *drawableXPtr and
 *	*drawableYPtr are filled in with the coordinates at which
 *	x and y should be drawn.  These coordinates are clipped
 *	to fit within a "short", since this is what X uses in
 *	most cases for drawing.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tk_CanvasDrawableCoords(canvas, x, y, drawableXPtr, drawableYPtr)
    Tk_Canvas canvas;			/* Token for the canvas. */
    double x, y;			/* Coordinates in canvas space. */
    short *drawableXPtr, *drawableYPtr;	/* Screen coordinates are stored
					 * here. */
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    double tmp;

    tmp = x - canvasPtr->drawableXOrigin;
    if (tmp > 0) {
	tmp += 0.5;
    } else {
	tmp -= 0.5;
    }
    if (tmp > 32767) {
	*drawableXPtr = 32767;
    } else if (tmp < -32768) {
	*drawableXPtr = -32768;
    } else {
	*drawableXPtr = tmp;
    }

    tmp = y  - canvasPtr->drawableYOrigin;
    if (tmp > 0) {
	tmp += 0.5;
    } else {
	tmp -= 0.5;
    }
    if (tmp > 32767) {
	*drawableYPtr = 32767;
    } else if (tmp < -32768) {
	*drawableYPtr = -32768;
    } else {
	*drawableYPtr = tmp;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CanvasWindowCoords --
 *
 *	Given an (x,y) coordinate pair within a canvas, this procedure
 *	returns the corresponding coordinates in the canvas's window.
 *
 * Results:
 *	There is no return value.  The values at *screenXPtr and
 *	*screenYPtr are filled in with the coordinates at which
 *	(x,y) appears in the canvas's window.  These coordinates
 *	are clipped to fit within a "short", since this is what X
 *	uses in most cases for drawing.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tk_CanvasWindowCoords(canvas, x, y, screenXPtr, screenYPtr)
    Tk_Canvas canvas;			/* Token for the canvas. */
    double x, y;			/* Coordinates in canvas space. */
    short *screenXPtr, *screenYPtr;	/* Screen coordinates are stored
					 * here. */
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    double tmp;

    tmp = x - canvasPtr->xOrigin;
    if (tmp > 0) {
	tmp += 0.5;
    } else {
	tmp -= 0.5;
    }
    if (tmp > 32767) {
	*screenXPtr = 32767;
    } else if (tmp < -32768) {
	*screenXPtr = -32768;
    } else {
	*screenXPtr = tmp;
    }

    tmp = y  - canvasPtr->yOrigin;
    if (tmp > 0) {
	tmp += 0.5;
    } else {
	tmp -= 0.5;
    }
    if (tmp > 32767) {
	*screenYPtr = 32767;
    } else if (tmp < -32768) {
	*screenYPtr = -32768;
    } else {
	*screenYPtr = tmp;
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasGetCoord --
 *
 *	Given a string, returns a floating-point canvas coordinate
 *	corresponding to that string.
 *
 * Results:
 *	The return value is a standard Tcl return result.  If
 *	TCL_OK is returned, then everything went well and the
 *	canvas coordinate is stored at *doublePtr;  otherwise
 *	TCL_ERROR is returned and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

int
Tk_CanvasGetCoord(interp, canvas, string, doublePtr)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Tk_Canvas canvas;		/* Canvas to which coordinate applies. */
    char *string;		/* Describes coordinate (any screen
				 * coordinate form may be used here). */
    double *doublePtr;		/* Place to store converted coordinate. */
{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;
    if (Tk_GetScreenMM(canvasPtr->interp, canvasPtr->tkwin, string,
	    doublePtr) != TCL_OK) {
	return TCL_ERROR;
    }
    *doublePtr *= canvasPtr->pixelsPerMM;
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CanvasSetStippleOrigin --
 *
 *	This procedure sets the stipple origin in a graphics context
 *	so that stipples drawn with the GC will line up with other
 *	stipples previously drawn in the canvas.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The graphics context is modified.
 *
 *----------------------------------------------------------------------
 */

void
Tk_CanvasSetStippleOrigin(canvas, gc)
    Tk_Canvas canvas;		/* Token for a canvas. */
    GC gc;			/* Graphics context that is about to be
				 * used to draw a stippled pattern as
				 * part of redisplaying the canvas. */

{
    TkCanvas *canvasPtr = (TkCanvas *) canvas;

    XSetTSOrigin(canvasPtr->display, gc, -canvasPtr->drawableXOrigin,
	    -canvasPtr->drawableYOrigin);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CanvasGetTextInfo --
 *
 *	This procedure returns a pointer to a structure containing
 *	information about the selection and insertion cursor for
 *	a canvas widget.  Items such as text items save the pointer
 *	and use it to share access to the information with the generic
 *	canvas code.
 *
 * Results:
 *	The return value is a pointer to the structure holding text
 *	information for the canvas.  Most of the fields should not
 *	be modified outside the generic canvas code;  see the user
 *	documentation for details.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_CanvasTextInfo *
Tk_CanvasGetTextInfo(canvas)
    Tk_Canvas canvas;			/* Token for the canvas widget. */
{
    return &((TkCanvas *) canvas)->textInfo;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasTagsParseProc --
 *
 *	This procedure is invoked during option processing to handle
 *	"-tags" options for canvas items.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	The tags for a given item get replaced by those indicated
 *	in the value argument.
 *
 *--------------------------------------------------------------
 */

int
Tk_CanvasTagsParseProc(clientData, interp, tkwin, value, widgRec, offset)
    ClientData clientData;		/* Not used.*/
    Tcl_Interp *interp;			/* Used for reporting errors. */
    Tk_Window tkwin;			/* Window containing canvas widget. */
    char *value;			/* Value of option (list of tag
					 * names). */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Offset into item (ignored). */
{
    register Tk_Item *itemPtr = (Tk_Item *) widgRec;
    int argc, i;
    char **argv;
    Tk_Uid *newPtr;

    /*
     * Break the value up into the individual tag names.
     */

    if (Tcl_SplitList(interp, value, &argc, &argv) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Make sure that there's enough space in the item to hold the
     * tag names.
     */

    if (itemPtr->tagSpace < argc) {
	newPtr = (Tk_Uid *) ckalloc((unsigned) (argc * sizeof(Tk_Uid)));
	for (i = itemPtr->numTags-1; i >= 0; i--) {
	    newPtr[i] = itemPtr->tagPtr[i];
	}
	if (itemPtr->tagPtr != itemPtr->staticTagSpace) {
	    ckfree((char *) itemPtr->tagPtr);
	}
	itemPtr->tagPtr = newPtr;
	itemPtr->tagSpace = argc;
    }
    itemPtr->numTags = argc;
    for (i = 0; i < argc; i++) {
	itemPtr->tagPtr[i] = Tk_GetUid(argv[i]);
    }
    ckfree((char *) argv);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CanvasTagsPrintProc --
 *
 *	This procedure is invoked by the Tk configuration code
 *	to produce a printable string for the "-tags" configuration
 *	option for canvas items.
 *
 * Results:
 *	The return value is a string describing all the tags for
 *	the item referred to by "widgRec".  In addition, *freeProcPtr
 *	is filled in with the address of a procedure to call to free
 *	the result string when it's no longer needed (or NULL to
 *	indicate that the string doesn't need to be freed).
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

char *
Tk_CanvasTagsPrintProc(clientData, tkwin, widgRec, offset, freeProcPtr)
    ClientData clientData;		/* Ignored. */
    Tk_Window tkwin;			/* Window containing canvas widget. */
    char *widgRec;			/* Pointer to record for item. */
    int offset;				/* Ignored. */
    Tcl_FreeProc **freeProcPtr;		/* Pointer to variable to fill in with
					 * information about how to reclaim
					 * storage for return string. */
{
    register Tk_Item *itemPtr = (Tk_Item *) widgRec;

    if (itemPtr->numTags == 0) {
	*freeProcPtr = (Tcl_FreeProc *) NULL;
	return "";
    }
    if (itemPtr->numTags == 1) {
	*freeProcPtr = (Tcl_FreeProc *) NULL;
	return (char *) itemPtr->tagPtr[0];
    }
    *freeProcPtr = TCL_DYNAMIC;
    return Tcl_Merge(itemPtr->numTags, (char **) itemPtr->tagPtr);
}

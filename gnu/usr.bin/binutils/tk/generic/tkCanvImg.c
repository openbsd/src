/* 
 * tkCanvImg.c --
 *
 *	This file implements image items for canvas widgets.
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkCanvImg.c 1.17 96/02/17 17:18:43
 */

#include <stdio.h>
#include "tkInt.h"
#include "tkPort.h"
#include "tkCanvas.h"

/*
 * The structure below defines the record for each image item.
 */

typedef struct ImageItem  {
    Tk_Item header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_Canvas canvas;		/* Canvas containing the image. */
    double x, y;		/* Coordinates of positioning point for
				 * image. */
    Tk_Anchor anchor;		/* Where to anchor image relative to
				 * (x,y). */
    char *imageString;		/* String describing -image option (malloc-ed).
				 * NULL means no image right now. */
    Tk_Image image;		/* Image to display in window, or NULL if
				 * no image at present. */
} ImageItem;

/*
 * Information used for parsing configuration specs:
 */

static Tk_CustomOption tagsOption = {Tk_CanvasTagsParseProc,
    Tk_CanvasTagsPrintProc, (ClientData) NULL
};

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_ANCHOR, "-anchor", (char *) NULL, (char *) NULL,
	"center", Tk_Offset(ImageItem, anchor), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_STRING, "-image", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(ImageItem, imageString), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
	(char *) NULL, 0, TK_CONFIG_NULL_OK, &tagsOption},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Prototypes for procedures defined in this file:
 */

static void		ImageChangedProc _ANSI_ARGS_((ClientData clientData,
			    int x, int y, int width, int height, int imgWidth,
			    int imgHeight));
static int		ImageCoords _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
			    char **argv));
static int		ImageToArea _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double *rectPtr));
static double		ImageToPoint _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double *coordPtr));
static void		ComputeImageBbox _ANSI_ARGS_((Tk_Canvas canvas,
			    ImageItem *imgPtr));
static int		ConfigureImage _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
			    char **argv, int flags));
static int		CreateImage _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, struct Tk_Item *itemPtr,
			    int argc, char **argv));
static void		DeleteImage _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, Display *display));
static void		DisplayImage _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, Display *display, Drawable dst,
			    int x, int y, int width, int height));
static void		ScaleImage _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double originX, double originY,
			    double scaleX, double scaleY));
static void		TranslateImage _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double deltaX, double deltaY));

/*
 * The structures below defines the image item type in terms of
 * procedures that can be invoked by generic item code.
 */

Tk_ItemType tkImageType = {
    "image",				/* name */
    sizeof(ImageItem),			/* itemSize */
    CreateImage,			/* createProc */
    configSpecs,			/* configSpecs */
    ConfigureImage,			/* configureProc */
    ImageCoords,			/* coordProc */
    DeleteImage,			/* deleteProc */
    DisplayImage,			/* displayProc */
    0,					/* alwaysRedraw */
    ImageToPoint,			/* pointProc */
    ImageToArea,			/* areaProc */
    (Tk_ItemPostscriptProc *) NULL,	/* postscriptProc */
    ScaleImage,				/* scaleProc */
    TranslateImage,			/* translateProc */
    (Tk_ItemIndexProc *) NULL,		/* indexProc */
    (Tk_ItemCursorProc *) NULL,		/* icursorProc */
    (Tk_ItemSelectionProc *) NULL,	/* selectionProc */
    (Tk_ItemInsertProc *) NULL,		/* insertProc */
    (Tk_ItemDCharsProc *) NULL,		/* dTextProc */
    (Tk_ItemType *) NULL		/* nextPtr */
};

/*
 *--------------------------------------------------------------
 *
 * CreateImage --
 *
 *	This procedure is invoked to create a new image
 *	item in a canvas.
 *
 * Results:
 *	A standard Tcl return value.  If an error occurred in
 *	creating the item, then an error message is left in
 *	interp->result;  in this case itemPtr is left uninitialized,
 *	so it can be safely freed by the caller.
 *
 * Side effects:
 *	A new image item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreateImage(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    Tk_Canvas canvas;			/* Canvas to hold new item. */
    Tk_Item *itemPtr;			/* Record to hold new item;  header
					 * has been initialized by caller. */
    int argc;				/* Number of arguments in argv. */
    char **argv;			/* Arguments describing rectangle. */
{
    ImageItem *imgPtr = (ImageItem *) itemPtr;

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

    imgPtr->canvas = canvas;
    imgPtr->anchor = TK_ANCHOR_CENTER;
    imgPtr->imageString = NULL;
    imgPtr->image = NULL;

    /*
     * Process the arguments to fill in the item record.
     */

    if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &imgPtr->x) != TCL_OK)
	    || (Tk_CanvasGetCoord(interp, canvas, argv[1], &imgPtr->y)
		!= TCL_OK)) {
	return TCL_ERROR;
    }

    if (ConfigureImage(interp, canvas, itemPtr, argc-2, argv+2, 0) != TCL_OK) {
	DeleteImage(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * ImageCoords --
 *
 *	This procedure is invoked to process the "coords" widget
 *	command on image items.  See the user documentation for
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
ImageCoords(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item whose coordinates are to be
					 * read or modified. */
    int argc;				/* Number of coordinates supplied in
					 * argv. */
    char **argv;			/* Array of coordinates: x1, y1,
					 * x2, y2, ... */
{
    ImageItem *imgPtr = (ImageItem *) itemPtr;
    char x[TCL_DOUBLE_SPACE], y[TCL_DOUBLE_SPACE];

    if (argc == 0) {
	Tcl_PrintDouble(interp, imgPtr->x, x);
	Tcl_PrintDouble(interp, imgPtr->y, y);
	Tcl_AppendResult(interp, x, " ", y, (char *) NULL);
    } else if (argc == 2) {
	if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &imgPtr->x) != TCL_OK)
		|| (Tk_CanvasGetCoord(interp, canvas, argv[1],
		    &imgPtr->y) != TCL_OK)) {
	    return TCL_ERROR;
	}
	ComputeImageBbox(canvas, imgPtr);
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
 * ConfigureImage --
 *
 *	This procedure is invoked to configure various aspects
 *	of an image item, such as its anchor position.
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
ConfigureImage(interp, canvas, itemPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tk_Canvas canvas;		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr;		/* Image item to reconfigure. */
    int argc;			/* Number of elements in argv.  */
    char **argv;		/* Arguments describing things to configure. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    ImageItem *imgPtr = (ImageItem *) itemPtr;
    Tk_Window tkwin;
    Tk_Image image;

    tkwin = Tk_CanvasTkwin(canvas);
    if (Tk_ConfigureWidget(interp, tkwin, configSpecs, argc,
	    argv, (char *) imgPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Create the image.  Save the old image around and don't free it
     * until after the new one is allocated.  This keeps the reference
     * count from going to zero so the image doesn't have to be recreated
     * if it hasn't changed.
     */

    if (imgPtr->imageString != NULL) {
	image = Tk_GetImage(interp, tkwin, imgPtr->imageString,
		ImageChangedProc, (ClientData) imgPtr);
	if (image == NULL) {
	    return TCL_ERROR;
	}
    } else {
	image = NULL;
    }
    if (imgPtr->image != NULL) {
	Tk_FreeImage(imgPtr->image);
    }
    imgPtr->image = image;
    ComputeImageBbox(canvas, imgPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DeleteImage --
 *
 *	This procedure is called to clean up the data structure
 *	associated with a image item.
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
DeleteImage(canvas, itemPtr, display)
    Tk_Canvas canvas;			/* Info about overall canvas widget. */
    Tk_Item *itemPtr;			/* Item that is being deleted. */
    Display *display;			/* Display containing window for
					 * canvas. */
{
    ImageItem *imgPtr = (ImageItem *) itemPtr;

    if (imgPtr->imageString != NULL) {
	ckfree(imgPtr->imageString);
    }
    if (imgPtr->image != NULL) {
	Tk_FreeImage(imgPtr->image);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ComputeImageBbox --
 *
 *	This procedure is invoked to compute the bounding box of
 *	all the pixels that may be drawn as part of a image item.
 *	This procedure is where the child image's placement is
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

	/* ARGSUSED */
static void
ComputeImageBbox(canvas, imgPtr)
    Tk_Canvas canvas;			/* Canvas that contains item. */
    ImageItem *imgPtr;			/* Item whose bbox is to be
					 * recomputed. */
{
    int width, height;
    int x, y;

    x = imgPtr->x + ((imgPtr->x >= 0) ? 0.5 : - 0.5);
    y = imgPtr->y + ((imgPtr->y >= 0) ? 0.5 : - 0.5);

    if (imgPtr->image == None) {
	imgPtr->header.x1 = imgPtr->header.x2 = x;
	imgPtr->header.y1 = imgPtr->header.y2 = y;
	return;
    }

    /*
     * Compute location and size of image, using anchor information.
     */

    Tk_SizeOfImage(imgPtr->image, &width, &height);
    switch (imgPtr->anchor) {
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

    imgPtr->header.x1 = x;
    imgPtr->header.y1 = y;
    imgPtr->header.x2 = x + width;
    imgPtr->header.y2 = y + height;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayImage --
 *
 *	This procedure is invoked to draw a image item in a given
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
DisplayImage(canvas, itemPtr, display, drawable, x, y, width, height)
    Tk_Canvas canvas;			/* Canvas that contains item. */
    Tk_Item *itemPtr;			/* Item to be displayed. */
    Display *display;			/* Display on which to draw item. */
    Drawable drawable;			/* Pixmap or window in which to draw
					 * item. */
    int x, y, width, height;		/* Describes region of canvas that
					 * must be redisplayed (not used). */
{
    ImageItem *imgPtr = (ImageItem *) itemPtr;
    short drawableX, drawableY;

    if (imgPtr->image == NULL) {
	return;
    }

    /*
     * Translate the coordinates to those of the image, then redisplay it.
     */

    Tk_CanvasDrawableCoords(canvas, (double) x, (double) y,
	    &drawableX, &drawableY);
    Tk_RedrawImage(imgPtr->image, x - imgPtr->header.x1, y - imgPtr->header.y1,
	    width, height, drawable, drawableX, drawableY);
}

/*
 *--------------------------------------------------------------
 *
 * ImageToPoint --
 *
 *	Computes the distance from a given point to a given
 *	rectangle, in canvas units.
 *
 * Results:
 *	The return value is 0 if the point whose x and y coordinates
 *	are coordPtr[0] and coordPtr[1] is inside the image.  If the
 *	point isn't inside the image then the return value is the
 *	distance from the point to the image.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static double
ImageToPoint(canvas, itemPtr, coordPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against point. */
    double *coordPtr;		/* Pointer to x and y coordinates. */
{
    ImageItem *imgPtr = (ImageItem *) itemPtr;
    double x1, x2, y1, y2, xDiff, yDiff;

    x1 = imgPtr->header.x1;
    y1 = imgPtr->header.y1;
    x2 = imgPtr->header.x2;
    y2 = imgPtr->header.y2;

    /*
     * Point is outside rectangle.
     */

    if (coordPtr[0] < x1) {
	xDiff = x1 - coordPtr[0];
    } else if (coordPtr[0] > x2)  {
	xDiff = coordPtr[0] - x2;
    } else {
	xDiff = 0;
    }

    if (coordPtr[1] < y1) {
	yDiff = y1 - coordPtr[1];
    } else if (coordPtr[1] > y2)  {
	yDiff = coordPtr[1] - y2;
    } else {
	yDiff = 0;
    }

    return hypot(xDiff, yDiff);
}

/*
 *--------------------------------------------------------------
 *
 * ImageToArea --
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
ImageToArea(canvas, itemPtr, rectPtr)
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item to check against rectangle. */
    double *rectPtr;		/* Pointer to array of four coordinates
				 * (x1, y1, x2, y2) describing rectangular
				 * area.  */
{
    ImageItem *imgPtr = (ImageItem *) itemPtr;

    if ((rectPtr[2] <= imgPtr->header.x1)
	    || (rectPtr[0] >= imgPtr->header.x2)
	    || (rectPtr[3] <= imgPtr->header.y1)
	    || (rectPtr[1] >= imgPtr->header.y2)) {
	return -1;
    }
    if ((rectPtr[0] <= imgPtr->header.x1)
	    && (rectPtr[1] <= imgPtr->header.y1)
	    && (rectPtr[2] >= imgPtr->header.x2)
	    && (rectPtr[3] >= imgPtr->header.y2)) {
	return 1;
    }
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * ScaleImage --
 *
 *	This procedure is invoked to rescale an item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The item referred to by itemPtr is rescaled so that the
 *	following transformation is applied to all point coordinates:
 *		x' = originX + scaleX*(x-originX)
 *		y' = originY + scaleY*(y-originY)
 *
 *--------------------------------------------------------------
 */

static void
ScaleImage(canvas, itemPtr, originX, originY, scaleX, scaleY)
    Tk_Canvas canvas;			/* Canvas containing rectangle. */
    Tk_Item *itemPtr;			/* Rectangle to be scaled. */
    double originX, originY;		/* Origin about which to scale rect. */
    double scaleX;			/* Amount to scale in X direction. */
    double scaleY;			/* Amount to scale in Y direction. */
{
    ImageItem *imgPtr = (ImageItem *) itemPtr;

    imgPtr->x = originX + scaleX*(imgPtr->x - originX);
    imgPtr->y = originY + scaleY*(imgPtr->y - originY);
    ComputeImageBbox(canvas, imgPtr);
}

/*
 *--------------------------------------------------------------
 *
 * TranslateImage --
 *
 *	This procedure is called to move an item by a given amount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The position of the item is offset by (xDelta, yDelta), and
 *	the bounding box is updated in the generic part of the item
 *	structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslateImage(canvas, itemPtr, deltaX, deltaY)
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item that is being moved. */
    double deltaX, deltaY;		/* Amount by which item is to be
					 * moved. */
{
    ImageItem *imgPtr = (ImageItem *) itemPtr;

    imgPtr->x += deltaX;
    imgPtr->y += deltaY;
    ComputeImageBbox(canvas, imgPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ImageChangedProc --
 *
 *	This procedure is invoked by the image code whenever the manager
 *	for an image does something that affects the image's size or
 *	how it is displayed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for the canvas to get redisplayed.
 *
 *----------------------------------------------------------------------
 */

static void
ImageChangedProc(clientData, x, y, width, height, imgWidth, imgHeight)
    ClientData clientData;		/* Pointer to canvas item for image. */
    int x, y;				/* Upper left pixel (within image)
					 * that must be redisplayed. */
    int width, height;			/* Dimensions of area to redisplay
					 * (may be <= 0). */
    int imgWidth, imgHeight;		/* New dimensions of image. */
{
    ImageItem *imgPtr = (ImageItem *) clientData;

    /*
     * If the image's size changed and it's not anchored at its
     * northwest corner then just redisplay the entire area of the
     * image.  This is a bit over-conservative, but we need to do
     * something because a size change also means a position change.
     */

    if (((imgPtr->header.x2 - imgPtr->header.x1) != imgWidth)
	    || ((imgPtr->header.y2 - imgPtr->header.y1) != imgHeight)) {
	x = y = 0;
	width = imgWidth;
	height = imgHeight;
	Tk_CanvasEventuallyRedraw(imgPtr->canvas, imgPtr->header.x1,
		imgPtr->header.y1, imgPtr->header.x2, imgPtr->header.y2);
    } 
    ComputeImageBbox(imgPtr->canvas, imgPtr);
    Tk_CanvasEventuallyRedraw(imgPtr->canvas, imgPtr->header.x1 + x,
	    imgPtr->header.y1 + y, (int) (imgPtr->header.x1 + x + width),
	    (int) (imgPtr->header.y1 + y + height));
}

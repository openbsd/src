/* 
 * tkCanvText.c --
 *
 *	This file implements text items for canvas widgets.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkCanvText.c 1.56 96/02/17 17:45:17
 */

#include <stdio.h>
#include "tkInt.h"
#include "tkCanvas.h"
#include "tkPort.h"

/*
 * One of the following structures is kept for each line of text
 * in a text item.  It contains geometry and display information
 * for that line.
 */

typedef struct TextLine {
    char *firstChar;		/* Pointer to the first character in this
				 * line (in the "text" field of enclosing
				 * text item). */
    int numChars;		/* Number of characters displayed in this
				 * line. */
    int totalChars;		/* Total number of characters included as
				 * part of this line (may include an extra
				 * space character at the end that isn't
				 * displayed). */
    int x, y;			/* Origin at which to draw line on screen
				 * (in integer pixel units, but in canvas
				 * coordinates, not screen coordinates). */
    int x1, y1;			/* Upper-left pixel that is part of text
				 * line on screen (again, in integer canvas
				 * pixel units). */
    int x2, y2;			/* Lower-left pixel that is part of text
				 * line on screen (again, in integer canvas
				 * pixel units). */
} TextLine;

/*
 * The structure below defines the record for each text item.
 */

typedef struct TextItem  {
    Tk_Item header;		/* Generic stuff that's the same for all
				 * types.  MUST BE FIRST IN STRUCTURE. */
    Tk_CanvasTextInfo *textInfoPtr;
				/* Pointer to a structure containing
				 * information about the selection and
				 * insertion cursor.  The structure is owned
				 * by (and shared with) the generic canvas
				 * code. */
    char *text;			/* Text for item (malloc-ed). */
    int numChars;		/* Number of non-NULL characters in text. */
    double x, y;		/* Positioning point for text. */
    Tk_Anchor anchor;		/* Where to anchor text relative to (x,y). */
    int width;			/* Width of lines for word-wrap, pixels.
				 * Zero means no word-wrap. */
    Tk_Justify justify;		/* Justification mode for text. */
    int rightEdge;		/* Pixel just to right of right edge of
				 * area of text item.  Used for selecting
				 * up to end of line. */
    XFontStruct *fontPtr;	/* Font for drawing text. */
    XColor *color;		/* Color for text. */
    Pixmap stipple;		/* Stipple bitmap for text, or None. */
    GC gc;			/* Graphics context for drawing text. */
    TextLine *linePtr;		/* Pointer to array of structures describing
				 * individual lines of text item (malloc-ed). */
    int numLines;		/* Number of structs at *linePtr. */
    int insertPos;		/* Insertion cursor is displayed just to left
				 * of character with this index. */
    GC cursorOffGC;		/* If not None, this gives a graphics context
				 * to use to draw the insertion cursor when
				 * it's off.  Usedif the selection and
				 * insertion cursor colors are the same.  */
    GC selTextGC;		/* Graphics context for selected text. */
} TextItem;

/*
 * Information used for parsing configuration specs:
 */

static Tk_CustomOption tagsOption = {Tk_CanvasTagsParseProc,
    Tk_CanvasTagsPrintProc, (ClientData) NULL
};

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_ANCHOR, "-anchor", (char *) NULL, (char *) NULL,
	"center", Tk_Offset(TextItem, anchor),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_COLOR, "-fill", (char *) NULL, (char *) NULL,
	"black", Tk_Offset(TextItem, color), 0},
    {TK_CONFIG_FONT, "-font", (char *) NULL, (char *) NULL,
	"-Adobe-Helvetica-Bold-R-Normal--*-120-*-*-*-*-*-*",
	Tk_Offset(TextItem, fontPtr), 0},
    {TK_CONFIG_JUSTIFY, "-justify", (char *) NULL, (char *) NULL,
	"left", Tk_Offset(TextItem, justify),
	TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_BITMAP, "-stipple", (char *) NULL, (char *) NULL,
	(char *) NULL, Tk_Offset(TextItem, stipple), TK_CONFIG_NULL_OK},
    {TK_CONFIG_CUSTOM, "-tags", (char *) NULL, (char *) NULL,
	(char *) NULL, 0, TK_CONFIG_NULL_OK, &tagsOption},
    {TK_CONFIG_STRING, "-text", (char *) NULL, (char *) NULL,
	"", Tk_Offset(TextItem, text), 0},
    {TK_CONFIG_PIXELS, "-width", (char *) NULL, (char *) NULL,
	"0", Tk_Offset(TextItem, width), TK_CONFIG_DONT_SET_DEFAULT},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Prototypes for procedures defined in this file:
 */

static void		ComputeTextBbox _ANSI_ARGS_((Tk_Canvas canvas,
			    TextItem *textPtr));
static int		ConfigureText _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, int argc,
			    char **argv, int flags));
static int		CreateText _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, struct Tk_Item *itemPtr,
			    int argc, char **argv));
static void		DeleteText _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, Display *display));
static void		DisplayText _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, Display *display, Drawable dst,
			    int x, int y, int width, int height));
static int		GetSelText _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, int offset, char *buffer,
			    int maxBytes));
static int		GetTextIndex _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr,
			    char *indexString, int *indexPtr));
static void		LineToPostscript _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, int numChars));
static void		ScaleText _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double originX, double originY,
			    double scaleX, double scaleY));
static void		SetTextCursor _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, int index));
static int		TextCoords _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr,
			    int argc, char **argv));
static void		TextDeleteChars _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, int first, int last));
static void		TextInsert _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, int beforeThis, char *string));
static int		TextToArea _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double *rectPtr));
static double		TextToPoint _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double *pointPtr));
static int		TextToPostscript _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Canvas canvas, Tk_Item *itemPtr, int prepass));
static void		TranslateText _ANSI_ARGS_((Tk_Canvas canvas,
			    Tk_Item *itemPtr, double deltaX, double deltaY));

/*
 * The structures below defines the rectangle and oval item types
 * by means of procedures that can be invoked by generic item code.
 */

Tk_ItemType tkTextType = {
    "text",				/* name */
    sizeof(TextItem),			/* itemSize */
    CreateText,				/* createProc */
    configSpecs,			/* configSpecs */
    ConfigureText,			/* configureProc */
    TextCoords,				/* coordProc */
    DeleteText,				/* deleteProc */
    DisplayText,			/* displayProc */
    0,					/* alwaysRedraw */
    TextToPoint,			/* pointProc */
    TextToArea,				/* areaProc */
    TextToPostscript,			/* postscriptProc */
    ScaleText,				/* scaleProc */
    TranslateText,			/* translateProc */
    GetTextIndex,			/* indexProc */
    SetTextCursor,			/* icursorProc */
    GetSelText,				/* selectionProc */
    TextInsert,				/* insertProc */
    TextDeleteChars,			/* dTextProc */
    (Tk_ItemType *) NULL		/* nextPtr */
};

/*
 *--------------------------------------------------------------
 *
 * CreateText --
 *
 *	This procedure is invoked to create a new text item
 *	in a canvas.
 *
 * Results:
 *	A standard Tcl return value.  If an error occurred in
 *	creating the item then an error message is left in
 *	interp->result;  in this case itemPtr is left uninitialized
 *	so it can be safely freed by the caller.
 *
 * Side effects:
 *	A new text item is created.
 *
 *--------------------------------------------------------------
 */

static int
CreateText(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Interpreter for error reporting. */
    Tk_Canvas canvas;			/* Canvas to hold new item. */
    Tk_Item *itemPtr;			/* Record to hold new item;  header
					 * has been initialized by caller. */
    int argc;				/* Number of arguments in argv. */
    char **argv;			/* Arguments describing rectangle. */
{
    TextItem *textPtr = (TextItem *) itemPtr;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		Tk_PathName(Tk_CanvasTkwin(canvas)), " create ",
		itemPtr->typePtr->name, " x y ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Carry out initialization that is needed in order to clean
     * up after errors during the the remainder of this procedure.
     */

    textPtr->text = NULL;
    textPtr->textInfoPtr = Tk_CanvasGetTextInfo(canvas);
    textPtr->numChars = 0;
    textPtr->anchor = TK_ANCHOR_CENTER;
    textPtr->width = 0;
    textPtr->justify = TK_JUSTIFY_LEFT;
    textPtr->rightEdge = 0;
    textPtr->fontPtr = NULL;
    textPtr->color = NULL;
    textPtr->stipple = None;
    textPtr->gc = None;
    textPtr->linePtr = NULL;
    textPtr->numLines = 0;
    textPtr->insertPos = 0;
    textPtr->cursorOffGC = None;
    textPtr->selTextGC = None;

    /*
     * Process the arguments to fill in the item record.
     */

    if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &textPtr->x) != TCL_OK)
	    || (Tk_CanvasGetCoord(interp, canvas, argv[1], &textPtr->y)
		!= TCL_OK)) {
	return TCL_ERROR;
    }

    if (ConfigureText(interp, canvas, itemPtr, argc-2, argv+2, 0) != TCL_OK) {
	DeleteText(canvas, itemPtr, Tk_Display(Tk_CanvasTkwin(canvas)));
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * TextCoords --
 *
 *	This procedure is invoked to process the "coords" widget
 *	command on text items.  See the user documentation for
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
TextCoords(interp, canvas, itemPtr, argc, argv)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item whose coordinates are to be
					 * read or modified. */
    int argc;				/* Number of coordinates supplied in
					 * argv. */
    char **argv;			/* Array of coordinates: x1, y1,
					 * x2, y2, ... */
{
    TextItem *textPtr = (TextItem *) itemPtr;
    char x[TCL_DOUBLE_SPACE], y[TCL_DOUBLE_SPACE];

    if (argc == 0) {
	Tcl_PrintDouble(interp, textPtr->x, x);
	Tcl_PrintDouble(interp, textPtr->y, y);
	Tcl_AppendResult(interp, x, " ", y, (char *) NULL);
    } else if (argc == 2) {
	if ((Tk_CanvasGetCoord(interp, canvas, argv[0], &textPtr->x) != TCL_OK)
		|| (Tk_CanvasGetCoord(interp, canvas, argv[1],
		    &textPtr->y) != TCL_OK)) {
	    return TCL_ERROR;
	}
	ComputeTextBbox(canvas, textPtr);
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
 * ConfigureText --
 *
 *	This procedure is invoked to configure various aspects
 *	of a text item, such as its border and background colors.
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
ConfigureText(interp, canvas, itemPtr, argc, argv, flags)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Tk_Canvas canvas;		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr;		/* Rectangle item to reconfigure. */
    int argc;			/* Number of elements in argv.  */
    char **argv;		/* Arguments describing things to configure. */
    int flags;			/* Flags to pass to Tk_ConfigureWidget. */
{
    TextItem *textPtr = (TextItem *) itemPtr;
    XGCValues gcValues;
    GC newGC, newSelGC;
    unsigned long mask;
    Tk_Window tkwin;
    Tk_CanvasTextInfo *textInfoPtr = textPtr->textInfoPtr;
    XColor *selBgColorPtr;

    tkwin = Tk_CanvasTkwin(canvas);
    if (Tk_ConfigureWidget(interp, tkwin, configSpecs, argc, argv,
	    (char *) textPtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * A few of the options require additional processing, such as
     * graphics contexts.
     */

    textPtr->numChars = strlen(textPtr->text);
    newGC = newSelGC = None;
    if ((textPtr->color != NULL) && (textPtr->fontPtr != NULL)) {
	gcValues.foreground = textPtr->color->pixel;
	gcValues.font = textPtr->fontPtr->fid;
	mask = GCForeground|GCFont;
	if (textPtr->stipple != None) {
	    gcValues.stipple = textPtr->stipple;
	    gcValues.fill_style = FillStippled;
	    mask |= GCForeground|GCStipple|GCFillStyle;
	}
	newGC = Tk_GetGC(tkwin, mask, &gcValues);
	gcValues.foreground = textInfoPtr->selFgColorPtr->pixel;
	newSelGC = Tk_GetGC(tkwin, mask, &gcValues);
    }
    if (textPtr->gc != None) {
	Tk_FreeGC(Tk_Display(tkwin), textPtr->gc);
    }
    textPtr->gc = newGC;
    if (textPtr->selTextGC != None) {
	Tk_FreeGC(Tk_Display(tkwin), textPtr->selTextGC);
    }
    textPtr->selTextGC = newSelGC;

    selBgColorPtr = Tk_3DBorderColor(textInfoPtr->selBorder);
    if (Tk_3DBorderColor(textInfoPtr->insertBorder)->pixel
	    == selBgColorPtr->pixel) {
	if (selBgColorPtr->pixel == BlackPixelOfScreen(Tk_Screen(tkwin))) {
	    gcValues.foreground = WhitePixelOfScreen(Tk_Screen(tkwin));
	} else {
	    gcValues.foreground = BlackPixelOfScreen(Tk_Screen(tkwin));
	}
	newGC = Tk_GetGC(tkwin, GCForeground, &gcValues);
    } else {
	newGC = None;
    }
    if (textPtr->cursorOffGC != None) {
	Tk_FreeGC(Tk_Display(tkwin), textPtr->cursorOffGC);
    }
    textPtr->cursorOffGC = newGC;

    /*
     * If the text was changed, move the selection and insertion indices
     * to keep them inside the item.
     */

    if (textInfoPtr->selItemPtr == itemPtr) {
	if (textInfoPtr->selectFirst >= textPtr->numChars) {
	    textInfoPtr->selItemPtr = NULL;
	} else {
	    if (textInfoPtr->selectLast >= textPtr->numChars) {
		textInfoPtr->selectLast = textPtr->numChars-1;
	    }
	    if ((textInfoPtr->anchorItemPtr == itemPtr)
		    && (textInfoPtr->selectAnchor >= textPtr->numChars)) {
		textInfoPtr->selectAnchor = textPtr->numChars-1;
	    }
	}
    }
    if (textPtr->insertPos >= textPtr->numChars) {
	textPtr->insertPos = textPtr->numChars;
    }

    ComputeTextBbox(canvas, textPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * DeleteText --
 *
 *	This procedure is called to clean up the data structure
 *	associated with a text item.
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
DeleteText(canvas, itemPtr, display)
    Tk_Canvas canvas;			/* Info about overall canvas widget. */
    Tk_Item *itemPtr;			/* Item that is being deleted. */
    Display *display;			/* Display containing window for
					 * canvas. */
{
    TextItem *textPtr = (TextItem *) itemPtr;

    if (textPtr->text != NULL) {
	ckfree(textPtr->text);
    }
    if (textPtr->fontPtr != NULL) {
	Tk_FreeFontStruct(textPtr->fontPtr);
    }
    if (textPtr->color != NULL) {
	Tk_FreeColor(textPtr->color);
    }
    if (textPtr->stipple != None) {
	Tk_FreeBitmap(display, textPtr->stipple);
    }
    if (textPtr->gc != None) {
	Tk_FreeGC(display, textPtr->gc);
    }
    if (textPtr->linePtr != NULL) {
	ckfree((char *) textPtr->linePtr);
    }
    if (textPtr->cursorOffGC != None) {
	Tk_FreeGC(display, textPtr->cursorOffGC);
    }
    if (textPtr->selTextGC != None) {
	Tk_FreeGC(display, textPtr->selTextGC);
    }
}

/*
 *--------------------------------------------------------------
 *
 * ComputeTextBbox --
 *
 *	This procedure is invoked to compute the bounding box of
 *	all the pixels that may be drawn as part of a text item.
 *	In addition, it recomputes all of the geometry information
 *	used to display a text item or check for mouse hits.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The fields x1, y1, x2, and y2 are updated in the header
 *	for itemPtr, and the linePtr structure is regenerated
 *	for itemPtr.
 *
 *--------------------------------------------------------------
 */

static void
ComputeTextBbox(canvas, textPtr)
    Tk_Canvas canvas;			/* Canvas that contains item. */
    TextItem *textPtr;			/* Item whose bbos is to be
					 * recomputed. */
{
    TextLine *linePtr;
#define MAX_LINES 100
    char *lineStart[MAX_LINES];
    int lineChars[MAX_LINES];
    int linePixels[MAX_LINES];
    int numLines, wrapPixels, maxLinePixels, leftX, topY, y;
    int lineHeight, i, fudge;
    char *p;
    XCharStruct *maxBoundsPtr = &textPtr->fontPtr->max_bounds;
    Tk_CanvasTextInfo *textInfoPtr = textPtr->textInfoPtr;

    if (textPtr->linePtr != NULL) {
	ckfree((char *) textPtr->linePtr);
	textPtr->linePtr = NULL;
    }

    /*
     * Work through the text computing the starting point, number of
     * characters, and number of pixels in each line.
     */

    p = textPtr->text;
    maxLinePixels = 0;
    if (textPtr->width > 0) {
	wrapPixels = textPtr->width;
    } else {
	wrapPixels = 10000000;
    }
    for (numLines = 0; (numLines < MAX_LINES); numLines++) {
	int numChars, numPixels;
	numChars = TkMeasureChars(textPtr->fontPtr, p,
		(textPtr->text + textPtr->numChars) - p, 0,
		wrapPixels, 0, TK_WHOLE_WORDS|TK_AT_LEAST_ONE, &numPixels);
	if (numPixels > maxLinePixels) {
	    maxLinePixels = numPixels;
	}
	lineStart[numLines] = p;
	lineChars[numLines] = numChars;
	linePixels[numLines] = numPixels;
	p += numChars;

	/*
	 * Skip space character that terminates a line, if there is one.
	 * In the case of multiple spaces, all but one will be displayed.
	 * This is important to make sure the insertion cursor gets
	 * displayed when it is in the middle of a multi-space.
	 */

	if (isspace(UCHAR(*p))) {
	    p++;
	} else if (*p == 0) {
	    /*
	     * The code below is tricky.  Putting the loop termination
	     * here guarantees that there's a TextLine for the last
	     * line of text, even if the line is empty (this can
	     * also happen if the entire text item is empty).  This is
	     * needed so that we can display the insertion cursor on a
	     * line even when it is empty.
	     */

	    numLines++;
	    break;
	}
    }

    /*
     * Use overall geometry information to compute the top-left corner
     * of the bounding box for the text item.
     */

    leftX = textPtr->x + 0.5;
    topY = textPtr->y + 0.5;
    lineHeight = textPtr->fontPtr->ascent + textPtr->fontPtr->descent;
    switch (textPtr->anchor) {
	case TK_ANCHOR_NW:
	case TK_ANCHOR_N:
	case TK_ANCHOR_NE:
	    break;

	case TK_ANCHOR_W:
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_E:
	    topY -= (lineHeight * numLines)/2;
	    break;

	case TK_ANCHOR_SW:
	case TK_ANCHOR_S:
	case TK_ANCHOR_SE:
	    topY -= lineHeight * numLines;
	    break;
    }
    switch (textPtr->anchor) {
	case TK_ANCHOR_NW:
	case TK_ANCHOR_W:
	case TK_ANCHOR_SW:
	    break;

	case TK_ANCHOR_N:
	case TK_ANCHOR_CENTER:
	case TK_ANCHOR_S:
	    leftX -= maxLinePixels/2;
	    break;

	case TK_ANCHOR_NE:
	case TK_ANCHOR_E:
	case TK_ANCHOR_SE:
	    leftX -= maxLinePixels;
	    break;
    }
    textPtr->rightEdge = leftX + maxLinePixels;

    /*
     * Create the new TextLine array and fill it in using the geometry
     * information gathered already.
     */

    if (numLines > 0) {
	textPtr->linePtr = (TextLine *) ckalloc((unsigned)
		(numLines * sizeof(TextLine)));
    } else {
	textPtr->linePtr = NULL;
    }
    textPtr->numLines = numLines;
    for (i = 0, linePtr = textPtr->linePtr, y = topY;
	    i < numLines; i++, linePtr++, y += lineHeight) {
	linePtr->firstChar = lineStart[i];
	linePtr->numChars = lineChars[i];
	if (i == (numLines-1)) {
	    linePtr->totalChars = linePtr->numChars;
	} else {
	    linePtr->totalChars = lineStart[i+1] - lineStart[i];
	}
	switch (textPtr->justify) {
	    case TK_JUSTIFY_LEFT:
		linePtr->x = leftX;
		break;
	    case TK_JUSTIFY_CENTER:
		linePtr->x = leftX + maxLinePixels/2 - linePixels[i]/2;
		break;
	    case TK_JUSTIFY_RIGHT:
		linePtr->x = leftX + maxLinePixels - linePixels[i];
		break;
	}
	linePtr->y = y + textPtr->fontPtr->ascent;
	linePtr->x1 = linePtr->x + maxBoundsPtr->lbearing;
	linePtr->y1 = y;
	linePtr->x2 = linePtr->x + linePixels[i];
	linePtr->y2 = linePtr->y + textPtr->fontPtr->descent - 1;
    }

    /*
     * Last of all, update the bounding box for the item.  The item's
     * bounding box includes the bounding box of all its lines, plus
     * an extra fudge factor for the cursor border (which could
     * potentially be quite large).
     */

    linePtr = textPtr->linePtr;
    textPtr->header.x1 = textPtr->header.x2 = leftX;
    textPtr->header.y1 = topY;
    textPtr->header.y2 = topY + numLines*lineHeight;
    for (linePtr = textPtr->linePtr, i = textPtr->numLines; i > 0;
	    i--, linePtr++) {
	if (linePtr->x1 < textPtr->header.x1) {
	    textPtr->header.x1 = linePtr->x1;
	}
	if (linePtr->x2 >= textPtr->header.x2) {
	    textPtr->header.x2 = linePtr->x2 + 1;
	}
    }

    fudge = (textInfoPtr->insertWidth+1)/2;
    if (textInfoPtr->selBorderWidth > fudge) {
	fudge = textInfoPtr->selBorderWidth;
    }
    textPtr->header.x1 -= fudge;
    textPtr->header.x2 += fudge;
}

/*
 *--------------------------------------------------------------
 *
 * DisplayText --
 *
 *	This procedure is invoked to draw a text item in a given
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
DisplayText(canvas, itemPtr, display, drawable, x, y, width, height)
    Tk_Canvas canvas;			/* Canvas that contains item. */
    Tk_Item *itemPtr;			/* Item to be displayed. */
    Display *display;			/* Display on which to draw item. */
    Drawable drawable;			/* Pixmap or window in which to draw
					 * item. */
    int x, y, width, height;		/* Describes region of canvas that
					 * must be redisplayed (not used). */
{
    TextItem *textPtr = (TextItem *) itemPtr;
    TextLine *linePtr;
    int i, focusHere, insertX, insertIndex, lineIndex, tabOrigin;
    int beforeSelect, inSelect, afterSelect, selStartX, selEndX;
    short drawableX, drawableY;
    Tk_CanvasTextInfo *textInfoPtr = textPtr->textInfoPtr;
    Tk_Window tkwin = Tk_CanvasTkwin(canvas);

    if (textPtr->gc == None) {
	return;
    }

    /*
     * If we're stippling, then modify the stipple offset in the GC.  Be
     * sure to reset the offset when done, since the GC is supposed to be
     * read-only.
     */

    if (textPtr->stipple != None) {
	Tk_CanvasSetStippleOrigin(canvas, textPtr->gc);
    }

    focusHere = (textInfoPtr->focusItemPtr == itemPtr) &&
	    (textInfoPtr->gotFocus);
    for (linePtr = textPtr->linePtr, i = textPtr->numLines;
	    i > 0; linePtr++, i--) {

	/*
	 * If part or all of this line is selected, then draw a special
	 * background under the selected part of the line.
	 */

	lineIndex = linePtr->firstChar - textPtr->text;
	if ((textInfoPtr->selItemPtr != itemPtr)
		|| (textInfoPtr->selectLast < lineIndex)
		|| (textInfoPtr->selectFirst >= (lineIndex
			+ linePtr->totalChars))) {
	    beforeSelect = linePtr->numChars;
	    inSelect = 0;
	} else {
	    beforeSelect = textInfoPtr->selectFirst - lineIndex;
	    if (beforeSelect <= 0) {
		beforeSelect = 0;
		selStartX = linePtr->x;
	    } else {
		(void) TkMeasureChars(textPtr->fontPtr,
			linePtr->firstChar, beforeSelect, 0,
			(int) 1000000, 0, TK_PARTIAL_OK, &selStartX);
		selStartX += linePtr->x;
	    }
	    inSelect = textInfoPtr->selectLast + 1 - (lineIndex + beforeSelect);

	    /*
	     * If the selection spans the end of this line, then display
	     * selection background all the way to the end of the line.
	     * However, for the last line we only want to display up to
	     * the last character, not the end of the line, hence the
	     * "i != 1" check.
	     */

	    if (inSelect >= (linePtr->totalChars - beforeSelect)) {
		inSelect = linePtr->numChars - beforeSelect;
		if (i != 1) {
		    selEndX = textPtr->rightEdge;
		    goto fillSelectBackground;
		}
	    }
	    (void) TkMeasureChars(textPtr->fontPtr,
		    linePtr->firstChar + beforeSelect, inSelect,
		    selStartX-linePtr->x, (int) 1000000, 0, TK_PARTIAL_OK,
		    &selEndX);
	    selEndX += linePtr->x;
	    fillSelectBackground:
	    Tk_CanvasDrawableCoords(canvas,
		    (double) (selStartX - textInfoPtr->selBorderWidth),
		    (double) (linePtr->y - textPtr->fontPtr->ascent),
		    &drawableX, &drawableY);
	    Tk_Fill3DRectangle(tkwin, drawable, textInfoPtr->selBorder,
		    drawableX, drawableY,
		    selEndX - selStartX + 2*textInfoPtr->selBorderWidth,
		    textPtr->fontPtr->ascent + textPtr->fontPtr->descent,
		    textInfoPtr->selBorderWidth, TK_RELIEF_RAISED);
	}

	/*
	 * If the insertion cursor is in this line, then draw a special
	 * background for the cursor before drawing the text.  Note:
	 * if we're the cursor item but the cursor is turned off, then
	 * redraw background over the area of the cursor.  This guarantees
	 * that the selection won't make the cursor invisible on mono
	 * displays, where both are drawn in the same color.
	 */

	if (focusHere) {
	    insertIndex = textPtr->insertPos
		    - (linePtr->firstChar - textPtr->text);
	    if ((insertIndex >= 0) && (insertIndex <= linePtr->numChars)) {
		(void) TkMeasureChars(textPtr->fontPtr, linePtr->firstChar,
		    insertIndex, 0, (int) 1000000, 0, TK_PARTIAL_OK, &insertX);
		Tk_CanvasDrawableCoords(canvas,
			(double) (linePtr->x + insertX
			    - (textInfoPtr->insertWidth)/2),
			(double) (linePtr->y - textPtr->fontPtr->ascent),
			&drawableX, &drawableY);
		if (textInfoPtr->cursorOn) {
		    Tk_Fill3DRectangle(tkwin, drawable,
			    textInfoPtr->insertBorder, drawableX, drawableY,
			    textInfoPtr->insertWidth,
			    textPtr->fontPtr->ascent
				+ textPtr->fontPtr->descent,
			    textInfoPtr->insertBorderWidth, TK_RELIEF_RAISED);
		} else if (textPtr->cursorOffGC != None) {
		    /* Redraw the background over the area of the cursor,
		     * even though the cursor is turned off.  This guarantees
		     * that the selection won't make the cursor invisible on
		     * mono displays, where both may be drawn in the same
		     * color.
		     */

		    XFillRectangle(display, drawable, textPtr->cursorOffGC,
			    drawableX, drawableY,
			    (unsigned) textInfoPtr->insertWidth,
			    (unsigned) (textPtr->fontPtr->ascent
				+ textPtr->fontPtr->descent));
		}
	    }
	}

	/*
	 * Display the text in three pieces:  the part before the
	 * selection, the selected part (which needs a different graphics
	 * context), and the part after the selection.
	 */

	Tk_CanvasDrawableCoords(canvas, (double) linePtr->x,
		(double) linePtr->y, &drawableX, &drawableY);
	tabOrigin = drawableX;
	if (beforeSelect != 0) {
	    TkDisplayChars(display, drawable, textPtr->gc, textPtr->fontPtr,
		    linePtr->firstChar, beforeSelect, drawableX,
		    drawableY, tabOrigin, 0);
	}
	if (inSelect != 0) {
	    Tk_CanvasDrawableCoords(canvas, (double) selStartX,
		    (double) linePtr->y, &drawableX, &drawableY);
	    TkDisplayChars(display, drawable, textPtr->selTextGC,
		    textPtr->fontPtr, linePtr->firstChar + beforeSelect,
		    inSelect, drawableX, drawableY, tabOrigin, 0);
	}
	afterSelect = linePtr->numChars - beforeSelect - inSelect;
	if (afterSelect > 0) {
	    Tk_CanvasDrawableCoords(canvas, (double) selEndX,
		    (double) linePtr->y, &drawableX, &drawableY);
	    TkDisplayChars(display, drawable, textPtr->gc, textPtr->fontPtr,
		    linePtr->firstChar + beforeSelect + inSelect,
		    afterSelect, drawableX, drawableY, tabOrigin, 0);
	}
    }
    if (textPtr->stipple != None) {
	XSetTSOrigin(display, textPtr->gc, 0, 0);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TextInsert --
 *
 *	Insert characters into a text item at a given position.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The text in the given item is modified.  The cursor and
 *	selection positions are also modified to reflect the
 *	insertion.
 *
 *--------------------------------------------------------------
 */

static void
TextInsert(canvas, itemPtr, beforeThis, string)
    Tk_Canvas canvas;		/* Canvas containing text item. */
    Tk_Item *itemPtr;		/* Text item to be modified. */
    int beforeThis;		/* Index of character before which text is
				 * to be inserted. */
    char *string;		/* New characters to be inserted. */
{
    TextItem *textPtr = (TextItem *) itemPtr;
    int length;
    char *new;
    Tk_CanvasTextInfo *textInfoPtr = textPtr->textInfoPtr;

    length = strlen(string);
    if (length == 0) {
	return;
    }
    if (beforeThis < 0) {
	beforeThis = 0;
    }
    if (beforeThis > textPtr->numChars) {
	beforeThis = textPtr->numChars;
    }

    new = (char *) ckalloc((unsigned) (textPtr->numChars + length + 1));
    strncpy(new, textPtr->text, (size_t) beforeThis);
    strcpy(new+beforeThis, string);
    strcpy(new+beforeThis+length, textPtr->text+beforeThis);
    ckfree(textPtr->text);
    textPtr->text = new;
    textPtr->numChars += length;

    /*
     * Inserting characters invalidates indices such as those for the
     * selection and cursor.  Update the indices appropriately.
     */

    if (textInfoPtr->selItemPtr == itemPtr) {
	if (textInfoPtr->selectFirst >= beforeThis) {
	    textInfoPtr->selectFirst += length;
	}
	if (textInfoPtr->selectLast >= beforeThis) {
	    textInfoPtr->selectLast += length;
	}
	if ((textInfoPtr->anchorItemPtr == itemPtr)
		&& (textInfoPtr->selectAnchor >= beforeThis)) {
	    textInfoPtr->selectAnchor += length;
	}
    }
    if (textPtr->insertPos >= beforeThis) {
	textPtr->insertPos += length;
    }
    ComputeTextBbox(canvas, textPtr);
}

/*
 *--------------------------------------------------------------
 *
 * TextDeleteChars --
 *
 *	Delete one or more characters from a text item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Characters between "first" and "last", inclusive, get
 *	deleted from itemPtr, and things like the selection
 *	position get updated.
 *
 *--------------------------------------------------------------
 */

static void
TextDeleteChars(canvas, itemPtr, first, last)
    Tk_Canvas canvas;		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr;		/* Item in which to delete characters. */
    int first;			/* Index of first character to delete. */
    int last;			/* Index of last character to delete. */
{
    TextItem *textPtr = (TextItem *) itemPtr;
    int count;
    char *new;
    Tk_CanvasTextInfo *textInfoPtr = textPtr->textInfoPtr;

    if (first < 0) {
	first = 0;
    }
    if (last >= textPtr->numChars) {
	last = textPtr->numChars-1;
    }
    if (first > last) {
	return;
    }
    count = last + 1 - first;

    new = (char *) ckalloc((unsigned) (textPtr->numChars + 1 - count));
    strncpy(new, textPtr->text, (size_t) first);
    strcpy(new+first, textPtr->text+last+1);
    ckfree(textPtr->text);
    textPtr->text = new;
    textPtr->numChars -= count;

    /*
     * Update indexes for the selection and cursor to reflect the
     * renumbering of the remaining characters.
     */

    if (textInfoPtr->selItemPtr == itemPtr) {
	if (textInfoPtr->selectFirst > first) {
	    textInfoPtr->selectFirst -= count;
	    if (textInfoPtr->selectFirst < first) {
		textInfoPtr->selectFirst = first;
	    }
	}
	if (textInfoPtr->selectLast >= first) {
	    textInfoPtr->selectLast -= count;
	    if (textInfoPtr->selectLast < (first-1)) {
		textInfoPtr->selectLast = (first-1);
	    }
	}
	if (textInfoPtr->selectFirst > textInfoPtr->selectLast) {
	    textInfoPtr->selItemPtr = NULL;
	}
	if ((textInfoPtr->anchorItemPtr == itemPtr)
		&& (textInfoPtr->selectAnchor > first)) {
	    textInfoPtr->selectAnchor -= count;
	    if (textInfoPtr->selectAnchor < first) {
		textInfoPtr->selectAnchor = first;
	    }
	}
    }
    if (textPtr->insertPos > first) {
	textPtr->insertPos -= count;
	if (textPtr->insertPos < first) {
	    textPtr->insertPos = first;
	}
    }
    ComputeTextBbox(canvas, textPtr);
    return;
}

/*
 *--------------------------------------------------------------
 *
 * TextToPoint --
 *
 *	Computes the distance from a given point to a given
 *	text item, in canvas units.
 *
 * Results:
 *	The return value is 0 if the point whose x and y coordinates
 *	are pointPtr[0] and pointPtr[1] is inside the arc.  If the
 *	point isn't inside the arc then the return value is the
 *	distance from the point to the arc.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static double
TextToPoint(canvas, itemPtr, pointPtr)
    Tk_Canvas canvas;		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr;		/* Item to check against point. */
    double *pointPtr;		/* Pointer to x and y coordinates. */
{
    TextItem *textPtr = (TextItem *) itemPtr;
    TextLine *linePtr;
    int i;
    double xDiff, yDiff, dist, minDist;

    /*
     * Treat each line in the text item as a rectangle, compute the
     * distance to that rectangle, and take the minimum of these
     * distances.  Perform most of the calculations in integer pixel
     * units, since that's how the dimensions of the text are defined.
     */

    minDist = -1.0;
    for (linePtr = textPtr->linePtr, i = textPtr->numLines;
	    i > 0; linePtr++, i--) {

	/*
	 * If the point is inside the line's rectangle, then can
	 * return immediately.
	 */
    
	if ((pointPtr[0] >= linePtr->x1)
		&& (pointPtr[0] <= linePtr->x2)
		&& (pointPtr[1] >= linePtr->y1)
		&& (pointPtr[1] <= linePtr->y2)) {
	    return 0.0;
	}
    
	/*
	 * Point is outside line's rectangle; compute distance to nearest
	 * side.
	 */
    
	if (pointPtr[0] < linePtr->x1) {
	    xDiff = linePtr->x1 - pointPtr[0];
	} else if (pointPtr[0] > linePtr->x2)  {
	    xDiff = pointPtr[0] - linePtr->x2;
	} else {
	    xDiff = 0;
	}
    
	if (pointPtr[1] < linePtr->y1) {
	    yDiff = linePtr->y1 - pointPtr[1];
	} else if (pointPtr[1] > linePtr->y2)  {
	    yDiff = pointPtr[1] - linePtr->y2;
	} else {
	    yDiff = 0;
	}

	dist = hypot(xDiff, yDiff);
	if ((dist < minDist) || (minDist < 0.0)) {
	    minDist = dist;
	}
    }
    return minDist;
}

/*
 *--------------------------------------------------------------
 *
 * TextToArea --
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
TextToArea(canvas, itemPtr, rectPtr)
    Tk_Canvas canvas;		/* Canvas containing itemPtr. */
    Tk_Item *itemPtr;		/* Item to check against rectangle. */
    double *rectPtr;		/* Pointer to array of four coordinates
				 * (x1, y1, x2, y2) describing rectangular
				 * area.  */
{
    TextItem *textPtr = (TextItem *) itemPtr;
    TextLine *linePtr;
    int i, result;

    /*
     * Scan the lines one at a time, seeing whether each line is
     * entirely in, entirely out, or overlapping the rectangle.  If
     * an overlap is detected, return immediately;  otherwise wait
     * until all lines have been processed and see if they were all
     * inside or all outside.
     */

    result = 0;
    for (linePtr = textPtr->linePtr, i = textPtr->numLines;
	    i > 0; linePtr++, i--) {
	if ((rectPtr[2] < linePtr->x1) || (rectPtr[0] > linePtr->x2)
		|| (rectPtr[3] < linePtr->y1) || (rectPtr[1] > linePtr->y2)) {
	    if (result == 1) {
		return 0;
	    }
	    result = -1;
	    continue;
	}
	if ((linePtr->x1 < rectPtr[0]) || (linePtr->x2 > rectPtr[2])
		|| (linePtr->y1 < rectPtr[1]) || (linePtr->y2 > rectPtr[3])) {
	    return 0;
	}
	if (result == -1) {
	    return 0;
	}
	result = 1;
    }
    return result;
}

/*
 *--------------------------------------------------------------
 *
 * ScaleText --
 *
 *	This procedure is invoked to rescale a text item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Scales the position of the text, but not the size
 *	of the font for the text.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
ScaleText(canvas, itemPtr, originX, originY, scaleX, scaleY)
    Tk_Canvas canvas;			/* Canvas containing rectangle. */
    Tk_Item *itemPtr;			/* Rectangle to be scaled. */
    double originX, originY;		/* Origin about which to scale rect. */
    double scaleX;			/* Amount to scale in X direction. */
    double scaleY;			/* Amount to scale in Y direction. */
{
    TextItem *textPtr = (TextItem *) itemPtr;

    textPtr->x = originX + scaleX*(textPtr->x - originX);
    textPtr->y = originY + scaleY*(textPtr->y - originY);
    ComputeTextBbox(canvas, textPtr);
    return;
}

/*
 *--------------------------------------------------------------
 *
 * TranslateText --
 *
 *	This procedure is called to move a text item by a
 *	given amount.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The position of the text item is offset by (xDelta, yDelta),
 *	and the bounding box is updated in the generic part of the
 *	item structure.
 *
 *--------------------------------------------------------------
 */

static void
TranslateText(canvas, itemPtr, deltaX, deltaY)
    Tk_Canvas canvas;			/* Canvas containing item. */
    Tk_Item *itemPtr;			/* Item that is being moved. */
    double deltaX, deltaY;		/* Amount by which item is to be
					 * moved. */
{
    TextItem *textPtr = (TextItem *) itemPtr;

    textPtr->x += deltaX;
    textPtr->y += deltaY;
    ComputeTextBbox(canvas, textPtr);
}

/*
 *--------------------------------------------------------------
 *
 * GetTextIndex --
 *
 *	Parse an index into a text item and return either its value
 *	or an error.
 *
 * Results:
 *	A standard Tcl result.  If all went well, then *indexPtr is
 *	filled in with the index (into itemPtr) corresponding to
 *	string.  Otherwise an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static int
GetTextIndex(interp, canvas, itemPtr, string, indexPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    Tk_Canvas canvas;		/* Canvas containing item. */
    Tk_Item *itemPtr;		/* Item for which the index is being
				 * specified. */
    char *string;		/* Specification of a particular character
				 * in itemPtr's text. */
    int *indexPtr;		/* Where to store converted index. */
{
    TextItem *textPtr = (TextItem *) itemPtr;
    size_t length;
    Tk_CanvasTextInfo *textInfoPtr = textPtr->textInfoPtr;

    length = strlen(string);

    if (string[0] == 'e') {
	if (strncmp(string, "end", length) == 0) {
	    *indexPtr = textPtr->numChars;
	} else {
	    badIndex:

	    /*
	     * Some of the paths here leave messages in interp->result,
	     * so we have to clear it out before storing our own message.
	     */

	    Tcl_SetResult(interp, (char *) NULL, TCL_STATIC);
	    Tcl_AppendResult(interp, "bad index \"", string, "\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
    } else if (string[0] == 'i') {
	if (strncmp(string, "insert", length) == 0) {
	    *indexPtr = textPtr->insertPos;
	} else {
	    goto badIndex;
	}
    } else if (string[0] == 's') {
	if (textInfoPtr->selItemPtr != itemPtr) {
	    interp->result = "selection isn't in item";
	    return TCL_ERROR;
	}
	if (length < 5) {
	    goto badIndex;
	}
	if (strncmp(string, "sel.first", length) == 0) {
	    *indexPtr = textInfoPtr->selectFirst;
	} else if (strncmp(string, "sel.last", length) == 0) {
	    *indexPtr = textInfoPtr->selectLast;
	} else {
	    goto badIndex;
	}
    } else if (string[0] == '@') {
	int x, y, dummy, i;
	double tmp;
	char *end, *p;
	TextLine *linePtr;

	p = string+1;
	tmp = strtod(p, &end);
	if ((end == p) || (*end != ',')) {
	    goto badIndex;
	}
	x = (tmp < 0) ? tmp - 0.5 : tmp + 0.5;
	p = end+1;
	tmp = strtod(p, &end);
	if ((end == p) || (*end != 0)) {
	    goto badIndex;
	}
	y = (tmp < 0) ? tmp - 0.5 : tmp + 0.5;
	if ((textPtr->numChars == 0) || (y < textPtr->linePtr[0].y1)) {
	    *indexPtr = 0;
	    return TCL_OK;
	}
	for (i = 0, linePtr = textPtr->linePtr; ; i++, linePtr++) {
	    if (i >= textPtr->numLines) {
		*indexPtr = textPtr->numChars;
		return TCL_OK;
	    }
	    if (y <= linePtr->y2) {
		break;
	    }
	}
	*indexPtr = TkMeasureChars(textPtr->fontPtr, linePtr->firstChar,
		linePtr->numChars, linePtr->x, x, linePtr->x, 0, &dummy);
	*indexPtr += linePtr->firstChar - textPtr->text;
    } else {
	if (Tcl_GetInt(interp, string, indexPtr) != TCL_OK) {
	    goto badIndex;
	}
	if (*indexPtr < 0){
	    *indexPtr = 0;
	} else if (*indexPtr > textPtr->numChars) {
	    *indexPtr = textPtr->numChars;
	}
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * SetTextCursor --
 *
 *	Set the position of the insertion cursor in this item.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor position will change.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
SetTextCursor(canvas, itemPtr, index)
    Tk_Canvas canvas;			/* Record describing canvas widget. */
    Tk_Item *itemPtr;			/* Text item in which cursor position
					 * is to be set. */
    int index;				/* Index of character just before which
					 * cursor is to be positioned. */
{
    TextItem *textPtr = (TextItem *) itemPtr;

    if (index < 0) {
	textPtr->insertPos = 0;
    } else  if (index > textPtr->numChars) {
	textPtr->insertPos = textPtr->numChars;
    } else {
	textPtr->insertPos = index;
    }
}

/*
 *--------------------------------------------------------------
 *
 * GetSelText --
 *
 *	This procedure is invoked to return the selected portion
 *	of a text item.  It is only called when this item has
 *	the selection.
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
GetSelText(canvas, itemPtr, offset, buffer, maxBytes)
    Tk_Canvas canvas;			/* Canvas containing selection. */
    Tk_Item *itemPtr;			/* Text item containing selection. */
    int offset;				/* Offset within selection of first
					 * character to be returned. */
    char *buffer;			/* Location in which to place
					 * selection. */
    int maxBytes;			/* Maximum number of bytes to place
					 * at buffer, not including terminating
					 * NULL character. */
{
    TextItem *textPtr = (TextItem *) itemPtr;
    int count;
    Tk_CanvasTextInfo *textInfoPtr = textPtr->textInfoPtr;

    count = textInfoPtr->selectLast + 1 - textInfoPtr->selectFirst - offset;
    if (textInfoPtr->selectLast == textPtr->numChars) {
	count -= 1;
    }
    if (count > maxBytes) {
	count = maxBytes;
    }
    if (count <= 0) {
	return 0;
    }
    strncpy(buffer, textPtr->text + textInfoPtr->selectFirst + offset,
	    (size_t) count);
    buffer[count] = '\0';
    return count;
}

/*
 *--------------------------------------------------------------
 *
 * TextToPostscript --
 *
 *	This procedure is called to generate Postscript for
 *	text items.
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
TextToPostscript(interp, canvas, itemPtr, prepass)
    Tcl_Interp *interp;			/* Leave Postscript or error message
					 * here. */
    Tk_Canvas canvas;			/* Information about overall canvas. */
    Tk_Item *itemPtr;			/* Item for which Postscript is
					 * wanted. */
    int prepass;			/* 1 means this is a prepass to
					 * collect font information;  0 means
					 * final Postscript is being created. */
{
    TextItem *textPtr = (TextItem *) itemPtr;
    TextLine *linePtr;
    int i;
    char *xoffset = NULL, *yoffset = NULL;	/* Initializations needed */
    char *justify = NULL;			/* only to stop compiler
   						 * warnings. */
    char buffer[500];

    if (textPtr->color == NULL) {
	return TCL_OK;
    }

    if (Tk_CanvasPsFont(interp, canvas, textPtr->fontPtr) != TCL_OK) {
	return TCL_ERROR;
    }
    if (Tk_CanvasPsColor(interp, canvas, textPtr->color) != TCL_OK) {
	return TCL_ERROR;
    }
    if (textPtr->stipple != None) {
	Tcl_AppendResult(interp, "/StippleText {\n    ",
		(char *) NULL);
	Tk_CanvasPsStipple(interp, canvas, textPtr->stipple);
	Tcl_AppendResult(interp, "} bind def\n", (char *) NULL);
    }
    sprintf(buffer, "%.15g %.15g [\n", textPtr->x,
	    Tk_CanvasPsY(canvas, textPtr->y));
    Tcl_AppendResult(interp, buffer, (char *) NULL);
    for (i = textPtr->numLines, linePtr = textPtr->linePtr;
	    i > 0; i--, linePtr++) {
	Tcl_AppendResult(interp, "    ", (char *) NULL);
	LineToPostscript(interp, linePtr->firstChar,
		linePtr->numChars);
	Tcl_AppendResult(interp, "\n", (char *) NULL);
    }
    switch (textPtr->anchor) {
	case TK_ANCHOR_NW:     xoffset = "0";    yoffset = "0";   break;
	case TK_ANCHOR_N:      xoffset = "-0.5"; yoffset = "0";   break;
	case TK_ANCHOR_NE:     xoffset = "-1";   yoffset = "0";   break;
	case TK_ANCHOR_E:      xoffset = "-1";   yoffset = "0.5"; break;
	case TK_ANCHOR_SE:     xoffset = "-1";   yoffset = "1";   break;
	case TK_ANCHOR_S:      xoffset = "-0.5"; yoffset = "1";   break;
	case TK_ANCHOR_SW:     xoffset = "0";    yoffset = "1";   break;
	case TK_ANCHOR_W:      xoffset = "0";    yoffset = "0.5"; break;
	case TK_ANCHOR_CENTER: xoffset = "-0.5"; yoffset = "0.5"; break;
    }
    switch (textPtr->justify) {
	case TK_JUSTIFY_LEFT:	justify = "0";   break;
	case TK_JUSTIFY_CENTER:	justify = "0.5"; break;
	case TK_JUSTIFY_RIGHT:	justify = "1";   break;
    }
    sprintf(buffer, "] %d %s %s %s %s DrawText\n",
	    textPtr->fontPtr->ascent + textPtr->fontPtr->descent,
	    xoffset, yoffset, justify,
	    (textPtr->stipple == None) ? "false" : "true");
    Tcl_AppendResult(interp, buffer, (char *) NULL);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * LineToPostscript --
 *
 *	This procedure generates a parenthesized Postscript string
 *	describing one line of text from a text item.
 *
 * Results:
 *	None. The parenthesized string is appended to
 *	interp->result.  It generates proper backslash notation so
 *	that Postscript can interpret the string correctly.
 *
 * Side effects:
 *	None.
 *
 *--------------------------------------------------------------
 */

static void
LineToPostscript(interp, string, numChars)
    Tcl_Interp *interp;		/* Interp whose result is to be appended to. */
    char *string;		/* String to Postscript-ify. */
    int numChars;		/* Number of characters in the string. */
{
#define BUFFER_SIZE 100
    char buffer[BUFFER_SIZE+5];
    int used, c;

    buffer[0] = '(';
    used = 1;
    for ( ; numChars > 0; string++, numChars--) {
	c = (*string) & 0xff;
	if ((c == '(') || (c == ')') || (c == '\\') || (c < 0x20)
		|| (c >= 0x7f)) {
	    /*
	     * Tricky point:  the "03" is necessary in the sprintf below,
	     * so that a full three digits of octal are always generated.
	     * Without the "03", a number following this sequence could
	     * be interpreted by Postscript as part of this sequence.
	     */
	    sprintf(buffer+used, "\\%03o", c);
	    used += strlen(buffer+used);
	} else {
	    buffer[used] = c;
	    used++;
	}
	if (used >= BUFFER_SIZE) {
	    buffer[used] = 0;
	    Tcl_AppendResult(interp, buffer, (char *) NULL);
	    used = 0;
	}
    }
    buffer[used] = ')';
    buffer[used+1] = 0;
    Tcl_AppendResult(interp, buffer, (char *) NULL);
}

/* 
 * tkUnixCursor.c --
 *
 *	This file contains X specific cursor manipulation routines.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUnixCursor.c 1.2 96/02/15 18:55:25
 */

#include "tkPort.h"
#include "tkInt.h"

/*
 * The following data structure is a superset of the TkCursor structure
 * defined in tkCursor.c.  Each system specific cursor module will define
 * a different cursor structure.  All of these structures must have the
 * same header consisting of the fields in TkCursor.
 */



typedef struct {
    TkCursor info;		/* Generic cursor info used by tkCursor.c */
    Display *display;		/* Display for which cursor is valid. */
} TkUnixCursor;

/*
 * The table below is used to map from the name of a cursor to its
 * index in the official cursor font:
 */

static struct CursorName {
    char		*name;
    unsigned int	shape;
} cursorNames[] = {
    {"X_cursor",		XC_X_cursor},
    {"arrow",			XC_arrow},
    {"based_arrow_down",	XC_based_arrow_down},
    {"based_arrow_up",		XC_based_arrow_up},
    {"boat",			XC_boat},
    {"bogosity",		XC_bogosity},
    {"bottom_left_corner",	XC_bottom_left_corner},
    {"bottom_right_corner",	XC_bottom_right_corner},
    {"bottom_side",		XC_bottom_side},
    {"bottom_tee",		XC_bottom_tee},
    {"box_spiral",		XC_box_spiral},
    {"center_ptr",		XC_center_ptr},
    {"circle",			XC_circle},
    {"clock",			XC_clock},
    {"coffee_mug",		XC_coffee_mug},
    {"cross",			XC_cross},
    {"cross_reverse",		XC_cross_reverse},
    {"crosshair",		XC_crosshair},
    {"diamond_cross",		XC_diamond_cross},
    {"dot",			XC_dot},
    {"dotbox",			XC_dotbox},
    {"double_arrow",		XC_double_arrow},
    {"draft_large",		XC_draft_large},
    {"draft_small",		XC_draft_small},
    {"draped_box",		XC_draped_box},
    {"exchange",		XC_exchange},
    {"fleur",			XC_fleur},
    {"gobbler",			XC_gobbler},
    {"gumby",			XC_gumby},
    {"hand1",			XC_hand1},
    {"hand2",			XC_hand2},
    {"heart",			XC_heart},
    {"icon",			XC_icon},
    {"iron_cross",		XC_iron_cross},
    {"left_ptr",		XC_left_ptr},
    {"left_side",		XC_left_side},
    {"left_tee",		XC_left_tee},
    {"leftbutton",		XC_leftbutton},
    {"ll_angle",		XC_ll_angle},
    {"lr_angle",		XC_lr_angle},
    {"man",			XC_man},
    {"middlebutton",		XC_middlebutton},
    {"mouse",			XC_mouse},
    {"pencil",			XC_pencil},
    {"pirate",			XC_pirate},
    {"plus",			XC_plus},
    {"question_arrow",		XC_question_arrow},
    {"right_ptr",		XC_right_ptr},
    {"right_side",		XC_right_side},
    {"right_tee",		XC_right_tee},
    {"rightbutton",		XC_rightbutton},
    {"rtl_logo",		XC_rtl_logo},
    {"sailboat",		XC_sailboat},
    {"sb_down_arrow",		XC_sb_down_arrow},
    {"sb_h_double_arrow",	XC_sb_h_double_arrow},
    {"sb_left_arrow",		XC_sb_left_arrow},
    {"sb_right_arrow",		XC_sb_right_arrow},
    {"sb_up_arrow",		XC_sb_up_arrow},
    {"sb_v_double_arrow",	XC_sb_v_double_arrow},
    {"shuttle",			XC_shuttle},
    {"sizing",			XC_sizing},
    {"spider",			XC_spider},
    {"spraycan",		XC_spraycan},
    {"star",			XC_star},
    {"target",			XC_target},
    {"tcross",			XC_tcross},
    {"top_left_arrow",		XC_top_left_arrow},
    {"top_left_corner",		XC_top_left_corner},
    {"top_right_corner",	XC_top_right_corner},
    {"top_side",		XC_top_side},
    {"top_tee",			XC_top_tee},
    {"trek",			XC_trek},
    {"ul_angle",		XC_ul_angle},
    {"umbrella",		XC_umbrella},
    {"ur_angle",		XC_ur_angle},
    {"watch",			XC_watch},
    {"xterm",			XC_xterm},
    {NULL,			0}
};

/*
 * Font to use for cursors:
 */

#ifndef CURSORFONT
#define CURSORFONT "cursor"
#endif


/*
 *----------------------------------------------------------------------
 *
 * TkGetCursorByName --
 *
 *	Retrieve a cursor by name.  Parse the cursor name into fields
 *	and create a cursor, either from the standard cursor font or
 *	from bitmap files.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.  
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkGetCursorByName(interp, tkwin, string)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    Tk_Uid string;		/* Description of cursor.  See manual entry
				 * for details on legal syntax. */
{
    TkUnixCursor *cursorPtr = NULL;
    Cursor cursor = None;
    int argc;
    char **argv = NULL;
    Pixmap source = None;
    Pixmap mask = None;
    Display *display = Tk_Display(tkwin);

    if (Tcl_SplitList(interp, string, &argc, &argv) != TCL_OK) {
	return NULL;
    }
    if (argc == 0) {
	goto badString;
    }
    if (argv[0][0] != '@') {
	XColor fg, bg;
	unsigned int maskIndex;
	register struct CursorName *namePtr;
	TkDisplay *dispPtr;

	/*
	 * The cursor is to come from the standard cursor font.  If one
	 * arg, it is cursor name (use black and white for fg and bg).
	 * If two args, they are name and fg color (ignore mask).  If
	 * three args, they are name, fg, bg.  Some of the code below
	 * is stolen from the XCreateFontCursor Xlib procedure.
	 */

	if (argc > 3) {
	    goto badString;
	}
	for (namePtr = cursorNames; ; namePtr++) {
	    if (namePtr->name == NULL) {
		goto badString;
	    }
	    if ((namePtr->name[0] == argv[0][0])
		    && (strcmp(namePtr->name, argv[0]) == 0)) {
		break;
	    }
	}
	maskIndex = namePtr->shape + 1;
	if (argc == 1) {
	    fg.red = fg.green = fg.blue = 0;
	    bg.red = bg.green = bg.blue = 65535;
	} else {
	    if (XParseColor(display, Tk_Colormap(tkwin), argv[1],
		    &fg) == 0) {
		Tcl_AppendResult(interp, "invalid color name \"", argv[1],
			"\"", (char *) NULL);
		goto cleanup;
	    }
	    if (argc == 2) {
		bg.red = bg.green = bg.blue = 0;
		maskIndex = namePtr->shape;
	    } else {
		if (XParseColor(display, Tk_Colormap(tkwin), argv[2],
			&bg) == 0) {
		    Tcl_AppendResult(interp, "invalid color name \"", argv[2],
			    "\"", (char *) NULL);
		    goto cleanup;
		}
	    }
	}
	dispPtr = ((TkWindow *) tkwin)->dispPtr;
	if (dispPtr->cursorFont == None) {
	    dispPtr->cursorFont = XLoadFont(display, CURSORFONT);
	    if (dispPtr->cursorFont == None) {
		interp->result = "couldn't load cursor font";
		goto cleanup;
	    }
	}
	cursor = XCreateGlyphCursor(display, dispPtr->cursorFont,
		dispPtr->cursorFont, namePtr->shape, maskIndex,
		&fg, &bg);
    } else {
	int width, height, maskWidth, maskHeight;
	int xHot, yHot, dummy1, dummy2;
	XColor fg, bg;

	/*
	 * The cursor is to be created by reading bitmap files.  There
	 * should be either two elements in the list (source, color) or
	 * four (source mask fg bg).
	 */

	if ((argc != 2) && (argc != 4)) {
	    goto badString;
	}
	if (XReadBitmapFile(display,
		RootWindowOfScreen(Tk_Screen(tkwin)), &argv[0][1],
		(unsigned int *) &width, (unsigned int *) &height,
		&source, &xHot, &yHot) != BitmapSuccess) {
	    Tcl_AppendResult(interp, "cleanup reading bitmap file \"",
		    &argv[0][1], "\"", (char *) NULL);
	    goto cleanup;
	}
	if ((xHot < 0) || (yHot < 0) || (xHot >= width) || (yHot >= height)) {
	    Tcl_AppendResult(interp, "bad hot spot in bitmap file \"",
		    &argv[0][1], "\"", (char *) NULL);
	    goto cleanup;
	}
	if (argc == 2) {
	    if (XParseColor(display, Tk_Colormap(tkwin), argv[1],
		    &fg) == 0) {
		Tcl_AppendResult(interp, "invalid color name \"",
			argv[1], "\"", (char *) NULL);
		goto cleanup;
	    }
	    cursor = XCreatePixmapCursor(display, source, source,
		    &fg, &fg, (unsigned) xHot, (unsigned) yHot);
	} else {
	    if (XReadBitmapFile(display,
		    RootWindowOfScreen(Tk_Screen(tkwin)), argv[1],
		    (unsigned int *) &maskWidth, (unsigned int *) &maskHeight,
		    &mask, &dummy1, &dummy2) != BitmapSuccess) {
		Tcl_AppendResult(interp, "cleanup reading bitmap file \"",
			argv[1], "\"", (char *) NULL);
		goto cleanup;
	    }
	    if ((maskWidth != width) && (maskHeight != height)) {
		interp->result =
			"source and mask bitmaps have different sizes";
		goto cleanup;
	    }
	    if (XParseColor(display, Tk_Colormap(tkwin), argv[2],
		    &fg) == 0) {
		Tcl_AppendResult(interp, "invalid color name \"", argv[2],
			"\"", (char *) NULL);
		goto cleanup;
	    }
	    if (XParseColor(display, Tk_Colormap(tkwin), argv[3],
		    &bg) == 0) {
		Tcl_AppendResult(interp, "invalid color name \"", argv[3],
			"\"", (char *) NULL);
		goto cleanup;
	    }
	    cursor = XCreatePixmapCursor(display, source, mask,
		    &fg, &bg, (unsigned) xHot, (unsigned) yHot);
	}
    }

    if (cursor != None) {
	cursorPtr = (TkUnixCursor *) ckalloc(sizeof(TkUnixCursor));
	cursorPtr->info.cursor = (Tk_Cursor) cursor;
	cursorPtr->display = display;
    }

    cleanup:
    if (argv != NULL) {
	ckfree((char *) argv);
    }
    if (source != None) {
	XFreePixmap(display, source);
	Tk_FreeXId(display, (XID) source);
    }
    if (mask != None) {
	XFreePixmap(display, mask);
	Tk_FreeXId(display, (XID) mask);
    }
    return (TkCursor *) cursorPtr;


    badString:
    Tcl_AppendResult(interp, "bad cursor spec \"", string, "\"",
	    (char *) NULL);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateCursorFromData --
 *
 *	Creates a cursor from the source and mask bits.
 *
 * Results:
 *	Returns a new cursor, or NULL on errors.
 *
 * Side effects:
 *	Allocates a new cursor.
 *
 *----------------------------------------------------------------------
 */

TkCursor *
TkCreateCursorFromData(tkwin, source, mask, width, height, xHot, yHot,
	fgColor, bgColor)
    Tk_Window tkwin;		/* Window in which cursor will be used. */
    char *source;		/* Bitmap data for cursor shape. */
    char *mask;			/* Bitmap data for cursor mask. */
    int width, height;		/* Dimensions of cursor. */
    int xHot, yHot;		/* Location of hot-spot in cursor. */
    XColor fgColor;		/* Foreground color for cursor. */
    XColor bgColor;		/* Background color for cursor. */
{
    Cursor cursor;
    Pixmap sourcePixmap, maskPixmap;
    TkUnixCursor *cursorPtr = NULL;
    Display *display = Tk_Display(tkwin);

    sourcePixmap = XCreateBitmapFromData(display,
	    RootWindowOfScreen(Tk_Screen(tkwin)), source, (unsigned) width,
	    (unsigned) height);
    maskPixmap = XCreateBitmapFromData(display, 
	    RootWindowOfScreen(Tk_Screen(tkwin)), mask, (unsigned) width,
	    (unsigned) height);
    cursor = XCreatePixmapCursor(display, sourcePixmap,
	    maskPixmap, &fgColor, &bgColor, (unsigned) xHot, (unsigned) yHot);
    XFreePixmap(display, sourcePixmap);
    Tk_FreeXId(display, (XID) sourcePixmap);
    XFreePixmap(display, maskPixmap);
    Tk_FreeXId(display, (XID) maskPixmap);

    if (cursor != None) {
	cursorPtr = (TkUnixCursor *) ckalloc(sizeof(TkUnixCursor));
	cursorPtr->info.cursor = (Tk_Cursor) cursor;
	cursorPtr->display = display;
    }
    return (TkCursor *) cursorPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFreeCursor --
 *
 *	This procedure is called to release a cursor allocated by
 *	TkGetCursorByName.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The cursor data structure is deallocated.
 *
 *----------------------------------------------------------------------
 */

void
TkFreeCursor(cursorPtr)
    TkCursor *cursorPtr;
{
    TkUnixCursor *unixCursorPtr = (TkUnixCursor *) cursorPtr;
    XFreeCursor(unixCursorPtr->display, (Cursor) unixCursorPtr->info.cursor);
    Tk_FreeXId(unixCursorPtr->display, (XID) unixCursorPtr->info.cursor);
    ckfree((char *) unixCursorPtr);
}

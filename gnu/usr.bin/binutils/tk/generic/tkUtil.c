/* 
 * tkUtil.c --
 *
 *	This file contains miscellaneous utility procedures that
 *	are used by the rest of Tk, such as a procedure for drawing
 *	a focus highlight.
 *
 * Copyright (c) 1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUtil.c 1.7 96/02/15 18:53:04
 */
#include "tk.h"
#include "tkPort.h"

/*
 *----------------------------------------------------------------------
 *
 * Tk_DrawFocusHighlight --
 *
 *	This procedure draws a rectangular ring around the outside of
 *	a widget to indicate that it has received the input focus.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A rectangle "width" pixels wide is drawn in "drawable",
 *	corresponding to the outer area of "tkwin".
 *
 *----------------------------------------------------------------------
 */

void
Tk_DrawFocusHighlight(tkwin, gc, width, drawable)
    Tk_Window tkwin;		/* Window whose focus highlight ring is
				 * to be drawn. */
    GC gc;			/* Graphics context to use for drawing
				 * the highlight ring. */
    int width;			/* Width of the highlight ring, in pixels. */
    Drawable drawable;		/* Where to draw the ring (typically a
				 * pixmap for double buffering). */
{
    XRectangle rects[4];

    rects[0].x = 0;
    rects[0].y = 0;
    rects[0].width = Tk_Width(tkwin);
    rects[0].height = width;
    rects[1].x = 0;
    rects[1].y = Tk_Height(tkwin) - width;
    rects[1].width = Tk_Width(tkwin);
    rects[1].height = width;
    rects[2].x = 0;
    rects[2].y = width;
    rects[2].width = width;
    rects[2].height = Tk_Height(tkwin) - 2*width;
    rects[3].x = Tk_Width(tkwin) - width;
    rects[3].y = width;
    rects[3].width = width;
    rects[3].height = rects[2].height;
    XFillRectangles(Tk_Display(tkwin), drawable, gc, rects, 4);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetScrollInfo --
 *
 *	This procedure is invoked to parse "xview" and "yview"
 *	scrolling commands for widgets using the new scrolling
 *	command syntax ("moveto" or "scroll" options).
 *
 * Results:
 *	The return value is either TK_SCROLL_MOVETO, TK_SCROLL_PAGES,
 *	TK_SCROLL_UNITS, or TK_SCROLL_ERROR.  This indicates whether
 *	the command was successfully parsed and what form the command
 *	took.  If TK_SCROLL_MOVETO, *dblPtr is filled in with the
 *	desired position;  if TK_SCROLL_PAGES or TK_SCROLL_UNITS,
 *	*intPtr is filled in with the number of lines to move (may be
 *	negative);  if TK_SCROLL_ERROR, interp->result contains an
 *	error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tk_GetScrollInfo(interp, argc, argv, dblPtr, intPtr)
    Tcl_Interp *interp;			/* Used for error reporting. */
    int argc;				/* # arguments for command. */
    char **argv;			/* Arguments for command. */
    double *dblPtr;			/* Filled in with argument "moveto"
					 * option, if any. */
    int *intPtr;			/* Filled in with number of pages
					 * or lines to scroll, if any. */
{
    int c;
    size_t length;

    length = strlen(argv[2]);
    c = argv[2][0];
    if ((c == 'm') && (strncmp(argv[2], "moveto", length) == 0)) {
	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " ", argv[1], " moveto fraction\"",
		    (char *) NULL);
	    return TK_SCROLL_ERROR;
	}
	if (Tcl_GetDouble(interp, argv[3], dblPtr) != TCL_OK) {
	    return TK_SCROLL_ERROR;
	}
	return TK_SCROLL_MOVETO;
    } else if ((c == 's')
	    && (strncmp(argv[2], "scroll", length) == 0)) {
	if (argc != 5) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " ", argv[1], " scroll number units|pages\"",
		    (char *) NULL);
	    return TK_SCROLL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[3], intPtr) != TCL_OK) {
	    return TK_SCROLL_ERROR;
	}
	length = strlen(argv[4]);
	c = argv[4][0];
	if ((c == 'p') && (strncmp(argv[4], "pages", length) == 0)) {
	    return TK_SCROLL_PAGES;
	} else if ((c == 'u')
		&& (strncmp(argv[4], "units", length) == 0)) {
	    return TK_SCROLL_UNITS;
	} else {
	    Tcl_AppendResult(interp, "bad argument \"", argv[4],
		    "\": must be units or pages", (char *) NULL);
	    return TK_SCROLL_ERROR;
	}
    }
    Tcl_AppendResult(interp, "unknown option \"", argv[2],
	    "\": must be moveto or scroll", (char *) NULL);
    return TK_SCROLL_ERROR;
}

/* 
 * tkSquare.c --
 *
 *	This module implements "square" widgets.  A "square" is
 *	a widget that displays a single square that can be moved
 *	around and resized.  This file is intended as an example
 *	of how to build a widget;  it isn't included in the
 *	normal wish, but it is included in "tktest".
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkSquare.c 1.16 96/02/15 18:52:44
 */

#include "tkPort.h"
#include "tk.h"

/*
 * A data structure of the following type is kept for each square
 * widget managed by this file:
 */

typedef struct {
    Tk_Window tkwin;		/* Window that embodies the square.  NULL
				 * means window has been deleted but
				 * widget record hasn't been cleaned up yet. */
    Display *display;		/* X's token for the window's display. */
    Tcl_Interp *interp;		/* Interpreter associated with widget. */
    Tcl_Command widgetCmd;	/* Token for square's widget command. */
    int x, y;			/* Position of square's upper-left corner
				 * within widget. */
    int size;			/* Width and height of square. */

    /*
     * Information used when displaying widget:
     */

    int borderWidth;		/* Width of 3-D border around whole widget. */
    Tk_3DBorder bgBorder;	/* Used for drawing background. */
    Tk_3DBorder fgBorder;	/* For drawing square. */
    int relief;			/* Indicates whether window as a whole is
				 * raised, sunken, or flat. */
    GC gc;			/* Graphics context for copying from
				 * off-screen pixmap onto screen. */
    int doubleBuffer;		/* Non-zero means double-buffer redisplay
				 * with pixmap;  zero means draw straight
				 * onto the display. */
    int updatePending;		/* Non-zero means a call to SquareDisplay
				 * has already been scheduled. */
} Square;

/*
 * Information used for argv parsing.
 */

static Tk_ConfigSpec configSpecs[] = {
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	"#cdb79e", Tk_Offset(Square, bgBorder), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-background", "background", "Background",
	"white", Tk_Offset(Square, bgBorder), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_SYNONYM, "-bd", "borderWidth", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_SYNONYM, "-bg", "background", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_PIXELS, "-borderwidth", "borderWidth", "BorderWidth",
	"2", Tk_Offset(Square, borderWidth), 0},
    {TK_CONFIG_INT, "-dbl", "doubleBuffer", "DoubleBuffer",
	"1", Tk_Offset(Square, doubleBuffer), 0},
    {TK_CONFIG_SYNONYM, "-fg", "foreground", (char *) NULL,
	(char *) NULL, 0, 0},
    {TK_CONFIG_BORDER, "-foreground", "foreground", "Foreground",
	"#b03060", Tk_Offset(Square, fgBorder), TK_CONFIG_COLOR_ONLY},
    {TK_CONFIG_BORDER, "-foreground", "foreground", "Foreground",
	"black", Tk_Offset(Square, fgBorder), TK_CONFIG_MONO_ONLY},
    {TK_CONFIG_RELIEF, "-relief", "relief", "Relief",
	"raised", Tk_Offset(Square, relief), 0},
    {TK_CONFIG_END, (char *) NULL, (char *) NULL, (char *) NULL,
	(char *) NULL, 0, 0}
};

/*
 * Forward declarations for procedures defined later in this file:
 */

int			SquareCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, int argc, char **argv));
static void		SquareCmdDeletedProc _ANSI_ARGS_((
			    ClientData clientData));
static int		SquareConfigure _ANSI_ARGS_((Tcl_Interp *interp,
			    Square *squarePtr, int argc, char **argv,
			    int flags));
static void		SquareDestroy _ANSI_ARGS_((char *memPtr));
static void		SquareDisplay _ANSI_ARGS_((ClientData clientData));
static void		KeepInWindow _ANSI_ARGS_((Square *squarePtr));
static void		SquareEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static int		SquareWidgetCmd _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *, int argc, char **argv));

/*
 *--------------------------------------------------------------
 *
 * SquareCmd --
 *
 *	This procedure is invoked to process the "square" Tcl
 *	command.  It creates a new "square" widget.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	A new widget is created and configured.
 *
 *--------------------------------------------------------------
 */

int
SquareCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window main = (Tk_Window) clientData;
    Square *squarePtr;
    Tk_Window tkwin;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " pathName ?options?\"", (char *) NULL);
	return TCL_ERROR;
    }

    tkwin = Tk_CreateWindowFromPath(interp, main, argv[1], (char *) NULL);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    Tk_SetClass(tkwin, "Square");

    /*
     * Allocate and initialize the widget record.
     */

    squarePtr = (Square *) ckalloc(sizeof(Square));
    squarePtr->tkwin = tkwin;
    squarePtr->display = Tk_Display(tkwin);
    squarePtr->interp = interp;
    squarePtr->widgetCmd = Tcl_CreateCommand(interp,
	    Tk_PathName(squarePtr->tkwin), SquareWidgetCmd,
	    (ClientData) squarePtr, SquareCmdDeletedProc);
    squarePtr->x = 0;
    squarePtr->y = 0;
    squarePtr->size = 20;
    squarePtr->borderWidth = 0;
    squarePtr->bgBorder = NULL;
    squarePtr->fgBorder = NULL;
    squarePtr->relief = TK_RELIEF_FLAT;
    squarePtr->gc = None;
    squarePtr->doubleBuffer = 1;
    squarePtr->updatePending = 0;

    Tk_CreateEventHandler(squarePtr->tkwin, ExposureMask|StructureNotifyMask,
	    SquareEventProc, (ClientData) squarePtr);
    if (SquareConfigure(interp, squarePtr, argc-2, argv+2, 0) != TCL_OK) {
	Tk_DestroyWindow(squarePtr->tkwin);
	return TCL_ERROR;
    }

    interp->result = Tk_PathName(squarePtr->tkwin);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * SquareWidgetCmd --
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
SquareWidgetCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Information about square widget. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Square *squarePtr = (Square *) clientData;
    int result = TCL_OK;
    size_t length;
    char c;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_Preserve((ClientData) squarePtr);
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'c') && (strncmp(argv[1], "cget", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " cget option\"",
		    (char *) NULL);
	    goto error;
	}
	result = Tk_ConfigureValue(interp, squarePtr->tkwin, configSpecs,
		(char *) squarePtr, argv[2], 0);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)
	    && (length >= 2)) {
	if (argc == 2) {
	    result = Tk_ConfigureInfo(interp, squarePtr->tkwin, configSpecs,
		    (char *) squarePtr, (char *) NULL, 0);
	} else if (argc == 3) {
	    result = Tk_ConfigureInfo(interp, squarePtr->tkwin, configSpecs,
		    (char *) squarePtr, argv[2], 0);
	} else {
	    result = SquareConfigure(interp, squarePtr, argc-2, argv+2,
		    TK_CONFIG_ARGV_ONLY);
	}
    } else if ((c == 'p') && (strncmp(argv[1], "position", length) == 0)) {
	if ((argc != 2) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " position ?x y?\"", (char *) NULL);
	    goto error;
	}
	if (argc == 4) {
	    if ((Tk_GetPixels(interp, squarePtr->tkwin, argv[2],
		    &squarePtr->x) != TCL_OK) || (Tk_GetPixels(interp,
		    squarePtr->tkwin, argv[3], &squarePtr->y) != TCL_OK)) {
		goto error;
	    }
	    KeepInWindow(squarePtr);
	}
	sprintf(interp->result, "%d %d", squarePtr->x, squarePtr->y);
    } else if ((c == 's') && (strncmp(argv[1], "size", length) == 0)) {
	if ((argc != 2) && (argc != 3)) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " size ?amount?\"", (char *) NULL);
	    goto error;
	}
	if (argc == 3) {
	    int i;

	    if (Tk_GetPixels(interp, squarePtr->tkwin, argv[2], &i) != TCL_OK) {
		goto error;
	    }
	    if ((i <= 0) || (i > 100)) {
		Tcl_AppendResult(interp, "bad size \"", argv[2],
			"\"", (char *) NULL);
		goto error;
	    }
	    squarePtr->size = i;
	    KeepInWindow(squarePtr);
	}
	sprintf(interp->result, "%d", squarePtr->size);
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be cget, configure, position, or size",
		(char *) NULL);
	goto error;
    }
    if (!squarePtr->updatePending) {
	Tcl_DoWhenIdle(SquareDisplay, (ClientData) squarePtr);
	squarePtr->updatePending = 1;
    }
    Tcl_Release((ClientData) squarePtr);
    return result;

    error:
    Tcl_Release((ClientData) squarePtr);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * SquareConfigure --
 *
 *	This procedure is called to process an argv/argc list in
 *	conjunction with the Tk option database to configure (or
 *	reconfigure) a square widget.
 *
 * Results:
 *	The return value is a standard Tcl result.  If TCL_ERROR is
 *	returned, then interp->result contains an error message.
 *
 * Side effects:
 *	Configuration information, such as colors, border width,
 *	etc. get set for squarePtr;  old resources get freed,
 *	if there were any.
 *
 *----------------------------------------------------------------------
 */

static int
SquareConfigure(interp, squarePtr, argc, argv, flags)
    Tcl_Interp *interp;			/* Used for error reporting. */
    Square *squarePtr;			/* Information about widget. */
    int argc;				/* Number of valid entries in argv. */
    char **argv;			/* Arguments. */
    int flags;				/* Flags to pass to
					 * Tk_ConfigureWidget. */
{
    if (Tk_ConfigureWidget(interp, squarePtr->tkwin, configSpecs,
	    argc, argv, (char *) squarePtr, flags) != TCL_OK) {
	return TCL_ERROR;
    }

    /*
     * Set the background for the window and create a graphics context
     * for use during redisplay.
     */

    Tk_SetWindowBackground(squarePtr->tkwin,
	    Tk_3DBorderColor(squarePtr->bgBorder)->pixel);
    if ((squarePtr->gc == None) && (squarePtr->doubleBuffer)) {
	XGCValues gcValues;
	gcValues.function = GXcopy;
	gcValues.graphics_exposures = False;
	squarePtr->gc = Tk_GetGC(squarePtr->tkwin,
		GCFunction|GCGraphicsExposures, &gcValues);
    }

    /*
     * Register the desired geometry for the window.  Then arrange for
     * the window to be redisplayed.
     */

    Tk_GeometryRequest(squarePtr->tkwin, 200, 150);
    Tk_SetInternalBorder(squarePtr->tkwin, squarePtr->borderWidth);
    if (!squarePtr->updatePending) {
	Tcl_DoWhenIdle(SquareDisplay, (ClientData) squarePtr);
	squarePtr->updatePending = 1;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * SquareEventProc --
 *
 *	This procedure is invoked by the Tk dispatcher for various
 *	events on squares.
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
SquareEventProc(clientData, eventPtr)
    ClientData clientData;	/* Information about window. */
    XEvent *eventPtr;		/* Information about event. */
{
    Square *squarePtr = (Square *) clientData;

    if (eventPtr->type == Expose) {
	if (!squarePtr->updatePending) {
	    Tcl_DoWhenIdle(SquareDisplay, (ClientData) squarePtr);
	    squarePtr->updatePending = 1;
	}
    } else if (eventPtr->type == ConfigureNotify) {
	KeepInWindow(squarePtr);
	if (!squarePtr->updatePending) {
	    Tcl_DoWhenIdle(SquareDisplay, (ClientData) squarePtr);
	    squarePtr->updatePending = 1;
	}
    } else if (eventPtr->type == DestroyNotify) {
	if (squarePtr->tkwin != NULL) {
	    squarePtr->tkwin = NULL;
	    Tcl_DeleteCommand(squarePtr->interp,
		    Tcl_GetCommandName(squarePtr->interp,
		    squarePtr->widgetCmd));
	}
	if (squarePtr->updatePending) {
	    Tcl_CancelIdleCall(SquareDisplay, (ClientData) squarePtr);
	}
	Tcl_EventuallyFree((ClientData) squarePtr, SquareDestroy);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SquareCmdDeletedProc --
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
SquareCmdDeletedProc(clientData)
    ClientData clientData;	/* Pointer to widget record for widget. */
{
    Square *squarePtr = (Square *) clientData;
    Tk_Window tkwin = squarePtr->tkwin;

    /*
     * This procedure could be invoked either because the window was
     * destroyed and the command was then deleted (in which case tkwin
     * is NULL) or because the command was deleted, and then this procedure
     * destroys the widget.
     */

    if (tkwin != NULL) {
	squarePtr->tkwin = NULL;
	Tk_DestroyWindow(tkwin);
    }
}

/*
 *--------------------------------------------------------------
 *
 * SquareDisplay --
 *
 *	This procedure redraws the contents of a square window.
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
SquareDisplay(clientData)
    ClientData clientData;	/* Information about window. */
{
    Square *squarePtr = (Square *) clientData;
    Tk_Window tkwin = squarePtr->tkwin;
    Pixmap pm = None;
    Drawable d;

    squarePtr->updatePending = 0;
    if (!Tk_IsMapped(tkwin)) {
	return;
    }

    /*
     * Create a pixmap for double-buffering, if necessary.
     */

    if (squarePtr->doubleBuffer) {
	pm = XCreatePixmap(Tk_Display(tkwin), Tk_WindowId(tkwin),
		(unsigned) Tk_Width(tkwin), (unsigned) Tk_Height(tkwin),
		(unsigned) DefaultDepthOfScreen(Tk_Screen(tkwin)));
	d = pm;
    } else {
	d = Tk_WindowId(tkwin);
    }

    /*
     * Redraw the widget's background and border.
     */

    Tk_Fill3DRectangle(tkwin, d, squarePtr->bgBorder, 0, 0, Tk_Width(tkwin),
	    Tk_Height(tkwin), squarePtr->borderWidth, squarePtr->relief);

    /*
     * Display the square.
     */

    Tk_Fill3DRectangle(tkwin, d, squarePtr->fgBorder, squarePtr->x,
	    squarePtr->y, squarePtr->size, squarePtr->size,
	    squarePtr->borderWidth, TK_RELIEF_RAISED);

    /*
     * If double-buffered, copy to the screen and release the pixmap.
     */

    if (squarePtr->doubleBuffer) {
	XCopyArea(Tk_Display(tkwin), pm, Tk_WindowId(tkwin), squarePtr->gc,
		0, 0, (unsigned) Tk_Width(tkwin), (unsigned) Tk_Height(tkwin),
		0, 0);
	XFreePixmap(Tk_Display(tkwin), pm);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * SquareDestroy --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a square at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the square is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
SquareDestroy(memPtr)
    char *memPtr;		/* Info about square widget. */
{
    Square *squarePtr = (Square *) memPtr;

    Tk_FreeOptions(configSpecs, (char *) squarePtr, squarePtr->display, 0);
    if (squarePtr->gc != None) {
	Tk_FreeGC(squarePtr->display, squarePtr->gc);
    }
    ckfree((char *) squarePtr);
}

/*
 *----------------------------------------------------------------------
 *
 * KeepInWindow --
 *
 *	Adjust the position of the square if necessary to keep it in
 *	the widget's window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The x and y position of the square are adjusted if necessary
 *	to keep the square in the window.
 *
 *----------------------------------------------------------------------
 */

static void
KeepInWindow(squarePtr)
    register Square *squarePtr;		/* Pointer to widget record. */
{
    int i, bd;
    bd = 0;
    if (squarePtr->relief != TK_RELIEF_FLAT) {
	bd = squarePtr->borderWidth;
    }
    i = (Tk_Width(squarePtr->tkwin) - bd) - (squarePtr->x + squarePtr->size);
    if (i < 0) {
	squarePtr->x += i;
    }
    i = (Tk_Height(squarePtr->tkwin) - bd) - (squarePtr->y + squarePtr->size);
    if (i < 0) {
	squarePtr->y += i;
    }
    if (squarePtr->x < bd) {
	squarePtr->x = bd;
    }
    if (squarePtr->y < bd) {
	squarePtr->y = bd;
    }
}

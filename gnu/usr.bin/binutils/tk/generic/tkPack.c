/* 
 * tkPack.c --
 *
 *	This file contains code to implement the "packer"
 *	geometry manager for Tk.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkPack.c 1.63 96/02/15 18:52:33
 */

#include "tkPort.h"
#include "tkInt.h"

typedef enum {TOP, BOTTOM, LEFT, RIGHT} Side;

/* For each window that the packer cares about (either because
 * the window is managed by the packer or because the window
 * has slaves that are managed by the packer), there is a
 * structure of the following type:
 */

typedef struct Packer {
    Tk_Window tkwin;		/* Tk token for window.  NULL means that
				 * the window has been deleted, but the
				 * packet hasn't had a chance to clean up
				 * yet because the structure is still in
				 * use. */
    struct Packer *masterPtr;	/* Master window within which this window
				 * is packed (NULL means this window
				 * isn't managed by the packer). */
    struct Packer *nextPtr;	/* Next window packed within same
				 * parent.  List is priority-ordered:
				 * first on list gets packed first. */
    struct Packer *slavePtr;	/* First in list of slaves packed
				 * inside this window (NULL means
				 * no packed slaves). */
    Side side;			/* Side of parent against which
				 * this window is packed. */
    Tk_Anchor anchor;		/* If frame allocated for window is larger
				 * than window needs, this indicates how
				 * where to position window in frame. */
    int padX, padY;		/* Total additional pixels to leave around the
				 * window (half of this space is left on each
				 * side).  This is space *outside* the window:
				 * we'll allocate extra space in frame but
				 * won't enlarge window). */
    int iPadX, iPadY;		/* Total extra pixels to allocate inside the
				 * window (half this amount will appear on
				 * each side). */
    int doubleBw;		/* Twice the window's last known border
				 * width.  If this changes, the window
				 * must be repacked within its parent. */
    int *abortPtr;		/* If non-NULL, it means that there is a nested
				 * call to ArrangePacking already working on
				 * this window.  *abortPtr may be set to 1 to
				 * abort that nested call.  This happens, for
				 * example, if tkwin or any of its slaves
				 * is deleted. */
    int flags;			/* Miscellaneous flags;  see below
				 * for definitions. */
} Packer;

/*
 * Flag values for Packer structures:
 *
 * REQUESTED_REPACK:		1 means a Tcl_DoWhenIdle request
 *				has already been made to repack
 *				all the slaves of this window.
 * FILLX:			1 means if frame allocated for window
 *				is wider than window needs, expand window
 *				to fill frame.  0 means don't make window
 *				any larger than needed.
 * FILLY:			Same as FILLX, except for height.
 * EXPAND:			1 means this window's frame will absorb any
 *				extra space in the parent window.
 * OLD_STYLE:			1 means this window is being managed with
 *				the old-style packer algorithms (before
 *				Tk version 3.3).  The main difference is
 *				that padding and filling are done differently.
 * DONT_PROPAGATE:		1 means don't set this window's requested
 *				size.  0 means if this window is a master
 *				then Tk will set its requested size to fit
 *				the needs of its slaves.
 */

#define REQUESTED_REPACK	1
#define FILLX			2
#define FILLY			4
#define EXPAND			8
#define OLD_STYLE		16
#define DONT_PROPAGATE		32

/*
 * Hash table used to map from Tk_Window tokens to corresponding
 * Packer structures:
 */

static Tcl_HashTable packerHashTable;

/*
 * Have statics in this module been initialized?
 */

static int initialized = 0;

/*
 * The following structure is the official type record for the
 * packer:
 */

static void		PackReqProc _ANSI_ARGS_((ClientData clientData,
			    Tk_Window tkwin));
static void		PackLostSlaveProc _ANSI_ARGS_((ClientData clientData,
			    Tk_Window tkwin));

static Tk_GeomMgr packerType = {
    "pack",			/* name */
    PackReqProc,		/* requestProc */
    PackLostSlaveProc,		/* lostSlaveProc */
};

/*
 * Forward declarations for procedures defined later in this file:
 */

static void		ArrangePacking _ANSI_ARGS_((ClientData clientData));
static int		ConfigureSlaves _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, int argc, char *argv[]));
static Packer *		GetPacker _ANSI_ARGS_((Tk_Window tkwin));
static int		PackAfter _ANSI_ARGS_((Tcl_Interp *interp,
			    Packer *prevPtr, Packer *masterPtr, int argc,
			    char **argv));
static void		PackReqProc _ANSI_ARGS_((ClientData clientData,
			    Tk_Window tkwin));
static void		PackStructureProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		Unlink _ANSI_ARGS_((Packer *packPtr));
static int		XExpansion _ANSI_ARGS_((Packer *slavePtr,
			    int cavityWidth));
static int		YExpansion _ANSI_ARGS_((Packer *slavePtr,
			    int cavityHeight));

/*
 *--------------------------------------------------------------
 *
 * Tk_PackCmd --
 *
 *	This procedure is invoked to process the "pack" Tcl command.
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

int
Tk_PackCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    size_t length;
    int c;

    if ((argc >= 2) && (argv[1][0] == '.')) {
	return ConfigureSlaves(interp, tkwin, argc-1, argv+1);
    }
    if (argc < 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (length >= 2)
	    && (strncmp(argv[1], "after", length) == 0)) {
	Packer *prevPtr;
	Tk_Window tkwin2;

	tkwin2 = Tk_NameToWindow(interp, argv[2], tkwin);
	if (tkwin2 == NULL) {
	    return TCL_ERROR;
	}
	prevPtr = GetPacker(tkwin2);
	if (prevPtr->masterPtr == NULL) {
	    Tcl_AppendResult(interp, "window \"", argv[2],
		    "\" isn't packed", (char *) NULL);
	    return TCL_ERROR;
	}
	return PackAfter(interp, prevPtr, prevPtr->masterPtr, argc-3, argv+3);
    } else if ((c == 'a') && (length >= 2)
	    && (strncmp(argv[1], "append", length) == 0)) {
	Packer *masterPtr;
	register Packer *prevPtr;
	Tk_Window tkwin2;

	tkwin2 = Tk_NameToWindow(interp, argv[2], tkwin);
	if (tkwin2 == NULL) {
	    return TCL_ERROR;
	}
	masterPtr = GetPacker(tkwin2);
	prevPtr = masterPtr->slavePtr;
	if (prevPtr != NULL) {
	    while (prevPtr->nextPtr != NULL) {
		prevPtr = prevPtr->nextPtr;
	    }
	}
	return PackAfter(interp, prevPtr, masterPtr, argc-3, argv+3);
    } else if ((c == 'b') && (strncmp(argv[1], "before", length) == 0)) {
	Packer *packPtr, *masterPtr;
	register Packer *prevPtr;
	Tk_Window tkwin2;

	tkwin2 = Tk_NameToWindow(interp, argv[2], tkwin);
	if (tkwin2 == NULL) {
	    return TCL_ERROR;
	}
	packPtr = GetPacker(tkwin2);
	if (packPtr->masterPtr == NULL) {
	    Tcl_AppendResult(interp, "window \"", argv[2],
		    "\" isn't packed", (char *) NULL);
	    return TCL_ERROR;
	}
	masterPtr = packPtr->masterPtr;
	prevPtr = masterPtr->slavePtr;
	if (prevPtr == packPtr) {
	    prevPtr = NULL;
	} else {
	    for ( ; ; prevPtr = prevPtr->nextPtr) {
		if (prevPtr == NULL) {
		    panic("\"pack before\" couldn't find predecessor");
		}
		if (prevPtr->nextPtr == packPtr) {
		    break;
		}
	    }
	}
	return PackAfter(interp, prevPtr, masterPtr, argc-3, argv+3);
    } else if ((c == 'c') && (strncmp(argv[1], "configure", length) == 0)) {
	if (argv[2][0] != '.') {
	    Tcl_AppendResult(interp, "bad argument \"", argv[2],
		    "\": must be name of window", (char *) NULL);
	    return TCL_ERROR;
	}
	return ConfigureSlaves(interp, tkwin, argc-2, argv+2);
    } else if ((c == 'f') && (strncmp(argv[1], "forget", length) == 0)) {
	Tk_Window slave;
	Packer *slavePtr;
	int i;

	for (i = 2; i < argc; i++) {
	    slave = Tk_NameToWindow(interp, argv[i], tkwin);
	    if (slave == NULL) {
		continue;
	    }
	    slavePtr = GetPacker(slave);
	    if ((slavePtr != NULL) && (slavePtr->masterPtr != NULL)) {
		Tk_ManageGeometry(slave, (Tk_GeomMgr *) NULL,
			(ClientData) NULL);
		if (slavePtr->masterPtr->tkwin != Tk_Parent(slavePtr->tkwin)) {
		    Tk_UnmaintainGeometry(slavePtr->tkwin,
			    slavePtr->masterPtr->tkwin);
		}
		Unlink(slavePtr);
		Tk_UnmapWindow(slavePtr->tkwin);
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "info", length) == 0)) {
	register Packer *slavePtr;
	Tk_Window slave;
	char buffer[300];
	static char *sideNames[] = {"top", "bottom", "left", "right"};

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " info window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	slave = Tk_NameToWindow(interp, argv[2], tkwin);
	if (slave == NULL) {
	    return TCL_ERROR;
	}
	slavePtr = GetPacker(slave);
	if (slavePtr->masterPtr == NULL) {
	    Tcl_AppendResult(interp, "window \"", argv[2],
		    "\" isn't packed", (char *) NULL);
	    return TCL_ERROR;
	}
	Tcl_AppendElement(interp, "-in");
	Tcl_AppendElement(interp, Tk_PathName(slavePtr->masterPtr->tkwin));
	Tcl_AppendElement(interp, "-anchor");
	Tcl_AppendElement(interp, Tk_NameOfAnchor(slavePtr->anchor));
	Tcl_AppendResult(interp, " -expand ",
		(slavePtr->flags & EXPAND) ? "1" : "0", " -fill ",
		(char *) NULL);
	switch (slavePtr->flags & (FILLX|FILLY)) {
	    case 0:
		Tcl_AppendResult(interp, "none", (char *) NULL);
		break;
	    case FILLX:
		Tcl_AppendResult(interp, "x", (char *) NULL);
		break;
	    case FILLY:
		Tcl_AppendResult(interp, "y", (char *) NULL);
		break;
	    case FILLX|FILLY:
		Tcl_AppendResult(interp, "both", (char *) NULL);
		break;
	}
	sprintf(buffer, " -ipadx %d -ipady %d -padx %d -pady %d",
		slavePtr->iPadX/2, slavePtr->iPadY/2, slavePtr->padX/2,
		slavePtr->padY/2);
	Tcl_AppendResult(interp, buffer, " -side ", sideNames[slavePtr->side],
		(char *) NULL);
    } else if ((c == 'p') && (strncmp(argv[1], "propagate", length) == 0)) {
	Tk_Window master;
	Packer *masterPtr;
	int propagate;

	if (argc > 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " propagate window ?boolean?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	master = Tk_NameToWindow(interp, argv[2], tkwin);
	if (master == NULL) {
	    return TCL_ERROR;
	}
	masterPtr = GetPacker(master);
	if (argc == 3) {
	    if (masterPtr->flags & DONT_PROPAGATE) {
		interp->result = "0";
	    } else {
		interp->result = "1";
	    }
	    return TCL_OK;
	}
	if (Tcl_GetBoolean(interp, argv[3], &propagate) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (propagate) {
	    masterPtr->flags &= ~DONT_PROPAGATE;

	    /*
	     * Repack the master to allow new geometry information to
	     * propagate upwards to the master's master.
	     */

	    if (masterPtr->abortPtr != NULL) {
		*masterPtr->abortPtr = 1;
	    }
	    if (!(masterPtr->flags & REQUESTED_REPACK)) {
		masterPtr->flags |= REQUESTED_REPACK;
		Tcl_DoWhenIdle(ArrangePacking, (ClientData) masterPtr);
	    }
	} else {
	    masterPtr->flags |= DONT_PROPAGATE;
	}
    } else if ((c == 's') && (strncmp(argv[1], "slaves", length) == 0)) {
	Tk_Window master;
	Packer *masterPtr, *slavePtr;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " slaves window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	master = Tk_NameToWindow(interp, argv[2], tkwin);
	if (master == NULL) {
	    return TCL_ERROR;
	}
	masterPtr = GetPacker(master);
	for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
		slavePtr = slavePtr->nextPtr) {
	    Tcl_AppendElement(interp, Tk_PathName(slavePtr->tkwin));
	}
    } else if ((c == 'u') && (strncmp(argv[1], "unpack", length) == 0)) {
	Tk_Window tkwin2;
	Packer *packPtr;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " unpack window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	tkwin2 = Tk_NameToWindow(interp, argv[2], tkwin);
	if (tkwin2 == NULL) {
	    return TCL_ERROR;
	}
	packPtr = GetPacker(tkwin2);
	if ((packPtr != NULL) && (packPtr->masterPtr != NULL)) {
	    Tk_ManageGeometry(tkwin2, (Tk_GeomMgr *) NULL,
		    (ClientData) NULL);
	    if (packPtr->masterPtr->tkwin != Tk_Parent(packPtr->tkwin)) {
		Tk_UnmaintainGeometry(packPtr->tkwin,
			packPtr->masterPtr->tkwin);
	    }
	    Unlink(packPtr);
	    Tk_UnmapWindow(packPtr->tkwin);
	}
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be configure, forget, info, ",
		"propagate, or slaves", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * PackReqProc --
 *
 *	This procedure is invoked by Tk_GeometryRequest for
 *	windows managed by the packer.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arranges for tkwin, and all its managed siblings, to
 *	be re-packed at the next idle point.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
PackReqProc(clientData, tkwin)
    ClientData clientData;	/* Packer's information about
				 * window that got new preferred
				 * geometry.  */
    Tk_Window tkwin;		/* Other Tk-related information
				 * about the window. */
{
    register Packer *packPtr = (Packer *) clientData;

    packPtr = packPtr->masterPtr;
    if (!(packPtr->flags & REQUESTED_REPACK)) {
	packPtr->flags |= REQUESTED_REPACK;
	Tcl_DoWhenIdle(ArrangePacking, (ClientData) packPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * PackLostSlaveProc --
 *
 *	This procedure is invoked by Tk whenever some other geometry
 *	claims control over a slave that used to be managed by us.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Forgets all packer-related information about the slave.
 *
 *--------------------------------------------------------------
 */

	/* ARGSUSED */
static void
PackLostSlaveProc(clientData, tkwin)
    ClientData clientData;	/* Packer structure for slave window that
				 * was stolen away. */
    Tk_Window tkwin;		/* Tk's handle for the slave window. */
{
    register Packer *slavePtr = (Packer *) clientData;

    if (slavePtr->masterPtr->tkwin != Tk_Parent(slavePtr->tkwin)) {
	Tk_UnmaintainGeometry(slavePtr->tkwin, slavePtr->masterPtr->tkwin);
    }
    Unlink(slavePtr);
    Tk_UnmapWindow(slavePtr->tkwin);
}

/*
 *--------------------------------------------------------------
 *
 * ArrangePacking --
 *
 *	This procedure is invoked (using the Tcl_DoWhenIdle
 *	mechanism) to re-layout a set of windows managed by
 *	the packer.  It is invoked at idle time so that a
 *	series of packer requests can be merged into a single
 *	layout operation.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The packed slaves of masterPtr may get resized or
 *	moved.
 *
 *--------------------------------------------------------------
 */

static void
ArrangePacking(clientData)
    ClientData clientData;	/* Structure describing parent whose slaves
				 * are to be re-layed out. */
{
    register Packer *masterPtr = (Packer *) clientData;
    register Packer *slavePtr;	
    int cavityX, cavityY, cavityWidth, cavityHeight;
				/* These variables keep track of the
				 * as-yet-unallocated space remaining in
				 * the middle of the parent window. */
    int frameX, frameY, frameWidth, frameHeight;
				/* These variables keep track of the frame
				 * allocated to the current window. */
    int x, y, width, height;	/* These variables are used to hold the
				 * actual geometry of the current window. */
    int intBWidth;		/* Width of internal border in parent window,
				 * if any. */
    int abort;			/* May get set to non-zero to abort this
				 * repacking operation. */
    int borderX, borderY;
    int maxWidth, maxHeight, tmp;

    masterPtr->flags &= ~REQUESTED_REPACK;

    /*
     * If the parent has no slaves anymore, then don't do anything
     * at all:  just leave the parent's size as-is.
     */

    if (masterPtr->slavePtr == NULL) {
	return;
    }

    /*
     * Abort any nested call to ArrangePacking for this window, since
     * we'll do everything necessary here, and set up so this call
     * can be aborted if necessary.  
     */

    if (masterPtr->abortPtr != NULL) {
	*masterPtr->abortPtr = 1;
    }
    masterPtr->abortPtr = &abort;
    abort = 0;
    Tcl_Preserve((ClientData) masterPtr);

    /*
     * Pass #1: scan all the slaves to figure out the total amount
     * of space needed.  Two separate width and height values are
     * computed:
     *
     * width -		Holds the sum of the widths (plus padding) of
     *			all the slaves seen so far that were packed LEFT
     *			or RIGHT.
     * height -		Holds the sum of the heights (plus padding) of
     *			all the slaves seen so far that were packed TOP
     *			or BOTTOM.
     *
     * maxWidth -	Gradually builds up the width needed by the master
     *			to just barely satisfy all the slave's needs.  For
     *			each slave, the code computes the width needed for
     *			all the slaves so far and updates maxWidth if the
     *			new value is greater.
     * maxHeight -	Same as maxWidth, except keeps height info.
     */

    intBWidth = Tk_InternalBorderWidth(masterPtr->tkwin);
    width = height = maxWidth = maxHeight = 2*intBWidth;
    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
	    slavePtr = slavePtr->nextPtr) {
	if ((slavePtr->side == TOP) || (slavePtr->side == BOTTOM)) {
	    tmp = Tk_ReqWidth(slavePtr->tkwin) + slavePtr->doubleBw
		    + slavePtr->padX + slavePtr->iPadX + width;
	    if (tmp > maxWidth) {
		maxWidth = tmp;
	    }
	    height += Tk_ReqHeight(slavePtr->tkwin) + slavePtr->doubleBw
		    + slavePtr->padY + slavePtr->iPadY;
	} else {
	    tmp = Tk_ReqHeight(slavePtr->tkwin) + slavePtr->doubleBw
		    + slavePtr->padY + slavePtr->iPadY + height;
	    if (tmp > maxHeight) {
		maxHeight = tmp;
	    }
	    width += Tk_ReqWidth(slavePtr->tkwin) + slavePtr->doubleBw
		    + slavePtr->padX + slavePtr->iPadX;
	}
    }
    if (width > maxWidth) {
	maxWidth = width;
    }
    if (height > maxHeight) {
	maxHeight = height;
    }

    /*
     * If the total amount of space needed in the parent window has
     * changed, and if we're propagating geometry information, then
     * notify the next geometry manager up and requeue ourselves to
     * start again after the parent has had a chance to
     * resize us.
     */

    if (((maxWidth != Tk_ReqWidth(masterPtr->tkwin))
	    || (maxHeight != Tk_ReqHeight(masterPtr->tkwin)))
	    && !(masterPtr->flags & DONT_PROPAGATE)) {
	Tk_GeometryRequest(masterPtr->tkwin, maxWidth, maxHeight);
	masterPtr->flags |= REQUESTED_REPACK;
	Tcl_DoWhenIdle(ArrangePacking, (ClientData) masterPtr);
	goto done;
    }

    /*
     * Pass #2: scan the slaves a second time assigning
     * new sizes.  The "cavity" variables keep track of the
     * unclaimed space in the cavity of the window;  this
     * shrinks inward as we allocate windows around the
     * edges.  The "frame" variables keep track of the space
     * allocated to the current window and its frame.  The
     * current window is then placed somewhere inside the
     * frame, depending on anchor.
     */

    cavityX = cavityY = x = y = intBWidth;
    cavityWidth = Tk_Width(masterPtr->tkwin) - 2*intBWidth;
    cavityHeight = Tk_Height(masterPtr->tkwin) - 2*intBWidth;
    for (slavePtr = masterPtr->slavePtr; slavePtr != NULL;
	    slavePtr = slavePtr->nextPtr) {
	if ((slavePtr->side == TOP) || (slavePtr->side == BOTTOM)) {
	    frameWidth = cavityWidth;
	    frameHeight = Tk_ReqHeight(slavePtr->tkwin) + slavePtr->doubleBw
		    + slavePtr->padY + slavePtr->iPadY;
	    if (slavePtr->flags & EXPAND) {
		frameHeight += YExpansion(slavePtr, cavityHeight);
	    }
	    cavityHeight -= frameHeight;
	    if (cavityHeight < 0) {
		frameHeight += cavityHeight;
		cavityHeight = 0;
	    }
	    frameX = cavityX;
	    if (slavePtr->side == TOP) {
		frameY = cavityY;
		cavityY += frameHeight;
	    } else {
		frameY = cavityY + cavityHeight;
	    }
	} else {
	    frameHeight = cavityHeight;
	    frameWidth = Tk_ReqWidth(slavePtr->tkwin) + slavePtr->doubleBw
		    + slavePtr->padX + slavePtr->iPadX;
	    if (slavePtr->flags & EXPAND) {
		frameWidth += XExpansion(slavePtr, cavityWidth);
	    }
	    cavityWidth -= frameWidth;
	    if (cavityWidth < 0) {
		frameWidth += cavityWidth;
		cavityWidth = 0;
	    }
	    frameY = cavityY;
	    if (slavePtr->side == LEFT) {
		frameX = cavityX;
		cavityX += frameWidth;
	    } else {
		frameX = cavityX + cavityWidth;
	    }
	}

	/*
	 * Now that we've got the size of the frame for the window,
	 * compute the window's actual size and location using the
	 * fill, padding, and frame factors.  The variables "borderX"
	 * and "borderY" are used to handle the differences between
	 * old-style packing and the new style (in old-style, iPadX
	 * and iPadY are always zero and padding is completely ignored
	 * except when computing frame size).
	 */

	if (slavePtr->flags & OLD_STYLE) {
	    borderX = borderY = 0;
	} else {
	    borderX = slavePtr->padX;
	    borderY = slavePtr->padY;
	}
	width = Tk_ReqWidth(slavePtr->tkwin) + slavePtr->doubleBw
		+ slavePtr->iPadX;
	if ((slavePtr->flags & FILLX)
		|| (width > (frameWidth - borderX))) {
	    width = frameWidth - borderX;
	}
	height = Tk_ReqHeight(slavePtr->tkwin) + slavePtr->doubleBw
		+ slavePtr->iPadY;
	if ((slavePtr->flags & FILLY)
		|| (height > (frameHeight - borderY))) {
	    height = frameHeight - borderY;
	}
	borderX /= 2;
	borderY /= 2;
	switch (slavePtr->anchor) {
	    case TK_ANCHOR_N:
		x = frameX + (frameWidth - width)/2;
		y = frameY + borderY;
		break;
	    case TK_ANCHOR_NE:
		x = frameX + frameWidth - width - borderX;
		y = frameY + borderY;
		break;
	    case TK_ANCHOR_E:
		x = frameX + frameWidth - width - borderX;
		y = frameY + (frameHeight - height)/2;
		break;
	    case TK_ANCHOR_SE:
		x = frameX + frameWidth - width - borderX;
		y = frameY + frameHeight - height - borderY;
		break;
	    case TK_ANCHOR_S:
		x = frameX + (frameWidth - width)/2;
		y = frameY + frameHeight - height - borderY;
		break;
	    case TK_ANCHOR_SW:
		x = frameX + borderX;
		y = frameY + frameHeight - height - borderY;
		break;
	    case TK_ANCHOR_W:
		x = frameX + borderX;
		y = frameY + (frameHeight - height)/2;
		break;
	    case TK_ANCHOR_NW:
		x = frameX + borderX;
		y = frameY + borderY;
		break;
	    case TK_ANCHOR_CENTER:
		x = frameX + (frameWidth - width)/2;
		y = frameY + (frameHeight - height)/2;
		break;
	    default:
		panic("bad frame factor in ArrangePacking");
	}
	width -= slavePtr->doubleBw;
	height -= slavePtr->doubleBw;

	/*
	 * The final step is to set the position, size, and mapped/unmapped
	 * state of the slave.  If the slave is a child of the master, then
	 * do this here.  Otherwise let Tk_MaintainGeometry do the work.
	 */

	if (masterPtr->tkwin == Tk_Parent(slavePtr->tkwin)) {
	    if ((width <= 0) || (height <= 0)) {
		Tk_UnmapWindow(slavePtr->tkwin);
	    } else {
		if ((x != Tk_X(slavePtr->tkwin))
			|| (y != Tk_Y(slavePtr->tkwin))
			|| (width != Tk_Width(slavePtr->tkwin))
			|| (height != Tk_Height(slavePtr->tkwin))) {
		    Tk_MoveResizeWindow(slavePtr->tkwin, x, y, width, height);
		}
		if (abort) {
		    goto done;
		}

		/*
		 * Don't map the slave if the master isn't mapped: wait
		 * until the master gets mapped later.
		 */

		if (Tk_IsMapped(masterPtr->tkwin)) {
		    Tk_MapWindow(slavePtr->tkwin);
		}
	    }
	} else {
	    if ((width <= 0) || (height <= 0)) {
		Tk_UnmaintainGeometry(slavePtr->tkwin, masterPtr->tkwin);
		Tk_UnmapWindow(slavePtr->tkwin);
	    } else {
		Tk_MaintainGeometry(slavePtr->tkwin, masterPtr->tkwin,
			x, y, width, height);
	    }
	}

	/*
	 * Changes to the window's structure could cause almost anything
	 * to happen, including deleting the parent or child.  If this
	 * happens, we'll be told to abort.
	 */

	if (abort) {
	    goto done;
	}
    }

    done:
    masterPtr->abortPtr = NULL;
    Tcl_Release((ClientData) masterPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * XExpansion --
 *
 *	Given a list of packed slaves, the first of which is packed
 *	on the left or right and is expandable, compute how much to
 *	expand the child.
 *
 * Results:
 *	The return value is the number of additional pixels to give to
 *	the child.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
XExpansion(slavePtr, cavityWidth)
    register Packer *slavePtr;		/* First in list of remaining
					 * slaves. */
    int cavityWidth;			/* Horizontal space left for all
					 * remaining slaves. */
{
    int numExpand, minExpand, curExpand;
    int childWidth;

    /*
     * This procedure is tricky because windows packed top or bottom can
     * be interspersed among expandable windows packed left or right.
     * Scan through the list, keeping a running sum of the widths of
     * all left and right windows (actually, count the cavity space not
     * allocated) and a running count of all expandable left and right
     * windows.  At each top or bottom window, and at the end of the
     * list, compute the expansion factor that seems reasonable at that
     * point.  Return the smallest factor seen at any of these points.
     */

    minExpand = cavityWidth;
    numExpand = 0;
    for ( ; slavePtr != NULL; slavePtr = slavePtr->nextPtr) {
	childWidth = Tk_ReqWidth(slavePtr->tkwin) + slavePtr->doubleBw
		+ slavePtr->padX + slavePtr->iPadX;
	if ((slavePtr->side == TOP) || (slavePtr->side == BOTTOM)) {
	    curExpand = (cavityWidth - childWidth)/numExpand;
	    if (curExpand < minExpand) {
		minExpand = curExpand;
	    }
	} else {
	    cavityWidth -= childWidth;
	    if (slavePtr->flags & EXPAND) {
		numExpand++;
	    }
	}
    }
    curExpand = cavityWidth/numExpand;
    if (curExpand < minExpand) {
	minExpand = curExpand;
    }
    return (minExpand < 0) ? 0 : minExpand;
}

/*
 *----------------------------------------------------------------------
 *
 * YExpansion --
 *
 *	Given a list of packed slaves, the first of which is packed
 *	on the top or bottom and is expandable, compute how much to
 *	expand the child.
 *
 * Results:
 *	The return value is the number of additional pixels to give to
 *	the child.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
YExpansion(slavePtr, cavityHeight)
    register Packer *slavePtr;		/* First in list of remaining
					 * slaves. */
    int cavityHeight;			/* Vertical space left for all
					 * remaining slaves. */
{
    int numExpand, minExpand, curExpand;
    int childHeight;

    /*
     * See comments for XExpansion.
     */

    minExpand = cavityHeight;
    numExpand = 0;
    for ( ; slavePtr != NULL; slavePtr = slavePtr->nextPtr) {
	childHeight = Tk_ReqHeight(slavePtr->tkwin) + slavePtr->doubleBw
		+ slavePtr->padY + slavePtr->iPadY;
	if ((slavePtr->side == LEFT) || (slavePtr->side == RIGHT)) {
	    curExpand = (cavityHeight - childHeight)/numExpand;
	    if (curExpand < minExpand) {
		minExpand = curExpand;
	    }
	} else {
	    cavityHeight -= childHeight;
	    if (slavePtr->flags & EXPAND) {
		numExpand++;
	    }
	}
    }
    curExpand = cavityHeight/numExpand;
    if (curExpand < minExpand) {
	minExpand = curExpand;
    }
    return (minExpand < 0) ? 0 : minExpand;
}

/*
 *--------------------------------------------------------------
 *
 * GetPacker --
 *
 *	This internal procedure is used to locate a Packer
 *	structure for a given window, creating one if one
 *	doesn't exist already.
 *
 * Results:
 *	The return value is a pointer to the Packer structure
 *	corresponding to tkwin.
 *
 * Side effects:
 *	A new packer structure may be created.  If so, then
 *	a callback is set up to clean things up when the
 *	window is deleted.
 *
 *--------------------------------------------------------------
 */

static Packer *
GetPacker(tkwin)
    Tk_Window tkwin;		/* Token for window for which
				 * packer structure is desired. */
{
    register Packer *packPtr;
    Tcl_HashEntry *hPtr;
    int new;

    if (!initialized) {
	initialized = 1;
	Tcl_InitHashTable(&packerHashTable, TCL_ONE_WORD_KEYS);
    }

    /*
     * See if there's already packer for this window.  If not,
     * then create a new one.
     */

    hPtr = Tcl_CreateHashEntry(&packerHashTable, (char *) tkwin, &new);
    if (!new) {
	return (Packer *) Tcl_GetHashValue(hPtr);
    }
    packPtr = (Packer *) ckalloc(sizeof(Packer));
    packPtr->tkwin = tkwin;
    packPtr->masterPtr = NULL;
    packPtr->nextPtr = NULL;
    packPtr->slavePtr = NULL;
    packPtr->side = TOP;
    packPtr->anchor = TK_ANCHOR_CENTER;
    packPtr->padX = packPtr->padY = 0;
    packPtr->iPadX = packPtr->iPadY = 0;
    packPtr->doubleBw = 2*Tk_Changes(tkwin)->border_width;
    packPtr->abortPtr = NULL;
    packPtr->flags = 0;
    Tcl_SetHashValue(hPtr, packPtr);
    Tk_CreateEventHandler(tkwin, StructureNotifyMask,
	    PackStructureProc, (ClientData) packPtr);
    return packPtr;
}

/*
 *--------------------------------------------------------------
 *
 * PackAfter --
 *
 *	This procedure does most of the real work of adding
 *	one or more windows into the packing order for its parent.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *	The geometry of the specified windows may change, both now and
 *	again in the future.
 *
 *--------------------------------------------------------------
 */

static int
PackAfter(interp, prevPtr, masterPtr, argc, argv)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Packer *prevPtr;		/* Pack windows in argv just after this
				 * window;  NULL means pack as first
				 * child of masterPtr. */
    Packer *masterPtr;		/* Master in which to pack windows. */
    int argc;			/* Number of elements in argv. */
    char **argv;		/* Array of lists, each containing 2
				 * elements:  window name and side
				 * against which to pack. */
{
    register Packer *packPtr;
    Tk_Window tkwin, ancestor, parent;
    size_t length;
    char **options;
    int index, tmp, optionCount, c;

    /*
     * Iterate over all of the window specifiers, each consisting of
     * two arguments.  The first argument contains the window name and
     * the additional arguments contain options such as "top" or
     * "padx 20".
     */

    for ( ; argc > 0; argc -= 2, argv += 2, prevPtr = packPtr) {
	if (argc < 2) {
	    Tcl_AppendResult(interp, "wrong # args: window \"",
		    argv[0], "\" should be followed by options",
		    (char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Find the packer for the window to be packed, and make sure
	 * that the window in which it will be packed is either its
	 * or a descendant of its parent.
	 */

	tkwin = Tk_NameToWindow(interp, argv[0], masterPtr->tkwin);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}

	parent = Tk_Parent(tkwin);
	for (ancestor = masterPtr->tkwin; ; ancestor = Tk_Parent(ancestor)) {
	    if (ancestor == parent) {
		break;
	    }
	    if (((Tk_FakeWin *) (ancestor))->flags & TK_TOP_LEVEL) {
		badWindow:
		Tcl_AppendResult(interp, "can't pack ", argv[0],
			" inside ", Tk_PathName(masterPtr->tkwin),
			(char *) NULL);
		return TCL_ERROR;
	    }
	}
	if (((Tk_FakeWin *) (tkwin))->flags & TK_TOP_LEVEL) {
	    goto badWindow;
	}
	if (tkwin == masterPtr->tkwin) {
	    goto badWindow;
	}
	packPtr = GetPacker(tkwin);

	/*
	 * Process options for this window.
	 */

	if (Tcl_SplitList(interp, argv[1], &optionCount, &options) != TCL_OK) {
	    return TCL_ERROR;
	}
	packPtr->side = TOP;
	packPtr->anchor = TK_ANCHOR_CENTER;
	packPtr->padX = packPtr->padY = 0;
	packPtr->iPadX = packPtr->iPadY = 0;
	packPtr->flags &= ~(FILLX|FILLY|EXPAND);
	packPtr->flags |= OLD_STYLE;
	for (index = 0 ; index < optionCount; index++) {
	    char *curOpt = options[index];

	    c = curOpt[0];
	    length = strlen(curOpt);

	    if ((c == 't')
		    && (strncmp(curOpt, "top", length)) == 0) {
		packPtr->side = TOP;
	    } else if ((c == 'b')
		    && (strncmp(curOpt, "bottom", length)) == 0) {
		packPtr->side = BOTTOM;
	    } else if ((c == 'l')
		    && (strncmp(curOpt, "left", length)) == 0) {
		packPtr->side = LEFT;
	    } else if ((c == 'r')
		    && (strncmp(curOpt, "right", length)) == 0) {
		packPtr->side = RIGHT;
	    } else if ((c == 'e')
		    && (strncmp(curOpt, "expand", length)) == 0) {
		packPtr->flags |= EXPAND;
	    } else if ((c == 'f')
		    && (strcmp(curOpt, "fill")) == 0) {
		packPtr->flags |= FILLX|FILLY;
	    } else if ((length == 5) && (strcmp(curOpt, "fillx")) == 0) {
		packPtr->flags |= FILLX;
	    } else if ((length == 5) && (strcmp(curOpt, "filly")) == 0) {
		packPtr->flags |= FILLY;
	    } else if ((c == 'p') && (strcmp(curOpt, "padx")) == 0) {
		if (optionCount < (index+2)) {
		    missingPad:
		    Tcl_AppendResult(interp, "wrong # args: \"", curOpt,
			    "\" option must be followed by screen distance",
			    (char *) NULL);
		    goto error;
		}
		if ((Tk_GetPixels(interp, tkwin, options[index+1], &tmp)
			!= TCL_OK) || (tmp < 0)) {
		    badPad:
		    Tcl_AppendResult(interp, "bad pad value \"",
			    options[index+1],
			    "\": must be positive screen distance",
			    (char *) NULL);
		    goto error;
		}
		packPtr->padX = tmp;
		packPtr->iPadX = 0;
		index++;
	    } else if ((c == 'p') && (strcmp(curOpt, "pady")) == 0) {
		if (optionCount < (index+2)) {
		    goto missingPad;
		}
		if ((Tk_GetPixels(interp, tkwin, options[index+1], &tmp)
			!= TCL_OK) || (tmp < 0)) {
		    goto badPad;
		}
		packPtr->padY = tmp;
		packPtr->iPadY = 0;
		index++;
	    } else if ((c == 'f') && (length > 1)
		    && (strncmp(curOpt, "frame", length) == 0)) {
		if (optionCount < (index+2)) {
		    Tcl_AppendResult(interp, "wrong # args: \"frame\" ",
			    "option must be followed by anchor point",
			    (char *) NULL);
		    goto error;
		}
		if (Tk_GetAnchor(interp, options[index+1],
			&packPtr->anchor) != TCL_OK) {
		    goto error;
		}
		index++;
	    } else {
		Tcl_AppendResult(interp, "bad option \"", curOpt,
			"\": should be top, bottom, left, right, ",
			"expand, fill, fillx, filly, padx, pady, or frame",
			(char *) NULL);
		goto error;
	    }
	}

	if (packPtr != prevPtr) {

	    /*
	     * Unpack this window if it's currently packed.
	     */

	    if (packPtr->masterPtr != NULL) {
		if ((packPtr->masterPtr != masterPtr) &&
			(packPtr->masterPtr->tkwin
			!= Tk_Parent(packPtr->tkwin))) {
		    Tk_UnmaintainGeometry(packPtr->tkwin,
			    packPtr->masterPtr->tkwin);
		}
		Unlink(packPtr);
	    }
	
	    /*
	     * Add the window in the correct place in its parent's
	     * packing order, then make sure that the window is
	     * managed by us.
	     */

	    packPtr->masterPtr = masterPtr;
	    if (prevPtr == NULL) {
		packPtr->nextPtr = masterPtr->slavePtr;
		masterPtr->slavePtr = packPtr;
	    } else {
		packPtr->nextPtr = prevPtr->nextPtr;
		prevPtr->nextPtr = packPtr;
	    }
	    Tk_ManageGeometry(tkwin, &packerType, (ClientData) packPtr);
	}
	ckfree((char *) options);
    }

    /*
     * Arrange for the parent to be re-packed at the first
     * idle moment.
     */

    if (masterPtr->abortPtr != NULL) {
	*masterPtr->abortPtr = 1;
    }
    if (!(masterPtr->flags & REQUESTED_REPACK)) {
	masterPtr->flags |= REQUESTED_REPACK;
	Tcl_DoWhenIdle(ArrangePacking, (ClientData) masterPtr);
    }
    return TCL_OK;

    error:
    ckfree((char *) options);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Unlink --
 *
 *	Remove a packer from its parent's list of slaves.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The parent will be scheduled for repacking.
 *
 *----------------------------------------------------------------------
 */

static void
Unlink(packPtr)
    register Packer *packPtr;		/* Window to unlink. */
{
    register Packer *masterPtr, *packPtr2;

    masterPtr = packPtr->masterPtr;
    if (masterPtr == NULL) {
	return;
    }
    if (masterPtr->slavePtr == packPtr) {
	masterPtr->slavePtr = packPtr->nextPtr;
    } else {
	for (packPtr2 = masterPtr->slavePtr; ; packPtr2 = packPtr2->nextPtr) {
	    if (packPtr2 == NULL) {
		panic("Unlink couldn't find previous window");
	    }
	    if (packPtr2->nextPtr == packPtr) {
		packPtr2->nextPtr = packPtr->nextPtr;
		break;
	    }
	}
    }
    if (!(masterPtr->flags & REQUESTED_REPACK)) {
	masterPtr->flags |= REQUESTED_REPACK;
	Tcl_DoWhenIdle(ArrangePacking, (ClientData) masterPtr);
    }
    if (masterPtr->abortPtr != NULL) {
	*masterPtr->abortPtr = 1;
    }

    packPtr->masterPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * DestroyPacker --
 *
 *	This procedure is invoked by Tcl_EventuallyFree or Tcl_Release
 *	to clean up the internal structure of a packer at a safe time
 *	(when no-one is using it anymore).
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Everything associated with the packer is freed up.
 *
 *----------------------------------------------------------------------
 */

static void
DestroyPacker(memPtr)
    char *memPtr;		/* Info about packed window that
				 * is now dead. */
{
    register Packer *packPtr = (Packer *) memPtr;
    ckfree((char *) packPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * PackStructureProc --
 *
 *	This procedure is invoked by the Tk event dispatcher in response
 *	to StructureNotify events.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If a window was just deleted, clean up all its packer-related
 *	information.  If it was just resized, repack its slaves, if
 *	any.
 *
 *----------------------------------------------------------------------
 */

static void
PackStructureProc(clientData, eventPtr)
    ClientData clientData;		/* Our information about window
					 * referred to by eventPtr. */
    XEvent *eventPtr;			/* Describes what just happened. */
{
    register Packer *packPtr = (Packer *) clientData;
    if (eventPtr->type == ConfigureNotify) {
	if ((packPtr->slavePtr != NULL)
		&& !(packPtr->flags & REQUESTED_REPACK)) {
	    packPtr->flags |= REQUESTED_REPACK;
	    Tcl_DoWhenIdle(ArrangePacking, (ClientData) packPtr);
	}
	if (packPtr->doubleBw != 2*Tk_Changes(packPtr->tkwin)->border_width) {
	    if ((packPtr->masterPtr != NULL)
		    && !(packPtr->masterPtr->flags & REQUESTED_REPACK)) {
		packPtr->doubleBw = 2*Tk_Changes(packPtr->tkwin)->border_width;
		packPtr->masterPtr->flags |= REQUESTED_REPACK;
		Tcl_DoWhenIdle(ArrangePacking, (ClientData) packPtr->masterPtr);
	    }
	}
    } else if (eventPtr->type == DestroyNotify) {
	register Packer *slavePtr, *nextPtr;

	if (packPtr->masterPtr != NULL) {
	    Unlink(packPtr);
	}
	for (slavePtr = packPtr->slavePtr; slavePtr != NULL;
		slavePtr = nextPtr) {
	    Tk_ManageGeometry(slavePtr->tkwin, (Tk_GeomMgr *) NULL,
		    (ClientData) NULL);
	    Tk_UnmapWindow(slavePtr->tkwin);
	    slavePtr->masterPtr = NULL;
	    nextPtr = slavePtr->nextPtr;
	    slavePtr->nextPtr = NULL;
	}
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&packerHashTable,
		(char *) packPtr->tkwin));
	if (packPtr->flags & REQUESTED_REPACK) {
	    Tcl_CancelIdleCall(ArrangePacking, (ClientData) packPtr);
	}
	packPtr->tkwin = NULL;
	Tcl_EventuallyFree((ClientData) packPtr, DestroyPacker);
    } else if (eventPtr->type == MapNotify) {
	/*
	 * When a master gets mapped, must redo the geometry computation
	 * so that all of its slaves get remapped.
	 */

	if ((packPtr->slavePtr != NULL)
		&& !(packPtr->flags & REQUESTED_REPACK)) {
	    packPtr->flags |= REQUESTED_REPACK;
	    Tcl_DoWhenIdle(ArrangePacking, (ClientData) packPtr);
	}
    } else if (eventPtr->type == UnmapNotify) {
	Packer *packPtr2;

	/*
	 * Unmap all of the slaves when the master gets unmapped,
	 * so that they don't bother to keep redisplaying
	 * themselves.
	 */

	for (packPtr2 = packPtr->slavePtr; packPtr2 != NULL;
		packPtr2 = packPtr2->nextPtr) {
	    Tk_UnmapWindow(packPtr2->tkwin);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureSlaves --
 *
 *	This implements the guts of the "pack configure" command.  Given
 *	a list of slaves and configuration options, it arranges for the
 *	packer to manage the slaves and sets the specified options.
 *
 * Results:
 *	TCL_OK is returned if all went well.  Otherwise, TCL_ERROR is
 *	returned and interp->result is set to contain an error message.
 *
 * Side effects:
 *	Slave windows get taken over by the packer.
 *
 *----------------------------------------------------------------------
 */

static int
ConfigureSlaves(interp, tkwin, argc, argv)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Tk_Window tkwin;		/* Any window in application containing
				 * slaves.  Used to look up slave names. */
    int argc;			/* Number of elements in argv. */
    char *argv[];		/* Argument strings:  contains one or more
				 * window names followed by any number
				 * of "option value" pairs.  Caller must
				 * make sure that there is at least one
				 * window name. */
{
    Packer *masterPtr, *slavePtr, *prevPtr, *otherPtr;
    Tk_Window other, slave, parent, ancestor;
    int i, j, numWindows, c, tmp, positionGiven;
    size_t length;

    /*
     * Find out how many windows are specified.
     */

    for (numWindows = 0; numWindows < argc; numWindows++) {
	if (argv[numWindows][0] != '.') {
	    break;
	}
    }

    /*
     * Iterate over all of the slave windows, parsing the configuration
     * options for each slave.  It's a bit wasteful to re-parse the
     * options for each slave, but things get too messy if we try to
     * parse the arguments just once at the beginning.  For example,
     * if a slave already is packed we want to just change a few
     * existing values without resetting everything.  If there are
     * multiple windows, the -after, -before, and -in options only
     * get processed for the first window.
     */

    masterPtr = NULL;
    prevPtr = NULL;
    positionGiven = 0;
    for (j = 0; j < numWindows; j++) {
	slave = Tk_NameToWindow(interp, argv[j], tkwin);
	if (slave == NULL) {
	    return TCL_ERROR;
	}
	if (Tk_IsTopLevel(slave)) {
	    Tcl_AppendResult(interp, "can't pack \"", argv[j],
		    "\": it's a top-level window", (char *) NULL);
	    return TCL_ERROR;
	}
	slavePtr = GetPacker(slave);
	slavePtr->flags &= ~OLD_STYLE;

	/*
	 * If the slave isn't currently packed, reset all of its
	 * configuration information to default values (there could
	 * be old values left from a previous packing).
	 */

	if (slavePtr->masterPtr == NULL) {
	    slavePtr->side = TOP;
	    slavePtr->anchor = TK_ANCHOR_CENTER;
	    slavePtr->padX = slavePtr->padY = 0;
	    slavePtr->iPadX = slavePtr->iPadY = 0;
	    slavePtr->flags &= ~(FILLX|FILLY|EXPAND);
	}

	for (i = numWindows; i < argc; i+=2) {
	    if ((i+2) > argc) {
		Tcl_AppendResult(interp, "extra option \"", argv[i],
			"\" (option with no value?)", (char *) NULL);
		return TCL_ERROR;
	    }
	    length = strlen(argv[i]);
	    if (length < 2) {
		goto badOption;
	    }
	    c = argv[i][1];
	    if ((c == 'a') && (strncmp(argv[i], "-after", length) == 0)
		    && (length >= 2)) {
		if (j == 0) {
		    other = Tk_NameToWindow(interp, argv[i+1], tkwin);
		    if (other == NULL) {
			return TCL_ERROR;
		    }
		    prevPtr = GetPacker(other);
		    if (prevPtr->masterPtr == NULL) {
			notPacked:
			Tcl_AppendResult(interp, "window \"", argv[i+1],
				"\" isn't packed", (char *) NULL);
			return TCL_ERROR;
		    }
		    masterPtr = prevPtr->masterPtr;
		    positionGiven = 1;
		}
	    } else if ((c == 'a') && (strncmp(argv[i], "-anchor", length) == 0)
		    && (length >= 2)) {
		if (Tk_GetAnchor(interp, argv[i+1], &slavePtr->anchor)
			!= TCL_OK) {
		    return TCL_ERROR;
		}
	    } else if ((c == 'b')
		    && (strncmp(argv[i], "-before", length) == 0)) {
		if (j == 0) {
		    other = Tk_NameToWindow(interp, argv[i+1], tkwin);
		    if (other == NULL) {
			return TCL_ERROR;
		    }
		    otherPtr = GetPacker(other);
		    if (otherPtr->masterPtr == NULL) {
			goto notPacked;
		    }
		    masterPtr = otherPtr->masterPtr;
		    prevPtr = masterPtr->slavePtr;
		    if (prevPtr == otherPtr) {
			prevPtr = NULL;
		    } else {
			while (prevPtr->nextPtr != otherPtr) {
			    prevPtr = prevPtr->nextPtr;
			}
		    }
		    positionGiven = 1;
		}
	    } else if ((c == 'e')
		    && (strncmp(argv[i], "-expand", length) == 0)) {
		if (Tcl_GetBoolean(interp, argv[i+1], &tmp) != TCL_OK) {
		    return TCL_ERROR;
		}
		slavePtr->flags &= ~EXPAND;
		if (tmp) {
		    slavePtr->flags |= EXPAND;
		}
	    } else if ((c == 'f') && (strncmp(argv[i], "-fill", length) == 0)) {
		if (strcmp(argv[i+1], "none") == 0) {
		    slavePtr->flags &= ~(FILLX|FILLY);
		} else if (strcmp(argv[i+1], "x") == 0) {
		    slavePtr->flags = (slavePtr->flags & ~FILLY) | FILLX;
		} else if (strcmp(argv[i+1], "y") == 0) {
		    slavePtr->flags = (slavePtr->flags & ~FILLX) | FILLY;
		} else if (strcmp(argv[i+1], "both") == 0) {
		    slavePtr->flags |= FILLX|FILLY;
		} else {
		    Tcl_AppendResult(interp, "bad fill style \"", argv[i+1],
			    "\": must be none, x, y, or both", (char *) NULL);
		    return TCL_ERROR;
		}
	    } else if ((c == 'i') && (strcmp(argv[i], "-in") == 0)) {
		if (j == 0) {
		    other = Tk_NameToWindow(interp, argv[i+1], tkwin);
		    if (other == NULL) {
			return TCL_ERROR;
		    }
		    masterPtr = GetPacker(other);
		    prevPtr = masterPtr->slavePtr;
		    if (prevPtr != NULL) {
			while (prevPtr->nextPtr != NULL) {
			    prevPtr = prevPtr->nextPtr;
			}
		    }
		    positionGiven = 1;
		}
	    } else if ((c == 'i') && (strcmp(argv[i], "-ipadx") == 0)) {
		if ((Tk_GetPixels(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp < 0)) {
		    badPad:
		    Tcl_ResetResult(interp);
		    Tcl_AppendResult(interp, "bad pad value \"", argv[i+1],
			    "\": must be positive screen distance",
			    (char *) NULL);
		    return TCL_ERROR;
		}
		slavePtr->iPadX = tmp*2;
	    } else if ((c == 'i') && (strcmp(argv[i], "-ipady") == 0)) {
		if ((Tk_GetPixels(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp< 0)) {
		    goto badPad;
		}
		slavePtr->iPadY = tmp*2;
	    } else if ((c == 'p') && (strcmp(argv[i], "-padx") == 0)) {
		if ((Tk_GetPixels(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp< 0)) {
		    goto badPad;
		}
		slavePtr->padX = tmp*2;
	    } else if ((c == 'p') && (strcmp(argv[i], "-pady") == 0)) {
		if ((Tk_GetPixels(interp, slave, argv[i+1], &tmp) != TCL_OK)
			|| (tmp< 0)) {
		    goto badPad;
		}
		slavePtr->padY = tmp*2;
	    } else if ((c == 's') && (strncmp(argv[i], "-side", length) == 0)) {
		c = argv[i+1][0];
		if ((c == 't') && (strcmp(argv[i+1], "top") == 0)) {
		    slavePtr->side = TOP;
		} else if ((c == 'b') && (strcmp(argv[i+1], "bottom") == 0)) {
		    slavePtr->side = BOTTOM;
		} else if ((c == 'l') && (strcmp(argv[i+1], "left") == 0)) {
		    slavePtr->side = LEFT;
		} else if ((c == 'r') && (strcmp(argv[i+1], "right") == 0)) {
		    slavePtr->side = RIGHT;
		} else {
		    Tcl_AppendResult(interp, "bad side \"", argv[i+1],
			    "\": must be top, bottom, left, or right",
			    (char *) NULL);
		    return TCL_ERROR;
		}
	    } else {
		badOption:
		Tcl_AppendResult(interp, "unknown or ambiguous option \"",
			argv[i], "\": must be -after, -anchor, -before, ",
			"-expand, -fill, -in, -ipadx, -ipady, -padx, ",
			"-pady, or -side", (char *) NULL);
		return TCL_ERROR;
	    }
	}

	/*
	 * If no position in a packing list was specified and the slave
	 * is already packed, then leave it in its current location in
	 * its current packing list.
	 */

	if (!positionGiven && (slavePtr->masterPtr != NULL)) {
	    masterPtr = slavePtr->masterPtr;
	    goto scheduleLayout;
	}

	/*
	 * If the slave is going to be put back after itself then
	 * skip the whole operation, since it won't work anyway.
	 */

	if (prevPtr == slavePtr) {
	    masterPtr = slavePtr->masterPtr;
	    goto scheduleLayout;
	}
    
	/*
	 * If none of the "-in", "-before", or "-after" options has
	 * been specified, arrange for the slave to go at the end of
	 * the order for its parent.
	 */
    
	if (!positionGiven) {
	    masterPtr = GetPacker(Tk_Parent(slave));
	    prevPtr = masterPtr->slavePtr;
	    if (prevPtr != NULL) {
		while (prevPtr->nextPtr != NULL) {
		    prevPtr = prevPtr->nextPtr;
		}
	    }
	}

	/*
	 * Make sure that the slave's parent is either the master or
	 * an ancestor of the master, and that the master and slave
	 * aren't the same.
	 */
    
	parent = Tk_Parent(slave);
	for (ancestor = masterPtr->tkwin; ; ancestor = Tk_Parent(ancestor)) {
	    if (ancestor == parent) {
		break;
	    }
	    if (Tk_IsTopLevel(ancestor)) {
		Tcl_AppendResult(interp, "can't pack ", argv[j],
			" inside ", Tk_PathName(masterPtr->tkwin),
			(char *) NULL);
		return TCL_ERROR;
	    }
	}
	if (slave == masterPtr->tkwin) {
	    Tcl_AppendResult(interp, "can't pack ", argv[j],
		    " inside itself", (char *) NULL);
	    return TCL_ERROR;
	}

	/*
	 * Unpack the slave if it's currently packed, then position it
	 * after prevPtr.
	 */

	if (slavePtr->masterPtr != NULL) {
	    if ((slavePtr->masterPtr != masterPtr) &&
		    (slavePtr->masterPtr->tkwin
		    != Tk_Parent(slavePtr->tkwin))) {
		Tk_UnmaintainGeometry(slavePtr->tkwin,
			slavePtr->masterPtr->tkwin);
	    }
	    Unlink(slavePtr);
	}
	slavePtr->masterPtr = masterPtr;
	if (prevPtr == NULL) {
	    slavePtr->nextPtr = masterPtr->slavePtr;
	    masterPtr->slavePtr = slavePtr;
	} else {
	    slavePtr->nextPtr = prevPtr->nextPtr;
	    prevPtr->nextPtr = slavePtr;
	}
	Tk_ManageGeometry(slave, &packerType, (ClientData) slavePtr);
	prevPtr = slavePtr;

	/*
	 * Arrange for the parent to be re-packed at the first
	 * idle moment.
	 */

	scheduleLayout:
	if (masterPtr->abortPtr != NULL) {
	    *masterPtr->abortPtr = 1;
	}
	if (!(masterPtr->flags & REQUESTED_REPACK)) {
	    masterPtr->flags |= REQUESTED_REPACK;
	    Tcl_DoWhenIdle(ArrangePacking, (ClientData) masterPtr);
	}
    }
    return TCL_OK;
}

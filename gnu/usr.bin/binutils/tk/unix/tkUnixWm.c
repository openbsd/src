/* 
 * tkUnixWm.c --
 *
 *	This module takes care of the interactions between a Tk-based
 *	application and the window manager.  Among other things, it
 *	implements the "wm" command and passes geometry information
 *	to the window manager.
 *
 * Copyright (c) 1991-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkUnixWm.c 1.124 96/03/29 14:05:44
 */

#include "tkPort.h"
#include "tkInt.h"
#include <errno.h>

/*
 * A data structure of the following type holds information for
 * each window manager protocol (such as WM_DELETE_WINDOW) for
 * which a handler (i.e. a Tcl command) has been defined for a
 * particular top-level window.
 */

typedef struct ProtocolHandler {
    Atom protocol;		/* Identifies the protocol. */
    struct ProtocolHandler *nextPtr;
				/* Next in list of protocol handlers for
				 * the same top-level window, or NULL for
				 * end of list. */
    Tcl_Interp *interp;		/* Interpreter in which to invoke command. */
    char command[4];		/* Tcl command to invoke when a client
				 * message for this protocol arrives. 
				 * The actual size of the structure varies
				 * to accommodate the needs of the actual
				 * command. THIS MUST BE THE LAST FIELD OF
				 * THE STRUCTURE. */
} ProtocolHandler;

#define HANDLER_SIZE(cmdLength) \
    ((unsigned) (sizeof(ProtocolHandler) - 3 + cmdLength))

/*
 * A data structure of the following type holds window-manager-related
 * information for each top-level window in an application.
 */

typedef struct TkWmInfo {
    TkWindow *winPtr;		/* Pointer to main Tk information for
				 * this window. */
    Window reparent;		/* If the window has been reparented, this
				 * gives the ID of the ancestor of the window
				 * that is a child of the root window (may
				 * not be window's immediate parent).  If
				 * the window isn't reparented, this has the
				 * value None. */
    Tk_Uid titleUid;		/* Title to display in window caption.  If
				 * NULL, use name of widget. */
    Tk_Uid iconName;		/* Name to display in icon. */
    Window master;		/* Master window for TRANSIENT_FOR property,
				 * or None. */
    XWMHints hints;		/* Various pieces of information for
				 * window manager. */
    Tk_Uid leaderName;		/* Path name of leader of window group
				 * (corresponds to hints.window_group).
				 * Note:  this field doesn't get updated
				 * if leader is destroyed. */
    Tk_Uid masterWindowName;	/* Path name of window specified as master
				 * in "wm transient" command, or NULL.
				 * Note:  this field doesn't get updated if
				 * masterWindowName is destroyed. */
    Tk_Window icon;		/* Window to use as icon for this window,
				 * or NULL. */
    Tk_Window iconFor;		/* Window for which this window is icon, or
				 * NULL if this isn't an icon for anyone. */
    int withdrawn;		/* Non-zero means window has been withdrawn. */

    /*
     * Information used to construct an XSizeHints structure for
     * the window manager:
     */

    int sizeHintsFlags;		/* Flags word for XSizeHints structure.
				 * If the PBaseSize flag is set then the
				 * window is gridded;  otherwise it isn't
				 * gridded. */
    int minWidth, minHeight;	/* Minimum dimensions of window, in
				 * grid units, not pixels. */
    int maxWidth, maxHeight;	/* Maximum dimensions of window, in
				 * grid units, not pixels. */
    Tk_Window gridWin;		/* Identifies the window that controls
				 * gridding for this top-level, or NULL if
				 * the top-level isn't currently gridded. */
    int widthInc, heightInc;	/* Increments for size changes (# pixels
				 * per step). */
    struct {
	int x;	/* numerator */
	int y;  /* denominator */
    } minAspect, maxAspect;	/* Min/max aspect ratios for window. */
    int reqGridWidth, reqGridHeight;
				/* The dimensions of the window (in
				 * grid units) requested through
				 * the geometry manager. */
    int gravity;		/* Desired window gravity. */

    /*
     * Information used to manage the size and location of a window.
     */

    int width, height;		/* Desired dimensions of window, specified
				 * in grid units.  These values are
				 * set by the "wm geometry" command and by
				 * ConfigureNotify events (for when wm
				 * resizes window).  -1 means user hasn't
				 * requested dimensions. */
    int x, y;			/* Desired X and Y coordinates for window.
				 * These values are set by "wm geometry",
				 * plus by ConfigureNotify events (when wm
				 * moves window).  These numbers are
				 * different than the numbers stored in
				 * winPtr->changes because (a) they could be
				 * measured from the right or bottom edge
				 * of the screen (see WM_NEGATIVE_X and
				 * WM_NEGATIVE_Y flags) and (b) if the window
				 * has been reparented then they refer to the
				 * parent rather than the window itself. */
    int parentWidth, parentHeight;
				/* Width and height of reparent, in pixels
				 * *including border*.  If window hasn't been
				 * reparented then these will be the outer
				 * dimensions of the window, including
				 * border. */
    int xInParent, yInParent;	/* Offset of window within reparent,  measured
				 * from upper-left outer corner of parent's
				 * border to upper-left outer corner of child's
				 * border.  If not reparented then these are
				 * zero. */
    int configWidth, configHeight;
				/* Dimensions passed to last request that we
				 * issued to change geometry of window.  Used
				 * to eliminate redundant resize operations. */

    /*
     * Information about the virtual root window for this top-level,
     * if there is one.
     */

    Window vRoot;		/* Virtual root window for this top-level,
				 * or None if there is no virtual root
				 * window (i.e. just use the screen's root). */
    int vRootX, vRootY;		/* Position of the virtual root inside the
				 * root window.  If the WM_VROOT_OFFSET_STALE
				 * flag is set then this information may be
				 * incorrect and needs to be refreshed from
				 * the X server.  If vRoot is None then these
				 * values are both 0. */
    int vRootWidth, vRootHeight;/* Dimensions of the virtual root window.
				 * If vRoot is None, gives the dimensions
				 * of the containing screen.  This information
				 * is never stale, even though vRootX and
				 * vRootY can be. */

    /*
     * Miscellaneous information.
     */

    ProtocolHandler *protPtr;	/* First in list of protocol handlers for
				 * this window (NULL means none). */
    int cmdArgc;		/* Number of elements in cmdArgv below. */
    char **cmdArgv;		/* Array of strings to store in the
				 * WM_COMMAND property.  NULL means nothing
				 * available. */
    char *clientMachine;	/* String to store in WM_CLIENT_MACHINE
				 * property, or NULL. */
    int flags;			/* Miscellaneous flags, defined below. */
    struct TkWmInfo *nextPtr;	/* Next in list of all top-level windows. */
} WmInfo;

/*
 * Flag values for WmInfo structures:
 *
 * WM_NEVER_MAPPED -		non-zero means window has never been
 *				mapped;  need to update all info when
 *				window is first mapped.
 * WM_UPDATE_PENDING -		non-zero means a call to UpdateGeometryInfo
 *				has already been scheduled for this
 *				window;  no need to schedule another one.
 * WM_NEGATIVE_X -		non-zero means x-coordinate is measured in
 *				pixels from right edge of screen, rather
 *				than from left edge.
 * WM_NEGATIVE_Y -		non-zero means y-coordinate is measured in
 *				pixels up from bottom of screen, rather than
 *				down from top.
 * WM_UPDATE_SIZE_HINTS -	non-zero means that new size hints need to be
 *				propagated to window manager.
 * WM_SYNC_PENDING -		set to non-zero while waiting for the window
 *				manager to respond to some state change.
 * WM_VROOT_OFFSET_STALE -	non-zero means that (x,y) offset information
 *				about the virtual root window is stale and
 *				needs to be fetched fresh from the X server.
 * WM_ABOUT_TO_MAP -		non-zero means that the window is about to
 *				be mapped by TkWmMapWindow.  This is used
 *				by UpdateGeometryInfo to modify its behavior.
 * WM_MOVE_PENDING -		non-zero means the application has requested
 *				a new position for the window, but it hasn't
 *				been reflected through the window manager
 *				yet.
 * WM_COLORMAPS_EXPLICIT -	non-zero means the colormap windows were
 *				set explicitly via "wm colormapwindows".
 * WM_ADDED_TOPLEVEL_COLORMAP - non-zero means that when "wm colormapwindows"
 *				was called the top-level itself wasn't
 *				specified, so we added it implicitly at
 *				the end of the list.
 * WM_WIDTH_NOT_RESIZABLE -	non-zero means that we're not supposed to
 *				allow the user to change the width of the
 *				window (controlled by "wm resizable"
 *				command).
 * WM_HEIGHT_NOT_RESIZABLE -	non-zero means that we're not supposed to
 *				allow the user to change the height of the
 *				window (controlled by "wm resizable"
 *				command).
 */

#define WM_NEVER_MAPPED			1
#define WM_UPDATE_PENDING		2
#define WM_NEGATIVE_X			4
#define WM_NEGATIVE_Y			8
#define WM_UPDATE_SIZE_HINTS		0x10
#define WM_SYNC_PENDING			0x20
#define WM_VROOT_OFFSET_STALE		0x40
#define WM_ABOUT_TO_MAP			0x100
#define WM_MOVE_PENDING			0x200
#define WM_COLORMAPS_EXPLICIT		0x400
#define WM_ADDED_TOPLEVEL_COLORMAP	0x800
#define WM_WIDTH_NOT_RESIZABLE		0x1000
#define WM_HEIGHT_NOT_RESIZABLE		0x2000

/*
 * This module keeps a list of all top-level windows, primarily to
 * simplify the job of Tk_CoordsToWindow.
 */

static WmInfo *firstWmPtr = NULL;	/* Points to first top-level window. */


/*
 * The variable below is used to enable or disable tracing in this
 * module.  If tracing is enabled, then information is printed on
 * standard output about interesting interactions with the window
 * manager.
 */

static int wmTracing = 0;

/*
 * The following structure is the official type record for geometry
 * management of top-level windows.
 */

static void		TopLevelReqProc _ANSI_ARGS_((ClientData dummy,
			    Tk_Window tkwin));

static Tk_GeomMgr wmMgrType = {
    "wm",				/* name */
    TopLevelReqProc,			/* requestProc */
    (Tk_GeomLostSlaveProc *) NULL,	/* lostSlaveProc */
};

/*
 * Structures of the following type are used for communication between
 * WaitForEvent, WaitRestrictProc, and WaitTimeoutProc.
 */

typedef struct WaitRestrictInfo {
    Display *display;		/* Window belongs to this display. */
    Window window;		/* We're waiting for events on this window. */
    int type;			/* We only care about this type of event. */
    XEvent *eventPtr;		/* Where to store the event when it's found. */
    int foundEvent;		/* Non-zero means that an event of the
				 * desired type has been found. */
    int timeout;		/* Non-zero means that too much time elapsed
				 * while waiting, and we should just give
				 * up. */
} WaitRestrictInfo;

/*
 * Forward declarations for procedures defined in this file:
 */

static int		ComputeReparentGeometry _ANSI_ARGS_((TkWindow *winPtr));
static void		ConfigureEvent _ANSI_ARGS_((TkWindow *winPtr,
			    XConfigureEvent *eventPtr));
static void		GetMaxSize _ANSI_ARGS_((WmInfo *wmPtr,
			    int *maxWidthPtr, int *maxHeightPtr));
static int		ParseGeometry _ANSI_ARGS_((Tcl_Interp *interp,
			    char *string, TkWindow *winPtr));
static void		ReparentEvent _ANSI_ARGS_((TkWindow *winPtr,
			    XReparentEvent *eventPtr));
static void		TopLevelEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		TopLevelReqProc _ANSI_ARGS_((ClientData dummy,
			    Tk_Window tkwin));
static void		UpdateGeometryInfo _ANSI_ARGS_((
			    ClientData clientData));
static void		UpdateHints _ANSI_ARGS_((TkWindow *winPtr));
static void		UpdateSizeHints _ANSI_ARGS_((TkWindow *winPtr));
static void		UpdateVRootGeometry _ANSI_ARGS_((WmInfo *wmPtr));
static void		UpdateWmProtocols _ANSI_ARGS_((WmInfo *wmPtr));
static void		WaitForConfigureNotify _ANSI_ARGS_((TkWindow *winPtr,
			    unsigned long serial));
static int		WaitForEvent _ANSI_ARGS_((Display *display,
			    Window window, int type, XEvent *eventPtr));
static void		WaitForMapNotify _ANSI_ARGS_((TkWindow *winPtr,
			    int mapped));
static Tk_RestrictAction
			WaitRestrictProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		WaitTimeoutProc _ANSI_ARGS_((ClientData clientData));

/*
 *--------------------------------------------------------------
 *
 * TkWmNewWindow --
 *
 *	This procedure is invoked whenever a new top-level
 *	window is created.  Its job is to initialize the WmInfo
 *	structure for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A WmInfo structure gets allocated and initialized.
 *
 *--------------------------------------------------------------
 */

void
TkWmNewWindow(winPtr)
    TkWindow *winPtr;		/* Newly-created top-level window. */
{
    register WmInfo *wmPtr;

    wmPtr = (WmInfo *) ckalloc(sizeof(WmInfo));
    wmPtr->winPtr = winPtr;
    wmPtr->reparent = None;
    wmPtr->titleUid = NULL;
    wmPtr->iconName = NULL;
    wmPtr->master = None;
    wmPtr->hints.flags = InputHint | StateHint;
    wmPtr->hints.input = True;
    wmPtr->hints.initial_state = NormalState;
    wmPtr->hints.icon_pixmap = None;
    wmPtr->hints.icon_window = None;
    wmPtr->hints.icon_x = wmPtr->hints.icon_y = 0;
    wmPtr->hints.icon_mask = None;
    wmPtr->hints.window_group = None;
    wmPtr->leaderName = NULL;
    wmPtr->masterWindowName = NULL;
    wmPtr->icon = NULL;
    wmPtr->iconFor = NULL;
    wmPtr->withdrawn = 0;
    wmPtr->sizeHintsFlags = 0;
    wmPtr->minWidth = wmPtr->minHeight = 1;

    /*
     * Default the maximum dimensions to the size of the display, minus
     * a guess about how space is needed for window manager decorations.
     */

    wmPtr->maxWidth = 0;
    wmPtr->maxHeight = 0;
    wmPtr->gridWin = NULL;
    wmPtr->widthInc = wmPtr->heightInc = 1;
    wmPtr->minAspect.x = wmPtr->minAspect.y = 1;
    wmPtr->maxAspect.x = wmPtr->maxAspect.y = 1;
    wmPtr->reqGridWidth = wmPtr->reqGridHeight = -1;
    wmPtr->gravity = NorthWestGravity;
    wmPtr->width = -1;
    wmPtr->height = -1;
    wmPtr->x = winPtr->changes.x;
    wmPtr->y = winPtr->changes.y;
    wmPtr->parentWidth = winPtr->changes.width
	    + 2*winPtr->changes.border_width;
    wmPtr->parentHeight = winPtr->changes.height
	    + 2*winPtr->changes.border_width;
    wmPtr->xInParent = wmPtr->yInParent = 0;
    wmPtr->configWidth = -1;
    wmPtr->configHeight = -1;
    wmPtr->vRoot = None;
    wmPtr->protPtr = NULL;
    wmPtr->cmdArgv = NULL;
    wmPtr->clientMachine = NULL;
    wmPtr->flags = WM_NEVER_MAPPED;
    wmPtr->nextPtr = firstWmPtr;
    firstWmPtr = wmPtr;
    winPtr->wmInfoPtr = wmPtr;

    UpdateVRootGeometry(wmPtr);

    /*
     * Tk must monitor structure events for top-level windows, in order
     * to detect size and position changes caused by window managers.
     */

    Tk_CreateEventHandler((Tk_Window) winPtr, StructureNotifyMask,
	    TopLevelEventProc, (ClientData) winPtr);

    /*
     * Arrange for geometry requests to be reflected from the window
     * to the window manager.
     */

    Tk_ManageGeometry((Tk_Window) winPtr, &wmMgrType, (ClientData) 0);
}

/*
 *--------------------------------------------------------------
 *
 * TkWmMapWindow --
 *
 *	This procedure is invoked to map a top-level window.  This
 *	module gets a chance to update all window-manager-related
 *	information in properties before the window manager sees
 *	the map event and checks the properties.  It also gets to
 *	decide whether or not to even map the window after all.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Properties of winPtr may get updated to provide up-to-date
 *	information to the window manager.  The window may also get
 *	mapped, but it may not be if this procedure decides that
 *	isn't appropriate (e.g. because the window is withdrawn).
 *
 *--------------------------------------------------------------
 */

void
TkWmMapWindow(winPtr)
    TkWindow *winPtr;		/* Top-level window that's about to
				 * be mapped. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    XTextProperty textProp;

    if (wmPtr->flags & WM_NEVER_MAPPED) {
	wmPtr->flags &= ~WM_NEVER_MAPPED;

	/*
	 * This is the first time this window has ever been mapped.
	 * Store all the window-manager-related information for the
	 * window.
	 */

	if (wmPtr->titleUid == NULL) {
	    wmPtr->titleUid = winPtr->nameUid;
	}
	if (XStringListToTextProperty(&wmPtr->titleUid, 1, &textProp)  != 0) {
	    XSetWMName(winPtr->display, winPtr->window, &textProp);
	    XFree((char *) textProp.value);
	}
    
	TkWmSetClass(winPtr);
    
	if (wmPtr->iconName != NULL) {
	    XSetIconName(winPtr->display, winPtr->window, wmPtr->iconName);
	}
    
	if (wmPtr->master != None) {
	    XSetTransientForHint(winPtr->display, winPtr->window,
		    wmPtr->master);
	}
    
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
	UpdateHints(winPtr);
	UpdateWmProtocols(wmPtr);
	if (wmPtr->cmdArgv != NULL) {
	    XSetCommand(winPtr->display, winPtr->window, wmPtr->cmdArgv,
		    wmPtr->cmdArgc);
	}
	if (wmPtr->clientMachine != NULL) {
	    if (XStringListToTextProperty(&wmPtr->clientMachine, 1, &textProp)
		    != 0) {
		XSetWMClientMachine(winPtr->display, winPtr->window,
			&textProp);
		XFree((char *) textProp.value);
	    }
	}
    }
    if (wmPtr->hints.initial_state == WithdrawnState) {
	return;
    }
    if (wmPtr->iconFor != NULL) {
	/*
	 * This window is an icon for somebody else.  Make sure that
	 * the geometry is up-to-date, then return without mapping
	 * the window.
	 */

	if (wmPtr->flags & WM_UPDATE_PENDING) {
	    Tcl_CancelIdleCall(UpdateGeometryInfo, (ClientData) winPtr);
	}
	UpdateGeometryInfo((ClientData) winPtr);
	return;
    }
    wmPtr->flags |= WM_ABOUT_TO_MAP;
    if (wmPtr->flags & WM_UPDATE_PENDING) {
	Tcl_CancelIdleCall(UpdateGeometryInfo, (ClientData) winPtr);
    }
    UpdateGeometryInfo((ClientData) winPtr);
    wmPtr->flags &= ~WM_ABOUT_TO_MAP;

    /*
     * Map the window, then wait to be sure that the window manager has
     * processed the map operation.
     */

    XMapWindow(winPtr->display, winPtr->window);
    if (wmPtr->hints.initial_state == NormalState) {
	WaitForMapNotify(winPtr, 1);
    }
}

/*
 *--------------------------------------------------------------
 *
 * TkWmUnmapWindow --
 *
 *	This procedure is invoked to unmap a top-level window.  The
 *	only thing it does special is to wait for the window actually
 *	to be unmapped.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Unmaps the window.
 *
 *--------------------------------------------------------------
 */

void
TkWmUnmapWindow(winPtr)
    TkWindow *winPtr;		/* Top-level window that's about to
				 * be mapped. */
{
    /*
     * It seems to be important to wait after unmapping a top-level
     * window until the window really gets unmapped.  I don't completely
     * understand all the interactions with the window manager, but if
     * we go on without waiting, and if the window is then mapped again
     * quickly, events seem to get lost so that we think the window isn't
     * mapped when in fact it is mapped.  I suspect that this has something
     * to do with the window manager filtering Map events (and possily not
     * filtering Unmap events?).
     */ 
    XUnmapWindow(winPtr->display, winPtr->window);
    WaitForMapNotify(winPtr, 0);
}

/*
 *--------------------------------------------------------------
 *
 * TkWmDeadWindow --
 *
 *	This procedure is invoked when a top-level window is
 *	about to be deleted.  It cleans up the wm-related data
 *	structures for the window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The WmInfo structure for winPtr gets freed up.
 *
 *--------------------------------------------------------------
 */

void
TkWmDeadWindow(winPtr)
    TkWindow *winPtr;		/* Top-level window that's being deleted. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    WmInfo *wmPtr2;

    if (wmPtr == NULL) {
	return;
    }
    if (firstWmPtr == wmPtr) {
	firstWmPtr = wmPtr->nextPtr;
    } else {
	register WmInfo *prevPtr;

	for (prevPtr = firstWmPtr; ; prevPtr = prevPtr->nextPtr) {
	    if (prevPtr == NULL) {
		panic("couldn't unlink window in TkWmDeadWindow");
	    }
	    if (prevPtr->nextPtr == wmPtr) {
		prevPtr->nextPtr = wmPtr->nextPtr;
		break;
	    }
	}
    }
    if (wmPtr->hints.flags & IconPixmapHint) {
	Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_pixmap);
    }
    if (wmPtr->hints.flags & IconMaskHint) {
	Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_mask);
    }
    if (wmPtr->icon != NULL) {
	wmPtr2 = ((TkWindow *) wmPtr->icon)->wmInfoPtr;
	wmPtr2->iconFor = NULL;
	wmPtr2->withdrawn = 1;
    }
    if (wmPtr->iconFor != NULL) {
	wmPtr2 = ((TkWindow *) wmPtr->iconFor)->wmInfoPtr;
	wmPtr2->icon = NULL;
	wmPtr2->hints.flags &= ~IconWindowHint;
	UpdateHints((TkWindow *) wmPtr->iconFor);
    }
    while (wmPtr->protPtr != NULL) {
	ProtocolHandler *protPtr;

	protPtr = wmPtr->protPtr;
	wmPtr->protPtr = protPtr->nextPtr;
	Tcl_EventuallyFree((ClientData) protPtr, TCL_DYNAMIC);
    }
    if (wmPtr->cmdArgv != NULL) {
	ckfree((char *) wmPtr->cmdArgv);
    }
    if (wmPtr->clientMachine != NULL) {
	ckfree((char *) wmPtr->clientMachine);
    }
    if (wmPtr->flags & WM_UPDATE_PENDING) {
	Tcl_CancelIdleCall(UpdateGeometryInfo, (ClientData) winPtr);
    }
    ckfree((char *) wmPtr);
    winPtr->wmInfoPtr = NULL;
}

/*
 *--------------------------------------------------------------
 *
 * TkWmSetClass --
 *
 *	This procedure is invoked whenever a top-level window's
 *	class is changed.  If the window has been mapped then this
 *	procedure updates the window manager property for the
 *	class.  If the window hasn't been mapped, the update is
 *	deferred until just before the first mapping.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A window property may get updated.
 *
 *--------------------------------------------------------------
 */

void
TkWmSetClass(winPtr)
    TkWindow *winPtr;		/* Newly-created top-level window. */
{
    if (winPtr->wmInfoPtr->flags & WM_NEVER_MAPPED) {
	return;
    }

    if (winPtr->classUid != NULL) {
	XClassHint *classPtr;

	classPtr = XAllocClassHint();
	classPtr->res_name = winPtr->nameUid;
	classPtr->res_class = winPtr->classUid;
	XSetClassHint(winPtr->display, winPtr->window, classPtr);
	XFree((char *) classPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_WmCmd --
 *
 *	This procedure is invoked to process the "wm" Tcl command.
 *	See the user documentation for details on what it does.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
Tk_WmCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr;
    register WmInfo *wmPtr;
    int c;
    size_t length;

    if (argc < 2) {
	wrongNumArgs:
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option window ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 't') && (strncmp(argv[1], "tracing", length) == 0)
	    && (length >= 3)) {
	if ((argc != 2) && (argc != 3)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " tracing ?boolean?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 2) {
	    interp->result = (wmTracing) ? "on" : "off";
	    return TCL_OK;
	}
	return Tcl_GetBoolean(interp, argv[2], &wmTracing);
    }

    if (argc < 3) {
	goto wrongNumArgs;
    }
    winPtr = (TkWindow *) Tk_NameToWindow(interp, argv[2], tkwin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	Tcl_AppendResult(interp, "window \"", winPtr->pathName,
		"\" isn't a top-level window", (char *) NULL);
	return TCL_ERROR;
    }
    wmPtr = winPtr->wmInfoPtr;
    if ((c == 'a') && (strncmp(argv[1], "aspect", length) == 0)) {
	int numer1, denom1, numer2, denom2;

	if ((argc != 3) && (argc != 7)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " aspect window ?minNumer minDenom ",
		    "maxNumer maxDenom?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->sizeHintsFlags & PAspect) {
		sprintf(interp->result, "%d %d %d %d", wmPtr->minAspect.x,
			wmPtr->minAspect.y, wmPtr->maxAspect.x,
			wmPtr->maxAspect.y);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->sizeHintsFlags &= ~PAspect;
	} else {
	    if ((Tcl_GetInt(interp, argv[3], &numer1) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[4], &denom1) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[5], &numer2) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[6], &denom2) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    if ((numer1 <= 0) || (denom1 <= 0) || (numer2 <= 0) ||
		    (denom2 <= 0)) {
		interp->result = "aspect number can't be <= 0";
		return TCL_ERROR;
	    }
	    wmPtr->minAspect.x = numer1;
	    wmPtr->minAspect.y = denom1;
	    wmPtr->maxAspect.x = numer2;
	    wmPtr->maxAspect.y = denom2;
	    wmPtr->sizeHintsFlags |= PAspect;
	}
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
	goto updateGeom;
    } else if ((c == 'c') && (strncmp(argv[1], "client", length) == 0)
	    && (length >= 2)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " client window ?name?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->clientMachine != NULL) {
		interp->result = wmPtr->clientMachine;
	    }
	    return TCL_OK;
	}
	if (argv[3][0] == 0) {
	    if (wmPtr->clientMachine != NULL) {
		ckfree((char *) wmPtr->clientMachine);
		wmPtr->clientMachine = NULL;
		if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
		    XDeleteProperty(winPtr->display, winPtr->window,
			    Tk_InternAtom((Tk_Window) winPtr,
			    "WM_CLIENT_MACHINE"));
		}
	    }
	    return TCL_OK;
	}
	if (wmPtr->clientMachine != NULL) {
	    ckfree((char *) wmPtr->clientMachine);
	}
	wmPtr->clientMachine = (char *)
		ckalloc((unsigned) (strlen(argv[3]) + 1));
	strcpy(wmPtr->clientMachine, argv[3]);
	if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	    XTextProperty textProp;
	    if (XStringListToTextProperty(&wmPtr->clientMachine, 1, &textProp)
		    != 0) {
		XSetWMClientMachine(winPtr->display, winPtr->window,
			&textProp);
		XFree((char *) textProp.value);
	    }
	}
    } else if ((c == 'c') && (strncmp(argv[1], "colormapwindows", length) == 0)
	    && (length >= 3)) {
	Window *cmapList;
	TkWindow *winPtr2;
	int count, i, windowArgc, gotToplevel;
	char buffer[20], **windowArgv;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " colormapwindows window ?windowList?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    Tk_MakeWindowExist((Tk_Window) winPtr);
	    if (XGetWMColormapWindows(winPtr->display, winPtr->window,
		    &cmapList, &count) == 0) {
		return TCL_OK;
	    }
	    for (i = 0; i < count; i++) {
		if ((i == (count-1))
			&& (wmPtr->flags & WM_ADDED_TOPLEVEL_COLORMAP)) {
		    break;
		}
	        winPtr2  = (TkWindow *) Tk_IdToWindow(winPtr->display,
			cmapList[i]);
		if (winPtr2 == NULL) {
		    sprintf(buffer, "0x%lx", cmapList[i]);
		    Tcl_AppendElement(interp, buffer);
		} else {
		    Tcl_AppendElement(interp, winPtr2->pathName);
		}
	    }
	    XFree((char *) cmapList);
	    return TCL_OK;
	}
	if (Tcl_SplitList(interp, argv[3], &windowArgc, &windowArgv)
		!= TCL_OK) {
	    return TCL_ERROR;
	}
	cmapList = (Window *) ckalloc((unsigned)
		((windowArgc+1)*sizeof(Window)));
	gotToplevel = 0;
	for (i = 0; i < windowArgc; i++) {
	    winPtr2 = (TkWindow *) Tk_NameToWindow(interp, windowArgv[i],
		    tkwin);
	    if (winPtr2 == NULL) {
		ckfree((char *) cmapList);
		ckfree((char *) windowArgv);
		return TCL_ERROR;
	    }
	    if (winPtr2 == winPtr) {
		gotToplevel = 1;
	    }
	    if (winPtr2->window == None) {
		Tk_MakeWindowExist((Tk_Window) winPtr2);
	    }
	    cmapList[i] = winPtr2->window;
	}
	if (!gotToplevel) {
	    wmPtr->flags |= WM_ADDED_TOPLEVEL_COLORMAP;
	    cmapList[windowArgc] = winPtr->window;
	    windowArgc++;
	} else {
	    wmPtr->flags &= ~WM_ADDED_TOPLEVEL_COLORMAP;
	}
	wmPtr->flags |= WM_COLORMAPS_EXPLICIT;
	XSetWMColormapWindows(winPtr->display, winPtr->window, cmapList,
		windowArgc);
	ckfree((char *) cmapList);
	ckfree((char *) windowArgv);
	return TCL_OK;
    } else if ((c == 'c') && (strncmp(argv[1], "command", length) == 0)
	    && (length >= 3)) {
	int cmdArgc;
	char **cmdArgv;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " command window ?value?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->cmdArgv != NULL) {
		interp->result = Tcl_Merge(wmPtr->cmdArgc, wmPtr->cmdArgv);
		interp->freeProc = TCL_DYNAMIC;
	    }
	    return TCL_OK;
	}
	if (argv[3][0] == 0) {
	    if (wmPtr->cmdArgv != NULL) {
		ckfree((char *) wmPtr->cmdArgv);
		wmPtr->cmdArgv = NULL;
		if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
		    XDeleteProperty(winPtr->display, winPtr->window,
			    Tk_InternAtom((Tk_Window) winPtr, "WM_COMMAND"));
		}
	    }
	    return TCL_OK;
	}
	if (Tcl_SplitList(interp, argv[3], &cmdArgc, &cmdArgv) != TCL_OK) {
	    return TCL_ERROR;
	}
	if (wmPtr->cmdArgv != NULL) {
	    ckfree((char *) wmPtr->cmdArgv);
	}
	wmPtr->cmdArgc = cmdArgc;
	wmPtr->cmdArgv = cmdArgv;
	if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	    XSetCommand(winPtr->display, winPtr->window, cmdArgv, cmdArgc);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "deiconify", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " deiconify window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->iconFor != NULL) {
	    Tcl_AppendResult(interp, "can't deiconify ", argv[2],
		    ": it is an icon for ", winPtr->pathName, (char *) NULL);
	    return TCL_ERROR;
	}
	wmPtr->hints.initial_state = NormalState;
	wmPtr->withdrawn = 0;
	if (wmPtr->flags & WM_NEVER_MAPPED) {
	    return TCL_OK;
	}
	UpdateHints(winPtr);
	Tk_MapWindow((Tk_Window) winPtr);
    } else if ((c == 'f') && (strncmp(argv[1], "focusmodel", length) == 0)
	    && (length >= 2)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " focusmodel window ?active|passive?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    interp->result = wmPtr->hints.input ? "passive" : "active";
	    return TCL_OK;
	}
	c = argv[3][0];
	length = strlen(argv[3]);
	if ((c == 'a') && (strncmp(argv[3], "active", length) == 0)) {
	    wmPtr->hints.input = False;
	} else if ((c == 'p') && (strncmp(argv[3], "passive", length) == 0)) {
	    wmPtr->hints.input = True;
	} else {
	    Tcl_AppendResult(interp, "bad argument \"", argv[3],
		    "\": must be active or passive", (char *) NULL);
	    return TCL_ERROR;
	}
	UpdateHints(winPtr);
    } else if ((c == 'f') && (strncmp(argv[1], "frame", length) == 0)
	    && (length >= 2)) {
	Window window;

	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " frame window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	window = wmPtr->reparent;
	if (window == None) {
	    window = Tk_WindowId((Tk_Window) winPtr);
	}
	sprintf(interp->result, "0x%x", (unsigned int) window);
    } else if ((c == 'g') && (strncmp(argv[1], "geometry", length) == 0)
	    && (length >= 2)) {
	char xSign, ySign;
	int width, height;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " geometry window ?newGeometry?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    xSign = (wmPtr->flags & WM_NEGATIVE_X) ? '-' : '+';
	    ySign = (wmPtr->flags & WM_NEGATIVE_Y) ? '-' : '+';
	    if (wmPtr->gridWin != NULL) {
		width = wmPtr->reqGridWidth + (winPtr->changes.width
			- winPtr->reqWidth)/wmPtr->widthInc;
		height = wmPtr->reqGridHeight + (winPtr->changes.height
			- winPtr->reqHeight)/wmPtr->heightInc;
	    } else {
		width = winPtr->changes.width;
		height = winPtr->changes.height;
	    }
	    sprintf(interp->result, "%dx%d%c%d%c%d", width, height,
		    xSign, wmPtr->x, ySign, wmPtr->y);
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->width = -1;
	    wmPtr->height = -1;
	    goto updateGeom;
	}
	return ParseGeometry(interp, argv[3], winPtr);
    } else if ((c == 'g') && (strncmp(argv[1], "grid", length) == 0)
	    && (length >= 3)) {
	int reqWidth, reqHeight, widthInc, heightInc;

	if ((argc != 3) && (argc != 7)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " grid window ?baseWidth baseHeight ",
		    "widthInc heightInc?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->sizeHintsFlags & PBaseSize) {
		sprintf(interp->result, "%d %d %d %d", wmPtr->reqGridWidth,
			wmPtr->reqGridHeight, wmPtr->widthInc,
			wmPtr->heightInc);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    /*
	     * Turn off gridding and reset the width and height
	     * to make sense as ungridded numbers.
	     */

	    wmPtr->sizeHintsFlags &= ~(PBaseSize|PResizeInc);
	    if (wmPtr->width != -1) {
		wmPtr->width = winPtr->reqWidth + (wmPtr->width
			- wmPtr->reqGridWidth)*wmPtr->widthInc;
		wmPtr->height = winPtr->reqHeight + (wmPtr->height
			- wmPtr->reqGridHeight)*wmPtr->heightInc;
	    }
	    wmPtr->widthInc = 1;
	    wmPtr->heightInc = 1;
	} else {
	    if ((Tcl_GetInt(interp, argv[3], &reqWidth) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[4], &reqHeight) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[5], &widthInc) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[6], &heightInc) != TCL_OK)) {
		return TCL_ERROR;
	    }
	    if (reqWidth < 0) {
		interp->result = "baseWidth can't be < 0";
		return TCL_ERROR;
	    }
	    if (reqHeight < 0) {
		interp->result = "baseHeight can't be < 0";
		return TCL_ERROR;
	    }
	    if (widthInc < 0) {
		interp->result = "widthInc can't be < 0";
		return TCL_ERROR;
	    }
	    if (heightInc < 0) {
		interp->result = "heightInc can't be < 0";
		return TCL_ERROR;
	    }
	    Tk_SetGrid((Tk_Window) winPtr, reqWidth, reqHeight, widthInc,
		    heightInc);
	}
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
	goto updateGeom;
    } else if ((c == 'g') && (strncmp(argv[1], "group", length) == 0)
	    && (length >= 3)) {
	Tk_Window tkwin2;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " group window ?pathName?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->hints.flags & WindowGroupHint) {
		interp->result = wmPtr->leaderName;
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->hints.flags &= ~WindowGroupHint;
	    wmPtr->leaderName = NULL;
	} else {
	    tkwin2 = Tk_NameToWindow(interp, argv[3], tkwin);
	    if (tkwin2 == NULL) {
		return TCL_ERROR;
	    }
	    Tk_MakeWindowExist(tkwin2);
	    wmPtr->hints.window_group = Tk_WindowId(tkwin2);
	    wmPtr->hints.flags |= WindowGroupHint;
	    wmPtr->leaderName = Tk_PathName(tkwin2);
	}
	UpdateHints(winPtr);
    } else if ((c == 'i') && (strncmp(argv[1], "iconbitmap", length) == 0)
	    && (length >= 5)) {
	Pixmap pixmap;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconbitmap window ?bitmap?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->hints.flags & IconPixmapHint) {
		interp->result = Tk_NameOfBitmap(winPtr->display,
			wmPtr->hints.icon_pixmap);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    if (wmPtr->hints.icon_pixmap != None) {
		Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_pixmap);
		wmPtr->hints.icon_pixmap = None;
	    }
	    wmPtr->hints.flags &= ~IconPixmapHint;
	} else {
	    pixmap = Tk_GetBitmap(interp, (Tk_Window) winPtr,
		    Tk_GetUid(argv[3]));
	    if (pixmap == None) {
		return TCL_ERROR;
	    }
	    wmPtr->hints.icon_pixmap = pixmap;
	    wmPtr->hints.flags |= IconPixmapHint;
	}
	UpdateHints(winPtr);
    } else if ((c == 'i') && (strncmp(argv[1], "iconify", length) == 0)
	    && (length >= 5)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconify window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (Tk_Attributes((Tk_Window) winPtr)->override_redirect) {
	    Tcl_AppendResult(interp, "can't iconify \"", winPtr->pathName,
		    "\": override-redirect flag is set", (char *) NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->master != None) {
	    Tcl_AppendResult(interp, "can't iconify \"", winPtr->pathName,
		    "\": it is a transient", (char *) NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->iconFor != NULL) {
	    Tcl_AppendResult(interp, "can't iconify ", argv[2],
		    ": it is an icon for ", winPtr->pathName, (char *) NULL);
	    return TCL_ERROR;
	}
	wmPtr->hints.initial_state = IconicState;
	if (wmPtr->flags & WM_NEVER_MAPPED) {
	    return TCL_OK;
	}
	if (wmPtr->withdrawn) {
	    UpdateHints(winPtr);
	    Tk_MapWindow((Tk_Window) winPtr);
	    wmPtr->withdrawn = 0;
	} else {
	    if (XIconifyWindow(winPtr->display, winPtr->window,
		winPtr->screenNum) == 0) {
	    interp->result =
		    "couldn't send iconify message to window manager";
	    return TCL_ERROR;
	    }
	    WaitForMapNotify(winPtr, 0);
	}
    } else if ((c == 'i') && (strncmp(argv[1], "iconmask", length) == 0)
	    && (length >= 5)) {
	Pixmap pixmap;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconmask window ?bitmap?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->hints.flags & IconMaskHint) {
		interp->result = Tk_NameOfBitmap(winPtr->display,
			wmPtr->hints.icon_mask);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    if (wmPtr->hints.icon_mask != None) {
		Tk_FreeBitmap(winPtr->display, wmPtr->hints.icon_mask);
	    }
	    wmPtr->hints.flags &= ~IconMaskHint;
	} else {
	    pixmap = Tk_GetBitmap(interp, tkwin, Tk_GetUid(argv[3]));
	    if (pixmap == None) {
		return TCL_ERROR;
	    }
	    wmPtr->hints.icon_mask = pixmap;
	    wmPtr->hints.flags |= IconMaskHint;
	}
	UpdateHints(winPtr);
    } else if ((c == 'i') && (strncmp(argv[1], "iconname", length) == 0)
	    && (length >= 5)) {
	if (argc > 4) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconname window ?newName?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    interp->result = (wmPtr->iconName != NULL) ? wmPtr->iconName : "";
	    return TCL_OK;
	} else {
	    wmPtr->iconName = Tk_GetUid(argv[3]);
	    if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
		XSetIconName(winPtr->display, winPtr->window, wmPtr->iconName);
	    }
	}
    } else if ((c == 'i') && (strncmp(argv[1], "iconposition", length) == 0)
	    && (length >= 5)) {
	int x, y;

	if ((argc != 3) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconposition window ?x y?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->hints.flags & IconPositionHint) {
		sprintf(interp->result, "%d %d", wmPtr->hints.icon_x,
			wmPtr->hints.icon_y);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->hints.flags &= ~IconPositionHint;
	} else {
	    if ((Tcl_GetInt(interp, argv[3], &x) != TCL_OK)
		    || (Tcl_GetInt(interp, argv[4], &y) != TCL_OK)){
		return TCL_ERROR;
	    }
	    wmPtr->hints.icon_x = x;
	    wmPtr->hints.icon_y = y;
	    wmPtr->hints.flags |= IconPositionHint;
	}
	UpdateHints(winPtr);
    } else if ((c == 'i') && (strncmp(argv[1], "iconwindow", length) == 0)
	    && (length >= 5)) {
	Tk_Window tkwin2;
	WmInfo *wmPtr2;
	XSetWindowAttributes atts;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " iconwindow window ?pathName?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->icon != NULL) {
		interp->result = Tk_PathName(wmPtr->icon);
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->hints.flags &= ~IconWindowHint;
	    if (wmPtr->icon != NULL) {
		/*
		 * Remove the icon window relationship.  In principle we
		 * should also re-enable button events for the window, but
		 * this doesn't work in general because the window manager
		 * is probably selecting on them (we'll get an error if
		 * we try to re-enable the events).  So, just leave the
		 * icon window event-challenged;  the user will have to
		 * recreate it if they want button events.
		 */

		wmPtr2 = ((TkWindow *) wmPtr->icon)->wmInfoPtr;
		wmPtr2->iconFor = NULL;
		wmPtr2->withdrawn = 1;
		wmPtr2->hints.initial_state = WithdrawnState;
	    }
	    wmPtr->icon = NULL;
	} else {
	    tkwin2 = Tk_NameToWindow(interp, argv[3], tkwin);
	    if (tkwin2 == NULL) {
		return TCL_ERROR;
	    }
	    if (!Tk_IsTopLevel(tkwin2)) {
		Tcl_AppendResult(interp, "can't use ", argv[3],
			" as icon window: not at top level", (char *) NULL);
		return TCL_ERROR;
	    }
	    wmPtr2 = ((TkWindow *) tkwin2)->wmInfoPtr;
	    if (wmPtr2->iconFor != NULL) {
		Tcl_AppendResult(interp, argv[3], " is already an icon for ",
			Tk_PathName(wmPtr2->iconFor), (char *) NULL);
		return TCL_ERROR;
	    }
	    if (wmPtr->icon != NULL) {
		WmInfo *wmPtr3 = ((TkWindow *) wmPtr->icon)->wmInfoPtr;
		wmPtr3->iconFor = NULL;
		wmPtr3->withdrawn = 1;

		/*
		 * Let the window use button events again.
		 */

		atts.event_mask = Tk_Attributes(wmPtr->icon)->event_mask
			| ButtonPressMask;
		Tk_ChangeWindowAttributes(wmPtr->icon, CWEventMask, &atts);
	    }

	    /*
	     * Disable button events in the icon window:  some window
	     * managers (like olvwm) want to get the events themselves,
	     * but X only allows one application at a time to receive
	     * button events for a window.
	     */

	    atts.event_mask = Tk_Attributes(tkwin2)->event_mask
		    & ~ButtonPressMask;
	    Tk_ChangeWindowAttributes(tkwin2, CWEventMask, &atts);
	    Tk_MakeWindowExist(tkwin2);
	    wmPtr->hints.icon_window = Tk_WindowId(tkwin2);
	    wmPtr->hints.flags |= IconWindowHint;
	    wmPtr->icon = tkwin2;
	    wmPtr2->iconFor = (Tk_Window) winPtr;
	    if (!wmPtr2->withdrawn && !(wmPtr2->flags & WM_NEVER_MAPPED)) {
		wmPtr2->withdrawn = 0;
		if (XWithdrawWindow(Tk_Display(tkwin2), Tk_WindowId(tkwin2),
			Tk_ScreenNumber(tkwin2)) == 0) {
		    interp->result =
			    "couldn't send withdraw message to window manager";
		    return TCL_ERROR;
		}
		WaitForMapNotify((TkWindow *) tkwin2, 0);
	    }
	}
	UpdateHints(winPtr);
    } else if ((c == 'm') && (strncmp(argv[1], "maxsize", length) == 0)
	    && (length >= 2)) {
	int width, height;
	if ((argc != 3) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " maxsize window ?width height?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    GetMaxSize(wmPtr, &width, &height);
	    sprintf(interp->result, "%d %d", width, height);
	    return TCL_OK;
	}
	if ((Tcl_GetInt(interp, argv[3], &width) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[4], &height) != TCL_OK)) {
	    return TCL_ERROR;
	}
	wmPtr->maxWidth = width;
	wmPtr->maxHeight = height;
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
	goto updateGeom;
    } else if ((c == 'm') && (strncmp(argv[1], "minsize", length) == 0)
	    && (length >= 2)) {
	int width, height;
	if ((argc != 3) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " minsize window ?width height?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    sprintf(interp->result, "%d %d", wmPtr->minWidth,
		    wmPtr->minHeight);
	    return TCL_OK;
	}
	if ((Tcl_GetInt(interp, argv[3], &width) != TCL_OK)
		|| (Tcl_GetInt(interp, argv[4], &height) != TCL_OK)) {
	    return TCL_ERROR;
	}
	wmPtr->minWidth = width;
	wmPtr->minHeight = height;
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
	goto updateGeom;
    } else if ((c == 'o')
	    && (strncmp(argv[1], "overrideredirect", length) == 0)) {
	int boolean;
	XSetWindowAttributes atts;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " overrideredirect window ?boolean?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (Tk_Attributes((Tk_Window) winPtr)->override_redirect) {
		interp->result = "1";
	    } else {
		interp->result = "0";
	    }
	    return TCL_OK;
	}
	if (Tcl_GetBoolean(interp, argv[3], &boolean) != TCL_OK) {
	    return TCL_ERROR;
	}
	atts.override_redirect = (boolean) ? True : False;
	Tk_ChangeWindowAttributes((Tk_Window) winPtr, CWOverrideRedirect,
		&atts);
    } else if ((c == 'p') && (strncmp(argv[1], "positionfrom", length) == 0)
	    && (length >= 2)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " positionfrom window ?user/program?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->sizeHintsFlags & USPosition) {
		interp->result = "user";
	    } else if (wmPtr->sizeHintsFlags & PPosition) {
		interp->result = "program";
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->sizeHintsFlags &= ~(USPosition|PPosition);
	} else {
	    c = argv[3][0];
	    length = strlen(argv[3]);
	    if ((c == 'u') && (strncmp(argv[3], "user", length) == 0)) {
		wmPtr->sizeHintsFlags &= ~PPosition;
		wmPtr->sizeHintsFlags |= USPosition;
	    } else if ((c == 'p') && (strncmp(argv[3], "program", length) == 0)) {
		wmPtr->sizeHintsFlags &= ~USPosition;
		wmPtr->sizeHintsFlags |= PPosition;
	    } else {
		Tcl_AppendResult(interp, "bad argument \"", argv[3],
			"\": must be program or user", (char *) NULL);
		return TCL_ERROR;
	    }
	}
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
	goto updateGeom;
    } else if ((c == 'p') && (strncmp(argv[1], "protocol", length) == 0)
	    && (length >= 2)) {
	register ProtocolHandler *protPtr, *prevPtr;
	Atom protocol;
	int cmdLength;

	if ((argc < 3) || (argc > 5)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " protocol window ?name? ?command?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    /*
	     * Return a list of all defined protocols for the window.
	     */
	    for (protPtr = wmPtr->protPtr; protPtr != NULL;
		    protPtr = protPtr->nextPtr) {
		Tcl_AppendElement(interp,
			Tk_GetAtomName((Tk_Window) winPtr, protPtr->protocol));
	    }
	    return TCL_OK;
	}
	protocol = Tk_InternAtom((Tk_Window) winPtr, argv[3]);
	if (argc == 4) {
	    /*
	     * Return the command to handle a given protocol.
	     */
	    for (protPtr = wmPtr->protPtr; protPtr != NULL;
		    protPtr = protPtr->nextPtr) {
		if (protPtr->protocol == protocol) {
		    interp->result = protPtr->command;
		    return TCL_OK;
		}
	    }
	    return TCL_OK;
	}

	/*
	 * Delete any current protocol handler, then create a new
	 * one with the specified command, unless the command is
	 * empty.
	 */

	for (protPtr = wmPtr->protPtr, prevPtr = NULL; protPtr != NULL;
		prevPtr = protPtr, protPtr = protPtr->nextPtr) {
	    if (protPtr->protocol == protocol) {
		if (prevPtr == NULL) {
		    wmPtr->protPtr = protPtr->nextPtr;
		} else {
		    prevPtr->nextPtr = protPtr->nextPtr;
		}
		Tcl_EventuallyFree((ClientData) protPtr, TCL_DYNAMIC);
		break;
	    }
	}
	cmdLength = strlen(argv[4]);
	if (cmdLength > 0) {
	    protPtr = (ProtocolHandler *) ckalloc(HANDLER_SIZE(cmdLength));
	    protPtr->protocol = protocol;
	    protPtr->nextPtr = wmPtr->protPtr;
	    wmPtr->protPtr = protPtr;
	    protPtr->interp = interp;
	    strcpy(protPtr->command, argv[4]);
	}
	if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	    UpdateWmProtocols(wmPtr);
	}
    } else if ((c == 'r') && (strncmp(argv[1], "resizable", length) == 0)) {
	int width, height;

	if ((argc != 3) && (argc != 5)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " resizable window ?width height?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    sprintf(interp->result, "%d %d",
		    (wmPtr->flags  & WM_WIDTH_NOT_RESIZABLE) ? 0 : 1,
		    (wmPtr->flags  & WM_HEIGHT_NOT_RESIZABLE) ? 0 : 1);
	    return TCL_OK;
	}
	if ((Tcl_GetBoolean(interp, argv[3], &width) != TCL_OK)
		|| (Tcl_GetBoolean(interp, argv[4], &height) != TCL_OK)) {
	    return TCL_ERROR;
	}
	if (width) {
	    wmPtr->flags &= ~WM_WIDTH_NOT_RESIZABLE;
	} else {
	    wmPtr->flags |= WM_WIDTH_NOT_RESIZABLE;
	}
	if (height) {
	    wmPtr->flags &= ~WM_HEIGHT_NOT_RESIZABLE;
	} else {
	    wmPtr->flags |= WM_HEIGHT_NOT_RESIZABLE;
	}
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
	goto updateGeom;
    } else if ((c == 's') && (strncmp(argv[1], "sizefrom", length) == 0)
	    && (length >= 2)) {
	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " sizefrom window ?user|program?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->sizeHintsFlags & USSize) {
		interp->result = "user";
	    } else if (wmPtr->sizeHintsFlags & PSize) {
		interp->result = "program";
	    }
	    return TCL_OK;
	}
	if (*argv[3] == '\0') {
	    wmPtr->sizeHintsFlags &= ~(USSize|PSize);
	} else {
	    c = argv[3][0];
	    length = strlen(argv[3]);
	    if ((c == 'u') && (strncmp(argv[3], "user", length) == 0)) {
		wmPtr->sizeHintsFlags &= ~PSize;
		wmPtr->sizeHintsFlags |= USSize;
	    } else if ((c == 'p')
		    && (strncmp(argv[3], "program", length) == 0)) {
		wmPtr->sizeHintsFlags &= ~USSize;
		wmPtr->sizeHintsFlags |= PSize;
	    } else {
		Tcl_AppendResult(interp, "bad argument \"", argv[3],
			"\": must be program or user", (char *) NULL);
		return TCL_ERROR;
	    }
	}
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
	goto updateGeom;
    } else if ((c == 's') && (strncmp(argv[1], "state", length) == 0)
	    && (length >= 2)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " state window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->iconFor != NULL) {
	    interp->result = "icon";
	} else if (wmPtr->withdrawn) {
	    interp->result = "withdrawn";
	} else if (Tk_IsMapped((Tk_Window) winPtr)
		|| ((wmPtr->flags & WM_NEVER_MAPPED)
		&& (wmPtr->hints.initial_state == NormalState))) {
	    interp->result = "normal";
	} else {
	    interp->result = "iconic";
	}
    } else if ((c == 't') && (strncmp(argv[1], "title", length) == 0)
	    && (length >= 2)) {
	if (argc > 4) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " title window ?newTitle?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    interp->result = (wmPtr->titleUid != NULL) ? wmPtr->titleUid
		    : winPtr->nameUid;
	    return TCL_OK;
	} else {
	    wmPtr->titleUid = Tk_GetUid(argv[3]);
	    if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
		XTextProperty textProp;

		if (XStringListToTextProperty(&wmPtr->titleUid, 1,
			&textProp)  != 0) {
		    XSetWMName(winPtr->display, winPtr->window, &textProp);
		    XFree((char *) textProp.value);
		}
	    }
	}
    } else if ((c == 't') && (strncmp(argv[1], "transient", length) == 0)
	    && (length >= 3)) {
	Tk_Window master;

	if ((argc != 3) && (argc != 4)) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " transient window ?master?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    if (wmPtr->master != None) {
		interp->result = wmPtr->masterWindowName;
	    }
	    return TCL_OK;
	}
	if (argv[3][0] == '\0') {
	    wmPtr->master = None;
	    wmPtr->masterWindowName = NULL;
	} else {
	    master = Tk_NameToWindow(interp, argv[3], tkwin);
	    if (master == NULL) {
		return TCL_ERROR;
	    }
	    Tk_MakeWindowExist(master);
	    wmPtr->master = Tk_WindowId(master);
	    wmPtr->masterWindowName = Tk_PathName(master);
	}
	if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	    XSetTransientForHint(winPtr->display, winPtr->window,
		    wmPtr->master);
	}
    } else if ((c == 'w') && (strncmp(argv[1], "withdraw", length) == 0)) {
	if (argc != 3) {
	    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
		    argv[0], " withdraw window\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (wmPtr->iconFor != NULL) {
	    Tcl_AppendResult(interp, "can't withdraw ", argv[2],
		    ": it is an icon for ", Tk_PathName(wmPtr->iconFor),
		    (char *) NULL);
	    return TCL_ERROR;
	}
	wmPtr->hints.initial_state = WithdrawnState;
	wmPtr->withdrawn = 1;
	if (wmPtr->flags & WM_NEVER_MAPPED) {
	    return TCL_OK;
	}
	if (XWithdrawWindow(winPtr->display, winPtr->window,
		winPtr->screenNum) == 0) {
	    interp->result =
		    "couldn't send withdraw message to window manager";
	    return TCL_ERROR;
	}
	WaitForMapNotify(winPtr, 0);
    } else {
	Tcl_AppendResult(interp, "unknown or ambiguous option \"", argv[1],
		"\": must be aspect, client, command, deiconify, ",
		"focusmodel, frame, geometry, grid, group, iconbitmap, ",
		"iconify, iconmask, iconname, iconposition, ",
		"iconwindow, maxsize, minsize, overrideredirect, ",
		"positionfrom, protocol, resizable, sizefrom, state, title, ",
		"transient, or withdraw",
		(char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;

    updateGeom:
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, (ClientData) winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetGrid --
 *
 *	This procedure is invoked by a widget when it wishes to set a grid
 *	coordinate system that controls the size of a top-level window.
 *	It provides a C interface equivalent to the "wm grid" command and
 *	is usually asscoiated with the -setgrid option.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Grid-related information will be passed to the window manager, so
 *	that the top-level window associated with tkwin will resize on
 *	even grid units.  If some other window already controls gridding
 *	for the top-level window then this procedure call has no effect.
 *
 *----------------------------------------------------------------------
 */

void
Tk_SetGrid(tkwin, reqWidth, reqHeight, widthInc, heightInc)
    Tk_Window tkwin;		/* Token for window.  New window mgr info
				 * will be posted for the top-level window
				 * associated with this window. */
    int reqWidth;		/* Width (in grid units) corresponding to
				 * the requested geometry for tkwin. */
    int reqHeight;		/* Height (in grid units) corresponding to
				 * the requested geometry for tkwin. */
    int widthInc, heightInc;	/* Pixel increments corresponding to a
				 * change of one grid unit. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    register WmInfo *wmPtr;

    /*
     * Find the top-level window for tkwin, plus the window manager
     * information.
     */

    while (!(winPtr->flags & TK_TOP_LEVEL)) {
	winPtr = winPtr->parentPtr;
	if (winPtr == NULL) {
	    /*
	     * The window is being deleted... just skip this operation.
	     */

	    return;
	}
    }
    wmPtr = winPtr->wmInfoPtr;

    if ((wmPtr->gridWin != NULL) && (wmPtr->gridWin != tkwin)) {
	return;
    }

    if ((wmPtr->reqGridWidth == reqWidth)
	    && (wmPtr->reqGridHeight == reqHeight)
	    && (wmPtr->widthInc == widthInc)
	    && (wmPtr->heightInc == heightInc)
	    && ((wmPtr->sizeHintsFlags & (PBaseSize|PResizeInc))
		    == PBaseSize|PResizeInc)) {
	return;
    }

    /*
     * If gridding was previously off, then forget about any window
     * size requests made by the user or via "wm geometry":  these are
     * in pixel units and there's no easy way to translate them to
     * grid units since the new requested size of the top-level window in
     * pixels may not yet have been registered yet (it may filter up
     * the hierarchy in DoWhenIdle handlers).  However, if the window
     * has never been mapped yet then just leave the window size alone:
     * assume that it is intended to be in grid units but just happened
     * to have been specified before this procedure was called.
     */

    if ((wmPtr->gridWin == NULL) && !(wmPtr->flags & WM_NEVER_MAPPED)) {
	wmPtr->width = -1;
	wmPtr->height = -1;
    }

    /* 
     * Set the new gridding information, and start the process of passing
     * all of this information to the window manager.
     */

    wmPtr->gridWin = tkwin;
    wmPtr->reqGridWidth = reqWidth;
    wmPtr->reqGridHeight = reqHeight;
    wmPtr->widthInc = widthInc;
    wmPtr->heightInc = heightInc;
    wmPtr->sizeHintsFlags |= PBaseSize|PResizeInc;
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, (ClientData) winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_UnsetGrid --
 *
 *	This procedure cancels the effect of a previous call
 *	to Tk_SetGrid.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If tkwin currently controls gridding for its top-level window,
 *	gridding is cancelled for that top-level window;  if some other
 *	window controls gridding then this procedure has no effect.
 *
 *----------------------------------------------------------------------
 */

void
Tk_UnsetGrid(tkwin)
    Tk_Window tkwin;		/* Token for window that is currently
				 * controlling gridding. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    register WmInfo *wmPtr;

    /*
     * Find the top-level window for tkwin, plus the window manager
     * information.
     */

    while (!(winPtr->flags & TK_TOP_LEVEL)) {
	winPtr = winPtr->parentPtr;
	if (winPtr == NULL) {
	    /*
	     * The window is being deleted... just skip this operation.
	     */

	    return;
	}
    }
    wmPtr = winPtr->wmInfoPtr;
    if (tkwin != wmPtr->gridWin) {
	return;
    }

    wmPtr->gridWin = NULL;
    wmPtr->sizeHintsFlags &= ~(PBaseSize|PResizeInc);
    if (wmPtr->width != -1) {
	wmPtr->width = winPtr->reqWidth + (wmPtr->width
		- wmPtr->reqGridWidth)*wmPtr->widthInc;
	wmPtr->height = winPtr->reqHeight + (wmPtr->height
		- wmPtr->reqGridHeight)*wmPtr->heightInc;
    }
    wmPtr->widthInc = 1;
    wmPtr->heightInc = 1;

    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, (ClientData) winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ConfigureEvent --
 *
 *	This procedure is called to handle ConfigureNotify events on
 *	top-level windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets updated in the WmInfo structure for the window.
 *
 *----------------------------------------------------------------------
 */

static void
ConfigureEvent(winPtr, configEventPtr)
    TkWindow *winPtr;			/* Top-level window. */
    XConfigureEvent *configEventPtr;	/* Event that just occurred for
					 * winPtr. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;

    /* 
     * Update size information from the event.  There are a couple of
     * tricky points here:
     *
     * 1. If the user changed the size externally then set wmPtr->width
     *    and wmPtr->height just as if a "wm geometry" command had been
     *    invoked with the same information.
     * 2. However, if the size is changing in response to a request
     *    coming from us (WM_SYNC_PENDING is set), then don't set wmPtr->width
     *    or wmPtr->height if they were previously -1 (otherwise the
     *    window will stop tracking geometry manager requests).
     */

    if (((winPtr->changes.width != configEventPtr->width)
	    || (winPtr->changes.height != configEventPtr->height))
	    && !(wmPtr->flags & WM_SYNC_PENDING)){
	if (wmTracing) {
	    printf("TopLevelEventProc: user changed %s size to %dx%d\n",
		    winPtr->pathName, configEventPtr->width,
		    configEventPtr->height);
	}
	if ((wmPtr->width == -1)
		&& (configEventPtr->width == winPtr->reqWidth)) {
	    /*
	     * Don't set external width, since the user didn't change it
	     * from what the widgets asked for.
	     */
	} else {
	    if (wmPtr->gridWin != NULL) {
		wmPtr->width = wmPtr->reqGridWidth
			+ (configEventPtr->width
			- winPtr->reqWidth)/wmPtr->widthInc;
		if (wmPtr->width < 0) {
		    wmPtr->width = 0;
		}
	    } else {
		wmPtr->width = configEventPtr->width;
	    }
	}
	if ((wmPtr->height == -1)
		&& (configEventPtr->height == winPtr->reqHeight)) {
	    /*
	     * Don't set external height, since the user didn't change it
	     * from what the widgets asked for.
	     */
	} else {
	    if (wmPtr->gridWin != NULL) {
		wmPtr->height = wmPtr->reqGridHeight
			+ (configEventPtr->height
			- winPtr->reqHeight)/wmPtr->heightInc;
		if (wmPtr->height < 0) {
		    wmPtr->height = 0;
		}
	    } else {
		wmPtr->height = configEventPtr->height;
	    }
	}
	wmPtr->configWidth = configEventPtr->width;
	wmPtr->configHeight = configEventPtr->height;
    }

    if (wmTracing) {
	printf("ConfigureEvent: %s x = %d y = %d, width = %d, height = %d",
		winPtr->pathName, configEventPtr->x, configEventPtr->y,
		configEventPtr->width, configEventPtr->height);
	printf(" send_event = %d, serial = %ld\n", configEventPtr->send_event,
		configEventPtr->serial);
    }
    winPtr->changes.width = configEventPtr->width;
    winPtr->changes.height = configEventPtr->height;
    winPtr->changes.border_width = configEventPtr->border_width;
    winPtr->changes.sibling = configEventPtr->above;
    winPtr->changes.stack_mode = Above;

    /*
     * Reparenting window managers make life difficult.  If the
     * window manager reparents a top-level window then the x and y
     * information that comes in events for the window is wrong:
     * it gives the location of the window inside its decorative
     * parent, rather than the location of the window in root
     * coordinates, which is what we want.  Window managers
     * are supposed to send synthetic events with the correct
     * information, but ICCCM doesn't require them to do this
     * under all conditions, and the information provided doesn't
     * include everything we need here.  So, the code below
     * maintains a bunch of information about the parent window.
     * If the window hasn't been reparented, we pretend that
     * there is a parent shrink-wrapped around the window.
     */

    if ((wmPtr->reparent == None) || !ComputeReparentGeometry(winPtr)) {
	wmPtr->parentWidth = configEventPtr->width
		+ 2*configEventPtr->border_width;
	wmPtr->parentHeight = configEventPtr->height
		+ 2*configEventPtr->border_width;
	winPtr->changes.x = wmPtr->x = configEventPtr->x;
	winPtr->changes.y = wmPtr->y = configEventPtr->y;
	if (wmPtr->flags & WM_NEGATIVE_X) {
	    wmPtr->x = wmPtr->vRootWidth - (wmPtr->x + wmPtr->parentWidth);
	}
	if (wmPtr->flags & WM_NEGATIVE_Y) {
	    wmPtr->y = wmPtr->vRootHeight - (wmPtr->y + wmPtr->parentHeight);
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ReparentEvent --
 *
 *	This procedure is called to handle ReparentNotify events on
 *	top-level windows.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Information gets updated in the WmInfo structure for the window.
 *
 *----------------------------------------------------------------------
 */

static void
ReparentEvent(winPtr, reparentEventPtr)
    TkWindow *winPtr;			/* Top-level window. */
    XReparentEvent *reparentEventPtr;	/* Event that just occurred for
					 * winPtr. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    Window vRoot, ancestor, *children, dummy2, *virtualRootPtr;
    Atom actualType;
    int actualFormat;
    unsigned long numItems, bytesAfter;
    unsigned int dummy;
    Tk_ErrorHandler handler;

    /*
     * Identify the root window for winPtr.  This is tricky because of
     * virtual root window managers like tvtwm.  If the window has a
     * property named __SWM_ROOT or __WM_ROOT then this property gives
     * the id for a virtual root window that should be used instead of
     * the root window of the screen.
     */

    vRoot = RootWindow(winPtr->display, winPtr->screenNum);
    wmPtr->vRoot = None;
    handler = Tk_CreateErrorHandler(winPtr->display, -1, -1, -1,
	    (Tk_ErrorProc *) NULL, (ClientData) NULL);
    if (((XGetWindowProperty(winPtr->display, winPtr->window,
	    Tk_InternAtom((Tk_Window) winPtr, "__WM_ROOT"), 0, (long) 1,
	    False, XA_WINDOW, &actualType, &actualFormat, &numItems,
	    &bytesAfter, (unsigned char **) &virtualRootPtr) == Success)
	    && (actualType == XA_WINDOW))
	    || ((XGetWindowProperty(winPtr->display, winPtr->window,
	    Tk_InternAtom((Tk_Window) winPtr, "__SWM_ROOT"), 0, (long) 1,
	    False, XA_WINDOW, &actualType, &actualFormat, &numItems,
	    &bytesAfter, (unsigned char **) &virtualRootPtr) == Success)
	    && (actualType == XA_WINDOW))) {
	if ((actualFormat == 32) && (numItems == 1)) {
	    vRoot = wmPtr->vRoot = *virtualRootPtr;
	} else if (wmTracing) {
	    printf("%s format %d numItems %ld\n",
		    "ReparentEvent got bogus VROOT property:", actualFormat,
		    numItems);
	}
	XFree((char *) virtualRootPtr);
    }
    Tk_DeleteErrorHandler(handler);

    if (wmTracing) {
	printf("ReparentEvent: %s reparented to 0x%x, vRoot = 0x%x\n",
		winPtr->pathName, (unsigned int) reparentEventPtr->parent,
		(unsigned int) vRoot);
    }

    /*
     * Fetch correct geometry information for the new virtual root.
     */

    UpdateVRootGeometry(wmPtr);

    /*
     * If the window's new parent is the root window, then mark it as
     * no longer reparented.
     */

    if (reparentEventPtr->parent == vRoot) {
	noReparent:
	wmPtr->reparent = None;
	wmPtr->parentWidth = winPtr->changes.width
		+ 2*winPtr->changes.border_width;
	wmPtr->parentHeight = winPtr->changes.height
		+ 2*winPtr->changes.border_width;
	wmPtr->xInParent = wmPtr->yInParent = 0;
	winPtr->changes.x = reparentEventPtr->x;
	winPtr->changes.y = reparentEventPtr->y;
	return;
    }

    /*
     * Search up the window hierarchy to find the ancestor of this
     * window that is just below the (virtual) root.  This is tricky
     * because it's possible that things have changed since the event
     * was generated so that the ancestry indicated by the event no
     * longer exists.  If this happens then an error will occur and
     * we just discard the event (there will be a more up-to-date
     * ReparentNotify event coming later).
     */

    handler = Tk_CreateErrorHandler(winPtr->display, -1, -1, -1,
	    (Tk_ErrorProc *) NULL, (ClientData) NULL);
    wmPtr->reparent = reparentEventPtr->parent;
    while (1) {
	if (XQueryTree(winPtr->display, wmPtr->reparent, &dummy2, &ancestor,
		&children, &dummy) == 0) {
	    Tk_DeleteErrorHandler(handler);
	    goto noReparent;
	}
	XFree((char *) children);
	if ((ancestor == vRoot) ||
		(ancestor == RootWindow(winPtr->display, winPtr->screenNum))) {
	    break;
	}
	wmPtr->reparent = ancestor;
    }
    Tk_DeleteErrorHandler(handler);

    if (!ComputeReparentGeometry(winPtr)) {
	goto noReparent;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * ComputeReparentGeometry --
 *
 *	This procedure is invoked to recompute geometry information
 *	related to a reparented top-level window, such as the position
 *	and total size of the parent and the position within it of
 *	the top-level window.
 *
 * Results:
 *	The return value is 1 if everything completed successfully
 *	and 0 if an error occurred while querying information about
 *	winPtr's parents.  In this case winPtr is marked as no longer
 *	being reparented.
 *
 * Side effects:
 *	Geometry information in winPtr and winPtr->wmPtr gets updated.
 *
 *----------------------------------------------------------------------
 */

static int
ComputeReparentGeometry(winPtr)
    TkWindow *winPtr;		/* Top-level window whose reparent info
				 * is to be recomputed. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int width, height, bd;
    unsigned int dummy;
    int xOffset, yOffset, x, y;
    Window dummy2;
    Status status;
    Tk_ErrorHandler handler;

    handler = Tk_CreateErrorHandler(winPtr->display, -1, -1, -1,
	    (Tk_ErrorProc *) NULL, (ClientData) NULL);
    (void) XTranslateCoordinates(winPtr->display, winPtr->window,
	    wmPtr->reparent, 0, 0, &xOffset, &yOffset, &dummy2);
    status = XGetGeometry(winPtr->display, wmPtr->reparent,
	    &dummy2, &x, &y, (unsigned int *) &width,
	    (unsigned int *) &height, (unsigned int *) &bd, &dummy);
    Tk_DeleteErrorHandler(handler);
    if (status == 0) {
	/*
	 * It appears that the reparented parent went away and
	 * no-one told us.  Reset the window to indicate that
	 * it's not reparented.
	 */
	wmPtr->reparent = None;
	wmPtr->xInParent = wmPtr->yInParent = 0;
	return 0;
    }
    wmPtr->xInParent = xOffset + bd - winPtr->changes.border_width;
    wmPtr->yInParent = yOffset + bd - winPtr->changes.border_width;
    wmPtr->parentWidth = width + 2*bd;
    wmPtr->parentHeight = height + 2*bd;

    /*
     * Some tricky issues in updating wmPtr->x and wmPtr->y:
     *
     * 1. Don't update them if the event occurred because of something
     * we did (i.e. WM_SYNC_PENDING and WM_MOVE_PENDING are both set).
     * This is because window managers treat coords differently than Tk,
     * and no two window managers are alike. If the window manager moved
     * the window because we told it to, remember the coordinates we told
     * it, not the ones it actually moved it to.  This allows us to move
     * the window back to the same coordinates later and get the same
     * result. Without this check, windows can "walk" across the screen
     * under some conditions.
     *
     * 2. Don't update wmPtr->x and wmPtr->y unless winPtr->changes.x
     * or winPtr->changes.y has changed (otherwise a size change can
     * spoof us into thinking that the position changed too and defeat
     * the intent of (1) above.
     *
     * 3. Ignore size changes coming from the window system if we're
     * about to change the size ourselves but haven't seen the event for
     * it yet:  our size change is supposed to take priority.
     */

    if (!(wmPtr->flags & WM_MOVE_PENDING)
	    && ((winPtr->changes.x != (x + wmPtr->xInParent))
	    || (winPtr->changes.y != (y + wmPtr->yInParent)))) {
	wmPtr->x = x;
	if (wmPtr->flags & WM_NEGATIVE_X) {
	    wmPtr->x = wmPtr->vRootWidth - (wmPtr->x + wmPtr->parentWidth);
	}
	wmPtr->y = y;
	if (wmPtr->flags & WM_NEGATIVE_Y) {
	    wmPtr->y = wmPtr->vRootHeight - (wmPtr->y + wmPtr->parentHeight);
	}
    }

    winPtr->changes.x = x + wmPtr->xInParent;
    winPtr->changes.y = y + wmPtr->yInParent;
    if (wmTracing) {
	printf("winPtr coords %d,%d, wmPtr coords %d,%d, offsets %d %d\n",
		winPtr->changes.x, winPtr->changes.y, wmPtr->x, wmPtr->y,
		wmPtr->xInParent, wmPtr->yInParent);
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * TopLevelEventProc --
 *
 *	This procedure is invoked when a top-level (or other externally-
 *	managed window) is restructured in any way.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Tk's internal data structures for the window get modified to
 *	reflect the structural change.
 *
 *----------------------------------------------------------------------
 */

static void
TopLevelEventProc(clientData, eventPtr)
    ClientData clientData;		/* Window for which event occurred. */
    XEvent *eventPtr;			/* Event that just happened. */
{
    register TkWindow *winPtr = (TkWindow *) clientData;

    winPtr->wmInfoPtr->flags |= WM_VROOT_OFFSET_STALE;
    if (eventPtr->type == DestroyNotify) {
	Tk_ErrorHandler handler;

	if (!(winPtr->flags & TK_ALREADY_DEAD)) {
	    /*
	     * A top-level window was deleted externally (e.g., by the window
	     * manager).  This is probably not a good thing, but cleanup as
	     * best we can.  The error handler is needed because
	     * Tk_DestroyWindow will try to destroy the window, but of course
	     * it's already gone.
	     */
    
	    handler = Tk_CreateErrorHandler(winPtr->display, -1, -1, -1,
		    (Tk_ErrorProc *) NULL, (ClientData) NULL);
	    Tk_DestroyWindow((Tk_Window) winPtr);
	    Tk_DeleteErrorHandler(handler);
	}
	if (wmTracing) {
	    printf("TopLevelEventProc: %s deleted\n", winPtr->pathName);
	}
    } else if (eventPtr->type == ConfigureNotify) {
	/*
	 * Ignore the event if the window has never been mapped yet.
	 * Such an event occurs only in weird cases like changing the
	 * internal border width of a top-level window, which results
	 * in a synthetic Configure event.  These events are not relevant
	 * to us, and if we process them confusion may result (e.g. we
	 * may conclude erroneously that the user repositioned or resized
	 * the window).
	 */

	if (!(winPtr->wmInfoPtr->flags & WM_NEVER_MAPPED)) {
	    ConfigureEvent(winPtr, &eventPtr->xconfigure);
	}
    } else if (eventPtr->type == MapNotify) {
	winPtr->flags |= TK_MAPPED;
	if (wmTracing) {
	    printf("TopLevelEventProc: %s mapped\n", winPtr->pathName);
	}
    } else if (eventPtr->type == UnmapNotify) {
	winPtr->flags &= ~TK_MAPPED;
	if (wmTracing) {
	    printf("TopLevelEventProc: %s unmapped\n", winPtr->pathName);
	}
    } else if (eventPtr->type == ReparentNotify) {
	ReparentEvent(winPtr, &eventPtr->xreparent);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TopLevelReqProc --
 *
 *	This procedure is invoked by the geometry manager whenever
 *	the requested size for a top-level window is changed.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Arrange for the window to be resized to satisfy the request
 *	(this happens as a when-idle action).
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static void
TopLevelReqProc(dummy, tkwin)
    ClientData dummy;			/* Not used. */
    Tk_Window tkwin;			/* Information about window. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    WmInfo *wmPtr;

    wmPtr = winPtr->wmInfoPtr;
    wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, (ClientData) winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }

    /*
     * If the window isn't being positioned by its upper left corner
     * then we have to move it as well.
     */

    if (wmPtr->flags & (WM_NEGATIVE_X | WM_NEGATIVE_Y)) {
	wmPtr->flags |= WM_MOVE_PENDING;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateGeometryInfo --
 *
 *	This procedure is invoked when a top-level window is first
 *	mapped, and also as a when-idle procedure, to bring the
 *	geometry and/or position of a top-level window back into
 *	line with what has been requested by the user and/or widgets.
 *	This procedure doesn't return until the window manager has
 *	responded to the geometry change.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window's size and location may change, unless the WM prevents
 *	that from happening.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateGeometryInfo(clientData)
    ClientData clientData;		/* Pointer to the window's record. */
{
    register TkWindow *winPtr = (TkWindow *) clientData;
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int x, y, width, height;
    unsigned long serial;

    wmPtr->flags &= ~WM_UPDATE_PENDING;

    /*
     * Compute the new size for the top-level window.  See the
     * user documentation for details on this, but the size
     * requested depends on (a) the size requested internally
     * by the window's widgets, (b) the size requested by the
     * user in a "wm geometry" command or via wm-based interactive
     * resizing (if any), and (c) whether or not the window is
     * gridded.  Don't permit sizes <= 0 because this upsets
     * the X server.
     */

    if (wmPtr->width == -1) {
	width = winPtr->reqWidth;
    } else if (wmPtr->gridWin != NULL) {
	width = winPtr->reqWidth
		+ (wmPtr->width - wmPtr->reqGridWidth)*wmPtr->widthInc;
    } else {
	width = wmPtr->width;
    }
    if (width <= 0) {
	width = 1;
    }
    if (wmPtr->height == -1) {
	height = winPtr->reqHeight;
    } else if (wmPtr->gridWin != NULL) {
	height = winPtr->reqHeight
		+ (wmPtr->height - wmPtr->reqGridHeight)*wmPtr->heightInc;
    } else {
	height = wmPtr->height;
    }
    if (height <= 0) {
	height = 1;
    }

    /*
     * Compute the new position for the upper-left pixel of the window's
     * decorative frame.  This is tricky, because we need to include the
     * border widths supplied by a reparented parent in this calculation,
     * but can't use the parent's current overall size since that may
     * change as a result of this code.
     */

    if (wmPtr->flags & WM_NEGATIVE_X) {
	x = wmPtr->vRootWidth - wmPtr->x
		- (width + (wmPtr->parentWidth - winPtr->changes.width));
    } else {
	x =  wmPtr->x;
    }
    if (wmPtr->flags & WM_NEGATIVE_Y) {
	y = wmPtr->vRootHeight - wmPtr->y
		- (height + (wmPtr->parentHeight - winPtr->changes.height));
    } else {
	y =  wmPtr->y;
    }

    /*
     * If the window's size is going to change and the window is
     * supposed to not be resizable by the user, then we have to
     * update the size hints.  There may also be a size-hint-update
     * request pending from somewhere else, too.
     */

    if (((width != winPtr->changes.width)
	    || (height != winPtr->changes.height))
	    && (wmPtr->gridWin == NULL)
	    && ((wmPtr->sizeHintsFlags & (PMinSize|PMaxSize)) == 0)) {
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    }
    if (wmPtr->flags & WM_UPDATE_SIZE_HINTS) {
	UpdateSizeHints(winPtr);
    }

    /*
     * Reconfigure the window if it isn't already configured correctly.
     * A few tricky points:
     *
     * 1. Sometimes the window manager will give us a different size
     *    than we asked for (e.g. mwm has a minimum size for windows), so
     *    base the size check on what we *asked for* last time, not what we
     *    got.
     * 2. Can't just reconfigure always, because we may not get a
     *    ConfigureNotify event back if nothing changed, so
     *    WaitForConfigureNotify will hang a long time.
     * 3. Don't move window unless a new position has been requested for
     *	  it.  This is because of "features" in some window managers (e.g.
     *    twm, as of 4/24/91) where they don't interpret coordinates
     *    according to ICCCM.  Moving a window to its current location may
     *    cause it to shift position on the screen.
     */

    serial = NextRequest(winPtr->display);
    if (wmPtr->flags & WM_MOVE_PENDING) {
	wmPtr->configWidth = width;
	wmPtr->configHeight = height;
	if (wmTracing) {
	    printf("UpdateGeometryInfo moving to %d %d, resizing to %d x %d,\n",
		    x, y, width, height);
	}
	Tk_MoveResizeWindow((Tk_Window) winPtr, x, y, width, height);
    } else if ((width != wmPtr->configWidth)
	    || (height != wmPtr->configHeight)) {
	wmPtr->configWidth = width;
	wmPtr->configHeight = height;
	if (wmTracing) {
	    printf("UpdateGeometryInfo resizing to %d x %d\n", width, height);
	}
	Tk_ResizeWindow((Tk_Window) winPtr, width, height);
    } else {
	return;
    }

    /*
     * Wait for the configure operation to complete.  Don't need to do
     * this, however, if the window is about to be mapped:  it will be
     * taken care of elsewhere.
     */

    if (!(wmPtr->flags & WM_ABOUT_TO_MAP)) {
	WaitForConfigureNotify(winPtr, serial);
    }
}

/*
 *--------------------------------------------------------------
 *
 * UpdateSizeHints --
 *
 *	This procedure is called to update the window manager's
 *	size hints information from the information in a WmInfo
 *	structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Properties get changed for winPtr.
 *
 *--------------------------------------------------------------
 */

static void
UpdateSizeHints(winPtr)
    TkWindow *winPtr;
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    XSizeHints *hintsPtr;
    int maxWidth, maxHeight;

    wmPtr->flags &= ~WM_UPDATE_SIZE_HINTS;

    hintsPtr = XAllocSizeHints();
    if (hintsPtr == NULL) {
	return;
    }

    /*
     * Compute the pixel-based sizes for the various fields in the
     * size hints structure, based on the grid-based sizes in
     * our structure.
     */

    GetMaxSize(wmPtr, &maxWidth, &maxHeight);
    if (wmPtr->gridWin != NULL) {
	hintsPtr->base_width = winPtr->reqWidth
		- (wmPtr->reqGridWidth * wmPtr->widthInc);
	if (hintsPtr->base_width < 0) {
	    hintsPtr->base_width = 0;
	}
	hintsPtr->base_height = winPtr->reqHeight
		- (wmPtr->reqGridHeight * wmPtr->heightInc);
	if (hintsPtr->base_height < 0) {
	    hintsPtr->base_height = 0;
	}
	hintsPtr->min_width = hintsPtr->base_width
		+ (wmPtr->minWidth * wmPtr->widthInc);
	hintsPtr->min_height = hintsPtr->base_height
		+ (wmPtr->minHeight * wmPtr->heightInc);
	hintsPtr->max_width = hintsPtr->base_width
		+ (maxWidth * wmPtr->widthInc);
	hintsPtr->max_height = hintsPtr->base_height
		+ (maxHeight * wmPtr->heightInc);
    } else {
	hintsPtr->min_width = wmPtr->minWidth;
	hintsPtr->min_height = wmPtr->minHeight;
	hintsPtr->max_width = maxWidth;
	hintsPtr->max_height = maxHeight;
	hintsPtr->base_width = 0;
	hintsPtr->base_height = 0;
    }
    hintsPtr->width_inc = wmPtr->widthInc;
    hintsPtr->height_inc = wmPtr->heightInc;
    hintsPtr->min_aspect.x = wmPtr->minAspect.x;
    hintsPtr->min_aspect.y = wmPtr->minAspect.y;
    hintsPtr->max_aspect.x = wmPtr->maxAspect.x;
    hintsPtr->max_aspect.y = wmPtr->maxAspect.y;
    hintsPtr->win_gravity = wmPtr->gravity;
    hintsPtr->flags = wmPtr->sizeHintsFlags | PMinSize | PMaxSize;

    /*
     * If the window isn't supposed to be resizable, then set the
     * minimum and maximum dimensions to be the same.
     */

    if (wmPtr->flags & WM_WIDTH_NOT_RESIZABLE) {
	if (wmPtr->width >= 0) {
	    hintsPtr->min_width = wmPtr->width;
	} else {
	    hintsPtr->min_width = winPtr->reqWidth;
	}
	hintsPtr->max_width = hintsPtr->min_width;
    }
    if (wmPtr->flags & WM_HEIGHT_NOT_RESIZABLE) {
	if (wmPtr->height >= 0) {
	    hintsPtr->min_height = wmPtr->height;
	} else {
	    hintsPtr->min_height = winPtr->reqHeight;
	}
	hintsPtr->max_height = hintsPtr->min_height;
    }

    XSetWMNormalHints(winPtr->display, winPtr->window, hintsPtr);

    XFree((char *) hintsPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * WaitForConfigureNotify --
 *
 *	This procedure is invoked in order to synchronize with the
 *	window manager.  It waits for a ConfigureNotify event to
 *	arrive, signalling that the window manager has seen an attempt
 *	on our part to move or resize a top-level window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Delays the execution of the process until a ConfigureNotify event
 *	arrives with serial number at least as great as serial.  This
 *	is useful for two reasons:
 *
 *	1. It's important to distinguish ConfigureNotify events that are
 *	   coming in response to a request we've made from those generated
 *	   spontaneously by the user.  The reason for this is that if the
 *	   user resizes the window we take that as an order to ignore
 *	   geometry requests coming from inside the window hierarchy.  If
 *	   we accidentally interpret a response to our request as a
 *	   user-initiated action, the window will stop responding to
 *	   new geometry requests.  To make this distinction, (a) this
 *	   procedure sets a flag for TopLevelEventProc to indicate that
 *	   we're waiting to sync with the wm, and (b) all changes to
 *	   the size of a top-level window are followed by calls to this
 *	   procedure.
 *	2. Races and confusion can come about if there are multiple
 *	   operations outstanding at a time (e.g. two different resizes
 *	   of the top-level window:  it's hard to tell which of the
 *	   ConfigureNotify events coming back is for which request).
 *	While waiting, all events covered by StructureNotifyMask are
 *	processed and all others are deferred.
 *
 *----------------------------------------------------------------------
 */

static void
WaitForConfigureNotify(winPtr, serial)
    TkWindow *winPtr;		/* Top-level window for which we want
				 * to see a ConfigureNotify. */
    unsigned long serial;	/* Serial number of resize request.  Want to
				 * be sure wm has seen this. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    XEvent event;
    int diff, code;
    int gotConfig = 0;

    /*
     * One more tricky detail about this procedure.  In some cases the
     * window manager will decide to ignore a configure request (e.g.
     * because it thinks the window is already in the right place).
     * To avoid hanging in this situation, only wait for a few seconds,
     * then give up.
     */

    while (!gotConfig) {
	wmPtr->flags |= WM_SYNC_PENDING;
	code = WaitForEvent(winPtr->display, winPtr->window, ConfigureNotify,
		&event);
	wmPtr->flags &= ~WM_SYNC_PENDING;
	if (code != TCL_OK) {
	    if (wmTracing) {
		printf("WaitForConfigureNotify giving up on %s\n",
			winPtr->pathName);
	    }
	    break;
	}
	diff = event.xconfigure.serial - serial;
	if (diff >= 0) {
	    gotConfig = 1;
	}
    }
    wmPtr->flags &= ~WM_MOVE_PENDING;
    if (wmTracing) {
	printf("WaitForConfigureNotify finished with %s, serial %ld\n",
		winPtr->pathName, serial);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * WaitForEvent --
 *
 *	This procedure is used by WaitForConfigureNotify and
 *	WaitForMapNotify to wait for an event of a certain type
 *	to arrive.
 *
 * Results:
 *	Under normal conditions, TCL_OK is returned and an event for
 *	display and window that matches "mask" is stored in *eventPtr.
 *	This event  has already been processed by Tk before this procedure
 *	returns.  If a long time goes by with no event of the right type
 *	arriving, or if an error occurs while waiting for the event to
 *	arrive, then TCL_ERROR is returned.
 *
 * Side effects:
 *	While waiting for the desired event to occur, Configurenotify
 *	events for window are processed, as are all ReparentNotify events,
 *
 *----------------------------------------------------------------------
 */

static int
WaitForEvent(display, window, type, eventPtr)
    Display *display;		/* Display event is coming from. */
    Window window;		/* Window for which event is desired. */
    int type;			/* Type of event that is wanted. */
    XEvent *eventPtr;		/* Place to store event. */
{
#define TIMEOUT_MS 2000
    WaitRestrictInfo info;
    Tk_RestrictProc *oldRestrictProc;
    ClientData oldRestrictData;

    /*
     * Set up an event filter to select just the events we want, and
     * a timer handler, then wait for events until we get the event
     * we want or a timeout happens.
     */

    info.display = display;
    info.window = window;
    info.type = type;
    info.eventPtr = eventPtr;
    info.foundEvent = 0;
    info.timeout = 0;
    oldRestrictProc = Tk_RestrictEvents(WaitRestrictProc, (ClientData) &info,
	    &oldRestrictData);
    Tcl_CreateModalTimeout(TIMEOUT_MS, WaitTimeoutProc,
	    (ClientData) &info);
    while (1) {
	Tcl_DoOneEvent(TCL_WINDOW_EVENTS);
	if (info.foundEvent) {
	    break;
	}
	if (info.timeout) {
	    break;
	}
    }
    Tcl_DeleteModalTimeout(WaitTimeoutProc, (ClientData) &info);
    (void) Tk_RestrictEvents(oldRestrictProc, oldRestrictData,
	    &oldRestrictData);
    if (info.foundEvent) {
	return TCL_OK;
    }
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * WaitRestrictProc --
 *
 *	This procedure is a Tk_RestrictProc that is used to filter
 *	events while WaitForEvent is active.
 *
 * Results:
 *	Returns TK_PROCESS_EVENT if the right event is found.  Also
 *	returns TK_PROCESS_EVENT if any ReparentNotify event is found
 *	for window or if the event is a ConfigureNotify for window.
 *	Otherwise returns TK_DEFER_EVENT.
 *
 * Side effects:
 *	An event may get stored in the area indicated by the caller
 *	of WaitForEvent.
 *
 *----------------------------------------------------------------------
 */

static Tk_RestrictAction
WaitRestrictProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to WaitRestrictInfo structure. */
    XEvent *eventPtr;		/* Event that is about to be handled. */
{
    WaitRestrictInfo *infoPtr = (WaitRestrictInfo *) clientData;

    if (eventPtr->type == ReparentNotify) {
	return TK_PROCESS_EVENT;
    }
    if ((eventPtr->xany.window != infoPtr->window)
	    || (eventPtr->xany.display != infoPtr->display)) {
	return TK_DEFER_EVENT;
    }
    if (eventPtr->type == infoPtr->type) {
	*infoPtr->eventPtr = *eventPtr;
	infoPtr->foundEvent = 1;
	return TK_PROCESS_EVENT;
    }
    if (eventPtr->type == ConfigureNotify) {
	return TK_PROCESS_EVENT;
    }
    return TK_DEFER_EVENT;
}

/*
 *----------------------------------------------------------------------
 *
 * WaitTimeoutProc --
 *
 *	This procedure is invoked as a timer handler when too much
 *	time elapses during a call to WaitForEvent.  It sets a flag
 *	in a structure shared with WaitForEvent so that WaitForEvent
 *	knows that it should return.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The timeout field gest set in the WaitRestrictInfo structure.
 *
 *----------------------------------------------------------------------
 */

static void
WaitTimeoutProc(clientData)
    ClientData clientData;	/* Pointer to WaitRestrictInfo structure. */
{
    WaitRestrictInfo *infoPtr = (WaitRestrictInfo *) clientData;

    infoPtr->timeout = 1;
}

/*
 *----------------------------------------------------------------------
 *
 * WaitForMapNotify --
 *
 *	This procedure is invoked in order to synchronize with the
 *	window manager.  It waits for the window's mapped state to
 *	reach the value given by mapped.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Delays the execution of the process until winPtr becomes mapped
 *	or unmapped, depending on the "mapped" argument.  This allows us
 *	to synchronize with the window manager, and allows us to
 *	identify changes in window size that come about when the window
 *	manager first starts managing the window (as opposed to those
 *	requested interactively by the user later).  See the comments
 *	for WaitForConfigureNotify and WM_SYNC_PENDING.  While waiting,
 *	all events covered by StructureNotifyMask are processed and all
 *	others are deferred.
 *
 *----------------------------------------------------------------------
 */

static void
WaitForMapNotify(winPtr, mapped)
    TkWindow *winPtr;		/* Top-level window for which we want
				 * to see a particular mapping state. */
    int mapped;			/* If non-zero, wait for window to become
				 * mapped, otherwise wait for it to become
				 * unmapped. */
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;
    XEvent event;
    int code;

    while (1) {
	if (mapped) {
	    if (winPtr->flags & TK_MAPPED) {
		break;
	    }
	} else if (!(winPtr->flags & TK_MAPPED)) {
	    break;
	}
	wmPtr->flags |= WM_SYNC_PENDING;
	code = WaitForEvent(winPtr->display, winPtr->window,
		mapped ? MapNotify : UnmapNotify, &event);
	wmPtr->flags &= ~WM_SYNC_PENDING;
	if (code != TCL_OK) {
	    /*
	     * There are some bizarre situations in which the window
	     * manager can't respond or chooses not to (e.g. if we've
	     * got a grab set it can't respond).  If this happens then
	     * just quit.
	     */

	    if (wmTracing) {
		printf("WaitForMapNotify giving up on %s\n", winPtr->pathName);
	    }
	    break;
	}
    }
    wmPtr->flags &= ~WM_MOVE_PENDING;
    if (wmTracing) {
	printf("WaitForMapNotify finished with %s\n", winPtr->pathName);
    }
}

/*
 *--------------------------------------------------------------
 *
 * UpdateHints --
 *
 *	This procedure is called to update the window manager's
 *	hints information from the information in a WmInfo
 *	structure.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Properties get changed for winPtr.
 *
 *--------------------------------------------------------------
 */

static void
UpdateHints(winPtr)
    TkWindow *winPtr;
{
    WmInfo *wmPtr = winPtr->wmInfoPtr;

    if (wmPtr->flags & WM_NEVER_MAPPED) {
	return;
    }
    XSetWMHints(winPtr->display, winPtr->window, &wmPtr->hints);
}

/*
 *--------------------------------------------------------------
 *
 * ParseGeometry --
 *
 *	This procedure parses a geometry string and updates
 *	information used to control the geometry of a top-level
 *	window.
 *
 * Results:
 *	A standard Tcl return value, plus an error message in
 *	interp->result if an error occurs.
 *
 * Side effects:
 *	The size and/or location of winPtr may change.
 *
 *--------------------------------------------------------------
 */

static int
ParseGeometry(interp, string, winPtr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *string;		/* String containing new geometry.  Has the
				 * standard form "=wxh+x+y". */
    TkWindow *winPtr;		/* Pointer to top-level window whose
				 * geometry is to be changed. */
{
    register WmInfo *wmPtr = winPtr->wmInfoPtr;
    int x, y, width, height, flags;
    char *end;
    register char *p = string;

    /*
     * The leading "=" is optional.
     */

    if (*p == '=') {
	p++;
    }

    /*
     * Parse the width and height, if they are present.  Don't
     * actually update any of the fields of wmPtr until we've
     * successfully parsed the entire geometry string.
     */

    width = wmPtr->width;
    height = wmPtr->height;
    x = wmPtr->x;
    y = wmPtr->y;
    flags = wmPtr->flags;
    if (isdigit(UCHAR(*p))) {
	width = strtoul(p, &end, 10);
	p = end;
	if (*p != 'x') {
	    goto error;
	}
	p++;
	if (!isdigit(UCHAR(*p))) {
	    goto error;
	}
	height = strtoul(p, &end, 10);
	p = end;
    }

    /*
     * Parse the X and Y coordinates, if they are present.
     */

    if (*p != '\0') {
	flags &= ~(WM_NEGATIVE_X | WM_NEGATIVE_Y);
	if (*p == '-') {
	    flags |= WM_NEGATIVE_X;
	} else if (*p != '+') {
	    goto error;
	}
	x = strtol(p+1, &end, 10);
	p = end;
	if (*p == '-') {
	    flags |= WM_NEGATIVE_Y;
	} else if (*p != '+') {
	    goto error;
	}
	y = strtol(p+1, &end, 10);
	if (*end != '\0') {
	    goto error;
	}

	/*
	 * Assume that the geometry information came from the user,
	 * unless an explicit source has been specified.  Otherwise
	 * most window managers assume that the size hints were
	 * program-specified and they ignore them.
	 */

	if ((wmPtr->sizeHintsFlags & (USPosition|PPosition)) == 0) {
	    wmPtr->sizeHintsFlags |= USPosition;
	    flags |= WM_UPDATE_SIZE_HINTS;
	}
    }

    /*
     * Everything was parsed OK.  Update the fields of *wmPtr and
     * arrange for the appropriate information to be percolated out
     * to the window manager at the next idle moment.
     */

    wmPtr->width = width;
    wmPtr->height = height;
    wmPtr->x = x;
    wmPtr->y = y;
    flags |= WM_MOVE_PENDING;
    wmPtr->flags = flags;

    if (!(wmPtr->flags & (WM_UPDATE_PENDING|WM_NEVER_MAPPED))) {
	Tcl_DoWhenIdle(UpdateGeometryInfo, (ClientData) winPtr);
	wmPtr->flags |= WM_UPDATE_PENDING;
    }
    return TCL_OK;

    error:
    Tcl_AppendResult(interp, "bad geometry specifier \"",
	    string, "\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetRootCoords --
 *
 *	Given a token for a window, this procedure traces through the
 *	window's lineage to find the (virtual) root-window coordinates
 *	corresponding to point (0,0) in the window.
 *
 * Results:
 *	The locations pointed to by xPtr and yPtr are filled in with
 *	the root coordinates of the (0,0) point in tkwin.  If a virtual
 *	root window is in effect for the window, then the coordinates
 *	in the virtual root are returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
Tk_GetRootCoords(tkwin, xPtr, yPtr)
    Tk_Window tkwin;		/* Token for window. */
    int *xPtr;			/* Where to store x-displacement of (0,0). */
    int *yPtr;			/* Where to store y-displacement of (0,0). */
{
    int x, y;
    register TkWindow *winPtr = (TkWindow *) tkwin;

    /*
     * Search back through this window's parents all the way to a
     * top-level window, combining the offsets of each window within
     * its parent.
     */

    x = y = 0;
    while (1) {
	x += winPtr->changes.x + winPtr->changes.border_width;
	y += winPtr->changes.y + winPtr->changes.border_width;
	if ((winPtr->flags & TK_TOP_LEVEL) || (winPtr->parentPtr == NULL)) {
	    break;
	}
	winPtr = winPtr->parentPtr;
    }
    *xPtr = x;
    *yPtr = y;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CoordsToWindow --
 *
 *	Given the (virtual) root coordinates of a point, this procedure
 *	returns the token for the top-most window covering that point,
 *	if there exists such a window in this application.
 *
 * Results:
 *	The return result is either a token for the window corresponding
 *	to rootX and rootY, or else NULL to indicate that there is no such
 *	window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
Tk_CoordsToWindow(rootX, rootY, tkwin)
    int rootX, rootY;		/* Coordinates of point in root window.  If
				 * a virtual-root window manager is in use,
				 * these coordinates refer to the virtual
				 * root, not the real root. */
    Tk_Window tkwin;		/* Token for any window in application;
				 * used to identify the display. */
{
    Window rootChild, root, vRoot;
    int dummy1, dummy2;
    register WmInfo *wmPtr;
    register TkWindow *winPtr, *childPtr;
    TkWindow *nextPtr;		/* Coordinates of highest child found so
				 * far that contains point. */
    int x, y;			/* Coordinates in winPtr. */
    int tmpx, tmpy, bd;

    /*
     * Step 1: find any top-level window for the right screen.
     */

    while (!Tk_IsTopLevel(tkwin)) {
	tkwin = Tk_Parent(tkwin);
    }
    wmPtr = ((TkWindow *) tkwin)->wmInfoPtr;

    /*
     * Step 2: find the window in the actual root that contains the
     * desired point.  Special trick:  if a virtual root window manager
     * is in use, there may be windows in both the true root (e.g.
     * pop-up menus) and in the virtual root;  have to look in *both*
     * places.
     */

    UpdateVRootGeometry(wmPtr);
    root = RootWindowOfScreen(Tk_Screen(tkwin));
    if (XTranslateCoordinates(Tk_Display(tkwin), root, root,
	    rootX + wmPtr->vRootX, rootY + wmPtr->vRootY,
	    &dummy1, &dummy2, &rootChild) == False) {
	panic("Tk_CoordsToWindow get False return from XTranslateCoordinates");
    }

    /*
     * Step 3: if the window we've found so far (a child of the root)
     * is the virtual root window, then look again to find the child of
     * the virtual root.
     */

    vRoot = ((TkWindow *) tkwin)->wmInfoPtr->vRoot;
    if ((vRoot != None) && (rootChild == vRoot)) {
	if (XTranslateCoordinates(Tk_Display(tkwin), vRoot, vRoot, rootX,
		rootY, &dummy1, &dummy2, &rootChild) == False) {
	    panic("Tk_CoordsToWindow get False return from XTranslateCoordinates");
	}
    }
    for (wmPtr = firstWmPtr; ; wmPtr = wmPtr->nextPtr) {
	if (wmPtr == NULL) {
	    return NULL;
	}
	if ((wmPtr->reparent == rootChild) || ((wmPtr->reparent == None)
		&& (wmPtr->winPtr->window == rootChild))) {
	    break;
	}
    }
    winPtr = wmPtr->winPtr;
    if (winPtr->mainPtr != ((TkWindow *) tkwin)->mainPtr) {
	return NULL;
    }

    /*
     * Step 4: work down through the hierarchy underneath this window.
     * At each level, scan through all the children to find the highest
     * one in the stacking order that contains the point.  Then repeat
     * the whole process on that child.
     */

    x = rootX;
    y = rootY;
    while (1) {
	x -= winPtr->changes.x;
	y -= winPtr->changes.y;
	nextPtr = NULL;
	for (childPtr = winPtr->childList; childPtr != NULL;
		childPtr = childPtr->nextPtr) {
	    if (!Tk_IsMapped(childPtr) || (childPtr->flags & TK_TOP_LEVEL)) {
		continue;
	    }
	    tmpx = x - childPtr->changes.x;
	    tmpy = y - childPtr->changes.y;
	    bd = childPtr->changes.border_width;
	    if ((tmpx >= -bd) && (tmpy >= -bd)
		    && (tmpx < (childPtr->changes.width + bd))
		    && (tmpy < (childPtr->changes.height + bd))) {
		nextPtr = childPtr;
	    }
	}
	if (nextPtr == NULL) {
	    break;
	}
	winPtr = nextPtr;
    }
    return (Tk_Window) winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateVRootGeometry --
 *
 *	This procedure is called to update all the virtual root
 *	geometry information in wmPtr.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The vRootX, vRootY, vRootWidth, and vRootHeight fields in
 *	wmPtr are filled with the most up-to-date information.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateVRootGeometry(wmPtr)
    WmInfo *wmPtr;		/* Window manager information to be
				 * updated.  The wmPtr->vRoot field must
				 * be valid. */
{
    TkWindow *winPtr = wmPtr->winPtr;
    int bd;
    unsigned int dummy;
    Window dummy2;
    Status status;
    Tk_ErrorHandler handler;

    /*
     * If this isn't a virtual-root window manager, just return information
     * about the screen.
     */

    wmPtr->flags &= ~WM_VROOT_OFFSET_STALE;
    if (wmPtr->vRoot == None) {
	noVRoot:
	wmPtr->vRootX = wmPtr->vRootY = 0;
	wmPtr->vRootWidth = DisplayWidth(winPtr->display, winPtr->screenNum);
	wmPtr->vRootHeight = DisplayHeight(winPtr->display, winPtr->screenNum);
	return;
    }

    /*
     * Refresh the virtual root information if it's out of date.
     */

    handler = Tk_CreateErrorHandler(winPtr->display, -1, -1, -1,
	    (Tk_ErrorProc *) NULL, (ClientData) NULL);
    status = XGetGeometry(winPtr->display, wmPtr->vRoot,
	    &dummy2, &wmPtr->vRootX, &wmPtr->vRootY,
	    (unsigned int *) &wmPtr->vRootWidth,
	    (unsigned int *) &wmPtr->vRootHeight, (unsigned int *) &bd,
	    &dummy);
    if (wmTracing) {
	printf("UpdateVRootGeometry: x = %d, y = %d, width = %d, ",
		wmPtr->vRootX, wmPtr->vRootY, wmPtr->vRootWidth);
	printf("height = %d, status = %d\n", wmPtr->vRootHeight, status);
    }
    Tk_DeleteErrorHandler(handler);
    if (status == 0) {
	/*
	 * The virtual root is gone!  Pretend that it never existed.
	 */

	wmPtr->vRoot = None;
	goto noVRoot;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetVRootGeometry --
 *
 *	This procedure returns information about the virtual root
 *	window corresponding to a particular Tk window.
 *
 * Results:
 *	The values at xPtr, yPtr, widthPtr, and heightPtr are set
 *	with the offset and dimensions of the root window corresponding
 *	to tkwin.  If tkwin is being managed by a virtual root window
 *	manager these values correspond to the virtual root window being
 *	used for tkwin;  otherwise the offsets will be 0 and the
 *	dimensions will be those of the screen.
 *
 * Side effects:
 *	Vroot window information is refreshed if it is out of date.
 *
 *----------------------------------------------------------------------
 */

void
Tk_GetVRootGeometry(tkwin, xPtr, yPtr, widthPtr, heightPtr)
    Tk_Window tkwin;		/* Window whose virtual root is to be
				 * queried. */
    int *xPtr, *yPtr;		/* Store x and y offsets of virtual root
				 * here. */
    int *widthPtr, *heightPtr;	/* Store dimensions of virtual root here. */
{
    WmInfo *wmPtr;
    TkWindow *winPtr = (TkWindow *) tkwin;

    /*
     * Find the top-level window for tkwin, and locate the window manager
     * information for that window.
     */

    while (!(winPtr->flags & TK_TOP_LEVEL) && (winPtr->parentPtr != NULL)) {
	winPtr = winPtr->parentPtr;
    }
    wmPtr = winPtr->wmInfoPtr;

    /*
     * Make sure that the geometry information is up-to-date, then copy
     * it out to the caller.
     */

    if (wmPtr->flags & WM_VROOT_OFFSET_STALE) {
	UpdateVRootGeometry(wmPtr);
    }
    *xPtr = wmPtr->vRootX;
    *yPtr = wmPtr->vRootY;
    *widthPtr = wmPtr->vRootWidth;
    *heightPtr = wmPtr->vRootHeight;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MoveToplevelWindow --
 *
 *	This procedure is called instead of Tk_MoveWindow to adjust
 *	the x-y location of a top-level window.  It delays the actual
 *	move to a later time and keeps window-manager information
 *	up-to-date with the move
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is eventually moved so that its upper-left corner
 *	(actually, the upper-left corner of the window's decorative
 *	frame, if there is one) is at (x,y).
 *
 *----------------------------------------------------------------------
 */

void
Tk_MoveToplevelWindow(tkwin, x, y)
    Tk_Window tkwin;		/* Window to move. */
    int x, y;			/* New location for window (within
				 * parent). */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    register WmInfo *wmPtr = winPtr->wmInfoPtr;

    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	panic("Tk_MoveToplevelWindow called with non-toplevel window");
    }
    wmPtr->x = x;
    wmPtr->y = y;
    wmPtr->flags |= WM_MOVE_PENDING;
    wmPtr->flags &= ~(WM_NEGATIVE_X|WM_NEGATIVE_Y);
    if ((wmPtr->sizeHintsFlags & (USPosition|PPosition)) == 0) {
	wmPtr->sizeHintsFlags |= USPosition;
	wmPtr->flags |= WM_UPDATE_SIZE_HINTS;
    }

    /*
     * If the window has already been mapped, must bring its geometry
     * up-to-date immediately, otherwise an event might arrive from the
     * server that would overwrite wmPtr->x and wmPtr->y and lose the
     * new position.
     */

    if (!(wmPtr->flags & WM_NEVER_MAPPED)) {
	if (wmPtr->flags & WM_UPDATE_PENDING) {
	    Tcl_CancelIdleCall(UpdateGeometryInfo, (ClientData) winPtr);
	}
	UpdateGeometryInfo((ClientData) winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateWmProtocols --
 *
 *	This procedure transfers the most up-to-date information about
 *	window manager protocols from the WmInfo structure to the actual
 *	property on the top-level window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The WM_PROTOCOLS property gets changed for wmPtr's window.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateWmProtocols(wmPtr)
    register WmInfo *wmPtr;	/* Information about top-level window. */
{
    register ProtocolHandler *protPtr;
    Atom deleteWindowAtom;
    int count;
    Atom *arrayPtr, *atomPtr;

    /*
     * There are only two tricky parts here.  First, there could be any
     * number of atoms for the window, so count them and malloc an array
     * to hold all of their atoms.  Second, we *always* want to respond
     * to the WM_DELETE_WINDOW protocol, even if no-one's officially asked.
     */

    for (protPtr = wmPtr->protPtr, count = 1; protPtr != NULL;
	    protPtr = protPtr->nextPtr, count++) {
	/* Empty loop body;  we're just counting the handlers. */
    }
    arrayPtr = (Atom *) ckalloc((unsigned) (count * sizeof(Atom)));
    deleteWindowAtom = Tk_InternAtom((Tk_Window) wmPtr->winPtr,
	    "WM_DELETE_WINDOW");
    arrayPtr[0] = deleteWindowAtom;
    for (protPtr = wmPtr->protPtr, atomPtr = &arrayPtr[1];
	    protPtr != NULL; protPtr = protPtr->nextPtr) {
	if (protPtr->protocol != deleteWindowAtom) {
	    *atomPtr = protPtr->protocol;
	    atomPtr++;
	}
    }
    XChangeProperty(wmPtr->winPtr->display, wmPtr->winPtr->window,
	    Tk_InternAtom((Tk_Window) wmPtr->winPtr, "WM_PROTOCOLS"),
	    XA_ATOM, 32, PropModeReplace, (unsigned char *) arrayPtr,
	    atomPtr-arrayPtr);
    ckfree((char *) arrayPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmProtocolEventProc --
 *
 *	This procedure is called by the Tk_HandleEvent whenever a
 *	ClientMessage event arrives whose type is "WM_PROTOCOLS".
 *	This procedure handles the message from the window manager
 *	in an appropriate fashion.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what sort of handler, if any, was set up for the
 *	protocol.
 *
 *----------------------------------------------------------------------
 */

void
TkWmProtocolEventProc(winPtr, eventPtr)
    TkWindow *winPtr;		/* Window to which the event was sent. */
    XEvent *eventPtr;		/* X event. */
{
    WmInfo *wmPtr;
    register ProtocolHandler *protPtr;
    Atom protocol;
    int result;
    char *protocolName;
    Tcl_Interp *interp;

    wmPtr = winPtr->wmInfoPtr;
    if (wmPtr == NULL) {
	return;
    }
    protocol = (Atom) eventPtr->xclient.data.l[0];

    /*
     * Note: it's very important to retrieve the protocol name now,
     * before invoking the command, even though the name won't be used
     * until after the command returns.  This is because the command
     * could delete winPtr, making it impossible for us to use it
     * later in the call to Tk_GetAtomName.
     */

    protocolName = Tk_GetAtomName((Tk_Window) winPtr, protocol);
    for (protPtr = wmPtr->protPtr; protPtr != NULL;
	    protPtr = protPtr->nextPtr) {
	if (protocol == protPtr->protocol) {
	    Tcl_Preserve((ClientData) protPtr);
            interp = protPtr->interp;
            Tcl_Preserve((ClientData) interp);
	    result = Tcl_GlobalEval(interp, protPtr->command);
	    if (result != TCL_OK) {
		Tcl_AddErrorInfo(interp, "\n    (command for \"");
		Tcl_AddErrorInfo(interp, protocolName);
		Tcl_AddErrorInfo(interp,
			"\" window manager protocol)");
		Tcl_BackgroundError(interp);
	    }
            Tcl_Release((ClientData) interp);
	    Tcl_Release((ClientData) protPtr);
	    return;
	}
    }

    /*
     * No handler was present for this protocol.  If this is a
     * WM_DELETE_WINDOW message then just destroy the window.
     */

    if (protocol == Tk_InternAtom((Tk_Window) winPtr, "WM_DELETE_WINDOW")) {
	Tk_DestroyWindow((Tk_Window) winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmRestackToplevel --
 *
 *	This procedure restacks a top-level window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	WinPtr gets restacked  as specified by aboveBelow and otherPtr.
 *	This procedure doesn't return until the restack has taken
 *	effect and the ConfigureNotify event for it has been received.
 *
 *----------------------------------------------------------------------
 */

void
TkWmRestackToplevel(winPtr, aboveBelow, otherPtr)
    TkWindow *winPtr;		/* Window to restack. */
    int aboveBelow;		/* Gives relative position for restacking;
				 * must be Above or Below. */
    TkWindow *otherPtr;		/* Window relative to which to restack;
				 * if NULL, then winPtr gets restacked
				 * above or below *all* siblings. */
{
    XWindowChanges changes;
    XWindowAttributes atts;
    unsigned int mask;
    Window window, dummy1, dummy2, vRoot;
    Window *children;
    unsigned int numChildren;
    int i;
    int desiredIndex = 0;	/* Initialized to stop gcc warnings. */
    int ourIndex = 0;		/* Initialized to stop gcc warnings. */
    unsigned long serial;
    XEvent event;
    int diff;
    Tk_ErrorHandler handler;

    changes.stack_mode = aboveBelow;
    changes.sibling = None;
    mask = CWStackMode;
    if (winPtr->window == None) {
	Tk_MakeWindowExist((Tk_Window) winPtr);
    }
    if (winPtr->wmInfoPtr->flags & WM_NEVER_MAPPED) {
	/*
	 * Can't set stacking order properly until the window is on the
	 * screen (mapping it may give it a reparent window), so make sure
	 * it's on the screen.
	 */

	TkWmMapWindow(winPtr);
    }
    window = (winPtr->wmInfoPtr->reparent != None)
	    ? winPtr->wmInfoPtr->reparent : winPtr->window;
    if (otherPtr != NULL) {
	if (otherPtr->window == None) {
	    Tk_MakeWindowExist((Tk_Window) otherPtr);
	}
	if (otherPtr->wmInfoPtr->flags & WM_NEVER_MAPPED) {
	    TkWmMapWindow(otherPtr);
	}
	changes.sibling = (otherPtr->wmInfoPtr->reparent != None)
		? otherPtr->wmInfoPtr->reparent : otherPtr->window;
	mask = CWStackMode|CWSibling;
    }

    /*
     * Before actually reconfiguring the window, see if it's already
     * in the right place.  If so then don't reconfigure it.  The
     * reason for this extra work is that some window managers will
     * ignore the reconfigure request if the window is already in
     * the right place, causing a long delay in WaitForConfigureNotify
     * while it times out.  Special note: if the window is almost in
     * the right place, and the only windows between it and the right
     * place aren't mapped, then we don't reconfigure it either, for
     * the same reason.
     */

    vRoot = winPtr->wmInfoPtr->vRoot;
    if (vRoot == None) {
	vRoot = RootWindowOfScreen(Tk_Screen((Tk_Window) winPtr));
    }
    if (XQueryTree(winPtr->display, vRoot, &dummy1, &dummy2,
	    &children, &numChildren) != 0) {
	/*
	 * Find where our window is in the stacking order, and
	 * compute the desired location in the stacking order.
	 */

	for (i = 0; i < numChildren; i++) {
	    if (children[i] == window) {
		ourIndex = i;
	    }
	    if (children[i] == changes.sibling) {
		desiredIndex = i;
	    }
	}
	if (mask & CWSibling) {
	    if (aboveBelow == Above) {
		if (desiredIndex < ourIndex) {
		    desiredIndex += 1;
		}
	    } else {
		if (desiredIndex > ourIndex) {
		    desiredIndex -= 1;
		}
	    }
	} else {
	    if (aboveBelow == Above) {
		desiredIndex = numChildren-1;
	    } else {
		desiredIndex = 0;
	    }
	}

	/*
	 * See if there are any mapped windows between where we are
	 * and where we want to be.
	 */

	handler = Tk_CreateErrorHandler(winPtr->display, -1, -1, -1,
		(Tk_ErrorProc *) NULL, (ClientData) NULL);
	while (desiredIndex != ourIndex) {
	    if ((XGetWindowAttributes(winPtr->display, children[desiredIndex],
		    &atts) != 0) && (atts.map_state != IsUnmapped)) {
		break;
	    }
	    if (desiredIndex < ourIndex) {
		desiredIndex++;
	    } else {
		desiredIndex--;
	    }
	}
	Tk_DeleteErrorHandler(handler);
	XFree((char *) children);
	if (ourIndex == desiredIndex) {
	    return;
	}
    }

    /*
     * Reconfigure the window.  This tricky because of two things:
     * (a) Some window managers, like olvwm, insist that we raise
     *     or lower the toplevel window itself, as opposed to its
     *     decorative frame.  Attempts to raise or lower the frame
     *     are ignored.
     * (b) If the raise or lower is relative to a sibling, X will
     *     generate an error unless we work with the frames (the
     *     toplevels themselves aren't siblings).
     * Fortunately, the procedure XReconfigureWMWindow is supposed
     * to handle all of this stuff, so be careful to use it instead
     * of XConfigureWindow.
     */

    serial = NextRequest(winPtr->display);
    if (window != winPtr->window) {
	XSelectInput(winPtr->display, window, StructureNotifyMask);
    }
    XReconfigureWMWindow(winPtr->display, winPtr->window,
	    Tk_ScreenNumber((Tk_Window) winPtr), mask,  &changes);

    /*
     * Wait for the reconfiguration to complete.  If we don't wait, then
     * the window may not restack for a while and the application might
     * observe it before it has restacked.  Waiting for the reconfiguration
     * is tricky if winPtr has been reparented, since the window getting
     * the event isn't one that Tk owns.
     */

    if (window == winPtr->window) {
	WaitForConfigureNotify(winPtr, serial);
    } else {
	while (1) {
	    if (WaitForEvent(winPtr->display, window, ConfigureNotify,
		    &event) != TCL_OK) {
		break;
	    }
	    diff = event.xconfigure.serial - serial;
	    if (diff >= 0) {
		break;
	    }
	}
	XSelectInput(winPtr->display, window, (long) 0);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmAddToColormapWindows --
 *
 *	This procedure is called to add a given window to the
 *	WM_COLORMAP_WINDOWS property for its top-level, if it
 *	isn't already there.  It is invoked by the Tk code that
 *	creates a new colormap, in order to make sure that colormap
 *	information is propagated to the window manager by default.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	WinPtr's window gets added to the WM_COLORMAP_WINDOWS
 *	property of its nearest top-level ancestor, unless the
 *	colormaps have been set explicitly with the
 *	"wm colormapwindows" command.
 *
 *----------------------------------------------------------------------
 */

void
TkWmAddToColormapWindows(winPtr)
    TkWindow *winPtr;		/* Window with a non-default colormap.
				 * Should not be a top-level window. */
{
    TkWindow *topPtr;
    Window *oldPtr, *newPtr;
    int count, i;

    if (winPtr->window == None) {
	return;
    }

    for (topPtr = winPtr->parentPtr; ; topPtr = topPtr->parentPtr) {
	if (topPtr == NULL) {
	    /*
	     * Window is being deleted.  Skip the whole operation.
	     */

	    return;
	}
	if (topPtr->flags & TK_TOP_LEVEL) {
	    break;
	}
    }
    if (topPtr->wmInfoPtr->flags & WM_COLORMAPS_EXPLICIT) {
	return;
    }

    /*
     * Fetch the old value of the property.
     */

    if (XGetWMColormapWindows(topPtr->display, topPtr->window,
	    &oldPtr, &count) == 0) {
	oldPtr = NULL;
	count = 0;
    }

    /*
     * Make sure that the window isn't already in the list.
     */

    for (i = 0; i < count; i++) {
	if (oldPtr[i] == winPtr->window) {
	    return;
	}
    }

    /*
     * Make a new bigger array and use it to reset the property.
     * Automatically add the toplevel itself as the last element
     * of the list.
     */

    newPtr = (Window *) ckalloc((unsigned) ((count+2)*sizeof(Window)));
    for (i = 0; i < count; i++) {
	newPtr[i] = oldPtr[i];
    }
    if (count == 0) {
	count++;
    }
    newPtr[count-1] = winPtr->window;
    newPtr[count] = topPtr->window;
    XSetWMColormapWindows(topPtr->display, topPtr->window, newPtr, count+1);
    ckfree((char *) newPtr);
    if (oldPtr != NULL) {
	XFree((char *) oldPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkWmRemoveFromColormapWindows --
 *
 *	This procedure is called to remove a given window from the
 *	WM_COLORMAP_WINDOWS property for its top-level.  It is invoked
 *	when windows are deleted.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	WinPtr's window gets removed from the WM_COLORMAP_WINDOWS
 *	property of its nearest top-level ancestor, unless the
 *	top-level itself is being deleted too.
 *
 *----------------------------------------------------------------------
 */

void
TkWmRemoveFromColormapWindows(winPtr)
    TkWindow *winPtr;		/* Window that may be present in
				 * WM_COLORMAP_WINDOWS property for its
				 * top-level.  Should not be a top-level
				 * window. */
{
    TkWindow *topPtr;
    Window *oldPtr;
    int count, i, j;

    for (topPtr = winPtr->parentPtr; ; topPtr = topPtr->parentPtr) {
	if (topPtr == NULL) {
	    /*
	     * Ancestors have been deleted, so skip the whole operation.
	     * Seems like this can't ever happen?
	     */

	    return;
	}
	if (topPtr->flags & TK_TOP_LEVEL) {
	    break;
	}
    }
    if (topPtr->flags & TK_ALREADY_DEAD) {
	/*
	 * Top-level is being deleted, so there's no need to cleanup
	 * the WM_COLORMAP_WINDOWS property.
	 */

	return;
    }

    /*
     * Fetch the old value of the property.
     */

    if (XGetWMColormapWindows(topPtr->display, topPtr->window,
	    &oldPtr, &count) == 0) {
	return;
    }

    /*
     * Find the window and slide the following ones down to cover
     * it up.
     */

    for (i = 0; i < count; i++) {
	if (oldPtr[i] == winPtr->window) {
	    for (j = i ; j < count-1; j++) {
		oldPtr[j] = oldPtr[j+1];
	    }
	    XSetWMColormapWindows(topPtr->display, topPtr->window,
		    oldPtr, count-1);
	    break;
	}
    }
    XFree((char *) oldPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetPointerCoords --
 *
 *	Fetch the position of the mouse pointer.
 *
 * Results:
 *	*xPtr and *yPtr are filled in with the (virtual) root coordinates
 *	of the mouse pointer for tkwin's display.  If the pointer isn't
 *	on tkwin's screen, then -1 values are returned for both
 *	coordinates.  The argument tkwin must be a toplevel window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TkGetPointerCoords(tkwin, xPtr, yPtr)
    Tk_Window tkwin;		/* Toplevel window that identifies screen
				 * on which lookup is to be done. */
    int *xPtr, *yPtr;		/* Store pointer coordinates here. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    WmInfo *wmPtr;
    Window w, root, child;
    int rootX, rootY;
    unsigned int mask;

    wmPtr = winPtr->wmInfoPtr;

    w = wmPtr->vRoot;
    if (w == None) {
	w = RootWindow(winPtr->display, winPtr->screenNum);
    }
    if (XQueryPointer(winPtr->display, w, &root, &child, &rootX, &rootY,
	    xPtr, yPtr, &mask) != True) {
	*xPtr = -1;
	*yPtr = -1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TkMakeWindow --
 *
 *	Create an actual window system window object based on the
 *	current attributes of the specified TkWindow.
 *
 * Results:
 *	Returns the handle to the new window, or None on failure.
 *
 * Side effects:
 *	Creates a new X window.
 *
 *----------------------------------------------------------------------
 */

Window
TkMakeWindow(winPtr, parent)
    TkWindow *winPtr;
    Window parent;
{
    return XCreateWindow(winPtr->display, parent, winPtr->changes.x,
	    winPtr->changes.y, (unsigned) winPtr->changes.width,
	    (unsigned) winPtr->changes.height,
	    (unsigned) winPtr->changes.border_width, winPtr->depth,
	    InputOutput, winPtr->visual, winPtr->dirtyAtts,
	    &winPtr->atts);
}

/*
 *----------------------------------------------------------------------
 *
 * GetMaxSize --
 *
 *	This procedure computes the current maxWidth and maxHeight
 *	values for a window, taking into account the possibility
 *	that they may be defaulted.
 *
 * Results:
 *	The values at *maxWidthPtr and *maxHeightPtr are filled
 *	in with the maximum allowable dimensions of wmPtr's window,
 *	in grid units.  If no maximum has been specified for the
 *	window, then this procedure computes the largest sizes that
 *	will fit on the screen.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
GetMaxSize(wmPtr, maxWidthPtr, maxHeightPtr)
    WmInfo *wmPtr;		/* Window manager information for the
				 * window. */
    int *maxWidthPtr;		/* Where to store the current maximum
				 * width of the window. */
    int *maxHeightPtr;		/* Where to store the current maximum
				 * height of the window. */
{
    int tmp;

    if (wmPtr->maxWidth > 0) {
	*maxWidthPtr = wmPtr->maxWidth;
    } else {
	/*
	 * Must compute a default width.  Fill up the display, leaving a
	 * bit of extra space for the window manager's borders.
	 */

	tmp = DisplayWidth(wmPtr->winPtr->display, wmPtr->winPtr->screenNum)
	    - 15;
	if (wmPtr->gridWin != NULL) {
	    /*
	     * Gridding is turned on;  convert from pixels to grid units.
	     */

	    tmp = wmPtr->reqGridWidth
		    + (tmp - wmPtr->winPtr->reqWidth)/wmPtr->widthInc;
	}
	*maxWidthPtr = tmp;
    }
    if (wmPtr->maxHeight > 0) {
	*maxHeightPtr = wmPtr->maxHeight;
    } else {
	tmp = DisplayHeight(wmPtr->winPtr->display, wmPtr->winPtr->screenNum)
	    - 30;
	if (wmPtr->gridWin != NULL) {
	    tmp = wmPtr->reqGridHeight
		    + (tmp - wmPtr->winPtr->reqHeight)/wmPtr->heightInc;
	}
	*maxHeightPtr = tmp;
    }
}

/* 
 * tkWindow.c --
 *
 *	This file provides basic window-manipulation procedures,
 *	which are equivalent to procedures in Xlib (and even
 *	invoke them) but also maintain the local Tk_Window
 *	structure.
 *
 * Copyright (c) 1989-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkWindow.c 1.209 96/04/05 15:16:54
 */

#include "tkPort.h"
#include "tkInt.h"
#include "patchlevel.h"

/*
 * Count of number of main windows currently open in this process.
 */

static int numMainWindows;

/*
 * First in list of all main windows managed by this process.
 */

TkMainInfo *tkMainWindowList = NULL;

/*
 * List of all displays currently in use.
 */

TkDisplay *tkDisplayList = NULL;

/*
 * Have statics in this module been initialized?
 */

static int initialized = 0;

/*
 * The variables below hold several uid's that are used in many places
 * in the toolkit.
 */

Tk_Uid tkDisabledUid = NULL;
Tk_Uid tkActiveUid = NULL;
Tk_Uid tkNormalUid = NULL;

/*
 * Default values for "changes" and "atts" fields of TkWindows.  Note
 * that Tk always requests all events for all windows, except StructureNotify
 * events on internal windows:  these events are generated internally.
 */

static XWindowChanges defChanges = {
    0, 0, 1, 1, 0, 0, Above
};
#define ALL_EVENTS_MASK \
    KeyPressMask|KeyReleaseMask|ButtonPressMask|ButtonReleaseMask| \
    EnterWindowMask|LeaveWindowMask|PointerMotionMask|ExposureMask| \
    VisibilityChangeMask|FocusChangeMask|PropertyChangeMask|ColormapChangeMask
static XSetWindowAttributes defAtts= {
    None,			/* background_pixmap */
    0,				/* background_pixel */
    CopyFromParent,		/* border_pixmap */
    0,				/* border_pixel */
    NorthWestGravity,		/* bit_gravity */
    NorthWestGravity,		/* win_gravity */
    NotUseful,			/* backing_store */
    (unsigned) ~0,		/* backing_planes */
    0,				/* backing_pixel */
    False,			/* save_under */
    ALL_EVENTS_MASK,		/* event_mask */
    0,				/* do_not_propagate_mask */
    False,			/* override_redirect */
    CopyFromParent,		/* colormap */
    None			/* cursor */
};

/*
 * The following structure defines all of the commands supported by
 * Tk, and the C procedures that execute them.
 */

typedef struct {
    char *name;			/* Name of command. */
    int (*cmdProc) _ANSI_ARGS_((ClientData clientData, Tcl_Interp *interp,
	    int argc, char **argv));
				/* Command procedure. */
} TkCmd;

static TkCmd commands[] = {
    /*
     * Commands that are part of the intrinsics:
     */

    {"bell",		Tk_BellCmd},
    {"bind",		Tk_BindCmd},
    {"bindtags",	Tk_BindtagsCmd},
    {"clipboard",	Tk_ClipboardCmd},
    {"destroy",		Tk_DestroyCmd},
    {"focus",		Tk_FocusCmd},
    {"grab",		Tk_GrabCmd},
    {"grid",		Tk_GridCmd},
    {"image",		Tk_ImageCmd},
    {"lower",		Tk_LowerCmd},
    {"option",		Tk_OptionCmd},
    {"pack",		Tk_PackCmd},
    {"place",		Tk_PlaceCmd},
    {"raise",		Tk_RaiseCmd},
    {"selection",	Tk_SelectionCmd},
    {"tk",		Tk_TkCmd},
    {"tkwait",		Tk_TkwaitCmd},
    {"update",		Tk_UpdateCmd},
    {"winfo",		Tk_WinfoCmd},
    {"wm",		Tk_WmCmd},

    /*
     * Widget class commands.
     */
    {"button",		Tk_ButtonCmd},
    {"canvas",		Tk_CanvasCmd},
    {"checkbutton",	Tk_CheckbuttonCmd},
    {"entry",		Tk_EntryCmd},
    {"frame",		Tk_FrameCmd},
    {"label",		Tk_LabelCmd},
    {"listbox",		Tk_ListboxCmd},
    {"menu",		Tk_MenuCmd},
    {"menubutton",	Tk_MenubuttonCmd},
    {"message",		Tk_MessageCmd},
    {"radiobutton",	Tk_RadiobuttonCmd},
    {"scale",		Tk_ScaleCmd},
    {"scrollbar",	Tk_ScrollbarCmd},
    {"text",		Tk_TextCmd},
    {"toplevel",	Tk_ToplevelCmd},
    /*
     * Native widget class commands.
     */
#ifdef MAC_TCL
    {"macscrollbar",	Tk_MacScrollbarCmd},
#endif
    {(char *) NULL,	(int (*)()) NULL}
};

/*
 * The variables and table below are used to parse arguments from
 * the "argv" variable in Tk_Init.
 */

static int synchronize;
static char *name;
static char *display;
static char *geometry;
static char *colormap;
static char *visual;
static int rest = 0;

static Tk_ArgvInfo argTable[] = {
    {"-colormap", TK_ARGV_STRING, (char *) NULL, (char *) &colormap,
	"Colormap for main window"},
    {"-display", TK_ARGV_STRING, (char *) NULL, (char *) &display,
	"Display to use"},
    {"-geometry", TK_ARGV_STRING, (char *) NULL, (char *) &geometry,
	"Initial geometry for window"},
    {"-name", TK_ARGV_STRING, (char *) NULL, (char *) &name,
	"Name to use for application"},
    {"-sync", TK_ARGV_CONSTANT, (char *) 1, (char *) &synchronize,
	"Use synchronous mode for display server"},
    {"-visual", TK_ARGV_STRING, (char *) NULL, (char *) &visual,
	"Visual for main window"},
    {"--", TK_ARGV_REST, (char *) 1, (char *) &rest,
	"Pass all remaining arguments through to script"},
    {(char *) NULL, TK_ARGV_END, (char *) NULL, (char *) NULL,
	(char *) NULL}
};

/*
 * Forward declarations to procedures defined later in this file:
 */

static TkWindow	*	AllocWindow _ANSI_ARGS_((TkDisplay *dispPtr,
			    int screenNum, TkWindow *parentPtr));
static Tk_Window	CreateTopLevelWindow _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window parent, char *name, char *screenName));
static void		DeleteWindowsExitProc _ANSI_ARGS_((
			    ClientData clientData));
static void		DoConfigureNotify _ANSI_ARGS_((TkWindow *winPtr));
static TkDisplay *	GetScreen _ANSI_ARGS_((Tcl_Interp *interp,
			    char *screenName, int *screenPtr));
static int		NameWindow _ANSI_ARGS_((Tcl_Interp *interp,
			    TkWindow *winPtr, TkWindow *parentPtr,
			    char *name));
static void		OpenIM _ANSI_ARGS_((TkDisplay *dispPtr));
static void		UnlinkWindow _ANSI_ARGS_((TkWindow *winPtr));

/*
 *----------------------------------------------------------------------
 *
 * CreateTopLevelWindow --
 *
 *	Make a new window that will be at top-level (its parent will
 *	be the root window of a screen).
 *
 * Results:
 *	The return value is a token for the new window, or NULL if
 *	an error prevented the new window from being created.  If
 *	NULL is returned, an error message will be left in
 *	interp->result.
 *
 * Side effects:
 *	A new window structure is allocated locally.  An X
 *	window is NOT initially created, but will be created
 *	the first time the window is mapped.
 *
 *----------------------------------------------------------------------
 */

static Tk_Window
CreateTopLevelWindow(interp, parent, name, screenName)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    Tk_Window parent;		/* Token for logical parent of new window
				 * (used for naming, options, etc.).  May
				 * be NULL. */
    char *name;			/* Name for new window;  if parent is
				 * non-NULL, must be unique among parent's
				 * children. */
    char *screenName;		/* Name of screen on which to create
				 * window.  NULL means use DISPLAY environment
				 * variable to determine.  Empty string means
				 * use parent's screen, or DISPLAY if no
				 * parent. */
{
    register TkWindow *winPtr;
    register TkDisplay *dispPtr;
    int screenId;

    if (!initialized) {
	initialized = 1;
	tkActiveUid = Tk_GetUid("active");
	tkDisabledUid = Tk_GetUid("disabled");
	tkNormalUid = Tk_GetUid("normal");

	/*
	 * Create built-in image types.
	 */
    
	Tk_CreateImageType(&tkBitmapImageType);
	Tk_CreateImageType(&tkPhotoImageType);
    
	/*
	 * Create built-in photo image formats.
	 */
    
	Tk_CreatePhotoImageFormat(&tkImgFmtGIF);
	Tk_CreatePhotoImageFormat(&tkImgFmtPPM);

	/*
	 * Create exit handler to delete all windows when the application
	 * exits.
	 */

	Tcl_CreateExitHandler(DeleteWindowsExitProc, (ClientData) NULL);
    }

    if ((parent != NULL) && (screenName != NULL) && (screenName[0] == '\0')) {
	dispPtr = ((TkWindow *) parent)->dispPtr;
	screenId = Tk_ScreenNumber(parent);
    } else {
	dispPtr = GetScreen(interp, screenName, &screenId);
	if (dispPtr == NULL) {
	    return (Tk_Window) NULL;
	}
    }

    winPtr = AllocWindow(dispPtr, screenId, (TkWindow *) parent);

    /*
     * Force the window to use a the border pixel instead of border
     * pixmap.  This is needed for the case where the window doesn't
     * use the default visual.  In this case, the default border is
     * a pixmap inherited from the root window, which won't work because
     * it will have the wrong visual.
     */

    winPtr->dirtyAtts |= CWBorderPixel;

    /*
     * Internal windows don't normally ask for StructureNotify events,
     * since we can generate them internally.  However, for top-level
     * windows we need to ask for the events because the window could
     * be manipulated externally.
     */

    winPtr->atts.event_mask |= StructureNotifyMask;

    /*
     * (Need to set the TK_TOP_LEVEL flag immediately here;  otherwise
     * Tk_DestroyWindow will core dump if it is called before the flag
     * has been set.)
     */

    winPtr->flags |= TK_TOP_LEVEL;
    if (parent != NULL) {
	if (NameWindow(interp, winPtr, (TkWindow *) parent, name) != TCL_OK) {
	    Tk_DestroyWindow((Tk_Window) winPtr);
	    return (Tk_Window) NULL;
	}
    }
    TkWmNewWindow(winPtr);
    return (Tk_Window) winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * GetScreen --
 *
 *	Given a string name for a display-plus-screen, find the
 *	TkDisplay structure for the display and return the screen
 *	number too.
 *
 * Results:
 *	The return value is a pointer to information about the display,
 *	or NULL if the display couldn't be opened.  In this case, an
 *	error message is left in interp->result.  The location at
 *	*screenPtr is overwritten with the screen number parsed from
 *	screenName.
 *
 * Side effects:
 *	A new connection is opened to the display if there is no
 *	connection already.  A new TkDisplay data structure is also
 *	setup, if necessary.
 *
 *----------------------------------------------------------------------
 */

static TkDisplay *
GetScreen(interp, screenName, screenPtr)
    Tcl_Interp *interp;		/* Place to leave error message. */
    char *screenName;		/* Name for screen.  NULL or empty means
				 * use DISPLAY envariable. */
    int *screenPtr;		/* Where to store screen number. */
{
    register TkDisplay *dispPtr;
    char *p;
    int screenId;
    size_t length;

    /*
     * Separate the screen number from the rest of the display
     * name.  ScreenName is assumed to have the syntax
     * <display>.<screen> with the dot and the screen being
     * optional.
     */

    screenName = TkGetDefaultScreenName(interp, screenName);
    if (screenName == NULL) {
	interp->result =
	    "no display name and no $DISPLAY environment variable";
	return (TkDisplay *) NULL;
    }
    length = strlen(screenName);
    screenId = 0;
    p = screenName+length-1;
    while (isdigit(UCHAR(*p)) && (p != screenName)) {
	p--;
    }
    if ((*p == '.') && (p[1] != '\0')) {
	length = p - screenName;
	screenId = strtoul(p+1, (char **) NULL, 10);
    }

    /*
     * See if we already have a connection to this display.  If not,
     * then open a new connection.
     */

    for (dispPtr = tkDisplayList; ; dispPtr = dispPtr->nextPtr) {
	if (dispPtr == NULL) {
	    Display *display;

	    display = XOpenDisplay(screenName);
	    if (display == NULL) {
		Tcl_AppendResult(interp, "couldn't connect to display \"",
			screenName, "\"", (char *) NULL);
		return (TkDisplay *) NULL;
	    }
	    dispPtr = (TkDisplay *) ckalloc(sizeof(TkDisplay));
	    dispPtr->display = display;
	    dispPtr->nextPtr = tkDisplayList;
	    dispPtr->name = (char *) ckalloc((unsigned) (length+1));
	    dispPtr->lastEventTime = CurrentTime;
	    strncpy(dispPtr->name, screenName, length);
	    dispPtr->name[length] = '\0';
	    dispPtr->bindInfoStale = 1;
	    dispPtr->numModKeyCodes = 0;
	    dispPtr->modKeyCodes = NULL;
	    OpenIM(dispPtr);
	    dispPtr->errorPtr = NULL;
	    dispPtr->deleteCount = 0;
	    dispPtr->commTkwin = NULL;
	    dispPtr->selectionInfoPtr = NULL;
	    dispPtr->multipleAtom = None;
	    dispPtr->clipWindow = NULL;
	    dispPtr->clipboardActive = 0;
	    dispPtr->clipboardAppPtr = NULL;
	    dispPtr->atomInit = 0;
	    dispPtr->cursorFont = None;
	    dispPtr->grabWinPtr = NULL;
	    dispPtr->eventualGrabWinPtr = NULL;
	    dispPtr->buttonWinPtr = NULL;
	    dispPtr->serverWinPtr = NULL;
	    dispPtr->firstGrabEventPtr = NULL;
	    dispPtr->lastGrabEventPtr = NULL;
	    dispPtr->grabFlags = 0;
	    TkInitXId(dispPtr);
	    dispPtr->destroyCount = 0;
	    dispPtr->lastDestroyRequest = 0;
	    dispPtr->cmapPtr = NULL;
	    dispPtr->focusWinPtr = NULL;
	    dispPtr->implicitWinPtr = NULL;
	    dispPtr->focusOnMapPtr = NULL;
	    dispPtr->forceFocus = 0;
	    dispPtr->stressPtr = NULL;
	    dispPtr->delayedMotionPtr = NULL;
	    Tcl_InitHashTable(&dispPtr->winTable, TCL_ONE_WORD_KEYS);
	    tkDisplayList = dispPtr;
	    break;
	}
	if ((strncmp(dispPtr->name, screenName, length) == 0)
		&& (dispPtr->name[length] == '\0')) {
	    break;
	}
    }
    if (screenId >= ScreenCount(dispPtr->display)) {
	sprintf(interp->result, "bad screen number \"%d\"", screenId);
	return (TkDisplay *) NULL;
    }
    *screenPtr = screenId;
    return dispPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetDisplay --
 *
 *	Given an X display, TkGetDisplay returns the TkDisplay 
 *      structure for the display.
 *
 * Results:
 *	The return value is a pointer to information about the display,
 *	or NULL if the display did not have a TkDisplay structure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

TkDisplay *
TkGetDisplay(display)
     Display *display;          /* X's display pointer */
{
    TkDisplay *dispPtr;

    for (dispPtr = tkDisplayList; dispPtr != NULL;
	    dispPtr = dispPtr->nextPtr) {
	if (dispPtr->display == display) {
	    break;
	}
    }
    return dispPtr;
}

/*
 *--------------------------------------------------------------
 *
 * AllocWindow --
 *
 *	This procedure creates and initializes a TkWindow structure.
 *
 * Results:
 *	The return value is a pointer to the new window.
 *
 * Side effects:
 *	A new window structure is allocated and all its fields are
 *	initialized.
 *
 *--------------------------------------------------------------
 */

static TkWindow *
AllocWindow(dispPtr, screenNum, parentPtr)
    TkDisplay *dispPtr;		/* Display associated with new window. */
    int screenNum;		/* Index of screen for new window. */
    TkWindow *parentPtr;	/* Parent from which this window should
				 * inherit visual information.  NULL means
				 * use screen defaults instead of
				 * inheriting. */
{
    register TkWindow *winPtr;

    winPtr = (TkWindow *) ckalloc(sizeof(TkWindow));
    winPtr->display = dispPtr->display;
    winPtr->dispPtr = dispPtr;
    winPtr->screenNum = screenNum;
    if ((parentPtr != NULL) && (parentPtr->display == winPtr->display)
	    && (parentPtr->screenNum == winPtr->screenNum)) {
	winPtr->visual = parentPtr->visual;
	winPtr->depth = parentPtr->depth;
    } else {
	winPtr->visual = DefaultVisual(dispPtr->display, screenNum);
	winPtr->depth = DefaultDepth(dispPtr->display, screenNum);
    }
    winPtr->window = None;
    winPtr->childList = NULL;
    winPtr->lastChildPtr = NULL;
    winPtr->parentPtr = NULL;
    winPtr->nextPtr = NULL;
    winPtr->mainPtr = NULL;
    winPtr->pathName = NULL;
    winPtr->nameUid = NULL;
    winPtr->classUid = NULL;
    winPtr->changes = defChanges;
    winPtr->dirtyChanges = CWX|CWY|CWWidth|CWHeight|CWBorderWidth;
    winPtr->atts = defAtts;
    if ((parentPtr != NULL) && (parentPtr->display == winPtr->display)
	    && (parentPtr->screenNum == winPtr->screenNum)) {
	winPtr->atts.colormap = parentPtr->atts.colormap;
    } else {
	winPtr->atts.colormap = DefaultColormap(dispPtr->display, screenNum);
    }
    winPtr->dirtyAtts = CWEventMask|CWColormap|CWBitGravity;
    winPtr->flags = 0;
    winPtr->handlerList = NULL;
#ifdef TK_USE_INPUT_METHODS
    winPtr->inputContext = NULL;
#endif /* TK_USE_INPUT_METHODS */
    winPtr->tagPtr = NULL;
    winPtr->numTags = 0;
    winPtr->optionLevel = -1;
    winPtr->selHandlerList = NULL;
    winPtr->geomMgrPtr = NULL;
    winPtr->geomData = NULL;
    winPtr->reqWidth = winPtr->reqHeight = 1;
    winPtr->internalBorderWidth = 0;
    winPtr->wmInfoPtr = NULL;
    winPtr->privatePtr = NULL;

    return winPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * NameWindow --
 *
 *	This procedure is invoked to give a window a name and insert
 *	the window into the hierarchy associated with a particular
 *	application.
 *
 * Results:
 *	A standard Tcl return value.
 *
 * Side effects:
 *      See above.
 *
 *----------------------------------------------------------------------
 */

static int
NameWindow(interp, winPtr, parentPtr, name)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    register TkWindow *winPtr;	/* Window that is to be named and inserted. */
    TkWindow *parentPtr;	/* Pointer to logical parent for winPtr
				 * (used for naming, options, etc.). */
    char *name;			/* Name for winPtr;   must be unique among
				 * parentPtr's children. */
{
#define FIXED_SIZE 200
    char staticSpace[FIXED_SIZE];
    char *pathName;
    int new;
    Tcl_HashEntry *hPtr;
    int length1, length2;

    /*
     * Setup all the stuff except name right away, then do the name stuff
     * last.  This is so that if the name stuff fails, everything else
     * will be properly initialized (needed to destroy the window cleanly
     * after the naming failure).
     */
    winPtr->parentPtr = parentPtr;
    winPtr->nextPtr = NULL;
    if (parentPtr->childList == NULL) {
	parentPtr->childList = winPtr;
    } else {
	parentPtr->lastChildPtr->nextPtr = winPtr;
    }
    parentPtr->lastChildPtr = winPtr;
    winPtr->mainPtr = parentPtr->mainPtr;
    winPtr->mainPtr->refCount++;
    winPtr->nameUid = Tk_GetUid(name);

    /*
     * Don't permit names that start with an upper-case letter:  this
     * will just cause confusion with class names in the option database.
     */

    if (isupper(UCHAR(name[0]))) {
	Tcl_AppendResult(interp,
		"window name starts with an upper-case letter: \"",
		name, "\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * To permit names of arbitrary length, must be prepared to malloc
     * a buffer to hold the new path name.  To run fast in the common
     * case where names are short, use a fixed-size buffer on the
     * stack.
     */

    length1 = strlen(parentPtr->pathName);
    length2 = strlen(name);
    if ((length1+length2+2) <= FIXED_SIZE) {
	pathName = staticSpace;
    } else {
	pathName = (char *) ckalloc((unsigned) (length1+length2+2));
    }
    if (length1 == 1) {
	pathName[0] = '.';
	strcpy(pathName+1, name);
    } else {
	strcpy(pathName, parentPtr->pathName);
	pathName[length1] = '.';
	strcpy(pathName+length1+1, name);
    }
    hPtr = Tcl_CreateHashEntry(&parentPtr->mainPtr->nameTable, pathName, &new);
    if (pathName != staticSpace) {
	ckfree(pathName);
    }
    if (!new) {
	Tcl_AppendResult(interp, "window name \"", name,
		"\" already exists in parent", (char *) NULL);
	return TCL_ERROR;
    }
    Tcl_SetHashValue(hPtr, winPtr);
    winPtr->pathName = Tcl_GetHashKey(&parentPtr->mainPtr->nameTable, hPtr);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkCreateMainWindow --
 *
 *	Make a new main window.  A main window is a special kind of
 *	top-level window used as the outermost window in an
 *	application.
 *
 * Results:
 *	The return value is a token for the new window, or NULL if
 *	an error prevented the new window from being created.  If
 *	NULL is returned, an error message will be left in
 *	interp->result.
 *
 * Side effects:
 *	A new window structure is allocated locally;  "interp" is
 *	associated with the window and registered for "send" commands
 *	under "baseName".  BaseName may be extended with an instance
 *	number in the form "#2" if necessary to make it globally
 *	unique.  Tk-related commands are bound into interp.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
TkCreateMainWindow(interp, screenName, baseName)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting. */
    char *screenName;		/* Name of screen on which to create
				 * window.  Empty or NULL string means
				 * use DISPLAY environment variable. */
    char *baseName;		/* Base name for application;  usually of the
				 * form "prog instance". */
{
    Tk_Window tkwin;
    int dummy;
    Tcl_HashEntry *hPtr;
    register TkMainInfo *mainPtr;
    register TkWindow *winPtr;
    register TkCmd *cmdPtr;

    /*
     * Panic if someone updated the TkWindow structure without
     * also updating the Tk_FakeWin structure (or vice versa).
     */

    if (sizeof(TkWindow) != sizeof(Tk_FakeWin)) {
	panic("TkWindow and Tk_FakeWin are not the same size");
    }

    /*
     * Create the basic TkWindow structure.
     */

    tkwin = CreateTopLevelWindow(interp, (Tk_Window) NULL, baseName,
	    screenName);
    if (tkwin == NULL) {
	return NULL;
    }

    /*
     * Create the TkMainInfo structure for this application, and set
     * up name-related information for the new window.
     */

    winPtr = (TkWindow *) tkwin;
    mainPtr = (TkMainInfo *) ckalloc(sizeof(TkMainInfo));
    mainPtr->winPtr = winPtr;
    mainPtr->refCount = 1;
    mainPtr->interp = interp;
    Tcl_InitHashTable(&mainPtr->nameTable, TCL_STRING_KEYS);
    mainPtr->bindingTable = Tk_CreateBindingTable(interp);
    mainPtr->focusPtr = NULL;
    mainPtr->focusSerial = 0;
    mainPtr->lastFocusPtr = NULL;
    mainPtr->optionRootPtr = NULL;
    Tcl_InitHashTable(&mainPtr->imageTable, TCL_STRING_KEYS);
    mainPtr->strictMotif = 0;
    if (Tcl_LinkVar(interp, "tk_strictMotif", (char *) &mainPtr->strictMotif,
	    TCL_LINK_BOOLEAN) != TCL_OK) {
	Tcl_ResetResult(interp);
    }
    mainPtr->nextPtr = tkMainWindowList;
    tkMainWindowList = mainPtr;
    winPtr->mainPtr = mainPtr;
    hPtr = Tcl_CreateHashEntry(&mainPtr->nameTable, ".", &dummy);
    Tcl_SetHashValue(hPtr, winPtr);
    winPtr->pathName = Tcl_GetHashKey(&mainPtr->nameTable, hPtr);

    /*
     * Register the interpreter for "send" purposes.
     */

    winPtr->nameUid = Tk_GetUid(Tk_SetAppName(tkwin, baseName));

    /*
     * Bind in Tk's commands.
     */

    for (cmdPtr = commands; cmdPtr->name != NULL; cmdPtr++) {
	Tcl_CreateCommand(interp, cmdPtr->name, cmdPtr->cmdProc,
		(ClientData) tkwin, (void (*)()) NULL);
    }

    /*
     * Set variables for the intepreter.
     */

    Tcl_SetVar(interp, "tk_patchLevel", TK_PATCH_LEVEL, TCL_GLOBAL_ONLY);
    Tcl_SetVar(interp, "tk_version", TK_VERSION, TCL_GLOBAL_ONLY);

    numMainWindows++;
    return tkwin;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_CreateWindow --
 *
 *	Create a new internal or top-level window as a child of an
 *	existing window.
 *
 * Results:
 *	The return value is a token for the new window.  This
 *	is not the same as X's token for the window.  If an error
 *	occurred in creating the window (e.g. no such display or
 *	screen), then an error message is left in interp->result and
 *	NULL is returned.
 *
 * Side effects:
 *	A new window structure is allocated locally.  An X
 *	window is not initially created, but will be created
 *	the first time the window is mapped.
 *
 *--------------------------------------------------------------
 */

Tk_Window
Tk_CreateWindow(interp, parent, name, screenName)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting.
				 * Interp->result is assumed to be
				 * initialized by the caller. */
    Tk_Window parent;		/* Token for parent of new window. */
    char *name;			/* Name for new window.  Must be unique
				 * among parent's children. */
    char *screenName;		/* If NULL, new window will be internal on
				 * same screen as its parent.  If non-NULL,
				 * gives name of screen on which to create
				 * new window;  window will be a top-level
				 * window. */
{
    TkWindow *parentPtr = (TkWindow *) parent;
    TkWindow *winPtr;

    if (screenName == NULL) {
	winPtr = AllocWindow(parentPtr->dispPtr, parentPtr->screenNum,
		parentPtr);
	if (NameWindow(interp, winPtr, parentPtr, name) != TCL_OK) {
	    Tk_DestroyWindow((Tk_Window) winPtr);
	    return NULL;
	} else {
	  return (Tk_Window) winPtr;
	}
    } else {
	return CreateTopLevelWindow(interp, parent, name, screenName);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_CreateWindowFromPath --
 *
 *	This procedure is similar to Tk_CreateWindow except that
 *	it uses a path name to create the window, rather than a
 *	parent and a child name.
 *
 * Results:
 *	The return value is a token for the new window.  This
 *	is not the same as X's token for the window.  If an error
 *	occurred in creating the window (e.g. no such display or
 *	screen), then an error message is left in interp->result and
 *	NULL is returned.
 *
 * Side effects:
 *	A new window structure is allocated locally.  An X
 *	window is not initially created, but will be created
 *	the first time the window is mapped.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
Tk_CreateWindowFromPath(interp, tkwin, pathName, screenName)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting.
				 * Interp->result is assumed to be
				 * initialized by the caller. */
    Tk_Window tkwin;		/* Token for any window in application
				 * that is to contain new window. */
    char *pathName;		/* Path name for new window within the
				 * application of tkwin.  The parent of
				 * this window must already exist, but
				 * the window itself must not exist. */
    char *screenName;		/* If NULL, new window will be on same
				 * screen as its parent.  If non-NULL,
				 * gives name of screen on which to create
				 * new window;  window will be a top-level
				 * window. */
{
#define FIXED_SPACE 5
    char fixedSpace[FIXED_SPACE+1];
    char *p;
    Tk_Window parent;
    int numChars;

    /*
     * Strip the parent's name out of pathName (it's everything up
     * to the last dot).  There are two tricky parts: (a) must
     * copy the parent's name somewhere else to avoid modifying
     * the pathName string (for large names, space for the copy
     * will have to be malloc'ed);  (b) must special-case the
     * situation where the parent is ".".
     */

    p = strrchr(pathName, '.');
    if (p == NULL) {
	Tcl_AppendResult(interp, "bad window path name \"", pathName,
		"\"", (char *) NULL);
	return NULL;
    }
    numChars = p-pathName;
    if (numChars > FIXED_SPACE) {
	p = (char *) ckalloc((unsigned) (numChars+1));
    } else {
	p = fixedSpace;
    }
    if (numChars == 0) {
	*p = '.';
	p[1] = '\0';
    } else {
	strncpy(p, pathName, (size_t) numChars);
	p[numChars] = '\0';
    }

    /*
     * Find the parent window.
     */

    parent = Tk_NameToWindow(interp, p, tkwin);
    if (p != fixedSpace) {
	ckfree(p);
    }
    if (parent == NULL) {
	return NULL;
    }

    /*
     * Create the window.
     */

    if (screenName == NULL) {
	TkWindow *parentPtr = (TkWindow *) parent;
	TkWindow *winPtr;

	winPtr = AllocWindow(parentPtr->dispPtr, parentPtr->screenNum,
		parentPtr);
	if (NameWindow(interp, winPtr, parentPtr, pathName+numChars+1)
		!= TCL_OK) {
	    Tk_DestroyWindow((Tk_Window) winPtr);
	    return NULL;
	} else {
	    return (Tk_Window) winPtr;
	}
    } else {
	return CreateTopLevelWindow(interp, parent, pathName+numChars+1,
		screenName);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_DestroyWindow --
 *
 *	Destroy an existing window.  After this call, the caller
 *	should never again use the token.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is deleted, along with all of its children.
 *	Relevant callback procedures are invoked.
 *
 *--------------------------------------------------------------
 */

void
Tk_DestroyWindow(tkwin)
    Tk_Window tkwin;		/* Window to destroy. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkDisplay *dispPtr = winPtr->dispPtr;
    XEvent event;

    if (winPtr->flags & TK_ALREADY_DEAD) {
	/*
	 * A destroy event binding caused the window to be destroyed
	 * again.  Ignore the request.
	 */

	return;
    }
    winPtr->flags |= TK_ALREADY_DEAD;

    /*
     * Some cleanup needs to be done immediately, rather than later,
     * because it needs information that will be destoyed before we
     * get to the main cleanup point.  For example, TkFocusDeadWindow
     * needs to access the parentPtr field from a window, but if
     * a Destroy event handler deletes the window's parent this
     * field will be NULL before the main cleanup point is reached.
     */

    TkFocusDeadWindow(winPtr);

    /*
     * If this is a main window, remove it from the list of main
     * windows.  This needs to be done now (rather than later with
     * all the other main window cleanup) to handle situations where
     * a destroy binding for a window calls "exit".  In this case
     * the child window cleanup isn't complete when exit is called,
     * so the reference count of its application doesn't go to zero
     * when exit calls Tk_DestroyWindow on ".", so the main window
     * doesn't get removed from the list and exit loops infinitely.
     * Even worse, if "destroy ." is called by the destroy binding
     * before calling "exit", "exit" will attempt to destroy
     * mainPtr->winPtr, which no longer exists, and there may be a
     * core dump.
     */

    if (winPtr->mainPtr->winPtr == winPtr) {
	if (tkMainWindowList == winPtr->mainPtr) {
	    tkMainWindowList = winPtr->mainPtr->nextPtr;
	} else {
	    TkMainInfo *prevPtr;

	    for (prevPtr = tkMainWindowList;
		    prevPtr->nextPtr != winPtr->mainPtr;
		    prevPtr = prevPtr->nextPtr) {
		/* Empty loop body. */
	    }
	    prevPtr->nextPtr = winPtr->mainPtr->nextPtr;
	}
	numMainWindows--;
    }

    /*
     * Recursively destroy children.
     */

    dispPtr->destroyCount++;
    while (winPtr->childList != NULL) {
	TkWindow *childPtr;

	childPtr = winPtr->childList;
	childPtr->flags |= TK_PARENT_DESTROYED;
	Tk_DestroyWindow((Tk_Window) childPtr);
	if (winPtr->childList == childPtr) {
	    /*
	     * The child didn't remove itself from the child list, so
	     * let's remove it here.  This can happen in some strange
	     * conditions, such as when a Delete event handler for a
	     * window deletes the window's parent.
	     */

	    winPtr->childList = childPtr->nextPtr;
	    childPtr->parentPtr = NULL;
	}
    }

    /*
     * Generate a DestroyNotify event.  In order for the DestroyNotify
     * event to be processed correctly, need to make sure the window
     * exists.  This is a bit of a kludge, and may be unnecessarily
     * expensive, but without it no event handlers will get called for
     * windows that don't exist yet.
     *
     * Note: if the window's pathName is NULL it means that the window
     * was not successfully initialized in the first place, so we should
     * not make the window exist or generate the event.
     */

    if (winPtr->pathName != NULL) {
	if (winPtr->window == None) {
	    Tk_MakeWindowExist(tkwin);
	}
	event.type = DestroyNotify;
	event.xdestroywindow.serial =
		LastKnownRequestProcessed(winPtr->display);
	event.xdestroywindow.send_event = False;
	event.xdestroywindow.display = winPtr->display;
	event.xdestroywindow.event = winPtr->window;
	event.xdestroywindow.window = winPtr->window;
	Tk_HandleEvent(&event);
    }

    /*
     * Cleanup the data structures associated with this window.
     */

    if (winPtr->flags & TK_TOP_LEVEL) {
	TkWmDeadWindow(winPtr);
    } else if (winPtr->flags & TK_WM_COLORMAP_WINDOW) {
	TkWmRemoveFromColormapWindows(winPtr);
    }
    if (winPtr->window != None) {
#if defined(MAC_TCL) || defined(__WIN32__)
	XDestroyWindow(winPtr->display, winPtr->window);
#else
	if ((winPtr->flags & TK_TOP_LEVEL)
		|| !(winPtr->flags & TK_PARENT_DESTROYED)) {
	    /*
	     * The parent has already been destroyed and this isn't
	     * a top-level window, so this window will be destroyed
	     * implicitly when the parent's X window is destroyed;
	     * it's much faster not to do an explicit destroy of this
	     * X window.
	     */

	    dispPtr->lastDestroyRequest = NextRequest(winPtr->display);
	    XDestroyWindow(winPtr->display, winPtr->window);
	}
#endif
	TkFreeWindowId(dispPtr, winPtr->window);
	Tcl_DeleteHashEntry(Tcl_FindHashEntry(&dispPtr->winTable,
		(char *) winPtr->window));
	winPtr->window = None;
    }
    dispPtr->destroyCount--;
    UnlinkWindow(winPtr);
    TkEventDeadWindow(winPtr);
#ifdef TK_USE_INPUT_METHODS
    if (winPtr->inputContext != NULL) {
	XDestroyIC(winPtr->inputContext);
    }
#endif /* TK_USE_INPUT_METHODS */
    if (winPtr->tagPtr != NULL) {
	TkFreeBindingTags(winPtr);
    }
    TkOptionDeadWindow(winPtr);
    TkSelDeadWindow(winPtr);
    TkGrabDeadWindow(winPtr);
    if (winPtr->mainPtr != NULL) {
	if (winPtr->pathName != NULL) {
	    Tk_DeleteAllBindings(winPtr->mainPtr->bindingTable,
		    (ClientData) winPtr->pathName);
	    Tcl_DeleteHashEntry(Tcl_FindHashEntry(&winPtr->mainPtr->nameTable,
		    winPtr->pathName));
	}
	winPtr->mainPtr->refCount--;
	if (winPtr->mainPtr->refCount == 0) {
	    register TkCmd *cmdPtr;

	    /*
	     * We just deleted the last window in the application.  Delete
	     * the TkMainInfo structure too and replace all of Tk's commands
	     * with dummy commands that return errors.  Also delete the
	     * "send" command to unregister the interpreter.
             *
             * NOTE: Only replace the commands it if the interpreter is
             * not being deleted. If it *is*, the interpreter cleanup will
             * do all the needed work.
	     */

            if ((winPtr->mainPtr->interp != NULL) &&
                    (!Tcl_InterpDeleted(winPtr->mainPtr->interp))) {
                for (cmdPtr = commands; cmdPtr->name != NULL; cmdPtr++) {
                    Tcl_CreateCommand(winPtr->mainPtr->interp, cmdPtr->name,
                            TkDeadAppCmd, (ClientData) NULL,
                            (void (*)()) NULL);
                }
                Tcl_CreateCommand(winPtr->mainPtr->interp, "send",
                        TkDeadAppCmd, (ClientData) NULL, (void (*)()) NULL);
                Tcl_UnlinkVar(winPtr->mainPtr->interp, "tk_strictMotif");
            }
	    Tcl_DeleteHashTable(&winPtr->mainPtr->nameTable);
	    Tk_DeleteBindingTable(winPtr->mainPtr->bindingTable);
	    TkDeleteAllImages(winPtr->mainPtr);
	    ckfree((char *) winPtr->mainPtr);
	}
    }
    ckfree((char *) winPtr);
}

/*
 *--------------------------------------------------------------
 *
 * Tk_MapWindow --
 *
 *	Map a window within its parent.  This may require the
 *	window and/or its parents to actually be created.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The given window will be mapped.  Windows may also
 *	be created.
 *
 *--------------------------------------------------------------
 */

void
Tk_MapWindow(tkwin)
    Tk_Window tkwin;		/* Token for window to map. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;
    XEvent event;

    if (winPtr->flags & TK_MAPPED) {
	return;
    }
    if (winPtr->window == None) {
	Tk_MakeWindowExist(tkwin);
    }
    if (winPtr->flags & TK_TOP_LEVEL) {
	/*
	 * Lots of special processing has to be done for top-level
	 * windows.  Let tkWm.c handle everything itself.
	 */

	TkWmMapWindow(winPtr);
	return;
    }
    winPtr->flags |= TK_MAPPED;
    XMapWindow(winPtr->display, winPtr->window);
    event.type = MapNotify;
    event.xmap.serial = LastKnownRequestProcessed(winPtr->display);
    event.xmap.send_event = False;
    event.xmap.display = winPtr->display;
    event.xmap.event = winPtr->window;
    event.xmap.window = winPtr->window;
    event.xmap.override_redirect = winPtr->atts.override_redirect;
    Tk_HandleEvent(&event);
}

/*
 *--------------------------------------------------------------
 *
 * Tk_MakeWindowExist --
 *
 *	Ensure that a particular window actually exists.  This
 *	procedure shouldn't normally need to be invoked from
 *	outside the Tk package, but may be needed if someone
 *	wants to manipulate a window before mapping it.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	When the procedure returns, the X window associated with
 *	tkwin is guaranteed to exist.  This may require the
 *	window's ancestors to be created also.
 *
 *--------------------------------------------------------------
 */

void
Tk_MakeWindowExist(tkwin)
    Tk_Window tkwin;		/* Token for window. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;
    TkWindow *winPtr2;
    Window parent;
    Tcl_HashEntry *hPtr;
    int new;

    if (winPtr->window != None) {
	return;
    }

    if ((winPtr->parentPtr == NULL) || (winPtr->flags & TK_TOP_LEVEL)) {
	parent = XRootWindow(winPtr->display, winPtr->screenNum);
    } else {
	if (winPtr->parentPtr->window == None) {
	    Tk_MakeWindowExist((Tk_Window) winPtr->parentPtr);
	}
	parent = winPtr->parentPtr->window;
    }

    winPtr->window = TkMakeWindow(winPtr, parent);
    hPtr = Tcl_CreateHashEntry(&winPtr->dispPtr->winTable,
	    (char *) winPtr->window, &new);
    Tcl_SetHashValue(hPtr, winPtr);
    winPtr->dirtyAtts = 0;
    winPtr->dirtyChanges = 0;
#ifdef TK_USE_INPUT_METHODS
    winPtr->inputContext = NULL;
#endif /* TK_USE_INPUT_METHODS */

    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	/*
	 * If any siblings higher up in the stacking order have already
	 * been created then move this window to its rightful position
	 * in the stacking order.
	 *
	 * NOTE: this code ignores any changes anyone might have made
	 * to the sibling and stack_mode field of the window's attributes,
	 * so it really isn't safe for these to be manipulated except
	 * by calling Tk_RestackWindow.
	 */

	for (winPtr2 = winPtr->nextPtr; winPtr2 != NULL;
		winPtr2 = winPtr2->nextPtr) {
	    if ((winPtr2->window != None) && !(winPtr2->flags & TK_TOP_LEVEL)) {
		XWindowChanges changes;
		changes.sibling = winPtr2->window;
		changes.stack_mode = Below;
		XConfigureWindow(winPtr->display, winPtr->window,
			CWSibling|CWStackMode, &changes);
		break;
	    }
	}

	/*
	 * If this window has a different colormap than its parent, add
	 * the window to the WM_COLORMAP_WINDOWS property for its top-level.
	 */

	if ((winPtr->parentPtr != NULL) &&
		(winPtr->atts.colormap != winPtr->parentPtr->atts.colormap)) {
	    TkWmAddToColormapWindows(winPtr);
	    winPtr->flags |= TK_WM_COLORMAP_WINDOW;
	}
    }

    /*
     * Issue a ConfigureNotify event if there were deferred configuration
     * changes (but skip it if the window is being deleted;  the
     * ConfigureNotify event could cause problems if we're being called
     * from Tk_DestroyWindow under some conditions).
     */

    if ((winPtr->flags & TK_NEED_CONFIG_NOTIFY)
	    && !(winPtr->flags & TK_ALREADY_DEAD)){
	winPtr->flags &= ~TK_NEED_CONFIG_NOTIFY;
	DoConfigureNotify(winPtr);
    }
}

/*
 *--------------------------------------------------------------
 *
 * Tk_UnmapWindow, etc. --
 *
 *	There are several procedures under here, each of which
 *	mirrors an existing X procedure.  In addition to performing
 *	the functions of the corresponding procedure, each
 *	procedure also updates the local window structure and
 *	synthesizes an X event (if the window's structure is being
 *	managed internally).
 *
 * Results:
 *	See the manual entries.
 *
 * Side effects:
 *	See the manual entries.
 *
 *--------------------------------------------------------------
 */

void
Tk_UnmapWindow(tkwin)
    Tk_Window tkwin;		/* Token for window to unmap. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    if (!(winPtr->flags & TK_MAPPED) || (winPtr->flags & TK_ALREADY_DEAD)) {
	return;
    }
    if (winPtr->flags & TK_TOP_LEVEL) {
	/*
	 * Special processing has to be done for top-level windows.  Let
	 * tkWm.c handle everything itself.
	 */

	TkWmUnmapWindow(winPtr);
	return;
    }
    winPtr->flags &= ~TK_MAPPED;
    XUnmapWindow(winPtr->display, winPtr->window);
    if (!(winPtr->flags & TK_TOP_LEVEL)) {
	XEvent event;

	event.type = UnmapNotify;
	event.xunmap.serial = LastKnownRequestProcessed(winPtr->display);
	event.xunmap.send_event = False;
	event.xunmap.display = winPtr->display;
	event.xunmap.event = winPtr->window;
	event.xunmap.window = winPtr->window;
	event.xunmap.from_configure = False;
	Tk_HandleEvent(&event);
    }
}

void
Tk_ConfigureWindow(tkwin, valueMask, valuePtr)
    Tk_Window tkwin;		/* Window to re-configure. */
    unsigned int valueMask;	/* Mask indicating which parts of
				 * *valuePtr are to be used. */
    XWindowChanges *valuePtr;	/* New values. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    if (valueMask & CWX) {
	winPtr->changes.x = valuePtr->x;
    }
    if (valueMask & CWY) {
	winPtr->changes.y = valuePtr->y;
    }
    if (valueMask & CWWidth) {
	winPtr->changes.width = valuePtr->width;
    }
    if (valueMask & CWHeight) {
	winPtr->changes.height = valuePtr->height;
    }
    if (valueMask & CWBorderWidth) {
	winPtr->changes.border_width = valuePtr->border_width;
    }
    if (valueMask & (CWSibling|CWStackMode)) {
	panic("Can't set sibling or stack mode from Tk_ConfigureWindow.");
    }

    if (winPtr->window != None) {
	XConfigureWindow(winPtr->display, winPtr->window,
		valueMask, valuePtr);
	if (!(winPtr->flags & TK_TOP_LEVEL)) {
	    DoConfigureNotify(winPtr);
	}
    } else {
	winPtr->dirtyChanges |= valueMask;
	winPtr->flags |= TK_NEED_CONFIG_NOTIFY;
    }
}

void
Tk_MoveWindow(tkwin, x, y)
    Tk_Window tkwin;		/* Window to move. */
    int x, y;			/* New location for window (within
				 * parent). */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->changes.x = x;
    winPtr->changes.y = y;
    if (winPtr->window != None) {
	XMoveWindow(winPtr->display, winPtr->window, x, y);
	if (!(winPtr->flags & TK_TOP_LEVEL)) {
	    DoConfigureNotify(winPtr);
	}
    } else {
	winPtr->dirtyChanges |= CWX|CWY;
	winPtr->flags |= TK_NEED_CONFIG_NOTIFY;
    }
}

void
Tk_ResizeWindow(tkwin, width, height)
    Tk_Window tkwin;		/* Window to resize. */
    int width, height;		/* New dimensions for window. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->changes.width = (unsigned) width;
    winPtr->changes.height = (unsigned) height;
    if (winPtr->window != None) {
	XResizeWindow(winPtr->display, winPtr->window, (unsigned) width,
		(unsigned) height);
	if (!(winPtr->flags & TK_TOP_LEVEL)) {
	    DoConfigureNotify(winPtr);
	}
    } else {
	winPtr->dirtyChanges |= CWWidth|CWHeight;
	winPtr->flags |= TK_NEED_CONFIG_NOTIFY;
    }
}

void
Tk_MoveResizeWindow(tkwin, x, y, width, height)
    Tk_Window tkwin;		/* Window to move and resize. */
    int x, y;			/* New location for window (within
				 * parent). */
    int width, height;		/* New dimensions for window. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->changes.x = x;
    winPtr->changes.y = y;
    winPtr->changes.width = (unsigned) width;
    winPtr->changes.height = (unsigned) height;
    if (winPtr->window != None) {
	XMoveResizeWindow(winPtr->display, winPtr->window, x, y,
		(unsigned) width, (unsigned) height);
	if (!(winPtr->flags & TK_TOP_LEVEL)) {
	    DoConfigureNotify(winPtr);
	}
    } else {
	winPtr->dirtyChanges |= CWX|CWY|CWWidth|CWHeight;
	winPtr->flags |= TK_NEED_CONFIG_NOTIFY;
    }
}

void
Tk_SetWindowBorderWidth(tkwin, width)
    Tk_Window tkwin;		/* Window to modify. */
    int width;			/* New border width for window. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->changes.border_width = width;
    if (winPtr->window != None) {
	XSetWindowBorderWidth(winPtr->display, winPtr->window,
		(unsigned) width);
	if (!(winPtr->flags & TK_TOP_LEVEL)) {
	    DoConfigureNotify(winPtr);
	}
    } else {
	winPtr->dirtyChanges |= CWBorderWidth;
	winPtr->flags |= TK_NEED_CONFIG_NOTIFY;
    }
}

void
Tk_ChangeWindowAttributes(tkwin, valueMask, attsPtr)
    Tk_Window tkwin;		/* Window to manipulate. */
    unsigned long valueMask;	/* OR'ed combination of bits,
				 * indicating which fields of
				 * *attsPtr are to be used. */
    register XSetWindowAttributes *attsPtr;
				/* New values for some attributes. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    if (valueMask & CWBackPixmap) {
	winPtr->atts.background_pixmap = attsPtr->background_pixmap;
    }
    if (valueMask & CWBackPixel) {
	winPtr->atts.background_pixel = attsPtr->background_pixel;
    }
    if (valueMask & CWBorderPixmap) {
	winPtr->atts.border_pixmap = attsPtr->border_pixmap;
    }
    if (valueMask & CWBorderPixel) {
	winPtr->atts.border_pixel = attsPtr->border_pixel;
    }
    if (valueMask & CWBitGravity) {
	winPtr->atts.bit_gravity = attsPtr->bit_gravity;
    }
    if (valueMask & CWWinGravity) {
	winPtr->atts.win_gravity = attsPtr->win_gravity;
    }
    if (valueMask & CWBackingStore) {
	winPtr->atts.backing_store = attsPtr->backing_store;
    }
    if (valueMask & CWBackingPlanes) {
	winPtr->atts.backing_planes = attsPtr->backing_planes;
    }
    if (valueMask & CWBackingPixel) {
	winPtr->atts.backing_pixel = attsPtr->backing_pixel;
    }
    if (valueMask & CWOverrideRedirect) {
	winPtr->atts.override_redirect = attsPtr->override_redirect;
    }
    if (valueMask & CWSaveUnder) {
	winPtr->atts.save_under = attsPtr->save_under;
    }
    if (valueMask & CWEventMask) {
	winPtr->atts.event_mask = attsPtr->event_mask;
    }
    if (valueMask & CWDontPropagate) {
	winPtr->atts.do_not_propagate_mask
		= attsPtr->do_not_propagate_mask;
    }
    if (valueMask & CWColormap) {
	winPtr->atts.colormap = attsPtr->colormap;
    }
    if (valueMask & CWCursor) {
	winPtr->atts.cursor = attsPtr->cursor;
    }

    if (winPtr->window != None) {
	XChangeWindowAttributes(winPtr->display, winPtr->window,
		valueMask, attsPtr);
    } else {
	winPtr->dirtyAtts |= valueMask;
    }
}

void
Tk_SetWindowBackground(tkwin, pixel)
    Tk_Window tkwin;		/* Window to manipulate. */
    unsigned long pixel;	/* Pixel value to use for
				 * window's background. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->atts.background_pixel = pixel;

    if (winPtr->window != None) {
	XSetWindowBackground(winPtr->display, winPtr->window, pixel);
    } else {
	winPtr->dirtyAtts = (winPtr->dirtyAtts & (unsigned) ~CWBackPixmap)
		| CWBackPixel;
    }
}

void
Tk_SetWindowBackgroundPixmap(tkwin, pixmap)
    Tk_Window tkwin;		/* Window to manipulate. */
    Pixmap pixmap;		/* Pixmap to use for window's
				 * background. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->atts.background_pixmap = pixmap;

    if (winPtr->window != None) {
	XSetWindowBackgroundPixmap(winPtr->display,
		winPtr->window, pixmap);
    } else {
	winPtr->dirtyAtts = (winPtr->dirtyAtts & (unsigned) ~CWBackPixel)
		| CWBackPixmap;
    }
}

void
Tk_SetWindowBorder(tkwin, pixel)
    Tk_Window tkwin;		/* Window to manipulate. */
    unsigned long pixel;	/* Pixel value to use for
				 * window's border. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->atts.border_pixel = pixel;

    if (winPtr->window != None) {
	XSetWindowBorder(winPtr->display, winPtr->window, pixel);
    } else {
	winPtr->dirtyAtts = (winPtr->dirtyAtts & (unsigned) ~CWBorderPixmap)
		| CWBorderPixel;
    }
}

void
Tk_SetWindowBorderPixmap(tkwin, pixmap)
    Tk_Window tkwin;		/* Window to manipulate. */
    Pixmap pixmap;		/* Pixmap to use for window's
				 * border. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->atts.border_pixmap = pixmap;

    if (winPtr->window != None) {
	XSetWindowBorderPixmap(winPtr->display,
		winPtr->window, pixmap);
    } else {
	winPtr->dirtyAtts = (winPtr->dirtyAtts & (unsigned) ~CWBorderPixel)
		| CWBorderPixmap;
    }
}

void
Tk_DefineCursor(tkwin, cursor)
    Tk_Window tkwin;		/* Window to manipulate. */
    Tk_Cursor cursor;		/* Cursor to use for window (may be None). */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

#ifdef MAC_TCL
    winPtr->atts.cursor = (XCursor) cursor;
#else
    winPtr->atts.cursor = (Cursor) cursor;
#endif
    
    if (winPtr->window != None) {
	XDefineCursor(winPtr->display, winPtr->window, winPtr->atts.cursor);
    } else {
	winPtr->dirtyAtts = winPtr->dirtyAtts | CWCursor;
    }
}

void
Tk_UndefineCursor(tkwin)
    Tk_Window tkwin;		/* Window to manipulate. */
{
    Tk_DefineCursor(tkwin, None);
}

void
Tk_SetWindowColormap(tkwin, colormap)
    Tk_Window tkwin;		/* Window to manipulate. */
    Colormap colormap;		/* Colormap to use for window. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->atts.colormap = colormap;

    if (winPtr->window != None) {
	XSetWindowColormap(winPtr->display, winPtr->window, colormap);
	if (!(winPtr->flags & TK_TOP_LEVEL)) {
	    TkWmAddToColormapWindows(winPtr);
	    winPtr->flags |= TK_WM_COLORMAP_WINDOW;
	}
    } else {
	winPtr->dirtyAtts |= CWColormap;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetWindowVisual --
 *
 *	This procedure is called to specify a visual to be used
 *	for a Tk window when it is created.  This procedure, if
 *	called at all, must be called before the X window is created
 *	(i.e. before Tk_MakeWindowExist is called).
 *
 * Results:
 *	The return value is 1 if successful, or 0 if the X window has
 *	been already created.
 *
 * Side effects:
 *	The information given is stored for when the window is created.
 *
 *----------------------------------------------------------------------
 */

int
Tk_SetWindowVisual(tkwin, visual, depth, colormap)
    Tk_Window tkwin;		/* Window to manipulate. */
    Visual *visual;		/* New visual for window. */
    int depth;			/* New depth for window. */
    Colormap colormap;		/* An appropriate colormap for the visual. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    if( winPtr->window != None ){
	/* Too late! */
	return 0;
    }

    winPtr->visual = visual;
    winPtr->depth = depth;
    winPtr->atts.colormap = colormap;

    /*
     * The following code is needed to make sure that the window doesn't
     * inherit the parent's border pixmap, which would result in a BadMatch
     * error.
     */

    if (!(winPtr->dirtyAtts & CWBorderPixmap)) {
	winPtr->dirtyAtts |= CWBorderPixel;
    }
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * DoConfigureNotify --
 *
 *	Generate a ConfigureNotify event describing the current
 *	configuration of a window.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	An event is generated and processed by Tk_HandleEvent.
 *
 *----------------------------------------------------------------------
 */

static void
DoConfigureNotify(winPtr)
    register TkWindow *winPtr;		/* Window whose configuration
					 * was just changed. */
{
    XEvent event;

    event.type = ConfigureNotify;
    event.xconfigure.serial = LastKnownRequestProcessed(winPtr->display);
    event.xconfigure.send_event = False;
    event.xconfigure.display = winPtr->display;
    event.xconfigure.event = winPtr->window;
    event.xconfigure.window = winPtr->window;
    event.xconfigure.x = winPtr->changes.x;
    event.xconfigure.y = winPtr->changes.y;
    event.xconfigure.width = winPtr->changes.width;
    event.xconfigure.height = winPtr->changes.height;
    event.xconfigure.border_width = winPtr->changes.border_width;
    if (winPtr->changes.stack_mode == Above) {
	event.xconfigure.above = winPtr->changes.sibling;
    } else {
	event.xconfigure.above = None;
    }
    event.xconfigure.override_redirect = winPtr->atts.override_redirect;
    Tk_HandleEvent(&event);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_SetClass --
 *
 *	This procedure is used to give a window a class.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	A new class is stored for tkwin, replacing any existing
 *	class for it.
 *
 *----------------------------------------------------------------------
 */

void
Tk_SetClass(tkwin, className)
    Tk_Window tkwin;		/* Token for window to assign class. */
    char *className;		/* New class for tkwin. */
{
    register TkWindow *winPtr = (TkWindow *) tkwin;

    winPtr->classUid = Tk_GetUid(className);
    if (winPtr->flags & TK_TOP_LEVEL) {
	TkWmSetClass(winPtr);
    }
    TkOptionClassChanged(winPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_NameToWindow --
 *
 *	Given a string name for a window, this procedure
 *	returns the token for the window, if there exists a
 *	window corresponding to the given name.
 *
 * Results:
 *	The return result is either a token for the window corresponding
 *	to "name", or else NULL to indicate that there is no such
 *	window.  In this case, an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
Tk_NameToWindow(interp, pathName, tkwin)
    Tcl_Interp *interp;		/* Where to report errors. */
    char *pathName;		/* Path name of window. */
    Tk_Window tkwin;		/* Token for window:  name is assumed to
				 * belong to the same main window as tkwin. */
{
    Tcl_HashEntry *hPtr;

    hPtr = Tcl_FindHashEntry(&((TkWindow *) tkwin)->mainPtr->nameTable,
	    pathName);
    if (hPtr == NULL) {
	Tcl_AppendResult(interp, "bad window path name \"",
		pathName, "\"", (char *) NULL);
	return NULL;
    }
    return (Tk_Window) Tcl_GetHashValue(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_IdToWindow --
 *
 *	Given an X display and window ID, this procedure returns the
 *	Tk token for the window, if there exists a Tk window corresponding
 *	to the given ID.
 *
 * Results:
 *	The return result is either a token for the window corresponding
 *	to the given X id, or else NULL to indicate that there is no such
 *	window.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
Tk_IdToWindow(display, window)
    Display *display;		/* X display containing the window. */
    Window window;		/* X window window id. */
{
    TkDisplay *dispPtr;
    Tcl_HashEntry *hPtr;

    for (dispPtr = tkDisplayList; ; dispPtr = dispPtr->nextPtr) {
	if (dispPtr == NULL) {
	    return NULL;
	}
	if (dispPtr->display == display) {
	    break;
	}
    }

    hPtr = Tcl_FindHashEntry(&dispPtr->winTable, (char *) window);
    if (hPtr == NULL) {
	return NULL;
    }
    return (Tk_Window) Tcl_GetHashValue(hPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_DisplayName --
 *
 *	Return the textual name of a window's display.
 *
 * Results:
 *	The return value is the string name of the display associated
 *	with tkwin.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
Tk_DisplayName(tkwin)
    Tk_Window tkwin;		/* Window whose display name is desired. */
{
    return ((TkWindow *) tkwin)->dispPtr->name;
}

/*
 *----------------------------------------------------------------------
 *
 * UnlinkWindow --
 *
 *	This procedure removes a window from the childList of its
 *	parent.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The window is unlinked from its childList.
 *
 *----------------------------------------------------------------------
 */

static void
UnlinkWindow(winPtr)
    TkWindow *winPtr;			/* Child window to be unlinked. */
{
    TkWindow *prevPtr;

    if (winPtr->parentPtr == NULL) {
	return;
    }
    prevPtr = winPtr->parentPtr->childList;
    if (prevPtr == winPtr) {
	winPtr->parentPtr->childList = winPtr->nextPtr;
	if (winPtr->nextPtr == NULL) {
	    winPtr->parentPtr->lastChildPtr = NULL;
	}
    } else {
	while (prevPtr->nextPtr != winPtr) {
	    prevPtr = prevPtr->nextPtr;
	    if (prevPtr == NULL) {
		panic("UnlinkWindow couldn't find child in parent");
	    }
	}
	prevPtr->nextPtr = winPtr->nextPtr;
	if (winPtr->nextPtr == NULL) {
	    winPtr->parentPtr->lastChildPtr = prevPtr;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_RestackWindow --
 *
 *	Change a window's position in the stacking order.
 *
 * Results:
 *	TCL_OK is normally returned.  If other is not a descendant
 *	of tkwin's parent then TCL_ERROR is returned and tkwin is
 *	not repositioned.
 *
 * Side effects:
 *	Tkwin is repositioned in the stacking order.
 *
 *----------------------------------------------------------------------
 */

int
Tk_RestackWindow(tkwin, aboveBelow, other)
    Tk_Window tkwin;		/* Token for window whose position in
				 * the stacking order is to change. */
    int aboveBelow;		/* Indicates new position of tkwin relative
				 * to other;  must be Above or Below. */
    Tk_Window other;		/* Tkwin will be moved to a position that
				 * puts it just above or below this window.
				 * If NULL then tkwin goes above or below
				 * all windows in the same parent. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkWindow *otherPtr = (TkWindow *) other;
    XWindowChanges changes;
    unsigned int mask;


    /*
     * Special case:  if winPtr is a top-level window then just find
     * the top-level ancestor of otherPtr and restack winPtr above
     * otherPtr without changing any of Tk's childLists.
     */

    changes.stack_mode = aboveBelow;
    mask = CWStackMode;
    if (winPtr->flags & TK_TOP_LEVEL) {
	while ((otherPtr != NULL) && !(otherPtr->flags & TK_TOP_LEVEL)) {
	    otherPtr = otherPtr->parentPtr;
	}
	TkWmRestackToplevel(winPtr, aboveBelow, otherPtr);
	return TCL_OK;
    }

    /*
     * Find an ancestor of otherPtr that is a sibling of winPtr.
     */

    if (winPtr->parentPtr == NULL) {
	/*
	 * Window is going to be deleted shortly;  don't do anything.
	 */

	return TCL_OK;
    }
    if (otherPtr == NULL) {
	if (aboveBelow == Above) {
	    otherPtr = winPtr->parentPtr->lastChildPtr;
	} else {
	    otherPtr = winPtr->parentPtr->childList;
	}
    } else {
	while (winPtr->parentPtr != otherPtr->parentPtr) {
	    if ((otherPtr == NULL) || (otherPtr->flags & TK_TOP_LEVEL)) {
		return TCL_ERROR;
	    }
	    otherPtr = otherPtr->parentPtr;
	}
    }
    if (otherPtr == winPtr) {
	return TCL_OK;
    }

    /*
     * Reposition winPtr in the stacking order.
     */

    UnlinkWindow(winPtr);
    if (aboveBelow == Above) {
	winPtr->nextPtr = otherPtr->nextPtr;
	if (winPtr->nextPtr == NULL) {
	    winPtr->parentPtr->lastChildPtr = winPtr;
	}
	otherPtr->nextPtr = winPtr;
    } else {
	TkWindow *prevPtr;

	prevPtr = winPtr->parentPtr->childList;
	if (prevPtr == otherPtr) {
	    winPtr->parentPtr->childList = winPtr;
	} else {
	    while (prevPtr->nextPtr != otherPtr) {
		prevPtr = prevPtr->nextPtr;
	    }
	    prevPtr->nextPtr = winPtr;
	}
	winPtr->nextPtr = otherPtr;
    }

    /*
     * Notify the X server of the change.  If winPtr hasn't yet been
     * created then there's no need to tell the X server now, since
     * the stacking order will be handled properly when the window
     * is finally created.
     */

    if (winPtr->window != None) {
	changes.stack_mode = Above;
	for (otherPtr = winPtr->nextPtr; otherPtr != NULL;
		otherPtr = otherPtr->nextPtr) {
	    if ((otherPtr->window != None)
		    && !(otherPtr->flags & TK_TOP_LEVEL)){
		changes.sibling = otherPtr->window;
		changes.stack_mode = Below;
		mask = CWStackMode|CWSibling;
		break;
	    }
	}
	XConfigureWindow(winPtr->display, winPtr->window, mask, &changes);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_MainWindow --
 *
 *	Returns the main window for an application.
 *
 * Results:
 *	If interp has a Tk application associated with it, the main
 *	window for the application is returned.  Otherwise NULL is
 *	returned and an error message is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tk_Window
Tk_MainWindow(interp)
    Tcl_Interp *interp;			/* Interpreter that embodies the
					 * application.  Used for error
					 * reporting also. */
{
    TkMainInfo *mainPtr;

    for (mainPtr = tkMainWindowList; mainPtr != NULL;
	    mainPtr = mainPtr->nextPtr) {
	if (mainPtr->interp == interp) {
	    return (Tk_Window) mainPtr->winPtr;
	}
    }
    interp->result = "this isn't a Tk application";
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_StrictMotif --
 *
 *	Indicates whether strict Motif compliance has been specified
 *	for the given window.
 *
 * Results:
 *	The return value is 1 if strict Motif compliance has been
 *	requested for tkwin's application by setting the tk_strictMotif
 *	variable in its interpreter to a true value.  0 is returned
 *	if tk_strictMotif has a false value.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tk_StrictMotif(tkwin)
    Tk_Window tkwin;			/* Window whose application is
					 * to be checked. */
{
    return ((TkWindow *) tkwin)->mainPtr->strictMotif;
}

/* 
 *--------------------------------------------------------------
 *
 * OpenIM --
 *
 *	Tries to open an X input method, associated with the
 *	given display.  Right now we can only deal with a bare-bones
 *	input style:  no preedit, and no status.
 *
 * Results:
 *	Stores the input method in dispPtr->inputMethod;  if there isn't
 *	a suitable input method, then NULL is stored in dispPtr->inputMethod.
 *
 * Side effects:
 *	An input method gets opened.
 *
 *--------------------------------------------------------------
 */

static void
OpenIM(dispPtr)
    TkDisplay *dispPtr;		/* Tk's structure for the display. */
{
#ifndef TK_USE_INPUT_METHODS
    return;
#else
    unsigned short i;
    XIMStyles *stylePtr;

    dispPtr->inputMethod = XOpenIM(dispPtr->display, NULL, NULL, NULL);
    if (dispPtr->inputMethod == NULL) {
	return;
    }

    if ((XGetIMValues(dispPtr->inputMethod, XNQueryInputStyle, &stylePtr,
	    NULL) != NULL) || (stylePtr == NULL)) {
	goto error;
    }
    for (i = 0; i < stylePtr->count_styles; i++) {
	if (stylePtr->supported_styles[i]
		== (XIMPreeditNothing|XIMStatusNothing)) {
	    XFree(stylePtr);
	    return;
	}
    }
    XFree(stylePtr);

    error:

    /*
     * Should close the input method, but this causes core dumps on some
     * systems (e.g. Solaris 2.3 as of 1/6/95).
     * XCloseIM(dispPtr->inputMethod);
     */
    dispPtr->inputMethod = NULL;
    return;
#endif /* TK_USE_INPUT_METHODS */
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_GetNumMainWindows --
 *
 *	This procedure returns the number of main windows currently
 *	open in this process.
 *
 * Results:
 *	The number of main windows open in this process.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tk_GetNumMainWindows()
{
    return numMainWindows;
}

/*
 *----------------------------------------------------------------------
 *
 * DeleteWindowsExitProc --
 *
 *	This procedure is invoked as an exit handler.  It deletes all
 *	of the main windows in the process.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static void
DeleteWindowsExitProc(clientData)
    ClientData clientData;		/* Not used. */
{
    while (tkMainWindowList != NULL) {
	Tk_DestroyWindow((Tk_Window) tkMainWindowList->winPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_Init --
 *
 *	This procedure is invoked to add Tk to an interpreter.  It
 *	incorporates all of Tk's commands into the interpreter and
 *	creates the main window for a new Tk application.  If the
 *	interpreter contains a variable "argv", this procedure
 *	extracts several arguments from that variable, uses them
 *	to configure the main window, and modifies argv to exclude
 *	the arguments (see the "wish" documentation for a list of
 *	the arguments that are extracted).
 *
 * Results:
 *	Returns a standard Tcl completion code and sets interp->result
 *	if there is an error.
 *
 * Side effects:
 *	Depends on various initialization scripts that get invoked.
 *
 *----------------------------------------------------------------------
 */

int
Tk_Init(interp)
    Tcl_Interp *interp;		/* Interpreter to initialize. */
{
    char *p;
    int argc, code;
    char **argv, *args[20];
    Tcl_DString class;
    char buffer[30];

    /*
     * If there is an "argv" variable, get its value, extract out
     * relevant arguments from it, and rewrite the variable without
     * the arguments that we used.
     */

    synchronize = 0;
    name = display = geometry = colormap = visual = NULL; 
    p = Tcl_GetVar2(interp, "argv", (char *) NULL, TCL_GLOBAL_ONLY);
    argv = NULL;
    if (p != NULL) {
	if (Tcl_SplitList(interp, p, &argc, &argv) != TCL_OK) {
	    argError:
	    Tcl_AddErrorInfo(interp,
		    "\n    (processing arguments in argv variable)");
	    return TCL_ERROR;
	}
	if (Tk_ParseArgv(interp, (Tk_Window) NULL, &argc, argv,
		argTable, TK_ARGV_DONT_SKIP_FIRST_ARG|TK_ARGV_NO_DEFAULTS)
		!= TCL_OK) {
	    ckfree((char *) argv);
	    goto argError;
	}
	p = Tcl_Merge(argc, argv);
	Tcl_SetVar2(interp, "argv", (char *) NULL, p, TCL_GLOBAL_ONLY);
	sprintf(buffer, "%d", argc);
	Tcl_SetVar2(interp, "argc", (char *) NULL, buffer, TCL_GLOBAL_ONLY);
	ckfree(p);
    }

    /*
     * Figure out the application's name and class.
     */

    if (name == NULL) {
	name = Tcl_GetVar(interp, "argv0", TCL_GLOBAL_ONLY);
	if ((name == NULL) || (*name == 0)) {
	    name = "tk";
	} else {
	    p = strrchr(name, '/');
	    if (p != NULL) {
		name = p+1;
	    }
	}
    }
    Tcl_DStringInit(&class);
    Tcl_DStringAppend(&class, name, -1);
    p = Tcl_DStringValue(&class);
    if (islower(*p)) {
	*p = toupper((unsigned char) *p);
    }

    /*
     * Create an argument list for creating the top-level window,
     * using the information parsed from argv, if any.
     */

    args[0] = "toplevel";
    args[1] = ".";
    args[2] = "-class";
    args[3] = Tcl_DStringValue(&class);
    argc = 4;
    if (display != NULL) {
	args[argc] = "-screen";
	args[argc+1] = display;
	argc += 2;

	/*
	 * If this is the first application for this process, save
	 * the display name in the DISPLAY environment variable so
	 * that it will be available to subprocesses created by us.
	 */

	if (numMainWindows == 0) {
	    Tcl_SetVar2(interp, "env", "DISPLAY", display, TCL_GLOBAL_ONLY);
	}
    }
    if (colormap != NULL) {
	args[argc] = "-colormap";
	args[argc+1] = colormap;
	argc += 2;
    }
    if (visual != NULL) {
	args[argc] = "-visual";
	args[argc+1] = visual;
	argc += 2;
    }
    args[argc] = NULL;
    code = TkCreateFrame((ClientData) NULL, interp, argc, args, 1, name);
    Tcl_DStringFree(&class);
    if (code != TCL_OK) {
	goto done;
    }
    Tcl_ResetResult(interp);
    if (synchronize) {
	XSynchronize(Tk_Display(Tk_MainWindow(interp)), True);
    }

    /*
     * Set the geometry of the main window, if requested.  Put the
     * requested geometry into the "geometry" variable.
     */

    if (geometry != NULL) {
	Tcl_SetVar(interp, "geometry", geometry, TCL_GLOBAL_ONLY);
	code = Tcl_VarEval(interp, "wm geometry . ", geometry, (char *) NULL);
	if (code != TCL_OK) {
	    goto done;
	}
    }
    if (Tcl_PkgRequire(interp, "Tcl", TCL_VERSION, 1) == NULL) {
	code = TCL_ERROR;
	goto done;
    }
    code = Tcl_PkgProvide(interp, "Tk", TK_VERSION);
    if (code != TCL_OK) {
	goto done;
    }

    /*
     * Invoke platform-specific initialization.
     */

    code = TkPlatformInit(interp);

    done:
    if (argv != NULL) {
	ckfree((char *) argv);
    }
    return code;
}

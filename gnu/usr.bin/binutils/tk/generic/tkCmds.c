/* 
 * tkCmds.c --
 *
 *	This file contains a collection of Tk-related Tcl commands
 *	that didn't fit in any particular file of the toolkit.
 *
 * Copyright (c) 1990-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkCmds.c 1.110 96/04/03 15:54:47
 */

#include "tkPort.h"
#include "tkInt.h"
#include <errno.h>

/*
 * Forward declarations for procedures defined later in this file:
 */

static Tk_Window	GetDisplayOf _ANSI_ARGS_((Tcl_Interp *interp,
			    Tk_Window tkwin, char **argv));
static TkWindow *	GetToplevel _ANSI_ARGS_((Tk_Window tkwin));
static char *		WaitVariableProc _ANSI_ARGS_((ClientData clientData,
			    Tcl_Interp *interp, char *name1, char *name2,
			    int flags));
static void		WaitVisibilityProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static void		WaitWindowProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));

/*
 *----------------------------------------------------------------------
 *
 * Tk_BellCmd --
 *
 *	This procedure is invoked to process the "bell" Tcl command.
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

int
Tk_BellCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    size_t length;

    if ((argc != 1) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?-displayof window?\"", (char *) NULL);
	return TCL_ERROR;
    }

    if (argc == 3) {
	length = strlen(argv[1]);
	if ((length < 2) || (strncmp(argv[1], "-displayof", length) != 0)) {
	    Tcl_AppendResult(interp, "bad option \"", argv[1],
		    "\": must be -displayof", (char *) NULL);
	    return TCL_ERROR;
	}
	tkwin = Tk_NameToWindow(interp, argv[2], tkwin);
	if (tkwin == NULL) {
	    return TCL_ERROR;
	}
    }
    XBell(Tk_Display(tkwin), 0);
    XForceScreenSaver(Tk_Display(tkwin), ScreenSaverReset);
    XFlush(Tk_Display(tkwin));
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_BindCmd --
 *
 *	This procedure is invoked to process the "bind" Tcl command.
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

int
Tk_BindCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr;
    ClientData object;

    if ((argc < 2) || (argc > 4)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" window ?pattern? ?command?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (argv[1][0] == '.') {
	winPtr = (TkWindow *) Tk_NameToWindow(interp, argv[1], tkwin);
	if (winPtr == NULL) {
	    return TCL_ERROR;
	}
	object = (ClientData) winPtr->pathName;
    } else {
	winPtr = (TkWindow *) clientData;
	object = (ClientData) Tk_GetUid(argv[1]);
    }

    if (argc == 4) {
	int append = 0;
	unsigned long mask;

	if (argv[3][0] == 0) {
	    return Tk_DeleteBinding(interp, winPtr->mainPtr->bindingTable,
		    object, argv[2]);
	}
	if (argv[3][0] == '+') {
	    argv[3]++;
	    append = 1;
	}
	mask = Tk_CreateBinding(interp, winPtr->mainPtr->bindingTable,
		object, argv[2], argv[3], append);
	if (mask == 0) {
	    return TCL_ERROR;
	}
    } else if (argc == 3) {
	char *command;

	command = Tk_GetBinding(interp, winPtr->mainPtr->bindingTable,
		object, argv[2]);
	if (command == NULL) {
	    Tcl_ResetResult(interp);
	    return TCL_OK;
	}
	interp->result = command;
    } else {
	Tk_GetAllBindings(interp, winPtr->mainPtr->bindingTable, object);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkBindEventProc --
 *
 *	This procedure is invoked by Tk_HandleEvent for each event;  it
 *	causes any appropriate bindings for that event to be invoked.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Depends on what bindings have been established with the "bind"
 *	command.
 *
 *----------------------------------------------------------------------
 */

void
TkBindEventProc(winPtr, eventPtr)
    TkWindow *winPtr;			/* Pointer to info about window. */
    XEvent *eventPtr;			/* Information about event. */
{
#define MAX_OBJS 20
    ClientData objects[MAX_OBJS], *objPtr;
    static Tk_Uid allUid = NULL;
    TkWindow *topLevPtr;
    int i, count;
    char *p;
    Tcl_HashEntry *hPtr;

    if ((winPtr->mainPtr == NULL) || (winPtr->mainPtr->bindingTable == NULL)) {
	return;
    }

    objPtr = objects;
    if (winPtr->numTags != 0) {
	/*
	 * Make a copy of the tags for the window, replacing window names
	 * with pointers to the pathName from the appropriate window.
	 */

	if (winPtr->numTags > MAX_OBJS) {
	    objPtr = (ClientData *) ckalloc((unsigned)
		    (winPtr->numTags * sizeof(ClientData)));
	}
	for (i = 0; i < winPtr->numTags; i++) {
	    p = (char *) winPtr->tagPtr[i];
	    if (*p == '.') {
		hPtr = Tcl_FindHashEntry(&winPtr->mainPtr->nameTable, p);
		if (hPtr != NULL) {
		    p = ((TkWindow *) Tcl_GetHashValue(hPtr))->pathName;
		} else {
		    p = NULL;
		}
	    }
	    objPtr[i] = (ClientData) p;
	}
	count = winPtr->numTags;
    } else {
	objPtr[0] = (ClientData) winPtr->pathName;
	objPtr[1] = (ClientData) winPtr->classUid;
	for (topLevPtr = winPtr;
		(topLevPtr != NULL) && !(topLevPtr->flags & TK_TOP_LEVEL);
		topLevPtr = topLevPtr->parentPtr) {
	    /* Empty loop body. */
	}
	if ((winPtr != topLevPtr) && (topLevPtr != NULL)) {
	    count = 4;
	    objPtr[2] = (ClientData) topLevPtr->pathName;
	} else {
	    count = 3;
	}
	if (allUid == NULL) {
	    allUid = Tk_GetUid("all");
	}
	objPtr[count-1] = (ClientData) allUid;
    }
    Tk_BindEvent(winPtr->mainPtr->bindingTable, eventPtr, (Tk_Window) winPtr,
	    count, objPtr);
    if (objPtr != objects) {
	ckfree((char *) objPtr);
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_BindtagsCmd --
 *
 *	This procedure is invoked to process the "bindtags" Tcl command.
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

int
Tk_BindtagsCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr, *winPtr2;
    int i, tagArgc;
    char *p, **tagArgv;

    if ((argc < 2) || (argc > 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" window ?tags?\"", (char *) NULL);
	return TCL_ERROR;
    }
    winPtr = (TkWindow *) Tk_NameToWindow(interp, argv[1], tkwin);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    if (argc == 2) {
	if (winPtr->numTags == 0) {
	    Tcl_AppendElement(interp, winPtr->pathName);
	    Tcl_AppendElement(interp, winPtr->classUid);
	    for (winPtr2 = winPtr;
		    (winPtr2 != NULL) && !(winPtr2->flags & TK_TOP_LEVEL);
		    winPtr2 = winPtr2->parentPtr) {
		/* Empty loop body. */
	    }
	    if ((winPtr != winPtr2) && (winPtr2 != NULL)) {
		Tcl_AppendElement(interp, winPtr2->pathName);
	    }
	    Tcl_AppendElement(interp, "all");
	} else {
	    for (i = 0; i < winPtr->numTags; i++) {
		Tcl_AppendElement(interp, (char *) winPtr->tagPtr[i]);
	    }
	}
	return TCL_OK;
    }
    if (winPtr->tagPtr != NULL) {
	TkFreeBindingTags(winPtr);
    }
    if (argv[2][0] == 0) {
	return TCL_OK;
    }
    if (Tcl_SplitList(interp, argv[2], &tagArgc, &tagArgv) != TCL_OK) {
	return TCL_ERROR;
    }
    winPtr->numTags = tagArgc;
    winPtr->tagPtr = (ClientData *) ckalloc((unsigned)
	    (tagArgc * sizeof(ClientData)));
    for (i = 0; i < tagArgc; i++) {
	p = tagArgv[i];
	if (p[0] == '.') {
	    char *copy;

	    /*
	     * Handle names starting with "." specially: store a malloc'ed
	     * string, rather than a Uid;  at event time we'll look up the
	     * name in the window table and use the corresponding window,
	     * if there is one.
	     */

	    copy = (char *) ckalloc((unsigned) (strlen(p) + 1));
	    strcpy(copy, p);
	    winPtr->tagPtr[i] = (ClientData) copy;
	} else {
	    winPtr->tagPtr[i] = (ClientData) Tk_GetUid(p);
	}
    }
    ckfree((char *) tagArgv);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TkFreeBindingTags --
 *
 *	This procedure is called to free all of the binding tags
 *	associated with a window;  typically it is only invoked where
 *	there are window-specific tags.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Any binding tags for winPtr are freed.
 *
 *----------------------------------------------------------------------
 */

void
TkFreeBindingTags(winPtr)
    TkWindow *winPtr;		/* Window whose tags are to be released. */
{
    int i;
    char *p;

    for (i = 0; i < winPtr->numTags; i++) {
	p = (char *) (winPtr->tagPtr[i]);
	if (*p == '.') {
	    /*
	     * Names starting with "." are malloced rather than Uids, so
	     * they have to be freed.
	     */
    
	    ckfree(p);
	}
    }
    ckfree((char *) winPtr->tagPtr);
    winPtr->numTags = 0;
    winPtr->tagPtr = NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_DestroyCmd --
 *
 *	This procedure is invoked to process the "destroy" Tcl command.
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

int
Tk_DestroyCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window window;
    Tk_Window tkwin = (Tk_Window) clientData;
    int i;

    for (i = 1; i < argc; i++) {
	window = Tk_NameToWindow(interp, argv[i], tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	Tk_DestroyWindow(window);
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_LowerCmd --
 *
 *	This procedure is invoked to process the "lower" Tcl command.
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
Tk_LowerCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window main = (Tk_Window) clientData;
    Tk_Window tkwin, other;

    if ((argc != 2) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " window ?belowThis?\"", (char *) NULL);
	return TCL_ERROR;
    }

    tkwin = Tk_NameToWindow(interp, argv[1], main);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    if (argc == 2) {
	other = NULL;
    } else {
	other = Tk_NameToWindow(interp, argv[2], main);
	if (other == NULL) {
	    return TCL_ERROR;
	}
    }
    if (Tk_RestackWindow(tkwin, Below, other) != TCL_OK) {
	Tcl_AppendResult(interp, "can't lower \"", argv[1], "\" below \"",
		argv[2], "\"", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_RaiseCmd --
 *
 *	This procedure is invoked to process the "raise" Tcl command.
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
Tk_RaiseCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window main = (Tk_Window) clientData;
    Tk_Window tkwin, other;

    if ((argc != 2) && (argc != 3)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " window ?aboveThis?\"", (char *) NULL);
	return TCL_ERROR;
    }

    tkwin = Tk_NameToWindow(interp, argv[1], main);
    if (tkwin == NULL) {
	return TCL_ERROR;
    }
    if (argc == 2) {
	other = NULL;
    } else {
	other = Tk_NameToWindow(interp, argv[2], main);
	if (other == NULL) {
	    return TCL_ERROR;
	}
    }
    if (Tk_RestackWindow(tkwin, Above, other) != TCL_OK) {
	Tcl_AppendResult(interp, "can't raise \"", argv[1], "\" above \"",
		argv[2], "\"", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_TkCmd --
 *
 *	This procedure is invoked to process the "tk" Tcl command.
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
Tk_TkCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    char c;
    size_t length;
    Tk_Window tkwin = (Tk_Window) clientData;
    TkWindow *winPtr;

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strncmp(argv[1], "appname", length) == 0)) {
	winPtr = ((TkWindow *) tkwin)->mainPtr->winPtr;
	if (argc > 3) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		    " appname ?newName?\"", (char *) NULL);
	    return TCL_ERROR;
	}
	if (argc == 3) {
	    winPtr->nameUid = Tk_GetUid(Tk_SetAppName(tkwin, argv[2]));
	}
	interp->result = winPtr->nameUid;
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be appname", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_TkwaitCmd --
 *
 *	This procedure is invoked to process the "tkwait" Tcl command.
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
Tk_TkwaitCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    int c, done;
    size_t length;

    if (argc != 3) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " variable|visibility|window name\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'v') && (strncmp(argv[1], "variable", length) == 0)
	    && (length >= 2)) {
	if (Tcl_TraceVar(interp, argv[2],
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		WaitVariableProc, (ClientData) &done) != TCL_OK) {
	    return TCL_ERROR;
	}
	done = 0;
	while (!done) {
	    Tcl_DoOneEvent(0);
	}
	Tcl_UntraceVar(interp, argv[2],
		TCL_GLOBAL_ONLY|TCL_TRACE_WRITES|TCL_TRACE_UNSETS,
		WaitVariableProc, (ClientData) &done);
    } else if ((c == 'v') && (strncmp(argv[1], "visibility", length) == 0)
	    && (length >= 2)) {
	Tk_Window window;

	window = Tk_NameToWindow(interp, argv[2], tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	Tk_CreateEventHandler(window, VisibilityChangeMask|StructureNotifyMask,
	    WaitVisibilityProc, (ClientData) &done);
	done = 0;
	while (!done) {
	    Tcl_DoOneEvent(0);
	}
	if (done != 1) {
	    /*
	     * Note that we do not delete the event handler because it
	     * was deleted automatically when the window was destroyed.
	     */

	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, "window \"", argv[2],
		    "\" was deleted before its visibility changed",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	Tk_DeleteEventHandler(window, VisibilityChangeMask|StructureNotifyMask,
	    WaitVisibilityProc, (ClientData) &done);
    } else if ((c == 'w') && (strncmp(argv[1], "window", length) == 0)) {
	Tk_Window window;

	window = Tk_NameToWindow(interp, argv[2], tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	Tk_CreateEventHandler(window, StructureNotifyMask,
	    WaitWindowProc, (ClientData) &done);
	done = 0;
	while (!done) {
	    Tcl_DoOneEvent(0);
	}
	/*
	 * Note:  there's no need to delete the event handler.  It was
	 * deleted automatically when the window was destroyed.
	 */
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be variable, visibility, or window", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Clear out the interpreter's result, since it may have been set
     * by event handlers.
     */

    Tcl_ResetResult(interp);
    return TCL_OK;
}

	/* ARGSUSED */
static char *
WaitVariableProc(clientData, interp, name1, name2, flags)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    Tcl_Interp *interp;		/* Interpreter containing variable. */
    char *name1;		/* Name of variable. */
    char *name2;		/* Second part of variable name. */
    int flags;			/* Information about what happened. */
{
    int *donePtr = (int *) clientData;

    *donePtr = 1;
    return (char *) NULL;
}

	/*ARGSUSED*/
static void
WaitVisibilityProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    XEvent *eventPtr;		/* Information about event (not used). */
{
    int *donePtr = (int *) clientData;

    if (eventPtr->type == VisibilityNotify) {
	*donePtr = 1;
    }
    if (eventPtr->type == DestroyNotify) {
	*donePtr = 2;
    }
}

static void
WaitWindowProc(clientData, eventPtr)
    ClientData clientData;	/* Pointer to integer to set to 1. */
    XEvent *eventPtr;		/* Information about event. */
{
    int *donePtr = (int *) clientData;

    if (eventPtr->type == DestroyNotify) {
	*donePtr = 1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_UpdateCmd --
 *
 *	This procedure is invoked to process the "update" Tcl command.
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
Tk_UpdateCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    int flags;
    Display *display;

    if (argc == 1) {
	flags = TCL_DONT_WAIT;
    } else if (argc == 2) {
	if (strncmp(argv[1], "idletasks", strlen(argv[1])) != 0) {
	    Tcl_AppendResult(interp, "bad option \"", argv[1],
		    "\": must be idletasks", (char *) NULL);
	    return TCL_ERROR;
	}
	flags = TCL_IDLE_EVENTS;
    } else {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " ?idletasks?\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Handle all pending events, sync the display, and repeat over
     * and over again until all pending events have been handled.
     * Special note:  it's possible that the entire application could
     * be destroyed by an event handler that occurs during the update.
     * Thus, don't use any information from tkwin after calling
     * Tcl_DoOneEvent.
     */

    display = Tk_Display(tkwin);
    while (1) {
	while (Tcl_DoOneEvent(flags) != 0) {
	    /* Empty loop body */
	}
	XSync(display, False);
	if (Tcl_DoOneEvent(flags) == 0) {
	    break;
	}
    }

    /*
     * Must clear the interpreter's result because event handlers could
     * have executed commands.
     */

    Tcl_ResetResult(interp);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tk_WinfoCmd --
 *
 *	This procedure is invoked to process the "winfo" Tcl command.
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

int
Tk_WinfoCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Main window associated with
				 * interpreter. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tk_Window tkwin = (Tk_Window) clientData;
    size_t length;
    char c, *argName;
    Tk_Window window;
    register TkWindow *winPtr;

#define SETUP(name) \
    if (argc != 3) {\
	argName = name; \
	goto wrongArgs; \
    } \
    window = Tk_NameToWindow(interp, argv[2], tkwin); \
    if (window == NULL) { \
	return TCL_ERROR; \
    }

    if (argc < 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " option ?arg?\"", (char *) NULL);
	return TCL_ERROR;
    }
    c = argv[1][0];
    length = strlen(argv[1]);
    if ((c == 'a') && (strcmp(argv[1], "atom") == 0)) {
	char *atomName;

	if (argc == 3) {
	    atomName = argv[2];
	} else if (argc == 5) {
	    atomName = argv[4];
	    tkwin = GetDisplayOf(interp, tkwin, argv+2);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " atom ?-displayof window? name\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	sprintf(interp->result, "%ld", Tk_InternAtom(tkwin, atomName));
    } else if ((c == 'a') && (strncmp(argv[1], "atomname", length) == 0)
	    && (length >= 5)) {
	Atom atom;
	char *name, *id;

	if (argc == 3) {
	    id = argv[2];
	} else if (argc == 5) {
	    id = argv[4];
	    tkwin = GetDisplayOf(interp, tkwin, argv+2);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " atomname ?-displayof window? id\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, id, (int *) &atom) != TCL_OK) {
	    return TCL_ERROR;
	}
	name = Tk_GetAtomName(tkwin, atom);
	if (strcmp(name, "?bad atom?") == 0) {
	    Tcl_AppendResult(interp, "no atom exists with id \"",
		    argv[2], "\"", (char *) NULL);
	    return TCL_ERROR;
	}
	interp->result = name;
    } else if ((c == 'c') && (strncmp(argv[1], "cells", length) == 0)
	    && (length >= 2)) {
	SETUP("cells");
	sprintf(interp->result, "%d", Tk_Visual(window)->map_entries);
    } else if ((c == 'c') && (strncmp(argv[1], "children", length) == 0)
	    && (length >= 2)) {
	SETUP("children");
	for (winPtr = ((TkWindow *) window)->childList; winPtr != NULL;
		winPtr = winPtr->nextPtr) {
	    Tcl_AppendElement(interp, winPtr->pathName);
	}
    } else if ((c == 'c') && (strncmp(argv[1], "class", length) == 0)
	    && (length >= 2)) {
	SETUP("class");
	interp->result = Tk_Class(window);
    } else if ((c == 'c') && (strncmp(argv[1], "colormapfull", length) == 0)
	    && (length >= 3)) {
	SETUP("colormapfull");
	interp->result = (TkCmapStressed(window, Tk_Colormap(window)))
		? "1" : "0";
    } else if ((c == 'c') && (strncmp(argv[1], "containing", length) == 0)
	    && (length >= 3)) {
	int rootX, rootY, index;

	if (argc == 4) {
	    index = 2;
	} else if (argc == 6) {
	    index = 4;
	    tkwin = GetDisplayOf(interp, tkwin, argv+2);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " containing ?-displayof window? rootX rootY\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if ((Tk_GetPixels(interp, tkwin, argv[index], &rootX) != TCL_OK)
		|| (Tk_GetPixels(interp, tkwin, argv[index+1], &rootY)
		!= TCL_OK)) {
	    return TCL_ERROR;
	}
	window = Tk_CoordsToWindow(rootX, rootY, tkwin);
	if (window != NULL) {
	    interp->result = Tk_PathName(window);
	}
    } else if ((c == 'd') && (strncmp(argv[1], "depth", length) == 0)) {
	SETUP("depth");
	sprintf(interp->result, "%d", Tk_Depth(window));
    } else if ((c == 'e') && (strncmp(argv[1], "exists", length) == 0)) {
	if (argc != 3) {
	    argName = "exists";
	    goto wrongArgs;
	}
	window = Tk_NameToWindow(interp, argv[2], tkwin);
	if ((window == NULL)
		|| (((TkWindow *) window)->flags & TK_ALREADY_DEAD)) {
	    interp->result = "0";
	} else {
	    interp->result = "1";
	}
    } else if ((c == 'f') && (strncmp(argv[1], "fpixels", length) == 0)
	    && (length >= 2)) {
	double mm, pixels;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " fpixels window number\"", (char *) NULL);
	    return TCL_ERROR;
	}
	window = Tk_NameToWindow(interp, argv[2], tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	if (Tk_GetScreenMM(interp, window, argv[3], &mm) != TCL_OK) {
	    return TCL_ERROR;
	}
	pixels = mm * WidthOfScreen(Tk_Screen(window))
		/ WidthMMOfScreen(Tk_Screen(window));
	Tcl_PrintDouble(interp, pixels, interp->result);
    } else if ((c == 'g') && (strncmp(argv[1], "geometry", length) == 0)) {
	SETUP("geometry");
	sprintf(interp->result, "%dx%d+%d+%d", Tk_Width(window),
		Tk_Height(window), Tk_X(window), Tk_Y(window));
    } else if ((c == 'h') && (strncmp(argv[1], "height", length) == 0)) {
	SETUP("height");
	sprintf(interp->result, "%d", Tk_Height(window));
    } else if ((c == 'i') && (strcmp(argv[1], "id") == 0)) {
	SETUP("id");
	Tk_MakeWindowExist(window);
	sprintf(interp->result, "0x%x", (unsigned int) Tk_WindowId(window));
    } else if ((c == 'i') && (strncmp(argv[1], "interps", length) == 0)
	    && (length >= 2)) {
	if (argc == 4) {
	    tkwin = GetDisplayOf(interp, tkwin, argv+2);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	} else if (argc != 2) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " interps ?-displayof window?\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	return TkGetInterpNames(interp, tkwin);
    } else if ((c == 'i') && (strncmp(argv[1], "ismapped", length) == 0)
	    && (length >= 2)) {
	SETUP("ismapped");
	interp->result = Tk_IsMapped(window) ? "1" : "0";
    } else if ((c == 'm') && (strncmp(argv[1], "manager", length) == 0)) {
	SETUP("manager");
	winPtr = (TkWindow *) window;
	if (winPtr->geomMgrPtr != NULL) {
	    interp->result = winPtr->geomMgrPtr->name;
	}
    } else if ((c == 'n') && (strncmp(argv[1], "name", length) == 0)) {
	SETUP("name");
	interp->result = Tk_Name(window);
    } else if ((c == 'p') && (strncmp(argv[1], "parent", length) == 0)) {
	SETUP("parent");
	winPtr = (TkWindow *) window;
	if (winPtr->parentPtr != NULL) {
	    interp->result = winPtr->parentPtr->pathName;
	}
    } else if ((c == 'p') && (strncmp(argv[1], "pathname", length) == 0)
	    && (length >= 2)) {
	int index, id;

	if (argc == 3) {
	    index = 2;
	} else if (argc == 5) {
	    index = 4;
	    tkwin = GetDisplayOf(interp, tkwin, argv+2);
	    if (tkwin == NULL) {
		return TCL_ERROR;
	    }
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " pathname ?-displayof window? id\"",
		    (char *) NULL);
	    return TCL_ERROR;
	}
	if (Tcl_GetInt(interp, argv[index], &id) != TCL_OK) {
	    return TCL_ERROR;
	}
	window = Tk_IdToWindow(Tk_Display(tkwin), (Window) id);
	if ((window == NULL) || (((TkWindow *) window)->mainPtr
		!= ((TkWindow *) tkwin)->mainPtr)) {
	    Tcl_AppendResult(interp, "window id \"", argv[index],
		    "\" doesn't exist in this application", (char *) NULL);
	    return TCL_ERROR;
	}
	interp->result = Tk_PathName(window);
    } else if ((c == 'p') && (strncmp(argv[1], "pixels", length) == 0)
	    && (length >= 2)) {
	int pixels;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " pixels window number\"", (char *) NULL);
	    return TCL_ERROR;
	}
	window = Tk_NameToWindow(interp, argv[2], tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	if (Tk_GetPixels(interp, window, argv[3], &pixels) != TCL_OK) {
	    return TCL_ERROR;
	}
	sprintf(interp->result, "%d", pixels);
    } else if ((c == 'p') && (strcmp(argv[1], "pointerx") == 0)) {
	int x, y;

	SETUP("pointerx");
	winPtr = GetToplevel(window);
	if (winPtr == NULL) {
	    x = -1;
	} else {
	    TkGetPointerCoords((Tk_Window)winPtr, &x, &y);
	}
	sprintf(interp->result, "%d", x);
    } else if ((c == 'p') && (strcmp(argv[1], "pointerxy") == 0)) {
	int x, y;

	SETUP("pointerxy");
	winPtr = GetToplevel(window);
	if (winPtr == NULL) {
	    x = -1;
	} else {
	    TkGetPointerCoords((Tk_Window)winPtr, &x, &y);
	}
	sprintf(interp->result, "%d %d", x, y);
    } else if ((c == 'p') && (strcmp(argv[1], "pointery") == 0)) {
	int x, y;

	SETUP("pointery");
	winPtr = GetToplevel(window);
	if (winPtr == NULL) {
	    y = -1;
	} else {
	    TkGetPointerCoords((Tk_Window)winPtr, &x, &y);
	}
	sprintf(interp->result, "%d", y);
    } else if ((c == 'r') && (strncmp(argv[1], "reqheight", length) == 0)
	    && (length >= 4)) {
	SETUP("reqheight");
	sprintf(interp->result, "%d", Tk_ReqHeight(window));
    } else if ((c == 'r') && (strncmp(argv[1], "reqwidth", length) == 0)
	    && (length >= 4)) {
	SETUP("reqwidth");
	sprintf(interp->result, "%d", Tk_ReqWidth(window));
    } else if ((c == 'r') && (strncmp(argv[1], "rgb", length) == 0)
	    && (length >= 2)) {
	XColor *colorPtr;

	if (argc != 4) {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " rgb window colorName\"", (char *) NULL);
	    return TCL_ERROR;
	}
	window = Tk_NameToWindow(interp, argv[2], tkwin);
	if (window == NULL) {
	    return TCL_ERROR;
	}
	colorPtr = Tk_GetColor(interp, window, argv[3]);
	if (colorPtr == NULL) {
	    return TCL_ERROR;
	}
	sprintf(interp->result, "%d %d %d", colorPtr->red, colorPtr->green,
		colorPtr->blue);
	Tk_FreeColor(colorPtr);
    } else if ((c == 'r') && (strcmp(argv[1], "rootx") == 0)) {
	int x, y;

	SETUP("rootx");
	Tk_GetRootCoords(window, &x, &y);
	sprintf(interp->result, "%d", x);
    } else if ((c == 'r') && (strcmp(argv[1], "rooty") == 0)) {
	int x, y;

	SETUP("rooty");
	Tk_GetRootCoords(window, &x, &y);
	sprintf(interp->result, "%d", y);
    } else if ((c == 's') && (strcmp(argv[1], "screen") == 0)) {
	char string[20];

	SETUP("screen");
	sprintf(string, "%d", Tk_ScreenNumber(window));
	Tcl_AppendResult(interp, Tk_DisplayName(window), ".", string,
		(char *) NULL);
    } else if ((c == 's') && (strncmp(argv[1], "screencells", length) == 0)
	    && (length >= 7)) {
	SETUP("screencells");
	sprintf(interp->result, "%d", CellsOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(argv[1], "screendepth", length) == 0)
	    && (length >= 7)) {
	SETUP("screendepth");
	sprintf(interp->result, "%d", DefaultDepthOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(argv[1], "screenheight", length) == 0)
	    && (length >= 7)) {
	SETUP("screenheight");
	sprintf(interp->result, "%d",  HeightOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(argv[1], "screenmmheight", length) == 0)
	    && (length >= 9)) {
	SETUP("screenmmheight");
	sprintf(interp->result, "%d",  HeightMMOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(argv[1], "screenmmwidth", length) == 0)
	    && (length >= 9)) {
	SETUP("screenmmwidth");
	sprintf(interp->result, "%d",  WidthMMOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(argv[1], "screenvisual", length) == 0)
	    && (length >= 7)) {
	SETUP("screenvisual");
	switch (DefaultVisualOfScreen(Tk_Screen(window))->class) {
	    case PseudoColor:	interp->result = "pseudocolor"; break;
	    case GrayScale:	interp->result = "grayscale"; break;
	    case DirectColor:	interp->result = "directcolor"; break;
	    case TrueColor:	interp->result = "truecolor"; break;
	    case StaticColor:	interp->result = "staticcolor"; break;
	    case StaticGray:	interp->result = "staticgray"; break;
	    default:		interp->result = "unknown"; break;
	}
    } else if ((c == 's') && (strncmp(argv[1], "screenwidth", length) == 0)
	    && (length >= 7)) {
	SETUP("screenwidth");
	sprintf(interp->result, "%d",  WidthOfScreen(Tk_Screen(window)));
    } else if ((c == 's') && (strncmp(argv[1], "server", length) == 0)
	    && (length >= 2)) {
	SETUP("server");
	TkGetServerInfo(interp, window);
    } else if ((c == 't') && (strncmp(argv[1], "toplevel", length) == 0)) {
	SETUP("toplevel");
	winPtr = GetToplevel(window);
	if (winPtr != NULL) {
	    interp->result = winPtr->pathName;
	}
    } else if ((c == 'v') && (strncmp(argv[1], "viewable", length) == 0)
	    && (length >= 3)) {
	SETUP("viewable");
	for (winPtr = (TkWindow *) window; ; winPtr = winPtr->parentPtr) {
	    if ((winPtr == NULL) || !(winPtr->flags & TK_MAPPED)) {
		interp->result = "0";
		break;
	    }
	    if (winPtr->flags & TK_TOP_LEVEL) {
		interp->result = "1";
		break;
	    }
	}
    } else if ((c == 'v') && (strcmp(argv[1], "visual") == 0)) {
	SETUP("visual");
	switch (Tk_Visual(window)->class) {
	    case PseudoColor:	interp->result = "pseudocolor"; break;
	    case GrayScale:	interp->result = "grayscale"; break;
	    case DirectColor:	interp->result = "directcolor"; break;
	    case TrueColor:	interp->result = "truecolor"; break;
	    case StaticColor:	interp->result = "staticcolor"; break;
	    case StaticGray:	interp->result = "staticgray"; break;
	    default:		interp->result = "unknown"; break;
	}
    } else if ((c == 'v') && (strncmp(argv[1], "visualid", length) == 0)
	       && (length >= 7)) {
	SETUP("visualid");
	sprintf(interp->result, "0x%x", (unsigned int)
		XVisualIDFromVisual(Tk_Visual(window)));
    } else if ((c == 'v') && (strncmp(argv[1], "visualsavailable", length) == 0)
	    && (length >= 7)) {
	XVisualInfo template, *visInfoPtr;
	int count, i;
	char string[70], visualIdString[16], *fmt;
	int includeVisualId;

	if (argc == 3) {
	    includeVisualId = 0;
	} else if ((argc == 4)
		&& (strncmp(argv[3], "includeids", strlen(argv[3])) == 0)) {
	    includeVisualId = 1;
	} else {
	    Tcl_AppendResult(interp, "wrong # args: should be \"",
		    argv[0], " visualsavailable window ?includeids?\"", 
		    (char *) NULL);
	    return TCL_ERROR;
	}

	window = Tk_NameToWindow(interp, argv[2], tkwin); 
	if (window == NULL) { 
	  return TCL_ERROR; 
	}

	template.screen = Tk_ScreenNumber(window);
	visInfoPtr = XGetVisualInfo(Tk_Display(window), VisualScreenMask,
		&template, &count);
	if (visInfoPtr == NULL) {
	    interp->result = "can't find any visuals for screen";
	    return TCL_ERROR;
	}
	for (i = 0; i < count; i++) {
	    switch (visInfoPtr[i].class) {
		case PseudoColor:	fmt = "pseudocolor %d"; break;
		case GrayScale:		fmt = "grayscale %d"; break;
		case DirectColor:	fmt = "directcolor %d"; break;
		case TrueColor:		fmt = "truecolor %d"; break;
		case StaticColor:	fmt = "staticcolor %d"; break;
		case StaticGray:	fmt = "staticgray %d"; break;
		default:		fmt = "unknown"; break;
	    }
	    sprintf(string, fmt, visInfoPtr[i].depth);
	    if (includeVisualId) {
		sprintf(visualIdString, " 0x%x",
			(unsigned int) visInfoPtr[i].visualid);
		strcat(string, visualIdString);
	    }
	    Tcl_AppendElement(interp, string);
	}
	XFree((char *) visInfoPtr);
    } else if ((c == 'v') && (strncmp(argv[1], "vrootheight", length) == 0)
	    && (length >= 6)) {
	int x, y;
	int width, height;

	SETUP("vrootheight");
	Tk_GetVRootGeometry(window, &x, &y, &width, &height);
	sprintf(interp->result, "%d", height);
    } else if ((c == 'v') && (strncmp(argv[1], "vrootwidth", length) == 0)
	    && (length >= 6)) {
	int x, y;
	int width, height;

	SETUP("vrootwidth");
	Tk_GetVRootGeometry(window, &x, &y, &width, &height);
	sprintf(interp->result, "%d", width);
    } else if ((c == 'v') && (strcmp(argv[1], "vrootx") == 0)) {
	int x, y;
	int width, height;

	SETUP("vrootx");
	Tk_GetVRootGeometry(window, &x, &y, &width, &height);
	sprintf(interp->result, "%d", x);
    } else if ((c == 'v') && (strcmp(argv[1], "vrooty") == 0)) {
	int x, y;
	int width, height;

	SETUP("vrooty");
	Tk_GetVRootGeometry(window, &x, &y, &width, &height);
	sprintf(interp->result, "%d", y);
    } else if ((c == 'w') && (strncmp(argv[1], "width", length) == 0)) {
	SETUP("width");
	sprintf(interp->result, "%d", Tk_Width(window));
    } else if ((c == 'x') && (argv[1][1] == '\0')) {
	SETUP("x");
	sprintf(interp->result, "%d", Tk_X(window));
    } else if ((c == 'y') && (argv[1][1] == '\0')) {
	SETUP("y");
	sprintf(interp->result, "%d", Tk_Y(window));
    } else {
	Tcl_AppendResult(interp, "bad option \"", argv[1],
		"\": must be atom, atomname, cells, children, ",
		"class, colormapfull, containing, depth, exists, fpixels, ",
		"geometry, height, ",
		"id, interps, ismapped, manager, name, parent, pathname, ",
		"pixels, pointerx, pointerxy, pointery, reqheight, ",
		"reqwidth, rgb, ",
		"rootx, rooty, ",
		"screen, screencells, screendepth, screenheight, ",
		"screenmmheight, screenmmwidth, screenvisual, ",
		"screenwidth, server, ",
		"toplevel, viewable, visual, visualid, visualsavailable, ",
		"vrootheight, vrootwidth, vrootx, vrooty, ",
		"width, x, or y", (char *) NULL);
	return TCL_ERROR;
    }
    return TCL_OK;

    wrongArgs:
    Tcl_AppendResult(interp, "wrong # arguments: must be \"",
	    argv[0], " ", argName, " window\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetDisplayOf --
 *
 *	Parses a "-displayof" option for the "winfo" command.
 *
 * Results:
 *	The return value is a token for the window specified in
 *	argv[1].  If argv[0] and argv[1] couldn't be parsed, NULL
 *	is returned and an error is left in interp->result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Tk_Window
GetDisplayOf(interp, tkwin, argv)
    Tcl_Interp *interp;		/* Interpreter for error reporting. */
    Tk_Window tkwin;		/* Window to use for looking up window
				 * given in argv[1]. */
    char **argv;		/* Array of two strings.   First must be
				 * "-displayof" or an abbreviation, second
				 * must be window name. */
{
    size_t length;

    length = strlen(argv[0]);
    if ((length < 2) || (strncmp(argv[0], "-displayof", length) != 0)) {
	Tcl_AppendResult(interp, "bad argument \"", argv[0],
		"\": must be -displayof", (char *) NULL);
	return (Tk_Window) NULL;
    }
    return Tk_NameToWindow(interp, argv[1], tkwin);
}

/*
 *----------------------------------------------------------------------
 *
 * TkDeadAppCmd --
 *
 *	If an application has been deleted then all Tk commands will be
 *	re-bound to this procedure.
 *
 * Results:
 *	A standard Tcl error is reported to let the user know that
 *	the application is dead.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
int
TkDeadAppCmd(clientData, interp, argc, argv)
    ClientData clientData;	/* Dummy. */
    Tcl_Interp *interp;		/* Current interpreter. */
    int argc;			/* Number of arguments. */
    char **argv;		/* Argument strings. */
{
    Tcl_AppendResult(interp, "can't invoke \"", argv[0],
	    "\" command:  application has been destroyed", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GetToplevel --
 *
 *	Retrieves the toplevel window which is the nearest ancestor of
 *	of the specified window.
 *
 * Results:
 *	Returns the toplevel window or NULL if the window has no
 *	ancestor which is a toplevel.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static TkWindow *
GetToplevel(tkwin)
    Tk_Window tkwin;		/* Window for which the toplevel should be
				 * deterined. */
{
     TkWindow *winPtr = (TkWindow *) tkwin;

     while (!(winPtr->flags & TK_TOP_LEVEL)) {
	 winPtr = winPtr->parentPtr;
	 if (winPtr == NULL) {
	     return NULL;
	 }
     }
     return winPtr;
}

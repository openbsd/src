/* 
 * tkSend.c --
 *
 *	This file provides procedures that implement the "send"
 *	command, allowing commands to be passed from interpreter
 *	to interpreter.
 *
 * Copyright (c) 1989-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkSend.c 1.63 96/03/29 12:37:13
 */

#include "tkPort.h"
#include "tkInt.h"

/* 
 * The following structure is used to keep track of the interpreters
 * registered by this process.
 */

typedef struct RegisteredInterp {
    char *name;			/* Interpreter's name (malloc-ed). */
    Tcl_Interp *interp;		/* Interpreter associated with name.  NULL
				 * means that the application was unregistered
				 * or deleted while a send was in progress
				 * to it. */
    TkDisplay *dispPtr;		/* Display for the application.  Needed
				 * because we may need to unregister the
				 * interpreter after its main window has
				 * been deleted. */
    struct RegisteredInterp *nextPtr;
				/* Next in list of names associated
				 * with interps in this process.
				 * NULL means end of list. */
} RegisteredInterp;

static RegisteredInterp *registry = NULL;
				/* List of all interpreters
				 * registered by this process. */

/*
 * A registry of all interpreters for a display is kept in a
 * property "InterpRegistry" on the root window of the display.
 * It is organized as a series of zero or more concatenated strings
 * (in no particular order), each of the form
 * 	window space name '\0'
 * where "window" is the hex id of the comm. window to use to talk
 * to an interpreter named "name".
 *
 * When the registry is being manipulated by an application (e.g. to
 * add or remove an entry), it is loaded into memory using a structure
 * of the following type:
 */

typedef struct NameRegistry {
    TkDisplay *dispPtr;		/* Display from which the registry was
				 * read. */
    int locked;			/* Non-zero means that the display was
				 * locked when the property was read in. */
    int modified;		/* Non-zero means that the property has
				 * been modified, so it needs to be written
				 * out when the NameRegistry is closed. */
    unsigned long propLength;	/* Length of the property, in bytes. */
    char *property;		/* The contents of the property, or NULL
				 * if none.  See format description above;
				 * this is *not* terminated by the first
				 * null character.  Dynamically allocated. */
    int allocedByX;		/* Non-zero means must free property with
				 * XFree;  zero means use ckfree. */
} NameRegistry;

/*
 * When a result is being awaited from a sent command, one of
 * the following structures is present on a list of all outstanding
 * sent commands.  The information in the structure is used to
 * process the result when it arrives.  You're probably wondering
 * how there could ever be multiple outstanding sent commands.
 * This could happen if interpreters invoke each other recursively.
 * It's unlikely, but possible.
 */

typedef struct PendingCommand {
    int serial;			/* Serial number expected in
				 * result. */
    TkDisplay *dispPtr;		/* Display being used for communication. */
    char *target;		/* Name of interpreter command is
				 * being sent to. */
    Window commWindow;		/* Target's communication window. */
    Tcl_Interp *interp;		/* Interpreter from which the send
				 * was invoked. */
    int code;			/* Tcl return code for command
				 * will be stored here. */
    char *result;		/* String result for command (malloc'ed),
				 * or NULL. */
    char *errorInfo;		/* Information for "errorInfo" variable,
				 * or NULL (malloc'ed). */
    char *errorCode;		/* Information for "errorCode" variable,
				 * or NULL (malloc'ed). */
    int gotResponse;		/* 1 means a response has been received,
				 * 0 means the command is still outstanding. */
    struct PendingCommand *nextPtr;
				/* Next in list of all outstanding
				 * commands.  NULL means end of
				 * list. */
} PendingCommand;

static PendingCommand *pendingCommands = NULL;
				/* List of all commands currently
				 * being waited for. */

/*
 * The information below is used for communication between processes
 * during "send" commands.  Each process keeps a private window, never
 * even mapped, with one property, "Comm".  When a command is sent to
 * an interpreter, the command is appended to the comm property of the
 * communication window associated with the interp's process.  Similarly,
 * when a result is returned from a sent command, it is also appended
 * to the comm property.
 *
 * Each command and each result takes the form of ASCII text.  For a
 * command, the text consists of a zero character followed by several
 * null-terminated ASCII strings.  The first string consists of the
 * single letter "c".  Subsequent strings have the form "option value"
 * where the following options are supported:
 *
 * -r commWindow serial
 *
 *	This option means that a response should be sent to the window
 *	whose X identifier is "commWindow" (in hex), and the response should
 *	be identified with the serial number given by "serial" (in decimal).
 *	If this option isn't specified then the send is asynchronous and
 *	no response is sent.
 *
 * -n name
 *	"Name" gives the name of the application for which the command is
 *	intended.  This option must be present.
 *
 * -s script
 *
 *	"Script" is the script to be executed.  This option must be present.
 *
 * The options may appear in any order.  The -n and -s options must be
 * present, but -r may be omitted for asynchronous RPCs.  For compatibility
 * with future releases that may add new features, there may be additional
 * options present;  as long as they start with a "-" character, they will
 * be ignored.
 *
 * A result also consists of a zero character followed by several null-
 * terminated ASCII strings.  The first string consists of the single
 * letter "r".  Subsequent strings have the form "option value" where
 * the following options are supported:
 *
 * -s serial
 *
 *	Identifies the command for which this is the result.  It is the
 *	same as the "serial" field from the -s option in the command.  This
 *	option must be present.
 *
 * -c code
 *
 *	"Code" is the completion code for the script, in decimal.  If the
 *	code is omitted it defaults to TCL_OK.
 *
 * -r result
 *
 *	"Result" is the result string for the script, which may be either
 *	a result or an error message.  If this field is omitted then it
 *	defaults to an empty string.
 *
 * -i errorInfo
 *
 *	"ErrorInfo" gives a string with which to initialize the errorInfo
 *	variable.  This option may be omitted;  it is ignored unless the
 *	completion code is TCL_ERROR.
 *
 * -e errorCode
 *
 *	"ErrorCode" gives a string with with to initialize the errorCode
 *	variable.  This option may be omitted;  it is ignored  unless the
 *	completion code is TCL_ERROR.
 *
 * Options may appear in any order, and only the -s option must be
 * present.  As with commands, there may be additional options besides
 * these;  unknown options are ignored.
 */

/*
 * The following variable is the serial number that was used in the
 * last "send" command.  It is exported only for testing purposes.
 */

int tkSendSerial = 0;

/*
 * Maximum size property that can be read at one time by
 * this module:
 */

#define MAX_PROP_WORDS 100000

/*
 * The following variable can be set while debugging to do things like
 * skip locking the server.
 */

static int sendDebug = 0;

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		AppendErrorProc _ANSI_ARGS_((ClientData clientData,
				XErrorEvent *errorPtr));
static void		AppendPropCarefully _ANSI_ARGS_((Display *display,
			    Window window, Atom property, char *value,
			    int length, PendingCommand *pendingPtr));
static void		DeleteProc _ANSI_ARGS_((ClientData clientData));
static void		RegAddName _ANSI_ARGS_((NameRegistry *regPtr,
			    char *name, Window commWindow));
static void		RegClose _ANSI_ARGS_((NameRegistry *regPtr));
static void		RegDeleteName _ANSI_ARGS_((NameRegistry *regPtr,
			    char *name));
static Window		RegFindName _ANSI_ARGS_((NameRegistry *regPtr,
			    char *name));
static NameRegistry *	RegOpen _ANSI_ARGS_((Tcl_Interp *interp,
			    TkDisplay *dispPtr, int lock));
static void		SendEventProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static int		SendInit _ANSI_ARGS_((Tcl_Interp *interp,
			    TkDisplay *dispPtr));
static Tk_RestrictAction SendRestrictProc _ANSI_ARGS_((ClientData clientData,
			    XEvent *eventPtr));
static int		ServerSecure _ANSI_ARGS_((TkDisplay *dispPtr));
static void		TimeoutProc _ANSI_ARGS_((ClientData clientData));
static void		UpdateCommWindow _ANSI_ARGS_((TkDisplay *dispPtr));
static int		ValidateName _ANSI_ARGS_((TkDisplay *dispPtr,
			    char *name, Window commWindow, int oldOK));

/*
 *----------------------------------------------------------------------
 *
 * RegOpen --
 *
 *	This procedure loads the name registry for a display into
 *	memory so that it can be manipulated.
 *
 * Results:
 *	The return value is a pointer to the loaded registry.
 *
 * Side effects:
 *	If "lock" is set then the server will be locked.  It is the
 *	caller's responsibility to call RegClose when finished with
 *	the registry, so that we can write back the registry if
 *	neeeded, unlock the server if needed, and free memory.
 *
 *----------------------------------------------------------------------
 */

static NameRegistry *
RegOpen(interp, dispPtr, lock)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting
				 * (errors cause a panic so in fact no
				 * error is ever returned, but the interpreter
				 * is needed anyway). */
    TkDisplay *dispPtr;		/* Display whose name registry is to be
				 * opened. */
    int lock;			/* Non-zero means lock the window server
				 * when opening the registry, so no-one
				 * else can use the registry until we
				 * close it. */
{
    NameRegistry *regPtr;
    int result, actualFormat;
    unsigned long bytesAfter;
    Atom actualType;

    if (dispPtr->commTkwin == NULL) {
	SendInit(interp, dispPtr);
    }

    regPtr = (NameRegistry *) ckalloc(sizeof(NameRegistry));
    regPtr->dispPtr = dispPtr;
    regPtr->locked = 0;
    regPtr->modified = 0;
    regPtr->allocedByX = 1;

    if (lock && !sendDebug) {
	XGrabServer(dispPtr->display);
	regPtr->locked = 1;
    }

    /*
     * Read the registry property.
     */

    result = XGetWindowProperty(dispPtr->display,
	    RootWindow(dispPtr->display, 0),
	    dispPtr->registryProperty, 0, MAX_PROP_WORDS,
	    False, XA_STRING, &actualType, &actualFormat,
	    &regPtr->propLength, &bytesAfter,
	    (unsigned char **) &regPtr->property);

    if (actualType == None) {
	regPtr->propLength = 0;
	regPtr->property = NULL;
    } else if ((result != Success) || (actualFormat != 8)
	    || (actualType != XA_STRING)) {
	/*
	 * The property is improperly formed;  delete it.
	 */

	if (regPtr->property != NULL) {
	    XFree(regPtr->property);
	    regPtr->propLength = 0;
	    regPtr->property = NULL;
	}
	XDeleteProperty(dispPtr->display,
		RootWindow(dispPtr->display, 0),
		dispPtr->registryProperty);
    }

    /*
     * Xlib placed an extra null byte after the end of the property, just
     * to make sure that it is always NULL-terminated.  Be sure to include
     * this byte in our count if it's needed to ensure null termination
     * (note: as of 8/95 I'm no longer sure why this code is needed;  seems
     * like it shouldn't be).
     */

    if ((regPtr->propLength > 0)
	    && (regPtr->property[regPtr->propLength-1] != 0)) {
	regPtr->propLength++;
    }
    return regPtr;
}

/*
 *----------------------------------------------------------------------
 *
 * RegFindName --
 *
 *	Given an open name registry, this procedure finds an entry
 *	with a given name, if there is one, and returns information
 *	about that entry.
 *
 * Results:
 *	The return value is the X identifier for the comm window for
 *	the application named "name", or None if there is no such
 *	entry in the registry.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static Window
RegFindName(regPtr, name)
    NameRegistry *regPtr;	/* Pointer to a registry opened with a
				 * previous call to RegOpen. */
    char *name;			/* Name of an application. */
{
    char *p, *entry;
    Window commWindow;

    commWindow = None;
    for (p = regPtr->property; (p-regPtr->property) < regPtr->propLength; ) {
	entry = p;
	while ((*p != 0) && (!isspace(UCHAR(*p)))) {
	    p++;
	}
	if ((*p != 0) && (strcmp(name, p+1) == 0)) {
	    if (sscanf(entry, "%x", (unsigned int *) &commWindow) == 1) {
		return commWindow;
	    }
	}
	while (*p != 0) {
	    p++;
	}
	p++;
    }
    return None;
}

/*
 *----------------------------------------------------------------------
 *
 * RegDeleteName --
 *
 *	This procedure deletes the entry for a given name from
 *	an open registry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there used to be an entry named "name" in the registry,
 *	then it is deleted and the registry is marked as modified
 *	so it will be written back when closed.
 *
 *----------------------------------------------------------------------
 */

static void
RegDeleteName(regPtr, name)
    NameRegistry *regPtr;	/* Pointer to a registry opened with a
				 * previous call to RegOpen. */
    char *name;			/* Name of an application. */
{
    char *p, *entry, *entryName;
    int count;

    for (p = regPtr->property; (p-regPtr->property) < regPtr->propLength; ) {
	entry = p;
	while ((*p != 0) && (!isspace(UCHAR(*p)))) {
	    p++;
	}
	if (*p != 0) {
	    p++;
	}
	entryName = p;
	while (*p != 0) {
	    p++;
	}
	p++;
	if ((strcmp(name, entryName) == 0)) {
	    /*
	     * Found the matching entry.  Copy everything after it
	     * down on top of it.
	     */

	    count = regPtr->propLength - (p - regPtr->property);
	    if (count > 0)  {
		memmove((VOID *) entry, (VOID *) p, (size_t) count);
	    }
	    regPtr->propLength -=  p - entry;
	    regPtr->modified = 1;
	    return;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * RegAddName --
 *
 *	Add a new entry to an open registry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The open registry is expanded;  it is marked as modified so that
 *	it will be written back when closed.
 *
 *----------------------------------------------------------------------
 */

static void
RegAddName(regPtr, name, commWindow)
    NameRegistry *regPtr;	/* Pointer to a registry opened with a
				 * previous call to RegOpen. */
    char *name;			/* Name of an application.  The caller
				 * must ensure that this name isn't
				 * already registered. */
    Window commWindow;		/* X identifier for comm. window of
				 * application.  */
{
    char id[30];
    char *newProp;
    int idLength, newBytes;

    sprintf(id, "%x ", (unsigned int) commWindow);
    idLength = strlen(id);
    newBytes = idLength + strlen(name) + 1;
    newProp = (char *) ckalloc((unsigned) (regPtr->propLength + newBytes));
    strcpy(newProp, id);
    strcpy(newProp+idLength, name);
    if (regPtr->property != NULL) {
	memcpy((VOID *) (newProp + newBytes), (VOID *) regPtr->property,
		regPtr->propLength);
	if (regPtr->allocedByX) {
	    XFree(regPtr->property);
	} else {
	    ckfree(regPtr->property);
	}
    }
    regPtr->modified = 1;
    regPtr->propLength += newBytes;
    regPtr->property = newProp;
    regPtr->allocedByX = 0;
}

/*
 *----------------------------------------------------------------------
 *
 * RegClose --
 *
 *	This procedure is called to end a series of operations on
 *	a name registry.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The registry is written back if it has been modified, and the
 *	X server is unlocked if it was locked.  Memory for the
 *	registry is freed, so the caller should never use regPtr
 *	again.
 *
 *----------------------------------------------------------------------
 */

static void
RegClose(regPtr)
    NameRegistry *regPtr;	/* Pointer to a registry opened with a
				 * previous call to RegOpen. */
{
    if (regPtr->modified) {
	if (!regPtr->locked && !sendDebug) {
	    panic("The name registry was modified without being locked!");
	}
	XChangeProperty(regPtr->dispPtr->display,
		RootWindow(regPtr->dispPtr->display, 0),
		regPtr->dispPtr->registryProperty, XA_STRING, 8,
		PropModeReplace, (unsigned char *) regPtr->property,
		(int) regPtr->propLength);
    }

    if (regPtr->locked) {
	XUngrabServer(regPtr->dispPtr->display);
    }
    XFlush(regPtr->dispPtr->display);

    if (regPtr->property != NULL) {
	if (regPtr->allocedByX) {
	    XFree(regPtr->property);
	} else {
	    ckfree(regPtr->property);
	}
    }
    ckfree((char *) regPtr);
}

/*
 *----------------------------------------------------------------------
 *
 * ValidateName --
 *
 *	This procedure checks to see if an entry in the registry
 *	is still valid.
 *
 * Results:
 *	The return value is 1 if the given commWindow exists and its
 *	name is "name".  Otherwise 0 is returned.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ValidateName(dispPtr, name, commWindow, oldOK)
    TkDisplay *dispPtr;		/* Display for which to perform the
				 * validation. */
    char *name;			/* The name of an application. */
    Window commWindow;		/* X identifier for the application's
				 * comm. window. */
    int oldOK;			/* Non-zero means that we should consider
				 * an application to be valid even if it
				 * looks like an old-style (pre-4.0) one;
				 * 0 means consider these invalid. */
{
    int result, actualFormat, argc, i;
    unsigned long length, bytesAfter;
    Atom actualType;
    char *property;
    Tk_ErrorHandler handler;
    char **argv;

    property = NULL;

    /*
     * Ignore X errors when reading the property (e.g., the window
     * might not exist).  If an error occurs, result will be some
     * value other than Success.
     */

    handler = Tk_CreateErrorHandler(dispPtr->display, -1, -1, -1,
	    (Tk_ErrorProc *) NULL, (ClientData) NULL);
    result = XGetWindowProperty(dispPtr->display, commWindow,
	    dispPtr->appNameProperty, 0, MAX_PROP_WORDS,
	    False, XA_STRING, &actualType, &actualFormat,
	    &length, &bytesAfter, (unsigned char **) &property);

    if ((result == Success) && (actualType == None)) {
	XWindowAttributes atts;

	/*
	 * The comm. window exists but the property we're looking for
	 * doesn't exist.  This probably means that the application
	 * comes from an older version of Tk (< 4.0) that didn't set the
	 * property;  if this is the case, then assume for compatibility's
	 * sake that everything's OK.  However, it's also possible that
	 * some random application has re-used the window id for something
	 * totally unrelated.  Check a few characteristics of the window,
	 * such as its dimensions and mapped state, to be sure that it
	 * still "smells" like a commWindow.
	 */

	if (!oldOK
		|| !XGetWindowAttributes(dispPtr->display, commWindow, &atts)
		|| (atts.width != 1) || (atts.height != 1)
		|| (atts.map_state != IsUnmapped)) {
	    result = 0;
	} else {
	    result = 1;
	}
    } else if ((result == Success) && (actualFormat == 8)
	   && (actualType == XA_STRING)) {
	result = 0;
	if (Tcl_SplitList((Tcl_Interp *) NULL, property, &argc, &argv)
		== TCL_OK) {
	    for (i = 0; i < argc; i++) {
		if (strcmp(argv[i], name) == 0) {
		    result = 1;
		    break;
		}
	    }
	    ckfree((char *) argv);
	}
    } else {
       result = 0;
    }
    Tk_DeleteErrorHandler(handler);
    if (property != NULL) {
	XFree(property);
    }
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * ServerSecure --
 *
 *	Check whether a server is secure enough for us to trust
 *	Tcl scripts arriving via that server.
 *
 * Results:
 *	The return value is 1 if the server is secure, which means
 *	that host-style authentication is turned on but there are
 *	no hosts in the enabled list.  This means that some other
 *	form of authorization (presumably more secure, such as xauth)
 *	is in use.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
ServerSecure(dispPtr)
    TkDisplay *dispPtr;		/* Display to check. */
{
#ifdef TK_NO_SECURITY
    return 1;
#else
    XHostAddress *addrPtr;
    int numHosts, secure;
    Bool enabled;

    secure = 0;
    addrPtr = XListHosts(dispPtr->display, &numHosts, &enabled);
    if (enabled && (numHosts == 0)) {
	secure = 1;
    }
    if (addrPtr != NULL) {
	XFree((char *) addrPtr);
    }
    return secure;
#endif /* TK_NO_SECURITY */
}

/*
 *--------------------------------------------------------------
 *
 * Tk_SetAppName --
 *
 *	This procedure is called to associate an ASCII name with a Tk
 *	application.  If the application has already been named, the
 *	name replaces the old one.
 *
 * Results:
 *	The return value is the name actually given to the application.
 *	This will normally be the same as name, but if name was already
 *	in use for an application then a name of the form "name #2" will
 *	be chosen,  with a high enough number to make the name unique.
 *
 * Side effects:
 *	Registration info is saved, thereby allowing the "send" command
 *	to be used later to invoke commands in the application.  In
 *	addition, the "send" command is created in the application's
 *	interpreter.  The registration will be removed automatically
 *	if the interpreter is deleted or the "send" command is removed.
 *
 *--------------------------------------------------------------
 */

char *
Tk_SetAppName(tkwin, name)
    Tk_Window tkwin;		/* Token for any window in the application
				 * to be named:  it is just used to identify
				 * the application and the display.  */
    char *name;			/* The name that will be used to
				 * refer to the interpreter in later
				 * "send" commands.  Must be globally
				 * unique. */
{
    RegisteredInterp *riPtr, *riPtr2;
    Window w;
    TkWindow *winPtr = (TkWindow *) tkwin;
    TkDisplay *dispPtr;
    NameRegistry *regPtr;
    Tcl_Interp *interp;
    char *actualName;
    Tcl_DString dString;
    int offset, i;

#ifdef __WIN32__
    return name;
#endif /* __WIN32__ */

    dispPtr = winPtr->dispPtr;
    interp = winPtr->mainPtr->interp;
    if (dispPtr->commTkwin == NULL) {
	SendInit(interp, winPtr->dispPtr);
    }

    /*
     * See if the application is already registered;  if so, remove its
     * current name from the registry.
     */

    regPtr = RegOpen(interp, winPtr->dispPtr, 1);
    for (riPtr = registry; ; riPtr = riPtr->nextPtr) {
	if (riPtr == NULL) {
	    /*
	     * This interpreter isn't currently registered;  create
	     * the data structure that will be used to register it locally,
	     * plus add the "send" command to the interpreter.
	     */

	    riPtr = (RegisteredInterp *) ckalloc(sizeof(RegisteredInterp));
	    riPtr->interp = interp;
	    riPtr->dispPtr = winPtr->dispPtr;
	    riPtr->nextPtr = registry;
	    registry = riPtr;
	    Tcl_CreateCommand(interp, "send", Tk_SendCmd, (ClientData) riPtr,
		    DeleteProc);
	    break;
	}
	if (riPtr->interp == interp) {
	    /*
	     * The interpreter is currently registered;  remove it from
	     * the name registry.
	     */

	    RegDeleteName(regPtr, riPtr->name);
	    ckfree(riPtr->name);
	    break;
	}
    }

    /*
     * Pick a name to use for the application.  Use "name" if it's not
     * already in use.  Otherwise add a suffix such as " #2", trying
     * larger and larger numbers until we eventually find one that is
     * unique.
     */

    actualName = name;
    offset = 0;				/* Needed only to avoid "used before
					 * set" compiler warnings. */
    for (i = 1; ; i++) {
	if (i > 1) {
	    if (i == 2) {
		Tcl_DStringInit(&dString);
		Tcl_DStringAppend(&dString, name, -1);
		Tcl_DStringAppend(&dString, " #", 2);
		offset = Tcl_DStringLength(&dString);
		Tcl_DStringSetLength(&dString, offset+10);
		actualName = Tcl_DStringValue(&dString);
	    }
	    sprintf(actualName + offset, "%d", i);
	}
	w = RegFindName(regPtr, actualName);
	if (w == None) {
	    break;
	}
    
	/*
	 * The name appears to be in use already, but double-check to
	 * be sure (perhaps the application died without removing its
	 * name from the registry?).
	 */

	if (w == Tk_WindowId(dispPtr->commTkwin)) {
	    for (riPtr2 = registry; riPtr2 != NULL; riPtr2 = riPtr2->nextPtr) {
		if ((riPtr2->interp != interp) &&
			(strcmp(riPtr2->name, actualName) == 0)) {
		    goto nextSuffix;
		}
	    }
	    RegDeleteName(regPtr, actualName);
	    break;
	} else if (!ValidateName(winPtr->dispPtr, actualName, w, 1)) {
	    RegDeleteName(regPtr, actualName);
	    break;
	}
	nextSuffix:
	continue;
    }

    /*
     * We've now got a name to use.  Store it in the name registry and
     * in the local entry for this application, plus put it in a property
     * on the commWindow.
     */

    RegAddName(regPtr, actualName, Tk_WindowId(dispPtr->commTkwin));
    RegClose(regPtr);
    riPtr->name = (char *) ckalloc((unsigned) (strlen(actualName) + 1));
    strcpy(riPtr->name, actualName);
    if (actualName != name) {
	Tcl_DStringFree(&dString);
    }
    UpdateCommWindow(dispPtr);

    return riPtr->name;
}

/*
 *--------------------------------------------------------------
 *
 * Tk_SendCmd --
 *
 *	This procedure is invoked to process the "send" Tcl command.
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
Tk_SendCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Information about sender (only
					 * dispPtr field is used). */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    TkWindow *winPtr;
    Window commWindow;
    PendingCommand pending;
    register RegisteredInterp *riPtr;
    char *destName, buffer[30];
    int result, c, async, i, firstArg;
    size_t length;
    Tk_RestrictProc *prevRestrictProc;
    ClientData prevArg;
    TkDisplay *dispPtr;
    NameRegistry *regPtr;
    Tcl_DString request;
    Tcl_Interp *localInterp;		/* Used when the interpreter to
                                         * send the command to is within
                                         * the same process. */

    /*
     * Process options, if any.
     */

    async = 0;
    winPtr = (TkWindow *) Tk_MainWindow(interp);
    if (winPtr == NULL) {
	return TCL_ERROR;
    }
    for (i = 1; i < (argc-1); ) {
	if (argv[i][0] != '-') {
	    break;
	}
	c = argv[i][1];
	length = strlen(argv[i]);
	if ((c == 'a') && (strncmp(argv[i], "-async", length) == 0)) {
	    async = 1;
	    i++;
	} else if ((c == 'd') && (strncmp(argv[i], "-displayof",
		length) == 0)) {
	    winPtr = (TkWindow *) Tk_NameToWindow(interp, argv[i+1],
		    (Tk_Window) winPtr);
	    if (winPtr == NULL) {
		return TCL_ERROR;
	    }
	    i += 2;
	} else if (strcmp(argv[i], "--") == 0) {
	    i++;
	    break;
	} else {
	    Tcl_AppendResult(interp, "bad option \"", argv[i],
		    "\": must be -async, -displayof, or --", (char *) NULL);
	    return TCL_ERROR;
	}
    }

    if (argc < (i+2)) {
	Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
		" ?options? interpName arg ?arg ...?\"", (char *) NULL);
	return TCL_ERROR;
    }
    destName = argv[i];
    firstArg = i+1;

    dispPtr = winPtr->dispPtr;
    if (dispPtr->commTkwin == NULL) {
	SendInit(interp, winPtr->dispPtr);
    }

    /*
     * See if the target interpreter is local.  If so, execute
     * the command directly without going through the X server.
     * The only tricky thing is passing the result from the target
     * interpreter to the invoking interpreter.  Watch out:  they
     * could be the same!
     */

    for (riPtr = registry; riPtr != NULL; riPtr = riPtr->nextPtr) {
	if ((riPtr->dispPtr != dispPtr)
		|| (strcmp(riPtr->name, destName) != 0)) {
	    continue;
	}
	Tcl_Preserve((ClientData) riPtr);
        localInterp = riPtr->interp;
        Tcl_Preserve((ClientData) localInterp);
	if (firstArg == (argc-1)) {
	    result = Tcl_GlobalEval(localInterp, argv[firstArg]);
	} else {
	    Tcl_DStringInit(&request);
	    Tcl_DStringAppend(&request, argv[firstArg], -1);
	    for (i = firstArg+1; i < argc; i++) {
		Tcl_DStringAppend(&request, " ", 1);
		Tcl_DStringAppend(&request, argv[i], -1);
	    }
	    result = Tcl_GlobalEval(localInterp, Tcl_DStringValue(&request));
	    Tcl_DStringFree(&request);
	}
	if (interp != localInterp) {
	    if (result == TCL_ERROR) {

		/*
		 * An error occurred, so transfer error information from the
		 * destination interpreter back to our interpreter.  Must clear
		 * interp's result before calling Tcl_AddErrorInfo, since
		 * Tcl_AddErrorInfo will store the interp's result in errorInfo
		 * before appending riPtr's $errorInfo;  we've already got
		 * everything we need in riPtr's $errorInfo.
		 */

		Tcl_ResetResult(interp);
		Tcl_AddErrorInfo(interp, Tcl_GetVar2(localInterp,
			"errorInfo", (char *) NULL, TCL_GLOBAL_ONLY));
		Tcl_SetVar2(interp, "errorCode", (char *) NULL,
			Tcl_GetVar2(localInterp, "errorCode", (char *) NULL,
			TCL_GLOBAL_ONLY), TCL_GLOBAL_ONLY);
	    }
            if (localInterp->freeProc != TCL_STATIC) {
                interp->result = localInterp->result;
                interp->freeProc = localInterp->freeProc;
                localInterp->freeProc = TCL_STATIC;
            } else {
                Tcl_SetResult(interp, localInterp->result, TCL_VOLATILE);
            }
            Tcl_ResetResult(localInterp);
	}
	Tcl_Release((ClientData) riPtr);
        Tcl_Release((ClientData) localInterp);
	return result;
    }

    /*
     * Bind the interpreter name to a communication window.
     */

    regPtr = RegOpen(interp, winPtr->dispPtr, 0);
    commWindow = RegFindName(regPtr, destName);
    RegClose(regPtr);
    if (commWindow == None) {
	Tcl_AppendResult(interp, "no application named \"",
		destName, "\"", (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * Send the command to the target interpreter by appending it to the
     * comm window in the communication window.
     */

    tkSendSerial++;
    Tcl_DStringInit(&request);
    Tcl_DStringAppend(&request, "\0c\0-n ", 6);
    Tcl_DStringAppend(&request, destName, -1);
    if (!async) {
	sprintf(buffer, "%x %d",
		(unsigned int) Tk_WindowId(dispPtr->commTkwin),
		tkSendSerial);
	Tcl_DStringAppend(&request, "\0-r ", 4);
	Tcl_DStringAppend(&request, buffer, -1);
    }
    Tcl_DStringAppend(&request, "\0-s ", 4);
    Tcl_DStringAppend(&request, argv[firstArg], -1);
    for (i = firstArg+1; i < argc; i++) {
	Tcl_DStringAppend(&request, " ", 1);
	Tcl_DStringAppend(&request, argv[i], -1);
    }
    (void) AppendPropCarefully(dispPtr->display, commWindow,
	    dispPtr->commProperty, Tcl_DStringValue(&request),
	    Tcl_DStringLength(&request) + 1,
	    (async) ? (PendingCommand *) NULL : &pending);
    Tcl_DStringFree(&request);
    if (async) {
	/*
	 * This is an asynchronous send:  return immediately without
	 * waiting for a response.
	 */

	return TCL_OK;
    }

    /*
     * Register the fact that we're waiting for a command to complete
     * (this is needed by SendEventProc and by AppendErrorProc to pass
     * back the command's results).  Set up a timeout handler so that
     * we can check during long sends to make sure that the destination
     * application is still alive.
     */

    pending.serial = tkSendSerial;
    pending.dispPtr = dispPtr;
    pending.target = destName;
    pending.commWindow = commWindow;
    pending.interp = interp;
    pending.result = NULL;
    pending.errorInfo = NULL;
    pending.errorCode = NULL;
    pending.gotResponse = 0;
    pending.nextPtr = pendingCommands;
    pendingCommands = &pending;

    /*
     * Enter a loop processing X events until the result comes
     * in or the target is declared to be dead.  While waiting
     * for a result, look only at send-related events so that
     * the send is synchronous with respect to other events in
     * the application.
     */

    prevRestrictProc = Tk_RestrictEvents(SendRestrictProc,
	    (ClientData) dispPtr->commTkwin, &prevArg);
    Tcl_CreateModalTimeout(1000, TimeoutProc, (ClientData) &pending);
    while (!pending.gotResponse) {
	Tcl_DoOneEvent(TCL_WINDOW_EVENTS);
    }
    Tcl_DeleteModalTimeout(TimeoutProc, (ClientData) &pending);
    (void) Tk_RestrictEvents(prevRestrictProc, prevArg, &prevArg);

    /*
     * Unregister the information about the pending command
     * and return the result.
     */

    if (pendingCommands == &pending) {
	pendingCommands = pending.nextPtr;
    } else {
	PendingCommand *pcPtr;

	for (pcPtr = pendingCommands; pcPtr != NULL;
		pcPtr = pcPtr->nextPtr) {
	    if (pcPtr->nextPtr == &pending) {
		pcPtr->nextPtr = pending.nextPtr;
		break;
	    }
	}
    }
    if (pending.errorInfo != NULL) {
	/*
	 * Special trick: must clear the interp's result before calling
	 * Tcl_AddErrorInfo, since Tcl_AddErrorInfo will store the interp's
	 * result in errorInfo before appending pending.errorInfo;  we've
	 * already got everything we need in pending.errorInfo.
	 */

	Tcl_ResetResult(interp);
	Tcl_AddErrorInfo(interp, pending.errorInfo);
	ckfree(pending.errorInfo);
    }
    if (pending.errorCode != NULL) {
	Tcl_SetVar2(interp, "errorCode", (char *) NULL, pending.errorCode,
		TCL_GLOBAL_ONLY);
	ckfree(pending.errorCode);
    }
    Tcl_SetResult(interp, pending.result, TCL_DYNAMIC);
    return pending.code;
}

/*
 *----------------------------------------------------------------------
 *
 * TkGetInterpNames --
 *
 *	This procedure is invoked to fetch a list of all the
 *	interpreter names currently registered for the display
 *	of a particular window.
 *
 * Results:
 *	A standard Tcl return value.  Interp->result will be set
 *	to hold a list of all the interpreter names defined for
 *	tkwin's display.  If an error occurs, then TCL_ERROR
 *	is returned and interp->result will hold an error message.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TkGetInterpNames(interp, tkwin)
    Tcl_Interp *interp;		/* Interpreter for returning a result. */
    Tk_Window tkwin;		/* Window whose display is to be used
				 * for the lookup. */
{
    TkWindow *winPtr = (TkWindow *) tkwin;
    char *p, *entry, *entryName;
    NameRegistry *regPtr;
    Window commWindow;
    int count;

    /*
     * Read the registry property, then scan through all of its entries.
     * Validate each entry to be sure that its application still exists.
     */

    regPtr = RegOpen(interp, winPtr->dispPtr, 1);
    for (p = regPtr->property; (p-regPtr->property) < regPtr->propLength; ) {
	entry = p;
	if (sscanf(p,  "%x",(unsigned int *) &commWindow) != 1) {
	    commWindow =  None;
	}
	while ((*p != 0) && (!isspace(UCHAR(*p)))) {
	    p++;
	}
	if (*p != 0) {
	    p++;
	}
	entryName = p;
	while (*p != 0) {
	    p++;
	}
	p++;
	if (ValidateName(winPtr->dispPtr, entryName, commWindow, 1)) {
	    /*
	     * The application still exists; add its name to the result.
	     */

	    Tcl_AppendElement(interp, entryName);
	} else {
	    /*
	     * This name is bogus (perhaps the application died without
	     * cleaning up its entry in the registry?).  Delete the name.
	     */

	    count = regPtr->propLength - (p - regPtr->property);
	    if (count > 0)  {
		memmove((VOID *) entry, (VOID *) p, (size_t) count);
	    }
	    regPtr->propLength -= p - entry;
	    regPtr->modified = 1;
	    p = entry;
	}
    }
    RegClose(regPtr);
    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * SendInit --
 *
 *	This procedure is called to initialize the
 *	communication channels for sending commands and
 *	receiving results.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Sets up various data structures and windows.
 *
 *--------------------------------------------------------------
 */

static int
SendInit(interp, dispPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting
				 * (no errors are ever returned, but the
				 * interpreter is needed anyway). */
    TkDisplay *dispPtr;		/* Display to initialize. */
{
    XSetWindowAttributes atts;

    /*
     * Create the window used for communication, and set up an
     * event handler for it.
     */

    dispPtr->commTkwin = Tk_CreateWindow(interp, (Tk_Window) NULL,
	    "_comm", DisplayString(dispPtr->display));
    if (dispPtr->commTkwin == NULL) {
	panic("Tk_CreateWindow failed in SendInit!");
    }
    atts.override_redirect = True;
    Tk_ChangeWindowAttributes(dispPtr->commTkwin,
	    CWOverrideRedirect, &atts);
    Tk_CreateEventHandler(dispPtr->commTkwin, PropertyChangeMask,
	    SendEventProc, (ClientData) dispPtr);
    Tk_MakeWindowExist(dispPtr->commTkwin);

    /*
     * Get atoms used as property names.
     */

    dispPtr->commProperty = Tk_InternAtom(dispPtr->commTkwin, "Comm");
    dispPtr->registryProperty = Tk_InternAtom(dispPtr->commTkwin,
	    "InterpRegistry");
    dispPtr->appNameProperty = Tk_InternAtom(dispPtr->commTkwin,
	    "TK_APPLICATION");

    return TCL_OK;
}

/*
 *--------------------------------------------------------------
 *
 * SendEventProc --
 *
 *	This procedure is invoked automatically by the toolkit
 *	event manager when a property changes on the communication
 *	window.  This procedure reads the property and handles
 *	command requests and responses.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If there are command requests in the property, they
 *	are executed.  If there are responses in the property,
 *	their information is saved for the (ostensibly waiting)
 *	"send" commands. The property is deleted.
 *
 *--------------------------------------------------------------
 */

static void
SendEventProc(clientData, eventPtr)
    ClientData clientData;	/* Display information. */	
    XEvent *eventPtr;		/* Information about event. */
{
    TkDisplay *dispPtr = (TkDisplay *) clientData;
    char *propInfo;
    register char *p;
    int result, actualFormat;
    unsigned long numItems, bytesAfter;
    Atom actualType;
    Tcl_Interp *remoteInterp;	/* Interp in which to execute the command. */

    if ((eventPtr->xproperty.atom != dispPtr->commProperty)
	    || (eventPtr->xproperty.state != PropertyNewValue)) {
	return;
    }

    /*
     * Read the comm property and delete it.
     */

    propInfo = NULL;
    result = XGetWindowProperty(dispPtr->display,
	    Tk_WindowId(dispPtr->commTkwin),
	    dispPtr->commProperty, 0, MAX_PROP_WORDS, True,
	    XA_STRING, &actualType, &actualFormat,
	    &numItems, &bytesAfter, (unsigned char **) &propInfo);

    /*
     * If the property doesn't exist or is improperly formed
     * then ignore it.
     */

    if ((result != Success) || (actualType != XA_STRING)
	    || (actualFormat != 8)) {
	if (propInfo != NULL) {
	    XFree(propInfo);
	}
	return;
    }

    /*
     * Several commands and results could arrive in the property at
     * one time;  each iteration through the outer loop handles a
     * single command or result.
     */

    for (p = propInfo; (p-propInfo) < numItems; ) {
	/*
	 * Ignore leading NULLs; each command or result starts with a
	 * NULL so that no matter how badly formed a preceding command
	 * is, we'll be able to tell that a new command/result is
	 * starting.
	 */

	if (*p == 0) {
	    p++;
	    continue;
	}

	if ((*p == 'c') && (p[1] == 0)) {
	    Window commWindow;
	    char *interpName, *script, *serial, *end;
	    Tcl_DString reply;
	    RegisteredInterp *riPtr;

	    /*
	     *----------------------------------------------------------
	     * This is an incoming command from some other application.
	     * Iterate over all of its options.  Stop when we reach
	     * the end of the property or something that doesn't look
	     * like an option.
	     *----------------------------------------------------------
	     */

	    p += 2;
	    interpName = NULL;
	    commWindow = None;
	    serial = "";
	    script = NULL;
	    while (((p-propInfo) < numItems) && (*p == '-')) {
		switch (p[1]) {
		    case 'r':
			commWindow = (Window) strtoul(p+2, &end, 16);
			if ((end == p+2) || (*end != ' ')) {
			    commWindow = None;
			} else {
			    p = serial = end+1;
			}
			break;
		    case 'n':
			if (p[2] == ' ') {
			    interpName = p+3;
			}
			break;
		    case 's':
			if (p[2] == ' ') {
			    script = p+3;
			}
			break;
		}
		while (*p != 0) {
		    p++;
		}
		p++;
	    }

	    if ((script == NULL) || (interpName == NULL)) {
		continue;
	    }

	    /*
	     * Initialize the result property, so that we're ready at any
	     * time if we need to return an error.
	     */

	    if (commWindow != None) {
		Tcl_DStringInit(&reply);
		Tcl_DStringAppend(&reply, "\0r\0-s ", 6);
		Tcl_DStringAppend(&reply, serial, -1);
		Tcl_DStringAppend(&reply, "\0-r ", 4);
	    }

	    if (!ServerSecure(dispPtr)) {
		if (commWindow != None) {
		    Tcl_DStringAppend(&reply, "X server insecure (must use xauth-style authorization); command ignored", -1);
		}
		result = TCL_ERROR;
		goto returnResult;
	    }

	    /*
	     * Locate the application, then execute the script.
	     */

	    for (riPtr = registry; ; riPtr = riPtr->nextPtr) {
		if (riPtr == NULL) {
		    if (commWindow != None) {
			Tcl_DStringAppend(&reply,
				"receiver never heard of interpreter \"", -1);
			Tcl_DStringAppend(&reply, interpName, -1);
			Tcl_DStringAppend(&reply, "\"", 1);
		    }
		    result = TCL_ERROR;
		    goto returnResult;
		}
		if (strcmp(riPtr->name, interpName) == 0) {
		    break;
		}
	    }
	    Tcl_Preserve((ClientData) riPtr);

            /*
             * We must protect the interpreter because the script may
             * enter another event loop, which might call Tcl_DeleteInterp.
             */

            remoteInterp = riPtr->interp;
            Tcl_Preserve((ClientData) remoteInterp);

            result = Tcl_GlobalEval(remoteInterp, script);

            /*
             * The call to Tcl_Release may have released the interpreter
             * which will cause the "send" command for that interpreter
             * to be deleted. The command deletion callback will set the
             * riPtr->interp field to NULL, hence the check below for NULL.
             */

	    if (commWindow != None) {
		Tcl_DStringAppend(&reply, remoteInterp->result, -1);
		if (result == TCL_ERROR) {
		    char *varValue;
    
		    varValue = Tcl_GetVar2(remoteInterp, "errorInfo",
			    (char *) NULL, TCL_GLOBAL_ONLY);
		    if (varValue != NULL) {
			Tcl_DStringAppend(&reply, "\0-i ", 4);
			Tcl_DStringAppend(&reply, varValue, -1);
		    }
		    varValue = Tcl_GetVar2(remoteInterp, "errorCode",
			    (char *) NULL, TCL_GLOBAL_ONLY);
		    if (varValue != NULL) {
			Tcl_DStringAppend(&reply, "\0-e ", 4);
			Tcl_DStringAppend(&reply, varValue, -1);
		    }
		}
	    }
            Tcl_Release((ClientData) remoteInterp);
	    Tcl_Release((ClientData) riPtr);

	    /*
	     * Return the result to the sender if a commWindow was
	     * specified (if none was specified then this is an asynchronous
	     * call).  Right now reply has everything but the completion
	     * code, but it needs the NULL to terminate the current option.
	     */

	    returnResult:
	    if (commWindow != None) {
		if (result != TCL_OK) {
		    char buffer[20];
    
		    sprintf(buffer, "%d", result);
		    Tcl_DStringAppend(&reply, "\0-c ", 4);
		    Tcl_DStringAppend(&reply, buffer, -1);
		}
		(void) AppendPropCarefully(dispPtr->display, commWindow,
			dispPtr->commProperty, Tcl_DStringValue(&reply),
			Tcl_DStringLength(&reply) + 1,
			(PendingCommand *) NULL);
		XFlush(dispPtr->display);
		Tcl_DStringFree(&reply);
	    }
	} else if ((*p == 'r') && (p[1] == 0)) {
	    int serial, code, gotSerial;
	    char *errorInfo, *errorCode, *resultString;
	    PendingCommand *pcPtr;

	    /*
	     *----------------------------------------------------------
	     * This is a reply to some command that we sent out.  Iterate
	     * over all of its options.  Stop when we reach the end of the
	     * property or something that doesn't look like an option.
	     *----------------------------------------------------------
	     */

	    p += 2;
	    code = TCL_OK;
	    gotSerial = 0;
	    errorInfo = NULL;
	    errorCode = NULL;
	    resultString = "";
	    while (((p-propInfo) < numItems) && (*p == '-')) {
		switch (p[1]) {
		    case 'c':
			if (sscanf(p+2, " %d", &code) != 1) {
			    code = TCL_OK;
			}
			break;
		    case 'e':
			if (p[2] == ' ') {
			    errorCode = p+3;
			}
			break;
		    case 'i':
			if (p[2] == ' ') {
			    errorInfo = p+3;
			}
			break;
		    case 'r':
			if (p[2] == ' ') {
			    resultString = p+3;
			}
			break;
		    case 's':
			if (sscanf(p+2, " %d", &serial) == 1) {
			    gotSerial = 1;
			}
			break;
		}
		while (*p != 0) {
		    p++;
		}
		p++;
	    }

	    if (!gotSerial) {
		continue;
	    }

	    /*
	     * Give the result information to anyone who's
	     * waiting for it.
	     */

	    for (pcPtr = pendingCommands; pcPtr != NULL;
		    pcPtr = pcPtr->nextPtr) {
		if ((serial != pcPtr->serial) || (pcPtr->result != NULL)) {
		    continue;
		}
		pcPtr->code = code;
		if (resultString != NULL) {
		    pcPtr->result = (char *) ckalloc((unsigned)
			    (strlen(resultString) + 1));
		    strcpy(pcPtr->result, resultString);
		}
		if (code == TCL_ERROR) {
		    if (errorInfo != NULL) {
			pcPtr->errorInfo = (char *) ckalloc((unsigned)
				(strlen(errorInfo) + 1));
			strcpy(pcPtr->errorInfo, errorInfo);
		    }
		    if (errorCode != NULL) {
			pcPtr->errorCode = (char *) ckalloc((unsigned)
				(strlen(errorCode) + 1));
			strcpy(pcPtr->errorCode, errorCode);
		    }
		}
		pcPtr->gotResponse = 1;
		break;
	    }
	} else {
	    /*
	     * Didn't recognize this thing.  Just skip through the next
	     * null character and try again.
	     */

	    while (*p != 0) {
		p++;
	    }
	    p++;
	}
    }
    XFree(propInfo);
}

/*
 *--------------------------------------------------------------
 *
 * AppendPropCarefully --
 *
 *	Append a given property to a given window, but set up
 *	an X error handler so that if the append fails this
 *	procedure can return an error code rather than having
 *	Xlib panic.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The given property on the given window is appended to.
 *	If this operation fails and if pendingPtr is non-NULL,
 *	then the pending operation is marked as complete with
 *	an error.
 *
 *--------------------------------------------------------------
 */

static void
AppendPropCarefully(display, window, property, value, length, pendingPtr)
    Display *display;		/* Display on which to operate. */
    Window window;		/* Window whose property is to
				 * be modified. */
    Atom property;		/* Name of property. */
    char *value;		/* Characters to append to property. */
    int length;			/* Number of bytes to append. */
    PendingCommand *pendingPtr;	/* Pending command to mark complete
				 * if an error occurs during the
				 * property op.  NULL means just
				 * ignore the error. */
{
    Tk_ErrorHandler handler;

    handler = Tk_CreateErrorHandler(display, -1, -1, -1, AppendErrorProc,
	(ClientData) pendingPtr);
    XChangeProperty(display, window, property, XA_STRING, 8,
	    PropModeAppend, (unsigned char *) value, length);
    Tk_DeleteErrorHandler(handler);
}

/*
 * The procedure below is invoked if an error occurs during
 * the XChangeProperty operation above.
 */

	/* ARGSUSED */
static int
AppendErrorProc(clientData, errorPtr)
    ClientData clientData;	/* Command to mark complete, or NULL. */
    XErrorEvent *errorPtr;	/* Information about error. */
{
    PendingCommand *pendingPtr = (PendingCommand *) clientData;
    register PendingCommand *pcPtr;

    if (pendingPtr == NULL) {
	return 0;
    }

    /*
     * Make sure this command is still pending.
     */

    for (pcPtr = pendingCommands; pcPtr != NULL;
	    pcPtr = pcPtr->nextPtr) {
	if ((pcPtr == pendingPtr) && (pcPtr->result == NULL)) {
	    pcPtr->result = (char *) ckalloc((unsigned)
		    (strlen(pcPtr->target) + 50));
	    sprintf(pcPtr->result, "no application named \"%s\"",
		    pcPtr->target);
	    pcPtr->code = TCL_ERROR;
	    pcPtr->gotResponse = 1;
	    break;
	}
    }
    return 0;
}

/*
 *--------------------------------------------------------------
 *
 * TimeoutProc --
 *
 *	This procedure is invoked when an unusually long amout of
 *	time has elapsed during the processing of a sent command.
 *	It checks to make sure that the target application still
 *	exists, and reschedules itself to check again later.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	If the target application has gone away abort the send
 *	operation with an error.
 *
 *--------------------------------------------------------------
 */

static void
TimeoutProc(clientData)
    ClientData clientData;	/* Information about command that
				 * has been sent but not yet
				 * responded to. */
{
    PendingCommand *pcPtr = (PendingCommand *) clientData;
    register PendingCommand *pcPtr2;

    /*
     * Make sure that the command is still in the pending list
     * and that it hasn't already completed.  Then validate the
     * existence of the target application.
     */

    for (pcPtr2 = pendingCommands; pcPtr2 != NULL;
	    pcPtr2 = pcPtr2->nextPtr) {
	char *msg;
	if ((pcPtr2 != pcPtr) || (pcPtr2->result != NULL)) {
	    continue;
	}
	if (!ValidateName(pcPtr2->dispPtr, pcPtr2->target,
		pcPtr2->commWindow, 0)) {
	    if (ValidateName(pcPtr2->dispPtr, pcPtr2->target,
		    pcPtr2->commWindow, 1)) {
		msg =
                    "target application died or uses a Tk version before 4.0";
	    } else {
		msg = "target application died";
	    }
	    pcPtr2->code = TCL_ERROR;
	    pcPtr2->result = (char *) ckalloc((unsigned) (strlen(msg) + 1));
	    strcpy(pcPtr2->result, msg);
	    pcPtr2->gotResponse = 1;
	} else {
	    Tcl_DeleteModalTimeout(TimeoutProc, clientData);
	    Tcl_CreateModalTimeout(2000, TimeoutProc, clientData);
	}
    }
}

/*
 *--------------------------------------------------------------
 *
 * DeleteProc --
 *
 *	This procedure is invoked by Tcl when the "send" command
 *	is deleted in an interpreter.  It unregisters the interpreter.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The interpreter given by riPtr is unregistered.
 *
 *--------------------------------------------------------------
 */

static void
DeleteProc(clientData)
    ClientData clientData;	/* Info about registration, passed
				 * as ClientData. */
{
    RegisteredInterp *riPtr = (RegisteredInterp *) clientData;
    register RegisteredInterp *riPtr2;
    NameRegistry *regPtr;

    regPtr = RegOpen(riPtr->interp, riPtr->dispPtr, 1);
    RegDeleteName(regPtr, riPtr->name);
    RegClose(regPtr);

    if (registry == riPtr) {
	registry = riPtr->nextPtr;
    } else {
	for (riPtr2 = registry; riPtr2 != NULL;
		riPtr2 = riPtr2->nextPtr) {
	    if (riPtr2->nextPtr == riPtr) {
		riPtr2->nextPtr = riPtr->nextPtr;
		break;
	    }
	}
    }
    ckfree((char *) riPtr->name);
    riPtr->interp = NULL;
    UpdateCommWindow(riPtr->dispPtr);
    Tcl_EventuallyFree((ClientData) riPtr, TCL_DYNAMIC);
}

/*
 *----------------------------------------------------------------------
 *
 * SendRestrictProc --
 *
 *	This procedure filters incoming events when a "send" command
 *	is outstanding.  It defers all events except those containing
 *	send commands and results.
 *
 * Results:
 *	False is returned except for property-change events on the
 *	given commWindow.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

    /* ARGSUSED */
static Tk_RestrictAction
SendRestrictProc(arg, eventPtr)
    ClientData arg;		/* Comunication window in which
				 * we're interested. */
    register XEvent *eventPtr;	/* Event that just arrived. */
{
    register Tk_Window comm = (Tk_Window) arg;

    if ((eventPtr->xany.display != Tk_Display(comm))
	    || (eventPtr->type != PropertyNotify)
	    || (eventPtr->xproperty.window != Tk_WindowId(comm))) {
	return TK_DEFER_EVENT;
    }
    return TK_PROCESS_EVENT;
}

/*
 *----------------------------------------------------------------------
 *
 * UpdateCommWindow --
 *
 *	This procedure updates the list of application names stored
 *	on our commWindow.  It is typically called when interpreters
 *	are registered and unregistered.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The TK_APPLICATION property on the comm window is updated.
 *
 *----------------------------------------------------------------------
 */

static void
UpdateCommWindow(dispPtr)
    TkDisplay *dispPtr;		/* Display whose commWindow is to be
				 * updated. */
{
    Tcl_DString names;
    RegisteredInterp *riPtr;

    Tcl_DStringInit(&names);
    for (riPtr = registry; riPtr != NULL; riPtr = riPtr->nextPtr) {
	Tcl_DStringAppendElement(&names, riPtr->name);
    }
    XChangeProperty(dispPtr->display, Tk_WindowId(dispPtr->commTkwin),
	    dispPtr->appNameProperty, XA_STRING, 8, PropModeReplace,
	    (unsigned char *) Tcl_DStringValue(&names),
	    Tcl_DStringLength(&names));
    Tcl_DStringFree(&names);
}

/* 
 * tkMacSend.c --
 *
 *	This file provides procedures that implement the "send"
 *	command, allowing commands to be passed from interpreter
 *	to interpreter.  This current implementation for the Mac
 *	has most functionality stubed out.
 *
 * Copyright (c) 1989-1994 The Regents of the University of California.
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacSend.c 1.6 96/02/15 18:55:59
 */

#include "tkPort.h"
#include "tkInt.h"

     /* 
      * The following structure is used to keep track of the
      * interpreters registered by this process.
      */

typedef struct RegisteredInterp {
    char *name;			/* Interpreter's name (malloc-ed). */
    Tcl_Interp *interp;		/* Interpreter associated with
				 * name. */
    TkWindow *winPtr;		/* Main window for the application. */
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
    char *property;		/* The contents of the property.  See format
				 * above;  this is *not* terminated by the
				 * first null character.  Dynamically
				 * allocated. */
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
    Tk_TimerToken timeout;	/* Token for timer handler used to check
				 * up on target during long sends. */
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
			     TkWindow *winPtr, int lock));
static void		SendEventProc _ANSI_ARGS_((ClientData clientData,
							   XEvent *eventPtr));
static int		SendInit _ANSI_ARGS_((Tcl_Interp *interp,
			      TkWindow *winPtr));
static Bool		SendRestrictProc _ANSI_ARGS_((Display *display,
			      XEvent *eventPtr, char *arg));
static int		ServerSecure _ANSI_ARGS_((TkDisplay *dispPtr));
static void		TimeoutProc _ANSI_ARGS_((ClientData clientData));
static int		ValidateName _ANSI_ARGS_((TkDisplay *dispPtr,
			     char *name, Window commWindow, int oldOK));

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
    char *name;       		/* The name that will be used to
				 * refer to the interpreter in later
				 * "send" commands.  Must be globally
				 * unique. */
{
    return name;
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
    Tcl_SetResult(interp, "Send not yet implemented", TCL_STATIC);
    return TCL_ERROR;
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
    Tcl_SetResult(interp, "Send not yet implemented", TCL_STATIC);
    return TCL_ERROR;
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
SendInit(interp, winPtr)
    Tcl_Interp *interp;		/* Interpreter to use for error reporting
				 * (no errors are ever returned, but the
				 * interpreter is needed anyway). */
    TkWindow *winPtr;		/* Window that identifies the display to
				 * initialize. */
{
    return TCL_OK;
}

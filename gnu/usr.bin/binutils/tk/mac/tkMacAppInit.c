/* 
 * tkMacAppInit.c --
 *
 *	Provides a version of the Tcl_AppInit procedure for the example shell.
 *
 * Copyright (c) 1993-1994 Lockheed Missle & Space Company, AI Center
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tkMacAppInit.c 1.19 96/04/09 23:09:13
 */

#include <Windows.h>
#include <Gestalt.h>
#include <ToolUtils.h>
#include <Fonts.h>
#include <Dialogs.h>
#include <Memory.h>
#include <SegLoad.h>

#include "tk.h"
#include "tkInt.h"
#include "tkMacInt.h"

#ifdef TK_TEST
EXTERN int		Tktest_Init _ANSI_ARGS_((Tcl_Interp *interp));
#endif /* TK_TEST */

typedef int (*TclMacConvertEventPtr) _ANSI_ARGS_((EventRecord *eventPtr));
Tcl_Interp *gStdoutInterp = NULL;

void 	TclMacSetEventProc _ANSI_ARGS_((TclMacConvertEventPtr procPtr));
int 	TkMacConvertEvent _ANSI_ARGS_((EventRecord *eventPtr));

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		MacintoshInit _ANSI_ARGS_((void));
static int		SetupMainInterp _ANSI_ARGS_((Tcl_Interp *interp));

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	Main program for Wish.
 *
 * Results:
 *	None. This procedure never returns (it exits the process when
 *	it's done
 *
 * Side effects:
 *	This procedure initializes the wish world and then 
 *	calls Tk_Main.
 *
 *----------------------------------------------------------------------
 */

void
main(argc, argv)
    int argc;				/* Number of arguments. */
    char **argv;			/* Array of argument strings. */
{
    char *newArgv[2];

    if (MacintoshInit()  != TCL_OK) {
	Tcl_Exit(1);
    }

    argc = 1;
    newArgv[0] = "Wish";
    newArgv[1] = NULL;
    Tk_Main(argc, newArgv, Tcl_AppInit);
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_AppInit --
 *
 *	This procedure performs application-specific initialization.
 *	Most applications, especially those that incorporate additional
 *	packages, will have their own version of this procedure.
 *
 * Results:
 *	Returns a standard Tcl completion code, and leaves an error
 *	message in interp->result if an error occurs.
 *
 * Side effects:
 *	Depends on the startup script.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_AppInit(interp)
    Tcl_Interp *interp;		/* Interpreter for application. */
{
    if (Tcl_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    if (Tk_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "Tk", Tk_Init, (Tcl_PackageInitProc *) NULL);
	
    /*
     * Call the init procedures for included packages.  Each call should
     * look like this:
     *
     * if (Mod_Init(interp) == TCL_ERROR) {
     *     return TCL_ERROR;
     * }
     *
     * where "Mod" is the name of the module.
     */

#ifdef TK_TEST
    if (Tktest_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
    Tcl_StaticPackage(interp, "Tktest", Tktest_Init,
            (Tcl_PackageInitProc *) NULL);
#endif /* TK_TEST */

    /*
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */

    SetupMainInterp(interp);

    /*
     * Specify a user-specific startup file to invoke if the application
     * is run interactively.  On the Mac we can specifiy that this file
     * is in the application's resource fork.  A traditional file location
     * may also be used.
     */

    Tcl_Eval(interp, "source -rsrc {tclshrc}");
    Tcl_ResetResult(interp);

    /* Tcl_SetVar(interp, "tcl_rcFileName", "~/.tclshrc", TCL_GLOBAL_ONLY); */
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * MacintoshInit --
 *
 *	This procedure calls Mac specific initilization calls.  Most of
 *	these calls must be made as soon as possible in the startup
 *	process.
 *
 * Results:
 *	Returns TCL_OK if everything went fine.  If it didn't the 
 *	application should probably fail.
 *
 * Side effects:
 *	Inits the application.
 *
 *----------------------------------------------------------------------
 */

static int
MacintoshInit()
{
    int i;
    long result, mask = 0x0700; 		/* mask = system 7.x */
    
    InitGraf(&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    InitDialogs((long) NULL);		
    InitCursor();

    /*
     * Make sure we are running on system 7 or higher
     */
    if (((Gestalt(gestaltSystemVersion, &result) != noErr)
	    && (mask == (result & mask)))) {
	panic("Tcl/Tk requires system 7 or higher.");
    }

    /*
     * Make sure we have color quick draw 
     * (this means we can't run on 68000 macs)
     */
    if (((Gestalt(gestaltQuickdrawVersion, &result) != noErr)
	    && (result < gestalt8BitQD))) {
	panic("Tk requires color quickdraw.");
    }

    
    FlushEvents(everyEvent, 0);
    SetEventMask(everyEvent);

    /*
     * Set up stack & heap sizes
     */
    /* TODO: stack size
       size = StackSpace();
       SetAppLimit(GetAppLimit() - 8192);
     */
    MaxApplZone();
    for (i = 0; i < 4; i++) {
	(void) MoreMasters();
    }

    TclMacSetEventProc(TkMacConvertEvent);
    TkConsoleCreate();

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * SetupMainInterp --
 *
 *	This procedure calls initalization routines require a Tcl 
 *	interp as an argument.  This call effectively makes the passed
 *	iterpreter the "main" interpreter for the application.
 *
 * Results:
 *	Returns TCL_OK if everything went fine.  If it didn't the 
 *	application should probably fail.
 *
 * Side effects:
 *	More initilization.
 *
 *----------------------------------------------------------------------
 */

static int
SetupMainInterp(interp)
    Tcl_Interp *interp;
{
    /*
     * Initialize the console only if we are running as an interactive
     * application.
     */

    if (strcmp(Tcl_GetVar(interp, "tcl_interactive", TCL_GLOBAL_ONLY), "1")
	    == 0) {
	if (TkConsoleInit(interp) == TCL_ERROR) {
	goto error;
	}
    }

    TkMacInitAppleEvents(interp);
    TkMacInitMenus(interp);
    
    /*
     * Attach the global interpreter to tk's expected global console
     */
    gStdoutInterp = interp;

    return TCL_OK;

error:
    panic(interp->result);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * InstallConsole, RemoveConsole, etc. --
 *
 *	The following functions provide the UI for the console package.
 *	Users wishing to replace SIOUX with their own console package 
 *	need only provide the four functions below in a library.
 *
 * Results:
 *	See SIOUX documentation for details.
 *
 * Side effects:
 *	See SIOUX documentation for details.
 *
 *----------------------------------------------------------------------
 */

short 
InstallConsole(short fd)
{
#pragma unused (fd)

	return 0;
}

void 
RemoveConsole(void)
{
}

long 
WriteCharsToConsole(char *buffer, long n)
{
    TkConsolePrint(gStdoutInterp, NULL, buffer, n);
    return n;
}

long 
ReadCharsFromConsole(char *buffer, long n)
{
    return 0;
}

extern char *
__ttyname(long fildes)
{
    static char *__devicename = "null device";

    if (fildes >= 0 && fildes <= 2) {
	return (__devicename);
    }
    
    return (0L);
}

short
SIOUXHandleOneEvent(EventRecord *event)
{
    return 0;
}

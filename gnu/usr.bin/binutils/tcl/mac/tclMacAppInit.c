/* 
 * tclMacAppInit.c --
 *
 *	Provides a version of the Tcl_AppInit procedure for the example shell.
 *
 * Copyright (c) 1993-1994 Lockheed Missle & Space Company, AI Center
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacAppInit.c 1.11 96/03/26 11:49:20
 */

#include "tcl.h"
#include "tclInt.h"
#include "tclPort.h"
#include <Events.h>

#if defined(THINK_C)
#   include <console.h>
#elif defined(__MWERKS__)
#   include <SIOUX.h>
short InstallConsole _ANSI_ARGS_((short fd));
#endif

#ifdef TCL_TEST
EXTERN int		Tcltest_Init _ANSI_ARGS_((Tcl_Interp *interp));
#endif /* TCL_TEST */

typedef int (*TclMacConvertEventPtr) _ANSI_ARGS_((EventRecord *eventPtr));
void 	TclMacSetEventProc _ANSI_ARGS_((TclMacConvertEventPtr procPtr));

/*
 * Forward declarations for procedures defined later in this file:
 */

static int		MacintoshInit _ANSI_ARGS_((void));

/*
 *----------------------------------------------------------------------
 *
 * main --
 *
 *	Main program for tclsh.  This file can be used as a prototype
 *	for other applications using the Tcl library.
 *
 * Results:
 *	None. This procedure never returns (it exits the process when
 *	it's done.
 *
 * Side effects:
 *	This procedure initializes the Macintosh world and then 
 *	calls Tcl_Main.  Tcl_Main will never return except to exit.
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
    newArgv[0] = "tclsh";
    newArgv[1] = NULL;
    Tcl_Main(argc, newArgv, Tcl_AppInit);
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

#ifdef TCL_TEST
    if (Tcltest_Init(interp) == TCL_ERROR) {
	return TCL_ERROR;
    }
#endif /* TCL_TEST */

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

    /*
     * Call Tcl_CreateCommand for application-specific commands, if
     * they weren't already created by the init procedures called above.
     */

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
 *	This procedure calls initalization routines to set up a simple
 *	console on a Macintosh.  This is necessary as the Mac doesn't
 *	have a stdout & stderr by default.
 *
 * Results:
 *	Returns TCL_OK if everything went fine.  If it didn't the 
 *	application should probably fail.
 *
 * Side effects:
 *	Inits the appropiate console package.
 *
 *----------------------------------------------------------------------
 */

static int
MacintoshInit()
{
#if defined(THINK_C)

    /* Set options for Think C console package */
    /* The console package calls the Mac init calls */
    console_options.pause_atexit = 0;
    console_options.title = "\pTcl Interpreter";
		
#elif defined(__MWERKS__)

    /* Set options for CodeWarrior SIOUX package */
    SIOUXSettings.autocloseonquit = true;
    SIOUXSettings.showstatusline = true;
    SIOUXSettings.asktosaveonclose = false;
    InstallConsole(0);
    SIOUXSetTitle("\pTcl Interpreter");
		
#elif defined(applec)

    /* Init packages used by MPW SIOW package */
    InitGraf((Ptr)&qd.thePort);
    InitFonts();
    InitWindows();
    InitMenus();
    TEInit();
    InitDialogs(nil);
    InitCursor();
		
#endif

    TclMacSetEventProc((TclMacConvertEventPtr) SIOUXHandleOneEvent);
    
    /* No problems with initialization */
    return TCL_OK;
}

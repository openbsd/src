/* 
 * tclMacTest.c --
 *
 *	Contains commands for platform specific tests for
 *	the Macintosh platform.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacTest.c 1.3 96/03/30 14:59:00
 */

#include "tclInt.h"
#include "tclMacInt.h"
#include "Files.h"
#include <Errors.h>
#include <Resources.h>
#include <Script.h>
#include <Strings.h>

/*
 * Forward declarations of procedures defined later in this file:
 */

int			TclplatformtestInit _ANSI_ARGS_((Tcl_Interp *interp));
static int		DebuggerCmd _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));
static int		WriteTextResource _ANSI_ARGS_((ClientData dummy,
			    Tcl_Interp *interp, int argc, char **argv));			    

/*
 *----------------------------------------------------------------------
 *
 * TclplatformtestInit --
 *
 *	Defines commands that test platform specific functionality for
 *	Unix platforms.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Defines new commands.
 *
 *----------------------------------------------------------------------
 */

int
TclplatformtestInit(interp)
    Tcl_Interp *interp;		/* Interpreter to add commands to. */
{
    /*
     * Add commands for platform specific tests on MacOS here.
     */
    
    Tcl_CreateCommand(interp, "debugger", DebuggerCmd,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);
    Tcl_CreateCommand(interp, "testWriteTextResource", WriteTextResource,
            (ClientData) 0, (Tcl_CmdDeleteProc *) NULL);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * DebuggerCmd --
 *
 *	This procedure simply calls the low level debugger.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
DebuggerCmd(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Not used. */
    int argc;				/* Not used. */
    char **argv;			/* Not used. */
{
    Debugger();
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * WriteTextResource --
 *
 *	This procedure will write a text resource out to the 
 *	application or a given file.  The format for this command is
 *	textwriteresource 
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
WriteTextResource(clientData, interp, argc, argv)
    ClientData clientData;		/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *errNum = "wrong # args: ";
    char *errBad = "bad argument: ";
    char *errStr;
    char *fileName = NULL, *rsrcName = NULL;
    char *data = NULL;
    int rsrcID = -1, i;
    short fileRef = -1;
    OSErr err;
    Handle dataHandle;
    Str255 resourceName;
    FSSpec fileSpec;

    /*
     * Process the arguments.
     */
    for (i = 1 ; i < argc ; i++) {
	if (!strcmp(argv[i], "-rsrc")) {
	    rsrcName = argv[i + 1];
	    i++;
	} else if (!strcmp(argv[i], "-rsrcid")) {
	    rsrcID = atoi(argv[i + 1]);
	    i++;
	} else if (!strcmp(argv[i], "-file")) {
	    fileName = argv[i + 1];
	    i++;
	} else {
	    data = argv[i];
	}
    }
	
    if ((rsrcName == NULL && rsrcID < 0) ||
	    (fileName == NULL) || (data == NULL)) {
    	errStr = errBad;
    	goto sourceFmtErr;
    }

    /*
     * Open the resource file.
     */
    err = FSpLocationFromPath(strlen(fileName), fileName, &fileSpec);
    if (!(err == noErr || err == fnfErr)) {
	Tcl_AppendResult(interp, "couldn't validate file name", (char *) NULL);
	return TCL_ERROR;
    }
    
    if (err == fnfErr) {
	FSpCreateResFile(&fileSpec, 'WIsH', 'rsrc', smSystemScript);
    }
    fileRef = FSpOpenResFile(&fileSpec, fsRdWrPerm);
    if (fileRef == -1) {
	Tcl_AppendResult(interp, "couldn't open resource file", (char *) NULL);
	return TCL_ERROR;
    }
		
    UseResFile(fileRef);

    /*
     * Prepare data needed to create resource.
     */
    if (rsrcID < 0) {
	rsrcID = UniqueID('TEXT');
    }
    
    strcpy((char *) resourceName, rsrcName);
    c2pstr((char *) resourceName);
    
    dataHandle = NewHandle(strlen(data) + 1);
    HLock(dataHandle);
    strcpy(*dataHandle, data);
    HUnlock(dataHandle);
    
    /*
     * Add the resource to the file and close it.
     */
    AddResource(dataHandle, 'TEXT', rsrcID, resourceName);
    UpdateResFile(fileRef);
    CloseResFile(fileRef);
    return TCL_OK;
    
    sourceFmtErr:
    Tcl_AppendResult(interp, errStr, "error in \"", argv[0], "\"",
	    (char *) NULL);
    return TCL_ERROR;
}

/* 
 * tclWinLoad.c --
 *
 *	This procedure provides a version of the TclLoadFile that
 *	works with the Windows "LoadLibrary" and "GetProcAddress"
 *	API for dynamic loading.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinLoad.c 1.6 96/02/15 11:54:07
 */

#include "tclInt.h"
#include "tclPort.h"


/*
 *----------------------------------------------------------------------
 *
 * TclLoadFile --
 *
 *	Dynamically loads a binary code file into memory and returns
 *	the addresses of two procedures within that file, if they
 *	are defined.
 *
 * Results:
 *	A standard Tcl completion code.  If an error occurs, an error
 *	message is left in interp->result.
 *
 * Side effects:
 *	New code suddenly appears in memory.
 *
 *----------------------------------------------------------------------
 */

int
TclLoadFile(interp, fileName, sym1, sym2, proc1Ptr, proc2Ptr)
    Tcl_Interp *interp;		/* Used for error reporting. */
    char *fileName;		/* Name of the file containing the desired
				 * code. */
    char *sym1, *sym2;		/* Names of two procedures to look up in
				 * the file's symbol table. */
    Tcl_PackageInitProc **proc1Ptr, **proc2Ptr;
				/* Where to return the addresses corresponding
				 * to sym1 and sym2. */
{
    HINSTANCE handle;
    char *buffer;

    handle = TclWinLoadLibrary(fileName);
    if (handle == NULL) {
	Tcl_AppendResult(interp, "couldn't load file \"", fileName,
		"\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }

    /*
     * For each symbol, check for both Symbol and _Symbol, since Borland
     * generates C symbols with a leading '_' by default.
     */

    *proc1Ptr = (Tcl_PackageInitProc *) GetProcAddress(handle, sym1);
    if (*proc1Ptr == NULL) {
	buffer = ckalloc(strlen(sym1)+2);
	buffer[0] = '_';
	strcpy(buffer+1, sym1);
	*proc1Ptr = (Tcl_PackageInitProc *) GetProcAddress(handle, buffer);
	ckfree(buffer);
    }
    
    *proc2Ptr = (Tcl_PackageInitProc *) GetProcAddress(handle, sym2);
    if (*proc2Ptr == NULL) {
	buffer = ckalloc(strlen(sym2)+2);
	buffer[0] = '_';
	strcpy(buffer+1, sym2);
	*proc2Ptr = (Tcl_PackageInitProc *) GetProcAddress(handle, buffer);
	ckfree(buffer);
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGuessPackageName --
 *
 *	If the "load" command is invoked without providing a package
 *	name, this procedure is invoked to try to figure it out.
 *
 * Results:
 *	Always returns 0 to indicate that we couldn't figure out a
 *	package name;  generic code will then try to guess the package
 *	from the file name.  A return value of 1 would have meant that
 *	we figured out the package name and put it in bufPtr.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
TclGuessPackageName(fileName, bufPtr)
    char *fileName;		/* Name of file containing package (already
				 * translated to local form if needed). */
    Tcl_DString *bufPtr;	/* Initialized empty dstring.  Append
				 * package name to this if possible. */
{
    return 0;
}

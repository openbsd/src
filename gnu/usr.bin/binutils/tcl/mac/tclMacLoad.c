/*
 * tclMacLoad.c --
 *
 *	This procedure provides a version of the TclLoadFile for use
 *	on the Macintosh.  This procedure will only work with systems 
 *	that use the Code Fragment Manager.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacLoad.c 1.11 96/03/25 14:57:40
 */

#include <CodeFragments.h>
#include <Errors.h>
#include <Resources.h>
#include <Strings.h>
#include "tclInt.h"
#include "tclMacInt.h"

/*
 *----------------------------------------------------------------------
 *
 * TclLoadFile --
 *
 *	This procedure is called to carry out dynamic loading of binary
 *	code for the Macintosh.  This implementation is based on the
 *	Code Fragment Manager & will not work on other systems.
 *
 * Results:
 *	The result is TCL_ERROR, and an error message is left in
 *	interp->result.
 *
 * Side effects:
 *	New binary code is loaded.
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
    ConnectionID connID;
    Ptr dummy;
    OSErr err;
    Str255 errName;
    SymClass symClass;
    FSSpec fileSpec;
    
    /*
     * TODO: This code only works for PowerPC
     *       for 69K-CFM - use kMotorola68KArch
     */
    
    err = FSpLocationFromPath(strlen(fileName), fileName, &fileSpec);
    if (err != noErr) {
	interp->result = "could not locate shared library";
	return TCL_ERROR;
    }
    
    /*
     * Open the resource fork for this library if it exists.  Note, that
     * this will make it the current res file.  The library init function
     * is the only library function that may assume this.  The resource
     * fork is opened in read only mode.
     */
    FSpOpenResFile(&fileSpec, fsRdPerm);
    
    err = GetDiskFragment(&fileSpec, 0, kWholeFork, fileSpec.name,
	    kLoadLib, &connID, &dummy, errName);
    if (err != fragNoErr) {
	p2cstr(errName);
	Tcl_AppendResult(interp, "couldn't load file \"", fileName,
	    "\": ", errName, (char *) NULL);
	return TCL_ERROR;
    }
    
    c2pstr(sym1);
    err = FindSymbol(connID, (StringPtr) sym1, (Ptr *) proc1Ptr, &symClass);
    p2cstr((StringPtr) sym1);
    if (err != fragNoErr || symClass == kDataCFragSymbol) {
	interp->result =
	    "could not find Initialization routine in library";
	return TCL_ERROR;
    }

    c2pstr(sym2);
    err = FindSymbol(connID, (StringPtr) sym2, (Ptr *) proc2Ptr, &symClass);
    p2cstr((StringPtr) sym2);
    if (err != fragNoErr || symClass == kDataCFragSymbol) {
	*proc2Ptr = NULL;
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

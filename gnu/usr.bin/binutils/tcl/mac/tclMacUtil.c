/* 
 * tclMacUtil.c --
 *
 *  This contains utility functions used to help with
 *  implementing Macintosh specific portions of the Tcl port.
 *
 * Copyright (c) 1993-1994 Lockheed Missle & Space Company, AI Center
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacUtil.c 1.43 96/03/30 14:54:55
 */

#include "tcl.h"
#include "tclInt.h"
#include "tclMacInt.h"
#include <math.h>

#include <Aliases.h>
#include <Errors.h>
#include <Files.h>
#include <Folders.h>
#include <FSpCompat.h>
#include <Fonts.h>
#include <Memory.h>
#include <Packages.h>
#include <Resources.h>
#include <Script.h>
#include <Strings.h>
#include <TextUtils.h>
#include <ToolUtils.h>
#include <MoreFilesExtras.h>

/* 
 * The following two Includes are from the More Files package.
 */
#include <FileCopy.h>
#include <MoreFiles.h>
#include <FullPath.h>


/*
 *----------------------------------------------------------------------
 *
 * hypot --
 *
 *	The standard math function hypot is not supported by Think C.
 *	It is included here so everything works.
 *
 * Results:
 *	Result of computation.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
 
#if defined(THINK_C)
double
hypot(x, y)
    double x, y;
{
    double sum;

    sum = x*x + y*y;
    return sqrt(sum);
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * TclMacEvalResource --
 *
 *    Used to extend the source command.  Sources Tcl code from a Text
 *    resource.  Currently only sources the resouce by name file ID may be
 *    supported at a later date.
 *
 * Side Effects:
 *	Depends on the Tcl code in the resource.
 *
 * Results:
 *      Returns a Tcl result.
 *
 *-----------------------------------------------------------------------------
 */
int
TclMacEvalResource(interp, resourceName, resourceNumber, fileName)
    Tcl_Interp *interp;		/* Interpreter in which to process file. */
    char *resourceName;		/* Name of TEXT resource to source,
				   NULL if number should be used. */
    int resourceNumber;		/* Resource id of source. */
    char *fileName;		/* Name of file to process.
				   NULL if application resource. */
{
    Handle sourceText;
    Str255 rezName;
    char msg[200];
    int result;
    short saveRef, fileRef = -1;
    char idStr[64];
    FSSpec fileSpec;
    Tcl_DString buffer;
    char *nativeName;

    saveRef = CurResFile();
	
    if (fileName != NULL) {
	OSErr err;
	
	nativeName = Tcl_TranslateFileName(interp, fileName, &buffer);
	if (nativeName == NULL) {
	    return TCL_ERROR;
	}
	err = FSpLocationFromPath(strlen(nativeName), nativeName, &fileSpec);
	Tcl_DStringFree(&buffer);
	if (err != noErr) {
	    Tcl_AppendResult(interp, "Error finding the file: \"", 
		fileName, "\".", NULL);
	    return TCL_ERROR;
	}
		
	fileRef = FSpOpenResFile(&fileSpec, fsRdPerm);
	if (fileRef == -1) {
	    Tcl_AppendResult(interp, "Error reading the file: \"", 
		fileName, "\".", NULL);
	    return TCL_ERROR;
	}
		
	UseResFile(fileRef);
    } else {
	/*
	 * The default behavior will search through all open resource files.
	 * This may not be the behavior you desire.  If you want the behavior
	 * of this call to *only* search the application resource fork, you
	 * must call UseResFile at this point to set it to the application
	 * file.  This means you must have already obtained the application's 
	 * fileRef when the application started up.
	 */
    }
	
    /*
     * Load the resource by name or ID
     */
    if (resourceName != NULL) {
	strcpy((char *) rezName + 1, resourceName);
	rezName[0] = strlen(resourceName);
	sourceText = GetNamedResource('TEXT', rezName);
    } else {
	sourceText = GetResource('TEXT', (short) resourceNumber);
    }
	
    if (sourceText == NULL) {
	result = TCL_ERROR;
    } else {
	int i, size;
	char *theSource = NULL, *allocSpace = NULL;
	char lastChar;
	
	HLock(sourceText);

	size = SizeResource(sourceText);
		
	/*
	 * To avoid cutting off part of some needed code we may 
	 * have to alloc space.
	 */
	
	lastChar = (*sourceText)[size - 1];
	if (lastChar == ' ' || lastChar == '\n' ||
		lastChar == '\t' || lastChar == '\r') {
	    (*sourceText)[size - 1] = '\0';
	    theSource = *sourceText;
	} else {
	    allocSpace = (char *) ckalloc(size + 1);
	    strncpy(allocSpace, *sourceText, size);
	    allocSpace[size] = '\0';
	    theSource = allocSpace;
	}
		
	/*
	 * Convert all carriage returns to newlines
	 */
	for (i=0; i<size; i++) {
	    if (theSource[i] == '\r') {
		theSource[i] = '\n';
	    }
	}
		
	/*
	 * We now evaluate the Tcl source
	 */
	result = Tcl_Eval(interp, theSource);
	if (result == TCL_RETURN) {
	    result = TCL_OK;
	} else if (result == TCL_ERROR) {
	    sprintf(msg, "\n    (rsrc \"%.150s\" line %d)", resourceName,
		    interp->errorLine);
	    Tcl_AddErrorInfo(interp, msg);
	}
		
	if (allocSpace != NULL) {
	    ckfree(allocSpace);
	}
	HUnlock(sourceText);
	ReleaseResource(sourceText);
		
	goto rezEvalCleanUp;
    }
	
    rezEvalError:
    sprintf(idStr, "ID=%d", resourceNumber);
    Tcl_AppendResult(interp, "The resource \"",
	    (resourceName != NULL ? resourceName : idStr),
	    "\" could not be loaded from ",
	    (fileName != NULL ? fileName : "application"),
	    ".", NULL);

    rezEvalCleanUp:
    if (fileRef != -1) {
	CloseResFile(fileRef);
    }

    UseResFile(saveRef);
	
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * FSpGetDefaultDir --
 *
 *	This function sets the provided FSSpec to the location of the 
 *	default directory.
 *
 * Results:
 *	The provided FSSpec is changed to point to the "default"
 *	directory.  The function returns what ever errors
 *	FSMakeFSSpecCompat may encounter.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
FSpGetDefaultDir(dirSpec)
	FSSpecPtr dirSpec;
{
    OSErr err;
    short vRefNum = 0;
    long int dirID = 0;

    err = HGetVol(NULL, &vRefNum, &dirID);
	
    if (err == noErr) {
	err = FSMakeFSSpecCompat(vRefNum, dirID, (ConstStr255Param) NULL,
		dirSpec);
    }
	
    return err;
}

/*
 *----------------------------------------------------------------------
 *
 * FSpSetDefaultDir --
 *
 *	This function sets the default directory to the directory
 *	pointed to by the provided FSSpec.
 *
 * Results:
 *	The function returns what ever errors HSetVol may encounter.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
FSpSetDefaultDir(dirSpec)
	FSSpecPtr dirSpec;
{
    OSErr err;

    /*
     * The following special case is needed to work around a bug
     * in the Macintosh OS.  (Acutally PC Exchange.)
     */
    
    if (dirSpec->parID == fsRtParID) {
	err = HSetVol(NULL, dirSpec->vRefNum, fsRtDirID);
    } else {
	err = HSetVol(dirSpec->name, dirSpec->vRefNum, dirSpec->parID);
    }
    
    return err;
}

/*
 *----------------------------------------------------------------------
 *
 * FSpFindFolder --
 *
 *	This function is a version of the FindFolder function that 
 *	returns the result as a FSSpec rather than a vRefNum and dirID.
 *
 * Results:
 *	Results will be simaler to that of the FindFolder function.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#ifdef __MWERKS__
OSErr
FSpFindFolder(short vRefNum, OSType folderType, Boolean createFolder,
	FSSpec *spec)
#else
OSErr
FSpFindFolder(vRefNum, folderType, createFolder, spec)
    short vRefNum;
    OSType folderType;
    Boolean createFolder;
    FSSpec *spec;
#endif
{
    short foundVRefNum;
    long foundDirID;
    OSErr err;

    err = FindFolder(vRefNum, folderType, createFolder,
	    &foundVRefNum, &foundDirID);
    if (err != noErr) {
	return err;
    }
		
    err = FSMakeFSSpecCompat(foundVRefNum, foundDirID, "\p", spec);
    return err;
}

/*
 *----------------------------------------------------------------------
 *
 * FSpLocationFromPath --
 *
 *	This function obtains an FSSpec for a given macintosh path.
 *	Unlike the More Files function FSpLocationFromFullPath, this
 *	function will also accept partial paths and resolve any aliases
 *	along the path.
 *
 * Results:
 *	OSErr code..
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
FSpLocationFromPath(length, path, fileSpecPtr)
    int length;
    char *path;
    FSSpecPtr fileSpecPtr;
{
    Str255 fileName;
    OSErr err;
    short vRefNum;
    long dirID;
    int pos, cur;
    Boolean isDirectory;
    Boolean wasAlias;

    /*
     * Check to see if this is a full path.  If partial
     * we assume that path starts with the current working
     * directory.  (Ie. volume & dir = 0)
     */
    vRefNum = 0;
    dirID = 0;
    cur = 0;
    if (length == 0 || path[cur] == ':') {
	cur++;
	if (cur >= length) {
	    /*
	     * If path = ":" or NULL  
	     * Just return current directory.
	     */
	    FSMakeFSSpecCompat(0, 0, NULL, fileSpecPtr);
	    return noErr;
	}
    } else {
	while (path[cur] != ':' && cur < length) {
	    cur++;
	}
	if (cur < length) {
	    /*
	     * This is a full path
	     */
	    cur++;
	    strncpy((char *) fileName + 1, path, cur);
	    fileName[0] = cur;
	    err = FSMakeFSSpecCompat(0, 0, fileName, fileSpecPtr);
	    if (err != noErr) return err;
	    FSpGetDirectoryID(fileSpecPtr, &dirID, &isDirectory);
	    vRefNum = fileSpecPtr->vRefNum;
	} else {
	    cur = 0;
	}
    }
    
    isDirectory = 1;
    while (cur < length) {
	if (!isDirectory) {
	    return dirNFErr;
	}
	pos = cur;
	while (path[pos] != ':' && pos < length) {
	    pos++;
	}
	if (pos == cur) {
	    /* Move up one dir */
	    /* cur++; */
	    strcpy((char *) fileName + 1, "::");
	    fileName[0] = 2;
	} else {
	    strncpy((char *) fileName + 1, &path[cur], pos - cur);
	    fileName[0] = pos - cur;
	}
	err = FSMakeFSSpecCompat(vRefNum, dirID, fileName, fileSpecPtr);
	if (err != noErr) return err;
	err = ResolveAliasFile(fileSpecPtr, true, &isDirectory, &wasAlias);
	if (err != noErr) return err;
	FSpGetDirectoryID(fileSpecPtr, &dirID, &isDirectory);
	vRefNum = fileSpecPtr->vRefNum;
	cur = pos;
	if (path[cur] == ':') {
	    cur++;
	}
    }
    
    return noErr;
}

/*
 *----------------------------------------------------------------------
 *
 * GetGlobalMouse --
 *
 *	This procedure obtains the current mouse position in global
 *	coordinates.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
GetGlobalMouse(mouse)
    Point *mouse;		/* Mouse position. */
{
    EventRecord event;
    
    OSEventAvail(0, &event);
    *mouse = event.where;
}

/* 
 * tclMacUnix.c --
 *
 *	This file contains routines to implement several features
 *	available to the Unix implementation, but that require
 *      extra work to do on a Macintosh.  These include routines
 *      Unix Tcl normally hands off to the Unix OS.
 *
 * Copyright (c) 1993-1994 Lockheed Missle & Space Company, AI Center
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacUnix.c 1.49 96/04/05 15:26:11
 */

#include <Files.h>
#include <Strings.h>
#include <TextUtils.h>
#include <Finder.h>
#include <FSpCompat.h>
#include <Aliases.h>
#include <Folders.h>
#include <Packages.h>
#include <Processes.h>
#include <Resources.h>
#include <Errors.h>
#include <Sound.h>
#include <Script.h>
#include <Timer.h>
#ifdef THINK_C
#	include <pascal.h>
#endif

#include "tclInt.h"
#include "tclMacInt.h"

/*
 * The following two Includes are from the More Files package
 */
#include "FileCopy.h"
#include "FullPath.h"
#include "MoreFiles.h"
#include "MoreFilesExtras.h"

/*
 * The following may not be defined in some versions of
 * MPW header files.
 */
#ifndef kIsInvisible
#define kIsInvisible 0x4000
#endif
#ifndef kIsAlias
#define kIsAlias 0x8000
#endif

/*
 * Missing error codes
 */
#define usageErr		500
#define noSourceErr		501
#define isDirErr		502

/*
 * Static functions in this file.
 */

static int	GlobArgs _ANSI_ARGS_((Tcl_Interp *interp,
		    int *argc, char ***argv));

/*
 *----------------------------------------------------------------------
 *
 * Tcl_EchoCmd --
 *
 *    Implements the TCL echo command:
 *        echo ?str ...?
 *
 * Results:
 *      Always returns TCL_OK.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_EchoCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;
    int mode, result, i;

    chan = Tcl_GetChannel(interp, "stdout", &mode);
    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    for (i = 1; i < argc; i++) {
	result = Tcl_Write(chan, argv[i], -1);
	if (result < 0) {
	    Tcl_AppendResult(interp, "echo: ", Tcl_GetChannelName(chan),
		    ": ", Tcl_PosixError(interp), (char *) NULL);
	    return TCL_ERROR;
	}
        if (i < (argc - 1)) {
	    Tcl_Write(chan, " ", -1);
	}
    }
    Tcl_Write(chan, "\n", -1);
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_LsCmd --
 *
 *	This procedure is invoked to process the "ls" Tcl command.
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
Tcl_LsCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
#define STRING_LENGTH 80
#define CR '\n'
    int i, j;
    int fieldLength, len = 0, maxLen = 0, perLine;
    char **origArgv = argv;
    OSErr err;
    CInfoPBRec paramBlock;
    HFileInfo *hpb = (HFileInfo *)&paramBlock;
    DirInfo *dpb = (DirInfo *)&paramBlock;
    char theFile[256];
    char theLine[STRING_LENGTH + 2];
    int fFlag = false, pFlag = false, aFlag = false, lFlag = false,
	cFlag = false, hFlag = false;

    /*
     * Process command flags.  End if argument doesn't start
     * with a dash or is a dash by itself.  The remaining arguments
     * should be files.
     */
    for (i = 1; i < argc; i++) {
	if (argv[i][0] != '-') {
	    break;
	}
		
	if (!strcmp(argv[i], "-")) {
	    i++;
	    break;
	}
		
	for (j = 1 ; argv[i][j] ; ++j) {
	    switch(argv[i][j]) {
	    case 'a':
	    case 'A':
		aFlag = true;
		break;
	    case '1':
		cFlag = false;
		break;
	    case 'C':
		cFlag = true;
		break;
	    case 'F':
		fFlag = true;
		break;
	    case 'H':
		hFlag = true;
		break;
	    case 'p':
		pFlag = true;
		break;
	    case 'l':
		pFlag = false;
		lFlag = true;
		break;
	    default:
		Tcl_AppendResult(interp, "error - unknown flag ",
			"usage: ls -apCFHl1 ?files? ", NULL);
		return TCL_ERROR;
	    }
	}
    }

    argv += i;
    argc -= i;

    /*
     * No file specifications means we search for all files.
     * Glob will be doing most of the work.
     */
     if (!argc) {
	argc = 1;
	argv = origArgv;
	strcpy(argv[0], "*");
    }

    if (!GlobArgs(interp, &argc, &argv)) {
	Tcl_ResetResult(interp);
	return TCL_ERROR;
    }

    /*
     * There are two major methods for listing files: the long
     * method and the normal method.
     */
    if (lFlag) {
	char	creator[5], type[5], time[16], date[16];
	char	lineTag;
	long	size;
	unsigned short flags;

	/*
	 * Print the header for long listing.
	 */
	if (hFlag) {
	    sprintf(theLine, "T %7s %8s %8s %4s %4s %6s %s",
		    "Size", "ModTime", "ModDate",
		    "CRTR", "TYPE", "Flags", "Name");
	    Tcl_AppendResult(interp, theLine, "\n", NULL);
	    Tcl_AppendResult(interp,
		    "-------------------------------------------------------------\n",
		    NULL);
	}
		
	for (i = 0; i < argc; i++) {
	    strcpy(theFile, argv[i]);
			
	    c2pstr(theFile);
	    hpb->ioCompletion = NULL;
	    hpb->ioVRefNum = 0;
	    hpb->ioFDirIndex = 0;
	    hpb->ioNamePtr = (StringPtr) theFile;
	    hpb->ioDirID = 0L;
	    err = PBGetCatInfoSync(&paramBlock);
	    p2cstr((StringPtr) theFile);

	    if (hpb->ioFlAttrib & 16) {
		/*
		 * For directories use zero as the size, use no Creator
		 * type, and use 'DIR ' as the file type.
		 */
		if ((aFlag == false) && (dpb->ioDrUsrWds.frFlags & 0x1000)) {
		    continue;
		}
		lineTag = 'D';
		size = 0;
		IUTimeString(dpb->ioDrMdDat, false, (unsigned char *)time);
		p2cstr((StringPtr)time);
		IUDateString(dpb->ioDrMdDat, shortDate, (unsigned char *)date);
		p2cstr((StringPtr)date);
		strcpy(creator, "    ");
		strcpy(type, "DIR ");
		flags = dpb->ioDrUsrWds.frFlags;
		if (fFlag || pFlag) {
		    strcat(theFile, ":");
		}
	    } else {
		/*
		 * All information for files should be printed.  This
		 * includes size, modtime, moddate, creator type, file
		 * type, flags, anf file name.
		 */
		if ((aFlag == false) &&
			(hpb->ioFlFndrInfo.fdFlags & kIsInvisible)) {
		    continue;
		}
		lineTag = 'F';
		size = hpb->ioFlLgLen + hpb->ioFlRLgLen;
		IUTimeString(hpb->ioFlMdDat, false, (unsigned char *)time);
		p2cstr((StringPtr)time);
		IUDateString(hpb->ioFlMdDat, shortDate, (unsigned char *)date);
		p2cstr((StringPtr)date);
		strncpy(creator, (char *) &hpb->ioFlFndrInfo.fdCreator, 4);
		creator[4] = 0;
		strncpy(type, (char *) &hpb->ioFlFndrInfo.fdType, 4);
		type[4] = 0;
		flags = hpb->ioFlFndrInfo.fdFlags;
		if (fFlag) {
		    if (hpb->ioFlFndrInfo.fdFlags & kIsAlias) {
			strcat(theFile, "@");
		    } else if (hpb->ioFlFndrInfo.fdType == 'APPL') {
			strcat(theFile, "*");
		    }
		}
	    }
			
	    sprintf(theLine, "%c %7ld %8s %8s %-4.4s %-4.4s 0x%4.4X %s",
		    lineTag, size, time, date, creator, type, flags, theFile);
						 
	    Tcl_AppendResult(interp, theLine, "\n", NULL);
	    
	}
		
	if ((interp->result != NULL) && (*(interp->result) != '\0')) {
	    int slen = strlen(interp->result);
	    if (interp->result[slen - 1] == '\n') {
		interp->result[slen - 1] = '\0';
	    }
	}
    } else {
	/*
	 * Not in long format. We only print files names.  If the
	 * -C flag is set we need to print in multiple coloumns.
	 */
	int argCount, linePos;
	Boolean needNewLine = false;

	/*
	 * Fiend the field length: the length each string printed
	 * to the terminal will be.
	 */
	if (!cFlag) {
	    perLine = 1;
	    fieldLength = STRING_LENGTH;
	} else {
	    for (i = 0; i < argc; i++) {
		len = strlen(argv[i]);
		if (len > maxLen) {
		    maxLen = len;
		}
	    }
	    fieldLength = maxLen + 3;
	    perLine = STRING_LENGTH / fieldLength;
	}

	argCount = 0;
	linePos = 0;
	memset(theLine, ' ', STRING_LENGTH);
	while (argCount < argc) {
	    strcpy(theFile, argv[argCount]);
			
	    c2pstr(theFile);
	    hpb->ioCompletion = NULL;
	    hpb->ioVRefNum = 0;
	    hpb->ioFDirIndex = 0;
	    hpb->ioNamePtr = (StringPtr) theFile;
	    hpb->ioDirID = 0L;
	    err = PBGetCatInfoSync(&paramBlock);
	    p2cstr((StringPtr) theFile);

	    if (hpb->ioFlAttrib & 16) {
		/*
		 * Directory. If -a show hidden files.  If -f or -p
		 * denote that this is a directory.
		 */
		if ((aFlag == false) && (dpb->ioDrUsrWds.frFlags & 0x1000)) {
		    argCount++;
		    continue;
		}
		if (fFlag || pFlag) {
		    strcat(theFile, ":");
		}
	    } else {
		/*
		 * File: If -a show hidden files, if -f show links
		 * (aliases) and executables (APPLs).
		 */
		if ((aFlag == false) &&
			(hpb->ioFlFndrInfo.fdFlags & kIsInvisible)) {
		    argCount++;
		    continue;
		}
		if (fFlag) {
		    if (hpb->ioFlFndrInfo.fdFlags & kIsAlias) {
			strcat(theFile, "@");
		    } else if (hpb->ioFlFndrInfo.fdType == 'APPL') {
			strcat(theFile, "*");
		    }
		}
	    }

	    /*
	     * Print the item, taking into account multi-
	     * coloum output.
	     */
	    strncpy(theLine + (linePos * fieldLength), theFile,
		    strlen(theFile));
	    linePos++;
			
	    if (linePos == perLine) {
		theLine[STRING_LENGTH] = '\0';
		if (needNewLine) {
		    Tcl_AppendResult(interp, "\n", theLine, NULL);
		} else {
		    Tcl_AppendResult(interp, theLine, NULL);
		    needNewLine = true;
		}
		linePos = 0;
		memset(theLine, ' ', STRING_LENGTH);
	    }
			
	    argCount++;
	}
		
	if (linePos != 0) {
	    theLine[STRING_LENGTH] = '\0';
	    if (needNewLine) {
		Tcl_AppendResult(interp, "\n", theLine, NULL);
	    } else {
		Tcl_AppendResult(interp, theLine, NULL);
	    }
	}
    }
	
    ckfree((char *) argv);
	
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MkdirCmd --
 *
 *	This procedure is invoked to process the "mkdir" Tcl command.
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
Tcl_MkdirCmd (dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_DString buffer;
    long createdDirID;
    int pargc, i;
    char **pargv;
    char *fileName;
    OSErr err;
    FSSpec fileSpec;

    if ((argc < 2) || (argc > 3)) {
	Tcl_AppendResult(interp, "wrong # args: ", argv[0], 
	    " ?-path? path", (char *) NULL);
	return TCL_ERROR;
    }
    if (argc == 3) {
	if (strcmp(argv[1], "-path")) {
	    Tcl_AppendResult(interp, "bad switch: ", argv[0], 
		" ?-path? path", (char *) NULL);
	    return TCL_ERROR;
	}
	fileName = argv[2];
    } else {
	fileName = argv[1];
    }

    Tcl_DStringInit(&buffer);
    fileName = Tcl_TranslateFileName(interp, fileName, &buffer);
    if (fileName == NULL) {
	goto errorExit;
    }
    err = FSpLocationFromPath(buffer.length, fileName, &fileSpec);
    if (err == noErr) {
	Tcl_AppendResult(interp, "directory already exists", (char *) NULL);
	goto errorExit;	
    }

    if (argc == 3) {
	Tcl_DString path;
	
	Tcl_DStringInit(&path);
	Tcl_SplitPath(fileName, &pargc, &pargv);
	for (i = 0; i < pargc; i++) {
	    Tcl_JoinPath(1, &pargv[i], &path);
	    err = FSpLocationFromPath(path.length, path.string, &fileSpec);
	    if (err == fnfErr) {
		err = FSpDirCreateCompat(&fileSpec, smSystemScript, &createdDirID);
	    }
	    if (err != noErr) {
		break;
	    }
	}
	ckfree((char *) pargv);
	Tcl_DStringFree(&path);
    } else {
	err = FSpDirCreateCompat(&fileSpec, smSystemScript, &createdDirID);
    }
    
    if (err != noErr) {
	Tcl_AppendResult(interp, "an error occured while creating a directory", 
	    (char *) NULL);
	goto errorExit; 
    } 

    return TCL_OK;

    errorExit:
    Tcl_DStringFree(&buffer);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RmdirCmd --
 *
 *     Implements the Tcl rmdir command:
 *
 * Results:
 *	Standard TCL results, may return the UNIX system error message.
 *
 * Side effects:
 *	See the user documentation.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_RmdirCmd (dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *fileName;
    Tcl_DString buffer;
    FSSpec fileSpec;
    OSErr err;

    if ((argc < 2) || (argc > 3)) {
	Tcl_AppendResult(interp, "wrong # args: ", argv[0], 
	    " ?-nocomplain? path", (char *) NULL);
	return TCL_ERROR;
    }
    if (argc == 3) {
	if (strcmp(argv[1], "-nocomplain")) {
	    Tcl_AppendResult(interp, "bad switch: ", argv[0], 
		" ?-nocomplain? path", (char *) NULL);
	    return TCL_ERROR;
	}
	fileName = argv[2];
    } else {
	fileName = argv[1];
    }

    Tcl_DStringInit(&buffer);
    fileName = Tcl_TranslateFileName(interp, fileName, &buffer);
    if (fileName == NULL) {
	return TCL_ERROR;	
    }
    err = FSpLocationFromPath(buffer.length, fileName, &fileSpec);
    Tcl_DStringFree(&buffer);
    if (err != noErr) {
	Tcl_AppendResult(interp, "couldn't locate directory", (char *) NULL);
	return TCL_ERROR;	
    }

    err = FSpDeleteCompat(&fileSpec);
    if (argc == 3) {
	err = noErr;
    }
    if (err == fBsyErr) {
	Tcl_AppendResult(interp, "directory not empty", (char *) NULL);
	return TCL_ERROR;
    } else if (err != noErr) {
	Tcl_AppendResult(interp, "error deleting directory", (char *) NULL);
	return TCL_ERROR;
    }
    
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_RmCmd --
 *
 *	This procedure is invoked to process the "rm" Tcl command.
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
Tcl_RmCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    int nocomplain = 0;
    OSErr err;
    FSSpec theSpec;
    int i, j, newArgc;
    char **newArgv;

    /*
     * Process options.  A dash by itself means no more options.
     */
    for (i = 1; i < argc; i++) {
	if (argv[i][0] != '-') {
	    break;
	}
		
	if (!strcmp(argv[i], "-nocomplain")) {
	    nocomplain = 1;
	}
		
	if (!strcmp(argv[i], "-")) {
	    i++;
	    break;
	}
		
	for (j = 1 ; argv[i][j] ; ++j) {
	    switch(argv[i][j]) {
	    	case 'f':
		    nocomplain = 1;
		    break;
		default:
		    Tcl_AppendResult(interp, 
			    "usage: rm [-nocomplain | -f] ?files? ",
			    TCL_STATIC);
		    return TCL_ERROR;
	    }
	}
    }

    newArgv = argv + i;
    newArgc = argc - i;
    
    /*
     * Use glob to expand file references.
     */
    if (!GlobArgs(interp, &newArgc, &newArgv)) {
	return TCL_OK;
    }

    for (i = 0 ; i < newArgc ; i++) {
	if (newArgv[i] == NULL) {
	    if (!nocomplain) {
		Tcl_AppendResult(interp,
			"could not substitute for directory \"",
			newArgv[i], "\" ", (char *) NULL);
		ckfree((char *) newArgv);
		return TCL_ERROR;
	    }
	    continue;
	}
		
	if ((err = FSpLocationFromPath(strlen(newArgv[i]), newArgv[i],
		&theSpec)) != noErr) {
	    if (!nocomplain) {
		Tcl_AppendResult(interp, "could not locate file \"",
			newArgv[i], "\" ", Tcl_PosixError(interp),
			(char *) NULL);
		ckfree((char *) newArgv);
		return TCL_ERROR;
	    }
	    continue;
	}
		
	if ((err = FSpDeleteCompat(&theSpec)) != noErr) {
	    if (err != noErr && !nocomplain) {
		Tcl_AppendResult(interp, "\"", newArgv[0], "\" ",
			"error deleting \"", newArgv[1], "\" ", (char *) NULL);
		ckfree((char *) newArgv);
		return TCL_ERROR;
	    }
	    continue;
	}
    }
	
    ckfree((char *) newArgv);

    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_CpCmd --
 *
 *	This procedure is invoked to process the "cp" Tcl command.
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
Tcl_CpCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    FSSpec fromSpec, toSpec;
    char fromName[256], toName[256];
    char *errStr;
    StringPtr fileName = NULL;
    OSErr err;
    long dirID;
    Boolean isDirectory;
    char *theName;
    Tcl_DString buffer;

    if (argc != 3) goto cpUsage;

    /*
     * Get the source files for the copy command
     */
    Tcl_DStringInit(&buffer);
    theName = Tcl_TranslateFileName(interp, argv[1], &buffer);
    if (theName == NULL) {
	Tcl_DStringFree(&buffer);
	Tcl_AppendResult(interp,"illegal file name \"",
		argv[1], "\"", 0);
	return TCL_ERROR;
    }
    strcpy(fromName, theName);
    Tcl_DStringFree(&buffer);
    if (FSpLocationFromPath(strlen(fromName), fromName, &fromSpec)) {
	goto cpUsage;
    }
    err = DirIDFromFSSpec(&fromSpec, &dirID, &isDirectory);
    if (err != noErr) {
	errStr = "Couldn't find source file.";
	goto cpError;
    }
    if (isDirectory) {
	errStr = "Can't copy directory.";
	goto cpError;
    }
	
    /*
     * Get destination directory or new file name
     */
    Tcl_DStringInit(&buffer);
    theName = Tcl_TranslateFileName(interp, argv[2], &buffer);
    if (theName == NULL) {
	Tcl_DStringFree(&buffer);
	Tcl_AppendResult(interp, "illegal file name \"",
		argv[2], "\"", 0);
	return TCL_ERROR;
    }
    strcpy(toName, theName);
    Tcl_DStringFree(&buffer);
    err = FSpLocationFromPath(strlen(toName), toName, &toSpec);
    err = DirIDFromFSSpec(&toSpec, &dirID, &isDirectory);
    if ((err == fnfErr) || !isDirectory) {
	/*
	 * Set toSpec to parent dir & fileName to actual file name
	 */
	fileName = (StringPtr) strrchr(toName, ':');
	if (fileName == NULL) {
	    fileName = (StringPtr) toName;
	} else {
	    fileName++;
	}
	c2pstr((char *) fileName);
	FSMakeFSSpecCompat(toSpec.vRefNum, dirID, NULL, &toSpec);
    }
		
    err = FSpFileCopy(&fromSpec, &toSpec, fileName, NULL, 0, true);
    switch (err) {
    case noErr:
	return TCL_OK;
    case dupFNErr:
	errStr = "Destination file already exists.";
	goto cpError;
    case dirNFErr:
	errStr = "Couldn't resolve destination directory.";
	goto cpError;
    case dskFulErr:
	errStr = "Not enough space to copy file.";
	goto cpError;
    case fnfErr:
	errStr = "File not found.";
	goto cpError;
    default:
	errStr = "Problem trying to copy file.";
	goto cpError;
    }
	
    cpUsage:
    errStr = "should be: cp <srcFile> [<destFile> | <destDir>]";
    
    cpError:
    Tcl_SetResult(interp, errStr, TCL_STATIC); 
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MvCmd --
 *
 *	This procedure is invoked to process the "cp" Tcl command.
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
Tcl_MvCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    OSErr err;
    Boolean isDirectory, doMoveRename;
    long fromID, toID, infoSize;
    FSSpec fromSpec, newDirSpec;
    Tcl_DString fromPath;
    char newName[80];
    char *errStr;
    GetVolParmsInfoBuffer volParmsInfo;
	
    Tcl_DStringInit(&fromPath);

    if (argc != 3) {
	goto mvUsage;
    }

    /*
     * Get file that will be moved
     */
    if (Tcl_TranslateFileName(interp, argv[1], &fromPath) == NULL) {
	Tcl_DStringFree(&fromPath);
	Tcl_AppendResult(interp,"illegal file name \"",
		argv[1], "\"", 0);
	return TCL_ERROR;
    }
    if (FSpLocationFromPath(fromPath.length, fromPath.string, &fromSpec)) {
	Tcl_DStringFree(&fromPath);
	goto mvUsage;
    }
    Tcl_DStringFree(&fromPath);
    err = DirIDFromFSSpec(&fromSpec, &fromID, &isDirectory);
    if (err != noErr) {
	errStr = "Couldn't find source file.";
	goto mvError;
    }

    /*
     * First check case of renaming (or just a dir name not a path)
     */
    strcpy(newName, argv[2]);
    if (strchr(newName, ':') == NULL) {
	c2pstr(newName);
	err = FSpRename(&fromSpec, (StringPtr) newName);
	if (err == noErr) {
	    return TCL_OK;
	} else if (err == dupFNErr) {
	    /*
	     * File may be dirctory, change to ":newName" & continue
	     */
	    newName[0] = ':';
	} else {
	    errStr = "Error occured while renaming file.";
	    goto mvError;
	}
    }

    /*
     * Check the volume to see what operations we can support
     */
    err = HGetVolParms(NULL, fromSpec.vRefNum, &volParmsInfo, &infoSize);
    if (err == noErr) {
	doMoveRename = hasMoveRename(volParmsInfo);
    } else {
	doMoveRename = false;
    }
	
    /*
     * Get destination directory (and maybe new file name as well)
     */
    err = FSpLocationFromPath(strlen(newName), newName, &newDirSpec);
    err = DirIDFromFSSpec(&newDirSpec, &toID, &isDirectory);
    if (err == noErr) {
	if (isDirectory) {
	    if (fromID == toID) {
		errStr = "Destination same as source.";
		goto mvError;
	    }
	    if (fromSpec.vRefNum != newDirSpec.vRefNum) {
		errStr = "Can't move across volumes.";
	    }
	    /*
	     * Changing location of file, name will remain the same
	     */
	    err = FSpCatMove(&fromSpec, &newDirSpec);
	} else {
	    errStr = "File already exists.";
	    goto mvError;
	}
    } else {
	if (err == fnfErr) {
	    /*
	     * Set newDirSpec to parent dir & fileName to actual file name
	     */
	    strcpy(newName, (char *) newDirSpec.name);
	    err = FSMakeFSSpecCompat(newDirSpec.vRefNum, toID, NULL,
		    &newDirSpec);
	    err = DirIDFromFSSpec(&newDirSpec, &toID, &isDirectory);
	    if (err != noErr || !isDirectory) {
		errStr = "No such directory.";
		goto mvError;
	    }
	    if (doMoveRename) {
		err = FSpMoveRename(&fromSpec, &newDirSpec,
			(StringPtr) newName);
	    } else {
		/*
		 * First do move
		 */
		err = FSpCatMove(&fromSpec, &newDirSpec);
		if (err == noErr) {
		    /*
		     * If that worked, rename the file
		     */
		    err = DirIDFromFSSpec(&newDirSpec, &toID, &isDirectory);
		    err = FSMakeFSSpecCompat(newDirSpec.vRefNum, toID,
			    fromSpec.name, &newDirSpec);
		    err = FSpRename(&newDirSpec, (StringPtr) newName);
		}
	    }			
	}
    }
	
    if (err != noErr) {
	errStr = "an unknown error occured.";
	goto mvError;
    } else {
	return TCL_OK;
    }
	
    mvUsage:
    errStr = "should be: mv <srcFile> [<destFile> | <destDir>]";
    
    mvError:
    Tcl_SetResult(interp, errStr, TCL_STATIC); 
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MacSourceCmd --
 *
 *	This procedure is invoked to process the "source" Tcl command.
 *	See the user documentation for details on what it does.  In addition,
 *	it supports sourceing from the resource fork of type 'TEXT'.
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
Tcl_MacSourceCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    char *errNum = "wrong # args: ";
    char *errBad = "bad argument: ";
    char *errStr;
    char *fileName = NULL, *rsrcName = NULL;
    int rsrcID = -1;

    if (argc < 2 || argc > 4)  {
    	errStr = errNum;
    	goto sourceFmtErr;
    }
    
    if (argc == 2)  {
	return Tcl_EvalFile(interp, argv[1]);
    }
    
    /*
     * The following code supports a few older forms of this command
     * for backward compatability.
     */
    if (!strcmp(argv[1], "-rsrc") || !strcmp(argv[1], "-rsrcname")) {
	rsrcName = argv[2];
    } else if (!strcmp(argv[1], "-rsrcid")) {
	if (Tcl_GetInt(interp, argv[2], &rsrcID) != TCL_OK) {
	    return TCL_ERROR;
	}
    } else {
    	errStr = errBad;
    	goto sourceFmtErr;
    }
    
    if (argc == 4) {
	fileName = argv[3];
    }
	
    return TclMacEvalResource(interp, rsrcName, rsrcID, fileName);
	
    sourceFmtErr:
    Tcl_AppendResult(interp, errStr, "should be \"", argv[0],
	    " fileName\" or \"", argv[0], " -rsrc name ?fileName?\" or \"",
	    argv[0], " -rsrcid id ?fileName?\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MacBeepCmd --
 *
 *	This procedure makes the beep sound.
 *
 * Results:
 *	A standard Tcl result.
 *
 * Side effects:
 *	Makes a beep.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_MacBeepCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Handle sound;
    Str255 sndName;
    int volume = -1;
    char * sndArg = NULL;
    long curVolume;

    if (argc == 1) {
	SysBeep(1);
	return TCL_OK;
    } else if (argc == 2) {
	if (!strcmp(argv[1], "-list")) {
	    int count, i;
	    short id;
	    Str255 theName;
	    ResType theType;
			
	    Tcl_ResetResult(interp);
	    count = CountResources('snd ');
	    for (i = 1; i <= count; i++) {
		sound = GetIndResource('snd ', i);
		if (sound != NULL) {
		    GetResInfo(sound, &id, &theType, theName);
		    if (theName[0] == 0) {
			continue;
		    }
		    theName[theName[0]+1] = '\0';
		    Tcl_AppendElement(interp, (char *) theName + 1);
		}
	    }
	    return TCL_OK;
	} else {
	    sndArg = argv[1];
	}
    } else if (argc == 3) {
	if (!strcmp(argv[1], "-volume")) {
	    volume = atoi(argv[2]);
	} else {
	    goto beepUsage;
	}
    } else if (argc == 4) {
	if (!strcmp(argv[1], "-volume")) {
	    volume = atoi(argv[2]);
	    sndArg = argv[3];
	} else {
	    goto beepUsage;
	}
    } else {
	goto beepUsage;
    }
	
    /*
     * Set Volume
     */
    if (volume >= 0) {
	GetSysBeepVolume(&curVolume);
	SetSysBeepVolume((short) volume);
    }
	
    /*
     * Play the sound
     */
    if (sndArg == NULL) {
	SysBeep(1);
    } else {
	strcpy((char *) sndName + 1, sndArg);
	sndName[0] = strlen(sndArg);
	sound = GetNamedResource('snd ', sndName);
	if (sound != NULL) {
#if (THINK_C == 7)
	    SndPlay(NULL, sound, false);
#else
	    SndPlay(NULL, (SndListHandle) sound, false);
#endif
	    return TCL_OK;
	} else {
	    if (volume >= 0) {
		SetSysBeepVolume(curVolume);
	    }
	    Tcl_ResetResult(interp);
	    Tcl_AppendResult(interp, " \"", sndArg, 
		    "\" is not a valid sound.  (Try ", argv[0],
		    " -list)", NULL);
	    return TCL_ERROR;
	}
    }

    /*
     * Reset Volume
     */
    if (volume >= 0) {
	SetSysBeepVolume(curVolume);
    }
    return TCL_OK;

    beepUsage:
    Tcl_AppendResult(interp, "wrong # args: should be \"", argv[0],
	    " [-volume num] [-list | sndName]?\"", (char *) NULL);
    return TCL_ERROR;
}

/*
 *----------------------------------------------------------------------
 *
 * GlobArgs --
 *
 *	The following function was taken from Peter Keleher's Alpha
 *	Editor.  *argc should only count the end arguments that should
 *	be globed.  argv should be incremented to point to the first
 *	arg to be globed.
 *
 * Results:
 *	Returns 'true' if it worked & memory was allocated, else 'false'.
 *
 * Side effects:
 *	argv will be alloced, the call will need to release the memory
 *
 *----------------------------------------------------------------------
 */
 
static int
GlobArgs(interp, argc, argv)
    Tcl_Interp *interp;
    int *argc;
    char ***argv;
{
    int res, len;
    char *list;
	
    /*
     * Places the globbed args all into 'interp->result' as a list.
     */
    res = Tcl_GlobCmd(NULL, interp, *argc + 1, *argv - 1);
    if (res != TCL_OK) {
	return false;
    }
    len = strlen(interp->result);
    list = (char *)ckalloc(len + 1);
    strcpy(list, interp->result);
    Tcl_ResetResult(interp);
	
    res = Tcl_SplitList(interp, list, argc, argv);
    if (res != TCL_OK) {
	return false;
    }
    return true;
}

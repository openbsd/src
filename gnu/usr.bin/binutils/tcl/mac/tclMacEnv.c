/* 
 * tclMacEnv.c --
 *
 *	Implements the "environment" on a Macintosh.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacEnv.c 1.24 96/04/09 15:01:46
 */

#if (THINK_C <= 7)
#	include <GestaltEqu.h>
#else
#	include <Gestalt.h>
#endif
#include <Folders.h>
#include <TextUtils.h>
#include <Resources.h>
#include <Memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "tcl.h"
#include "tclInt.h"
#include "tclMacInt.h"
#include "tclPort.h"

/*
 * From the MoreFiles package
 */
#include "FullPath.h"

#define kMaxEnvStringSize 	255
#define kMaxEnvVarSize 		100
#define kLoginnameTag 		"LOGIN="
#define kUsernameTag 		"USER="
#define kDefaultDirTag 		"HOME="

/* 
 * The following specifies a text file where additional environment variables 
 * can be set.  The file must reside in the preferences folder.  If the file 
 * doesn't exist NO error will occur.  Commet out the difinition if you do 
 * NOT want to use an environment variables file. 
 */
#define kPrefsFile	 		"Tcl Environment Variables"

/* 
 * The following specifies the Name of a 'STR#' resource in the application 
 * where additional environment variables may be set.  If the resource doesn't
 * exist no errors will occur.  Commet it out if you don't want it.
 */
#define REZ_ENV "\pTcl Environment Variables"

/* Globals */
char **environ = NULL;

/*
 * Declarations for local procedures defined in this file:
 */
static char ** RezRCVariables _ANSI_ARGS_((void));
static char ** FileRCVariables _ANSI_ARGS_((void));
static char ** PathVariables _ANSI_ARGS_((void));
static char ** SystemVariables _ANSI_ARGS_((void));
static char * MakeFolderEnvVar _ANSI_ARGS_((char * prefixTag,
	long whichFolder));
static char * GetUserName _ANSI_ARGS_((void));

/*
 *----------------------------------------------------------------------
 *
 * RezRCVariables --
 *
 *  Creates environment variables from the applications resource fork.
 *  The function looks for the 'STR#' resource with the name defined
 *  in the #define REZ_ENV.  If the define is not defined this code
 *  will not be included.  If the resource doesn't exist or no strings
 *  reside in the resource nothing will happen.
 *
 * Results:
 *	ptr to value on success, NULL if error.
 *
 * Side effects:
 *	Memory is allocated and returned to the caller.
 *
 *----------------------------------------------------------------------
 */

#ifdef REZ_ENV
static char ** 
RezRCVariables()
{
    Handle envStrs = NULL;
    char** rezEnv = NULL;
    short int numStrs;

    envStrs = GetNamedResource('STR#', REZ_ENV);
    if (envStrs == NULL) return NULL;
    numStrs = *((short *) (*envStrs));
	
    rezEnv = (char **) ckalloc((numStrs + 1) * sizeof(char *));

    if (envStrs != NULL) {
	ResType	theType;
	Str255 theName;
	short theID, index = 1;
	int i = 0;
	char* string;
	
	GetResInfo(envStrs, &theID, &theType, theName);
	for(;;) {
	    GetIndString(theName, theID, index++);
	    if (theName[0] == '\0') break;
	    string = (char *) ckalloc(theName[0] + 2);
	    strncpy(string, (char *) theName + 1, theName[0]);
	    string[theName[0]] = '\0';
	    rezEnv[i++] = string;
	}
	ReleaseResource(envStrs);
		
	rezEnv[i] = NULL;
	return rezEnv;
    }
	
    return NULL;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * FileRCVariables --
 *
 *  Creates environment variables from a file in the system preferences
 *  folder.  The function looks for a file in the preferences folder 
 *  a name defined in the #define kPrefsFile.  If the define is not 
 *  defined this code will not be included.  If the resource doesn't exist or
 *  no strings reside in the resource nothing will happen.
 *
 * Results:
 *	ptr to value on success, NULL if error.
 *
 * Side effects:
 *	Memory is allocated and returned to the caller.
 *
 *----------------------------------------------------------------------
 */

#ifdef kPrefsFile
static char ** 
FileRCVariables()
{
    char *prefsFolder = NULL;
    char *tempPtr = NULL;
    char **fileEnv = NULL;
    FILE *thePrefsFile = NULL;
    int	i;
    char tempStr[kMaxEnvStringSize];
    FSSpec currentDir, prefDir;
    OSErr err;
	
    /*
     * Save default folder and change working dir
     * to the preferences folder.
     */
    FSpGetDefaultDir(&currentDir);
    err = FSpFindFolder(kOnSystemDisk, kPreferencesFolderType, 
	    kDontCreateFolder, &prefDir);
    err = FSpSetDefaultDir(&prefDir);

    /* TODO: this code should use new channel IO. */
    if ((thePrefsFile = fopen(kPrefsFile, "r")) == NULL) {
	FSpSetDefaultDir(&currentDir);
	return NULL;
    }
	
    fileEnv = (char **) ckalloc((kMaxEnvVarSize + 1) * sizeof(char *));

    i = 0;
    while (fgets(tempStr, kMaxEnvStringSize, thePrefsFile) != NULL) {
	/*
	 * First strip off new line char
	 */
	if (tempStr[strlen(tempStr)-1] == '\n') {
	    tempStr[strlen(tempStr)-1] = '\0';
	}
	if (tempStr[0] == '\0' || tempStr[0] == '#') {
	    /*
	     * skip empty lines or commented lines
	     */
	    continue;
	}
		
	tempPtr = (char *) ckalloc(strlen(tempStr) + 1);
	strcpy(tempPtr,tempStr);
	fileEnv[i++] = tempPtr;
    }
	
    fileEnv[i] = NULL;
    fclose(thePrefsFile);	
	
    FSpSetDefaultDir(&currentDir);
    return fileEnv;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * MakeFolderEnvVar --
 *
 *	This function creates "environment" variable by taking a prefix and
 *	appending a folder path to a directory.  The directory is specified
 *	by a integer value acceptable by the FindFolder function.
 *
 * Results:
 *	The function returns an *allocated* string.  If the folder doesn't
 *	exist the return string is still allocated and just contains the
 *	given prefix.
 *
 * Side effects:
 *	Memory is allocated and returned to the caller.
 *
 *----------------------------------------------------------------------
 */

static char * 
MakeFolderEnvVar(prefixTag, whichFolder)
    char * prefixTag;		/* Prefix added before result. */
    long whichFolder;		/* Constant for FSpFindFolder. */
{
    char * thePath = NULL;
    char * result = NULL;
    OSErr theErr = noErr;
    Handle theString = NULL;
    FSSpec theFolder;
    short size;
    Tcl_DString pathStr;
    Tcl_DString tagPathStr;
    
    Tcl_DStringInit(&pathStr);
    theErr = FSpFindFolder(kOnSystemDisk, whichFolder, 
	    kDontCreateFolder, &theFolder);
    if (theErr == noErr) {
	theErr = FSpGetFullPath(&theFolder, &size, &theString);
		
	SetHandleSize(theString, size + 1);
	HLock(theString);
	(*theString)[size] = '\0';
	tclPlatform = TCL_PLATFORM_MAC;
	Tcl_DStringAppend(&pathStr, *theString, -1);
	HUnlock(theString);
	DisposeHandle(theString);
		
	Tcl_DStringInit(&tagPathStr);
	Tcl_DStringAppend(&tagPathStr, prefixTag, strlen(prefixTag));
	Tcl_DStringAppend(&tagPathStr, pathStr.string, pathStr.length);
	Tcl_DStringFree(&pathStr);
	
	/*
	 * Don't free tagPathStr - rather make sure it's allocated
	 * and return it as the result.
	 */
	if (tagPathStr.string == tagPathStr.staticSpace) {
	    result = (char *) ckalloc(tagPathStr.length + 1);
	    strcpy(result, tagPathStr.string);
	} else {
	    result = tagPathStr.string;
	}
    } else {
	result = (char *) ckalloc(strlen(prefixTag) + 1);
	strcpy(result, prefixTag);
    }
	
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * PathVariables --
 *
 *  Creates environment variables from the system call FSpFindFolder.
 *  The function generates environment variables for many of the 
 *  commonly used paths on the Macintosh.
 *
 * Results:
 *	ptr to value on success, NULL if error.
 *
 * Side effects:
 *	Memory is allocated and returned to the caller.
 *
 *----------------------------------------------------------------------
 */

static char ** 
PathVariables()
{
    int i = 0;
    char **sysEnv;
    char *thePath = NULL;
    
    sysEnv = (char **) ckalloc((12) * sizeof(char *));

    sysEnv[i++] = MakeFolderEnvVar("PREF_FOLDER=", kPreferencesFolderType);
    sysEnv[i++] = MakeFolderEnvVar("SYS_FOLDER=", kSystemFolderType);
    sysEnv[i++] = MakeFolderEnvVar("TEMP=", kTemporaryFolderType);
    sysEnv[i++] = MakeFolderEnvVar("APPLE_M_FOLDER=", kAppleMenuFolderType);
    sysEnv[i++] = MakeFolderEnvVar("CP_FOLDER=", kControlPanelFolderType);
    sysEnv[i++] = MakeFolderEnvVar("DESK_FOLDER=", kDesktopFolderType);
    sysEnv[i++] = MakeFolderEnvVar("EXT_FOLDER=", kExtensionFolderType);
    sysEnv[i++] = MakeFolderEnvVar("PRINT_MON_FOLDER=",
	    kPrintMonitorDocsFolderType);
    sysEnv[i++] = MakeFolderEnvVar("SHARED_TRASH_FOLDER=",
	    kWhereToEmptyTrashFolderType);
    sysEnv[i++] = MakeFolderEnvVar("TRASH_FOLDER=", kTrashFolderType);
    sysEnv[i++] = MakeFolderEnvVar("START_UP_FOLDER=", kStartupFolderType);
    sysEnv[i++] = NULL;
	
    return sysEnv;
}

/*
 *----------------------------------------------------------------------
 *
 * SystemVariables --
 *
 *  Creates environment variables from various Mac system calls.
 *
 * Results:
 *	ptr to value on success, NULL if error.
 *
 * Side effects:
 *	Memory is allocated and returned to the caller.
 *
 *----------------------------------------------------------------------
 */

static char ** 
SystemVariables()
{
    int i = 0;
    char ** sysEnv;
    char * thePath = NULL;
    Handle theString = NULL;
    FSSpec currentDir;
    short size;
    
    sysEnv = (char **) ckalloc((4) * sizeof(char *));

    /*
     * Get user name from chooser.  It will be assigned to both
     * the USER and LOGIN environment variables.
     */
    thePath = GetUserName();
    sysEnv[i] = (char *) ckalloc(strlen(kLoginnameTag) + strlen(thePath) + 1);
    strcpy(sysEnv[i], kLoginnameTag);
    strcpy(sysEnv[i]+strlen(kLoginnameTag), thePath);
    i++;
    sysEnv[i] = (char *) ckalloc(strlen(kUsernameTag) + strlen(thePath) + 1);
    strcpy(sysEnv[i], kUsernameTag);
    strcpy(sysEnv[i]+strlen(kUsernameTag), thePath);
    i++;

    /*
     * Get 'home' directory
     */
#ifdef kDefaultDirTag
    FSpGetDefaultDir(&currentDir);
    GetFullPath(currentDir.vRefNum, currentDir.parID,
	    currentDir.name, &size, &theString);
    HLock(theString);
    sysEnv[i] = (char *) ckalloc(strlen(kDefaultDirTag) + size + 3);
    strcpy(sysEnv[i], kDefaultDirTag);
    strncpy(sysEnv[i]+strlen(kDefaultDirTag) , *theString, size);
    sysEnv[i][strlen(kDefaultDirTag) + size] = '\0';
    HUnlock(theString);
    DisposeHandle(theString);
    i++;
#endif

    sysEnv[i++] = NULL;
    return sysEnv;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacCreateEnv --
 *
 *	This function allocates and populates the global "environ"
 *	variable.  Entries are in traditional Unix format but variables
 *	are, hopefully, a bit more relevant for the Macintosh.
 *
 * Results:
 *	The number of elements in the newly created environ array.
 *
 * Side effects:
 *	Memory is allocated and pointed too by the environ variable.
 *
 *----------------------------------------------------------------------
 */

int
TclMacCreateEnv()
{
    char ** sysEnv = NULL;
    char ** pathEnv = NULL;
    char ** fileEnv = NULL;
    char ** rezEnv = NULL;
    int count = 0;
    int i, j;

    sysEnv = SystemVariables();
    if (sysEnv != NULL) {
	for (i = 0; sysEnv[i] != NULL; count++, i++) {
	    /* Empty Loop */
	}
    }

    pathEnv = PathVariables();
    if (pathEnv != NULL) {
	for (i = 0; pathEnv[i] != NULL; count++, i++) {
	    /* Empty Loop */
	}
    }

#ifdef kPrefsFile
    fileEnv = FileRCVariables();
    if (fileEnv != NULL) {
	for (i = 0; fileEnv[i] != NULL; count++, i++) {
	    /* Empty Loop */
	}
    }
#endif

#ifdef REZ_ENV
    rezEnv = RezRCVariables();
    if (rezEnv != NULL) {
	for (i = 0; rezEnv[i] != NULL; count++, i++) {
	    /* Empty Loop */
	}
    }
#endif

    /*
     * Create environ variable
     */
    environ = (char **) ckalloc((count + 1) * sizeof(char *));
    j = 0;
	
    if (sysEnv != NULL) {
	for (i = 0; sysEnv[i] != NULL;)
	    environ[j++] = sysEnv[i++];
	ckfree((char *) sysEnv);
    }

    if (pathEnv != NULL) {
	for (i = 0; pathEnv[i] != NULL;)
	    environ[j++] = pathEnv[i++];
	ckfree((char *) pathEnv);
    }

#ifdef kPrefsFile
    if (fileEnv != NULL) {
	for (i = 0; fileEnv[i] != NULL;)
	    environ[j++] = fileEnv[i++];
	ckfree((char *) fileEnv);
    }
#endif

#ifdef REZ_ENV
    if (rezEnv != NULL) {
	for (i = 0; rezEnv[i] != NULL;)
	    environ[j++] = rezEnv[i++];
	ckfree((char *) rezEnv);
    }
#endif

    environ[j] = NULL;
    return j;
}

/*
 *----------------------------------------------------------------------
 *
 * GetUserName --
 *
 *	Get the user login name.
 *
 * Results:
 *  ptr to static string, NULL if error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
GetUserName()
{
    static char buf[33];
    short refnum;
    Handle h;
	
    refnum = CurResFile();
    UseResFile(0);
    h = GetResource('STR ', -16096);
    UseResFile(refnum);
    if (h == NULL) {
	return NULL;
    }
    
    HLock(h);
    strncpy(buf, (*h)+1, **h);
    buf[**h] = '\0';
    HUnlock(h);
    return(buf[0] ? buf : NULL);
}

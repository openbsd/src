/* 
 * tclWinFile.c --
 *
 *      This file contains temporary wrappers around UNIX file handling
 *      functions. These wrappers map the UNIX functions to Win32 HANDLE-style
 *      files, which can be manipulated through the Win32 console redirection
 *      interfaces.
 *
 * Copyright (c) 1995 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinFile.c 1.34 96/04/15 18:28:13
 */

#include <sys/stat.h>
#include "tclInt.h"
#include "tclPort.h"

/*
 * The variable below caches the name of the current working directory
 * in order to avoid repeated calls to getcwd.  The string is malloc-ed.
 * NULL means the cache needs to be refreshed.
 */

static char *currentDir =  NULL;


/*
 *----------------------------------------------------------------------
 *
 * TclCreateTempFile --
 *
 *	This function opens a unique file with the property that it
 *	will be deleted when its file handle is closed.  The temporary
 *	file is created in the system temporary directory.
 *
 * Results:
 *	Returns a valid C file descriptor, or -1 on failure.
 *
 * Side effects:
 *	Creates a new temporary file.
 *
 *----------------------------------------------------------------------
 */

Tcl_File
TclCreateTempFile(contents)
    char *contents;		/* String to write into temp file, or NULL. */
{
    SECURITY_ATTRIBUTES secAttr;
    char name[MAX_PATH];
    HANDLE handle;

    if (!GetTempPath(MAX_PATH, name)
	    || !GetTempFileName(name, "TCL", 0, name)) {
	return NULL;
    }

    secAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAttr.lpSecurityDescriptor = NULL;
    secAttr.bInheritHandle = FALSE;

    handle = CreateFile(name, GENERIC_READ | GENERIC_WRITE, 0, &secAttr,
	    TRUNCATE_EXISTING,
	    FILE_ATTRIBUTE_NORMAL | FILE_FLAG_DELETE_ON_CLOSE, NULL);

    if (handle == INVALID_HANDLE_VALUE) {
	goto error;
    }

    /*
     * Write the file out, doing line translations on the way.
     */

    if (contents != NULL) {
	DWORD result, length;
	char *p;
	
	for (p = contents; *p != '\0'; p++) {
	    if (*p == '\n') {
		length = p - contents;
		if (length > 0) {
		    if (!WriteFile(handle, contents, length, &result, NULL)) {
			goto error;
		    }
		}
		if (!WriteFile(handle, "\r\n", 2, &result, NULL)) {
		    goto error;
		}
		contents = p+1;
	    }
	}
	length = p - contents;
	if (length > 0) {
	    if (!WriteFile(handle, contents, length, &result, NULL)) {
		goto error;
	    }
	}
    }
    if (SetFilePointer(handle, 0, NULL, FILE_BEGIN) == 0xFFFFFFFF) {
	goto error;
    }

    return Tcl_GetFile((ClientData) handle, TCL_WIN_FILE);

  error:
    TclWinConvertError(GetLastError());
    CloseHandle(handle);
    DeleteFile(name);
    return NULL;
}

/*
 *----------------------------------------------------------------------
 *
 * TclOpenFile --
 *
 *	This function wraps the normal system open() to ensure that
 *	files are opened with the _O_NOINHERIT flag set.
 *
 * Results:
 *	Same as open().
 *
 * Side effects:
 *	Same as open().
 *
 *----------------------------------------------------------------------
 */

Tcl_File
TclOpenFile(path, mode)
    char *path;
    int mode;
{
    HANDLE handle;
    DWORD accessMode;
    DWORD createMode;
    DWORD shareMode;
    DWORD flags;
    SECURITY_ATTRIBUTES sec;

    /*
     * Map the access bits to the NT access mode.
     */

    switch (mode & (O_RDONLY | O_WRONLY | O_RDWR)) {
	case O_RDONLY:
	    accessMode = GENERIC_READ;
	    break;
	case O_WRONLY:
	    accessMode = GENERIC_WRITE;
	    break;
	case O_RDWR:
	    accessMode = (GENERIC_READ | GENERIC_WRITE);
	    break;
	default:
	    TclWinConvertError(ERROR_INVALID_FUNCTION);
	    return NULL;
    }

    /*
     * Map the creation flags to the NT create mode.
     */

    switch (mode & (O_CREAT | O_EXCL | O_TRUNC)) {
	case (O_CREAT | O_EXCL):
	case (O_CREAT | O_EXCL | O_TRUNC):
	    createMode = CREATE_NEW;
	    break;
	case (O_CREAT | O_TRUNC):
	    createMode = CREATE_ALWAYS;
	    break;
	case O_CREAT:
	    createMode = OPEN_ALWAYS;
	    break;
	case O_TRUNC:
	case (O_TRUNC | O_EXCL):
	    createMode = TRUNCATE_EXISTING;
	    break;
	default:
	    createMode = OPEN_EXISTING;
	    break;
    }

    /*
     * If the file is not being created, use the existing file attributes.
     */

    flags = 0;
    if (!(mode & O_CREAT)) {
	flags = GetFileAttributes(path);
	if (flags == 0xFFFFFFFF) {
	    flags = 0;
	}
    }

    /*
     * Set up the security attributes so this file is not inherited by
     * child processes.
     */

    sec.nLength = sizeof(sec);
    sec.lpSecurityDescriptor = NULL;
    sec.bInheritHandle = 0;

    /*
     * Set up the file sharing mode.  We want to allow simultaneous access.
     */

    shareMode = FILE_SHARE_READ | FILE_SHARE_WRITE;

    /*
     * Now we get to create the file.
     */

    handle = CreateFile(path, accessMode, shareMode, &sec, createMode, flags,
            (HANDLE) NULL);
    if (handle == INVALID_HANDLE_VALUE) {
	DWORD err = GetLastError();
	if ((err & 0xffffL) == ERROR_OPEN_FAILED) {
	    err = (mode & O_CREAT) ? ERROR_FILE_EXISTS : ERROR_FILE_NOT_FOUND;
	}
        TclWinConvertError(err);
        return NULL;
    }

    return Tcl_GetFile((ClientData) handle, TCL_WIN_FILE);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCloseFile --
 *
 *	Closes a file on Windows.
 *
 * Results:
 *	0 on success, -1 on failure.
 *
 * Side effects:
 *	The file is closed.
 *
 *----------------------------------------------------------------------
 */

int
TclCloseFile(file)
    Tcl_File file;	/* The file to close. */
{
    HANDLE handle;
    int type;

    handle = (HANDLE) Tcl_GetFileInfo(file, &type);

    if (type == TCL_WIN_FILE) {
	if (CloseHandle(handle) == FALSE) {
	    TclWinConvertError(GetLastError());
	    return -1;
	}
    } else {
	panic("Tcl_CloseFile: unexpected file type");
    }

    Tcl_FreeFile(file);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclSeekFile --
 *
 *	Sets the file pointer on a file indicated by the file.
 *
 * Results:
 *	The new position at which the file pointer is after it was
 *	moved, or -1 on failure.
 *
 * Side effects:
 *	May move the position at which subsequent operations on the
 *	file access it.
 *
 *----------------------------------------------------------------------
 */

int
TclSeekFile(file, offset, whence)
    Tcl_File file;	/* File to seek on. */
    int offset;			/* How much to move. */
    int whence;			/* Relative to where? */
{
    DWORD moveMethod;
    DWORD newPos;
    HANDLE handle;
    int type;

    handle = (HANDLE) Tcl_GetFileInfo(file, &type);
    if (type != TCL_WIN_FILE) {
	panic("Tcl_SeekFile: unexpected file type");
    }
    
    if (whence == SEEK_SET) {
        moveMethod = FILE_BEGIN;
    } else if (whence == SEEK_CUR) {
        moveMethod = FILE_CURRENT;
    } else {
        moveMethod = FILE_END;
    }

    newPos = SetFilePointer(handle, offset, NULL, moveMethod);
    if (newPos == 0xFFFFFFFF) {
        TclWinConvertError(GetLastError());
        return -1;
    }
    return newPos;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FindExecutable --
 *
 *	This procedure computes the absolute path name of the current
 *	application, given its argv[0] value.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	The variable tclExecutableName gets filled in with the file
 *	name for the application, if we figured it out.  If we couldn't
 *	figure it out, Tcl_FindExecutable is set to NULL.
 *
 *----------------------------------------------------------------------
 */

void
Tcl_FindExecutable(argv0)
    char *argv0;		/* The value of the application's argv[0]. */
{
    Tcl_DString buffer;
    int length;

    Tcl_DStringInit(&buffer);

    if (tclExecutableName != NULL) {
	ckfree(tclExecutableName);
	tclExecutableName = NULL;
    }

    /*
     * Under Windows we ignore argv0, and return the path for the file used to
     * create this process.
     */

    Tcl_DStringSetLength(&buffer, MAX_PATH+1);
    length = GetModuleFileName(NULL, Tcl_DStringValue(&buffer), MAX_PATH+1);
    if (length > 0) {
	tclExecutableName = (char *) ckalloc((unsigned) (length + 1));
	strcpy(tclExecutableName, Tcl_DStringValue(&buffer));
    }
    Tcl_DStringFree(&buffer);
}

/*
 *----------------------------------------------------------------------
 *
 * TclMatchFiles --
 *
 *	This routine is used by the globbing code to search a
 *	directory for all files which match a given pattern.
 *
 * Results: 
 *	If the tail argument is NULL, then the matching files are
 *	added to the interp->result.  Otherwise, TclDoGlob is called
 *	recursively for each matching subdirectory.  The return value
 *	is a standard Tcl result indicating whether an error occurred
 *	in globbing.
 *
 * Side effects:
 *	None.
 *
 *---------------------------------------------------------------------- */

int
TclMatchFiles(interp, separators, dirPtr, pattern, tail)
    Tcl_Interp *interp;		/* Interpreter to receive results. */
    char *separators;		/* Directory separators to pass to TclDoGlob. */
    Tcl_DString *dirPtr;	/* Contains path to directory to search. */
    char *pattern;		/* Pattern to match against. */
    char *tail;			/* Pointer to end of pattern.  Tail must
				 * point to a location in pattern. */
{
    char drivePattern[4] = "?:\\";
    char *newPattern, *p, *dir, *root, c;
    int length, matchDotFiles;
    int result = TCL_OK;
    int baseLength = Tcl_DStringLength(dirPtr);
    Tcl_DString buffer;
    DWORD atts, volFlags;
    HANDLE handle;
    WIN32_FIND_DATA data;
    BOOL found;

    /*
     * Convert the path to normalized form since some interfaces only
     * accept backslashes.  Also, ensure that the directory ends with a
     * separator character.
     */

    Tcl_DStringInit(&buffer);
    if (baseLength == 0) {
	Tcl_DStringAppend(&buffer, ".", 1);
    } else {
	Tcl_DStringAppend(&buffer, Tcl_DStringValue(dirPtr),
		Tcl_DStringLength(dirPtr));
    }
    for (p = Tcl_DStringValue(&buffer); *p != '\0'; p++) {
	if (*p == '/') {
	    *p = '\\';
	}
    }
    p--;
    if (*p != '\\' && *p != ':') {
	Tcl_DStringAppend(&buffer, "\\", 1);
    }
    dir = Tcl_DStringValue(&buffer);
    
    /*
     * First verify that the specified path is actually a directory.
     */

    atts = GetFileAttributes(dir);
    if ((atts == 0xFFFFFFFF) || ((atts & FILE_ATTRIBUTE_DIRECTORY) == 0)) {
	Tcl_DStringFree(&buffer);
	return TCL_OK;
    }

    /*
     * Next check the volume information for the directory to see whether
     * comparisons should be case sensitive or not.  If the root is null, then
     * we use the root of the current directory.  If the root is just a drive
     * specifier, we use the root directory of the given drive.
     */

    switch (Tcl_GetPathType(dir)) {
	case TCL_PATH_RELATIVE:
	    found = GetVolumeInformation(NULL, NULL, 0, NULL,
		    NULL, &volFlags, NULL, 0);
	    break;
	case TCL_PATH_VOLUME_RELATIVE:
	    if (*dir == '\\') {
		root = NULL;
	    } else {
		root = drivePattern;
		*root = *dir;
	    }
	    found = GetVolumeInformation(root, NULL, 0, NULL,
		    NULL, &volFlags, NULL, 0);
	    break;
	case TCL_PATH_ABSOLUTE:
	    if (dir[1] == ':') {
		root = drivePattern;
		*root = *dir;
		found = GetVolumeInformation(root, NULL, 0, NULL,
			NULL, &volFlags, NULL, 0);
	    } else if (dir[1] == '\\') {
		p = strchr(dir+2, '\\');
		p = strchr(p+1, '\\');
		p++;
		c = *p;
		*p = 0;
		found = GetVolumeInformation(dir, NULL, 0, NULL,
			NULL, &volFlags, NULL, 0);
		*p = c;
	    }
	    break;
    }

    if (!found) {
	Tcl_DStringFree(&buffer);
	TclWinConvertError(GetLastError());
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "couldn't read volume information for \"",
		dirPtr->string, "\": ", Tcl_PosixError(interp), (char *) NULL);
	return TCL_ERROR;
    }
    
    /*
     * If the volume is not case sensitive, then we need to convert the pattern
     * to lower case.
     */

    length = tail - pattern;
    newPattern = ckalloc(length+1);
    if (volFlags & FS_CASE_SENSITIVE) {
	strncpy(newPattern, pattern, length);
	newPattern[length] = '\0';
    } else {
	char *src, *dest;
	for (src = pattern, dest = newPattern; src < tail; src++, dest++) {
	    *dest = (char) tolower(*src);
	}
	*dest = '\0';
    }
    
    /*
     * We need to check all files in the directory, so append a *.*
     * to the path. 
     */


    dir = Tcl_DStringAppend(&buffer, "*.*", 3);

    /*
     * Now open the directory for reading and iterate over the contents.
     */

    handle = FindFirstFile(dir, &data);
    Tcl_DStringFree(&buffer);

    if (handle == INVALID_HANDLE_VALUE) {
	TclWinConvertError(GetLastError());
	Tcl_ResetResult(interp);
	Tcl_AppendResult(interp, "couldn't read directory \"",
		dirPtr->string, "\": ", Tcl_PosixError(interp), (char *) NULL);
	ckfree(newPattern);
	return TCL_ERROR;
    }

    /*
     * Clean up the tail pointer.  Leave the tail pointing to the 
     * first character after the path separator or NULL. 
     */

    if (*tail == '\\') {
	tail++;
    }
    if (*tail == '\0') {
	tail = NULL;
    } else {
	tail++;
    }

    /*
     * Check to see if the pattern needs to compare with dot files.
     */

    if ((newPattern[0] == '.')
	    || ((pattern[0] == '\\') && (pattern[1] == '.'))) {
        matchDotFiles = 1;
    } else {
        matchDotFiles = 0;
    }

    /*
     * Now iterate over all of the files in the directory.
     */

    Tcl_DStringInit(&buffer);
    for (found = 1; found; found = FindNextFile(handle, &data)) {
	char *matchResult;

	/*
	 * Ignore hidden files.
	 */

	if ((data.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) 
		|| (!matchDotFiles && (data.cFileName[0] == '.'))) {
	    continue;
	}

	/*
	 * Check to see if the file matches the pattern.  If the volume is not
	 * case sensitive, we need to convert the file name to lower case.  If
	 * the volume also doesn't preserve case, then we return the lower case
	 * form of the name, otherwise we return the system form.
 	 */

	matchResult = NULL;
	if (!(volFlags & FS_CASE_SENSITIVE)) {
	    Tcl_DStringSetLength(&buffer, 0);
	    Tcl_DStringAppend(&buffer, data.cFileName, -1);
	    for (p = buffer.string; *p != '\0'; p++) {
		*p = (char) tolower(*p);
	    }
	    if (Tcl_StringMatch(buffer.string, newPattern)) {
		if (volFlags & FS_CASE_IS_PRESERVED) {
		    matchResult = data.cFileName;
		} else {
		    matchResult = buffer.string;
		}	
	    }
	} else {
	    if (Tcl_StringMatch(data.cFileName, newPattern)) {
		matchResult = data.cFileName;
	    }
	}

	if (matchResult == NULL) {
	    continue;
	}

	/*
	 * If the file matches, then we need to process the remainder of the
	 * path.  If there are more characters to process, then ensure matching
	 * files are directories and call TclDoGlob. Otherwise, just add the
	 * file to the result.
	 */

	Tcl_DStringSetLength(dirPtr, baseLength);
	Tcl_DStringAppend(dirPtr, matchResult, -1);
	if (tail == NULL) {
	    Tcl_AppendElement(interp, dirPtr->string);
	} else {
	    atts = GetFileAttributes(dirPtr->string);
	    if (atts & FILE_ATTRIBUTE_DIRECTORY) {
		Tcl_DStringAppend(dirPtr, "/", 1);
		result = TclDoGlob(interp, separators, dirPtr, tail);
		if (result != TCL_OK) {
		    break;
		}
	    }
	}
    }

    Tcl_DStringFree(&buffer);
    FindClose(handle);
    ckfree(newPattern);
    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetStdHandles --
 *
 *	This function returns the file handles for standard I/O.
 *
 * Results:
 *	Sets the arguments to the standard file handles.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
TclGetStdHandles(stdinPtr, stdoutPtr, stderrPtr)
    Tcl_File *stdinPtr;
    Tcl_File *stdoutPtr;
    Tcl_File *stderrPtr;
{
    HANDLE hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    HANDLE hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    HANDLE hStdError = GetStdHandle(STD_ERROR_HANDLE);

    *stdinPtr = Tcl_GetFile((ClientData) hStdInput, TCL_WIN_FILE);
    *stdoutPtr = Tcl_GetFile((ClientData) hStdOutput, TCL_WIN_FILE);
    *stderrPtr = Tcl_GetFile((ClientData) hStdError, TCL_WIN_FILE);
}

/*
 *----------------------------------------------------------------------
 *
 * TclChdir --
 *
 *	Change the current working directory.
 *
 * Results:
 *	The result is a standard Tcl result.  If an error occurs and 
 *	interp isn't NULL, an error message is left in interp->result.
 *
 * Side effects:
 *	The working directory for this application is changed.  Also
 *	the cache maintained used by TclGetCwd is deallocated and
 *	set to NULL.
 *
 *----------------------------------------------------------------------
 */

int
TclChdir(interp, dirName)
    Tcl_Interp *interp;		/* If non NULL, used for error reporting. */
    char *dirName;     		/* Path to new working directory. */
{
    if (currentDir != NULL) {
	ckfree(currentDir);
	currentDir = NULL;
    }
    if (!SetCurrentDirectory(dirName)) {
	TclWinConvertError(GetLastError());
	if (interp != NULL) {
	    Tcl_AppendResult(interp, "couldn't change working directory to \"",
		    dirName, "\": ", Tcl_PosixError(interp), (char *) NULL);
	}
	return TCL_ERROR;
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetCwd --
 *
 *	Return the path name of the current working directory.
 *
 * Results:
 *	The result is the full path name of the current working
 *	directory, or NULL if an error occurred while figuring it
 *	out.  If an error occurs and interp isn't NULL, an error
 *	message is left in interp->result.
 *
 * Side effects:
 *	The path name is cached to avoid having to recompute it
 *	on future calls;  if it is already cached, the cached
 *	value is returned.
 *
 *----------------------------------------------------------------------
 */

char *
TclGetCwd(interp)
    Tcl_Interp *interp;		/* If non NULL, used for error reporting. */
{
    char buffer[MAXPATHLEN+1], *bufPtr;

    if (currentDir == NULL) {
	if (GetCurrentDirectory(MAXPATHLEN+1, buffer) == 0) {
	    TclWinConvertError(GetLastError());
	    if (interp != NULL) {
		if (errno == ERANGE) {
		    interp->result = "working directory name is too long";
		} else {
		    Tcl_AppendResult(interp,
			    "error getting working directory name: ",
			    Tcl_PosixError(interp), (char *) NULL);
		}
	    }
	    return NULL;
	}
	/*
	 * Watch for the wierd Windows '95 c:\\UNC syntax.
	 */

	if (buffer[0] != '\0' && buffer[1] == ':' && buffer[2] == '\\'
		&& buffer[3] == '\\') {
	    bufPtr = &buffer[2];
	} else {
	    bufPtr = buffer;
	}
	currentDir = (char *) ckalloc((unsigned) (strlen(bufPtr) + 1));
	strcpy(currentDir, bufPtr);

	/*
	 * Convert to forward slashes for easier use in scripts.
	 */

	for (bufPtr = currentDir; *bufPtr != '\0'; bufPtr++) {
	    if (*bufPtr == '\\') {
		*bufPtr = '/';
	    }
	}
    }
    return currentDir;
}

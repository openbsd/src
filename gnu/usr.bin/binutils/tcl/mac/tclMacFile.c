/* 
 * tclMacFile.c --
 *
 *      This file implements the channel drivers for Macintosh
 *	files.  It also comtains Macintosh version of other Tcl
 *	functions that deal with the file system.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacFile.c 1.35 96/04/09 16:49:54
 */

#include "tclInt.h"
#include "tclPort.h"
#include "tclMacInt.h"
#include <dirent.h>
#include <Aliases.h>
#include <Errors.h>
#include <FullPath.h>
#include <Processes.h>
#include <Strings.h>
#include <MoreFiles.h>
#include <MoreFilesExtras.h>

/*
 * The following are flags returned by GetOpenMode.  They
 * are or'd together to determine how opening and handling
 * a file should occur.
 */

#define TCL_RDONLY		(1<<0)
#define TCL_WRONLY		(1<<1)
#define TCL_RDWR		(1<<2)
#define TCL_CREAT		(1<<3)
#define TCL_TRUNC		(1<<4)
#define TCL_APPEND		(1<<5)
#define TCL_ALWAYS_APPEND	(1<<6)
#define TCL_EXCL		(1<<7)
#define TCL_NOCTTY		(1<<8)
#define TCL_NONBLOCK		(1<<9)
#define TCL_RW_MODES 		(TCL_RDONLY|TCL_WRONLY|TCL_RDWR)

/*
 * This structure describes per-instance state of a 
 * macintosh file based channel.
 */

typedef struct FileState {
    Tcl_Channel fileChan;	/* Pointer to the channel for this file. */
    int appendMode;		/* Flag to tell if in O_APPEND mode or not. */
    int volumeRef;		/* Flag to tell if in O_APPEND mode or not. */
} FileState;

/*
 * The variable below caches the name of the current working directory
 * in order to avoid repeated calls to getcwd.  The string is malloc-ed.
 * NULL means the cache needs to be refreshed.
 */

static char *currentDir =  NULL;

/*
 * Static routines for this file:
 */

static int		FileBlockMode _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File inFile, Tcl_File outFile, int mode));
static int		FileClose _ANSI_ARGS_((ClientData instanceData,
			    Tcl_Interp *interp, Tcl_File inFile,
			    Tcl_File outFile));
static int		FileInput _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File inFile, char *buf, int toRead,
			    int *errorCode));
static int		FileOutput _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File outFile, char *buf, int toWrite,
			    int *errorCode));
static int		FileSeek _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File inFile, Tcl_File outFile, long offset,
			    int mode, int *errorCode));
static int		GetOpenMode _ANSI_ARGS_((Tcl_Interp *interp,
        		    char *string));
static Tcl_Channel	OpenFileChannel _ANSI_ARGS_((char *fileName, int mode, 
			    int permissions, int *errorCodePtr));
static int		OSErrorToPosixError _ANSI_ARGS_((int error));

/*
 * This variable describes the channel type structure for file based IO.
 */

static Tcl_ChannelType fileChannelType = {
    "file",			/* Type name. */
    FileBlockMode,		/* Set blocking or
                                 * non-blocking mode.*/
    FileClose,			/* Close proc. */
    FileInput,			/* Input proc. */
    FileOutput,			/* Output proc. */
    FileSeek,			/* Seek proc. */
    NULL,			/* Set option proc. */
    NULL,			/* Get option proc. */
};


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
    FSSpec spec;
    OSErr err;
    Boolean isFolder;
    long dirID;

    if (currentDir != NULL) {
	ckfree(currentDir);
	currentDir = NULL;
    }

    err = FSpLocationFromPath(strlen(dirName), dirName, &spec);
    if (err != noErr) {
	errno = ENOENT;
	goto chdirError;
    }
    
    err = FSpGetDirectoryID(&spec, &dirID, &isFolder);
    if (err != noErr) {
	errno = ENOENT;
	goto chdirError;
    }

    if (isFolder != true) {
	errno = ENOTDIR;
	goto chdirError;
    }

    err = FSpSetDefaultDir(&spec);
    if (err != noErr) {
	switch (err) {
	    case afpAccessDenied:
		errno = EACCES;
		break;
	    default:
		errno = ENOENT;
	}
	goto chdirError;
    }

    return TCL_OK;
    chdirError:
    if (interp != NULL) {
	Tcl_AppendResult(interp, "couldn't change working directory to \"",
		dirName, "\": ", Tcl_PosixError(interp), (char *) NULL);
    }
    return TCL_ERROR;
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
    FSSpec theSpec;
    short length;
    Handle pathHandle = NULL;
    
    if (currentDir == NULL) {
	if (FSpGetDefaultDir(&theSpec) != noErr) {
	    if (interp != NULL) {
		interp->result = "error getting working directory name";
	    }
	    return NULL;
	}
	if (FSpGetFullPath(&theSpec, &length, &pathHandle) != noErr) {
	    if (interp != NULL) {
		interp->result = "error getting working directory name";
	    }
	    return NULL;
	}
	HLock(pathHandle);
	currentDir = (char *) ckalloc((unsigned) (length + 1));
	strncpy(currentDir, *pathHandle, length);
	currentDir[length] = '\0';
	HUnlock(pathHandle);
	DisposeHandle(pathHandle);	
    }
    return currentDir;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitPid --
 *
 *	Fakes a call to wait pid.
 *
 * Results:
 *	Always returns -1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitPid(pid, statPtr, options)
    int pid;
    int *statPtr;
    int options;
{
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateTempFile --
 *
 *	This function creates a temporary file initialized with an
 *	optional string, and returns a file handle with the file pointer
 *	at the beginning of the file.
 *
 * Results:
 *	A handle to a file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_File
TclCreateTempFile(contents)
    char *contents;		/* String to write into temp file, or NULL. */
{
    char fileName[L_tmpnam];
    Tcl_File file;
    int length = (contents == NULL) ? 0 : strlen(contents);

    tmpnam(fileName);
    file = TclOpenFile(fileName, O_RDWR|O_CREAT|O_TRUNC);
    unlink(fileName);

    if ((file != NULL) && (length > 0)) {
	int fd = (int)Tcl_GetFileInfo(file, NULL);
	while (1) {
	    if (write(fd, contents, length) != -1) {
		break;
	    } else if (errno != EINTR) {
		close(fd);
		Tcl_FreeFile(file);
		return NULL;
	    }
	}
	lseek(fd, 0, SEEK_SET);
    }
    return file;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_FindExecutable --
 *
 *	This procedure computes the absolute path name of the current
 *	application, given its argv[0] value.  However, this
 *	implementation doesn't use of need the argv[0] value.  NULL
 *	may be passed in its place.
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
    ProcessSerialNumber psn;
    ProcessInfoRec info;
    Str63 appName;
    FSSpec fileSpec;
    short pathLength;
    Handle pathName = NULL;
    OSErr err;
    
    GetCurrentProcess(&psn);
    info.processInfoLength = sizeof(ProcessInfoRec);
    info.processName = appName;
    info.processAppSpec = &fileSpec;
    GetProcessInformation(&psn, &info);

    if (tclExecutableName != NULL) {
	ckfree(tclExecutableName);
	tclExecutableName = NULL;
    }
    
    err = FSpGetFullPath(&fileSpec, &pathLength, &pathName);

    tclExecutableName = (char *) ckalloc((unsigned) pathLength + 1);
    HLock(pathName);
    strncpy(tclExecutableName, *pathName, pathLength);
    tclExecutableName[pathLength] = '\0';
    HUnlock(pathName);
    DisposeHandle(pathName);	
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetUserHome --
 *
 *	This function takes the passed in user name and finds the
 *	corresponding home directory specified in the password file.
 *
 * Results:
 *	On a Macintosh we always return a NULL.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TclGetUserHome(name, bufferPtr)
    char *name;			/* User name to use to find home directory. */
    Tcl_DString *bufferPtr;	/* May be used to hold result.  Must not hold
				 * anything at the time of the call, and need
				 * not even be initialized. */
{
    return NULL;
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
    char *dirName, *patternEnd = tail;
    char savedChar;
    int result = TCL_OK;
    int baseLength = Tcl_DStringLength(dirPtr);
    CInfoPBRec pb;
    OSErr err;
    FSSpec dirSpec;
    Boolean isDirectory;
    long dirID;
    short itemIndex;
    Str255 fileName;
    

    /*
     * Make sure that the directory part of the name really is a
     * directory.
     */

    dirName = dirPtr->string;
    FSpLocationFromPath(strlen(dirName), dirName, &dirSpec);
    err = FSpGetDirectoryID(&dirSpec, &dirID, &isDirectory);
    if ((err != noErr) || !isDirectory) {
	return TCL_OK;
    }

    /*
     * Now open the directory for reading and iterate over the contents.
     */

    pb.hFileInfo.ioVRefNum = dirSpec.vRefNum;
    pb.hFileInfo.ioDirID = dirID;
    pb.hFileInfo.ioNamePtr = (StringPtr) fileName;
    pb.hFileInfo.ioFDirIndex = itemIndex = 1;
    
    /*
     * Clean up the end of the pattern and the tail pointer.  Leave
     * the tail pointing to the first character after the path separator
     * following the pattern, or NULL.  Also, ensure that the pattern
     * is null-terminated.
     */

    if (*tail == '\\') {
	tail++;
    }
    if (*tail == '\0') {
	tail = NULL;
    } else {
	tail++;
    }
    savedChar = *patternEnd;
    *patternEnd = '\0';

    while (1) {
	pb.hFileInfo.ioFDirIndex = itemIndex;
	pb.hFileInfo.ioDirID = dirID;
	err = PBGetCatInfoSync(&pb);
	if (err != noErr) {
	    break;
	}

	/*
	 * Now check to see if the file matches.  If there are more
	 * characters to be processed, then ensure matching files are
	 * directories before calling TclDoGlob. Otherwise, just add
	 * the file to the result.
	 */

	p2cstr(fileName);
	if (Tcl_StringMatch((char *) fileName, pattern)) {
	    Tcl_DStringSetLength(dirPtr, baseLength);
	    Tcl_DStringAppend(dirPtr, (char *) fileName, -1);
	    if (tail == NULL) {
		if ((dirPtr->length > 1) &&
			(strchr(dirPtr->string+1, ':') == NULL)) {
		    Tcl_AppendElement(interp, dirPtr->string+1);
		} else {
		    Tcl_AppendElement(interp, dirPtr->string);
		}
	    } else if ((pb.hFileInfo.ioFlAttrib & ioDirMask) != 0) {
		Tcl_DStringAppend(dirPtr, ":", 1);
		result = TclDoGlob(interp, separators, dirPtr, tail);
		if (result != TCL_OK) {
		    break;
		}
	    }
	}
	
	itemIndex++;
    }
    *patternEnd = savedChar;

    return result;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacStat --
 *
 *	This function replaces the library version of stat.  The stat
 *	function provided by most Mac compiliers is rather broken and
 *	incomplete.
 *
 * Results:
 *	See stat documentation.
 *
 * Side effects:
 *	See stat documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclMacStat(path, buf)
    char *path;
    struct stat *buf;
{
    HFileInfo fpb;
    HVolumeParam vpb;
    OSErr err;
    FSSpec fileSpec;
    Boolean isDirectory;
    long dirID;

    err = FSpLocationFromPath(strlen(path), path, &fileSpec);
    if (err != noErr) {
	errno = OSErrorToPosixError(err);
	return -1;
    }
    
    /*
     * Fill the fpb & vpb struct up with info about file or directory.
     */
    FSpGetDirectoryID(&fileSpec, &dirID, &isDirectory);
    vpb.ioVRefNum = fpb.ioVRefNum = fileSpec.vRefNum;
    vpb.ioNamePtr = fpb.ioNamePtr = fileSpec.name;
    if (isDirectory) {
	fpb.ioDirID = fileSpec.parID;
    } else {
	fpb.ioDirID = dirID;
    }

    fpb.ioFDirIndex = 0;
    err = PBGetCatInfoSync((CInfoPBPtr)&fpb);
    if (err == noErr) {
	vpb.ioVolIndex = 0;
	err = PBHGetVInfoSync((HParmBlkPtr)&vpb);
	if (err == noErr && buf != NULL) {
	    /* 
	     * Use the Volume Info & File Info to fill out stat buf.
	     */
	    if (fpb.ioFlAttrib & 0x10) {
		buf->st_mode = S_IFDIR;
		buf->st_nlink = 2;
	    } else {
		buf->st_nlink = 1;
		if (fpb.ioFlFndrInfo.fdFlags & 0x8000) {
		    buf->st_mode = S_IFLNK;
		} else {
		    buf->st_mode = S_IFREG;
		}
	    }
	    buf->st_ino = fpb.ioDirID;
	    buf->st_dev = fpb.ioVRefNum;
	    buf->st_uid = -1;
	    buf->st_gid = -1;
	    buf->st_rdev = 0;
	    buf->st_size = fpb.ioFlLgLen;
	    buf->st_atime = buf->st_mtime = fpb.ioFlMdDat;
	    buf->st_ctime = fpb.ioFlCrDat;
	    buf->st_blksize = vpb.ioVAlBlkSiz;
	    buf->st_blocks = (buf->st_size + buf->st_blksize - 1) / buf->st_blksize;
	}
    }

    if (err != noErr) {
	errno = OSErrorToPosixError(err);
    }
    
    return (err == noErr ? 0 : -1);
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacAccess --
 *
 *	This function replaces the library version of access.  The
 *	access function provided by most Mac compiliers is rather 
 *	broken or incomplete.
 *
 * Results:
 *	See access documentation.
 *
 * Side effects:
 *	See access documentation.
 *
 *----------------------------------------------------------------------
 */

int
TclMacAccess(path, mode)
    const char *path;
    int mode;
{
    HFileInfo fpb;
    HVolumeParam vpb;
    OSErr err;
    FSSpec fileSpec;
    Boolean isDirectory;
    long dirID;
    int full_mode = 0;

    err = FSpLocationFromPath(strlen(path), (char *) path, &fileSpec);
    if (err != noErr) {
	errno = OSErrorToPosixError(err);
	return -1;
    }
    
    /*
     * Fill the fpb & vpb struct up with info about file or directory.
     */
    FSpGetDirectoryID(&fileSpec, &dirID, &isDirectory);
    vpb.ioVRefNum = fpb.ioVRefNum = fileSpec.vRefNum;
    vpb.ioNamePtr = fpb.ioNamePtr = fileSpec.name;
    if (isDirectory) {
	fpb.ioDirID = fileSpec.parID;
    } else {
	fpb.ioDirID = dirID;
    }

    fpb.ioFDirIndex = 0;
    err = PBGetCatInfoSync((CInfoPBPtr)&fpb);
    if (err == noErr) {
	vpb.ioVolIndex = 0;
	err = PBHGetVInfoSync((HParmBlkPtr)&vpb);
	if (err == noErr) {
	    /* 
	     * Use the Volume Info & File Info to determine
	     * access information.  If we have got this far
	     * we know the directory is searchable or the file
	     * exists.  (We have F_OK)
	     */

	    /*
	     * Check to see if the volume is hardware or
	     * software locked.  If so we arn't W_OK.
	     */
	    if (mode & W_OK) {
		if ((vpb.ioVAtrb & 0x0080) || (vpb.ioVAtrb & 0x8000)) {
		    errno = EROFS;
		    return -1;
		}
		if ((fpb.ioFlAttrib & 0x10) && (fpb.ioFlAttrib & 0x01)) {
		    errno = EACCES;
		    return -1;
		}
	    }
	    
	    /*
	     * Directories are always searchable and executable.  But only 
	     * files of type 'APPL' are executable.
	     */
	    if (!(fpb.ioFlAttrib & 0x10) && (mode & X_OK)
	    	&& (fpb.ioFlFndrInfo.fdType != 'APPL')) {
		return -1;
	    }
	}
    }

    if (err != noErr) {
	errno = OSErrorToPosixError(err);
	return -1;
    }
    
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * TclMacFOpenHack --
 *
 *	This function replaces fopen.  It supports paths with alises.
 *	Note, remember to undefine the fopen macro!
 *
 * Results:
 *	See fopen documentation.
 *
 * Side effects:
 *	See fopen documentation.
 *
 *----------------------------------------------------------------------
 */

#undef fopen
FILE *
TclMacFOpenHack(path, mode)
    const char *path;
    const char *mode;
{
    OSErr err;
    FSSpec fileSpec;
    Handle pathString = NULL;
    short size;
    FILE * f;
    
    err = FSpLocationFromPath(strlen(path), (char *) path, &fileSpec);
    if (err != noErr) {
	return NULL;
    }
    err = FSpGetFullPath(&fileSpec, &size, &pathString);
    if (err != noErr) {
	return NULL;
    }
    
    SetHandleSize(pathString, size + 1);
    HLock(pathString);
    (*pathString)[size] = '\0';
    f = fopen(*pathString, mode);
    HUnlock(pathString);
    DisposeHandle(pathString);
    return f;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_OpenFileChannel --
 *
 *	Open an File based channel on Unix systems.
 *
 * Results:
 *	The new channel or NULL. If NULL, the output argument
 *	errorCodePtr is set to a POSIX error.
 *
 * Side effects:
 *	May open the channel and may cause creation of a file on the
 *	file system.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_OpenFileChannel(interp, fileName, modeString, permissions)
    Tcl_Interp *interp;			/* Interpreter for error reporting;
                                         * can be NULL. */
    char *fileName;			/* Name of file to open. */
    char *modeString;			/* A list of POSIX open modes or
                                         * a string such as "rw". */
    int permissions;			/* If the open involves creating a
                                         * file, with what modes to create
                                         * it? */
{
    Tcl_Channel chan;
    int mode;
    char *nativeName;
    Tcl_DString buffer;
    int errorCode;
    
    mode = GetOpenMode(interp, modeString);
    if (mode == -1) {
	return NULL;
    }

    nativeName = Tcl_TranslateFileName(interp, fileName, &buffer);
    if (nativeName == NULL) {
	return NULL;
    }

    chan = OpenFileChannel(nativeName, mode, permissions, &errorCode);
    Tcl_DStringFree(&buffer);

    if (chan == NULL) {
	Tcl_SetErrno(errorCode);
	if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "couldn't open \"", fileName, "\": ",
                    Tcl_PosixError(interp), (char *) NULL);
        }
	return NULL;
    }
    
    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * FileOutput--
 *
 *	Writes the given output on the IO channel. Returns count of how
 *	many characters were actually written, and an error indication.
 *
 * Results:
 *	A count of how many characters were written is returned and an
 *	error indication is returned in an output argument.
 *
 * Side effects:
 *	Writes output on the actual channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
OpenFileChannel(fileName, mode, permissions, errorCodePtr)
    char *fileName;			/* Name of file to open. */
    int mode;				/* Mode for opening file. */
    int permissions;			/* If the open involves creating a
                                         * file, with what modes to create
                                         * it? */
    int *errorCodePtr;			/* Where to store error code. */
{
    int channelPermissions;
    Tcl_File file;
    Tcl_Channel chan;
    char macPermision;
    FSSpec fileSpec;
    OSErr err;
    short fileRef;
    FileState *fileState;
    char channelName[64];
    
    /*
     * Note we use fsRdWrShPerm instead of fsRdWrPerm which allows shared
     * writes on a file.  This isn't common on a mac but is common with 
     * Windows and UNIX and the feature is used by Tcl.
     */

    switch (mode & (TCL_RDONLY | TCL_WRONLY | TCL_RDWR)) {
	case TCL_RDWR:
	    channelPermissions = (TCL_READABLE | TCL_WRITABLE);
	    macPermision = fsRdWrShPerm;
	    break;
	case TCL_WRONLY:
	    /*
	     * Mac's fsRdPerm permission actually defaults to fsRdWrPerm because
	     * the Mac OS doesn't realy support write only access.  We explicitly
	     * set the permission fsRdWrShPerm so that we can have shared write
	     * access.
	     */
	    channelPermissions = TCL_WRITABLE;
	    macPermision = fsRdWrShPerm;
	    break;
	case TCL_RDONLY:
	default:
	    channelPermissions = TCL_READABLE;
	    macPermision = fsRdPerm;
	    break;
    }
     
    err = FSpLocationFromPath(strlen(fileName), fileName, &fileSpec);
    if ((err != noErr) && (err != fnfErr)) {
	*errorCodePtr = errno = OSErrorToPosixError(err);
	Tcl_SetErrno(errno);
	return NULL;
    }

    if ((err == fnfErr) && (mode & TCL_CREAT)) {
	err = HCreate(fileSpec.vRefNum, fileSpec.parID, fileSpec.name, 'MPW ', 'TEXT');
	if (err != noErr) {
	    *errorCodePtr = errno = OSErrorToPosixError(err);
	    Tcl_SetErrno(errno);
	    return NULL;
	}
    } else if ((mode & TCL_CREAT) && (mode & TCL_EXCL)) {
        *errorCodePtr = errno = EEXIST;
	Tcl_SetErrno(errno);
        return NULL;
    }

    err = HOpenDF(fileSpec.vRefNum, fileSpec.parID, fileSpec.name, macPermision, &fileRef);
    if (err != noErr) {
	*errorCodePtr = errno = OSErrorToPosixError(err);
	Tcl_SetErrno(errno);
	return NULL;
    }

    if (mode & TCL_TRUNC) {
	SetEOF(fileRef, 0);
    }
    
    file = Tcl_GetFile((ClientData) fileRef, TCL_MAC_FILE);
    sprintf(channelName, "file%d", (int) Tcl_GetFileInfo(file, NULL));
    fileState = (FileState *) ckalloc((unsigned) sizeof(FileState));
    chan = Tcl_CreateChannel(&fileChannelType, channelName,
            (channelPermissions & TCL_READABLE) ? file : NULL,
            (channelPermissions & TCL_WRITABLE) ? file : NULL,
            (ClientData) fileState);
    if (chan == (Tcl_Channel) NULL) {
	*errorCodePtr = errno = EFAULT;
	Tcl_SetErrno(errno);
        Tcl_FreeFile(file);
	FSClose(fileRef);
	ckfree((char *) fileState);
        return NULL;
    }

    fileState->fileChan = chan;
    fileState->volumeRef = fileSpec.vRefNum;
    if (mode & TCL_ALWAYS_APPEND) {
	fileState->appendMode = true;
    } else {
	fileState->appendMode = false;
    }
        
    if ((mode & TCL_ALWAYS_APPEND) || (mode & TCL_APPEND)) {
        if (Tcl_Seek(chan, 0, SEEK_END) < 0) {
	    *errorCodePtr = errno = EFAULT;
	    Tcl_SetErrno(errno);
            Tcl_Close(NULL, chan);
            FSClose(fileRef);
            ckfree((char *) fileState);
            return NULL;
        }
    }
    
    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * FileOutput--
 *
 *	Writes the given output on the IO channel. Returns count of how
 *	many characters were actually written, and an error indication.
 *
 * Results:
 *	A count of how many characters were written is returned and an
 *	error indication is returned in an output argument.
 *
 * Side effects:
 *	Writes output on the actual channel.
 *
 *----------------------------------------------------------------------
 */

static int
FileOutput(instanceData, outFile, buffer, toWrite, errorCodePtr)
    ClientData instanceData;		/* Unused. */
    Tcl_File outFile;			/* Output device for channel. */
    char *buffer;			/* The data buffer. */
    int toWrite;			/* How many bytes to write? */
    int *errorCodePtr;			/* Where to store error code. */
{
    FileState *fileState = (FileState *) instanceData;
    short fileRef;
    long length = toWrite;
    OSErr err;

    *errorCodePtr = 0;
    errno = 0;
    fileRef = (short) Tcl_GetFileInfo(outFile, NULL);
    
    if (fileState->appendMode == true) {
	FileSeek(instanceData, outFile, NULL, 0, SEEK_END, errorCodePtr);
	*errorCodePtr = 0;
    }
    
    err = FSWrite(fileRef, &length, buffer);
    if (err == noErr) {
	err = FlushFile(fileRef);
    } else {
	*errorCodePtr = errno = OSErrorToPosixError(err);
	return -1;
    }
    return length;
}

/*
 *----------------------------------------------------------------------
 *
 * FileInput --
 *
 *	Reads input from the IO channel into the buffer given. Returns
 *	count of how many bytes were actually read, and an error indication.
 *
 * Results:
 *	A count of how many bytes were read is returned and an error
 *	indication is returned in an output argument.
 *
 * Side effects:
 *	Reads input from the actual channel.
 *
 *----------------------------------------------------------------------
 */

int
FileInput(instanceData, inFile, buffer, bufSize, errorCodePtr)
    ClientData instanceData;		/* Unused. */
    Tcl_File inFile;			/* Input device for channel. */
    char *buffer;			/* Where to store data read. */
    int bufSize;			/* How much space is available
                                         * in the buffer? */
    int *errorCodePtr;			/* Where to store error code. */
{
    short fileRef;
    OSErr err;
    long length = bufSize;

    *errorCodePtr = 0;
    errno = 0;
    fileRef = (short) Tcl_GetFileInfo(inFile, NULL);
    err = FSRead(fileRef, &length, buffer);
    if ((err == noErr) || (err == eofErr)) {
	return length;
    } else {
	switch (err) {
	    case ioErr:
		*errorCodePtr = errno = EIO;
	    case afpAccessDenied:
		*errorCodePtr = errno = EACCES;
	    default:
		*errorCodePtr = errno = EINVAL;
	}
        return -1;	
    }
    *errorCodePtr = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * FileSeek --
 *
 *	Seeks on an IO channel. Returns the new position.
 *
 * Results:
 *	-1 if failed, the new position if successful. If failed, it
 *	also sets *errorCodePtr to the error code.
 *
 * Side effects:
 *	Moves the location at which the channel will be accessed in
 *	future operations.
 *
 *----------------------------------------------------------------------
 */

static int
FileSeek(instanceData, inFile, outFile, offset, mode, errorCodePtr)
    ClientData instanceData;			/* Unused. */
    Tcl_File inFile, outFile;			/* Input and output
                                                 * devices for channel. */
    long offset;				/* Offset to seek to. */
    int mode;					/* Relative to where
                                                 * should we seek? */
    int *errorCodePtr;				/* To store error code. */
{
    short fileRef;
    IOParam pb;
    OSErr err;

    *errorCodePtr = 0;
    if (inFile != (Tcl_File) NULL) {
        fileRef = (short) Tcl_GetFileInfo(inFile, NULL);
    } else if (outFile != (Tcl_File) NULL) {
        fileRef = (short) Tcl_GetFileInfo(outFile, NULL);
    } else {
        *errorCodePtr = EFAULT;
        return -1;
    }
    pb.ioCompletion = NULL;
    pb.ioRefNum = fileRef;
    if (mode == SEEK_SET) {
	pb.ioPosMode = fsFromStart;
    } else if (mode == SEEK_END) {
	pb.ioPosMode = fsFromLEOF;
    } else if (mode == SEEK_CUR) {
	err = PBGetFPosSync((ParmBlkPtr) &pb);
	if (pb.ioResult == noErr) {
	    if (offset == 0) {
		return pb.ioPosOffset;
	    }
	    offset += pb.ioPosOffset;
	}
	pb.ioPosMode = fsFromStart;
    }
    pb.ioPosOffset = offset;
    err = PBSetFPosSync((ParmBlkPtr) &pb);
    if (pb.ioResult == noErr){
	return pb.ioPosOffset;
    } else if (pb.ioResult == eofErr) {
	long currentEOF, newEOF;
	long buffer, i, length;
	
	err = PBGetEOFSync((ParmBlkPtr) &pb);
	currentEOF = (long) pb.ioMisc;
	if (mode == SEEK_SET) {
	    newEOF = offset;
	} else if (mode == SEEK_END) {
	    newEOF = offset + currentEOF;
	} else if (mode == SEEK_CUR) {
	    err = PBGetFPosSync((ParmBlkPtr) &pb);
	    newEOF = offset + pb.ioPosOffset;
	}
	
	/*
	 * Write 0's to the new EOF.
	 */
	pb.ioPosOffset = 0;
	pb.ioPosMode = fsFromLEOF;
	err = PBGetFPosSync((ParmBlkPtr) &pb);
	length = 1;
	buffer = 0;
	for (i = 0; i < (newEOF - currentEOF); i++) {
	    err = FSWrite(fileRef, &length, &buffer);
	}
	err = PBGetFPosSync((ParmBlkPtr) &pb);
	if (pb.ioResult == noErr){
	    return pb.ioPosOffset;
	}
    }
    *errorCodePtr = errno = OSErrorToPosixError(err);
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * FileBlockMode --
 *
 *	Set blocking or non-blocking mode on channel.  Macintosh files
 *	can never really be set to blocking or non-blocking modes.
 *	However, we don't generate an error - we just return success.
 *
 * Results:
 *	0 if successful, errno when failed.
 *
 * Side effects:
 *	Sets the device into blocking or non-blocking mode.
 *
 *----------------------------------------------------------------------
 */

static int
FileBlockMode(instanceData, inFile, outFile, mode)
    ClientData instanceData;		/* Unused. */
    Tcl_File inFile, outFile;		/* Input, output for channel. */
    int mode;				/* The mode to set. */
{
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * FileClose --
 *
 *	Closes the IO channel.
 *
 * Results:
 *	0 if successful, the value of errno if failed.
 *
 * Side effects:
 *	Closes the physical channel
 *
 *----------------------------------------------------------------------
 */

static int
FileClose(instanceData, interp, inFile, outFile)
    ClientData instanceData;	/* Unused. */
    Tcl_Interp *interp;		/* Unused. */
    Tcl_File inFile;		/* Input file to close. */
    Tcl_File outFile;		/* Output file to close. */
{
    FileState *fileState = (FileState *) instanceData;
    int errorCode = 0;
    short fileRef;
    OSErr err;

    /*
     * Check for read/write file so we only close it once.
     */

    if (inFile == outFile) {
	outFile = NULL;
    }

    if (inFile != NULL) {
        fileRef = (short) Tcl_GetFileInfo(inFile, NULL);
        Tcl_FreeFile(inFile);
	err = FSClose(fileRef);
	FlushVol(NULL, fileState->volumeRef);
	if (err != noErr) {
	    errorCode = errno = OSErrorToPosixError(err);
	    panic("error during file close");
	}
    }

    if (outFile != NULL) {
        fileRef = (short) Tcl_GetFileInfo(outFile, NULL);
        Tcl_FreeFile(outFile);        
	err = FSClose(fileRef);
	FlushVol(NULL, fileState->volumeRef);
	if (err != noErr) {
	    errorCode = errno = OSErrorToPosixError(err);
	    panic("error during file close");
	}
    }

    ckfree((char *) fileState);
    Tcl_SetErrno(errorCode);
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * OSErrorToPosixError --
 *
 *	Given a Macintosh OSErr return the appropiate POSIX error.
 *
 * Results:
 *	A Posix error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
OSErrorToPosixError(error)
    int error;	/* A Macintosh error. */
{
    switch (error) {
	case noErr:
	    return 0;
	case bdNamErr:
	    return ENAMETOOLONG;
	case afpObjectTypeErr:
	    return ENOTDIR;
	case fnfErr:
	case dirNFErr:
	    return ENOENT;
	case dupFNErr:
	    return EEXIST;
	case dirFulErr:
	case dskFulErr:
	    return ENOSPC;
	case fBsyErr:
	    return EBUSY;
	case tmfoErr:
	    return ENFILE;
	case fLckdErr:
	case permErr:
	case afpAccessDenied:
	    return EACCES;
	case wPrErr:
	case vLckdErr:
	    return EROFS;
	case badMovErr:
	    return EINVAL;
	case diffVolErr:
	    return EXDEV;
	default:
	    return EINVAL;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * GetOpenMode --
 *
 * Description:
 *	Computes a POSIX mode mask from a given string and also sets
 *	a flag to indicate whether the caller should seek to EOF during
 *	opening of the file.
 *
 * Results:
 *	On success, returns mode to pass to "open". If an error occurs, the
 *	returns -1 and if interp is not NULL, sets interp->result to an
 *	error message.
 *
 * Side effects:
 *	Sets the integer referenced by seekFlagPtr to 1 if the caller
 *	should seek to EOF during opening the file.
 *
 * Special note:
 *	This code is based on a prototype implementation contributed
 *	by Mark Diekhans.
 *
 *----------------------------------------------------------------------
 */

static int
GetOpenMode(interp, string)
    Tcl_Interp *interp;			/* Interpreter to use for error
					 * reporting - may be NULL. */
    char *string;			/* Mode string, e.g. "r+" or
					 * "RDONLY CREAT". */
{
    int mode, modeArgc, c, i, gotRW;
    char **modeArgv, *flag;

    /*
     * Check for the simpler fopen-like access modes (e.g. "r").  They
     * are distinguished from the POSIX access modes by the presence
     * of a lower-case first letter.
     */

    mode = 0;
    if (islower(UCHAR(string[0]))) {
	switch (string[0]) {
	    case 'r':
		mode = TCL_RDONLY;
		break;
	    case 'w':
		mode = TCL_WRONLY|TCL_CREAT|TCL_TRUNC;
		break;
	    case 'a':
		mode = TCL_WRONLY|TCL_CREAT|TCL_APPEND;
		break;
	    default:
		error:
                if (interp != (Tcl_Interp *) NULL) {
                    Tcl_AppendResult(interp,
                            "illegal access mode \"", string, "\"",
                            (char *) NULL);
                }
		return -1;
	}
	if (string[1] == '+') {
	    mode &= ~(TCL_RDONLY|TCL_WRONLY);
	    mode |= TCL_RDWR;
	    if (string[2] != 0) {
		goto error;
	    }
	} else if (string[1] != 0) {
	    goto error;
	}
        return mode;
    }

    /*
     * The access modes are specified using a list of POSIX modes
     * such as TCL_CREAT.
     */

    if (Tcl_SplitList(interp, string, &modeArgc, &modeArgv) != TCL_OK) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AddErrorInfo(interp,
                    "\n    while processing open access modes \"");
            Tcl_AddErrorInfo(interp, string);
            Tcl_AddErrorInfo(interp, "\"");
        }
        return -1;
    }
    
    gotRW = 0;
    for (i = 0; i < modeArgc; i++) {
	flag = modeArgv[i];
	c = flag[0];
	if ((c == 'R') && (strcmp(flag, "RDONLY") == 0)) {
	    mode = (mode & ~TCL_RW_MODES) | TCL_RDONLY;
	    gotRW = 1;
	} else if ((c == 'W') && (strcmp(flag, "WRONLY") == 0)) {
	    mode = (mode & ~TCL_RW_MODES) | TCL_WRONLY;
	    gotRW = 1;
	} else if ((c == 'R') && (strcmp(flag, "RDWR") == 0)) {
	    mode = (mode & ~TCL_RW_MODES) | TCL_RDWR;
	    gotRW = 1;
	} else if ((c == 'A') && (strcmp(flag, "APPEND") == 0)) {
	    mode |= TCL_ALWAYS_APPEND;
	} else if ((c == 'C') && (strcmp(flag, "CREAT") == 0)) {
	    mode |= TCL_CREAT;
	} else if ((c == 'E') && (strcmp(flag, "EXCL") == 0)) {
	    mode |= TCL_EXCL;
	} else if ((c == 'N') && (strcmp(flag, "NOCTTY") == 0)) {
	    mode |= TCL_NOCTTY;
	} else if ((c == 'N') && (strcmp(flag, "NONBLOCK") == 0)) {
	    mode |= TCL_NONBLOCK;
	} else if ((c == 'T') && (strcmp(flag, "TRUNC") == 0)) {
	    mode |= TCL_TRUNC;
	} else {
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "invalid access mode \"", flag,
                        "\": must be RDONLY, WRONLY, RDWR, APPEND, CREAT",
                        " EXCL, NOCTTY, NONBLOCK, or TRUNC", (char *) NULL);
            }
	    ckfree((char *) modeArgv);
	    return -1;
	}
    }
    ckfree((char *) modeArgv);
    if (!gotRW) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "access mode must include either",
                    " RDONLY, WRONLY, or RDWR", (char *) NULL);
        }
	return -1;
    }
    return mode;
}

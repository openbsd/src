/* 
 * tclWinChan.c
 *
 *	Channel drivers for Windows channels based on files, command
 *	pipes and TCP sockets.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinChan.c 1.51 96/04/12 17:06:06
 */

#include	"tclInt.h"	/* Internal definitions for Tcl. */
#include	"tclPort.h"	/* Portability features for Tcl. */

/*
 * Static routines for this file:
 */

static int		FilePipeBlockMode _ANSI_ARGS_((
    			    ClientData instanceData, Tcl_File inFile,
			    Tcl_File outFile, int mode));
static int		FileClose _ANSI_ARGS_((ClientData instanceData,
	            	    Tcl_Interp *interp, Tcl_File inFile,
                            Tcl_File outFile));
static int		FileSeek _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File inFile, Tcl_File outFile, long offset,
			    int mode, int *errorCode));
static int		FilePipeInput _ANSI_ARGS_((ClientData instanceData,
	            	    Tcl_File inFile, char *buf, int toRead,
	            	    int *errorCode));
static int		FilePipeOutput _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File outFile, char *buf, int toWrite,
			    int *errorCode));
static int		FileType _ANSI_ARGS_((HANDLE h));
static int		PipeClose _ANSI_ARGS_((ClientData instanceData,
	            	    Tcl_Interp *interp, Tcl_File inFile,
                            Tcl_File outFile));

/*
 * This structure describes the channel type structure for file based IO.
 */

static Tcl_ChannelType fileChannelType = {
    "file",			/* Type name. */
    FilePipeBlockMode,		/* Set blocking or non-blocking mode.*/
    FileClose,			/* Close proc. */
    FilePipeInput,		/* Input proc. */
    FilePipeOutput,		/* Output proc. */
    FileSeek,			/* Seek proc. */
    NULL,			/* Set option proc. */
    NULL,			/* Get option proc. */
};

/*
 * This structure describes the channel type structure for command pipe
 * based IO.
 */

static Tcl_ChannelType pipeChannelType = {
    "pipe",			/* Type name. */
    FilePipeBlockMode,		/* Set blocking or non-blocking mode.*/
    PipeClose,			/* Close proc. */
    FilePipeInput,		/* Input proc. */
    FilePipeOutput,		/* Output proc. */
    NULL,			/* Seek proc. */
    NULL,			/* Set option proc. */
    NULL,			/* Get option proc. */
};

/*
 * This is the size of the channel name for File based channels
 */

#define CHANNEL_NAME_SIZE	64
static char channelName[CHANNEL_NAME_SIZE+1];

/*
 * Structure describing per-instance state for file based channels.
 *
 * IMPORTANT NOTE: If you modify this structure, make sure that the
 * "asynch" field remains the first field - FilePipeBlockMode depends
 * on this.
 */

typedef struct FileState {
    int asynch;			/* 1 if channel is in asynch mode. */
    int append;			/* 1 if channel is in append mode. */
} FileState;

/*
 * This structure describes per-instance state of a pipe based channel.
 *
 * IMPORTANT NOTE: If you modify this structure, make sure that the
 * "asynch" field remains the first field - FilePipeBlockMode depends
 * on this.
 */

typedef struct PipeState {
    int asynch;			/* 1 if channel is in asynch mode. */
    Tcl_File readFile;		/* Output from pipe. */
    Tcl_File writeFile;		/* Input from pipe. */
    Tcl_File errorFile;		/* Error output from pipe. */
    int numPids;		/* Number of processes attached to pipe. */
    int *pidPtr;		/* Pids of attached processes. */
} PipeState;

/*
 *----------------------------------------------------------------------
 *
 * FilePipeOutput--
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

	/* ARGSUSED */
static int
FilePipeOutput(instanceData, outFile, buf, toWrite, errorCode)
    ClientData instanceData;		/* Unused. */
    Tcl_File outFile;			/* Output device for channel. */
    char *buf;				/* The data buffer. */
    int toWrite;			/* How many bytes to write? */
    int *errorCode;			/* Where to store error code. */
{
    int type;
    DWORD bytesWritten;
    HANDLE handle;
    
    *errorCode = 0;
    handle = (HANDLE) Tcl_GetFileInfo(outFile, &type);

    /*
     * If we are writing to a file that was opened with O_APPEND, we need to
     * seek to the end of the file before writing the current buffer.
     */

    if (type == TCL_WIN_FILE) {
	FileState *statePtr = (FileState *)instanceData;
	if (statePtr->append) {
	    SetFilePointer(handle, 0, NULL, FILE_END);
	}
    }

    if (WriteFile(handle, (LPVOID) buf, (DWORD) toWrite, &bytesWritten,
            (LPOVERLAPPED) NULL) == FALSE) {
        TclWinConvertError(GetLastError());
        if (errno == EPIPE) {
            return 0;
        }
        *errorCode = errno;
        return -1;
    }
    if (type == TCL_WIN_FILE) {
	FlushFileBuffers(handle);
    }
    return bytesWritten;
}

/*
 *----------------------------------------------------------------------
 *
 * FilePipeInput --
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

static int
FilePipeInput(instanceData, inFile, buf, bufSize, errorCode)
    ClientData instanceData;		/* File state. */
    Tcl_File inFile;			/* Input device for channel. */
    char *buf;				/* Where to store data read. */
    int bufSize;			/* How much space is available
                                         * in the buffer? */
    int *errorCode;			/* Where to store error code. */
{
    FileState *statePtr;
    HANDLE handle;
    DWORD count;
    DWORD bytesRead;
    int type;

    *errorCode = 0;
    statePtr = (FileState *) instanceData;
    handle = (HANDLE) Tcl_GetFileInfo(inFile, &type);

    if (type == TCL_WIN_PIPE) {

	/*
	 * Pipes will block until the requested number of bytes has been
	 * read.  To avoid blocking unnecessarily, we look ahead and only
	 * read as much as is available.
	 */

	if (PeekNamedPipe(handle, (LPVOID) NULL, (DWORD) 0, (LPDWORD) NULL,
		&count, (LPDWORD) NULL) == TRUE) {
	    if ((count != 0) && ((DWORD) bufSize > count)) {
		bufSize = (int) count;
	    } else if ((count == 0) && statePtr->asynch) {
		errno = *errorCode = EAGAIN;
		return 0;
	    } else if ((count == 0) && !statePtr->asynch) {
		bufSize = 1;
	    }
	} else {
	    goto error;
	}
    }

    /*
     * Note that we will block on reads from a console buffer until a
     * full line has been entered.  The only way I know of to get
     * around this is to write a console driver.  We should probably
     * do this at some point, but for now, we just block.
     */

    if (ReadFile(handle, (LPVOID) buf, (DWORD) bufSize, &bytesRead,
            (LPOVERLAPPED) NULL) == FALSE) {
	goto error;
    }
    
    return bytesRead;

    error:
    TclWinConvertError(GetLastError());
    if (errno == EPIPE) {
	return 0;
    }
    *errorCode = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * FilePipeBlockMode --
 *
 *	Set blocking or non-blocking mode on channel.
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
FilePipeBlockMode(instanceData, inFile, outFile, mode)
    ClientData instanceData;		/* Instance state for channel. */
    Tcl_File inFile, outFile;		/* Input, output for channel. */
    int mode;				/* The mode to set. */
{
    /*
     * Files on Windows can not be switched between blocking and nonblocking,
     * hence we have to emulate the behavior. This is done in the input
     * function by checking against a bit in the state. We set or unset the
     * bit here to cause the input function to emulate the correct behavior.
     *
     * IMPORTANT NOTE:
     *
     * The use of the "asynch" field below relies on the assumption that it
     * will be located at the same offset (0) in the instanceData associated
     * with all types of channels using this routine.
     */

    if (instanceData != (ClientData) NULL) {
        FileState *sPtr = (FileState *) instanceData;
	sPtr->asynch = (mode == TCL_MODE_BLOCKING) ? 0 : 1;
    }

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

	/* ARGSUSED */
static int
FileClose(instanceData, interp, inFile, outFile)
    ClientData instanceData;	/* Pointer to FileState structure. */
    Tcl_Interp *interp;		/* Not used. */
    Tcl_File inFile;		/* Input side. */
    Tcl_File outFile;		/* Output side. */
{
    HANDLE handle;
    int type, errorCode = 0;

    if (instanceData != (ClientData) NULL) {
        ckfree((char *) instanceData);
    }

    if (inFile != NULL) {
        handle = (HANDLE) Tcl_GetFileInfo(inFile, &type);

	/*
	 * Check for read/write file so we only close it once.
	 */

	if (inFile == outFile) {
	    outFile = NULL;
	}
        Tcl_FreeFile(inFile);

	if (CloseHandle(handle) == FALSE) {
	    TclWinConvertError(GetLastError());
	    errorCode = errno;
	}

    }
    if (outFile != NULL) {
        handle = (HANDLE) Tcl_GetFileInfo(outFile, &type);
	Tcl_FreeFile(outFile);

	if (CloseHandle(handle) == FALSE) {
	    TclWinConvertError(GetLastError());
	    if (errorCode == 0) {
		errorCode = errno;
	    }
	}
    }
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * FileSeek --
 *
 *	Seeks on a file-based channel. Returns the new position.
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

	/* ARGSUSED */
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
    DWORD moveMethod;
    DWORD newPos;
    HANDLE handle;
    int type;

    *errorCodePtr = 0;
    if (inFile != (Tcl_File) NULL) {
        handle = (HANDLE) Tcl_GetFileInfo(inFile, &type);
    } else if (outFile != (Tcl_File) NULL) {
        handle = (HANDLE) Tcl_GetFileInfo(outFile, &type);
    } else {
        *errorCodePtr = EFAULT;
        return -1;
    }
    
    if (mode == SEEK_SET) {
        moveMethod = FILE_BEGIN;
    } else if (mode == SEEK_CUR) {
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
 * PipeClose --
 *
 *	Closes a pipe based IO channel.
 *
 * Results:
 *	0 on success, errno otherwise.
 *
 * Side effects:
 *	Closes the physical channel.
 *
 *----------------------------------------------------------------------
 */

	/* ARGSUSED */
static int
PipeClose(instanceData, interp, inFile, outFile)
    ClientData instanceData;	/* Pointer to PipeState structure. */
    Tcl_Interp *interp;		/* For error reporting. */
    Tcl_File inFile, outFile;	/* Unused. */
{
    PipeState *pipePtr = (PipeState *) instanceData;
    HANDLE handle;
    Tcl_Channel errChan;
    int errorCode, result;

    errorCode = 0;
    if (pipePtr->readFile != NULL) {
        handle = (HANDLE) Tcl_GetFileInfo(pipePtr->readFile, NULL);
        Tcl_FreeFile(pipePtr->readFile);

	if (CloseHandle(handle) == FALSE) {
	    TclWinConvertError(GetLastError());
	    errorCode = errno;
	}
    }
    if (pipePtr->writeFile != NULL) {
        handle = (HANDLE) Tcl_GetFileInfo(pipePtr->writeFile, NULL);
	Tcl_FreeFile(pipePtr->writeFile);

	if (CloseHandle(handle) == FALSE) {
	    TclWinConvertError(GetLastError());
	    if (errorCode == 0) {
		errorCode = errno;
	    }
	}
    }
    
    /*
     * Wrap the error file into a channel and give it to the cleanup
     * routine.
     */

    if (pipePtr->errorFile != NULL) {
	errChan = Tcl_CreateChannel(&fileChannelType, "pipeError",
                pipePtr->errorFile, NULL, NULL);
        if (Tcl_SetChannelOption(interp, errChan, "-translation", "auto") ==
                TCL_ERROR) {
            Tcl_Close((Tcl_Interp *) NULL, errChan);
            errChan = (Tcl_Channel) NULL;
        }
        if ((errChan != (Tcl_Channel) NULL) &&
                (Tcl_SetChannelOption(NULL, errChan, "-eofchar", "\032") ==
                        TCL_ERROR)) {
            Tcl_Close((Tcl_Interp *) NULL, errChan);
            errChan = (Tcl_Channel) NULL;
        }
    } else {
        errChan = NULL;
    }
    result = TclCleanupChildren(interp, pipePtr->numPids, pipePtr->pidPtr,
            errChan);
    if (pipePtr->numPids > 0) {
        ckfree((char *) pipePtr->pidPtr);
    }
    ckfree((char *) pipePtr);
    if (errorCode == 0) {
        return result;
    }
    return errorCode;
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
    Tcl_File file;
    Tcl_Channel chan;
    FileState *sPtr;
    int seekFlag, mode;
    HANDLE handle;
    DWORD accessMode, createMode, shareMode, flags;
    SECURITY_ATTRIBUTES sec;
    char *nativeName;
    Tcl_DString buffer;

    mode = TclGetOpenMode(interp, modeString, &seekFlag);
    if (mode == -1) {
        return NULL;
    }
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
	    panic("Tcl_OpenFileChannel: invalid mode value");
	    break;
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
     * If the file is being created, get the file attributes from the
     * permissions argument, else use the existing file attributes.
     */

    if (mode & O_CREAT) {
        if (permissions & S_IWRITE) {
            flags = FILE_ATTRIBUTE_NORMAL;
        } else {
            flags = FILE_ATTRIBUTE_READONLY;
        }
    } else {
	flags = GetFileAttributes(fileName);
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

    nativeName = Tcl_TranslateFileName(interp, fileName, &buffer);
    if (nativeName == NULL) {
	return NULL;
    }
    handle = CreateFile(nativeName, accessMode, shareMode, &sec, createMode,
            flags, (HANDLE) NULL);
    Tcl_DStringFree(&buffer);

    if (handle == INVALID_HANDLE_VALUE) {
	DWORD err = GetLastError();
	if ((err & 0xffffL) == ERROR_OPEN_FAILED) {
	    err = (mode & O_CREAT) ? ERROR_FILE_EXISTS : ERROR_FILE_NOT_FOUND;
	}
        TclWinConvertError(err);
	if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "couldn't open \"", fileName, "\": ",
                    Tcl_PosixError(interp), (char *) NULL);
        }
        return NULL;
    }

    file = Tcl_GetFile((ClientData) handle, TCL_WIN_FILE);

    sPtr = (FileState *) ckalloc((unsigned) sizeof(FileState));
    sPtr->asynch = 0;
    sPtr->append = (mode & O_APPEND) ? 1 : 0;
    sprintf(channelName, "file%d", (int) Tcl_GetFileInfo(file, NULL));
    chan = Tcl_CreateChannel(&fileChannelType, channelName,
            (accessMode & GENERIC_READ) ? file : NULL,
            (accessMode & GENERIC_WRITE) ? file : NULL,
            (ClientData) sPtr);
    if (chan == (Tcl_Channel) NULL) {
        if (interp != (Tcl_Interp *) NULL) {
            Tcl_AppendResult(interp, "could not open channel \"",
                    channelName, "\": ", Tcl_PosixError(interp),
                    (char *) NULL);
        }
        Tcl_FreeFile(file);
        CloseHandle(handle);
        ckfree((char *) sPtr);
        return NULL;
    }

    if (seekFlag) {
        if (Tcl_Seek(chan, 0, SEEK_END) < 0) {
            if (interp != (Tcl_Interp *) NULL) {
                Tcl_AppendResult(interp, "could not seek to end of file on \"",
                        channelName, "\": ", Tcl_PosixError(interp),
                        (char *) NULL);
            }
            Tcl_Close((Tcl_Interp *) NULL, chan);
            return NULL;
        }
    }

    /*
     * Files have default translation of AUTO and ^Z eof char, which
     * means that a ^Z will be appended to them at close.
     */
    
    if (Tcl_SetChannelOption(interp, chan, "-translation", "auto") ==
            TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption(NULL, chan, "-eofchar", "\032 {}") ==
            TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return (Tcl_Channel) NULL;
    }
    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * FileType --
 *
 *	Converts a Windows handle type to a Tcl file type
 *
 * Results:
 *	The Tcl file type corresponding to the given Windows handle type
 *	or -1 on error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static int
FileType(h)
    HANDLE h;		/* Convert the type of this handle to
                         * a Tcl file type. */
{
    switch (GetFileType(h)) {
    case FILE_TYPE_CHAR:
        return TCL_WIN_CONSOLE;
    case FILE_TYPE_DISK:
        return TCL_WIN_FILE;
    case FILE_TYPE_PIPE:
        return TCL_WIN_PIPE;
    default:
        return -1;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_MakeFileChannel --
 *
 *	Creates a Tcl_Channel from an existing platform specific file
 *	handle.
 *
 * Results:
 *	The Tcl_Channel created around the preexisting file.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
Tcl_MakeFileChannel(inFile, outFile, mode)
    ClientData inFile;		/* OS level handle used for input. */
    ClientData outFile;		/* OS level handle used for output. */
    int mode;			/* ORed combination of TCL_READABLE and
                                 * TCL_WRITABLE to indicate whether inFile
                                 * and/or outFile are valid. */
{
    Tcl_File inFd, outFd;
    char channelName[20];
    FileState *sPtr;
    Tcl_Channel chan;

    if (mode & TCL_READABLE) {
        sprintf(channelName, "file%d", (int) inFile);
        inFd = Tcl_GetFile(inFile, FileType((HANDLE) inFile));
    } else {
        inFd = (Tcl_File) NULL;
    }
    
    if (mode & TCL_WRITABLE) {
        sprintf(channelName, "file%d", (int) outFile);
        outFd = Tcl_GetFile(outFile, FileType((HANDLE) outFile));
    } else {
        outFd = (Tcl_File) NULL;
    }

    sPtr = (FileState *) ckalloc((unsigned) sizeof(FileState));
    sPtr->asynch = 0;
    sPtr->append = 0;

    chan = Tcl_CreateChannel(&fileChannelType, channelName, inFd, outFd,
            (ClientData) sPtr);
    if (chan == (Tcl_Channel) NULL) {
        ckfree((char *) sPtr);
        return NULL;
    }

    /*
     * Windows files have AUTO translation mode and ^Z eof char, which
     * means that ^Z will be appended on close and accepted as EOF.
     */
    
    if (Tcl_SetChannelOption((Tcl_Interp *) NULL, chan, "-translation",
            "auto") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption((Tcl_Interp *) NULL, chan, "-eofchar",
            "\032 {}") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, chan);
        return (Tcl_Channel) NULL;
    }
    return chan;
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreateCommandChannel --
 *
 *	This function is called by Tcl_OpenCommandChannel to perform
 *	the platform specific channel initialization for a command
 *	channel.
 *
 * Results:
 *	Returns a new channel or NULL on failure.
 *
 * Side effects:
 *	Allocates a new channel.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclCreateCommandChannel(readFile, writeFile, errorFile, numPids, pidPtr)
    Tcl_File readFile;		/* If non-null, gives the file for reading. */
    Tcl_File writeFile;		/* If non-null, gives the file for writing. */
    Tcl_File errorFile;		/* If non-null, gives the file where errors
				 * can be read. */
    int numPids;		/* The number of pids in the pid array. */
    int *pidPtr;		/* An array of process identifiers. */
{
    Tcl_Channel channel;
    char channelName[20];
    int channelId;
    PipeState *statePtr = (PipeState *) ckalloc((unsigned) sizeof(PipeState));

    statePtr->asynch = 0;
    statePtr->readFile = readFile;
    statePtr->writeFile = writeFile;
    statePtr->errorFile = errorFile;
    statePtr->numPids = numPids;
    statePtr->pidPtr = pidPtr;

    /*
     * Use one of the fds associated with the channel as the
     * channel id.
     */

    if (readFile) {
	channelId = (int) Tcl_GetFileInfo(readFile, NULL);
    } else if (writeFile) {
	channelId = (int) Tcl_GetFileInfo(writeFile, NULL);
    } else if (errorFile) {
	channelId = (int) Tcl_GetFileInfo(errorFile, NULL);
    } else {
	channelId = 0;
    }

    /*
     * For backward compatibility with previous versions of Tcl, we
     * use "file%d" as the base name for pipes even though it would
     * be more natural to use "pipe%d".
     */

    sprintf(channelName, "file%d", channelId);
    channel = Tcl_CreateChannel(&pipeChannelType, channelName, readFile,
	    writeFile, (ClientData) statePtr);

    if (channel == NULL) {
	ckfree((char *)statePtr);
        return NULL;
    }

    /*
     * Pipes have AUTO translation mode on Windows and ^Z eof char, which
     * means that a ^Z will be appended to them at close. This is needed
     * for Windows programs that expect a ^Z at EOF.
     */

    if (Tcl_SetChannelOption((Tcl_Interp *) NULL, channel, "-translation",
            "auto") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, channel);
        return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption((Tcl_Interp *) NULL, channel, "-eofchar",
            "\032 {}") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, channel);
        return (Tcl_Channel) NULL;
    }
    return channel;
}

/*
 *----------------------------------------------------------------------
 *
 * Tcl_PidCmd --
 *
 *	This procedure is invoked to process the "pid" Tcl command.
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

	/* ARGSUSED */
int
Tcl_PidCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    Tcl_Channel chan;			/* The channel to get pids for. */
    Tcl_ChannelType *typePtr;
    PipeState *pipePtr;			/* The pipe state. */
    int i;				/* Loops over PIDs attached to the
                                         * pipe. */
    char string[50];			/* Temp buffer for string rep. of
                                         * PIDs attached to the pipe. */

    if (argc > 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " ?channelId?\"", (char *) NULL);
	return TCL_ERROR;
    }
    if (argc == 1) {
	sprintf(interp->result, "%ld", (long) getpid());
    } else {
        chan = Tcl_GetChannel(interp, argv[1], NULL);
        if (chan == (Tcl_Channel) NULL) {
	    return TCL_ERROR;
	}
	typePtr = Tcl_GetChannelType(chan);
	if (typePtr != &pipeChannelType) {
            return TCL_OK;
        }
        pipePtr = (PipeState *) Tcl_GetChannelInstanceData(chan);
        for (i = 0; i < pipePtr->numPids; i++) {
	    sprintf(string, "%d", pipePtr->pidPtr[i]);
	    Tcl_AppendElement(interp, string);
	}
    }
    return TCL_OK;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetDefaultStdChannel --
 *
 *	Constructs a channel for the specified standard OS handle.
 *
 * Results:
 *	Returns the specified default standard channel, or NULL.
 *
 * Side effects:
 *	May cause the creation of a standard channel and the underlying
 *	file.
 *
 *----------------------------------------------------------------------
 */

Tcl_Channel
TclGetDefaultStdChannel(type)
    int type;			/* One of TCL_STDIN, TCL_STDOUT, TCL_STDERR. */
{
    Tcl_Channel channel;
    HANDLE handle;
    int mode;
    char *bufMode;
    DWORD handleId;		/* Standard handle to retrieve. */

    switch (type) {
	case TCL_STDIN:
	    handleId = STD_INPUT_HANDLE;
	    mode = TCL_READABLE;
	    bufMode = "line";
	    break;
	case TCL_STDOUT:
	    handleId = STD_OUTPUT_HANDLE;
	    mode = TCL_WRITABLE;
	    bufMode = "line";
	    break;
	case TCL_STDERR:
	    handleId = STD_ERROR_HANDLE;
	    mode = TCL_WRITABLE;
	    bufMode = "none";
	    break;
	default:
	    panic("TclGetDefaultStdChannel: Unexpected channel type");
	    break;
    }
    handle = GetStdHandle(handleId);

    /*
     * Note that we need to check for 0 because Windows will return 0 if this
     * is not a console mode application, even though this is not a valid
     * handle. 
     */

    if ((handle == INVALID_HANDLE_VALUE) || (handle == 0)) {
	return NULL;
    }

    channel = Tcl_MakeFileChannel(handle, handle, mode);

    /*
     * Set up the normal channel options for stdio handles.
     */

    if (Tcl_SetChannelOption((Tcl_Interp *) NULL, channel, "-translation",
            "auto") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, channel);
        return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption((Tcl_Interp *) NULL, channel, "-eofchar",
            "\032 {}") == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, channel);
        return (Tcl_Channel) NULL;
    }
    if (Tcl_SetChannelOption((Tcl_Interp *) NULL, channel, "-buffering",
            bufMode) == TCL_ERROR) {
        Tcl_Close((Tcl_Interp *) NULL, channel);
        return (Tcl_Channel) NULL;
    }
    return channel;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetAndDetachPids --
 *
 *	Stores a list of the command PIDs for a command channel in
 *	interp->result.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Modifies interp->result.
 *
 *----------------------------------------------------------------------
 */

void
TclGetAndDetachPids(interp, chan)
    Tcl_Interp *interp;
    Tcl_Channel chan;
{
    PipeState *pipePtr;
    Tcl_ChannelType *chanTypePtr;
    int i;
    char buf[20];

    /*
     * Punt if the channel is not a command channel.
     */

    chanTypePtr = Tcl_GetChannelType(chan);
    if (chanTypePtr != &pipeChannelType) {
        return;
    }

    pipePtr = (PipeState *) Tcl_GetChannelInstanceData(chan);
    for (i = 0; i < pipePtr->numPids; i++) {
        sprintf(buf, "%d", pipePtr->pidPtr[i]);
        Tcl_AppendElement(interp, buf);
        Tcl_DetachPids(1, &(pipePtr->pidPtr[i]));
    }
    if (pipePtr->numPids > 0) {
        ckfree((char *) pipePtr->pidPtr);
        pipePtr->numPids = 0;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclClosePipeFile --
 *
 *	This function is a simple wrapper for close on a file or
 *	pipe handle.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Closes the HANDLE and frees the Tcl_File.
 *
 *----------------------------------------------------------------------
 */

void
TclClosePipeFile(file)
    Tcl_File file;
{
    int type;
    HANDLE handle = (HANDLE) Tcl_GetFileInfo(file, &type);
    switch (type) {
	case TCL_WIN_FILE:
	case TCL_WIN_PIPE:
	    CloseHandle(handle);
	    break;
	default:
	    break;
    }
    Tcl_FreeFile(file);
}

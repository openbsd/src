/* 
 * tclMacChan.c
 *
 *	Channel drivers for Macintosh channels for the
 *	console fds.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacChan.c 1.33 96/04/11 17:03:05
 */

#include "tclInt.h"
#include "tclPort.h"
#include <Aliases.h>
#include <Errors.h>
#include <Files.h>
#include <Gestalt.h>
#include <Processes.h>
#include <Strings.h>

/*
 * Static routines for this file:
 */

static int		StdIOBlockMode _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File inFile, Tcl_File outFile, int mode));
static int		StdIOInput _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File inFile, char *buf, int toRead,
			    int *errorCode));
static int		StdIOOutput _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File outFile, char *buf, int toWrite,
			    int *errorCode));
static int		StdIOSeek _ANSI_ARGS_((ClientData instanceData,
			    Tcl_File inFile, Tcl_File outFile, long offset,
			    int mode, int *errorCode));
static int		StdIOClose _ANSI_ARGS_((ClientData instanceData,
			    Tcl_Interp *interp, Tcl_File inFile,
			    Tcl_File outFile));

/*
 * This structure describes the channel type structure for file based IO:
 */

static Tcl_ChannelType consoleChannelType = {
    "file",			/* Type name. */
    StdIOBlockMode,		/* Set blocking/nonblocking mode.*/
    StdIOClose,			/* Close proc. */
    StdIOInput,			/* Input proc. */
    StdIOOutput,		/* Output proc. */
    StdIOSeek,			/* Seek proc. */
    NULL,			/* Set option proc. */
    NULL,			/* Get option proc. */
};

/*
 * Hack to allow Mac Tk to override the TclGetStdChannels function.
 */
 
typedef void (*TclGetStdChannelsProc) _ANSI_ARGS_((Tcl_Channel *stdinPtr,
	Tcl_Channel *stdoutPtr, Tcl_Channel *stderrPtr));
	
TclGetStdChannelsProc getStdChannelsProc = NULL;

/*
 * Static variables to hold channels for stdin, stdout and stderr.
 */

static Tcl_Channel stdinChannel = NULL;
static Tcl_Channel stdoutChannel = NULL;
static Tcl_Channel stderrChannel = NULL;

/*
 * Static variables to hold files for stdin, stdout and stderr.
 */

static Tcl_File stdinFile = NULL;
static Tcl_File stdoutFile = NULL;
static Tcl_File stderrFile = NULL;

/*
 *----------------------------------------------------------------------
 *
 * StdIOOutput--
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
StdIOOutput(instanceData, outFile, buf, toWrite, errorCode)
    ClientData instanceData;		/* Unused. */
    Tcl_File outFile;			/* Output device for channel. */
    char *buf;				/* The data buffer. */
    int toWrite;			/* How many bytes to write? */
    int *errorCode;			/* Where to store error code. */
{
    int written;
    int fd;

    *errorCode = 0;
    errno = 0;
    fd = (int) Tcl_GetFileInfo(outFile, NULL);
    written = write(fd, buf, (size_t) toWrite);
    if (written > -1) {
        return written;
    }
    *errorCode = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * StdIOInput --
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
StdIOInput(instanceData, inFile, buf, bufSize, errorCode)
    ClientData instanceData;		/* Unused. */
    Tcl_File inFile;			/* Input device for channel. */
    char *buf;				/* Where to store data read. */
    int bufSize;			/* How much space is available
                                         * in the buffer? */
    int *errorCode;			/* Where to store error code. */
{
    int fd;
    int bytesRead;			/* How many bytes were read? */

    *errorCode = 0;
    errno = 0;
    fd = (int) Tcl_GetFileInfo(inFile, NULL);
    bytesRead = read(fd, buf, (size_t) bufSize);
    if (bytesRead > -1) {
        return bytesRead;
    }
    *errorCode = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * StdIOSeek --
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
StdIOSeek(instanceData, inFile, outFile, offset, mode, errorCodePtr)
    ClientData instanceData;			/* Unused. */
    Tcl_File inFile, outFile;			/* Input and output
                                                 * devices for channel. */
    long offset;				/* Offset to seek to. */
    int mode;					/* Relative to where
                                                 * should we seek? */
    int *errorCodePtr;				/* To store error code. */
{
    int newLoc;
    int fd;

    *errorCodePtr = 0;
    if (inFile != (Tcl_File) NULL) {
        fd = (int) Tcl_GetFileInfo(inFile, NULL);
    } else if (outFile != (Tcl_File) NULL) {
        fd = (int) Tcl_GetFileInfo(outFile, NULL);
    } else {
        *errorCodePtr = EFAULT;
        return -1;
    }
    newLoc = lseek(fd, offset, mode);
    if (newLoc > -1) {
        return newLoc;
    }
    *errorCodePtr = errno;
    return -1;
}

/*
 *----------------------------------------------------------------------
 *
 * StdIOBlockMode --
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
StdIOBlockMode(instanceData, inFile, outFile, mode)
    ClientData instanceData;		/* Unused. */
    Tcl_File inFile, outFile;		/* Input, output for channel. */
    int mode;				/* The mode to set. */
{
    int curStatus;
    int fd;

    /*
     * Do not allow putting stdin, stdout or stderr into nonblocking mode.
     */
    
    if (mode == TCL_MODE_NONBLOCKING) {
        if ((inFile == stdinFile) || (outFile == stdoutFile) ||
                (outFile == stderrFile)) {
            return EFAULT;
        }
    }
    
    if (inFile != NULL) {
        fd = (int) Tcl_GetFileInfo(inFile, NULL);
        curStatus = fcntl(fd, F_GETFL);
        if (mode == TCL_MODE_BLOCKING) {
            curStatus &= (~(O_NONBLOCK));
        } else {
            curStatus |= O_NONBLOCK;
        }
        if (fcntl(fd, F_SETFL, curStatus) < 0) {
            return errno;
        }
    }
    if (outFile != NULL) {
        fd = (int) Tcl_GetFileInfo(outFile, NULL);
        curStatus = fcntl(fd, F_GETFL);
        if (mode == TCL_MODE_BLOCKING) {
            curStatus &= (~(O_NONBLOCK));
        } else {
            curStatus |= O_NONBLOCK;
        }
        if (fcntl(fd, F_SETFL, curStatus) < 0) {
            return errno;
        }
    }

    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * StdIOClose --
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
StdIOClose(instanceData, interp, inFile, outFile)
    ClientData instanceData;	/* Unused. */
    Tcl_Interp *interp;		/* Unused. */
    Tcl_File inFile;		/* Input file to close. */
    Tcl_File outFile;		/* Output file to close. */
{
    int fd, errorCode = 0;

    /*
     * Invalidate the stdio cache if necessary.  Note that we assume that
     * the stdio file and channel pointers will become invalid at the same
     * time.
     */

    if (inFile == stdinFile) {
	stdinFile = NULL;
	stdinChannel = NULL;
    }
    if (outFile == stdoutFile) {
	stdoutFile = NULL;
	stdoutChannel = NULL;
    } else if (outFile == stderrFile) {
	stderrFile = NULL;
	stderrChannel = NULL;
    }

    if (inFile != NULL) {
        fd = (int) Tcl_GetFileInfo(inFile, NULL);

	/*
	 * Check for read/write file so we only close it once.
	 */

	if (inFile == outFile) {
	    outFile = NULL;
	}
        Tcl_FreeFile(inFile);

        if (fd > 2) {
            if (close(fd) < 0) {
		errorCode = errno;
	    }
        }
    }

    if (outFile != NULL) {
        fd = (int) Tcl_GetFileInfo(outFile, NULL);
        Tcl_FreeFile(outFile);        
        if (fd > 2) {
	    if ((close(fd) < 0) && (errorCode == 0)) {
		errorCode = errno;
	    }
        } 
    }
    return errorCode;
}

/*
 *----------------------------------------------------------------------
 *
 * TclGetAndDetachPids --
 *
 *	Stores a list of the command PIDs for a command channel in
 *	interp->result and detaches the PIDs.
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
    return;
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

int
Tcl_PidCmd(dummy, interp, argc, argv)
    ClientData dummy;			/* Not used. */
    Tcl_Interp *interp;			/* Current interpreter. */
    int argc;				/* Number of arguments. */
    char **argv;			/* Argument strings. */
{
    ProcessSerialNumber psn;
    Tcl_Channel chan;

    if (argc > 2) {
	Tcl_AppendResult(interp, "wrong # args: should be \"",
		argv[0], " ?channelId?\"", (char *) NULL);
	return TCL_ERROR;
    }
    
    if (argc == 2) {
        chan = Tcl_GetChannel(interp, argv[1], NULL);
	if (chan == (Tcl_Channel) NULL) {
	    return TCL_ERROR;
	}
	
	/*
	 * We can't create pipelines on the Mac so
	 * this will always return an empty list.
	 */
	return TCL_OK;
    }
    
    GetCurrentProcess(&psn);
    sprintf(interp->result, "0x%08x%08x", psn.highLongOfPSN, psn.lowLongOfPSN);
    
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
    Tcl_Channel channel = NULL;
    int fd = 0;			/* Initializations needed to prevent */
    int mode = 0;		/* compiler warning (used before set). */
    char *bufMode = NULL;
    Tcl_File inFile = NULL, outFile = NULL;
    char channelName[20];

    /*
     * If the channels were not created yet, create them now and
     * store them in the static variables.
     */

    switch (type) {
	case TCL_STDIN:
	    fd = 0;
	    inFile = Tcl_GetFile((ClientData) fd, TCL_UNIX_FD);
	    bufMode = "line";
	    break;
	case TCL_STDOUT:
	    fd = 1;
	    outFile = Tcl_GetFile((ClientData) fd, TCL_UNIX_FD);
	    bufMode = "line";
	    break;
	case TCL_STDERR:
	    fd = 2;
	    outFile = Tcl_GetFile((ClientData) fd, TCL_UNIX_FD);
	    bufMode = "none";
	    break;
	default:
	    panic("TclGetDefaultStdChannel: Unexpected channel type");
	    break;
    }

    sprintf(channelName, "console%d", (int) fd);
    channel = Tcl_CreateChannel(&consoleChannelType, channelName,
	    inFile, outFile, (ClientData) NULL);
    /*
     * Set up the normal channel options for stdio handles.
     */

    Tcl_SetChannelOption(NULL, channel, "-translation", "cr");
    Tcl_SetChannelOption(NULL, channel, "-buffering", bufMode);
    return channel;
}

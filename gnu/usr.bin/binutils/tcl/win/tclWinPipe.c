/* 
 * tclWinPipe.c --
 *
 *	This file implements the Windows-specific pipeline exec functions.
 *
 * Copyright (c) 1996 by Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinPipe.c 1.26 96/03/08 11:21:21
 */

#include "tclInt.h"
#include "tclPort.h"

#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <sys/stat.h>

#include "tclWinInt.h"

static BOOL		HasConsole _ANSI_ARGS_((void));
static BOOL 		IsWinApp _ANSI_ARGS_((char *filename,
			    Tcl_Interp *interp));
static int 		Win32Pipeline _ANSI_ARGS_((Tcl_Interp *interp,
			    int *pidPtr, int *numPids, int argc, char **argv,
			    Tcl_File inputFile,
			    Tcl_File outputFile,
			    Tcl_File errorFile));
static int 		Win32sPipeline _ANSI_ARGS_((Tcl_Interp *interp,
			    int argc, char **argv, char *IntIn,
			    char *FinalOut, int *pidPtr));

/*
 *----------------------------------------------------------------------
 *
 * TclSpawnPipeline --
 *
 *      Calls either Win32Pipeline or Win32sPipeline, depending on which
 *      version of Windows is running.
 *
 * Results:
 *      The return value is 1 on success, 0 on error
 *
 * Side effects:
 *      Processes and pipes are created.
 *
 *----------------------------------------------------------------------
 */

int
TclSpawnPipeline(interp, pidPtr, numPids, argc, argv, inputFile, outputFile,
	errorFile, intIn, finalOut)
    Tcl_Interp *interp;		/* Interpreter in which to process pipeline. */
    int *pidPtr;		/* Array of pids which are created. */
    int *numPids;		/* Number of pids created. */
    int argc;			/* Number of entries in argv. */
    char **argv;		/* Array of strings describing commands in
				 * pipeline plus I/O redirection with <,
				 * <<, >, etc. argv[argc] must be NULL. */
    Tcl_File inputFile;	/* If >=0, gives file id to use as input for
				 * first process in pipeline (specified via <
				 * or <@). */
    Tcl_File outputFile;	/* Writable file id for output from last
				 * command in pipeline (could be file or
				 * pipe). NULL means use stdout. */
    Tcl_File errorFile;	/* Writable file id for error output from all
				 * commands in the pipeline. NULL means use
				 * stderr */
    char *intIn;		/* File name for initial input (for Win32s). */
    char *finalOut;		/* File name for final output (for Win32s). */
{
    if (TclHasPipes()) {
	return(Win32Pipeline(interp, pidPtr, numPids, argc, argv, inputFile,
		outputFile, errorFile));
    }

    return(Win32sPipeline(interp, argc, argv, intIn, finalOut, pidPtr));
}

/*
 *----------------------------------------------------------------------
 *
 * Win32Pipeline --
 *
 *      Given an argc/argv array, instantiate a pipeline of processes
 *      as described by the argv.
 *
 * Results:
 *      The return value is 1 on success, 0 on error
 *
 * Side effects:
 *      Processes and pipes are created.
 *
 *----------------------------------------------------------------------
 */

static int
Win32Pipeline(interp, pidPtr, numPids, argc, argv, inputFile, outputFile,
	errorFile)
    Tcl_Interp *interp;		/* Interpreter in which to process pipeline. */
    int *pidPtr;		/* Array of pids which are created. */
    int *numPids;		/* Number of pids created. */
    int argc;			/* Number of entries in argv. */
    char **argv;		/* Array of strings describing commands in
				 * pipeline plus I/O redirection with <,
				 * <<, >, etc. argv[argc] must be NULL. */
    Tcl_File inputFile;	/* If >=0, gives file id to use as input for
				 * first process in pipeline (specified via <
				 * or <@). */
    Tcl_File outputFile;	/* Writable file id for output from last
				 * command in pipeline (could be file or
				 * pipe). NULL means use stdout. */
    Tcl_File errorFile;	/* Writable file id for error output from all
				 * commands in the pipeline. NULL means use
				 * stderr */
{
    Tcl_Channel channel;
    STARTUPINFO startInfo;
    PROCESS_INFORMATION procInfo;
    SECURITY_ATTRIBUTES secAtts;
    int firstArg, lastArg;
    int pid, i, type;
    Tcl_DString buffer;
    char *execName;
    int joinThisError;
    Tcl_File pipeIn = NULL;
    Tcl_File curOutFile = NULL;
    Tcl_File curInFile = inputFile;
    Tcl_File stdInFile = NULL, stdOutFile = NULL, stdErrFile = NULL;
    DWORD createFlags;

    /*
     * Fetch the current standard channels.  Note that we have to check the
     * type of each file, since we cannot duplicate some file types.
     */

    channel = Tcl_GetStdChannel(TCL_STDIN);
    if (channel) {
	stdInFile = Tcl_GetChannelFile(channel, TCL_READABLE);
	if (stdInFile) {
	    Tcl_GetFileInfo(stdInFile, &type);
	    if ((type < TCL_WIN_PIPE) || (type > TCL_WIN_CONSOLE)) {
		stdInFile = NULL;
	    }
	}
    }
    channel = Tcl_GetStdChannel(TCL_STDOUT);
    if (channel) {
	stdOutFile = Tcl_GetChannelFile(channel, TCL_WRITABLE);
	if (stdOutFile) {
	    Tcl_GetFileInfo(stdOutFile, &type);
	    if ((type < TCL_WIN_PIPE) || (type > TCL_WIN_CONSOLE)) {
		stdOutFile = NULL;
	    }
	}
    }
    channel = Tcl_GetStdChannel(TCL_STDERR);
    if (channel) {
	stdErrFile = Tcl_GetChannelFile(channel, TCL_WRITABLE);
	if (stdErrFile) {
	    Tcl_GetFileInfo(stdErrFile, &type);
	    if ((type < TCL_WIN_PIPE) || (type > TCL_WIN_CONSOLE)) {
		stdErrFile = NULL;
	    }
	}
    }

    /*
     * If the current process has a console attached, let the child inherit
     * it.  Otherwise, create the child as a detached process.
     */

    createFlags = (HasConsole() ? 0 : DETACHED_PROCESS);

    startInfo.cb = sizeof(startInfo);
    startInfo.lpReserved = NULL;
    startInfo.lpDesktop = NULL;
    startInfo.lpTitle = NULL;
    startInfo.dwX = startInfo.dwY = 0;
    startInfo.dwXSize = startInfo.dwYSize = 0;
    startInfo.dwXCountChars = startInfo.dwYCountChars = 0;
    startInfo.dwFillAttribute = 0;
    startInfo.dwFlags = STARTF_USESTDHANDLES;
    startInfo.wShowWindow = 0;
    startInfo.cbReserved2 = 0;
    startInfo.lpReserved2 = NULL;

    secAtts.nLength = sizeof(SECURITY_ATTRIBUTES);
    secAtts.lpSecurityDescriptor = NULL;
    secAtts.bInheritHandle = TRUE;

    Tcl_DStringInit(&buffer);

    for (firstArg = 0; firstArg < argc; firstArg = lastArg+1) { 

	startInfo.hStdInput = INVALID_HANDLE_VALUE;
	startInfo.hStdOutput = INVALID_HANDLE_VALUE;
	startInfo.hStdError = INVALID_HANDLE_VALUE;

	/*
	 * Convert the program name into native form.  Also, ensure that
	 * the argv entry was copied into the DString.
	 */

	Tcl_DStringFree(&buffer);
	execName = Tcl_TranslateFileName(interp, argv[firstArg], &buffer);
	if (execName == NULL) {
	    goto error;
	} else if (execName == argv[firstArg]) {
	    Tcl_DStringAppend(&buffer, argv[firstArg], -1);
	}

	/*
	 * Find the end of the current segment of the pipeline.
	 */

	joinThisError = 0;
	for (lastArg = firstArg; lastArg < argc; lastArg++) {
	    if (argv[lastArg][0] == '|') { 
		if (argv[lastArg][1] == 0) { 
		    break;
		}
		if ((argv[lastArg][1] == '&') && (argv[lastArg][2] == 0)) {
		    joinThisError = 1;
		    break;
		}
	    }
	}

	/*
	 * Now append the rest of the command line arguments.
	 */

	for (i = firstArg + 1; i < lastArg; i++) {
	    Tcl_DStringAppend(&buffer, " ", -1);
	    Tcl_DStringAppend(&buffer, argv[i], -1);
	}

	/*
	 * If this is the last segment, use the specified outputFile.
	 * Otherwise create an intermediate pipe.
	 */

	if (lastArg == argc) { 
	    curOutFile = outputFile;
	} else {
	    if (!TclCreatePipe(&pipeIn, &curOutFile)) {
		Tcl_AppendResult(interp, "couldn't create pipe: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	}

	/*
	 * In the absence of any redirections, use the standard handles.
	 */

	if (!curInFile) {
	    curInFile = stdInFile;
	}
	if (!curOutFile) {
	    curOutFile = stdOutFile;
	}
	if (!curOutFile) {
	    errorFile = stdErrFile;
	}

	/*
	 * Duplicate all the handles which will be passed off as stdin, stdout
	 * and stderr of the child process. The duplicate handles are set to
	 * be inheritable, so the child process can use them.
	 */

	if (curInFile) {
	    if (!DuplicateHandle(GetCurrentProcess(),
		    (HANDLE) Tcl_GetFileInfo(curInFile, NULL),
		    GetCurrentProcess(), &startInfo.hStdInput, 0, TRUE,
		    DUPLICATE_SAME_ACCESS)) {
		TclWinConvertError(GetLastError());
		Tcl_AppendResult(interp, "couldn't duplicate input handle: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	}
	if (curOutFile) {
	    if (!DuplicateHandle(GetCurrentProcess(),
		    (HANDLE) Tcl_GetFileInfo(curOutFile, NULL),
		    GetCurrentProcess(), &startInfo.hStdOutput, 0, TRUE,
		    DUPLICATE_SAME_ACCESS)) {
		TclWinConvertError(GetLastError());
		Tcl_AppendResult(interp, "couldn't duplicate output handle: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	}
	if (joinThisError) {
	    if (!DuplicateHandle(GetCurrentProcess(), (HANDLE)
		    Tcl_GetFileInfo(curOutFile, NULL),
		    GetCurrentProcess(), &startInfo.hStdError, 0, TRUE,
		    DUPLICATE_SAME_ACCESS)) {
		TclWinConvertError(GetLastError());
		Tcl_AppendResult(interp, "couldn't duplicate output handle: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	} else if (errorFile) {
	    if (!DuplicateHandle(GetCurrentProcess(), (HANDLE)
		    Tcl_GetFileInfo(errorFile, NULL),
		    GetCurrentProcess(), &startInfo.hStdError, 0, TRUE,
		    DUPLICATE_SAME_ACCESS)) {
		TclWinConvertError(GetLastError());
		Tcl_AppendResult(interp, "couldn't duplicate error handle: ",
			Tcl_PosixError(interp), (char *) NULL);
		goto error;
	    }
	}

	/*
	 * If any handle was not set, open the null device instead.
	 */

	if (startInfo.hStdInput == INVALID_HANDLE_VALUE) {
	    startInfo.hStdInput = CreateFile("NUL:", GENERIC_READ, 0,
		    &secAtts, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	if (startInfo.hStdOutput == INVALID_HANDLE_VALUE) {
	    startInfo.hStdOutput = CreateFile("NUL:", GENERIC_WRITE, 0,
		    &secAtts, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}
	if (startInfo.hStdError == INVALID_HANDLE_VALUE) {
	    startInfo.hStdError = CreateFile("NUL:", GENERIC_WRITE, 0,
		    &secAtts, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	}

	/*
	 * Start the subprocess by invoking the executable directly.  If that
	 * fails, then attempt to use cmd.exe or command.com.
	 */

	if (!CreateProcess(NULL, Tcl_DStringValue(&buffer), NULL, NULL,
		TRUE, createFlags, NULL, NULL, &startInfo, &procInfo)) {
	    TclWinConvertError(GetLastError());
	    Tcl_AppendResult(interp, "couldn't execute \"", argv[firstArg],
		    "\": ", Tcl_PosixError(interp), (char *) NULL);
	    goto error;
	}
	Tcl_DStringFree(&buffer);

	/*
	 * Add the child process to the list of those to be reaped.  Also,
	 * close the thread handle, since it won't be used.
	 */

	pid = (int) procInfo.hProcess;
	pidPtr[*numPids] = pid;
	(*numPids)++;
	CloseHandle(procInfo.hThread);

	/*
	 * Close off our copies of file descriptors that were set up for
	 * this child, then set up the input for the next child.
	 */

	if (curInFile && (curInFile != inputFile)
		&& (curInFile != stdInFile)) {
	    TclCloseFile(curInFile);
	}
	curInFile = pipeIn;
	pipeIn = NULL;

	if (curOutFile && (curOutFile != outputFile)
		&& (curOutFile != stdOutFile)) {
	    TclCloseFile(curOutFile);
	}
	curOutFile = NULL;

	CloseHandle(startInfo.hStdInput);
	CloseHandle(startInfo.hStdOutput);
	CloseHandle(startInfo.hStdError);
    }
    return 1;

    /*
     * An error occured, so we need to clean up any open pipes.
     */

error:
    Tcl_DStringFree(&buffer);
    if (pipeIn) {
	TclCloseFile(pipeIn);
    }
    if (curOutFile && (curOutFile != outputFile)
		&& (curOutFile != stdOutFile)) {
	TclCloseFile(curOutFile);
    }
    if (curInFile && (curInFile != inputFile)
		&& (curInFile != stdInFile)) {
	TclCloseFile(curInFile);
    }
    CloseHandle(startInfo.hStdInput);
    CloseHandle(startInfo.hStdOutput);
    CloseHandle(startInfo.hStdError);
    return 0;
}

/*
 *----------------------------------------------------------------------
 *
 * HasConsole --
 *
 *	Determines whether the current application is attached to a
 *	console.
 *
 * Results:
 *	Returns TRUE if this application has a console, else FALSE.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static BOOL
HasConsole()
{
    HANDLE handle = CreateFile("CONOUT$", GENERIC_WRITE, FILE_SHARE_WRITE,
	    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (handle != INVALID_HANDLE_VALUE) {
        CloseHandle(handle);
	return TRUE;
    } else {
        return FALSE;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclHasPipes --
 *
 *      Checks to see if it's running under Win32s or Win32.
 *
 * Results:
 *      Returns 1 if running under Win32, 0 otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
TclHasPipes(void)
{
    OSVERSIONINFO info;
    info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    GetVersionEx(&info);
    return((info.dwPlatformId == VER_PLATFORM_WIN32s) ? 0 : 1);
}

/*
 *----------------------------------------------------------------------
 *
 * TclCreatePipe --
 *
 *      Creates an anonymous pipe.
 *
 * Results:
 *      Returns 1 on success, 0 on failure. 
 *
 * Side effects:
 *      Creates a pipe.
 *
 *----------------------------------------------------------------------
 */

int
TclCreatePipe(readPipe, writePipe)
    Tcl_File *readPipe;	/* Location to store file handle for
				 * read side of pipe. */
    Tcl_File *writePipe;	/* Location to store file handle for
				 * write side of pipe. */
{
    HANDLE readHandle, writeHandle;
    SECURITY_ATTRIBUTES sec;

    sec.nLength = sizeof(SECURITY_ATTRIBUTES);
    sec.lpSecurityDescriptor = NULL;
    sec.bInheritHandle = FALSE;

    if (!CreatePipe(&readHandle, &writeHandle, &sec, 0)) {
	TclWinConvertError(GetLastError());
	return 0;
    }

    *readPipe = Tcl_GetFile((ClientData)readHandle, TCL_WIN_FILE);
    *writePipe = Tcl_GetFile((ClientData)writeHandle, TCL_WIN_FILE);
    return 1;
}

/*
 *----------------------------------------------------------------------
 *
 * Win32sPipeline --
 *
 *      Given an argc/argv array, instantiate a pipeline of processes
 *      as described by the argv. This is the Win32s version, which works
 *      by creating a batch file to run the pipeline.
 *
 * Results:
 *      The return value is 1 on success, 0 on error
 *
 * Side effects:
 *      A batch file describing the pipeline is executed.
 *
 *----------------------------------------------------------------------
 */

static int
Win32sPipeline(interp, argc, argv, intIn, finalOut, pidPtr)
    Tcl_Interp *interp;		/* Interpreter in which to process pipeline. */
    int argc;			/* Number of entries in argv. */
    char **argv;		/* Array of strings describing commands in
				 * pipeline plus I/O redirection with <,
				 * <<, >, etc. argv[argc] must be NULL. */
    char *intIn;		/* File name for initial input (for Win32s). */
    char *finalOut;		/* File name for final output (for Win32s). */
    int *pidPtr;		/* Array of pids which are created. */
{
    char batchFile[20];		/* Name of batch file */
    int batchHandle;		/* Handle to the batch file */ 
    int outputHandle;		/* Handle to final output file */
    char string[100];		/* Temporary string */
    int numBytes;
    int firstArg, lastArg;	/* Indices of first and last arguments
				 * in current command. */
    Tcl_DString execProc;	/* Dynamic string used to construct
				 * intermediate commands */
    char *ptr;			/* Temporary pointer - used to build
				 * execProc strings */
    char intOut[L_tmpnam];	/* Name of temporary file */
    int result = 1;		/* Return value of this function. */
    int dirLength, offset;
    
    Tcl_DStringInit(&execProc);

    /*
     * Create batch file
     */

    tmpnam(batchFile);
    ptr = strchr(batchFile, '.');
    strcpy(ptr, ".bat");
    batchHandle = open(batchFile, O_CREAT | O_WRONLY | O_TEXT, 0600);
    if (batchHandle < 0) {
	Tcl_AppendResult(interp,
		"couldn't create batch file \"", batchFile,
		"\" for command: ", Tcl_PosixError(interp),
		(char *) NULL);
	result = 0;
	goto cleanup;
    }

    /*
     * Cycle through the pipeline, generating commands to place in batch file.
     */ 

    for (firstArg = 0; firstArg < argc; firstArg = lastArg+1) { 
	Tcl_DStringSetLength(&execProc, 0);
	for (lastArg = firstArg; lastArg < argc; lastArg++) {
	    if (argv[lastArg][0] == '|') {
		if (argv[lastArg][1] == 0) {
		    break;
		}
		if ((argv[lastArg][1] == '&') && (argv[lastArg][2] == 0)) {
		    break;
		}
	    } else {
		Tcl_DStringAppend(&execProc, argv[lastArg], -1);
		Tcl_DStringAppend(&execProc, " ", -1);
	    }
	}

	/*
	 * Check if this pipeline command is a Windows application.
	 */

	if (IsWinApp(argv[firstArg], interp)) {
	    if ((firstArg != 0) && (lastArg != argc)) {
		Tcl_AppendResult(interp,
			"Can't pipe input in or out of Windows app", finalOut,
			(char *) NULL);
		result = 0;
		close(batchHandle);
		goto cleanup;
	    }
	}

	/*
	 * Set input redirection for the current command in the pipeline.
	 */

	if (intIn[0]) {
	    Tcl_DStringAppend(&execProc, " < ", -1);
	    Tcl_DStringAppend(&execProc, intIn, -1);
	}

	/*
	 * Set output redirection for the current command in the pipeline.
	 */

	tmpnam(intOut);
	Tcl_DStringAppend(&execProc, " > ", -1);
	if (lastArg == argc) {
	    Tcl_DStringAppend(&execProc, finalOut, -1);
	} else {
	    Tcl_DStringAppend(&execProc, intOut, -1);
	}
	Tcl_DStringAppend(&execProc, "\r\n", -1);
	
	if (intIn[0]) {
	    Tcl_DStringAppend(&execProc, "del ", -1);
	    Tcl_DStringAppend(&execProc, intIn, -1);
	    Tcl_DStringAppend(&execProc, "\r\n", -1);
	}

	write(batchHandle, Tcl_DStringValue(&execProc),
		Tcl_DStringLength(&execProc));

	strcpy(intIn, intOut);
    }
    close(batchHandle);

    /*
     * Set up the command to execute the batch file
     */

    dirLength = GetCurrentDirectory(0, NULL);
    Tcl_DStringSetLength(&execProc, 0);
    Tcl_DStringAppend(&execProc, "command.com /C ", -1);
    offset = Tcl_DStringLength(&execProc);
    Tcl_DStringSetLength(&execProc, (dirLength - 1) + offset);
    GetCurrentDirectory(dirLength, Tcl_DStringValue(&execProc) + offset);
    Tcl_DStringAppend(&execProc, "\\", -1);
    Tcl_DStringAppend(&execProc, batchFile, -1);

    /*
     * Do a synchronous spawn of the batch file
     */

    TclSynchSpawn(Tcl_DStringValue(&execProc), SW_SHOWNORMAL);

    /*
     * Read the output from the command pipeline
     */

    outputHandle = open(finalOut, O_RDONLY | O_TEXT, 0600);
    if (outputHandle < 0) { 
	Tcl_AppendResult(interp,
		"couldn't read output file \"", finalOut,
		"\" for command: ", Tcl_PosixError(interp),
		(char *) NULL);
	result = 0;
	goto cleanup;
    }
    
    do {
	numBytes = read(outputHandle, string, 100);
	string[numBytes] = '\0';
	Tcl_AppendResult(interp, string, (char *)NULL);
    } while(numBytes > 0);
    close(outputHandle);

  cleanup:
    unlink(batchFile);
    unlink(finalOut);
    
    Tcl_DStringFree(&execProc);
    return(result);
}

/*
 *----------------------------------------------------------------------
 *
 * IsWinApp --
 *
 *      Given a filename, checks to see if that file is a Windows executable.
 *
 * Results:
 *      TRUE if it is a Windows executable, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static BOOL
IsWinApp(filename, interp)
    char *filename;                /* Name of file to check */
    Tcl_Interp *interp;            /* Interpreter in which pipeline is being
				    * processed. */ 
{
    int fileId;
    char buffer[MAX_PATH+1], *dummy;

   /*
    * If the word value at offset 18h of the file is 40h or greater,
    * the word value at 3Ch is an offset to a Windows header. Sooo.... we
    * must read the file header and check that offset to determine whether
    * "filename" is a Windows app.
    */

    if (!SearchPath(NULL, filename, ".exe", MAX_PATH, buffer, &dummy)) {
	return FALSE;
    }
	
    fileId = open(buffer, O_RDONLY, 0600);
    if (fileId < 0) {
	Tcl_AppendResult(interp,
		"couldn't open file \"", filename,
		"\" for command: ", Tcl_PosixError(interp),
		(char *) NULL);
    } else {
	read(fileId, buffer, MAX_PATH);
	close(fileId);
	if (buffer[24] >= 64) {
	    return(TRUE);
	}
    }
    return(FALSE);
}

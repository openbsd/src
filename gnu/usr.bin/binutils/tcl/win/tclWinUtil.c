/* 
 * tclWinUtil.c --
 *
 *	This file contains a collection of utility procedures that
 *	are present in Tcl's Windows core but not in the generic
 *	core.  For example, they do file manipulation and process
 *	manipulation.
 *
 * Copyright (c) 1994-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWinUtil.c 1.9 96/01/16 10:31:48
 */

#include "tclInt.h"
#include "tclPort.h"


/*
 *----------------------------------------------------------------------
 *
 * Tcl_WaitPid --
 *
 *	Does the waitpid system call.
 *
 * Results:
 *	Returns return value of pid it's waiting for.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
Tcl_WaitPid(pid, statPtr, options)
    pid_t pid;
    int *statPtr;
    int options;
{
    int flags;
    DWORD ret;

    if (options & WNOHANG) {
	flags = 0;
    } else {
	flags = INFINITE;
    }
    ret = WaitForSingleObject((HANDLE)pid, flags);
    if (ret == WAIT_TIMEOUT) {
	*statPtr = 0;
	return 0;
    } else if (ret != WAIT_FAILED) {
	GetExitCodeProcess((HANDLE)pid, (DWORD*)statPtr);
	*statPtr = ((*statPtr << 8) & 0xff00);
	CloseHandle((HANDLE)pid);
	return pid;
    } else {
	errno = ECHILD;
	return -1;
    }
}

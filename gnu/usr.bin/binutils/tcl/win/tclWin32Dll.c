/* 
 * tclWin32Dll.c --
 *
 *	This file contains the DLL entry point which sets up the 32-to-16-bit
 *	thunking code for SynchSpawn if the library is running under Win32s.
 *
 * Copyright (c) 1995-1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclWin32Dll.c 1.13 96/02/13 19:07:11
 */

#include <windows.h>
#include "tcl.h"
#include "tclPort.h"
#include "tclWinInt.h"

typedef DWORD (WINAPI * UT32PROC)(LPVOID lpBuff, DWORD dwUserDefined,
	LPVOID *lpTranslationList);

typedef BOOL (WINAPI * PUTREGISTER)(HANDLE hModule, LPCSTR SixteenBitDLL,
	LPCSTR InitName, LPCSTR ProcName, UT32PROC* ThirtyTwoBitThunk,
	FARPROC UT32Callback, LPVOID Buff);

typedef VOID (WINAPI * PUTUNREGISTER)(HANDLE hModule);

typedef DWORD (APIENTRY * PUT32CBPROC)(LPVOID Buff, DWORD UserDefined);

UT32PROC      tclUTProc = NULL;
PUTREGISTER   tclUTRegister = NULL;
PUTUNREGISTER tclUTUnRegister = NULL;
PUT32CBPROC   tclUT32CBProc = NULL;
int           tclProcessesAttached = 0;
BOOL          tclWin32s = FALSE;

/*
 * The following data structure is used to keep track of all of the DLL's
 * opened by Tcl so that they can be freed with the Tcl.dll is unloaded.
 */

typedef struct LibraryList {
    HINSTANCE handle;
    struct LibraryList *nextPtr;
} LibraryList;

static LibraryList *libraryList = NULL;	/* List of currently loaded DLL's.  */

static HINSTANCE tclInstance;		/* Global library instance handle. */

/*
 * Declarations for functions that are only used in this file.
 */

static void 		UnloadLibraries _ANSI_ARGS_((void));

/*
 * The following declaration is for the VC++ DLL entry point.
 */

BOOL APIENTRY		DllMain _ANSI_ARGS_((HINSTANCE hInst,
			    DWORD reason, LPVOID reserved));

/*
 *----------------------------------------------------------------------
 *
 * DllEntryPoint --
 *
 *	This wrapper function is used by Borland to invoke the
 *	initialization code for Tcl.  It simply calls the DllMain
 *	routine.
 *
 * Results:
 *	See DllMain.
 *
 * Side effects:
 *	See DllMain.
 *
 *----------------------------------------------------------------------
 */

BOOL APIENTRY
DllEntryPoint(hInst, reason, reserved)
    HINSTANCE hInst;		/* Library instance handle. */
    DWORD reason;		/* Reason this function is being called. */
    LPVOID reserved;		/* Not used. */
{
    return DllMain(hInst, reason, reserved);
}

/*
 *----------------------------------------------------------------------
 *
 * DllMain --
 *
 *	This routine is called by the VC++ C run time library init
 *	code, or the DllEntryPoint routine.  It is responsible for
 *	initializing various dynamically loaded libraries.
 *
 * Results:
 *	TRUE on sucess, FALSE on failure.
 *
 * Side effects:
 *	Establishes 32-to-16 bit thunk and initializes sockets library.
 *
 *----------------------------------------------------------------------
 */
BOOL APIENTRY
DllMain(hInst, reason, reserved)
    HINSTANCE hInst;		/* Library instance handle. */
    DWORD reason;		/* Reason this function is being called. */
    LPVOID reserved;		/* Not used. */
{
    char buffer[] = "TCL16xx.DLL";
    OSVERSIONINFO info;
    HINSTANCE tclKernel32;

    switch (reason) {
    case DLL_PROCESS_ATTACH:

	/*
	 * Registration of UT need to be done only once for first
	 * attaching process.  At that time set the tclWin32s flag
	 * to indicate if the DLL is executing under Win32s or not.
	 */

	if (tclProcessesAttached++) {
	    return FALSE;         /* Not the first initialization. */
	}

	tclInstance = hInst;

	/*
	 * Find out if we're running on Win32s.
	 */

	info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&info);
	tclWin32s = (info.dwPlatformId == VER_PLATFORM_WIN32s);

	if (tclWin32s) {
	    /*
	     * Load the Universal Thunking routines from kernel32.dll.
	     */

	    tclKernel32 = TclWinLoadLibrary("Kernel32.Dll");
	    if (tclKernel32 == NULL) {
		return FALSE;
	    }

	    tclUTRegister = (PUTREGISTER) GetProcAddress(tclKernel32,
		    "UTRegister");
	    tclUTUnRegister = (PUTUNREGISTER) GetProcAddress(tclKernel32,
		    "UTUnRegister");
	    if (!tclUTRegister || !tclUTUnRegister) {
		UnloadLibraries();
		return FALSE;
	    }

	    /*
	     * Construct the complete name of tcl16xx.dll.
	     */

	    buffer[5] = '0' + TCL_MAJOR_VERSION;
	    buffer[6] = '0' + TCL_MINOR_VERSION;

	    /*
	     * Register the Tcl thunk.
	     */

	    if (!(*tclUTRegister)(hInst, buffer, "UTInit", "UTProc",
		    &tclUTProc, (FARPROC)tclUT32CBProc, NULL)) {
		UnloadLibraries();
		return FALSE;
	    }
	}

	return TRUE;

    case DLL_PROCESS_DETACH:

	tclProcessesAttached--;
	if (tclProcessesAttached == 0) {

	    /*
	     * Unregister the Tcl thunk.
	     */

	    if (tclWin32s) {
		(*tclUTUnRegister)(hInst);
	    }

	    /*
	     * Cleanup any dynamically loaded libraries.
	     */

	    UnloadLibraries();
	}
	break;
    }

    return TRUE; 
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinLoadLibrary --
 *
 *	This function is a wrapper for the system LoadLibrary.  It is
 *	responsible for adding library handles to the library list so
 *	the libraries can be freed when tcl.dll is unloaded.
 *
 * Results:
 *	Returns the handle of the newly loaded library, or NULL on
 *	failure.
 *
 * Side effects:
 *	Loads the specified library into the process.
 *
 *----------------------------------------------------------------------
 */

HINSTANCE
TclWinLoadLibrary(name)
    char *name;			/* Library file to load. */
{
    HINSTANCE handle;
    LibraryList *ptr;

    handle = LoadLibrary(name);
    if (handle != NULL) {
	ptr = (LibraryList*) ckalloc(sizeof(LibraryList));
	ptr->handle = handle;
	ptr->nextPtr = libraryList;
	libraryList = ptr;
    } else {
	TclWinConvertError(GetLastError());
    }
    return handle;
}

/*
 *----------------------------------------------------------------------
 *
 * UnloadLibraries --
 *
 *	Frees any dynamically allocated libraries loaded by Tcl.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	Frees the libraries on the library list as well as the list.
 *
 *----------------------------------------------------------------------
 */

static void
UnloadLibraries()
{
    LibraryList *ptr;

    while (libraryList != NULL) {
	FreeLibrary(libraryList->handle);
	ptr = libraryList->nextPtr;
	ckfree(libraryList);
	libraryList = ptr;
    }
}

/*
 *----------------------------------------------------------------------
 *
 * TclSynchSpawn --
 *
 *	32-bit entry point to the 16-bit SynchSpawn code.
 *
 * Results:
 *	1 on sucess, 0 on failure.
 *
 * Side effects:
 *	Spawns a command and waits for it to complete.
 *
 *----------------------------------------------------------------------
 */
DWORD APIENTRY
TclSynchSpawn( LPCSTR CmdLine, UINT CmdShow )
{
   DWORD Args[2];
   PVOID Translist[2];

   Args[0] = (DWORD) CmdLine;
   Args[1] = (DWORD) CmdShow;

   Translist[0] = &Args[0];
   Translist[1] = NULL;

   return( (*tclUTProc)( Args, TCLSYNCHSPAWN, Translist) );
}

/*
 *----------------------------------------------------------------------
 *
 * TclWinGetTclInstance --
 *
 *	Retrieves the global library instance handle.
 *
 * Results:
 *	Returns the global library instance handle.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

HINSTANCE
TclWinGetTclInstance()
{
    return tclInstance;
}

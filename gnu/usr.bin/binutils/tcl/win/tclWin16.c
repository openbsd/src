/* 
 * tclWin16.c --
 *
 *	This file contains code for a 16-bit DLL to handle 32-to-16 bit
 *      thunking. This is necessary for the Win32s SynchSpawn() call.
 *
 * Copyright (c) 1994 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef APIENTRY
#define APIENTRY
#endif

#include <windows.h>
#include <toolhelp.h>
#include <malloc.h>
#ifdef _MSC_VER
#include "tclWinInt.h"
#else
#include "tclWinIn.h"
#endif

typedef DWORD (FAR PASCAL  * UT16CBPROC)(LPVOID lpBuff, DWORD dwUserDefined,
	LPVOID FAR *lpTranslationList);

BOOL FAR PASCAL NotifyCallbackProc(WORD id, DWORD data);

UT16CBPROC tclUT16CallBack;

HINSTANCE hInstance;
HTASK ExecedTaskHandle;
int done;


/*
 *----------------------------------------------------------------------
 *
 * LibMain --
 *
 *	DLL entry point
 *
 * Results:
 *	Returns 1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
int FAR PASCAL
LibMain(instance, dataSeg, heapSize, cmdLine)
    HINSTANCE instance;
    WORD dataSeg;
    WORD heapSize;
    LPSTR cmdLine;
{
    hInstance = instance;
    return 1;
}


/*
 *----------------------------------------------------------------------
 *
 * UTInit --
 *
 *	Universal Thunk initialization procedure.
 *
 * Results:
 *	Always returns 1.
 *
 * Side effects:
 *	Sets the universal thunk callback procedure.
 *
 *----------------------------------------------------------------------
 */
DWORD FAR PASCAL _export
UTInit(callback, buf)
    UT16CBPROC callback;
    LPVOID buf;
{
    tclUT16CallBack = callback;
    return 1;   /* Return Success */
} 

/*
 *----------------------------------------------------------------------
 *
 * UTProc --
 *
 *	Universal Thunk dispatch routine.
 *
 * Results:
 *	1 on success, 0 or -1 on failure.
 *
 * Side effects:
 *	Executes 16-bit code.
 *
 *----------------------------------------------------------------------
 */

DWORD FAR PASCAL _export
UTProc(buf, func)
    LPVOID buf;
    DWORD func;
{
    switch (func) {
	case TCLSYNCHSPAWN: {
	    UINT inst;
	    LPCSTR cmdLine;
	    UINT cmdShow;
	    MSG msg;
	    BOOL again=TRUE;
	    TASKENTRY te;
	    LPFNNOTIFYCALLBACK lpfnNotifyProc;
	    
	    /* Retrieve the command line arguments stored in buffer */

	    cmdLine = (LPSTR) ((LPDWORD)buf)[0];
	    cmdShow = (UINT) ((LPDWORD)buf)[1];

	    /* Start the application with WinExec() */
	    
	    inst = WinExec(cmdLine, cmdShow);
	    if (inst < 32) {
		return 0;
	    }

	    /* Loop until the application is terminated. The Toolhelp API
	     * ModuleFindHandle() returns NULL when the application is
	     * terminated. NOTE: PeekMessage() is used to yield the
	     * processor; otherwise, nothing else could execute on the
	     * system.
	     */

	    te.dwSize = sizeof(TASKENTRY);
	    if (TaskFirst(&te)) {
		while((te.hInst != (HINSTANCE) inst) && TaskNext(&te));
	    }
	    if (te.hInst == (HINSTANCE) inst) {
		ExecedTaskHandle = te.hTask;
	    }
	    
	    lpfnNotifyProc = (LPFNNOTIFYCALLBACK) MakeProcInstance(
		(FARPROC) NotifyCallbackProc, hInstance);
	    done = 0;
	    NotifyRegister(NULL, lpfnNotifyProc, NF_NORMAL);
	    while( !done && again ) {
		while( PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) && again ) {
		    if (msg.message == WM_QUIT) {
			PostQuitMessage(msg.wParam);
			again=FALSE;
		    }
		    TranslateMessage(&msg);
		    DispatchMessage(&msg);
		}
	    }
	    NotifyUnRegister(NULL);
	    return 1;
	}
    }

    return (DWORD)-1L; /* We should never get here. */
}

/*
 *----------------------------------------------------------------------
 *
 * _WEP --
 *
 *	Windows exit procedure
 *
 * Results:
 *	Always returns 1.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

int FAR PASCAL
_WEP(dummy)
    int dummy;
{
   return 1;
}

BOOL FAR PASCAL
NotifyCallbackProc(id, data)
    WORD id;
    DWORD data;
{
    if ((id == NFY_EXITTASK) && (GetCurrentTask() == ExecedTaskHandle)) {
	done = 1;
    }
    return FALSE;
}

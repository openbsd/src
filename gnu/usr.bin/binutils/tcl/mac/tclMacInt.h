/*
 * tclMacInt.h --
 *
 *	Declarations of Macintosh specific shared variables and procedures.
 *
 * Copyright (c) 1996 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * SCCS: @(#) tclMacInt.h 1.5 96/03/25 17:54:59
 */

#ifndef _TCLMACINT
#define _TCLMACINT

#include <Files.h>

typedef pascal void (*ExitToShellProcPtr)(void);

/*
 * Prototypes for functions found in the tclMacUtil.c compatability library.
 */

EXTERN int 	FSpGetDefaultDir _ANSI_ARGS_((FSSpecPtr theSpec));
EXTERN int 	FSpSetDefaultDir _ANSI_ARGS_((FSSpecPtr theSpec));
EXTERN int 	FSpLocationFromPath _ANSI_ARGS_((int length, char *path,
		    FSSpecPtr theSpec));
EXTERN OSErr 	FSpFindFolder _ANSI_ARGS_((short vRefNum, OSType folderType,
		    Boolean createFolder, FSSpec *spec));
EXTERN void	GetGlobalMouse _ANSI_ARGS_((Point *mouse));
EXTERN OSErr	InstallExitToShellPatch _ANSI_ARGS_((
		    ExitToShellProcPtr newProc));

/*
 * Prototypes of Mac only internal functions.
 */

EXTERN void	TclCreateMacEventSource _ANSI_ARGS_((void));
EXTERN int	TclMacConsoleInit _ANSI_ARGS_((void));
EXTERN int 	TclMacEvalResource _ANSI_ARGS_((Tcl_Interp *interp,
		    char *resourceName, int resourceNumber, char *fileName));
EXTERN int	TclMacNotifySocket _ANSI_ARGS_((void));
EXTERN void	TclMacRemoveTimer _ANSI_ARGS_((void *timerToken));
EXTERN int	TclMacSocketReady _ANSI_ARGS_((Tcl_File file, int mask));
EXTERN void *	TclMacStartTimer _ANSI_ARGS_((long ms));
EXTERN int	TclMacTimerExpired _ANSI_ARGS_((void *timerToken));
EXTERN void	TclMacWatchSocket _ANSI_ARGS_((Tcl_File file, int mask));
#endif /* _TCLMACINT */

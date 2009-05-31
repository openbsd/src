#ifndef __LYEXIT_H
/*
 *	Avoid include redundancy
 */
#define __LYEXIT_H

/*
 *	Copyright (c) 1994, University of Kansas, All Rights Reserved
 *
 *	Include File:	LYexit.h
 *	Purpose:	Provide an atexit function for libraries without such.
 *	Remarks/Portability/Dependencies/Restrictions:
 *		Include this header in every file that you have an exit or
 *			atexit statment.
 *	Revision History:
 *		06-15-94	created Lynx 2-3-1 Garrett Arch Blythe
 */

/*
 *	Required includes
 */
#ifdef _WINDOWS
#include <process.h>		/* declares exit() */
#endif

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif
/*
 *	Constant defines
 */
#ifdef exit
#undef exit
#endif
#define exit(code) LYexit(code)
#define atexit LYatexit
#define ATEXITSIZE 50
/*
 *	Data structures
 *//*
   * Global variable declarations
 *//*
   * Macros
 *//*
   * Function declarations
 */ extern void outofmem(const char *fname, const char *func);
    extern void reset_signals(void);
    extern void exit_immediately(int status) GCC_NORETURN;
    extern void LYexit(int status) GCC_NORETURN;
    extern int LYatexit(void (*function) (void));

#ifdef __cplusplus
}
#endif
#endif				/* __LYEXIT_H */

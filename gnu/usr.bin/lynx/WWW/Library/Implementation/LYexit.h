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

#ifndef HTUTILS_H
#include <HTUtils.h>
#endif

/*
 *	Constant defines
 */
#ifdef _WINDOWS
#undef exit
#endif /* _WINDOWS */

#define exit LYexit

#define atexit LYatexit
#define ATEXITSIZE 40

/*
 *	Data structures
 */

/*
 *	Global variable declarations
 */

/*
 *	Macros
 */

/*
 *	Function declarations
 */
extern void exit_immediately PARAMS((int status)) GCC_NORETURN;
extern void LYexit PARAMS((int status)) GCC_NORETURN;
extern int LYatexit PARAMS((void (*function)(void)));

#endif /* __LYEXIT_H */

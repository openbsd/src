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
#include <stdlib.h>
#include "HTUtils.h"

/*
 *	Constant defines
 */
#ifdef _WINDOWS
#undef exit
#endif /* _WINDOWS */

#define exit LYexit
#define atexit LYatexit
#define ATEXITSIZE 32

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
extern void LYexit PARAMS((int status));
#ifdef __STDC__
extern int LYatexit(void (*function)(void));
#else
extern int LYatexit();
#endif /* __STDC__ */

#endif /* __LYEXIT_H */

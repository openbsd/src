
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	deb.h
 * DESCRIPTION	:	Defines Breakpoint macro.
 * Author		:	SGP
 * Date			:	January 2001.
 *
 */



#ifndef __Inc__deb___
#define __Inc__deb___


#include <nwconio.h>


#if defined(DEBUGON) && !defined(USE_D2)
	//debug build and d1 flag is used, so enable IDB
	#define DBGMESG	ConsolePrintf
	#define IDB(x)					\
				ConsolePrintf(x);	\
				_asm {int 3}
#else
	#if defined(USE_D2)
		//debug build and d2 flag is used, so disable IDB
		#define DBGMESG ConsolePrintf
		#define IDB ConsolePrintf
	#else
		//release build, so disable DBGMESG and IDB
		#define DBGMESG 
		#define IDB 
	#endif	//if defined(USE_D2)
#endif	//if defined(DEBUGON) && !defined(USE_D2)


#endif	/*__Inc__deb___*/


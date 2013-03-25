
/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	Win32ish.h
 * DESCRIPTION	:	For Win32 type definitions like BOOL.
 * Author		:	HYAK
 * Date			:	January 2001.
 *
 */



#ifndef __Win32ish_H__
#define __Win32ish_H__


#ifndef BOOL
	typedef unsigned int BOOL;
#endif

#ifndef DWORD
	typedef unsigned long DWORD;
#endif

typedef DWORD	LCID;
typedef long HRESULT;
typedef void* LPVOID;

#ifndef TRUE
	#define TRUE	1
#endif

#ifndef FALSE
	#define FALSE	0
#endif


#endif		// __Win32ish_H__



/*
 * Copyright © 2001 Novell, Inc. All Rights Reserved.
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Artistic License, as specified in the README file.
 *
 */

/*
 * FILENAME		:	intdef.h
 * DESCRIPTION	:	ANSI functions hash defined to equivalent Netware functions.
 * Author		:	SGP
 * Date			:	July 1999.
 *
 */



#ifndef __INTDEF__
#define __INTDEF__


#include <nwlocale.h>
#include "..\utility\utility.h"


//ANSI functions define to equivalent NetWare internationalization functions

#define setlocale	NWLsetlocale
#define localeconv	NWLlocaleconv
#define	strncoll	NWstrncoll
#define strftime	NWLstrftime

#define atoi		NWLatoi
#define itoa        NWitoa
#define utoa        NWutoa
#define ultoa       NWultoa
#define ltoa        NWltoa

#define isalnum		NWLisalnum
#define	isalpha		NWLisalpha
#define isdigit		NWLisdigit

#define strlen		NWLmbslen
#define mblen		NWLmblen

//#define strcpy(x,y) NWLstrbcpy(x,y,NWstrlen(y)+1)
#define strcpy(x,y)     \
	NWstrncpy(x,y,NWstrlen(y)); \
	x[NWstrlen(y)] ='\0';
#define strncpy(x,y,z)     NWLstrbcpy(x,y,(z + 1))
#define strcat(x,y)		 NWLstrbcpy((x + NWstrlen(x)), y, (NWstrlen(y) +1))
#define strncmp(s1,s2,l) NWgstrncmp(s1,s2,l)
#define strnicmp(s1,s2,l) NWgstrnicmp(s1,s2,l)

#define toupper(s1)  NWCharUpr(s1)
#define wsprintf	 NWsprintf

#define strncat(x,y,l)   \
			NWsprintf("oops!!! Not yet defined for NWI18N, define in intdef.h, still using strncat\n");	\
			strncat(x,y,l);

#define strdup(s1)   \
			NWsprintf("oops!!! Not yet defined for NWI18N, define in intdef.h, still using strdup\n");	\
			strdup(s1);

#define strlist   \
			NWsprintf("oops!!! Not yet defined for NWI18N, define in intdef.h, still using strlist\n");	\
			strlist;

#define strlwr(s1)   \
			NWsprintf("oops!!! Not yet defined for NWI18N, define in intdef.h, still using strlwr\n");	\
			strlwr(s1);

#define strnset(s1,l1,l2)   \
			NWsprintf("oops!!! Not yet defined for NWI18N, define in intdef.h, still using strnset\n");	\
			strnset(s1,l1,l2);

#define strset(s1,l1)   \
			NWsprintf("oops!!! Not yet defined for NWI18N, define in intdef.h, still using strset\n");	\
			strset(s1,l1);


#endif	// __INTDEF__


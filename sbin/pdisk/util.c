/*
 * util.c -
 *
 * Written by Eryk Vershen (eryk@apple.com)
 */

/*
 * Copyright 1997,1998 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <stdio.h>
#include <ctype.h>

#include "version.h"
#include "util.h"


/*
 * Defines
 */
#define NumToolboxTraps() (                             \
	(NGetTrapAddress(_InitGraf, ToolTrap)           \
		== NGetTrapAddress(0xAA6E, ToolTrap))   \
	    ? 0x200 : 0x400                             \
    )
#define GetTrapType(theTrap) (                          \
	(((theTrap) & 0x800) != 0) ? ToolTrap : OSTrap  \
    )


/*
 * Types
 */


/*
 * Global Constants
 */


/*
 * Global Variables
 */
static char dynamic_version[10];

/*
 * Forward declarations
 */


/*
 * Routines
 */
void
clear_memory(void *dataPtr, unsigned long size)
{
    char           *ptr;

    ptr = (char *) dataPtr;
    while (size > 0) {
	*ptr++ = 0;
	--size;
    }
}


#if !defined(__linux__) && !defined(__unix__)
/* (see Inside Mac VI 3-8) */
int
TrapAvailable(short theTrap)
{
	TrapType                trapType;
	
	trapType = GetTrapType(theTrap);
	
	if (trapType == ToolTrap) {
	    theTrap &= 0x07FF;
	    if (theTrap >= NumToolboxTraps())
		theTrap = _Unimplemented;
	}
	
	return (
	    NGetTrapAddress(theTrap, trapType)
	    != NGetTrapAddress(_Unimplemented, ToolTrap)
	);
}
#endif


/* Ascii case-insensitive string comparison */
int
istrncmp(const char *x, const char *y, long len)
{
    unsigned char *p = (unsigned char *)x;
    unsigned char *q = (unsigned char *)y;

    while (len > 0) {
	if (tolower(*p) != tolower(*q)) {
	    return (*p - *q);
	} else if (*p == 0) {
	    break;
	}
	p++;
	q++;
	len--;
    }
    return (0);
}


const char *
get_version_string(void)
{
    int stage;
    /* "copy" of stuff from SysTypes.r, since we can't include that*/
    enum {development = 0x20, alpha = 0x40, beta = 0x60, final = 0x80, /* or */ release = 0x80};

    switch (kVersionStage) {
    case development:	stage = 'd'; break;
    case alpha:		stage = 'a'; break;
    case beta:		stage = 'b'; break;
    case final:		stage = 'f'; break;
    default:		stage = '?'; break;
    }

    if (kVersionBugFix != 0) {
	if (kVersionStage == final) {
	    snprintf(dynamic_version, sizeof dynamic_version, "%d.%d.%d",
		    kVersionMajor, kVersionMinor, kVersionBugFix);
	} else {
	    snprintf(dynamic_version, sizeof dynamic_version, "%d.%d.%d%c%d",
		    kVersionMajor, kVersionMinor, kVersionBugFix, stage, kVersionDelta);
	}
    } else {
	if (kVersionStage == final) {
	    snprintf(dynamic_version, sizeof dynamic_version, "%d.%d",
		    kVersionMajor, kVersionMinor);
	} else {
	    snprintf(dynamic_version, sizeof dynamic_version, "%d.%d%c%d",
		    kVersionMajor, kVersionMinor, stage, kVersionDelta);
	}
    }
    return dynamic_version;
}

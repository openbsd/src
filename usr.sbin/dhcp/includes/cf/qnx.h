/* qnx.h

   System dependencies for QNX...
*/

/*
 * Copyright (c) 1996 The Internet Software Consortium.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of RadioMail Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RADIOMAIL CORPORATION AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * RADIOMAIL CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software was written for the Internet Software Consortium by Ted Lemon
 * under a contract with Vixie Labs.
 */

#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <setjmp.h>
#include <limits.h>
#include <syslog.h>
#include <sys/select.h>

#include <sys/wait.h>
#include <signal.h>

#include <netdb.h>
extern int h_errno;

#include <net/if.h>
#define INADDR_LOOPBACK ((u_long)0x7f000001)

/* Varargs stuff... */
#include <stdarg.h>
#define VA_DOTDOTDOT ...
#define va_dcl
#define VA_start(list, last) va_start (list, last)

#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID	"/etc/dhcpd.pid"
#endif
#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID "/etc/dhclient.pid"
#endif
#ifndef _PATH_DHCRELAY_PID
#define _PATH_DHCRELAY_PID "/etc/dhcrelay.pid"
#endif

#define EOL	'\n'
#define VOIDPTR void *

/* Time stuff... */
#include <sys/time.h>
#define TIME time_t
#define GET_TIME(x)	time ((x))
#define TIME_DIFF(high, low)	 	(*(high) - *(low))
#define SET_TIME(x, y)	(*(x) = (y))
#define ADD_TIME(d, s1, s2) (*(d) = *(s1) + *(s2))
#define SET_MAX_TIME(x)	(*(x) = INT_MAX)

typedef unsigned char	u_int8_t;
typedef unsigned short	u_int16_t;
typedef unsigned long	u_int32_t;
typedef signed char	int8_t;
typedef signed short	int16_t;
typedef signed long	int32_t;

#define strcasecmp( s1, s2 )			stricmp( s1, s2 )
#define strncasecmp( s1, s2, n )		strnicmp( s1, s2, n )
#define vsnprintf( buf, size, fmt, list )	vsprintf( buf, fbuf, list )
#define random()				rand()

#define HAVE_SA_LEN
#define BROKEN_TM_GMT
#define USE_SOCKETS
#define NO_SNPRINTF
#undef AF_LINK

#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif
#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

/*
    NOTE: to get the routing of the 255.255.255.255 broadcasts to work
    under QNX, you need to issue the following command before starting
    the daemon:

    	route add -interface 255.255.255.0 <hostname>

    where <hostname> is replaced by the hostname or IP number of the
    machine that dhcpd is running on.
*/

/* sunos5-5.h

   System dependencies for Solaris 2.x (tested on 2.5 with gcc)... */

/*
 * Copyright (c) 1996, 1998 The Internet Software Consortium.
 * All rights reserved.
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
 * 3. Neither the name of The Internet Software Consortium nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE INTERNET SOFTWARE CONSORTIUM OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software was written for the Internet Software Consortium by Ted Lemon
 * under a contract with Vixie Laboratories.
 */

/* Basic Integer Types not defined in SunOS headers... */

#define int8_t		char
#define int16_t		short
#define int32_t		long

#define u_int8_t	unsigned char
#define u_int16_t	unsigned short 
#define u_int32_t	unsigned long 

/* The jmp_buf type is an array on Solaris, so we can't dereference it
   and must declare it differently. */

#define jbp_decl(x)	jmp_buf x
#define jref(x)		(x)
#define jdref(x)	(x)
#define jrefproto	jmp_buf

#include <syslog.h>
#include <sys/types.h>
#include <sys/sockio.h>

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <setjmp.h>
#include <limits.h>

extern int h_errno;

#include <net/if.h>
#include <net/if_arp.h>

/* Solaris 2.6 defines AF_LINK, so we need the rest of the baggage that
   comes with it, but of course Solaris 2.5 and previous do not. */
#if defined (AF_LINK)
#include <net/if_dl.h>
#endif

/*
 * Definitions for IP type of service (ip_tos)
 */
#define IPTOS_LOWDELAY          0x10
#define IPTOS_THROUGHPUT        0x08
#define IPTOS_RELIABILITY       0x04
/*      IPTOS_LOWCOST           0x02 XXX */

/* Solaris systems don't have /var/run, but some sites have added it.
   If you want to put dhcpd.pid in /var/run, define _PATH_DHCPD_PID
   in site.h. */
#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID	"/etc/dhcpd.pid"
#endif
#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID "/etc/dhclient.pid"
#endif
#ifndef _PATH_DHCRELAY_PID
#define _PATH_DHCRELAY_PID "/etc/dhcrelay.pid"
#endif

#if defined (__GNUC__) || defined (__SVR4)
/* Varargs stuff: use stdarg.h instead ... */
#include <stdarg.h>
#define VA_DOTDOTDOT ...
#define VA_start(list, last) va_start (list, last)
#define va_dcl
#else /* !__GNUC__*/
/* Varargs stuff... */
#include <varargs.h>
#define VA_DOTDOTDOT va_alist
#define VA_start(list, last) va_start (list)
#endif /* !__GNUC__*/

/* Solaris doesn't support limited sprintfs. */
#define vsnprintf(buf, size, fmt, list) vsprintf (buf, fmt, list)
#define NO_SNPRINTF

#define NEED_INET_ATON

#if defined (USE_DEFAULT_NETWORK)
# define USE_DLPI
# define USE_DLPI_PFMOD
#endif

#define USE_POLL

#define EOL	'\n'
#define VOIDPTR	void *

/* Time stuff... */

#include <time.h>

#define TIME time_t
#define GET_TIME(x)	time ((x))

#define random()	rand()

/* Solaris doesn't provide an endian.h, so we have to do it. */

#define BIG_ENDIAN 1
#define LITTLE_ENDIAN 2
#if defined (__i386) || defined (i386)
# define BYTE_ORDER LITTLE_ENDIAN
#else
# if defined (__sparc) || defined (sparc)
#  define BYTE_ORDER BIG_ENDIAN
# else
@@@ ERROR @@@   Unable to determine byte order!
# endif
#endif

#define ALIAS_NAMES_PERMUTED

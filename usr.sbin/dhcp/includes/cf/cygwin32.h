/* cygwin32.h

   System dependencies for Win32, compiled with Cygwin32... */

/*
 * Copyright (c) 1997 The Internet Software Consortium.  All rights reserved.
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

#include <sys/time.h>

#define IN
#define OUT
#undef fd_set
#undef FD_SET
#undef FD_CLR
#undef FD_ZERO
#undef FD_ISSET
#undef FD_ISCLR
#undef FD_SETSIZE
#define IFNAMSIZ 16
#include <winsock.h>

#include <syslog.h>
#include <string.h>
#include <paths.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <setjmp.h>
#include <limits.h>

#include <sys/wait.h>
#include <signal.h>

#define NO_H_ERRNO

#include <sys/param.h>

/* Varargs stuff... */
#include <stdarg.h>
#define VA_DOTDOTDOT ...
#define va_dcl
#define VA_start(list, last) va_start (list, last)
#define vsnprintf(buf, size, fmt, list) vsprintf (buf, fmt, list)
#define NO_SNPRINTF

#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID	"//e/etc/dhcpd.pid"
#endif
#ifndef _PATH_DHCPD_DB
#define _PATH_DHCPD_DB "//e/etc/dhcpd.leases"
#endif
#ifndef _PATH_DHCPD_CONF
#define _PATH_DHCPD_CONF "//e/etc/dhcpd.conf"
#endif
#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID "//e/etc/dhclient.pid"
#endif
#ifndef _PATH_DHCLIENT_DB
#define _PATH_DHCLIENT_DB "//e/etc/dhclient.leases"
#endif
#ifndef _PATH_DHCLIENT_CONF
#define _PATH_DHCLIENT_CONF "//e/etc/dhclient.conf"
#endif
#ifndef _PATH_DHCRELAY_PID
#define _PATH_DHCRELAY_PID "//e/etc/dhcrelay.pid"
#endif

#ifndef _PATH_RESOLV_CONF
#define _PATH_RESOLV_CONF "//e/etc/resolv.conf"
#endif

#define int8_t		char
#define int16_t		short 
#define int32_t		long 

#define u_int8_t	unsigned char		/* Not quite POSIX... */
#define u_int16_t	unsigned short 
#define u_int32_t	unsigned long 

#define EOL	'\n'
#define VOIDPTR void *

/* Time stuff... */
#define TIME time_t
#define GET_TIME(x)	time ((x))

#if defined (USE_DEFAULT_NETWORK)
#  define USE_SOCKETS
#endif

#ifdef __alpha__
#define PTRSIZE_64BIT
#endif

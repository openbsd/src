/* sample.h

   Sample config file for clients.

   This file is provided as a sample in case the system you want to run
   on is not currently supported.   If that is the case, follow the Porting::
   comments here and in other files as guides for what to change. */

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
 * 3. Neither the name of The Internet Software Consortium nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INTERNET SOFTWARE CONSORTIUM AND
 * CONTRIBUTORS ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Porting::

   Some systems do not define basic integer types as shown below.
   On some systems, you need to include <bitypes.h> or <sys/bitypes.h>.
   If you get parse errors in dhcpd.h while compiling dhcpd.conf, try
   including bitypes.h, and if that fails, use the hard-coded definitions
   shown below. */
   
#if 0
#include <sys/bitypes.h>
#endif

#if 0
#define int8_t		char
#define int16_t		short
#define int32_t		long

#define u_int8_t	unsigned char
#define u_int16_t	unsigned short 
#define u_int32_t	unsigned long 
#endif

#include <sys/types.h>

/* Porting::

   The jmp_buf type as declared in <setjmp.h> is sometimes a structure
   and sometimes an array.   By default, we assume it's a structure.
   If it's an array on your system, you may get compile warnings or errors
   as a result in confpars.c.   If so, try including the following definitions,
   which treat jmp_buf as an array: */

#if 0
#define jbp_decl(x)	jmp_buf x
#define jref(x)		(x)
#define jdref(x)	(x)
#define jrefproto	jmp_buf
#endif

/* Porting::

   Some older systems (e.g., Ultrix) still use the 4.2BSD-style syslog
   API.  These differ from later versions of the syslog API in that the
   openlog system call takes two arguments instead of three, and the
   facility code (the third argument to modern versions of openlog())
   is ORed into the log priority in the syslog() call.

   If you are running with the 4.2BSD-style syslog interface, define
   SYSLOG_4_2. */

/* #define SYSLOG_4_2 */

#include <syslog.h>

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

/* Porting::

   Some older systems do not have defines for IP type-of-service,
   or don't define them the way we expect.   If you get undefined
   symbol errors on the following symbols, they probably need to be
   defined here. */

#if 0
#define IPTOS_LOWDELAY          0x10
#define IPTOS_THROUGHPUT        0x08
#define IPTOS_RELIABILITY       0x04
#endif

/* Porting::

   Newer BSD derivatives store non-permanent daemon files in a
   directory called /var/run.   If your system has a /var/run,
   use it; otherwise, use /etc. */

#ifndef _PATH_DHCPD_PID
#define _PATH_DHCPD_PID	"/etc/dhcpd.pid"
#endif
#ifndef _PATH_DHCLIENT_PID
#define _PATH_DHCLIENT_PID "/etc/dhclient.pid"
#endif
#ifndef _PATH_DHCRELAY_PID
#define _PATH_DHCRELAY_PID "/etc/dhcrelay.pid"
#endif

/* Porting::

   If your system supports standard ANSI C, it should provide the file
   /usr/include/stdarg.h.   This contains the ANSI standard declarations
   for functions which take a variable number of arguments.

   Older systems with non-ANSI compilers cannot support this interface,
   and generally use the older varargs interface, defined in <varargs.h>.
   Some systems only support varargs, but define the interface in
   <stdarg.h> anyway.

   You must choose one of the two sets of definitions below.   Try
   stdarg.h first, unless you know it won't work.   If you have
   trouble compiling errwarn.c, try switching to the varargs.h definitions.
   If that fails, try using stdarg.h with the varargs definitions. */

#if 0
/* Stdarg definitions for ANSI-compliant C compilers. */
#include <stdarg.h>
#define VA_DOTDOTDOT ...
#define VA_start(list, last) va_start (list, last)
#define va_dcl
#endif

#if 0
/* Varargs definitions, for non-ANSI-compliant C compilers. */
#include <varargs.h>
#define VA_DOTDOTDOT va_alist
#define VA_start(list, last) va_start (list)
#endif

/* Porting::

   Some systems (notably 4.4BSD derivatives) support versions of the
   sprintf functions which will deposit a limited number of characters
   into the buffer; that limit is provided in an extra argument.
   If your system doesn't support this functionality, you must include
   the definitions below: */

#if 0
#define vsnprintf(buf, size, fmt, list) vsprintf (buf, fmt, list)
#define NO_SNPRINTF
#endif

/* Porting::

   Some systems provide a function, strerror(), which takes the unix
   error number (see errno) and returns a pointer to a static buffer
   containing the corresponding error message.

   If your system doesn't provide strerror(), define NO_STRERROR
   as shown below: */

#if 0
#define NO_STRERROR
char *strerror PROTO ((int));
#endif

/* Porting::

   Once dhcpd has initialized itself, it loops forever waiting for
   packets to come in.   Since we need to support multiple input streams
   in order to support multiple interfaces, dhcpd needs to be able to
   do a syscall to determine which descriptors have input waiting on
   them.

   Normally, dhcpd uses the select() system call, which is a 4.2BSD
   syscall invented precisely for this purpose.   Unfortunately, some
   System V-based systems do not support select() properly when it
   operates on streams.   The System V interface which does (largely)
   the same thing as select is called poll().   In some cases, this may
   work better than select() - if you find that dhcpd is hanging and not
   responding to packets very consistently, you might try defining
   USE_POLL and including <poll.h>. */

#if 0
#define USE_POLL
#include <poll.h>
#endif

/* Porting::

   You must define the default network API for your port.   This
   will depend on whether one of the existing APIs will work for
   you, or whether you need to implement support for a new API.
   Currently, the following APIs are supported:

   	The BSD socket API: define USE_SOCKETS.
	The Berkeley Packet Filter: define USE_BPF.
	The Streams Network Interface Tap (NIT): define USE_NIT.
	Raw sockets: define USE_RAW_SOCKETS

   If your system supports the BSD socket API and doesn't provide
   one of the supported interfaces to the physical packet layer,
   you can either provide support for the low-level API that your
   system does support (if any) or just use the BSD socket interface.
   The BSD socket interface doesn't support multiple network interfaces,
   and on many systems, it does not support the all-ones broadcast
   address, which can cause problems with some DHCP clients (e.g.
   Microsoft Windows 95). */

#if defined (USE_DEFAULT_NETWORK)
#  define USE_SOCKETS
#endif

/* Porting::

   Recent versions of BSD added a new element to the sockaddr structure:
   sa_len.   This indicates the length of the structure, and is used
   in a variety of places, not the least of which is the SIOCGIFCONF
   ioctl, which is used to figure out what interfaces are attached to
   the system.

   You should be able to determine if your system has an sa_len element
   by looking at the struct sockaddr definition in /usr/include/sys/socket.h.
   If it does, you must define HAVE_SA_LEN.   Otherwise, you must not.
   The most obvious symptom that you've got this wrong is either a compile
   error complaining about the use of the sa_len structure element, or
   the failure of dhcpd to find any interfaces. */

/* #define HAVE_SA_LEN */

/* Every operating system has its own way of seperating lines in a
   sequential text file.  Most modern systems use a single character,
   either an ASCII Newline (10) or an ASCII Carriage Return (13).

   The most notable exception is MS-DOS (and consequently, Windows),
   which uses an ASCII Carriage Return followed by a Newline to
   seperate each line.  Fortunately, MS-DOS C compiler libraries
   typically hide this from the programmer, returning just a Newline.

   Define EOL to be whatever getc() returns for a newline. */

#define EOL '\n'

/* Some older C compilers don't support the void pointer type.
   ANSI C defines void * to be a pointer type that matches
   any other pointer type.   This is handy for returning a pointer
   which will always need to be cast to a different value.   For
   example, malloc() on an ANSI C-compliant system returns void *.

   If your compiler doesn't support void pointers, you may need to
   define VOIDPTR to be char *; otherwise, define it to be void *. */

#define VOIDPTR void *

/* Porting::

   The following definitions for time should work on any unix machine.
   They may not work (or at least, may not work well) on a variety of
   non-unix machines.   If you are porting to a non-unix machine, you
   probably need to change the definitions below and perhaps include
   different headers.

   I should note that dhcpd is not yet entirely clean of unix-specific
   time references, so the list of defines shown below probably isn't
   good enough if you're porting to a system that really doesn't support
   unix time.   It's probably a reasonable place to start, though. */

#include <time.h>

#define TIME time_t
#define GET_TIME(x)	time ((x))

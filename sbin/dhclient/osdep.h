/*	$OpenBSD: osdep.h,v 1.10 2004/02/23 20:09:02 deraadt Exp $	*/

/* Operating system dependencies... */

/*
 * Copyright (c) 1996, 1997, 1998, 1999 The Internet Software Consortium.
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

#include <syslog.h>
#include <sys/types.h>
#include <string.h>
#include <paths.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <setjmp.h>
#include <limits.h>

#include <sys/wait.h>
#include <signal.h>

extern int h_errno;

#include <net/if.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <sys/sockio.h>

#include <sys/time.h>

#define ifr_netmask ifr_addr

/* Varargs stuff... */
#include <stdarg.h>

#define EOL	'\n'
#define VOIDPTR void *


#if defined(__alpha__) || defined(__sparc64__) || defined(__amd64__)
#define PTRSIZE_64BIT
#endif

#if !defined (TIME_MAX)
# define TIME_MAX 2147483647
#endif

#define PACKET_ASSEMBLY
#define PACKET_DECODING

/* jmp_buf is assumed to be a struct unless otherwise defined in the
   system header. */
#ifndef jbp_decl
# define jbp_decl(x)	jmp_buf *x
#endif
#ifndef jref
# define jref(x)	(&(x))
#endif
#ifndef jdref
# define jdref(x)	(*(x))
#endif
#ifndef jrefproto
# define jrefproto	jmp_buf *
#endif

#ifndef BPF_FORMAT
# define BPF_FORMAT "/dev/bpf%d"
#endif

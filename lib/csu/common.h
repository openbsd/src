/*	$OpenBSD: common.h,v 1.6 2002/09/17 21:16:01 deraadt Exp $	*/
/*	$NetBSD: common.h,v 1.3 1995/06/15 21:41:48 pk Exp $	*/

/*
 * Copyright (c) 1993,1995 Paul Kranenburg
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Paul Kranenburg.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <string.h>

#ifdef DYNAMIC

#include <sys/syscall.h>
#include <a.out.h>
#ifndef N_GETMAGIC
#define N_GETMAGIC(x)	((x).a_magic)
#endif
#ifndef N_BSSADDR
#define N_BSSADDR(x)	(N_DATADDR(x)+(x).a_data)
#endif

#include <sys/mman.h>

#include <link.h>
#include <dlfcn.h>

extern struct _dynamic	_DYNAMIC;
static void		__load_rtld(struct _dynamic *);
extern int		__syscall(int, ...);
int			_callmain(void);
static char		*_strrchr(char *, char);
#ifdef DEBUG
static char		*_getenv(char *);
static int		_strncmp(char *, char *, int);
#endif

#define LDSO	"/usr/libexec/ld.so"

/*
 * We need these system calls, but can't use library stubs
 */
#define _exit(v)		__syscall(SYS_exit, (v))
#define open(name, f, m)	__syscall(SYS_open, (name), (f), (m))
#define close(fd)		__syscall(SYS_close, (fd))
#define read(fd, s, n)		__syscall(SYS_read, (fd), (s), (n))
#define write(fd, s, n)		__syscall(SYS_write, (fd), (s), (n))
#define dup(fd)			__syscall(SYS_dup, (fd))
#define dup2(fd, fdnew)		__syscall(SYS_dup2, (fd), (fdnew))
#define mmap(addr, len, prot, flags, fd, off)	\
    __syscall(SYS___syscall, (quad_t)SYS_mmap, (addr), (len), (prot), (flags), \
	(fd), 0, (off_t)(off))

#define _FATAL(str) \
	write(2, str, sizeof(str)), \
	_exit(1);

#endif /* DYNAMIC */

extern int		main(int, char **, char **);
#ifdef MCRT0
extern void		monstartup(u_long, u_long);
extern void		_mcleanup(void);
#endif

char			**environ;
int			errno;
static char		empty[1];
char			*__progname = empty;
#ifndef DYNAMIC
#define _strrchr	strrchr
#endif

extern unsigned char	etext;
extern unsigned char	eprol asm ("eprol");


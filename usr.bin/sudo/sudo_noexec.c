/*
 * Copyright (c) 2004-2005 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <config.h>

#include <errno.h>
#ifndef HAVE_TIMESPEC
# include <time.h>
#endif
#ifdef __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include <compat.h>

#ifndef lint
__unused static const char rcsid[] = "$Sudo: sudo_noexec.c,v 1.5.2.2 2007/06/12 00:56:43 millert Exp $";
#endif /* lint */

/*
 * Dummy versions of the execve() family of syscalls.  We don't need
 * to stub out all of them, just the ones that correspond to actual
 * system calls (which varies by OS).  Note that it is still possible
 * to access the real syscalls via the syscall() interface but very
 * few programs actually do that.
 */

#ifndef errno
extern int errno;
#endif

#define DUMMY_BODY				\
{						\
    errno = EACCES;				\
    return(-1);					\
}

#ifdef __STDC__

#define DUMMY2(fn, t1, t2)			\
int						\
fn(t1 a1, t2 a2)				\
DUMMY_BODY

#define DUMMY3(fn, t1, t2, t3)			\
int						\
fn(t1 a1, t2 a2, t3 a3)				\
DUMMY_BODY

#define DUMMY_VA(fn, t1, t2)			\
int						\
fn(t1 a1, t2 a2, ...)				\
DUMMY_BODY

#else /* !__STDC__ */

#define DUMMY2(fn, t1, t2)			\
int						\
fn(a1, a2)					\
t1 a1; t2 a2;					\
DUMMY_BODY

#define DUMMY3(fn, t1, t2, t3)			\
int						\
fn(a1, a2, a3)					\
t1 a1; t2 a2; t3 a3;				\
DUMMY_BODY

#define DUMMY_VA(fn, t1, t2)			\
int						\
fn(a1, a2, va_alist)				\
t1 a1; t2 a2; va_dcl				\
DUMMY_BODY

#endif /* !__STDC__ */

DUMMY_VA(execl, const char *, const char *)
DUMMY_VA(_execl, const char *, const char *)
DUMMY_VA(__execl, const char *, const char *)
DUMMY_VA(execle, const char *, const char *)
DUMMY_VA(_execle, const char *, const char *)
DUMMY_VA(__execle, const char *, const char *)
DUMMY_VA(execlp, const char *, const char *)
DUMMY_VA(_execlp, const char *, const char *)
DUMMY_VA(__execlp, const char *, const char *)
DUMMY2(execv, const char *, char * const *)
DUMMY2(_execv, const char *, char * const *)
DUMMY2(__execv, const char *, char * const *)
DUMMY2(execvp, const char *, char * const *)
DUMMY2(_execvp, const char *, char * const *)
DUMMY2(__execvp, const char *, char * const *)
DUMMY3(execvP, const char *, const char *, char * const *)
DUMMY3(_execvP, const char *, const char *, char * const *)
DUMMY3(__execvP, const char *, const char *, char * const *)
DUMMY3(execve, const char *, char * const *, char * const *)
DUMMY3(_execve, const char *, char * const *, char * const *)
DUMMY3(__execve, const char *, char * const *, char * const *)
DUMMY3(fexecve, int , char * const *, char * const *)
DUMMY3(_fexecve, int , char * const *, char * const *)
DUMMY3(__fexecve, int , char * const *, char * const *)

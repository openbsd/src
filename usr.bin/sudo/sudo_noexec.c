/*
 * Copyright (c) 2004 Todd C. Miller <Todd.Miller@courtesan.com>
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

#include "config.h"

#include <errno.h>

#ifndef lint
static const char rcsid[] = "$Sudo: sudo_noexec.c,v 1.5 2004/02/13 21:36:43 millert Exp $";
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

#define DUMMY(fn, args, atypes)	\
int				\
fn args				\
    atypes			\
{				\
    errno = EACCES;		\
    return(-1);			\
}

DUMMY(execve, (path, argv, envp),
      const char *path; char *const argv[]; char *const envp[];)
DUMMY(_execve, (path, argv, envp),
      const char *path; char *const argv[]; char *const envp[];)
DUMMY(execv, (path, argv, envp),
      const char *path; char *const argv[];)
DUMMY(_execv, (path, argv, envp),
      const char *path; char *const argv[];)
DUMMY(fexecve, (fd, argv, envp),
      int fd; char *const argv[]; char *const envp[];)
DUMMY(_fexecve, (fd, argv, envp),
      int fd; char *const argv[]; char *const envp[];)

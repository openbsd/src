/*
 * Copyright (c) 1999, 2001 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_FLOCK
# include <sys/file.h>
#endif /* HAVE_FLOCK */
#include <stdio.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <fcntl.h>
#include <time.h>
#ifdef HAVE_UTIME
# ifdef HAVE_UTIME_H
#  include <utime.h>
# endif /* HAVE_UTIME_H */
#else
# include "emul/utime.h"
#endif /* HAVE_UTIME */

#include "sudo.h"

#ifndef lint
static const char rcsid[] = "$Sudo: fileops.c,v 1.4 2003/04/16 00:42:10 millert Exp $";
#endif /* lint */

/*
 * Update the access and modify times on a file.
 */
int
touch(path, when)
    char *path;
    time_t when;
{
#ifdef HAVE_UTIME_POSIX
    struct utimbuf ut, *utp;

    ut.actime = ut.modtime = when;
    utp = &ut;
#else
    /* BSD <= 4.3 has no struct utimbuf */
    time_t utp[2];

    utp[0] = utp[1] = when;
#endif /* HAVE_UTIME_POSIX */

    return(utime(path, utp));
}

/*
 * Lock/unlock a file.
 */
#ifdef HAVE_LOCKF
int
lock_file(fd, lockit)
    int fd;
    int lockit;
{
    int op = 0;

    switch (lockit) {
	case SUDO_LOCK:
	    op = F_LOCK;
	    break;
	case SUDO_TLOCK:
	    op = F_TLOCK;
	    break;
	case SUDO_UNLOCK:
	    op = F_ULOCK;
	    break;
    }
    return(lockf(fd, op, 0) == 0);
}
#elif HAVE_FLOCK
int
lock_file(fd, lockit)
    int fd;
    int lockit;
{
    int op = 0;

    switch (lockit) {
	case SUDO_LOCK:
	    op = LOCK_EX;
	    break;
	case SUDO_TLOCK:
	    op = LOCK_EX | LOCK_NB;
	    break;
	case SUDO_UNLOCK:
	    op = LOCK_UN;
	    break;
    }
    return(flock(fd, op) == 0);
}
#else
int
lock_file(fd, lockit)
    int fd;
    int lockit;
{
#ifdef F_SETLK
    int func;
    struct flock lock;

    lock.l_start = 0;
    lock.l_len = 0;
    lock.l_pid = getpid();
    lock.l_type = (lockit == SUDO_UNLOCK) ? F_UNLCK : F_WRLCK;
    lock.l_whence = SEEK_SET;
    func = (lockit == SUDO_TLOCK) ? F_SETLK : F_SETLKW;

    return(fcntl(fd, func, &lock) == 0);
#else
    return(TRUE);
#endif
}
#endif

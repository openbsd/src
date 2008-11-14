/*
 * Copyright (c) 1999-2005, 2007 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

#include <config.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#ifdef HAVE_FLOCK
# include <sys/file.h>
#endif /* HAVE_FLOCK */
#include <stdio.h>
#ifdef HAVE_STRING_H
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#include <ctype.h>
#include <limits.h>
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <fcntl.h>
#if TIME_WITH_SYS_TIME
# include <time.h>
#endif
#ifndef HAVE_TIMESPEC
# include <emul/timespec.h>
#endif

#include "sudo.h"

#ifndef LINE_MAX
# define LINE_MAX 2048
#endif

#ifndef lint
__unused static const char rcsid[] = "$Sudo: fileops.c,v 1.16 2008/11/09 14:13:12 millert Exp $";
#endif /* lint */

/*
 * Update the access and modify times on an fd or file.
 */
int
touch(fd, path, tsp)
    int fd;
    char *path;
    struct timespec *tsp;
{
    struct timeval times[2];

    if (tsp != NULL) {
	times[0].tv_sec = times[1].tv_sec = tsp->tv_sec;
	times[0].tv_usec = times[1].tv_usec = tsp->tv_nsec / 1000;
    }

#if defined(HAVE_FUTIME) || defined(HAVE_FUTIMES)
    if (fd != -1)
	return(futimes(fd, tsp ? times : NULL));
    else
#endif
    if (path != NULL)
	return(utimes(path, tsp ? times : NULL));
    else
	return(-1);
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

/*
 * Read a line of input, remove comments and strip off leading
 * and trailing spaces.  Returns static storage that is reused.
 */
char *
sudo_parseln(fp)
    FILE *fp;
{
    size_t len;
    char *cp = NULL;
    static char buf[LINE_MAX];

    if (fgets(buf, sizeof(buf), fp) != NULL) {
	/* Remove comments */
	if ((cp = strchr(buf, '#')) != NULL)
	    *cp = '\0';

	/* Trim leading and trailing whitespace/newline */
	len = strlen(buf);
	while (len > 0 && isspace(buf[len - 1]))
	    buf[--len] = '\0';
	for (cp = buf; isblank(*cp); cp++)
	    continue;
    }
    return(cp);
}

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

#include <sys/types.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>

#include "config.h"
#include <compat.h>

#ifndef lint
static const char rcsid[] = "$Sudo: gettime.c,v 1.1 2004/09/08 15:47:09 millert Exp $";
#endif /* lint */

/*
 * Get the current time via gettimeofday() for systems with
 * timespecs in struct stat or, otherwise, using time().
 * XXX - configure check for gettimeofday() - XXX
 */
int
gettime(ts)
    struct timespec *ts;
{
    int rval;
#if defined(HAVE_GETTIMEOFDAY) && (defined(HAVE_ST_MTIM) || defined(HAVE_ST_MTIMESPEC))
    struct timeval tv;

    rval = gettimeofday(&tv, NULL);
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
#else
    rval = (int)time(&ts->tv_sec);
    ts->tv_nsec = 0;
#endif
    return (rval);
}

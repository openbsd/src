/*
 * Copyright (c) 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

RCSID("$arla: mmaptime_test.c,v 1.6 2002/05/16 22:09:46 hin Exp $");

#ifdef USE_MMAPTIME

#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <err.h>
#include "mmaptime.h"

static void
print_time_diff(char *str, struct timeval *tv, struct timeval *tv2)
{
    struct timeval t;

    t.tv_sec = tv2->tv_sec - tv->tv_sec;
    t.tv_usec = tv2->tv_usec - tv->tv_usec;
    if (t.tv_usec < 0) {
	t.tv_usec += 100000;
	--t.tv_sec;
    }

    printf("%s: %f\n", 
	   str,
	   (double)t.tv_sec + 
	   (double)t.tv_usec / 100000 );
}


int main(int argc, char **argv)
{
    struct timeval t, t2, start, stop;
    int i, times;
    time_t tim;

    i = mmaptime_probe();
    if (i == EOPNOTSUPP) {
	errx(1, "You don't have support for mmaptime\n"
	     "Try run configure with --enable-mmaptime");
    } else if (i) {
	err(1, "probe");
    }

    if (!(argc == 2 && sscanf(argv[1], "%d", &times) == 1))
	times = 100000;

    /* Get date two different ways */

    if (mmaptime_gettimeofday(&t, NULL))
	err(1, "mmaptime_gettimeofday: here am I");
    if (gettimeofday(&t2, NULL))
	err(1, "gettimeofday: this should not happen");
    tim = t.tv_sec;
    printf("mmaptime: %s", ctime(&tim));
    tim = t2.tv_sec;
    printf("gettimeofday: %s", ctime(&tim));

    print_time_diff("Diff", &t, &t2);

    /* Test run */
     
    printf("Doing %d tests of mmaptime_gettimeofday()\n", times);
    if (mmaptime_gettimeofday(&start, NULL))
	err(1, "mmaptime_gettimeofday: start");
    for (i = 0 ; i < times ; i++) {
	if (mmaptime_gettimeofday(&t, NULL))
	    err(1, "mmaptime_gettimeofday: running");
    }
    if (mmaptime_gettimeofday(&stop, NULL))
	err(1, "mmaptime_gettimeofday: stop");
    print_time_diff("End time", &start, &stop);
   
    mmaptime_close();

    /* Do the same with gettimeofday */

    printf("Doing %d tests of gettimeofday()\n", times);
    if (gettimeofday(&start, NULL))
	err(1, "gettimeofday: start");
    for (i = 0 ; i < times ; i++) {
	if (gettimeofday(&t, NULL))
	    err(1, "gettimeofday: running");
    }
    if (gettimeofday(&stop, NULL))
	err(1, "gettimeofday: stop");
    print_time_diff("End time", &start, &stop);
    
    return 0;
}

#else /* !USE_MMAPTIME */

int
main(int argc, char **argv)
{
    return 0;
}

#endif /* USE_MMAPTIME */

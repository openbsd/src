/*
 * Copyright (c) 1998 - 2000 Kungliga Tekniska Högskolan
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

#include <roken.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <parse_units.h>

#include "bool.h"
#include "hash.h"
#include "log.h"
#include "arlamath.h"

struct timeval time1, time2;

static void
starttesting(char *msg)
{
    printf("--------------------------------------------\n");
    printf("testing %s...\n", msg);
    fflush (stdout);
    gettimeofday(&time1, NULL);
}    

static int
endtesting(int bool)
{
    gettimeofday(&time2, NULL);
    printf("-%s--------------------------------------\n", 
	   !bool ? "ok --" : "fail ");
    time2.tv_usec -= time1.tv_usec;
    if (time2.tv_usec < 0) {
	time2.tv_usec += 10000;
	--time2.tv_sec;
    }
    time2.tv_sec -= time1.tv_sec;    
    printf("timing: %ld.%ld\n", (long)time2.tv_sec, (long)time2.tv_usec);

    return bool;
}


static int
hash_cmp(void *foo, void *bar)
{
    return strcmp((char *) foo, (char *)bar);
}

static unsigned
hash_hash(void *foo)
{
    return hashcaseadd((char *) foo);
}

static Bool
hash_print(void *foo, void *bar)
{
    printf("%s\n", (char *) foo);
    return FALSE;
}

static int
test_hash(void)
{
    Hashtab *h;

    starttesting("hashtab");

    h = hashtabnew(100, hash_cmp, hash_hash);
    if (!h)
	return endtesting(1);

    if (!hashtabadd(h, "one")||
	!hashtabadd(h, "two")||
	!hashtabadd(h, "three")||
	!hashtabadd(h, "four"))
	return endtesting(1);

    printf("foreach ----\n");
    hashtabforeach(h, hash_print, NULL);

    printf("search ----\none == %s\ntwo == %s\nthree == %s\nfour == %s\n", 
	   (char *)hashtabsearch(h, "one"),
	   (char *)hashtabsearch(h, "two"),
	   (char *)hashtabsearch(h, "three"),
	   (char *)hashtabsearch(h, "four"));

    hashtabrelease(h);

    return endtesting(0);
}

struct units u1_units[] = {
    { "all",		0xff },
    { "u1-hack",	0x04 },
    { "warning", 	0x02 },
    { "debug",		0x01 },
    { NULL, 		0 }
};

struct units u2_units[] = {
    { "all",		0xff },
    { "u2-hack2",	0x08 },
    { "u2-hack1",	0x04 },
    { "warning", 	0x02 },
    { "debug",		0x01 },
    { NULL, 		0 }
};

static int
test_log (void)
{
    Log_method *m;
    Log_unit *u1, *u2;
    char buf[1024];

    starttesting ("log");

    m = log_open ("util-tester", "/dev/stderr:notime");
    if (m == NULL)
	return endtesting(1);

    u1 = log_unit_init (m, "u1", u1_units, 0x3);
    if (u1 == NULL)
	return endtesting(1);

    u2 = log_unit_init (m, "u2", u2_units, 0x0);
    if (u2 == NULL)
	return endtesting(1);

    log_mask2str (m, NULL, buf, sizeof(buf));
    printf ("%s\n", buf); fflush (stdout);
    log_set_mask_str (m, NULL, buf);
    log_log (u1, 0x1, "1.  this should show");
    log_log (u2, 0x1, "X.  this should NOT show");

    log_set_mask_str (m, NULL, "u1:-debug;u2:+debug");
    log_log (u1, 0x1, "X.  now this should NOT show");
    log_log (u2, 0x1, "2.  now this should show");
    log_mask2str (m, NULL, buf, sizeof(buf));
    printf ("%s\n", buf); fflush (stdout);
    log_set_mask_str (m, NULL, buf);

    log_set_mask_str (m, NULL, "u1:-debug;u2:-debug");
    log_log (u1, 0x1, "X.  now this should NOT show");
    log_log (u2, 0x1, "X.  now this should NOT show");
    log_mask2str (m, NULL, buf, sizeof(buf));
    printf ("%s\n", buf); fflush (stdout);
    log_set_mask_str (m, NULL, buf);

    log_set_mask_str (m, NULL, "+debug");
    log_log (u1, 0x1, "3.  now this should show");
    log_log (u2, 0x1, "4.  now this should show");
    log_mask2str (m, NULL, buf, sizeof(buf));
    printf ("%s\n", buf); fflush (stdout);
    log_set_mask_str (m, NULL, buf);

    log_set_mask_str (m, NULL, "-debug");
    log_log (u1, 0x1, "X.  now this should NOT show");
    log_log (u2, 0x1, "X.  now this should NOT show");
    log_mask2str (m, NULL, buf, sizeof(buf));
    printf ("%s\n", buf); fflush (stdout);
    log_set_mask_str (m, NULL, buf);

    log_set_mask_str (m, NULL, "+debug,+warning");
    log_log (u1, 0x1, "5.  now this should show");
    log_log (u2, 0x1, "6.  now this should show");
    log_log (u1, 0x2, "7.  now this should show");
    log_log (u2, 0x2, "8.  now this should show");
    log_mask2str (m, NULL, buf, sizeof(buf));
    printf ("%s\n", buf); fflush (stdout);
    log_set_mask_str (m, NULL, buf);

    log_set_mask_str (m, u1, "-debug,-warning");
    log_log (u1, 0x1, "X. now this should NOT show");
    log_log (u2, 0x1, "9. now this should show");
    log_log (u1, 0x2, "X. now this should NOT show");
    log_log (u2, 0x2, "10. now this should show");

    log_mask2str (m, NULL, buf, sizeof(buf));
    printf ("%s\n", buf); fflush (stdout);
    log_set_mask_str (m, NULL, buf);

    log_set_mask (u1, 0x4 + 0x2 + 0x1);
    log_set_mask (u2, 0x8 + 0x4 + 0x2 + 0x1);

    log_mask2str (m, NULL, buf, sizeof(buf));
    printf ("%s\n", buf); fflush (stdout);
    log_set_mask_str (m, NULL, buf);

    log_set_mask_str (m, NULL, "all");
    log_mask2str (m, NULL, buf, sizeof(buf));
    printf ("all: %s\n", buf); fflush (stdout);

    log_set_mask_str (m, NULL, "-all");
    log_mask2str (m, NULL, buf, sizeof(buf));
    printf ("none: %s\n", buf); fflush (stdout);


    log_close (m);
    return endtesting (0);
}

static int
test_math (void)
{
    starttesting ("math");

    if (arlautil_findprime(17) != 17)
	return endtesting (1);
    if (arlautil_findprime(18) != 19)
	return endtesting (1);
    if (arlautil_findprime(11412) != 11423)
	return endtesting (1);

    if (arlautil_findprime(11412) != 11423)
	return endtesting (1);

    if (arlautil_isprime(20897) == 0)
	return endtesting (1);
    if (arlautil_isprime(49037) == 0)
	return endtesting (1);

    return endtesting (0);
}

int 
main(int argc, char **argv)
{
    int ret = 0;
    ret |= test_hash();
    ret |= test_log();
    ret |= test_math();
    return ret;
}

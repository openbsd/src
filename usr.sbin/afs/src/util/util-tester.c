/*	$OpenBSD: util-tester.c,v 1.1.1.1 1998/09/14 21:53:26 art Exp $	*/
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
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

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "bool.h"
#include "timeprio.h"
#include "hash.h"

struct timeval time1, time2;

static void
starttesting(char *msg)
{
    printf("--------------------------------------------\n");
    printf("testing %s...\n", msg);
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
    printf("timing: %ld.%ld\n",time2.tv_sec, time2.tv_usec);

    return bool;
}


int
test_timeprio(void)
{
    Timeprio *tp = timeprionew(100);

    starttesting("timeprio");

    timeprioinsert(tp, 10, "ten");
    timeprioinsert(tp, 40, "fourty");
    timeprioinsert(tp, 30, "thirty");


    while(!timeprioemptyp(tp)) {
	printf("timepriohead(tp) = %s\n", (char *) timepriohead(tp));
	timeprioremove(tp);
    }

    timepriofree(tp);

    endtesting(0);

    return 0;
}

int
hash_cmp(void *foo, void *bar)
{
    return strcmp((char *) foo, (char *)bar);
}

unsigned
hash_hash(void *foo)
{
    return hashcaseadd((char *) foo);
}

Bool
hash_print(void *foo, void *bar)
{
    printf("%s\n", (char *) foo);
    return FALSE;
}

int
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

    
    printf("XXX there is no simple way to free a hashtab\n");
    return endtesting(0);
}


int 
main(int argc, char **argv)
{
    test_timeprio();
    test_hash();
    return 0;
}





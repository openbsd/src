/*
 * Copyright (c) 1998, 1999 Kungliga Tekniska Högskolan
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

RCSID("$arla: heaptest.c,v 1.3 2000/10/03 00:31:03 lha Exp $");

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <err.h>
#include "heap.h"

struct foo {
    int i;
    heap_ptr hptr;
};

static int
cmp(const void *v1, const void *v2)
{
    const struct foo *foo1 = (const struct foo *)v1;
    const struct foo *foo2 = (const struct foo *)v2;

    return foo1->i - foo2->i;
}

static int
testit (unsigned n)
{
    struct foo *foos, *bars;
    Heap *h1, *h2;
    int i;

    foos = malloc (n * sizeof(*foos));
    bars = malloc (n * sizeof(*bars));
    assert (foos != NULL && bars != NULL);
    h1 = heap_new (n, cmp);
    h2 = heap_new (n, cmp);
    assert (h1 != NULL && h2 != NULL);
    for (i = 0; i < n; ++i) {
	foos[i].i = bars[i].i = rand();
	heap_insert (h1, (void *)&foos[i], NULL);
	heap_insert (h2, (void *)&foos[i], &foos[i].hptr);
	if (!heap_verify(h1) || !heap_verify(h2))
	    abort ();
    }
    for (i = 0; i < n; ++i) {
	heap_remove (h2, foos[i].hptr);
	if (!heap_verify(h2))
	    abort ();
    }
    qsort (bars, n, sizeof(*bars), cmp);
    for (i = 0; i < n; ++i) {
	struct foo *f = (struct foo *)heap_head (h1);

	if (bars[i].i != f->i)
	    abort ();
	heap_remove_head (h1);
	if (!heap_verify(h1))
	    abort ();
    }
    heap_delete (h1);
    heap_delete (h2);
    free (foos);
    free (bars);
    return 0;
}

int
main(int argc, char **argv)
{
    int i, n;

    if (argc != 2)
	errx (1, "argc != 2");

    n = atoi (argv[1]);
    for (i = 0; i < n; ++i)
	testit (rand () % 1000);
    return 0;
}

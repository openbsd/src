/*	$OpenBSD: prio.c,v 1.1.1.1 1998/09/14 21:53:24 art Exp $	*/
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

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: prio.c,v 1.1 1998/07/07 15:57:10 lha Exp $");
#endif

#include <stdlib.h>
#include "bool.h"
#include "prio.h"

#define PRIO_PARENT(x)	(((x) + 1)/ 2 -1 )
#define PRIO_LEFT(x)	(2 * (i) + 1)
#define PRIO_RIGHT(x)	(2 * (i) + 2)


Prio *
prionew(unsigned size, prio_cmp cmp)
{
    Prio *prio;

    if (!size || !cmp)
	return NULL;

    prio = calloc(sizeof(Prio), 1);
    if (!prio)
	return prio;

    prio->heap = calloc(sizeof(Prio), size);
    if (!prio->heap)
	free(prio);

    prio->cmp = cmp;
    prio->sz = size;

    return prio;
}

void
priofree(Prio *prio)
{
    if (!prio)
	return;

    free(prio);
}

static void
heapify(Prio *prio, unsigned i)
{
    unsigned j;
    void *el;

    if (!prio || i > prio->sz)
	return;

    el = &prio->heap[0];
    while (prio->size && i <= PRIO_PARENT(prio->size)) {
	j = PRIO_LEFT(i);
	if (j < prio->size) {
	    if (prio->cmp(prio->heap[i], prio->heap[j]) < 0) 
		j++;
	    
	    if (prio->cmp(prio->heap[0], prio->heap[j]) < 0)
		break;
	
	    prio->heap[i] = prio->heap[j];
	}
	i = j;
    }
    prio->heap[i] = el;
}



int
prioinsert(Prio *prio, void *data)
{
    void **ptr;
    unsigned i;

    if (!prio || !data)
	return -1;


    if (prio->sz == prio->size) {
	ptr = realloc(prio->heap, prio->sz *2);
	if (!ptr)
	    return -1;

	prio->heap = ptr;
    }

    i = prio->size++;

    while (i > 0 &&
	   prio->cmp(data,prio->heap[PRIO_PARENT(i)]) < 0)
    {
	prio->heap[i] = prio->heap[PRIO_PARENT(i)];
        i = PRIO_PARENT(i);
    }
    prio->heap[i] = data;
    return 0;
}

void *
priohead(Prio *prio)
{
    if (!prio)
	return NULL;
    
    return prio->heap[0];
}

void
prioremove(Prio *prio)
{
    if (!prio)
	return;

    if (prioemptyp (prio)) /* underflow */
	return;

    prio->heap[0] = prio->heap[--prio->size];
    heapify (prio, prio->size);
}
    
Bool 
prioemptyp(Prio *prio)
{
    if (!prio || prio->size == 0)
	return TRUE;

    return FALSE;
}


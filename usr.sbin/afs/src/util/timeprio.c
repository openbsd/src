/*	$OpenBSD: timeprio.c,v 1.1.1.1 1998/09/14 21:53:24 art Exp $	*/
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
RCSID("$KTH: timeprio.c,v 1.1 1998/07/07 15:57:11 lha Exp $");
#endif

#include <stdlib.h>
#include "bool.h"
#include "timeprio.h"


static int 
timeprio_cmp(void *a1, void *b1)
{
    Tpel *a = a1, *b = b1;

    return a->time > b->time;
}


Timeprio *
timeprionew(unsigned size)
{
    return (Timeprio *) prionew(size, timeprio_cmp);
}

void
timepriofree(Timeprio *prio)
{
    priofree(prio);
}

int
timeprioinsert(Timeprio *prio, time_t time, void *data)
{
    Tpel *el = malloc(sizeof(Tpel));
    if (!el)
	return -1;

    el->time = time;
    el->data = data;
    if (prioinsert(prio, el)) {
	free(el);
	el = NULL;
    }
    return el ? 0 : -1;
}

void *
timepriohead(Timeprio *prio)
{
    Tpel *el = priohead(prio);
    
    return el->data;
}

void
timeprioremove(Timeprio *prio)
{
    void *el;

    if (timeprioemptyp((Prio *)prio))
	return;

    el = priohead(prio);
    if (el) free (el);
    
    prioremove((Prio *)prio);
}
    
Bool 
timeprioemptyp(Timeprio *prio)
{
    return prioemptyp(prio); 
}


/*	$OpenBSD: freelist.c,v 1.1.1.1 1998/09/14 21:53:22 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997 Kungliga Tekniska Högskolan
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

/*
 * Function for handling freelist of different stuff.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$KTH: freelist.c,v 1.3 1998/02/22 11:22:15 assar Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "freelist.h"

/*
 * Create a freelist, that is a block of data stuff and a list of the
 * free ones.
 */

Freelist *
freelistnew (size_t n, size_t sz)
{
     Freelist *f;
     int i;

     f = (Freelist *)malloc (sizeof (Freelist));
     if (f == NULL)
	  return NULL;
     f->list = listnew ();
     f->data = malloc (n * sz);
     for (i = 0; i < n; ++i)
	  listaddhead (f->list, (char *)f->data + i * sz);
     return f;
}

/*
 * Allocate an element from a freelist.
 * Right now just fail if there is no place, otherwise we might want
 * to allocate some more data.
 */

void *
freelistalloc (Freelist *f)
{
     return listdelhead (f->list);
}

/*
 * Free an element and return it to the freelist.
 */

void
freelistfree (Freelist *f, void *q)
{
     listaddhead (f->list, q);
}

/*
 * Delete an freelist. All elements are deleted and there should be no
 * references left to it.
 */

void
freelistdel (Freelist *f)
{
     free (f->data);
     free (f->list);
}

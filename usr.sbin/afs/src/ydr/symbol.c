/*	$OpenBSD: symbol.c,v 1.1.1.1 1998/09/14 21:53:27 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
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
RCSID("$KTH: symbol.c,v 1.4 1998/02/19 05:16:28 assar Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mem.h>
#include "sym.h"
#include <hash.h>

static Hashtab *hashtab;

static int
symcmp (void *a, void *b)
{
     Symbol *sa = (Symbol *)a;
     Symbol *sb = (Symbol *)b;
     
     return strcmp (sa->name, sb->name);
}

static unsigned
symhash (void *a)
{
     Symbol *sa = (Symbol *)a;

     return hashadd (sa->name);
}

#define HASHTABSIZE 149

void
initsym (void)
{
     hashtab = hashtabnew (HASHTABSIZE, symcmp, symhash);
}

Symbol *
addsym (char *name)
{
     Symbol tmp;
     Symbol *sym;

     tmp.name = name;
     sym = (Symbol *)hashtabsearch (hashtab, (void *)&tmp);
     if (sym == NULL) {
	  sym = (Symbol *)emalloc (sizeof (Symbol));
	  sym->name = name;
	  sym->type = TUNDEFINED;
	  hashtabadd (hashtab, sym);
     }
     return sym;
}

Symbol *
findsym (char *name)
{
     Symbol tmp;

     tmp.name = name;
     return (Symbol *)hashtabsearch (hashtab, (void *)&tmp);
}

#ifdef notyet
static Bool
printsymbol (void *ptr, void *arg)
{
     Symbol *s = (Symbol *)ptr;

     switch (s->type) {
	  case TUNDEFINED :
	       printf ("undefined ");
	       break;
	  case TSTRUCT :
	       printf ("struct ");
	       break;
	  case TENUM :
	       printf ("enum ");
	       break;
	  case TENUMVAL :
	       printf ("enumval ");
	       break;
	  case TCONST :
	       printf ("const ");
	       break;
	  case TTYPEDEF :
	       printf ("typedef ");
	       break;
	  default :
	       abort ();
     }
     puts (s->name);

     return FALSE;
}
#endif

void
symiterate (Bool (*func)(void *, void *), void *arg)
{
     hashtabforeach (hashtab, func, arg);
}

/*
 * Copyright (c) 1995, 1996, 1997, 1998, 1999 Kungliga Tekniska Högskolan
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
RCSID("$arla: symbol.c,v 1.10 2002/04/15 14:53:19 lha Exp $");
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sym.h"
#include <hash.h>
#include <roken.h>

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
	  sym->type = YDR_TUNDEFINED;
	  sym->attrs = NULL;
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

static Bool __attribute__ ((unused))
printsymbol (void *ptr, void *arg)
{
     Symbol *s = (Symbol *)ptr;

     switch (s->type) {
	  case YDR_TUNDEFINED :
	       printf ("undefined ");
	       break;
	  case YDR_TSTRUCT :
	       printf ("struct ");
	       break;
	  case YDR_TENUM :
	       printf ("enum ");
	       break;
	  case YDR_TENUMVAL :
	       printf ("enumval ");
	       break;
	  case YDR_TCONST :
	       printf ("const ");
	       break;
	  case YDR_TTYPEDEF :
	       printf ("typedef ");
	       break;
	  default :
	       abort ();
     }
     puts (s->name);

     return FALSE;
}

void
symiterate (Bool (*func)(void *, void *), void *arg)
{
     hashtabforeach (hashtab, func, arg);
}

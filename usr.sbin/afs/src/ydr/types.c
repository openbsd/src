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
RCSID("$arla: types.c,v 1.10 2002/04/15 14:53:19 lha Exp $");
#endif

#include <stdio.h>
#include <roken.h>
#include "types.h"
#include "lex.h"

Symbol *
define_const (char *name, int value)
{
     Symbol *s;

     s = addsym (name);

     if (s->type != YDR_TUNDEFINED) {
	  error_message (1, "Redeclaration of %s\n", s->name);
	  return NULL;
     }
     s->type = YDR_TCONST;
     s->u.val = value;
     return s;
}

Symbol *
define_enum (char *name, List *list)
{
     Symbol *s;

     s = addsym (name);

     if (s->type != YDR_TUNDEFINED) {
	  error_message (1, "Redeclaration of %s\n", s->name);
	  return NULL;
     }
     s->type = YDR_TENUM;
     s->u.list = list;
     return s;
}

Symbol *
define_struct (char *name)
{
     Symbol *s;

     s = addsym (name);

     if (s->type != YDR_TSTRUCT && s->type != YDR_TUNDEFINED) {
	  error_message (1, "Redeclaration of %s as a different type\n",
			 s->name);
	  return NULL;
     }
     s->type = YDR_TSTRUCT;
     s->u.list = NULL;
     return s;
}

Symbol *
set_struct_body_sym (Symbol *s, List *list)
{
    s->u.list = list;
    return s;
}

Symbol *
set_struct_body (char *name, List *list)
{
    Symbol *s;

    s = findsym(name);
    if (s == NULL) {
	error_message(1, "struct %s not declared", name);
	return NULL;
    }
    return set_struct_body_sym (s, list);
}

Symbol *
define_typedef (StructEntry *entry)
{
     Symbol *s;

     s = addsym (entry->name);

     if (s->type != YDR_TUNDEFINED) {
	  error_message (1, "Redeclaration of %s\n", s->name);
	  return NULL;
     }
     s->type = YDR_TTYPEDEF;
     s->name = entry->name;
     s->u.type = entry->type;
     free (entry);
     return s;
}


Symbol *
define_proc (char *name, List *args, unsigned id)
{
     Symbol *s;

     s = addsym (name);

     if (s->type != YDR_TUNDEFINED) {
	  error_message (1, "Redeclaration of %s\n", s->name);
	  return NULL;
     }
     s->type = YDR_TPROC;
     s->u.proc.id = id;
     s->u.proc.arguments = args;
     return s;
}

Symbol *
createenumentry (char *name, int value)
{
     Symbol *s;

     s = addsym (name);

     if (s->type != YDR_TUNDEFINED) {
	  error_message (1,"Redeclaration of %s\n", s->name);
	  return NULL;
     }
     s->type = YDR_TENUMVAL;
     s->u.val = value;
     return s;
}

StructEntry *
createstructentry (char *name, Type *type)
{
     StructEntry *e;

     e = (StructEntry *)emalloc (sizeof (StructEntry));
     e->name = name;
     e->type = type;
     return e;
}

struct Type *
create_type (TypeType type, Symbol *symbol, unsigned size,
	     Type *subtype, Type *indextype, int flags)
{
    Type *t;

    t = emalloc(sizeof(*t));

    t->type = type;
    t->symbol = symbol;
    t->size = size;
    t->subtype = subtype;
    t->indextype = indextype;
    t->flags = flags;

    return t;
}

void
set_sym_attrs(Symbol *symbol, List *attrs)
{
    switch (symbol->type) {
    case YDR_TSTRUCT:
	break;
    default:
	error_message(1, "can't set attribues for %s", symbol->name);
	return;
    }
    symbol->attrs = attrs;
}

struct fa {
    char *attr;
    int found;
};

static int
find_attr(List *l, Listitem *li, void *ptr)
{
    struct fa *f = ptr;
    char *mem = listdata(li);
    if (strcmp(f->attr, mem) == 0) {
	f->found = 1;
	return 1;
    }
    return 0;
}

int
sym_find_attr(Symbol *symbol, char *attr)
{
    struct fa f;

    if (symbol->attrs == NULL)
	return 0;

    f.attr = attr;
    f.found = 0;

    listiter(symbol->attrs, find_attr, &f);

    return f.found;
}

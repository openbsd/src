/*
 * Copyright (c) 1995, 1996, 1997, 2002 Kungliga Tekniska Högskolan
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

/*
 * list handling functions
 */

/* $arla: list.h,v 1.13 2003/01/10 12:38:43 lha Exp $ */

#ifndef _ARLAUTIL_LIST_H
#define _ARLAUTIL_LIST_H 1

#include "bool.h"
#include <roken.h>

struct listitem {
     void *data;
     struct listitem *prev, *next;
};

typedef struct listitem Listitem;

struct list {
     Listitem *head, *tail;
};

typedef struct list List;

/*
 * functions
 */

List *listnew (void);

void listfree(List *);

Listitem *listaddhead (List *list, void *data);

Listitem *listaddtail (List *list, void *data);

void listdel (List *list, Listitem *item);

Listitem *listaddbefore (List *list, Listitem *old_item, void *data);

Listitem *listaddafter (List *list, Listitem *old_item, void *data);

void *listdelhead (List *list);

void *listdeltail (List *list);

void listiter (List *list, Bool (*fn)(List *, Listitem *, void *arg),
	       void *arg);

/*
 * inline functions
 */

static inline Listitem * __attribute__ ((unused))
listhead (List *list)
{
     return list->head;
}

static inline Listitem * __attribute__ ((unused))
listtail (List *list)
{
    return list->tail;
}

static inline Listitem * __attribute__ ((unused))
listprev (List *list, Listitem *item)
{
    return item->prev;
}

static inline Listitem * __attribute__ ((unused))
listnext (List *list, Listitem *item)
{
     return item->next;
}

static inline void * __attribute__ ((unused))
listdata (Listitem *item)
{
     return item->data;
}

static inline Bool __attribute__ ((unused))
listemptyp (List *list)
{
     return (Bool)(list->head == NULL);
}

static inline Bool __attribute__ ((unused))
listnextp(Listitem *item)
{
    return (Bool)(item->next != NULL);
}

#endif /* _ARLAUTIL_LIST_H */

/*	$OpenBSD: list.h,v 1.1.1.1 1998/09/14 21:53:23 art Exp $	*/
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
 * list handling functions
 */

/* $KTH: list.h,v 1.4 1998/07/05 18:25:00 assar Exp $ */

#ifndef _LIST_
#define _LIST_

#include "bool.h"

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

Listitem *listaddhead (List *list, void *data);

Listitem *listaddtail (List *list, void *data);

void listdel (List *list, Listitem *item);

Listitem *listaddbefore (List *list, Listitem *old_item, void *data);

Listitem *listaddafter (List *list, Listitem *old_item, void *data);

void *listdelhead (List *list);

void *listdeltail (List *list);

Bool listemptyp (List *list);

Listitem *listhead (List *list);

Listitem *listtail (List *list);

Listitem *listprev (List *list, Listitem *item);

Listitem *listnext (List *list, Listitem *item);

void *listdata (Listitem *item);

void listiter (List *list, Bool (*fn)(List *, Listitem *, void *arg),
	       void *arg);

#endif /* _LIST_ */

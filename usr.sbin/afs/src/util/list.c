/*
 * Copyright (c) 1995, 1996, 1997, 1998, 2002 Kungliga Tekniska Högskolan
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
 * List handling functions
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
RCSID("$arla: list.c,v 1.12 2002/04/20 17:06:21 lha Exp $");
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "list.h"

/*
 * The representation is with a double-linked list, a pointer to
 * the tail, and another one to the head.
 */

/*
 * Create a new list.
 */

List*
listnew (void)
{
     List *tmp = (List *)malloc (sizeof (List));

     if (tmp)
	  tmp->head = tmp->tail = NULL;
     return tmp;
}

/*
 * Free a list, assume that its empty
 */

void
listfree(List *list)
{
	list->head = list->tail = NULL;
	free(list);
} 

/*
 * Add an element before `item'
 */

Listitem *
listaddbefore (List *list, Listitem *old_item, void *data)
{
     Listitem *item = (Listitem *)malloc (sizeof (Listitem));

     if (item == NULL)
	 return item;

     item->data = data;
     item->prev = old_item->prev;
     item->next = old_item;
     if (item->prev)
	 item->prev->next = item;
     else
	 list->head = item;
     old_item->prev = item;
     return item;
}

/*
 * Add an element after `item'
 */

Listitem *
listaddafter (List *list, Listitem *old_item, void *data)
{
     Listitem *item = (Listitem *)malloc (sizeof (Listitem));

     if (item == NULL)
	 return item;

     item->data = data;
     item->next = old_item->next;
     item->prev = old_item;
     if (item->next)
	 item->next->prev = item;
     else
	 list->tail = item;
     old_item->next = item;
     return item;
}

/*
 * Add an element to the head of the list
 */

Listitem *
listaddhead (List *list, void *data)
{
     Listitem *item = (Listitem *)malloc (sizeof (Listitem));

     if (item == NULL)
	 return item;

     item->data = data;
     item->prev = NULL;
     item->next = list->head;
     if (list->head)
	  list->head->prev = item;
     list->head = item;
     if (list->tail == NULL)
	  list->tail = item;
     return item;
}

/*
 * Add an element to the tail of the list
 */

Listitem *
listaddtail (List *list, void *data)
{
     Listitem *item = (Listitem *)malloc (sizeof (Listitem));

     if (item == NULL)
	 return item;

     item->data = data;
     item->next = NULL;
     item->prev = list->tail;
     if (list->tail)
	  list->tail->next = item;
     list->tail = item;
     if (list->head == NULL)
	  list->head = item;
     return item;
}

/*
 * Remove an element from the head of the list.
 * Return this element.
 */

void *
listdelhead (List *list)
{
     Listitem	*item = list->head;
     void	*ret;

     if (item == NULL)
	 return NULL;
     ret = item->data;

     list->head = list->head->next;
     if (list->head)
	  list->head->prev = NULL;
     free(item);
     if (list->tail == item)
	  list->tail = NULL;
     return ret;
}

/*
 * Remove an element from the tail of the list.
 * Return this element.
 */

void *
listdeltail (List *list)
{
     Listitem	*item = list->tail;
     void	*ret;

     if (item == NULL)
	 return NULL;
     ret = item->data;

     list->tail = list->tail->prev;
     if (list->tail)
	  list->tail->next = NULL;
     free (item);
     if (list->head == item)
	  list->head = NULL;
     return ret;
}

/*
 * listdel
 */

void
listdel (List *list, Listitem *item)
{
     if (item->prev)
	  item->prev->next = item->next;
     if (item->next)
	  item->next->prev = item->prev;
     if (item == list->head)
	  list->head = item->next;
     if (item == list->tail)
	  list->tail = item->prev;
     free (item);
}

/*
 * Iterate through all the items in a list.
 */

void listiter (List *list, Bool (*fn)(List *, Listitem *, void *arg),
	       void *arg)
{
     Listitem *item;

     for (item = list->head; item; item = item->next)
	  if ((*fn) (list, item, arg))
	       break;
}

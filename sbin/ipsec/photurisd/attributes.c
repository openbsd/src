/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * attributes.c:
 * functions for handling attributess
 */

#ifndef lint
static char rcsid[] = "$Id: attributes.c,v 1.2 1997/07/19 12:07:40 provos Exp $";
#endif

#define _ATTRIBUTES_C_

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "config.h"
#include "attributes.h"

static struct attribute_list *attribob = NULL;

int
isinattrib(u_int8_t *attributes, u_int16_t attribsize, u_int8_t attribute)
{
     while(attribsize>0) {
	  if(*attributes==attribute)
	       return 1;
	  if(attribsize - (*(attributes+1)+2) > attribsize) 
	       return 0;

	  attribsize -= *(attributes+1)+2;
	  attributes += *(attributes+1)+2;
     }
     return 0;
}

int 
isattribsubset(u_int8_t *set, u_int16_t setsize, 
	       u_int8_t *subset, u_int16_t subsetsize)
{
     while(subsetsize>0) {
	  if (!isinattrib(set, setsize, *subset))
	       return 0;
	  if (subsetsize - (*(subset+1)+2) > subsetsize)
	       return 0;
	  subsetsize -= *(subset+1)+2;
	  subset += *(subset+1)+2;
     }
     return 1;
}

int
attrib_insert(struct attribute_list *ob)
{
     struct attribute_list *tmp;

     ob->next = NULL;

     if(attribob == NULL) {
	  attribob = ob;
	  return 1;
     }
     
     tmp=attribob;
     while(tmp->next!=NULL)
	  tmp = tmp->next;

     tmp->next = ob;
     return 1;
}

int
attrib_unlink(struct attribute_list *ob)
{
     struct attribute_list *tmp;
     if(attribob == ob) {
	  attribob = ob->next;
	  free(ob);
	  return 1;
     }

     for(tmp=attribob; tmp!=NULL; tmp=tmp->next) {
	  if(tmp->next==ob) {
	       tmp->next=ob->next;
	       free(ob);
	       return 1;
	  }
     }
     return 0;
}

struct attribute_list *
attrib_new(void)
{
     struct attribute_list *p;

     if((p = calloc(1, sizeof(struct attribute_list)))==NULL)
	  return NULL;

     return p;
}

int
attrib_value_reset(struct attribute_list *ob)
{ 
     if (ob->address != NULL)
	  free(ob->address);
     if (ob->attributes != NULL)
	  free(ob->attributes);

     bzero(ob, sizeof(struct attribute_list));
     return 1;
}

/* 
 * find the attributes to the address or 0 address.
 * if passed a null pointer as first argument we return our default
 * list.
 */

struct attribute_list *
attrib_find(char *address)
{
     struct attribute_list *tmp = attribob;
     struct attribute_list *null = NULL;
     while(tmp!=NULL) {
	  if (tmp->address == NULL) {
	       null = tmp;
	       if (address == NULL)
		    break;
	  }
	  else if (address != NULL &&
		   (tmp->netmask & inet_addr(address)) ==
		   inet_addr(tmp->address))
	       return tmp;

	  tmp = tmp->next;
     }
     return null;
}

void
attrib_cleanup()
{
     struct attribute_list *p;
     struct attribute_list *tmp = attribob;
     while(tmp!=NULL) {
	  p = tmp;
	  tmp = tmp->next;
	  attrib_value_reset(p);
	  free(p);
     }
     attribob = NULL;
}


/*	$OpenBSD: schedule.c,v 1.7 2001/09/19 10:58:07 mpech Exp $	*/

/*
 * Copyright 1997-2000 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 * 
 * Parts derived from code by Angelos D. Keromytis, kermit@forthnet.gr 
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
 * schedule.c:
 * SCHEDULE handling functions
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: schedule.c,v 1.7 2001/09/19 10:58:07 mpech Exp $";
#endif

#define _SCHEDULE_C_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <sys/time.h>
#include <arpa/inet.h> 
#include "state.h"
#include "spi.h"
#include "photuris.h"
#include "buffer.h"
#include "schedule.h"
#include "log.h"
#include "cookie.h"
#include "modulus.h"
#include "api.h"
#ifdef IPSEC
#include "attributes.h"
#include "kernel.h"
#endif
#ifdef DEBUG
#include "config.h"
#endif

static struct schedule *schedob = NULL;

void
schedule_insert(int type, int off, u_int8_t *cookie, u_int16_t cookie_size)
{
     struct schedule *tmp;

#ifdef DEBUG
     {
	  if( cookie != NULL) {
	       int i = BUFFER_SIZE;
	       bin2hex(buffer, &i, cookie, cookie_size);
	  }
	  printf("Adding event type %d, due in %d seconds, cookie %s\n",
		 type, off, cookie == NULL ? "None" : (char *)buffer);
     }
#endif
     
     if ((tmp = calloc(1, sizeof(struct schedule))) == NULL) {
	  log_error("calloc() in schedule_insert()");
	  return;
     }

     tmp->event = type;
     tmp->offset = off;
     tmp->tm = time(NULL) + off;

     if (cookie != NULL) {
	  tmp->cookie = calloc(cookie_size, sizeof(u_int8_t));
	  if (tmp->cookie == NULL) {
	       log_error("calloc() in schedule_insert()");
	       free(tmp);
	       return;
	  }
	  bcopy(cookie, tmp->cookie, cookie_size);
	  tmp->cookie_size = cookie_size;
     }

     tmp->next = NULL;

     if (schedob == NULL)
	  schedob = tmp;
     else {
	  tmp->next = schedob;
	  schedob = tmp;
     }
}

int
schedule_next(void)
{
     struct schedule *tmp;
     time_t tm;

     if (schedob == NULL)
	  return -1;

     tm = schedob->tm;
     tmp = schedob->next;
     while (tmp != NULL) {
	  if (tmp->tm < tm)
	       tm = tmp->tm;
	  tmp = tmp->next;
     }

     if ((tm -= time(NULL)) < 0)
	  return 0;

     return((int) tm);
}

int
schedule_offset(int type, u_int8_t *cookie)
{
     struct schedule *tmp = schedob;
     while (tmp != NULL) { 
          if (tmp->event == type &&  
              ((tmp->cookie == NULL && cookie == NULL) ||  
               !bcmp(tmp->cookie, cookie, tmp->cookie_size)))
	       return tmp->offset;
	  tmp = tmp->next;
     }

     return -1;
}

void
schedule_remove(int type, u_int8_t *cookie)
{
     struct schedule *tmp, *otmp = NULL;

     tmp = schedob;
     while (tmp != NULL) {
	  if (tmp->event == type && 
	      ((tmp->cookie == NULL && cookie == NULL) || 
	       !bcmp(tmp->cookie, cookie, tmp->cookie_size))) {
	       if (tmp == schedob)
		    schedob = tmp->next;
	       else
		    otmp->next = tmp->next;

	       if (tmp->cookie != NULL)
		    free(tmp->cookie);
	       free(tmp);
	       return;
	  }
	  otmp = tmp;
	  tmp = tmp->next;
     }
}

void
schedule_process(int sock)
{
     struct schedule *tmp, *tmp2;
     struct sockaddr_in sin;
     struct stateob *st;
     time_t tm;
     int remove;

     tm = time(NULL);
     tmp = schedob;
     while (tmp != NULL) {
	  if (tmp->tm > tm) {
	       tmp = tmp->next;
	       continue;
	  }

	  remove = 0;
	  switch(tmp->event) {
	  case REKEY:
	       reset_secret();
	       tmp->tm = time(NULL) + REKEY_TIMEOUT;
	       break;
          case MODULUS: 
#ifdef DEBUG2
	       printf("Checking moduli\n"); 
#endif 
	       mod_check_prime(MOD_PRIME_ITER, MOD_PRIME_TIME); 
	       tmp->tm = time(NULL) + MODULUS_TIMEOUT; 
	       break; 
	  case CLEANUP:
#ifdef DEBUG2
	       printf("Cleaning up states\n");
#endif
	       state_expire();
#ifdef DEBUG2
	       printf("Cleaning up SPI's\n");
#endif
	       spi_expire();
	       tmp->tm = time(NULL) + CLEANUP_TIMEOUT;
	       break;
	  case TIMEOUT:
	       st = state_find_cookies(NULL, tmp->cookie, NULL);
	       if (st == NULL) {
		    remove = 1;
		    break;
	       } else if (st->retries >= max_retries) {
		    remove = 1;
		    if (st->phase == COOKIE_REQUEST && st->resource == 0) {
			 log_print("no anwser for cookie request to %s:%d",
				   st->address, st->port);
#ifdef IPSEC
			 if (st->flags & IPSEC_NOTIFY)
			      kernel_notify_result(st, NULL, 0);
#endif
			 break;
		    } else if(st->phase == COOKIE_REQUEST) {
			 /* Try again with updated counters */
			 struct stateob *newst;
			 if ((newst = state_new()) == NULL) {
			      log_error("state_new() in schedule_process()");
			      break;
			 }
			 state_copy_flags(st, newst);
#ifdef DEBUG
			 printf("Starting a new exchange to %s:%d with updated rcookie and"
				" counter.\n", newst->address, newst->port);
#endif /* DEBUG */
			 start_exchange(sock, newst, st->address, st->port);
			 state_insert(newst);
			 break;
		    } else {
			 log_print("exchange terminated, phase %d to %s:%d",
				   st->phase, st->address, st->port);
			 break;
		    }
	       }

	       
	       if (st->packet == NULL || st->packetlen == 0) {
		    log_print("no packet in schedule_process()");
		    remove = 1;
		    break;
	       }

	       /* Only send the packet when no error occurred */
	       if (!remove) {
		    st->retries++;

		    sin.sin_port = htons(st->port); 
		    sin.sin_family = AF_INET; 
		    sin.sin_addr.s_addr = inet_addr(st->address);
		    
		    if (sendto(sock, st->packet, st->packetlen, 0,
			       (struct sockaddr *) &sin, sizeof(sin)) 
			!= st->packetlen) {
			 log_error("sendto() in schedule_process()");
			 remove = 1;
			 break;
		    }
		    
#ifdef DEBUG
		    printf("Resending packet to %s type %d, length %d.\n",
			   st->address, st->phase, st->packetlen);
#endif
		    tmp->tm = tm + retrans_timeout;
	       }
	       break;
	  case UPDATE:
		  spi_update(sock, tmp->cookie);
		  remove = 1;
		  break;
	  default:
	       remove = 1;
	       log_print("Unknown event in schedule_process()");
	       break;
	  }

	  if (remove) {
	       tmp2 = tmp;
	       tmp = tmp->next;
	       schedule_remove(tmp2->event, tmp2->cookie);
	  } else
	       tmp = tmp->next;
     }
}

void
init_schedule(void)
{
     schedule_insert(REKEY, REKEY_TIMEOUT, NULL, 0);
     schedule_insert(CLEANUP, CLEANUP_TIMEOUT, NULL, 0);
     schedule_insert(MODULUS, MODULUS_TIMEOUT, NULL, 0);
}

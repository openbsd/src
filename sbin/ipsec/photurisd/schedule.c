/*
 * Copyright 1997,1998 Niels Provos <provos@physnet.uni-hamburg.de>
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
static char rcsid[] = "$Id: schedule.c,v 1.8 1998/06/30 16:58:36 provos Exp $";
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
#include "secrets.h"
#include "errlog.h"
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
	  log_error(1, "calloc() in schedule_insert()");
	  return;
     }

     tmp->event = type;
     tmp->offset = off;
     tmp->tm = time(NULL) + off;

     if (cookie != NULL) {
	  tmp->cookie = calloc(cookie_size, sizeof(u_int8_t));
	  if (tmp->cookie == NULL) {
	       log_error(1, "calloc() in schedule_insert()");
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
     struct spiob *spi, *nspi;
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
#ifdef DEBUG
	       if (state_root() != NULL)
		    printf("Resetting secrets\n");
#endif
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
			 log_error(0, "no anwser for cookie request to %s:%d",
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
			      log_error(1, "state_new() in schedule_process()");
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
			 log_error(0, "exchange terminated, phase %d to %s:%d",
				   st->phase, st->address, st->port);
			 break;
		    }
	       }

	       
	       if (st->packet == NULL || st->packetlen == 0) {
		    log_error(0, "no packet in schedule_process()");
		    remove = 1;
		    break;
	       }

	       /* Only send the packet when no error occured */
	       if (!remove) {
		    st->retries++;

		    sin.sin_port = htons(st->port); 
		    sin.sin_family = AF_INET; 
		    sin.sin_addr.s_addr = inet_addr(st->address);
		    
		    if (sendto(sock, st->packet, st->packetlen, 0,
			       (struct sockaddr *) &sin, sizeof(sin)) 
			!= st->packetlen) {
			 log_error(1, "sendto() in schedule_process()");
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
#ifdef DEBUG
	  {
	       int i = BUFFER_SIZE;
	       bin2hex(buffer, &i, tmp->cookie, SPI_SIZE);
	       printf("Upating SPI 0x%s\n", buffer);
	  }
#endif
	       remove = 1;
	       /* We are to create a new SPI */
	       if ((spi = spi_find(NULL, tmp->cookie)) == NULL) {
		    log_error(0, "spi_find() in schedule_process()");
		    break;
	       }
	       if ((st = state_find_cookies(spi->address, spi->icookie, NULL)) == NULL) {
#ifdef DEBUG2
		    /* 
		     * This happens always when an exchange expires but
		     * updates are still scheduled for it.
		     */
		    log_error(0, "state_find_cookies() in schedule_process()");
#endif
		    break;
	       }

	       if (st->oSPIattrib != NULL)
		    free(st->oSPIattrib);
	       if ((st->oSPIattrib = calloc(spi->attribsize, sizeof(u_int8_t))) == NULL) {
		    log_error(1, "calloc() in schedule_process()");
		    break;
	       }
	       st->oSPIattribsize = spi->attribsize;
	       bcopy(spi->attributes, st->oSPIattrib, st->oSPIattribsize);

	       /* We can keep our old attributes, this is only an update */
	       if (make_spi(st, spi->local_address, st->oSPI, &(st->olifetime),
			    &(st->oSPIattrib), &(st->oSPIattribsize)) == -1) {
		    log_error(0, "make_spi() in schedule_process()");
		    break;
	       }

	       packet_size = PACKET_BUFFER_SIZE; 
	       if (photuris_spi_update(st, packet_buffer, &packet_size) == -1) {
		    log_error(0, "photuris_spi_update() in schedule_process()");
		    break;
	       }

	       /* Send the packet */
	       sin.sin_port = htons(st->port); 
	       sin.sin_family = AF_INET; 
	       sin.sin_addr.s_addr = inet_addr(st->address);
		    
	       if (sendto(sock, packet_buffer, packet_size, 0,
			  (struct sockaddr *) &sin, sizeof(sin)) != packet_size) {
		    log_error(1, "sendto() in schedule_process()");
		    break;
	       }
	       
#ifdef DEBUG
	       printf("Sending SPI UPDATE to %s.\n", st->address);
#endif
	       /* Insert Owner SPI */
	       if ((nspi = spi_new(st->address, st->oSPI)) == NULL) {
		    log_error(1, "spi_new() in handle_spi_needed()");
		    break;
	       }
	       if ((nspi->local_address = strdup(spi->local_address)) == NULL) {
		    log_error(1, "strdup() in handle_spi_needed()");
		    spi_value_reset(nspi);
		    break;
	       }
	       bcopy(st->icookie, nspi->icookie, COOKIE_SIZE);
	       nspi->flags |= SPI_OWNER;
	       nspi->attribsize = st->oSPIattribsize;
	       nspi->attributes = calloc(nspi->attribsize, sizeof(u_int8_t));
	       if (nspi->attributes == NULL) {
		    log_error(1, "calloc() in handle_spi_needed()");
		    spi_value_reset(nspi);
		    break;
	       }
	       bcopy(st->oSPIattrib, nspi->attributes, nspi->attribsize);
	       nspi->lifetime = time(NULL) + st->olifetime;

	       make_session_keys(st, nspi);

	       spi_insert(nspi);
	       schedule_insert(UPDATE, st->olifetime/2, nspi->SPI, SPI_SIZE);
#ifdef IPSEC
	       kernel_insert_spi(st, nspi);
#endif
	       break;
	  default:
	       remove = 1;
	       log_error(0, "Unknown event in schedule_process()");
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

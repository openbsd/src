/*
 * Copyright 1997,1998 Niels Provos <provos@physnet.uni-hamburg.de>
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
 * spi.c:
 * SPI handling functions
 */

#ifndef lint
static char rcsid[] = "$Id: spi.c,v 1.5 2000/12/14 23:28:59 provos Exp $";
#endif

#define _SPI_C_

#include <sys/types.h>
#include <sys/queue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include "config.h"
#include "photuris.h"
#include "state.h"
#include "attributes.h"
#include "buffer.h"
#include "spi.h"
#include "schedule.h"
#include "log.h"
#ifdef IPSEC
#include "kernel.h"
#endif


TAILQ_HEAD(spilist, spiob) spihead;

void
spi_init(void)
{
	TAILQ_INIT(&spihead);
}

time_t
getspilifetime(struct stateob *st)
{
     /* XXX - destination depend lifetimes */
     return (st->spi_lifetime);
}

int
make_spi(struct stateob *st, char *local_address,
	 u_int8_t *SPI, time_t *lifetime,
	 u_int8_t **attributes, u_int16_t *attribsize)
{
     u_int32_t tmp = 0;
     int i, flags = 0;

     if(*attributes == NULL) {           /* We are in need of attributes */
	  if (select_attrib(st, attributes, attribsize) == -1) {
	       log_print("select_attrib() in make_spi()");
	       return (-1);
	  }
     }
	
#ifdef IPSEC
     /* Let the kernel reserve a SPI for us */
     for (i=0; i<*attribsize; i += (*attributes)[i+1]+2)
	  if ((*attributes)[i] == AT_ESP_ATTRIB)
	       flags |= IPSEC_OPT_ENC;
	  else if ((*attributes)[i] == AT_AH_ATTRIB)
	       flags |= IPSEC_OPT_AUTH;
     
     tmp = kernel_reserve_spi(local_address, st->address, flags);
#else
     /* Just grab a random number, this should be uniq */
     tmp = arc4random();
#endif
     for (i = SPI_SIZE - 1; i >= 0; i--) {
	  SPI[i] = tmp & 0xFF;
	  tmp = tmp >> 8;
     }
	  
     *lifetime = getspilifetime(st) + (arc4random() & 0x1F);

     return (0);
}


int
spi_insert(struct spiob *ob)
{
	TAILQ_INSERT_TAIL(&spihead, ob, next);

	return (1);
}

int
spi_unlink(struct spiob *ob)
{
	LOG_DBG((LOG_SPI, 45, __FUNCTION__": unlinking %s spi %x",
		 ob->flags & SPI_OWNER ? "Owner" : "User",
		 ntohl(*(u_int32_t *)ob->SPI)));

	TAILQ_REMOVE(&spihead, ob, next);
	free(ob);
	
	return (1);
}

struct spiob *
spi_new(char *address, u_int8_t *spi)
{
     struct spiob *p;

     if (spi_find(address, spi) != NULL)
	  return (NULL);
     if ((p = calloc(1, sizeof(struct spiob))) == NULL)
	  return (NULL);

     if ((p->address = strdup(address)) == NULL) {
	  free(p);
	  return (NULL);
     }
     bcopy(spi, p->SPI, SPI_SIZE);
     
     return (p);
}

int
spi_value_reset(struct spiob *ob)
{ 
	if (ob->address != NULL) {
		free(ob->address);
		ob->address = NULL;
	}
	if (ob->local_address != NULL) {
		free(ob->local_address);
		ob->local_address = NULL;
	}
	if (ob->attributes != NULL) {
		free(ob->attributes);
		ob->attributes = NULL;
	}
	if (ob->sessionkey != NULL) {
		memset(ob->sessionkey, 0, ob->sessionkeysize);
		free(ob->sessionkey);
		ob->sessionkey = NULL;
	}

	return (1);
}


struct spiob * 
spi_find_attrib(char *address, u_int8_t *attrib, u_int16_t attribsize) 
{ 
     struct spiob *tmp; 
     u_int16_t i;

     for (tmp = TAILQ_FIRST(&spihead); tmp; tmp = TAILQ_NEXT(tmp, next)) { 
          if (!strcmp(address, tmp->address)) {
	       for (i = 0; i < attribsize; i += attrib[i + 1] + 2) {
		    if (attrib[i] == AT_AH_ATTRIB || 
			attrib[i] == AT_ESP_ATTRIB)
			    continue;
		    if (!isinattrib(tmp->attributes, tmp->attribsize, attrib[i]))
			 break;
	       }
	       if (i == attribsize)
		    return (tmp);
	  }
     } 

     return (NULL); 
} 

/* 
 * find the spi ob with matching address
 * Alas this is tweaked, for SPI_OWNER compare with local_address
 * and for user compare with address.
 */

struct spiob *
spi_find(char *address, u_int8_t *spi)
{
	struct spiob *tmp;

	for (tmp = TAILQ_FIRST(&spihead); tmp; tmp = TAILQ_NEXT(tmp, next)) {
		if (bcmp(spi, tmp->SPI, SPI_SIZE))
			continue;

		if (address == NULL)
			break;

		if (tmp->flags & SPI_OWNER ?
		    !strcmp(address, tmp->local_address) :
		    !strcmp(address, tmp->address))
			break;
	}

	return (tmp);
}

void
spi_expire(void)
{
	struct spiob *tmp, *next;
	time_t tm;

	tm = time(NULL);
	for (tmp = TAILQ_FIRST(&spihead); tmp; tmp = next) {
		next = TAILQ_NEXT(tmp, next);

		if (tmp->lifetime == -1 || 
		    tmp->lifetime + (tmp->flags & SPI_OWNER ? 
				     CLEANUP_TIMEOUT : 0) > tm)
			continue;

		LOG_DBG((LOG_SPI, 30, __FUNCTION__
			 ": expiring %s spi %x to %s",
			 tmp->flags & SPI_OWNER ? "Owner" : "User",
			 ntohl(*(u_int32_t *)tmp->SPI), tmp->address));

#ifdef IPSEC
		kernel_unlink_spi(tmp);
#endif
		spi_value_reset(tmp);
		spi_unlink(tmp);
	}
}

/*	$OpenBSD: spi.c,v 1.10 2002/06/10 19:58:20 espie Exp $	*/

/*
 * Copyright 1997-2000 Niels Provos <provos@citi.umich.edu>
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
static char rcsid[] = "$OpenBSD: spi.c,v 1.10 2002/06/10 19:58:20 espie Exp $";
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
#include "secrets.h"
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
	LOG_DBG((LOG_SPI, 45, "%s: unlinking %s spi %x", __func__,
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

		if (tmp->lifetime == -1 || tmp->lifetime > tm)
			continue;

		LOG_DBG((LOG_SPI, 30, 
			 "%s: expiring %s spi %x to %s", __func__,
			 tmp->flags & SPI_OWNER ? "Owner" : "User",
			 ntohl(*(u_int32_t *)tmp->SPI), tmp->address));

#ifdef IPSEC
		kernel_unlink_spi(tmp);
#endif
		spi_value_reset(tmp);
		spi_unlink(tmp);
	}
}

void
spi_update_insert(struct spiob *spi)
{
	time_t tm = time(NULL);
	int seconds;

	seconds = spi->lifetime - tm;
	if (seconds < 0)
		seconds = 0;
	seconds = seconds * 9 / 10;

	schedule_insert(UPDATE, seconds, spi->SPI, SPI_SIZE);
}

void
spi_update(int sock, u_int8_t *spinr)
{
	struct stateob *st;
	struct spiob *spi, *nspi;
	struct sockaddr_in sin;
	
	/* We are to create a new SPI */
	if ((spi = spi_find(NULL, spinr)) == NULL) {
		log_print("spi_find() in schedule_process()");
		return;
	}

	if (!(spi->flags & SPI_OWNER))
		return;

	if (spi->flags & SPI_UPDATED) {
		LOG_DBG((LOG_SPI, 55, "%s: SPI %x already updated", __func__,
			 ntohl(*(u_int32_t *)spinr)));
		return;
	}

	LOG_DBG((LOG_SPI, 45, "%s: updating SPI %x", __func__,
		 ntohl(*(u_int32_t *)spinr)));


	if ((st = state_find_cookies(spi->address, spi->icookie, NULL)) == NULL) {
		/*
		 * This happens always when an exchange expires but
		 * updates are still scheduled for it.
		 */
		LOG_DBG((LOG_SPI, 65, "%s: state_find_cookies()", __func__));
		return;
	}

	if (st->oSPIattrib != NULL)
		free(st->oSPIattrib);
	if ((st->oSPIattrib = calloc(spi->attribsize, sizeof(u_int8_t))) == NULL) {
		log_error("calloc() in schedule_process()");
		return;
	}
	st->oSPIattribsize = spi->attribsize;
	bcopy(spi->attributes, st->oSPIattrib, st->oSPIattribsize);

	/* We can keep our old attributes, this is only an update */
	if (make_spi(st, spi->local_address, st->oSPI, &(st->olifetime),
		     &(st->oSPIattrib), &(st->oSPIattribsize)) == -1) {
		log_print("%s: make_spi()", __func__);
		return;
	}

	packet_size = PACKET_BUFFER_SIZE;
	if (photuris_spi_update(st, packet_buffer, &packet_size) == -1) {
		log_print("%s: photuris_spi_update()", __func__);
		return;
	}

	/* Send the packet */
	sin.sin_port = htons(st->port);
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = inet_addr(st->address);
		
	if (sendto(sock, packet_buffer, packet_size, 0,
		   (struct sockaddr *) &sin, sizeof(sin)) != packet_size) {
		log_error("sendto() in schedule_process()");
		return;
	}
	
#ifdef DEBUG
	printf("Sending SPI UPDATE to %s.\n", st->address);
#endif
	/* Insert Owner SPI */
	if ((nspi = spi_new(st->address, st->oSPI)) == NULL) {
		log_error("spi_new() in handle_spi_needed()");
		return;
	}
	if ((nspi->local_address = strdup(spi->local_address)) == NULL) {
		log_error("strdup() in handle_spi_needed()");
		spi_value_reset(nspi);
		return;
	}
	bcopy(st->icookie, nspi->icookie, COOKIE_SIZE);
	nspi->flags |= SPI_OWNER;
	nspi->attribsize = st->oSPIattribsize;
	nspi->attributes = calloc(nspi->attribsize, sizeof(u_int8_t));
	if (nspi->attributes == NULL) {
		log_error("calloc() in handle_spi_needed()");
		spi_value_reset(nspi);
		return;
	}
	bcopy(st->oSPIattrib, nspi->attributes, nspi->attribsize);
	nspi->lifetime = time(NULL) + st->olifetime;

	make_session_keys(st, nspi);

	spi_insert(nspi);
	spi_update_insert(nspi);

#ifdef IPSEC
	kernel_insert_spi(st, nspi);
#endif

	/* Our old SPI has been updated, dont update it again */
	spi->flags |= SPI_UPDATED;
}

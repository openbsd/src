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
static char rcsid[] = "$Id: spi.c,v 1.2 1999/03/27 21:18:02 provos Exp $";
#endif

#define _SPI_C_

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
#include "errlog.h"
#ifdef IPSEC
#include "kernel.h"
#endif


static struct spiob *spiob = NULL;

time_t
getspilifetime(struct stateob *st)
{
     /* XXX - destination depend lifetimes */
     return st->spi_lifetime;
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
	       log_error(0, "select_attrib() in make_spi()");
	       return -1;
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

     return 0;
}

int
spi_set_tunnel(struct stateob *st, struct spiob *spi)
{
     if (st->flags & IPSEC_OPT_TUNNEL) {
	  spi->flags |= SPI_TUNNEL;
	  spi->isrc = st->isrc;
	  spi->ismask = st->ismask;
	  spi->idst = st->idst;
	  spi->idmask = st->idmask;
     } else {
	  spi->isrc = inet_addr(spi->local_address);
	  spi->ismask = inet_addr("255.255.255.255");
	  spi->idst = inet_addr(spi->address);
	  spi->idmask = inet_addr("255.255.255.255");
     }
     return 1;
}


int
spi_insert(struct spiob *ob)
{
     struct spiob *tmp;

     ob->next = NULL;

     if(spiob == NULL) {
	  spiob = ob;
	  return 1;
     }
     
     tmp=spiob;
     while(tmp->next!=NULL)
	  tmp = tmp->next;

     tmp->next = ob;
     return 1;
}

int
spi_unlink(struct spiob *ob)
{
     struct spiob *tmp;
     if(spiob == ob) {
	  spiob = ob->next;
	  free(ob);
	  return 1;
     }

     for(tmp=spiob; tmp!=NULL; tmp=tmp->next) {
	  if(tmp->next==ob) {
	       tmp->next=ob->next;
	       free(ob);
	       return 1;
	  }
     }
     return 0;
}

struct spiob *
spi_new(char *address, u_int8_t *spi)
{
     struct spiob *p;
     if (spi_find(address, spi) != NULL)
	  return NULL;
     if ((p = calloc(1, sizeof(struct spiob))) == NULL)
	  return NULL;

     if ((p->address = strdup(address)) == NULL) {
	  free(p);
	  return NULL;
     }
     bcopy(spi, p->SPI, SPI_SIZE);
     
     return p;
}

int
spi_value_reset(struct spiob *ob)
{ 
     if (ob->address != NULL)
	  free(ob->address);
     if (ob->local_address != NULL)
	  free(ob->local_address);
     if (ob->attributes != NULL)
	  free(ob->attributes);
     if (ob->sessionkey != NULL)
	  free(ob->sessionkey);

     return 1;
}


struct spiob * 
spi_find_attrib(char *address, u_int8_t *attrib, u_int16_t attribsize) 
{ 
     struct spiob *tmp = spiob; 
     u_int16_t i;

     while(tmp!=NULL) { 
          if(!strcmp(address, tmp->address)) {
	       for(i=0;i<attribsize; i += attrib[i+1]+2) {
		    if (attrib[i] == AT_AH_ATTRIB || attrib[i] == AT_ESP_ATTRIB)
			 continue;
		    if (!isinattrib(tmp->attributes, tmp->attribsize, attrib[i]))
			 break;
	       }
	       if (i == attribsize)
		    return tmp;
	  }
          tmp = tmp->next; 
     } 
     return NULL; 
} 

/* 
 * find the spi ob with matching address
 * Alas this is tweaked, for SPI_OWNER compare with local_address
 * and for user compare with address.
 */

struct spiob *
spi_find(char *address, u_int8_t *spi)
{
     struct spiob *tmp = spiob;
     while(tmp!=NULL) {
          if ((address == NULL || (tmp->flags & SPI_OWNER ? 
	      !strcmp(address, tmp->local_address) :
	      !strcmp(address, tmp->address))) &&
	     !bcmp(spi, tmp->SPI, SPI_SIZE))
	       return tmp;
	  tmp = tmp->next;
     }
     return NULL;
}

struct spiob *
spi_root(void)
{
     return spiob;
}

void
spi_cleanup()
{
     struct spiob *p;
     struct spiob *tmp = spiob;
     while(tmp!=NULL) {
	  p = tmp;
	  tmp = tmp->next;
	  spi_value_reset(p);
	  free(p);
     }
     spiob = NULL;
}

void
spi_expire(void)
{
     struct spiob *tmp = spiob, *p;
     time_t tm;

     tm = time(NULL);
     while (tmp != NULL) {
	  if (tmp->lifetime == -1 || 
	      tmp->lifetime + (tmp->flags & SPI_OWNER ? 
			       CLEANUP_TIMEOUT : 0) > tm) {
	       tmp = tmp->next;
	       continue;
	  }
#ifdef DEBUG
	  {
	       int i = BUFFER_SIZE;
	       bin2hex(buffer, &i, tmp->SPI, 4);
	       printf("Expiring %s spi %s to %s\n", 
		      tmp->flags & SPI_OWNER ? "Owner" : "User",
		      buffer, tmp->address);
	  }
#endif
#ifdef IPSEC
	  kernel_unlink_spi(tmp);
#endif
	  p = tmp;
	  tmp = tmp->next;
	  spi_value_reset(p);
	  spi_unlink(p);
     }
}

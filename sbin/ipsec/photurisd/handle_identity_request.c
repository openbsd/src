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
 * handle_identity_request:
 * receive a IDENTITY_REQUEST packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: handle_identity_request.c,v 1.2 1997/07/19 12:07:45 provos Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "photuris.h"
#include "packets.h"
#include "state.h"
#include "cookie.h"
#include "buffer.h"
#include "packet.h"
#include "encrypt.h"
#include "identity.h"
#include "spi.h"
#include "secrets.h"
#include "scheme.h"
#include "errlog.h"
#include "schedule.h"
#include "attributes.h"
#ifdef IPSEC
#include "kernel.h"
#endif

int
handle_identity_request(u_char *packet, int size, char *address, 
			char *local_address)
{
	struct identity_message *header;
	struct stateob *st;
	struct spiob *spi;
	u_int8_t *p, *attributes;
	u_int16_t i, asize, attribsize, tmp;
	u_int8_t signature[22];  /* XXX - constant */

	if (size < IDENTITY_MESSAGE_MIN)
	     return -1;	/* packet too small  */

	header = (struct identity_message *) packet;

	st = state_find_cookies(address, header->icookie, header->rcookie);
	if (st == NULL) {
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, BAD_COOKIE);
	     send_packet();
	     return 0;
	}

	if (st->phase != VALUE_RESPONSE && st->phase != SPI_UPDATE)
	     return -1;     /* We don't want this packet */

	/* Decrypt message */
	tmp = size - IDENTITY_MESSAGE_MIN;
	if (packet_decrypt(st, IDENTITY_MESSAGE_CHOICE(header), &tmp) == -1) {
	     log_error(0, "packet_decrypt() in handle_identity_request()");
	     goto verification_failed;
	}

	/* Verify message */
	if (!(i = get_identity_verification_size(st, IDENTITY_MESSAGE_CHOICE(header))))
	     goto verification_failed;

	asize = IDENTITY_MESSAGE_MIN;

	p = IDENTITY_MESSAGE_CHOICE(header);
	asize += p[1] + 2;
	p += p[1] + 2;
	asize += varpre2octets(p);
	p += varpre2octets(p);

	attributes = p + i;
	asize += i;                            /* Verification size */
	asize += packet[size-1];               /* Padding size */
	attribsize = 0;
	while(asize + attribsize < size)
	     attribsize += attributes[attribsize+1] + 2;

	asize += attribsize;

	if (asize != size) {
	     log_error(0, "wrong packet size in handle_identity_request()");
	     return 0;
	}

	if (!isattribsubset(st->oSPIoattrib,st->oSPIoattribsize,
			    attributes, attribsize)) {
	     log_error(0, "attributes are not a subset in handle_identity_request()");
	     return 0;
	}

	if (i > sizeof(signature)) {
	     log_error(0, "verification too long in handle_identity_request()");
	     goto verification_failed;
	}

	bcopy(p, signature, i);
	bzero(p, i);

	if (st->phase == VALUE_RESPONSE) {
	     /* Fill the state object, but only if we have not dont so before */
	     if (st->uSPIidentver == NULL) {
		  if((st->uSPIidentver = calloc(i, sizeof(u_int8_t))) == NULL) { 
		       log_error(1, "calloc() in handle_identity_request()"); 
		       goto verification_failed;
		  }
		  bcopy(signature, st->uSPIidentver, i);
		  st->uSPIidentversize = i;
	     }
	     
	     p = IDENTITY_MESSAGE_CHOICE(header);
	     if (st->uSPIidentchoice == NULL) {
		  if((st->uSPIidentchoice = calloc(p[1]+2, sizeof(u_int8_t))) == NULL) {
		       log_error(1, "calloc() in handle_identity_request()");
		       goto verification_failed;
		  }
		  bcopy(p, st->uSPIidentchoice, p[1]+2);
		  st->uSPIidentchoicesize = p[1]+2;
	     }

	     p += p[1] + 2;
	     if (st->uSPIident == NULL) {
		  if((st->uSPIident = calloc(varpre2octets(p), sizeof(u_int8_t))) == NULL) {
		       log_error(1,"calloc() in handle_identity_request()"); 
		       goto verification_failed;
		  }
		  bcopy(p, st->uSPIident, varpre2octets(p));
	     }
	
	     if (st->uSPIattrib == NULL) {
		  if((st->uSPIattrib = calloc(attribsize, sizeof(u_int8_t))) == NULL) {
		       log_error(1, "calloc() in handle_identity_request()");
		       return -1;
		  }
		  bcopy(attributes, st->uSPIattrib, attribsize);
		  st->uSPIattribsize = attribsize;
	     }

	     if (st->oSPIident == NULL && 
		 get_secrets(st, (ID_REMOTE|ID_LOCAL)) == -1) {
		  log_error(0, "get_secrets() in in handle_identity_request()");
		  goto verification_failed;
	     }

	}

	if (!verify_identity_verification(st, signature, packet, size)) {
	     if (st->phase != SPI_UPDATE) {
		  /* 
		   * Clean up everything used from this packet 
		   * but only if we did not get a valid packet before.
		   * Otherwise this could be used as Denial of Service.
		   */
		  free(st->uSPIidentchoice);
		  st->uSPIidentchoice = NULL; st->uSPIidentchoicesize = 0;
		  free(st->uSPIidentver);
		  st->uSPIidentver = NULL; st->uSPIidentversize = 0;
		  free(st->uSPIattrib);
		  st->uSPIattrib = NULL; st->uSPIattribsize = 0;
		  free(st->uSPIident);
		  st->uSPIident = NULL;
		  free(st->oSPIident);
		  st->oSPIident = NULL;

		  /* Clean up secrets */
		  free(st->oSPIsecret);
		  st->oSPIsecret = NULL; st->oSPIsecretsize = 0;
		  free(st->uSPIsecret);
		  st->uSPIsecret = NULL; st->uSPIsecretsize = 0;
	     }

	verification_failed:
	     log_error(0, "verification failed in handle_identity_request()");
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, VERIFICATION_FAILURE);
	     send_packet();
	     return 0;
	}

	if (st->phase != VALUE_RESPONSE) {
	     /* We got send the old packet again */
	     bcopy(st->packet, packet_buffer, st->packetlen);
	     packet_size = st->packetlen;
	     
	     send_packet();
	     return 0;
	}

	/* Create SPI + choice of attributes */
	if(make_spi(st, local_address, st->oSPI, &(st->olifetime), 
		    &(st->oSPIattrib), &(st->oSPIattribsize)) == -1) {
	     log_error(0, "make_spi() in handle_identity_request()");
	     return -1;
	}
	
	packet_size = PACKET_BUFFER_SIZE;
	if (photuris_identity_response(st, packet_buffer, &packet_size) == -1)
	     return -1;

	send_packet();

	packet_save(st, packet_buffer, packet_size);

	bcopy(header->SPI, st->uSPI, SPI_SIZE);
	st->ulifetime = (header->lifetime[0] << 16) + 
	     (header->lifetime[1] << 8) + header->lifetime[2];
	
	if (st->oSPI[0] || st->oSPI[1] || st->oSPI[2] || st->oSPI[3]) {
             /* Insert Owner SPI */ 
             if ((spi = spi_new(st->address, st->oSPI)) == NULL) { 
                  log_error(0, "spi_new() in handle_identity_request()"); 
                  return -1; 
             } 
	     if ((spi->local_address = strdup(local_address)) == NULL) {
		  log_error(0, "strdup() in handle_identity_request()");
		  return -1;
	     }
             bcopy(st->icookie, spi->icookie, COOKIE_SIZE); 
	     spi->owner = 1;
             spi->attribsize = st->oSPIattribsize; 
             spi->attributes = calloc(spi->attribsize, sizeof(u_int8_t)); 
             if (spi->attributes == NULL) { 
                  log_error(1, "calloc() in handle_identity_request()"); 
                  spi_value_reset(spi); 
                  return -1; 
             } 
             bcopy(st->oSPIattrib, spi->attributes, spi->attribsize); 
             spi->lifetime = time(NULL) + st->olifetime; 

	     /* Make session keys for Owner */
	     make_session_keys(st, spi);

             spi_insert(spi); 
#ifdef IPSEC
	     kernel_insert_spi(spi);
#endif
             schedule_insert(UPDATE, st->olifetime/2, spi->SPI, SPI_SIZE); 
	}

	if (st->uSPI[0] || st->uSPI[1] || st->uSPI[2] || st->uSPI[3]) {
	     /* Insert User SPI */
	     if ((spi = spi_new(st->address, st->uSPI)) == NULL) {
		  log_error(0, "spi_new() in handle_identity_request()");
		  return -1;
	     }
	     if ((spi->local_address = strdup(local_address)) == NULL) {
		  log_error(1, "strdup() in handle_identity_request()");
		  return -1;
	     }
	     bcopy(st->icookie, spi->icookie, COOKIE_SIZE);
	     spi->attribsize = st->uSPIattribsize;
	     spi->attributes = calloc(spi->attribsize, sizeof(u_int8_t));
	     if (spi->attributes == NULL) {
		  log_error(1, "calloc() in handle_identity_request()");
		  spi_value_reset(spi);
		  return -1;
	     }
	     bcopy(st->uSPIattrib, spi->attributes, spi->attribsize);
	     spi->lifetime = time(NULL) + st->ulifetime;

	     /* Make session keys for User */
	     make_session_keys(st, spi);

	     spi_insert(spi);
#ifdef IPSEC
	     kernel_insert_spi(spi);
#endif
	}

	st->lifetime = exchange_lifetime + time(NULL) + random() % 20;

	st->retries = 0;
	st->phase = SPI_UPDATE;
	return 0;
}

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
 * handle_spi_needed:
 * receive a SPI_NEEDED packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: handle_spi_needed.c,v 1.1.1.1 1997/07/18 22:48:51 provos Exp $";
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
#include "validity.h"
#include "secrets.h"
#include "schedule.h"
#include "scheme.h"
#include "errlog.h"
#include "spi.h"
#ifdef IPSEC
#include "kernel.h"
#endif

int
handle_spi_needed(u_char *packet, int size, char *address, 
			char *local_address)
{
	struct spi_needed *header;
	struct stateob *st;
	struct spiob *spi;
	u_int8_t *p, *attributes;
	u_int16_t i, asize, attribsize, tmp;
	u_int8_t signature[22];  /* XXX - constant */

	if (size < SPI_NEEDED_MIN)
	     return -1;	/* packet too small  */

	header = (struct spi_needed *) packet;

	st = state_find_cookies(address, header->icookie, header->rcookie);
	if (st == NULL) {
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, BAD_COOKIE);
	     send_packet();
	     return 0;
	}

	if (st->phase != SPI_UPDATE && st->phase != SPI_NEEDED)
	     return 0;     /* We don't want this packet */

	/* Decrypt message */
	tmp = size - SPI_NEEDED_MIN;
	if (packet_decrypt(st, SPI_NEEDED_VERIFICATION(header), &tmp) == -1) {
	     log_error(0, "packet_decrypt() in handle_spi_needed()");
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, VERIFICATION_FAILURE);
	     send_packet();
	     return -1;
	}

	/* Verify message */
	if (!(i = get_validity_verification_size(st))) {
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, VERIFICATION_FAILURE);
	     send_packet();
	     return -1;
	}
	
	asize = SPI_NEEDED_MIN + i;

	p = SPI_NEEDED_VERIFICATION(header);

	attributes = p + i;
	asize += packet[size-1];           /* Padding size */
	attribsize = 0;
	while(asize + attribsize < size)
	     attribsize += attributes[attribsize+1] + 2;

	asize += attribsize;

	if (asize != size) {
	     log_error(0, "wrong packet size in handle_spi_needed()");
	     return -1;
	}

	if (i > sizeof(signature)) {
	     log_error(0, "verification too long in handle_spi_needed()");
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, VERIFICATION_FAILURE);
	     send_packet();
	     return -1;
	}

	bcopy(p, signature, i);
	bzero(p, i);

	if (!verify_validity_verification(st, signature, packet, size)) {
	     log_error(0, "verification failed in handle_spi_needed()");
	     packet_size = PACKET_BUFFER_SIZE;
	     photuris_error_message(st, packet_buffer, &packet_size,
				    header->icookie, header->rcookie,
				    0, VERIFICATION_FAILURE);
	     send_packet();
	     return 0;
	}

	if (st->uSPIoattrib != NULL)
	     free(st->uSPIoattrib);

	if((st->uSPIoattrib = calloc(attribsize, sizeof(u_int8_t))) == NULL) {
	     log_error(1, "calloc() in handle_spi_needed()");
	     return -1;
	}
	bcopy(attributes, st->uSPIoattrib, attribsize);
	st->uSPIoattribsize = attribsize;

	/* Delete old attributes, make_spi will make new */
	if (st->oSPIattrib != NULL) {
	     free(st->oSPIattrib);
	     st->oSPIattrib = NULL;
	     st->oSPIattribsize = 0;
	}
	if (make_spi(st, local_address, st->oSPI, &(st->olifetime),
		     &(st->oSPIattrib), &(st->oSPIattribsize)) == -1)
	     return -1;

	packet_size = PACKET_BUFFER_SIZE; 
	if (photuris_spi_update(st, packet_buffer, &packet_size) == -1) {
	     log_error(0, "photuris_spi_update() in handle_spi_needed()");
	     return -1;
	}
	send_packet(); 

	/* Insert Owner SPI */
	if ((spi = spi_new(st->address, st->oSPI)) == NULL) {
	     log_error(0, "spi_new() in handle_spi_needed()");
	     return -1;
	}
	if ((spi->local_address = strdup(local_address)) == NULL) {
	     log_error(1, "strdup() in handle_spi_needed()");
	     return -1;
	}
	bcopy(st->icookie, spi->icookie, COOKIE_SIZE);
	spi->owner = 1;
	spi->attribsize = st->oSPIattribsize;
	spi->attributes = calloc(spi->attribsize, sizeof(u_int8_t));
	if (spi->attributes == NULL) {
	     log_error(1, "calloc() in handle_spi_needed()");
	     spi_value_reset(spi);
	     return -1;
	}
	bcopy(st->oSPIattrib, spi->attributes, spi->attribsize);
	spi->lifetime = time(NULL) + st->olifetime;

	make_session_keys(st, spi);

	spi_insert(spi);
	schedule_insert(UPDATE, st->olifetime/2, spi->SPI, SPI_SIZE);
#ifdef IPSEC
	kernel_insert_spi(spi);
#endif
	return 0;
}

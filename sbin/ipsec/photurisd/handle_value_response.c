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
 * handle_value_response:
 * receive a VALUE_RESPONSE packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: handle_value_response.c,v 1.2 1997/07/24 23:47:14 provos Exp $";
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "photuris.h"
#include "packets.h"
#include "state.h"
#include "cookie.h"
#include "buffer.h"
#include "scheme.h"
#include "packet.h"
#include "schedule.h"
#include "exchange.h"
#include "secrets.h"
#include "spi.h"
#include "errlog.h"
#ifdef DEBUG
#include "config.h"
#endif

int
handle_value_response(u_char *packet, int size, char *address, 
		      char *local_address)

{
	struct value_response *header;
	struct stateob *st;
	mpz_t test;
	u_int8_t *p;
	u_int16_t i, asize;

	if (size < VALUE_RESPONSE_MIN)
	     return -1;	/* packet too small  */

	header = (struct value_response *) packet;

	st = state_find_cookies(address, header->icookie, header->rcookie);
	if (st == NULL)
	     return -1;     /* Silently discard */

	if (st->phase != VALUE_REQUEST)
	     return -1;     /* We don't want this packet */

	/* Check exchange value - XXX doesn't check long form */
	p = VALUE_RESPONSE_VALUE(header);
	asize = VALUE_RESPONSE_MIN + varpre2octets(p);
	p += varpre2octets(p);
	if (asize >= size)
	     return -1;  /* Exchange value too big */
	     
	/* Check attributes */
	i = 0;
	while(asize + i < size)
	     i += p[i+1] + 2;

	if (asize + i != size)
	     return -1;  /* attributes dont match udp length */

	/* Now check the exchange value for defects */
	mpz_init_set_varpre(test, VALUE_RESPONSE_VALUE(header));
	if (!exchange_check_value(test, st->generator, st->modulus)) {
	     mpz_clear(test);
	     return 0;
	}
	mpz_clear(test);

	/* Fill the state object */
	st->uSPIoattrib = calloc(i, sizeof(u_int8_t));
	if (st->uSPIoattrib == NULL) {
	     state_value_reset(st);
	     state_unlink(st);
	     return -1;
	}
	bcopy(p, st->uSPIoattrib, i);  
	st->uSPIoattribsize = i;  

#ifdef DEBUG 
	{
	     int i = BUFFER_SIZE; 
	     bin2hex(buffer, &i, VALUE_RESPONSE_VALUE(header), 
		     varpre2octets(VALUE_RESPONSE_VALUE(header))); 
	     printf("Got exchange value 0x%s\n", buffer); 
	}
#endif 

	/* Set exchange value */
	st->texchangesize = varpre2octets(VALUE_RESPONSE_VALUE(header));
	st->texchange = calloc(st->texchangesize, sizeof(u_int8_t));
	if (st->texchange == NULL) {
	     log_error(1, "calloc() in handle_value_response()");
	     return -1;
	}
	bcopy(VALUE_RESPONSE_VALUE(header), st->texchange, st->texchangesize);

	/* Compute the shared secret now */
	compute_shared_secret(st, &(st->shared), &(st->sharedsize));
#ifdef DEBUG  
	{
	     int i = BUFFER_SIZE;
	     bin2hex(buffer, &i, st->shared, st->sharedsize);
	     printf("Shared secret is: 0x%s\n", buffer);  
	}
#endif  

	/* Create SPI + choice of attributes */
	if (make_spi(st, local_address, st->oSPI, &(st->olifetime),
		     &(st->oSPIattrib), &(st->oSPIattribsize)) == -1) {
	     log_error(0, "make_spi() in handle_value_response()");
	     return -1;
	}
	
	packet_size = PACKET_BUFFER_SIZE;
	if (photuris_identity_request(st, packet_buffer, &packet_size) == -1)
	     return -1;

	packet_save(st, packet_buffer, packet_size);

	send_packet();

	st->retries = 0;
	st->phase = IDENTITY_REQUEST;

	schedule_remove(TIMEOUT, st->icookie);
	schedule_insert(TIMEOUT, retrans_timeout, st->icookie, COOKIE_SIZE);
	return 0;
}

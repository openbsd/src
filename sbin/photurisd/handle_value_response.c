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
static char rcsid[] = "$Id: handle_value_response.c,v 1.1 1998/11/14 23:37:24 deraadt Exp $";
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
        struct packet_sub parts[] = {
	     { "Exchange Value", FLD_VARPRE, 0, 0, },
	     { "Offered Attributes", FLD_ATTRIB, FMD_ATT_FILL, 0, },
	     { NULL }
	};
	struct packet vr_msg = {
	     "Value Response",
	     VALUE_RESPONSE_MIN, 0, parts
	};
	struct value_response *header;
	struct stateob *st;
	mpz_t test;

	if (size < VALUE_RESPONSE_MIN)
	     return -1;	/* packet too small  */

	if (packet_check(packet, size, &vr_msg) == -1) {
	     log_error(0, "bad packet structure in handle_value_response()");
	     return -1;
	}

	header = (struct value_response *) packet;

	st = state_find_cookies(address, header->icookie, header->rcookie);
	if (st == NULL)
	     return -1;     /* Silently discard */

	if (st->phase != VALUE_REQUEST)
	     return -1;     /* We don't want this packet */

	/* Now check the exchange value for defects */
	mpz_init_set_varpre(test, parts[0].where);
	if (!exchange_check_value(test, st->generator, st->modulus)) {
	     mpz_clear(test);
	     return 0;
	}
	mpz_clear(test);

	/* Reserved Field for TBV */
	bcopy(header->reserved, st->uSPITBV, 3);

	/* Fill the state object */
	st->uSPIoattrib = calloc(parts[1].size, sizeof(u_int8_t));
	if (st->uSPIoattrib == NULL) {
	     state_value_reset(st);
	     state_unlink(st);
	     return -1;
	}
	bcopy(parts[1].where, st->uSPIoattrib, parts[1].size);  
	st->uSPIoattribsize = parts[1].size;  

#ifdef DEBUG 
	{
	     int i = BUFFER_SIZE; 
	     bin2hex(buffer, &i, parts[0].where, parts[0].size);
	     printf("Got exchange value 0x%s\n", buffer); 
	}
#endif 

	/* Set exchange value */
	st->texchangesize = parts[0].size;
	st->texchange = calloc(st->texchangesize, sizeof(u_int8_t));
	if (st->texchange == NULL) {
	     log_error(1, "calloc() in handle_value_response()");
	     return -1;
	}
	bcopy(parts[0].where, st->texchange, st->texchangesize);

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
	
	/* Initialize Privacy Keys from Exchange Values */
	init_privacy_key(st, 0);   /* User -> Owner direction */
	init_privacy_key(st, 1);   /* Owner -> User direction */

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

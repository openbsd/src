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
 * handle_cookie_response:
 * receive a COOKIE_RESPONSE packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: handle_cookie_response.c,v 1.1 1998/11/14 23:37:23 deraadt Exp $";
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
#include "scheme.h"
#include "packet.h"
#include "schedule.h"
#include "errlog.h"
#include "config.h"

int
handle_cookie_response(u_char *packet, int size,
		       char *address, int port)

{
	struct cookie_response *header;
	struct stateob *st;
	u_int8_t *p;
	u_int16_t i, n;

	if (size < COOKIE_RESPONSE_MIN)
	     return -1;	/* packet too small */

	header = (struct cookie_response *) packet;

	/* Take multi home hosts into account */
	st = state_root();
	while(st != NULL) {
	     if (!bcmp(header->icookie,st->icookie,COOKIE_SIZE))
		  break;
	     st = st->next;
	}
	if (st == NULL)
	     return -1;    /* Silently discard - XXX log perhaps ? */
		
	if (st->phase != COOKIE_REQUEST)
	     return -1;    /* We didn't want a cookie response */

	if (strcmp(address, st->address)) {
	     /* XXX - is this a sane thing to do ? */
	     log_error(0, "Response from multihomed host, address %s will "
		       "be changed to %s.", st->address, address);
	     strncpy(st->address, address, 15);
	     st->address[15] = '\0';
	}

	/* Check scheme size */
	p = COOKIE_RESPONSE_SCHEMES(header);
	i = 0;
	while(i<size-COOKIE_RESPONSE_MIN) {
	     if ((n = scheme_get_len(p + i)) == 0)
		  break;
	     i += n;
	}

	if (i != size-COOKIE_RESPONSE_MIN) {
	     log_error(0, "schemes corrupt in handle_cookie_response()");
	     return -1;    /* Size didn't match UDP size */
	}

	/* Copy responder cookies and offered schemes */
	bcopy(header->rcookie, st->rcookie, COOKIE_SIZE);
	if ((st->roschemes = calloc(i, sizeof(u_int8_t))) == NULL) {
	     state_value_reset(st);
	     state_unlink(st);
	     return -1;    /* Not enough memory */
	}
	bcopy(p, st->roschemes, i);
	st->roschemesize = i;

	if (pick_scheme(&(st->scheme), &(st->schemesize), p, i) == -1) {
	     state_value_reset(st);
	     state_unlink(st);
	     return -1; 
	}

	if (pick_attrib(st, &(st->oSPIoattrib), 
			&(st->oSPIoattribsize)) == -1) {
             state_value_reset(st);
             state_unlink(st);
	     return -1;
	}

	/* Take the counter from the cookie response */
	st->counter = header->counter;
	     
	packet_size = PACKET_BUFFER_SIZE;
	if (photuris_value_request(st, packet_buffer, &packet_size) == -1)
	     return -1;
	
	packet_save(st, packet_buffer, packet_size);

	send_packet();

	st->retries = 0;
	st->phase = VALUE_REQUEST;

	schedule_remove(TIMEOUT, st->icookie);
	schedule_insert(TIMEOUT, retrans_timeout, st->icookie, COOKIE_SIZE);
	return 0;
}

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
 * handle_cookie_request:
 * receive a COOKIE_REQUEST packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: handle_cookie_request.c,v 1.1 1998/11/14 23:37:23 deraadt Exp $";
#endif

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"
#include "photuris.h"
#include "packets.h"
#include "state.h"
#include "cookie.h"
#include "buffer.h"
#include "packet.h"

int
handle_cookie_request(u_char *packet, int size,
		      u_int8_t *address, u_int16_t port, 
		      u_int8_t *schemes, u_int16_t ssize) 

{
	struct cookie_request *header;
	struct stateob *prev_st, *st = NULL;
	time_t tm = 0;

	u_int8_t icookie[COOKIE_SIZE];

	/* XXX - check resource limit */

	if (size != COOKIE_REQUEST_PACKET_SIZE)
	     return -1;	/* packet too small/big  */

	header = (struct cookie_request *) packet;

	if ((prev_st=state_find(address)) != NULL) {
	     int exceeded = 1, match = 0;

	     st = prev_st;

	     /* 
	      * Find exchanges which are not timed out and the rcookie doesnt
	      * match any exchange -> resource limit.
	      */

	     tm = time(NULL);
	     while(prev_st != NULL) {
		  if (prev_st->lifetime > tm)
		       exceeded = 0;

		  if (prev_st->lifetime > st->lifetime)
		       st = prev_st;

		  if ((!prev_st->initiator && 
		       !bcmp(prev_st->rcookie, header->rcookie, COOKIE_SIZE))||
		       (prev_st->initiator && 
			!bcmp(prev_st->icookie, header->rcookie, COOKIE_SIZE)))
		       match = 1;
		  prev_st = state_find_next(prev_st, address);
	     }
	     if (!match && !exceeded) {
		  packet_size = PACKET_BUFFER_SIZE;
		  photuris_error_message(st, packet_buffer, &packet_size,
					 header->icookie, header->rcookie,
					 header->counter, RESOURCE_LIMIT);
		  send_packet();
		  return 0;
	     }
	}


	bcopy(header->icookie, icookie, COOKIE_SIZE);

	packet_size = PACKET_BUFFER_SIZE;
	if (photuris_cookie_response(st != NULL && 
				     st->lifetime > tm ? st : NULL, 
				     packet_buffer, &packet_size,
				     icookie, header->counter,
				     address, port,
				     schemes, ssize) == -1 )
	     return -1; /* Some error happened */

	send_packet();

	return 0;
}

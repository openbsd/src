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
 * handle_resource_limit:
 * receive a RESOURCE_LIMIT packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: handle_resource_limit.c,v 1.1 1998/11/14 23:37:24 deraadt Exp $";
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
#include "schedule.h"
#include "errlog.h"

int
handle_resource_limit(u_char *packet, int size, char *address)
{
	struct error_message *header;
	struct stateob *st;
	int offset, counter;

	if (size != ERROR_MESSAGE_PACKET_SIZE + 1)
	     return -1;	/* packet too small/big  */

	header = (struct error_message *) packet;
	counter = packet[ERROR_MESSAGE_PACKET_SIZE];

	if ((st = state_find_cookies(NULL, header->icookie, NULL)) == NULL) {
	     log_error(0, "No state for RESOURCE_LIMIT message from %s", 
		       address);
	     return -1;
	}
	
	switch(st->phase) {
	case COOKIE_REQUEST:
	     /* 
	      * The other party has still an exchange which has been
	      * purged on our side.
	      */
	     if (counter != 0) {
		  bcopy(header->rcookie, st->rcookie, COOKIE_SIZE);
		  st->counter = counter;
	     }

	     /* We crank the timeout, so we can start a new exchange */
	     st->lifetime += exchange_timeout;
	     st->resource = 1;
	case VALUE_REQUEST:
	     offset = schedule_offset(TIMEOUT, st->icookie);
	     if (offset == -1)
		  offset = retrans_timeout;
	     else {
		  offset *= 2;
		  if (offset > exchange_timeout/2)
		       offset = exchange_timeout/2;
	     }
	     
	     schedule_remove(TIMEOUT, st->icookie);
	     schedule_insert(TIMEOUT, offset, st->icookie, COOKIE_SIZE);
	     break;
	case SPI_UPDATE:
	     /* XXX - we have to wait for expiring of another SPI */
	     break;
	default:
	     log_error(0, "Wrong phase for RESOURCE_LIMIT from %s",  
		       address); 
	     return 0;
	}

	return 0;
}

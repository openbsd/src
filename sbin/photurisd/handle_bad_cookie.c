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
 * handle_bad_cookie:
 * receive a BAD_COOKIE packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: handle_bad_cookie.c,v 1.1 1998/11/14 23:37:23 deraadt Exp $";
#endif

#include <stdio.h>
#include <stdlib.h>
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
#include "schedule.h"
#include "errlog.h"
#include "server.h"
#include "packet.h"
#include "api.h"

int
handle_bad_cookie(u_char *packet, int size, char *address)
{
	struct error_message *header;
	struct stateob *st, *newst;

	if (size != ERROR_MESSAGE_PACKET_SIZE)
	     return -1;	/* packet too small/big  */

	header = (struct error_message *) packet;

	if ((st = state_find_cookies(address, header->icookie, 
				     header->rcookie)) == NULL) {
	     log_error(0, "No state for BAD_COOKIE message from %s", 
		       address);
	     return -1;
	}

	if ((st->retries < max_retries && 
	     (st->phase == VALUE_REQUEST || st->phase == IDENTITY_REQUEST)) ||
	     (st->phase != VALUE_REQUEST && st->phase != IDENTITY_REQUEST &&
	      st->phase != SPI_NEEDED && st->phase != SPI_UPDATE)) {
	     log_error(0, "Ignored BAD_COOKIE message from %s", address); 
	     
	     return 0;                 /* Nothing needs to be done */
	}

	if (st->phase == SPI_UPDATE) {
	     st->lifetime = time(NULL);

	     log_error(0, "Expired exchange on BAD_COOKIE from %s",
		       address);
	     return 0;
	}

	schedule_remove(TIMEOUT, st->icookie);
	state_unlink(st);

        /* Set up a new state object */
        if ((newst = state_new()) == NULL) {
             log_error(1, "state_new() in handle_bad_cookie()");
             return -1;
        }

	newst->flags = st->flags;
	if (st->user != NULL)
	     newst->user = strdup(st->user);

	state_value_reset(st);

	if (start_exchange(global_socket, newst, address, global_port) == -1) {
	     log_error(0, "start_exchange() in handle_bad_cookie()");
	     state_value_reset(st);
	     return -1;
	}

        state_insert(newst);

	return 0;
}

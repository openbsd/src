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
 * photuris_cookie_request:
 * create a COOKIE_REQUEST packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: photuris_cookie_request.c,v 1.1 1998/11/14 23:37:26 deraadt Exp $";
#endif

#include <stdio.h>
#include <string.h> /* XXX - get header files right */
#include <strings.h>
#include <time.h>
#include <sys/time.h>
#include "config.h"
#include "photuris.h"
#include "packets.h"
#include "state.h"
#include "cookie.h"

int
photuris_cookie_request(struct stateob *st, u_char *buffer, int *size)
{
	struct cookie_request *header;
	struct stateob *prev_st, *old_st;
	time_t timeout = 0;

	if (*size < COOKIE_REQUEST_PACKET_SIZE)
	     return -1;	/* buffer not large enough */

	header = (struct cookie_request *) buffer;
	*size = COOKIE_REQUEST_PACKET_SIZE;	/* fixed size */
	
	if (st->counter == 0) {
	     prev_st = state_find(st->address);
	     old_st = NULL;
	     while (prev_st != NULL) {
		  if (prev_st->lifetime >= timeout) {
		       timeout = prev_st->lifetime;
		       old_st = prev_st;
		  }
		  prev_st = prev_st->next;
	     }
	     
	     /* Check if we have an exchange going already */
	     if (old_st != NULL && old_st != st && timeout > time(NULL)) {
		  if (old_st->initiator) {
		       bcopy(old_st->rcookie, st->rcookie, COOKIE_SIZE);
		       st->counter = old_st->counter;
		  } else {
		       bcopy(old_st->icookie, st->rcookie, COOKIE_SIZE);
		       st->counter = 0;
		  }
	     }
	}

	cookie_generate(st, st->icookie, COOKIE_SIZE, NULL, 0);
	st->phase = COOKIE_REQUEST;
	st->lifetime = exchange_timeout + time(NULL);

	bcopy(st->icookie, header->icookie, COOKIE_SIZE);
	bcopy(st->rcookie, header->rcookie, COOKIE_SIZE);

	header->counter = st->counter;		/* set to zero or prev. */

	header->type = COOKIE_REQUEST;

	return 0;
}

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
 * photuris_cookie_response:
 * create a COOKIE_RESPONSE packet; return -1 on failure, 0 on success
 *
 */

#ifndef lint
static char rcsid[] = "$Id: photuris_cookie_response.c,v 1.1 1998/11/14 23:37:26 deraadt Exp $";
#endif

#include <stdio.h>
#include <string.h>
#include "config.h"
#include "photuris.h"
#include "packets.h"
#include "state.h"
#include "cookie.h"
#include "server.h"


/* XXX - on value_request receive we need to set the responder schemes */

int
photuris_cookie_response(struct stateob *st, u_char *buffer, int *size,
			 u_int8_t *icookie, u_int8_t counter,
			 u_int8_t *address, u_int16_t port,
			 u_int8_t *schemes, u_int16_t ssize)
{
	struct cookie_response *header;
	struct stateob tempst;

	if (*size < COOKIE_RESPONSE_MIN + ssize)
	  return -1;	/* buffer not large enough */

	header = (struct cookie_response *) buffer;

        /* Copy list of schemes */
	bcopy(schemes, COOKIE_RESPONSE_SCHEMES(header), ssize);

	/* XXX - There are no state information at this phase */
	bzero((char *)&tempst, sizeof(tempst)); /* Set up temp. state */
	tempst.initiator = 0;                   /* We are the Responder */
	bcopy(icookie, tempst.icookie, COOKIE_SIZE);
	strncpy(tempst.address, address, 15);
	tempst.port = global_port;

      	bcopy(tempst.icookie, header->icookie, COOKIE_SIZE);

	if (st == NULL)
	     tempst.counter = counter + 1;
	else
	     tempst.counter = st->counter + 1;
	
	if (tempst.counter == 0)
	     tempst.counter = 1;

	cookie_generate(&tempst, header->rcookie, COOKIE_SIZE, schemes, ssize);

	header->counter = tempst.counter;		

	header->type = COOKIE_RESPONSE;

	*size = COOKIE_RESPONSE_MIN + ssize;
	return 0;
}

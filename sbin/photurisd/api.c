/*	$OpenBSD: api.c,v 1.6 2001/11/30 20:31:49 provos Exp $	*/

/*
 * Copyright 1997-2000 Niels Provos <provos@citi.umich.edu>
 * All rights reserved.
 *
 * Parts derived from code by Angelos D. Keromytis, kermit@forthnet.gr
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
 * This is an experimental implementation of the Photuris Session Key Management
 * Protocol, as of draft-ietf-ipsec-photuris-06.txt.
 *
 * The usual disclaimers/non-guarantees etc. etc. apply.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: api.c,v 1.6 2001/11/30 20:31:49 provos Exp $";
#endif

#define _API_C_

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "state.h"
#include "photuris.h"
#include "config.h"
#include "api.h"
#include "log.h"
#include "buffer.h"
#include "schedule.h"
#include "server.h"
#include "packet.h"

int
start_exchange(int sd, struct stateob *st, char *address, int port)
{
     struct sockaddr_in sin;

     /* Now fill it in */
     strncpy(st->address, address, 15);
     st->address[15] = '\0';
     st->port = port;
     st->initiator = 1;
	     

     /* Determine sender address before we invalidate buffer */
     sin.sin_addr.s_addr = inet_addr(st->address);
     sin.sin_port = htons(st->port);
     sin.sin_family = AF_INET;

     packet_size = PACKET_BUFFER_SIZE;
     if (photuris_cookie_request(st, packet_buffer, &packet_size) == -1) {
	  log_print("photuris_cookie_request() in start_exchange() "
		    "for %s:%d", st->address, st->port);
	  return -1;
     }

     /* Save the packets for later retransmits */
     packet_save(st, packet_buffer, packet_size);

     if (sendto(sd, packet_buffer, packet_size, 0, 
		(struct sockaddr *) &sin, sizeof(sin)) != packet_size) {
	  /* XXX Code to notify kernel of failure */
	  log_error("sendto() in start_exchange() for %s:%d",
		    st->address, st->port);
	  return -1;
     }

     schedule_insert(TIMEOUT, retrans_timeout, st->icookie, COOKIE_SIZE);
     
     return 0;
}

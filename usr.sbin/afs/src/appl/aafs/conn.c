/*
 * Copyright (c) 2002, Stockholms Universitet
 * (Stockholm University, Stockholm Sweden)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the university nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <netinet/in.h>

#include <aafs/aafs_private.h>
#include <aafs/aafs_cell.h>
#include <aafs/aafs_conn.h>

#include <rx/rx.h>

#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>

#include <roken.h>

#include <atypes.h>

struct rx_connection *
aafs_conn_byaddr(struct aafs_cell *cell,
		 struct sockaddr *so,
		 socklen_t slen,
		 short port,
		 uint32_t service)
{
    uint32_t addr;
    struct rx_securityClass *secobj;
    int secidx;

    if (so->sa_family != PF_INET)
	return NULL;

    addr = ((struct sockaddr_in *)so)->sin_addr.s_addr;

    aafs_cell_rx_secclass(cell, &secobj, &secidx);

    return rx_NewConnection(addr, port, service, secobj, secidx);
}

struct rx_connection *
aafs_conn_byserver(struct aafs_server *server,
		   short port,
		   uint32_t service)
{
    struct rx_connection *conn;
    int i;

    if ((server->flags & SERVER_HAVE_ADDR) == 0)
	return NULL;

    if (server->num_addr <= 0)
	return NULL;

    for (i = 0; i < server->num_addr; i++) {
	conn = aafs_conn_byaddr(server->cell, 
				(struct sockaddr *)&server->addr[i].addr,
				server->addr[i].addrlen,
				port, service);
	if (conn)
	    return conn;
    }
    return NULL;
}

struct rx_connection *
aafs_conn_byname(struct aafs_cell *cell,
		 const char *hostname,
		 short port,
		 uint32_t service)
{
    struct addrinfo hints, *res, *res0;
    struct rx_connection *conn = NULL;
    int error;

    memset (&hints, 0, sizeof(hints));
    hints.ai_family = 0;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    error = getaddrinfo(hostname, NULL, &hints, &res0);
    if (error) {
	fprintf (stderr, "Cannot find host %s\n", hostname);
	return  NULL;
    }

    for(res = res0; res != NULL; res = res->ai_next) {
	switch(res->ai_family) {
#ifdef RX_SUPPORTS_INET6
	case PF_INET6:
#endif
	case PF_INET:
	    conn = aafs_conn_byaddr(cell, res->ai_addr, res->ai_addrlen,
				    port, service);
	    break;
	}
	if (conn)
	    break;
    }

    freeaddrinfo(res0);

    return conn;
}

void
aafs_conn_free(struct rx_connection *conn)
{
    free(conn);
}

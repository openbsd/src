/*	$OpenBSD: conn.h,v 1.1.1.1 1998/09/14 21:52:55 art Exp $	*/
/*
 * Copyright (c) 1995, 1996, 1997, 1998 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the Kungliga Tekniska
 *      Högskolan and its contributors.
 * 
 * 4. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Header for connection cache
 */

/* $KTH: conn.h,v 1.11 1998/02/19 05:39:03 assar Exp $ */

#ifndef _CONN_H_
#define _CONN_H_

#include <stdio.h>
#include <xfs/xfs_message.h>
#include <cred.h>

typedef struct {
    u_int32_t host;		/* IP address of host */
    u_int16_t port;		/* port number at host */
    u_int16_t service;		/* RX service # */
    int32_t cell;		/* cell of host */
    int securityindex;
    pag_t cred;
    struct rx_connection *connection;
    struct {
	unsigned alivep : 1;
	unsigned killme : 1;
    } flags;
    unsigned refcount;
} ConnCacheEntry;

void
conn_init (unsigned nentries);

ConnCacheEntry *
conn_get (int32_t cell, u_int32_t host, u_int16_t port, u_int16_t service,
	  CredCacheEntry *ce);

void
conn_free (ConnCacheEntry *e);

int32_t
conn_host2cell (u_int32_t host, u_int16_t port, u_int16_t service);

Bool
conn_serverupp (u_int32_t host, u_int16_t port, u_int16_t service);

void
conn_status (FILE *);

void
conn_clearcred (int32_t cell, pag_t cred, int securityindex);

#endif /* _CONN_H_ */

/*
 * Copyright (c) 1995 - 2002 Kungliga Tekniska Högskolan
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
 * 3. Neither the name of the Institute nor the names of its contributors
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

/* $arla: conn.h,v 1.31 2002/09/07 10:42:57 lha Exp $ */

#ifndef _CONN_H_
#define _CONN_H_

#include <stdio.h>
#include <nnpfs/nnpfs_message.h>
#include <cred.h>

struct conncacheentry {
    uint32_t host;		/* IP address of host */
    uint16_t port;		/* port number at host */
    uint16_t service;		/* RX service # */
    int32_t cell;		/* cell of host */
    int securityindex;
    int (*probe)(struct rx_connection *);
    nnpfs_pag_t cred;
    struct rx_connection *connection;
    struct {
	unsigned alivep : 1;
	unsigned killme : 1;
	unsigned old : 1;	/* Old server,vldb -> only VL_GetEntryByName */
    } flags;
    unsigned refcount;
    Listitem *probe_le;
    unsigned probe_next;
    unsigned ntries;
    struct conncacheentry *parent;
    int rtt;
};

typedef struct conncacheentry ConnCacheEntry;

typedef enum { CONN_CS_NONE = 0,
	       CONN_CS_CELL = 1, 
	       CONN_CS_CRED = 2, 
	       CONN_CS_SECIDX = 4,
	       CONN_CS_ALL = 0xffff
	       
} clear_state_mask;

extern int conn_rxkad_level;

void
conn_init (unsigned nentries);

ConnCacheEntry *
conn_get (int32_t cell, uint32_t host, uint16_t port, uint16_t service,
	  int (*probe)(struct rx_connection *),
	  CredCacheEntry *ce);

void
conn_dead (ConnCacheEntry *);

void
conn_alive (ConnCacheEntry *);

void
conn_probe (ConnCacheEntry *);

void
conn_ref (ConnCacheEntry *e);

void
conn_free (ConnCacheEntry *e);

int32_t
conn_host2cell (uint32_t host, uint16_t port, uint16_t service);

Bool
conn_serverupp (uint32_t host, uint16_t port, uint16_t service);

void
conn_status (void);

void
conn_clearcred (clear_state_mask mask,
		int32_t cell, nnpfs_pag_t cred, int securityindex);

void
conn_downhosts(int32_t cell, uint32_t *hosts, int *num, int flags);

int
conn_rtt_cmp (const void *v1, const void *v2);

Bool
host_downp (int error);

Bool
conn_isalivep (ConnCacheEntry *e);

/*
 * Random factor to add to rtts when comparing them.
 * This is in microseconds/8
 */

static const int RTT_FUZZ = 400000;

#endif /* _CONN_H_ */

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

#include <arla_local.h>

enum connected_mode connected_mode = CONNECTED;

static CredCacheEntry dummy_cred;

CredCacheEntry *
cred_get (long cell, nnpfs_pag_t cred, int type)
{
    return &dummy_cred;
}

void
cred_free (CredCacheEntry *ce)
{
    assert(&dummy_cred == ce);
}

int
fs_probe (struct rx_connection *conn)
{
    return 0;
}

static ConnCacheEntry dummy_cce;

ConnCacheEntry *
conn_get (int32_t cell, uint32_t host, uint16_t port, uint16_t service,
	  int (*probe)(struct rx_connection *),
	  CredCacheEntry *ce)
{
    return &dummy_cce;
}

void
conn_free (ConnCacheEntry *e)
{
    assert(&dummy_cce == e);
}

Bool
conn_isalivep (ConnCacheEntry *e)
{
    return TRUE;
}

int
volcache_getname (uint32_t id, int32_t cell,
		  char *name, size_t name_sz)
{
    snprintf(name, name_sz, "dummy");
    return 0;
}

int
main(int argc, char **argv)
{
    ConnCacheEntry cce;
    struct rx_connection conn;
    struct rx_peer peer;
    unsigned long number;
    void *pe, *pe2;
    PROCESS p;

    LWP_InitializeProcessSupport(4, &p);
    arla_loginit("/dev/stdout", 0);
    cell_init(0, arla_log_method);
    poller_init();

    memset(&cce, 0, sizeof(cce));
    memset(&conn, 0, sizeof(conn));
    memset(&peer, 0, sizeof(peer));
    
    conn.peer = &peer;
    cce.connection = &conn;

    printf("add\n");
    number = 1000000;
    while(number--) {
	pe = poller_add_conn(&cce);
    }
    poller_remove(pe);

    printf("add-remove\n");
    number = 1000000;
    while(number--) {
	pe = poller_add_conn(&cce);
	poller_remove(pe);
    }

    printf("add-add-remove-remove-remove\n");
    pe = NULL;
    number = 1000000;
    while(number--) {
	pe = poller_add_conn(&cce);
	pe2 = poller_add_conn(&cce);
	if (pe != pe2)
		exit(-1);
	poller_remove(pe);
	poller_remove(pe2);
    }
    return 0;
}

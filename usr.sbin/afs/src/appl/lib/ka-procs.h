/*
 * Copyright (c) 2000 Kungliga Tekniska Högskolan
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
 * $arla: ka-procs.h,v 1.4 2002/05/16 22:09:43 hin Exp $
 */

#ifndef _ARLA_KA_PROCS_H
#define _ARLA_KA_PROCS_H 1

#include <ka.h>

struct ka_Answer {
    uint32_t challange;
    des_cblock sessionkey;
    uint32_t start_time;
    uint32_t end_time;
    int kvno;
    char user[MAXKANAMELEN];
    char instance[MAXKANAMELEN];
    char realm[MAXKANAMELEN];
    char server_user[MAXKANAMELEN];
    char server_instance[MAXKANAMELEN];
    KTEXT_ST ticket;
    char label[KA_LABELSIZE];
};

typedef struct ka_Answer ka_auth_data_t;
typedef struct ka_Answer ka_ticket_data_t;

/*
 * `ka_cell_query' will be used to enable the interface to query
 * different servers then in the cell database.
 */

struct ka_cell_query {
    int foo;
};

/*
 *
 */

typedef enum  { KA_AUTH_TICKET = 1,
		KA_AUTH_TOKEN = 2 } ka_auth_flags_t;

int
ka_authenticate (const char *user, const char *instance, const char *cell,
		 const char *password, uint32_t lifetime,
		 ka_auth_flags_t flags);

int
ka_auth (const char *user, const char *instance, const char *cell,
	 des_cblock *key, ka_auth_data_t *adata,
	 uint32_t lifetime, struct ka_cell_query *cinfo);

int
ka_getticket (const char *suser, const char *sinstance, const char *srealm,
	      ka_auth_data_t *adata, ka_ticket_data_t *tdata,
	      struct ka_cell_query *cinfo);

/*
 * store requests to disk in form of a kerberos v4 ticket.
 */

int
ka_auth_create (char *filename, ka_auth_data_t *data);

int
ka_write_ticket (char *filename, ka_ticket_data_t *data);

#endif /* _ARLA_KA_PROCS_H */

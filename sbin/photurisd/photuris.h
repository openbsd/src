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
/* $Id: photuris.h,v 1.1 1998/11/14 23:37:26 deraadt Exp $ */
/*
 * photuris.h:
 * general header file
 */
 
#ifndef _PHOTURIS_H_
#define _PHOTURIS_H_

#include "state.h"

#undef EXTERN
#ifdef _PHOTURIS_C_
#define EXTERN
#else
#define EXTERN extern
#endif

#define PHOTURIS_DIR         "/etc/photuris"
#define PHOTURIS_FIFO        "photuris.pipe"
#define PHOTURIS_STARTUP     "photuris.startup"
#define PHOTURIS_CONFIG      "photuris.conf"
#define PHOTURIS_SECRET      "secrets.conf"
#define PHOTURIS_USER_SECRET ".photuris_secrets"
#define PHOTURIS_ATTRIB      "attributes.conf"

EXTERN char *config_file;
EXTERN char *attrib_file;
EXTERN u_int8_t *global_schemes;
EXTERN u_int16_t global_schemesize;
EXTERN int max_retries;
EXTERN int retrans_timeout;
EXTERN int exchange_timeout;
EXTERN int exchange_lifetime;
EXTERN int spi_lifetime;
EXTERN int vpn_mode;

EXTERN int daemon_mode;

/* Infos about our interfaces */
EXTERN char **addresses;
EXTERN int *sockets;
EXTERN int num_ifs;


/* Packet creation functions */

int photuris_cookie_request(struct stateob *, u_char *, int *);
int photuris_cookie_response(struct stateob *, u_char *, int *, u_int8_t *, 
		   u_int8_t, u_int8_t *, u_int16_t, u_int8_t *, u_int16_t);
int photuris_value_request(struct stateob *, u_char *, int *);
int photuris_value_response(struct stateob *, u_char *, int *);
int photuris_identity_request(struct stateob *, u_char *, int *);
int photuris_identity_response(struct stateob *, u_char *, int *);
int photuris_spi_update(struct stateob *, u_char *, int *);
int photuris_spi_needed(struct stateob *, u_char *, int *, u_int8_t *, 
			u_int16_t);
int photuris_error_message(struct stateob *, u_char *, int *, char *, char *, 
			   u_int8_t, u_int8_t);

/* Packet handling functions */

int handle_cookie_request(u_char *, int, u_int8_t *, u_int16_t, u_int8_t *, u_int16_t);
int handle_cookie_response(u_char *, int , char *, int);
int handle_value_request(u_char *, int, char *, u_short, u_int8_t *, u_int16_t );
int handle_value_response(u_char *, int , char *, char *);
int handle_identity_request(u_char *, int , char *, char *);
int handle_identity_response(u_char *, int, char *, char *);
int handle_spi_needed(u_char *, int , char *, char *);
int handle_spi_update(u_char *, int, char *, char *);
int handle_bad_cookie(u_char *, int, char *);
int handle_resource_limit(u_char *, int, char *);
int handle_verification_failure(u_char *, int, char *);
int handle_message_reject(u_char *, int, char *);

#if defined(DEBUG) && !defined(IPSEC)
#define PHOTURIS_PORT 7468
#else
#define PHOTURIS_PORT 468
#endif

#endif /* _PHOTURIS_H */

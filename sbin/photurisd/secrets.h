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
/* $Id: secrets.h,v 1.1 1998/11/14 23:37:28 deraadt Exp $ */
/*
 * secrets.h:
 * prototypes for compute_secrets.c
 */

#ifndef _SECRETS_H_ 
#define _SECRETS_H_ 
 
#include "state.h" 
#include "spi.h"
#include "gmp.h"
 
#undef EXTERN 
  
#ifdef _SECRETS_C_ 
#define EXTERN 
#else 
#define EXTERN extern 
#endif

EXTERN int compute_shared_secret(struct stateob *, u_int8_t **, u_int16_t *);
EXTERN int compute_session_key(struct stateob *st, u_int8_t *key, 
			       u_int8_t *attribute, int owner, 
			       u_int16_t *order);
EXTERN int get_session_key_length(u_int8_t *attribute);

EXTERN int init_privacy_key(struct stateob *st, int owner);
EXTERN int compute_privacy_key(struct stateob *st, u_int8_t *key, 
			       u_int8_t *packet, u_int16_t bits, 
			       u_int16_t *order, int owner);
EXTERN int make_session_keys(struct stateob *st, struct spiob *spi);

#endif /* _SECRETS_H_ */

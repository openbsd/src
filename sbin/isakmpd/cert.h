/*	$OpenBSD: cert.h,v 1.4 1999/07/17 21:54:39 niklas Exp $	*/
/*	$EOM: cert.h,v 1.6 1999/07/17 20:44:09 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niels Provos.  All rights reserved.
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
 *	This product includes software developed by Ericsson Radio Systems.
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
 * This code was written under funding by Ericsson Radio Systems.
 */

#ifndef _CERT_H_
#define _CERT_H_

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>

/* 
 * CERT handler for each kind of certificate:
 *
 * cert_init - Initialize CERT handler - called only once
 * cert_get  - Get a certificate in internal representation from raw data
 * cert_validate - validated a certificate, if it returns != 0 we can use it.
 * cert_insert - inserts cert into memory storage, we can retrieve with
 *               cert_obtain.
 */

struct cert_handler {
  u_int16_t id;				/* ISAKMP Cert Encoding ID */
  int (*cert_init) (void);		
  void *(*cert_get) (u_int8_t *, u_int32_t); 
  int (*cert_validate) (void *);
  int (*cert_insert) (void *);
  void (*cert_free) (void *);
  int (*certreq_validate) (u_int8_t *, u_int32_t);
  void *(*certreq_decode) (u_int8_t *, u_int32_t);
  void (*free_aca) (void *);
  int (*cert_obtain) (u_int8_t *, size_t, void *, u_int8_t **, u_int32_t *);
  int (*cert_get_key) (void *, void *);
  int (*cert_get_subject) (void *, u_int8_t **, u_int32_t *);
};

/* the acceptable authority of cert request */

struct certreq_aca {
  TAILQ_ENTRY (certreq_aca) link;

  u_int16_t id;
  struct cert_handler *handler;
  void *data;			/* if NULL everything is acceptable */
};

struct cert_handler *cert_get (u_int16_t);
struct certreq_aca *certreq_decode (u_int16_t, u_int8_t *, u_int32_t);
int cert_init (void);

#endif /* _CERT_H_ */

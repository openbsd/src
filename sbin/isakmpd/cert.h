/*	$OpenBSD: cert.h,v 1.3 1998/11/17 11:10:08 niklas Exp $	*/
/*	$EOM: cert.h,v 1.5 1998/08/21 13:47:51 provos Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
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

struct exchange;

struct cert_handler {
  u_int16_t id;				/* ISAKMP Cert Encoding ID */
  int (*certreq_validate) (u_int8_t *, u_int32_t);
  void *(*certreq_decode) (u_int8_t *, u_int32_t);
  void (*free_aca) (void *);
  int (*cert_obtain) (struct exchange *, void *, u_int8_t **, u_int32_t *);
  int (*cert_get_key) (u_int8_t *, u_int32_t, void *);
  int (*cert_get_subject) (u_int8_t *, u_int32_t, u_int8_t **, u_int32_t *);
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

#endif /* _CERT_H_ */

/*	$OpenBSD: x509.h,v 1.6 2000/02/01 02:46:19 niklas Exp $	*/
/*	$EOM: x509.h,v 1.9 2000/01/31 22:33:49 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niels Provos.  All rights reserved.
 * Copyright (c) 1999 Angelos D. Keromytis.  All rights reserved.
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

#ifndef _X509_H_
#define _X509_H_

#include "libcrypto.h"

#define X509v3_RFC_NAME		1
#define X509v3_DNS_NAME		2
#define X509v3_IPV4_ADDR	7


struct x509_attribval {
  char *type;
  char *val;
};

/*
 * The acceptable certification authority. 
 * XXX We only support two names at the moment, as of ASN this can
 * be dynamic but we don't care for now.
 */

struct x509_aca {
  struct x509_attribval name1;
  struct x509_attribval name2;
};

struct X509;

/* Functions provided by cert handler.  */

int x509_cert_init (void);
void *x509_cert_get (u_int8_t *, u_int32_t);
int x509_cert_validate (void *);
void x509_cert_free (void *);
int x509_certreq_validate (u_int8_t *, u_int32_t);
void *x509_certreq_decode (u_int8_t *, u_int32_t);
void x509_free_aca (void *);
int x509_cert_obtain (u_int8_t *, size_t, void *, u_int8_t **, u_int32_t *);
int x509_cert_get_key (void *, void *);
int x509_cert_get_subject (void *, u_int8_t **, u_int32_t *);

/* Misc. X509 certificate functions.  */

int x509_cert_insert (void *);
int x509_read_from_dir (X509_STORE *, char *, int);

int x509_cert_subjectaltname (X509 *cert, u_char **, u_int *);
int x509_check_subjectaltname (u_char *, u_int, X509 *);
X509 *x509_from_asn (u_char *, u_int);

int x509_generate_kn(X509 *);
#endif /* _X509_H_ */

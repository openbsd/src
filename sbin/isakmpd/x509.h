/*	$Id: x509.h,v 1.1.1.1 1998/11/15 00:03:49 niklas Exp $	*/

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

#ifndef _X509_H_
#define _X509_H_

#include "pkcs.h"		/* for struct rsa_public_key */

struct x509_attribval {
  char *type;
  char *val;
};

/*
 * The acceptable certification authority 
 * XXX we only support two names at the moment, as of ASN this can
 * be dynamic but we dont care for now.
 */

struct x509_aca {
  struct x509_attribval name1;
  struct x509_attribval name2;
};

struct exchange;

struct x509_certificate {
  u_int32_t version;
  u_int32_t serialnumber;
  char *signaturetype;
  struct x509_attribval issuer1;	/* At the moment Country */
  struct x509_attribval issuer2;	/* At the moment Organization  */
  struct x509_attribval subject1;	/* At the moment Country */
  struct x509_attribval subject2;	/* At the moment Organization  */
  struct x509_attribval extension;	/* Raw Extension */
  char *start;       		/* Certificate Validity Start and End */
  char *end;
  struct rsa_public_key key;
};

int x509_certreq_validate (u_int8_t *, u_int32_t);
void *x509_certreq_decode (u_int8_t *, u_int32_t);
void x509_free_aca (void *);
int x509_cert_obtain (struct exchange *, void *, u_int8_t **, u_int32_t *);
int x509_cert_get_key (u_int8_t *, u_int32_t, void *);
int x509_cert_get_subject (u_int8_t *, u_int32_t, u_int8_t **, u_int32_t *);

void x509_get_attribval (struct norm_type *, struct x509_attribval *);
void x509_set_attribval (struct norm_type *, struct x509_attribval *);
void x509_free_attrbival (struct x509_attribval *);

int x509_validate_signed (u_int8_t *, u_int32_t, struct rsa_public_key *,
			  u_int8_t **, u_int32_t *);
int x509_create_signed (u_int8_t *, u_int32_t, struct rsa_private_key *,
			u_int8_t **, u_int32_t *);
int x509_decode_certificate (u_int8_t *, u_int32_t, struct x509_certificate *);
int x509_encode_certificate (struct x509_certificate *, u_int8_t **,
			     u_int32_t *);
void x509_free_certificate (struct x509_certificate *);

int x509_decode_cert_extension (u_int8_t *, u_int32_t, 
				struct x509_certificate *);
int x509_encode_cert_extension (struct x509_certificate *, u_int8_t **,
				u_int32_t *);

#endif /* _X509_H_ */

/*	$OpenBSD: pkcs.h,v 1.5 1999/04/19 19:54:54 niklas Exp $	*/
/*	$EOM: pkcs.h,v 1.8 1999/04/02 00:58:05 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 * Copyright (c) 1999 Niklas Hallqvist.  All rights reserved.
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

#ifndef _PKCS_H_
#define _PKCS_H_

#include <gmp.h>

#define PKCS_PRIVATE	1	/* Private Key Encryption */
#define PKCS_PUBLIC	2	/* Public Key Encryption */

struct rsa_public_key {
  mpz_t n;		/* Group Modulus */
  mpz_t e;		/* Public Exponent */
};

struct rsa_private_key {
  mpz_t n;		/* Group Modulus */
  mpz_t p;		/* Prime p */
  mpz_t q;		/* Prime q */
  mpz_t d1;		/* d mod (p - 1) */
  mpz_t d2;		/* d mod (q - 1) */
  mpz_t e;		/* Public Exponent */
  mpz_t d;		/* Private Exponent */
  mpz_t qinv;		/* inversion of q modulo p */
  mpz_t qinv_mul_q;     /* qinv mul q */
};

struct norm_type;

int pkcs_mpz_to_norm_type (struct norm_type *obj, mpz_ptr n);

u_int8_t *pkcs_public_key_to_asn (struct rsa_public_key *);
int pkcs_public_key_from_asn (struct rsa_public_key *, u_int8_t *, u_int32_t);
void pkcs_free_public_key (struct rsa_public_key *);

u_int8_t *pkcs_private_key_to_asn (struct rsa_private_key *);
int pkcs_private_key_from_asn (struct rsa_private_key *, u_int8_t *,
			       u_int32_t);
void pkcs_free_private_key (struct rsa_private_key *);

int pkcs_rsa_encrypt (int, struct rsa_public_key *, struct rsa_private_key *,
		u_int8_t *, u_int32_t, u_int8_t **, u_int32_t *);
int pkcs_rsa_decrypt (int, struct rsa_public_key *, struct rsa_private_key *,
		u_int8_t *, u_int8_t **, u_int16_t *);

int pkcs_generate_rsa_keypair (struct rsa_public_key *, 
			       struct rsa_private_key *, u_int32_t);
int pkcs_generate_prime (mpz_ptr, u_int32_t);

#endif /* _PKCS_H_ */

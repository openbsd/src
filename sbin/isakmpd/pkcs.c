/*	$OpenBSD: pkcs.c,v 1.4 1998/11/16 21:07:17 niklas Exp $	*/

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

#include <sys/param.h>
#include <gmp.h>
#include <stdlib.h>
#include <string.h>

#include "gmp_util.h"
#include "log.h"
#include "sysdep.h"
#include "asn.h"
#include "asn_useful.h"
#include "pkcs.h"

struct norm_type RSAPublicKey[] = {
  { TAG_INTEGER, UNIVERSAL, "modulus", 0, NULL},	/* modulus */
  { TAG_INTEGER, UNIVERSAL, "publicExponent", 0, NULL},	/* public exponent */
  { TAG_STOP, 0, NULL, 0, NULL}
};

struct norm_type RSAPrivateKey[] = {
  { TAG_INTEGER, UNIVERSAL, "version", 1, "\0"},	/* version */
  { TAG_INTEGER, UNIVERSAL, "modulus", 0, NULL},	/* modulus */
  { TAG_INTEGER, UNIVERSAL, "publicExponent", 0, NULL},	/* public exponent */
  { TAG_INTEGER, UNIVERSAL, "privateExponent", 0, NULL},/* private exponent */
  { TAG_INTEGER, UNIVERSAL, "prime1", 0, NULL},		/* p */
  { TAG_INTEGER, UNIVERSAL, "prime2", 0, NULL},		/* q */
  { TAG_INTEGER, UNIVERSAL, "exponent1", 0, NULL},	/* d mod (p-1) */
  { TAG_INTEGER, UNIVERSAL, "exponent2", 0, NULL},	/* d mod (q-1) */
  { TAG_INTEGER, UNIVERSAL, "coefficient", 0, NULL},	/* inv. of q mod p */
  { TAG_STOP, 0, NULL, 0, NULL}
};

/*
 * Fill in the data field in struct norm_type with the octet data
 * from n.
 */

int
pkcs_mpz_to_norm_type (struct norm_type *obj, mpz_ptr n)
{
  obj->len = sizeof (mpz_ptr);
  if ((obj->data = malloc (obj->len)) == NULL)
    return 0;

  mpz_init_set ((mpz_ptr) obj->data, n);

  return 1;
}

/*
 * Given the modulus and the public key, return an BER ASN.1 encoded
 * PKCS#1 compliant RSAPublicKey object.
 */

u_int8_t *
pkcs_public_key_to_asn (struct rsa_public_key *pub)
{
  u_int8_t *erg;
  struct norm_type *key, seq = {TAG_SEQUENCE, UNIVERSAL, NULL, 0, NULL};

  seq.data = &RSAPublicKey;
  asn_template_clone (&seq, 1);
  key = seq.data;
  if (key == NULL)
    return NULL;

  if (!pkcs_mpz_to_norm_type (&key[0], pub->n))
    {
      free (key);
      return NULL;
    }

  if (!pkcs_mpz_to_norm_type (&key[1], pub->e))
    {
      free (key[0].data); 
      free (key);
      return NULL;
    }

  erg = asn_encode_sequence (&seq, NULL);

  asn_free (&seq);

  return erg;
}

/*
 * Initalizes and Set's a Public Key Structure from an ASN BER encoded
 * Public Key.
 */

int
pkcs_public_key_from_asn (struct rsa_public_key *pub, u_int8_t *asn,
			  u_int32_t len)
{
  struct norm_type *key, seq = {TAG_SEQUENCE, UNIVERSAL, NULL, 0, NULL};

  mpz_init (pub->n);
  mpz_init (pub->e);

  seq.data = RSAPublicKey;
  asn_template_clone (&seq, 1);

  if (seq.data == NULL)
    return 0;

  if (asn_decode_sequence (asn, len, &seq) == NULL)
    {
      asn_free (&seq);
      return 0;
    }

  key = seq.data;
  mpz_set (pub->n, (mpz_ptr) key[0].data);
  mpz_set (pub->e, (mpz_ptr) key[1].data);

  asn_free (&seq);
      
  return 1;
}

void
pkcs_free_public_key (struct rsa_public_key *pub)
{
  mpz_clear (pub->n);
  mpz_clear (pub->e);
}

/*
 * Get ASN.1 representation of PrivateKey.
 * XXX - not sure if we need this.
 */

u_int8_t *
pkcs_private_key_to_asn (struct rsa_private_key *priv)
{
  mpz_t d1, d2, qinv;
  struct norm_type *key, seq = {TAG_SEQUENCE, UNIVERSAL, NULL, 0, NULL};
  u_int8_t *erg = NULL;

  seq.data = RSAPrivateKey;
  asn_template_clone (&seq, 1);
  key = seq.data;
  if (key == NULL)
    return NULL;

  mpz_init (d1);
  mpz_sub_ui (d1, priv->p, 1);
  mpz_mod (d1, priv->d, d1);

  mpz_init (d2);
  mpz_sub_ui (d2, priv->q, 1);
  mpz_mod (d2, priv->d, d2);

  mpz_init (qinv);
  mpz_invert (qinv, priv->q, priv->p);

  if (!pkcs_mpz_to_norm_type (&key[1], priv->n))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[2], priv->e))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[3], priv->d))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[4], priv->p))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[5], priv->q))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[6], d1))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[7], d2))
    goto done;

  if (!pkcs_mpz_to_norm_type (&key[8], qinv))
    goto done;

  mpz_set_ui (d1, 0);

  if (!pkcs_mpz_to_norm_type (&key[0], d1))
    goto done;

  erg = asn_encode_sequence (&seq, NULL);

 done:
  asn_free (&seq);

  mpz_clear (d1);
  mpz_clear (d2);
  mpz_clear (qinv);

  return erg;
}

/*
 * Initalizes and Set's a Private Key Structure from an ASN BER encoded
 * Private Key.
 */

int
pkcs_private_key_from_asn (struct rsa_private_key *priv, u_int8_t *asn,
			   u_int32_t len)
{
  struct norm_type *key, seq = {TAG_SEQUENCE, UNIVERSAL, NULL, 0, NULL};
  u_int8_t *erg;

  mpz_init (priv->n);
  mpz_init (priv->p);
  mpz_init (priv->q);
  mpz_init (priv->e);
  mpz_init (priv->d);

  seq.data = RSAPrivateKey;
  asn_template_clone (&seq, 1);
  if (seq.data == NULL)
    return 0;

  if (!(erg = asn_decode_sequence (asn, len, &seq)))
    goto done;

  key = seq.data;
  if (mpz_cmp_ui ((mpz_ptr) key[0].data, 0))
    {
      log_print ("pkcs_set_private_key: version too high");
      erg = 0;
      goto done;
    }

  mpz_set (priv->n, key[1].data);
  mpz_set (priv->e, key[2].data);
  mpz_set (priv->d, key[3].data);
  mpz_set (priv->p, key[4].data);
  mpz_set (priv->q, key[5].data);

 done:
  asn_free (&seq);

  return erg == NULL ? 0 : 1;
}

void
pkcs_free_private_key (struct rsa_private_key *priv)
{
  mpz_clear (priv->n);
  mpz_clear (priv->e);
  mpz_clear (priv->d);
  mpz_clear (priv->p);
  mpz_clear (priv->q);
}

/*
 * Creates a PKCS#1 block with data and then uses the private
 * exponent to do RSA encryption, returned is an allocated buffer
 * with the encryption result.
 *
 * XXX CRIPPLED in the OpenBSD version as RSA is patented in the US.
 */
int
pkcs_rsa_encrypt (int art, mpz_ptr n, mpz_ptr e, u_int8_t *data, u_int32_t len,
		  u_int8_t **out, u_int32_t *outlen)
{
  /* XXX Always fail until we interface legal (in the US) RSA code.  */
  return 0;
}

/*
 * Private Key Decryption, the 'in'-buffer is being destroyed 
 *
 * XXX CRIPPLED in the OpenBSD version as RSA is patented in the US.
 */
int
pkcs_rsa_decrypt (int art, mpz_ptr n, mpz_ptr d, u_int8_t *in,
		  u_int8_t **out, u_int16_t *outlen)
{
  /* XXX Always fail until we interface legal (in the US) RSA code.  */
  return 0;
}

/*
 * Generates a keypair suitable to be used for RSA. No checks are done
 * on the generated key material. The following criteria might be
 * enforced: p and q chosen randomly, |p-q| should be large, (p+1), (q+1),
 * (p-1), (q-1) should have a large prime factor to be resistant e.g. 
 * against Pollard p-1 and Pollard p+1 factoring algorithms.
 * For p-1 and q-1 the large prime factor itself - 1 should have a large
 * prime factor.
 *
 * XXX CRIPPLED in the OpenBSD version as RSA is patented in the US.
 */
int
pkcs_generate_rsa_keypair (struct rsa_public_key *pubk, 
			   struct rsa_private_key *seck, u_int32_t bits)
{
  /* XXX Always fail until we interface legal (in the US) RSA code.  */
  return 0;
}

/* Generate a random prime with at most bits significant bits */

int
pkcs_generate_prime (mpz_ptr p, u_int32_t bits)
{
  u_int32_t tmp, i;

  mpz_set_ui (p, 0);
  i = tmp = 0;
  while (bits > 0)
    {
      tmp = sysdep_random();

      if (i++ == 0)
	{ 
	  if (bits & 0x1f)
	    tmp &= (1 << (bits & 0x1f)) - 1;
	  tmp |= 1 << ((bits - 1) & 0x1f);
	}

      mpz_mul_2exp (p, p, 32);
      mpz_add_ui (p, p, tmp);

      bits -= (bits & 0x1f ? bits & 0x1f : 32);
    }

  /* Make p odd */
  mpz_setbit (p, 0);

  /* Iterate as long as p is not a probable prime */
  while (!mpz_probab_prime_p (p, 50))
    mpz_add_ui (p, p, 2);

  return 1;
}

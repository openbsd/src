/*	$OpenBSD: crypto.c,v 1.16 2003/08/28 14:43:35 markus Exp $	*/
/*	$EOM: crypto.c,v 1.32 2000/03/07 20:08:51 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niels Provos.  All rights reserved.
 * Copyright (c) 1999, 2000 Niklas Hallqvist.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "crypto.h"
#include "log.h"

enum cryptoerr evp_init (struct keystate *, u_int8_t *, u_int16_t,
    const EVP_CIPHER *);
enum cryptoerr des1_init (struct keystate *, u_int8_t *, u_int16_t);
enum cryptoerr des3_init (struct keystate *, u_int8_t *, u_int16_t);
enum cryptoerr blf_init (struct keystate *, u_int8_t *, u_int16_t);
enum cryptoerr cast_init (struct keystate *, u_int8_t *, u_int16_t);
enum cryptoerr aes_init (struct keystate *, u_int8_t *, u_int16_t);
void evp_encrypt (struct keystate *, u_int8_t *, u_int16_t);
void evp_decrypt (struct keystate *, u_int8_t *, u_int16_t);

struct crypto_xf transforms[] = {
#ifdef USE_DES
  {
    DES_CBC, "Data Encryption Standard (CBC-Mode)", 8, 8, BLOCKSIZE, 0,
    des1_init,
    evp_encrypt, evp_decrypt
  },
#endif
#ifdef USE_TRIPLEDES
  {
    TRIPLEDES_CBC, "Triple-DES (CBC-Mode)", 24, 24, BLOCKSIZE, 0,
    des3_init,
    evp_encrypt, evp_decrypt
  },
#endif
#ifdef USE_BLOWFISH
  {
    BLOWFISH_CBC, "Blowfish (CBC-Mode)", 12, 56, BLOCKSIZE, 0,
    blf_init,
    evp_encrypt, evp_decrypt
  },
#endif
#ifdef USE_CAST
  {
    CAST_CBC, "CAST (CBC-Mode)", 12, 16, BLOCKSIZE, 0,
    cast_init,
    evp_encrypt, evp_decrypt
  },
#endif
#ifdef USE_AES
  {
    AES_CBC, "AES (CBC-Mode)", 16, 32, 2*BLOCKSIZE, 0,
    aes_init,
    evp_encrypt, evp_decrypt
  },
#endif
};

#ifdef USE_DES
enum cryptoerr
des1_init (struct keystate *ks, u_int8_t *key, u_int16_t len)
{
  const EVP_CIPHER *evp;

  evp = EVP_des_cbc();
  return evp_init (ks, key, len, evp);
}
#endif

#ifdef USE_TRIPLEDES
enum cryptoerr
des3_init (struct keystate *ks, u_int8_t *key, u_int16_t len)
{
  const EVP_CIPHER *evp;

  evp = EVP_des_ede3_cbc();
  return evp_init (ks, key, len, evp);
}
#endif

#ifdef USE_BLOWFISH
enum cryptoerr
blf_init (struct keystate *ks, u_int8_t *key, u_int16_t len)
{
  const EVP_CIPHER *evp;

  evp = EVP_bf_cbc();
  return evp_init (ks, key, len, evp);
}
#endif

#ifdef USE_CAST
enum cryptoerr
cast_init (struct keystate *ks, u_int8_t *key, u_int16_t len)
{
  const EVP_CIPHER *evp;

  evp = EVP_cast5_cbc();
  return evp_init (ks, key, len, evp);
}
#endif

#ifdef USE_AES
enum cryptoerr
aes_init (struct keystate *ks, u_int8_t *key, u_int16_t len)
{
  const EVP_CIPHER *evp;

  switch (8 * len)
    {
    case 128:
      evp = EVP_aes_128_cbc();
      break;
    case 192:
      evp = EVP_aes_192_cbc();
      break;
    case 256:
      evp = EVP_aes_256_cbc();
      break;
    default:
      return EKEYLEN;
    }
  return evp_init (ks, key, len, evp);
}
#endif

enum cryptoerr
evp_init (struct keystate *ks, u_int8_t *key, u_int16_t len, const EVP_CIPHER *evp)
{
  EVP_CIPHER_CTX_init(&ks->ks_evpenc);
  EVP_CIPHER_CTX_init(&ks->ks_evpdec);

  if (EVP_CIPHER_key_length(evp) != len
      && !(EVP_CIPHER_flags(evp) & EVP_CIPH_VARIABLE_LENGTH))
    return EKEYLEN;
  if (EVP_CipherInit(&ks->ks_evpenc, evp, key, NULL, 1) <= 0)
    return EKEYLEN;
  if (EVP_CipherInit(&ks->ks_evpdec, evp, key, NULL, 0) <= 0)
    return EKEYLEN;
  return EOKAY;
}

void
evp_encrypt (struct keystate *ks, u_int8_t *data, u_int16_t len)
{
  (void) EVP_CipherInit(&ks->ks_evpenc, NULL, NULL, ks->riv, -1);
  EVP_Cipher(&ks->ks_evpenc, data, data, len);
}

void
evp_decrypt (struct keystate *ks, u_int8_t *data, u_int16_t len)
{
  (void) EVP_CipherInit(&ks->ks_evpdec, NULL, NULL, ks->riv, -1);
  EVP_Cipher(&ks->ks_evpdec, data, data, len);
}

struct crypto_xf *
crypto_get (enum transform id)
{
  int i;

  for (i = 0; i < sizeof transforms / sizeof transforms[0]; i++)
    if (id == transforms[i].id)
      return &transforms[i];

  return 0;
}

struct keystate *
crypto_init (struct crypto_xf *xf, u_int8_t *key, u_int16_t len,
	     enum cryptoerr *err)
{
  struct keystate *ks;

  if (len < xf->keymin || len > xf->keymax)
    {
      LOG_DBG ((LOG_CRYPTO, 10, "crypto_init: invalid key length %d", len));
      *err = EKEYLEN;
      return 0;
    }

  ks = calloc (1, sizeof *ks);
  if (!ks)
    {
      log_error ("crypto_init: calloc (1, %lu) failed",
	(unsigned long)sizeof *ks);
      *err = ENOCRYPTO;
      return 0;
    }

  ks->xf = xf;

  /* Setup the IV.  */
  ks->riv = ks->iv;
  ks->liv = ks->iv2;

  LOG_DBG_BUF ((LOG_CRYPTO, 40, "crypto_init: key", key, len));

  *err = xf->init (ks, key, len);
  if (*err != EOKAY)
    {
      LOG_DBG ((LOG_CRYPTO, 30, "crypto_init: weak key found for %s",
		xf->name));
      free (ks);
      return 0;
    }

  return ks;
}

void
crypto_update_iv (struct keystate *ks)
{
  u_int8_t *tmp;

  tmp = ks->riv;
  ks->riv = ks->liv;
  ks->liv = tmp;

  LOG_DBG_BUF ((LOG_CRYPTO, 50, "crypto_update_iv: updated IV", ks->riv,
		ks->xf->blocksize));
}

void
crypto_init_iv (struct keystate *ks, u_int8_t *buf, size_t len)
{
  memcpy (ks->riv, buf, len);

  LOG_DBG_BUF ((LOG_CRYPTO, 50, "crypto_update_iv: initialized IV", ks->riv,
		len));
}

void
crypto_encrypt (struct keystate *ks, u_int8_t *buf, u_int16_t len)
{
  LOG_DBG_BUF ((LOG_CRYPTO, 10, "crypto_encrypt: before encryption", buf,
		len));
  ks->xf->encrypt (ks, buf, len);
  memcpy (ks->liv, buf + len - ks->xf->blocksize, ks->xf->blocksize);
  LOG_DBG_BUF ((LOG_CRYPTO, 30, "crypto_encrypt: after encryption", buf,
		len));
}

void
crypto_decrypt (struct keystate *ks, u_int8_t *buf, u_int16_t len)
{
  LOG_DBG_BUF ((LOG_CRYPTO, 10, "crypto_decrypt: before decryption", buf,
		len));
  /*
   * XXX There is controversy about the correctness of updating the IV
   * like this.
   */
  memcpy (ks->liv, buf + len - ks->xf->blocksize, ks->xf->blocksize);
  ks->xf->decrypt (ks, buf, len);
  LOG_DBG_BUF ((LOG_CRYPTO, 30, "crypto_decrypt: after decryption", buf,
		len));
}

/* Make a copy of the keystate pointed to by OKS.  */
struct keystate *
crypto_clone_keystate (struct keystate *oks)
{
  struct keystate *ks;

  ks = malloc (sizeof *ks);
  if (!ks)
    {
      log_error ("crypto_clone_keystate: malloc (%lu) failed",
	(unsigned long)sizeof *ks);
      return 0;
    }
  memcpy (ks, oks, sizeof *ks);
  if (oks->riv == oks->iv)
    {
      ks->riv = ks->iv;
      ks->liv = ks->iv2;
    }
  else
    {
      ks->riv = ks->iv2;
      ks->liv = ks->iv;
    }
  return ks;
}

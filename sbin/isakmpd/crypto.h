/*	$OpenBSD: crypto.h,v 1.9 2003/08/28 14:43:35 markus Exp $	*/
/*	$EOM: crypto.h,v 1.12 2000/10/15 21:56:41 niklas Exp $	*/

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

#ifndef _CRYPTO_H_
#define _CRYPTO_H_

#include <openssl/evp.h>

/*
 * This is standard for all block ciphers we use at the moment.
 * Theoretically this could increase in future, e.g. for TwoFish.
 * Keep MAXBLK uptodate
 */
#define BLOCKSIZE	8

#define MAXBLK		(2*BLOCKSIZE)

struct keystate {
  struct crypto_xf *xf;			/* Back pointer */
  u_int16_t	ebytes;			/* Number of encrypted bytes */
  u_int16_t	dbytes;			/* Number of decrypted bytes */
  time_t	life;			/* Creation time */
  u_int8_t	iv[MAXBLK];		/* Next IV to use */
  u_int8_t	iv2[MAXBLK];
  u_int8_t	*riv, *liv;
  struct {
      EVP_CIPHER_CTX enc, dec;
  } evp;
};

#define ks_evpenc	evp.enc
#define ks_evpdec	evp.dec

/*
 * Information about the cryptotransform.
 *
 * XXX - In regards to the IV (Initialization Vector) the drafts are
 * completly fucked up and specify a MUST as how it is derived, so
 * we also have to provide for that. I just don't know where.
 * Furthermore is this enum needed at all?  It seems to be Oakley IDs
 * only anyhow, and we already have defines for that in ipsec_doi.h.
 */
enum transform {
  DES_CBC=1,			/* This is a MUST */
  IDEA_CBC=2,			/* Licensed, DONT use */
  BLOWFISH_CBC=3,
  RC5_R16_B64_CBC=4,		/* Licensed, DONT use */
  TRIPLEDES_CBC=5,			/* This is a SHOULD */
  CAST_CBC=6,
  AES_CBC=7
};

enum cryptoerr {
  EOKAY,			/* No error */
  ENOCRYPTO,			/* A none crypto related error, see errno */
  EWEAKKEY,			/* A weak key was found in key setup */
  EKEYLEN			/* The key length was invalid for the cipher */
};

struct crypto_xf {
  enum transform id;		/* Oakley ID */
  char *name;			/* Transform Name */
  u_int16_t keymin, keymax;	/* Possible Keying Bytes */
  u_int16_t blocksize;		/* Need to keep IV in the state */
  struct keystate *state;	/* Key information, can also be passed sep. */
  enum cryptoerr (*init) (struct keystate *, u_int8_t *, u_int16_t);
  void (*encrypt) (struct keystate *, u_int8_t *, u_int16_t);
  void (*decrypt) (struct keystate *, u_int8_t *, u_int16_t);
};

extern struct keystate *crypto_clone_keystate (struct keystate *);
extern void crypto_decrypt (struct keystate *, u_int8_t *, u_int16_t);
extern void crypto_encrypt (struct keystate *, u_int8_t *, u_int16_t);
extern struct crypto_xf *crypto_get (enum transform);
extern struct keystate *crypto_init (struct crypto_xf *, u_int8_t *,
				     u_int16_t, enum cryptoerr *);
extern void crypto_init_iv (struct keystate *, u_int8_t *, size_t);
extern void crypto_update_iv (struct keystate *);

#endif /* _CRYPTO_H_ */

/*	$Id: cryptotest.c,v 1.1.1.1 1998/11/15 00:03:50 niklas Exp $	*/

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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "crypto.h"

void test_crypto (enum transform);

#define SET_KEY(x,y) {int i; for (i=0; i < (y); i++) (x)[i] = i;}

int
verify_buf (u_int8_t *buf, u_int16_t len)
{
  int i;

  for (i = 0; i < len; i++)
    if (buf[i] != i)
      return 0;

  return 1;
}

#define nibble2bin(y) (tolower((y)) < 'a' ? (y) - '0': tolower((y)) - 'a' + 10)
#define hexchar2bin(x) ((nibble2bin((x)[0]) << 4) + nibble2bin((x)[1]))
#define nibble2c(x) ((x) >= 10 ? ('a'-10+(x)) : ('0' + (x)))

void asc2bin (u_int8_t *bin, u_int8_t *asc, u_int16_t len)
{
  int i;

  for (i = 0; i < len; i += 2, asc += 2)
    {
      *bin++ = hexchar2bin(asc);
    }
}

void
special_test_blf (void)
{
  u_int8_t *akey = "0123456789ABCDEFF0E1D2C3B4A59687";
  u_int8_t *aiv = "FEDCBA9876543210";
  u_int8_t data[] = "7654321 Now is the time for \0\0\0"; /* len 29 */
  u_int8_t *acipher = "6B77B4D63006DEE605B156E27403979358DEB9E7154616D959F1652BD5FF92CCE7";
  u_int8_t key[16], cipher[32], iv[8];
  struct crypto_xf *xf;
  struct keystate *ks;
  enum cryptoerr err;
  int i;

  asc2bin (key, akey, strlen (akey));
  asc2bin (iv, aiv, strlen (aiv));
  asc2bin (cipher, acipher, 64);
  
  xf = crypto_get (BLOWFISH_CBC);
  printf ("Special Test-Case %s: ", xf->name);

  ks = crypto_init (xf, key, 16, &err);
  if (!ks)
    {
      printf ("FAILED (init %d)", err);
      goto fail;
    }

  crypto_init_iv (ks, iv, xf->blocksize);
  crypto_encrypt (ks, data, 32);

  for (i = 0; i < 32; i++)
    if (data[i] != cipher[i])
	break;
  if (i < 32)
    printf ("FAILED ");
  else
    printf ("OKAY ");

  free (ks);

fail:
  printf ("\n");
  return;
}

int
main (void)
{
  test_crypto (DES_CBC);

  test_crypto (TRIPLEDES_CBC);

  test_crypto (BLOWFISH_CBC);

  test_crypto (CAST_CBC);
  
  special_test_blf ();

  return 1;
}

void
test_crypto (enum transform which)
{
  u_int8_t buf[256];
  struct crypto_xf *xf;
  struct keystate *ks;
  enum cryptoerr err;
  
  xf = crypto_get (which);
  printf ("Testing %s: ", xf->name);

  SET_KEY (buf, xf->keymax);
  ks = crypto_init (xf, buf, xf->keymax, &err);
  if (!ks)
    {
      printf ("FAILED (init %d)", err);
      goto fail;
    }
  SET_KEY (buf, sizeof (buf));
  crypto_init_iv (ks, buf, xf->blocksize);
  crypto_encrypt (ks, buf, sizeof (buf));
  crypto_decrypt (ks, buf, sizeof (buf));
  if (!verify_buf (buf, sizeof (buf)))
    printf ("FAILED ");
  else
    printf ("OKAY ");

  free (ks);

 fail:
  printf ("\n");
  return;
}

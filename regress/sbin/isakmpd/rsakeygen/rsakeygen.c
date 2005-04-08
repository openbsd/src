/*	$OpenBSD: rsakeygen.c,v 1.1 2005/04/08 17:12:50 cloder Exp $	*/
/*	$EOM: rsakeygen.c,v 1.10 2000/12/21 15:18:53 ho Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niels Provos.  All rights reserved.
 * Copyright (c) 1999, 2001 Niklas Hallqvist.  All rights reserved.
 * Copyright (c) 2001 Håkan Olsson.  All rights reserved.
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
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "libcrypto.h"
#include "log.h"
#include "math_mp.h"

#define nibble2bin(y) (tolower (y) < 'a' ? (y) - '0' : tolower (y) - 'a' + 10)
#define hexchar2bin(x) ((nibble2bin ((x)[0]) << 4) + nibble2bin ((x)[1]))
#define nibble2c(x) ((x) >= 10 ? ('a' - 10 + (x)) : ('0' + (x)))

#define TEST_STRING "!Dies ist ein Test"

int
main (void)
{
  u_int8_t enc[256], dec[256], *asn, *foo;
  int len;
  FILE *fd;
  int erg = 0;
  RSA *key;

  libcrypto_init ();

  log_debug_cmd (LOG_CRYPTO, 99);
  memset (dec, '\0', sizeof dec);
  strlcpy (dec, TEST_STRING, 256);

  key = RSA_generate_key (1024, RSA_F4, NULL, NULL);
  if (key == NULL)
    {
      printf("Failed to generate key\n");
      return 1;
    }

  printf ("n: 0x");
  BN_print_fp (stdout, key->n);
  printf ("\ne: 0x");
  BN_print_fp (stdout, key->e);
  printf ("\n");

  printf ("n: 0x");
  BN_print_fp (stdout, key->n);
  printf ("\ne: 0x");
  BN_print_fp (stdout, key->e);
  printf ("\nd: 0x");
  BN_print_fp (stdout, key->d);
  printf ("\np: 0x");
  BN_print_fp (stdout, key->p);
  printf ("\nq: 0x");
  BN_print_fp (stdout, key->q);
  printf ("\n");

  printf ("Testing Signing/Verifying: ");
  /* Sign with Private Key */
  len = RSA_private_encrypt (strlen (dec) + 1, dec, enc, key,
			     RSA_PKCS1_PADDING);
  if (len == -1)
    printf ("SIGN FAILED ");
  else
    {
      /* Decrypt/Verify with Public Key */
      erg = RSA_public_decrypt (len, enc, dec, key, RSA_PKCS1_PADDING);

      if (erg == -1 || strcmp (dec, TEST_STRING))
	printf ("VERIFY FAILED");
      else
	printf ("OKAY");
    }

  printf ("\n");

  len = i2d_RSAPublicKey (key, NULL);
  foo = asn = malloc (len);
  len = i2d_RSAPublicKey (key, &foo);
  fd = fopen ("isakmpd_key.pub", "w");
  fwrite (asn, len, 1, fd);
  fclose (fd);
  free (asn);

  len = i2d_RSAPrivateKey (key, NULL);
  foo = asn = malloc (len);
  len = i2d_RSAPrivateKey (key, &foo);
  fd = fopen ("isakmpd_key", "w");
  fwrite (asn, len, 1, fd);
  fclose (fd);
  free (asn);

  RSA_free (key);

  return 0;
}

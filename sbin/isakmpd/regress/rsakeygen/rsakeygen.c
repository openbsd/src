/*	$OpenBSD: rsakeygen.c,v 1.9 1999/08/26 22:30:46 niklas Exp $	*/
/*	$EOM: rsakeygen.c,v 1.9 1999/08/12 22:34:30 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niels Provos.  All rights reserved.
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

#include <sys/param.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <gmp.h>

#include "libcrypto.h"
#include "log.h"

#define nibble2bin(y) (tolower (y) < 'a' ? (y) - '0' : tolower (y) - 'a' + 10)
#define hexchar2bin(x) ((nibble2bin ((x)[0]) << 4) + nibble2bin ((x)[1]))
#define nibble2c(x) ((x) >= 10 ? ('a' - 10 + (x)) : ('0' + (x)))

#define TEST_STRING "!Dies ist ein Test"

void asc2bin (u_int8_t *bin, u_int8_t *asc, u_int16_t len)
{
  int i;

  for (i = 0; i < len; i += 2, asc += 2)
    *bin++ = hexchar2bin (asc);
}

int
main (void)
{
  u_int8_t enc[256], dec[256], *asn, *foo;
  int len;
  FILE *fd;
  int erg = 0;
  RSA *key;

  libcrypto_init ();

#ifndef USE_LIBCRYPTO
  if (!libcrypto)
    {
      fprintf (stderr, "I did not find the RSA support, giving up...");
      exit (1);
    }
#endif

  log_debug_cmd (LOG_CRYPTO, 99);
  memset (dec, '\0', sizeof dec);
  strcpy (dec, TEST_STRING);

  key = LC (RSA_generate_key, (1024, RSA_F4, NULL, NULL));
  if (key == NULL) 
    {
      printf("Failed to generate key\n");
      return 0;
    }

  printf ("n: 0x");
  LC (BN_print_fp, (stdout, key->n));
  printf ("\ne: 0x");
  LC (BN_print_fp, (stdout, key->e));
  printf ("\n");

  printf ("n: 0x");
  LC (BN_print_fp, (stdout, key->n));
  printf ("\ne: 0x");
  LC (BN_print_fp, (stdout, key->e));
  printf ("\nd: 0x");
  LC (BN_print_fp, (stdout, key->d));
  printf ("\np: 0x");
  LC (BN_print_fp, (stdout, key->p));
  printf ("\nq: 0x");
  LC (BN_print_fp, (stdout, key->q));
  printf ("\n");

  printf ("Testing Signing/Verifying: ");
  /* Sign with Private Key */
  len = LC (RSA_private_encrypt, (strlen (dec) + 1, dec, enc, key,
				  RSA_PKCS1_PADDING));
  if (len == -1)
    printf ("SIGN FAILED ");
  else
    {
      /* Decrypt/Verify with Public Key */
      erg = LC (RSA_public_decrypt, (len, enc, dec, key, RSA_PKCS1_PADDING));

      if (erg == -1 || strcmp (dec, TEST_STRING))
	printf ("VERIFY FAILED");
      else
	printf ("OKAY");
    }

  printf ("\n");

  len = LC (i2d_RSAPublicKey, (key, NULL));
  foo = asn = malloc (len);
  len = LC (i2d_RSAPublicKey, (key, &foo));
  fd = fopen ("isakmpd_key.pub", "w");
  fwrite (asn, len, 1, fd);
  fclose (fd);
  free (asn);

  len = LC (i2d_RSAPrivateKey, (key, NULL));
  foo = asn = malloc (len);
  len = LC (i2d_RSAPrivateKey, (key, &foo));
  fd = fopen ("isakmpd_key", "w");
  fwrite (asn, len, 1, fd);
  fclose (fd);
  free (asn);

  LC (RSA_free, (key));

  return 1;
}

/*	$Id: rsakeygen.c,v 1.1.1.1 1998/11/15 00:03:50 niklas Exp $	*/

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
#include <gmp.h>

#include "log.h"
#include "gmp_util.h"
#include "asn.h"
#include "pkcs.h"

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

int
main (void)
{
  char *data = "Niels ist ein Luser!";
  u_int8_t *enc, *dec, *asn;
  u_int32_t enclen;
  u_int16_t len;
  FILE *fd;
  int erg = 0;

  struct rsa_public_key key;
  struct rsa_private_key priv;

  log_debug_cmd ((enum log_classes)LOG_CRYPTO, 99);
  pkcs_generate_rsa_keypair (&key, &priv, 1024);

  printf ("n: 0x"); mpz_out_str (stdout, 16, key.n);
  printf ("\ne: 0x"); mpz_out_str (stdout, 16, key.e);
  printf ("\n");

  printf ("n: 0x"); mpz_out_str (stdout, 16, priv.n);
  printf ("\ne: 0x"); mpz_out_str (stdout, 16, priv.e);
  printf ("\nd: 0x"); mpz_out_str (stdout, 16, priv.d);
  printf ("\np: 0x"); mpz_out_str (stdout, 16, priv.p);
  printf ("\nq: 0x"); mpz_out_str (stdout, 16, priv.q);
  printf ("\n");

  printf ("Testing Signing/Verifying: ");
  /* Sign with Private Key */
  if (!pkcs_rsa_encrypt (PKCS_PRIVATE, priv.n, priv.d, data, strlen(data)+1,
			 &enc, &enclen))
    printf ("FAILED ");
  else
    /* Decrypt/Verify with Public Key */
    erg = pkcs_rsa_decrypt (PKCS_PRIVATE, key.n, key.e, enc, &dec, &len);

  if (!erg || strcmp(data,dec))
    printf ("FAILED ");
  else
    printf ("OKAY ");

  printf ("\n");

  asn = pkcs_public_key_to_asn (&key);
  fd = fopen ("isakmpd_key.pub", "w");
  fwrite (asn, asn_get_len (asn), 1, fd);
  fclose (fd);
  free (asn);

  asn = pkcs_private_key_to_asn (&priv);
  fd = fopen ("isakmpd_key", "w");
  fwrite (asn, asn_get_len (asn), 1, fd);
  fclose (fd);
  free (asn);

  pkcs_free_public_key (&key);
  pkcs_free_private_key (&priv);

  return 1;
}

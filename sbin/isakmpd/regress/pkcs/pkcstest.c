/*	$OpenBSD: pkcstest.c,v 1.6 1999/03/24 14:57:54 niklas Exp $	*/
/*	$EOM: pkcstest.c,v 1.6 1999/03/13 17:43:22 niklas Exp $	*/

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
#include <stdio.h>
#include <gmp.h>
#include <stdlib.h>
#include <string.h>

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
  char buf[500];
  char *publickey = "304702400a66791dc6988168de7ab77419bb7fb0c001c6271027"
    "0075142942e19a8d8c51d053b3e3782a1de5dc5af4ebe99468170114a1dfe67cdc9a9"
    "af55d655620bbab0203010001";
  char *privatekey = "3082013602010002400a66791dc6988168de7ab77419bb7fb0c001"
    "c62710270075142942e19a8d8c51d053b3e3782a1de5dc5af4ebe99468170114a1dfe67"
    "cdc9a9af55d655620bbab020301000102400123c5b61ba36edb1d3679904199a89ea80c"
    "09b9122e1400c09adcf7784676d01d23356a7d44d6bd8bd50e94bfc723fa87d8862b751"
    "77691c11d757692df8881022033d48445c859e52340de704bcdda065fbb4058d740bd1d"
    "67d29e9c146c11cf610220335e8408866b0fd38dc7002d3f972c67389a65d5d8306566d"
    "5c4f2a5aa52628b0220045ec90071525325d3d46db79695e9afacc4523964360e02b119"
    "baa366316241022015eb327360c7b60d12e5e2d16bdcd97981d17fba6b70db13b20b436"
    "e24eada5902202ca6366d72781dfa24d34a9a24cbc2ae927a9958af426563ff63fb1165"
    "8a461d";
  char *data = "Niels ist ein Luser!";
  u_int8_t *enc, *dec;
  u_int16_t len;
  u_int32_t enclen;
  int erg = 0;

  struct rsa_public_key key;
  struct rsa_private_key priv;
  
  asc2bin (buf, publickey, strlen (publickey));
  pkcs_public_key_from_asn (&key, buf, sizeof (buf));

  printf ("n: 0x"); mpz_out_str (stdout, 16, key.n);
  printf ("\ne: 0x"); mpz_out_str (stdout, 16, key.e);
  printf ("\n");

  asc2bin (buf, privatekey, strlen (privatekey));
  pkcs_private_key_from_asn (&priv, buf, sizeof (buf));

  printf ("n: 0x"); mpz_out_str (stdout, 16, priv.n);
  printf ("\ne: 0x"); mpz_out_str (stdout, 16, priv.e);
  printf ("\nd: 0x"); mpz_out_str (stdout, 16, priv.d);
  printf ("\np: 0x"); mpz_out_str (stdout, 16, priv.p);
  printf ("\nq: 0x"); mpz_out_str (stdout, 16, priv.q);
  printf ("\n");

  printf ("Testing Signing/Verifying: ");
  /* Sign with Private Key */
  if (!pkcs_rsa_encrypt (PKCS_PRIVATE, NULL, &priv,  data, strlen(data)+1,
			 &enc, &enclen))
    printf ("FAILED ");
  else
    /* Decrypt/Verify with Public Key */
    erg = pkcs_rsa_decrypt (PKCS_PRIVATE, &key, NULL, enc, &dec, &len);

  if (!erg || strcmp(data,dec))
    printf ("FAILED ");
  else
    printf ("OKAY ");

  printf ("\n");

  pkcs_free_public_key (&key);
  pkcs_free_private_key (&priv);

  return 1;
}

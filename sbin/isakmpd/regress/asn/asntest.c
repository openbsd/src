/*	$OpenBSD: asntest.c,v 1.2 1998/11/15 00:44:06 niklas Exp $	*/

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
#include <stdio.h>
#include <string.h>
#include <gmp.h>

#include "conf.h"
#include "asn.h"
#include "asn_useful.h"
#include "pkcs.h"
#include "x509.h"

int
main (void)
{
  char buf[1000];
  char buf2[1000];
  u_int32_t len;
  struct norm_type test = SEQ("test", Signed);
  struct norm_type test2 = SEQ("cert", Certificate);
  struct norm_type *tmp, *tmp2;
  struct rsa_public_key key;
  struct x509_certificate cert;
  int i, j;
  u_int8_t *asn;
  char *p;

  FILE *f = fopen ("ssh-test-ca.pem", "r");
  len = 0;
  while (conf_get_line (f, buf + len, sizeof (buf) - len))
    if (buf[len] != '-')
      len = strlen (buf);

  conf_decode_base64 (buf, &len, buf);

  asn_template_clone (&test, 1);

  asn_decode_sequence (buf, len, &test);

  p = ASN_SIGNED_ALGORITHM(&test);

  printf ("ObjectId: %s = %s\n", p, asn_parse_objectid (asn_ids, p));

  asn_template_clone (&test2, 1);

  len = asn_get_len (ASN_SIGNED_DATA(&test));
  asn_decode_sequence (ASN_SIGNED_DATA(&test), len, &test2);

  tmp = asn_decompose ("cert.version", &test2);
  printf ("Version: "); mpz_out_str (stdout, 16, tmp->data);
  tmp = asn_decompose ("cert.serialNumber", &test2);
  printf ("\nSerialNumber: "); mpz_out_str (stdout, 16, tmp->data);
  tmp = asn_decompose ("cert.signature.algorithm", &test2);
  printf ("\nsignature: %s\n", 
	  asn_parse_objectid (asn_ids, (char *)tmp->data));

  tmp = ASN_CERT_VALIDITY(&test2);
  printf ("Begin: %s, End: %s\n", ASN_VAL_BEGIN(tmp), ASN_VAL_END(tmp));

  i = 0;
  while (1)
    {
      sprintf (buf2, "cert.issuer.RelativeDistinguishedName[%d]", i++);
      tmp = asn_decompose (buf2, &test2);
      if (tmp == NULL)
	break;

      j = 0;
      while (1) 
	{
	   sprintf (buf2, "RelativeDistinguishedName.AttributeValueAssertion[%d].AttributeType", j);
	   tmp2 = asn_decompose (buf2, tmp);
	   if (tmp2 == NULL)
	     break;
	   
	   printf ("Issuer: (%s) ", 
		   asn_parse_objectid (asn_ids, tmp2->data));
	   sprintf (buf2, "RelativeDistinguishedName.AttributeValueAssertion[%d].AttributeValue", j++);
	   tmp2 = asn_decompose (buf2, tmp);
	   printf ("%s\n", (char *)tmp2->data);
	}
    };

  tmp = asn_decompose ("cert.subjectPublicKeyInfo.algorithm.algorithm", &test2);
  printf ("Key: %s\n", asn_parse_objectid (asn_ids, tmp->data));

  tmp = asn_decompose ("cert.subjectPublicKeyInfo.subjectPublicKey", &test2);
  asn = tmp->data + 1;

  pkcs_public_key_from_asn (&key, asn, asn_get_len (asn));
  printf ("n (%u): 0x", (unsigned int)mpz_sizeinbase (key.n, 2)); 
  mpz_out_str (stdout, 16, key.n);
  printf ("\ne: 0x"); mpz_out_str (stdout, 16, key.e);
  printf ("\n");

  printf ("Validate SIGNED: ");
  if (!x509_validate_signed (buf, asn_get_len (buf), &key, &asn, &len))
    printf ("FAILED ");
  else
    printf ("OKAY ");
  printf ("\n");

  memset (&cert, 0, sizeof (cert));
  x509_decode_certificate (buf, asn_get_len (buf), &cert);

  printf ("Encoding Certificiate: ");
  if (!x509_encode_certificate(&cert, &asn, &len))
    printf ("FAILED ");
  else
    printf ("OKAY ");
  printf ("\n");
  return 1;
}

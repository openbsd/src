/*	$OpenBSD: keyconv.c,v 1.2 2001/08/22 15:25:32 ho Exp $	*/

/*
 * Copyright (c) 2000, 2001 Hakan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/dh.h>
#include <openssl/pem.h>

#ifdef LWRES
#include <dns/keyvalues.h>
#include <lwres/lwres.h>
#include <lwres/netdb.h>
#else
#include <netdb.h>
#include "keyvalues.h"
#endif

/* base64.c prototypes.  */
int b64_pton (const char *, u_char *, size_t);
int b64_ntop (const u_char *, size_t, char *, size_t);

#define OPENSSL_DEFAULT_EXT ".pem"
#define DNSSEC_DEFAULT_EXT  ".private"

/* DNSSEC private key format version.  */
#define MAJOR_VERSION 1
#define MINOR_VERSION 2

/* Make life a bit easier.  */
static const char *dns_tags[] =
{
  "Private-key-format:", "Algorithm:", "Modulus:", "PublicExponent:",
  "PrivateExponent:", "Prime1:", "Prime2:", "Exponent1:", "Exponent2:",
  "Coefficient:", "Generator(g):", "Prime(p):", "Private_value(x):",
  "Public_value(y):", "Subprime(q):", "Base(g):", "Key:",
  NULL
};

/* Must match the above.  */
enum DNS_TAGS
{ TAG_VERSION, TAG_ALGORITHM, TAG_MODULUS, TAG_PUBEXP, TAG_PRIVEXP,
  TAG_PRIME1, TAG_PRIME2, TAG_EXP1, TAG_EXP2, TAG_COEFF, TAG_GENERATOR,
  TAG_PRIME, TAG_PRIVVAL, TAG_PUBVAL, TAG_SUBPRIME, TAG_BASE, TAG_KEY };

#define MAX_LINE 4096

/* For DH calc.  */
BIGNUM bn_b2, bn_oakley1, bn_oakley2, bn_oakley5;

char *progname;

void
usage_exit (void)
{
  fprintf (stderr, "Usage: %s -d infile outfile\n", progname);
  fprintf (stderr, "or     %s -o infile outfile\n", progname);
  exit (1);
}

char *
hex2bin (char *h, int *len)
{
  static char b[MAX_LINE];
  const char hex[] = "0123456789abcdef0123456789ABCDEF";
  u_int8_t c;
  char *n;
  int  p;

  for (p = 0; p < strlen (h); p += 2)
    {
      n = strchr (hex, *(h + p));
      if (!n)
	{
	  fprintf (stderr, "hex2bin: bad input data!\n");
	  exit (1);
	}
      c = ((char)(n - hex) & 0xF) << 4;
      n = strchr (hex, *(h + p + 1));
      if (!n)
	{
	  fprintf (stderr, "hex2bin: bad input data!\n");
	  exit (1);
	}
      c |= ((char)(n - hex)) & 0xF;
      *(b + (p/2)) = (char)c;
    }
  *(b + (p/2)) = (char)0;
  *len = p/2;
  return b;
}

void
init_DH_constants (void)
{
#define MODP_768 \
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1" \
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD" \
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245" \
    "E485B576625E7EC6F44C42E9A63A3620FFFFFFFFFFFFFFFF"

#define MODP_1024 \
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1" \
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD" \
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245" \
    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED" \
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381" \
    "FFFFFFFFFFFFFFFF"

#define MODP_1536 \
    "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1" \
    "29024E088A67CC74020BBEA63B139B22514A08798E3404DD" \
    "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245" \
    "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED" \
    "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE45B3D" \
    "C2007CB8A163BF0598DA48361C55D39A69163FA8FD24CF5F" \
    "83655D23DCA3AD961C62F356208552BB9ED529077096966D" \
    "670C354E4ABC9804F1746C08CA237327FFFFFFFFFFFFFFFF"

  int len;
  char *t;

  BN_init (&bn_b2);
  BN_init (&bn_oakley1);
  BN_init (&bn_oakley2);
  BN_init (&bn_oakley5);

  BN_set_word (&bn_b2, 2);

  t = hex2bin (MODP_768, &len);
  BN_bin2bn (t, len, &bn_oakley1);

  t = hex2bin (MODP_1024, &len);
  BN_bin2bn (t, len, &bn_oakley2);

  t = hex2bin (MODP_1536, &len);
  BN_bin2bn (t, len, &bn_oakley5);
}

int
convert_dns_to_openssl (char *file_in, char *file_out)
{
  FILE *ii, *oo;
  char line[MAX_LINE], data[MAX_LINE], *valuestr;
  DH *dh;
  RSA *rsa;
  DSA *dsa;
  BIGNUM *bn;
  int lineno = 0, algorithm = 0, m, n, dh_code;
  enum DNS_TAGS found;
  u_int32_t bitfield = 0;

  /* Try to open input file */
  ii = fopen (file_in, "r");
  if (!ii)
    {
      fprintf (stderr, "Cannot open input file %.100s for reading\n", file_in);
      return 1;
    }

  /* Avoid compiler noises. */
  dh = NULL; rsa = NULL; dsa = NULL; bn = NULL;

  while (fgets (line, MAX_LINE, ii) != NULL)
    {
      lineno++;

      /* Try to find a matching field in the *.private file (dnssec-keygen) */
      for (m = 0, found = -1; found == -1 && dns_tags[m]; m++)
	if (strncasecmp (line, dns_tags[m], strlen (dns_tags[m])) == 0)
	  found = m;

      if (found == -1)
	{
	  fprintf (stderr, "Unrecognized input line %d\n", lineno);
	  fprintf (stderr, " --> %s", line);
	  continue;
	}

      valuestr = line + strlen (dns_tags[found]) + 1;

      if (found == TAG_VERSION)
	{
	  if (sscanf (valuestr, "v%d.%d", &m, &n) != 2)
	    {
	      fprintf (stderr, "Invalid/unknown version string (%.100s).\n",
		       valuestr);
	      return 1;
	    }
	  if (m > MAJOR_VERSION || (m == MAJOR_VERSION && n > MINOR_VERSION))
	    {
	      fprintf (stderr, "Cannot parse file formats above v%d.%d. "
		       "This is v%d.%d.\n", MAJOR_VERSION, MINOR_VERSION, m,
		       n);
	      return 1;
	    }
	  continue;
	}

      if (found == TAG_ALGORITHM)
	{
	  algorithm = strtol (valuestr, NULL, 10);
	  switch (algorithm)
	    {
	    case DNS_KEYALG_RSAMD5:
	      rsa = RSA_new ();
	      rsa->flags &= ~(RSA_FLAG_CACHE_PUBLIC | RSA_FLAG_CACHE_PRIVATE);
	      break;
	    case DNS_KEYALG_DH:
	      dh = DH_new();
	      dh->flags &= ~DH_FLAG_CACHE_MONT_P;
	      break;
	    case DNS_KEYALG_DSA:
	      dsa = DSA_new();
	      dsa->flags &= ~DSA_FLAG_CACHE_MONT_P;
	      break;
	    default:
	      fprintf (stderr, "Invalid algorithm %d\n", algorithm);
	      return 1;
	    }
	  continue;
	}

      switch (algorithm)
	{
	case DNS_KEYALG_RSAMD5:
	  if (found > TAG_COEFF)
	    {
	      fprintf (stderr, "Invalid keyword for RSA, line %d\n", lineno);
	      continue;
	    }
	  break;
	case DNS_KEYALG_DH:
	  if (found < TAG_GENERATOR || found > TAG_PUBVAL)
	    {
	      fprintf (stderr, "Invalid keyword for DH, line %d\n", lineno);
	      continue;
	    }
	  break;
	case DNS_KEYALG_DSA:
	  if (found < TAG_PRIME || found > TAG_BASE)
	    {
	      fprintf (stderr, "Invalid keyword for DSA, line %d\n", lineno);
	      continue;
	    }
	  break;
	default:
	  /* We should never reach this unless the file format is invalid. */
	  fprintf (stderr, "Invalid file format, no algorithm specified.\n");
	  return 1;
	}

      /* Ok, now we just have to deal with the fields as they appear. */

      if (bitfield & (1 << found))
	{
	  fprintf (stderr, "Duplicate \"%s\" field, line %d - ignored\n",
		   dns_tags[found], lineno);
	  continue; /* XXX return 1; ? */
	}
      bitfield |= (1 << found);

      m  = b64_pton (valuestr, data, MAX_LINE);
      bn = BN_bin2bn (data, m, NULL);
      if (!bn)
	{
	  fprintf (stderr,
		   "BN_bin2bn() failed for field \"%s\", line %d.\n",
		   dns_tags[found], lineno);
	  return 1;
	}

      switch (found)
	{
	case TAG_MODULUS:	/* RSA modulus */
	  rsa->n = bn;
	  break;

	case TAG_PUBEXP:	/* RSA public exponent */
	  rsa->e = bn;
	  break;

	case TAG_PRIVEXP:	/* RSA private exponent */
	  rsa->d = bn;
	  break;

	case TAG_PRIME1:	/* RSA prime 1 */
	  rsa->p = bn;
	  break;

	case TAG_PRIME2:	/* RSA prime 2 */
	  rsa->q = bn;
	  break;

	case TAG_EXP1:		/* RSA exponent 1 */
	  rsa->dmp1 = bn;
	  break;

	case TAG_EXP2:		/* RSA exponent 2 */
	  rsa->dmq1 = bn;
	  break;

	case TAG_COEFF:		/* RSA coefficient */
	  rsa->iqmp = bn;
	  break;

	case TAG_GENERATOR:	/* DH     generator(g) */
	  dh->g = bn;
	  break;

	case TAG_PRIME:		/* DH/DSA prime(p) */
	  if (algorithm == DNS_KEYALG_DH)
	    dh->p = bn;
	  else
	    dsa->p = bn;
	  break;

	case TAG_PUBVAL:	/* DH/DSA private-value(x) */
	  if (algorithm == DNS_KEYALG_DH)
	    dh->pub_key = bn;
	  else
	    dsa->pub_key = bn;
	  break;

	case TAG_PRIVVAL:	/* DH/DSA public-value(y) */
	  if (algorithm == DNS_KEYALG_DH)
	    dh->priv_key = bn;
	  else
	    dsa->priv_key = bn;
	  break;

	case TAG_SUBPRIME:	/* DSA    Subprime(q) */
	  dsa->q = bn;
	  break;

	case TAG_BASE:		/* DSA    Base(g) */
	  dsa->g = bn;
	  break;

	default:
	  break;
	}
    }
  fclose (ii);

  /* Check results */
  switch (algorithm)
    {
    case DNS_KEYALG_RSAMD5:
      m = (RSA_check_key (rsa) < 1 ? 0 : 1);
      break;

    case DNS_KEYALG_DH:
      dh_code = 0;
      m = DH_check (dh, &dh_code);
      break;

    case DNS_KEYALG_DSA:
      /* No check available. */
    default:
      m = -1;
      break;
    }

  if (m == 1)
    fprintf (stderr, "Key succeeded validation check.\n");
  else if (m == 0)
    {
      fprintf (stderr, "Key failed validation check.\n");
      return 1;
    }

  oo = fopen (file_out, "w");
  if (!oo)
    fprintf (stderr, "Cannot open output file %.100s\n", file_out);
  else
    fprintf (stderr, "Writing output file \"%s\".\n", file_out);

  /* Write result and cleanup */
  switch (algorithm)
    {
    case DNS_KEYALG_RSAMD5:
      if (oo)
      	PEM_write_RSAPrivateKey (oo, rsa, NULL, NULL, 0, NULL, NULL);
      RSA_free (rsa);
      break;

    case DNS_KEYALG_DH:
      if (oo)
	PEM_write_DHparams (oo, dh);
      DH_free (dh);
      break;

    case DNS_KEYALG_DSA:
      if (oo)
	PEM_write_DSAPrivateKey (oo, dsa, NULL, NULL, 0, NULL, NULL);
      DSA_free (dsa);
      break;

    default:
      break;
    }

  if (oo)
    fclose (oo);
  return 0;
}

char *
bn_to_b64 (const BIGNUM *bn)
{
  static char b64data[MAX_LINE];
  char data[MAX_LINE];
  int dlen, blen;

  if (!bn)
    {
      blen = 0;
      goto out;
    }

  dlen = BN_bn2bin (bn, data);
  blen = b64_ntop (data, dlen, b64data, MAX_LINE);

 out:
  b64data[blen] = (char) 0;
  return b64data;
}

void
write_RSA_public_key (char *fname, RSA *rsa)
{
  FILE *out;
  int e_bytes, mod_bytes, dlen, blen;
  char data[MAX_LINE], b64buf[MAX_LINE];

  out = fopen (fname, "w");
  if (!out)
    {
      fprintf (stderr, "Couldn't open public key file for writing.\n");
      return;
    }

  fprintf (stderr, "Writing output file \"%.100s\".\n", fname);
  dlen = 0;

  e_bytes   = BN_num_bytes (rsa->e);
  mod_bytes = BN_num_bytes (rsa->n);

  fprintf (out, ";name IN KEY %d %d %d ", DNS_KEYOWNER_ENTITY,
	   DNS_KEYPROTO_IPSEC, DNS_KEYALG_RSAMD5);

  if (e_bytes <= 0xFF)
    data[dlen++] = (char)e_bytes;
  else
    {
      data[dlen++] = (char)0;
      data[dlen++] = (char)(e_bytes >> 8);
      data[dlen++] = (char)(e_bytes & 0xFF);
    }
  dlen += BN_bn2bin (rsa->e, data + dlen);
  dlen += BN_bn2bin (rsa->n, data + dlen);

  blen = b64_ntop (data, dlen, b64buf, MAX_LINE);
  b64buf[blen] = (char)0;

  fprintf (out, "%s\n", b64buf);
  fclose (out);
}

void
write_DSA_public_key (char *fname, DSA *dsa)
{
  FILE *out;
  int p_bytes, dlen, blen, p;
  char data[MAX_LINE], b64buf[MAX_LINE];

  out = fopen (fname, "w");
  if (!out)
    {
      fprintf (stderr, "Couldn't open public key file for writing.\n");
      return;
    }

  fprintf (stderr, "Writing output file \"%.100s\".\n", fname);

  p_bytes = BN_num_bytes (dsa->p);
  if (((p_bytes - 64) / 8) > 8)
    {
      fprintf (stderr, "Invalid DSA key.\n");
      fclose (out);
      return;
    }

  dlen = blen = 0;
  *(data + dlen++) = (p_bytes - 64) / 8;

  /* Fields in DSA public key are zero-padded (left) */
#define PAD_ADD(var,len) \
  for (p = 0; p < (len - BN_num_bytes (var)); p++) \
    *(data + dlen++) = (char)0; \
  BN_bn2bin (var, data + dlen); \
  dlen += BN_num_bytes (var)

  PAD_ADD (dsa->q, SHA_DIGEST_LENGTH);
  PAD_ADD (dsa->p, p_bytes);
  PAD_ADD (dsa->g, p_bytes);
  PAD_ADD (dsa->pub_key, p_bytes);

#undef PAD_ADD

  blen = b64_ntop (data, dlen, b64buf, MAX_LINE);
  b64buf[blen] = (char)0;

  fprintf (out, ";name IN KEY %d %d %d %s\n", DNS_KEYOWNER_ENTITY,
	   DNS_KEYPROTO_IPSEC, DNS_KEYALG_DSA, b64buf);
  fclose (out);
}

void
write_DH_public_key (char *fname, DH *dh)
{
  FILE *out;
  int dlen, blen, p, g;
  char data[MAX_LINE], b64buf[MAX_LINE];

  out = fopen (fname, "w");
  if (!out)
    {
      fprintf (stderr, "Couldn't open public key file for writing.\n");
      return;
    }

  fprintf (stderr, "Writing output file \"%.100s\".\n", fname);

  dlen = p = g = 0;
  if (BN_cmp (dh->g, &bn_b2) == 0)
    {
      p = 1;
      if (BN_cmp (dh->p, &bn_oakley1) == 0)
	*(data + dlen++) = (char)1;
      else if (BN_cmp (dh->p, &bn_oakley2) == 0)
	*(data + dlen++) = (char)2;
      else if (BN_cmp (dh->p, &bn_oakley5) == 0)
	*(data + dlen++) = (char)5;
      else
	p = 0;
    }

  if (p == 0)
    {
      p = BN_num_bytes (dh->p);
      g = BN_num_bytes (dh->g);
      dlen += BN_bn2bin (dh->p, data + dlen);
      if (g > 0)
	dlen += BN_bn2bin (dh->p, data + dlen);
    }

  dlen += BN_bn2bin (dh->pub_key, data + dlen);

  blen = b64_ntop (data, dlen, b64buf, MAX_LINE);
  b64buf[blen] = (char)0;

  fprintf (out, ";name IN KEY %d %d %d %s\n", DNS_KEYOWNER_ENTITY,
	   DNS_KEYPROTO_IPSEC, DNS_KEYALG_DH, b64buf);

  fclose (out);
}

int
convert_openssl_to_dns (char *file_in, char *file_out)
{
  FILE *ii, *oo;
  RSA *rsa;
  DSA *dsa;
  DH *dh;
  enum DNS_TAGS tag;
  char *pubname;

  rsa = NULL;
  dsa = NULL;
  dh = NULL;

  ii = fopen (file_in, "r");
  if (!ii)
    {
      fprintf (stderr, "Cannot open input file %.100s for reading\n", file_in);
      return 1;
    }

  /* Try to load PEM */
  rsa = PEM_read_RSAPrivateKey (ii, NULL, NULL, NULL);
  if (!rsa)
    {
      rewind (ii);
      dsa = PEM_read_DSAPrivateKey (ii, NULL, NULL, NULL);
      if (!dsa)
	{
	  rewind (ii);
	  dh = PEM_read_DHparams (ii, NULL, NULL, NULL);
	  if (!dh)
	    {
	      fprintf (stderr, "Unrecognized infile format.\n");
	      fclose (ii);
	      return 1;
	    }
	}
    }
  fclose (ii);

  oo = fopen (file_out, "w");
  if (!oo)
    {
      fprintf (stderr, "Cannot open output file %.100s\n", file_out);
      return 1;
    }

  pubname = strdup (file_out);
  sprintf (pubname + strlen (pubname), ".pubkey");

  fprintf (stderr, "Writing output file \"%s\".\n", file_out);
  fprintf (oo, "%s v%d.%d\n", dns_tags[TAG_VERSION], MAJOR_VERSION,
	   MINOR_VERSION);

  /* XXX Algorithm dependent validation checks? */

  if (rsa)
    {
      /* Write private key */
      fprintf (oo, "%s %d (%s)\n", dns_tags[TAG_ALGORITHM],
	       DNS_KEYALG_RSAMD5, "RSA");
      tag = TAG_MODULUS;
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (rsa->n));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (rsa->e));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (rsa->d));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (rsa->p));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (rsa->q));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (rsa->dmp1));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (rsa->dmq1));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (rsa->iqmp));

      write_RSA_public_key (pubname, rsa);
    }
  else if (dsa)
    {
      /* DSA_print_fp (stdout, dsa, 2); */

      fprintf (oo, "%s %d (%s)\n", dns_tags[TAG_ALGORITHM],
	       DNS_KEYALG_DSA, "DSA");
      tag = TAG_PRIME;
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (dsa->p));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (dsa->priv_key));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (dsa->pub_key));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (dsa->q));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (dsa->g));

      write_DSA_public_key (pubname, dsa);
    }
  else if (dh)
    {
      /*
       * OpenSSL never stores private and public key values, so
       * we have to regenerate them from p and g. 
       */
      if (DH_generate_key (dh) == 0)
	{
	  fprintf (stderr, "DH key generation failed\n");
	  fclose (oo);
	  unlink (file_out);
	  return 1;
	}

      fprintf (oo, "%s %d (%s)\n", dns_tags[TAG_ALGORITHM],
	       DNS_KEYALG_DH, "DH");
      tag = TAG_GENERATOR;
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (dh->g));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (dh->p));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (dh->priv_key));
      fprintf (oo, "%s %s\n", dns_tags[tag++], bn_to_b64 (dh->pub_key));

      write_DH_public_key (pubname, dh);
    }
  else
    {
      fprintf (stderr, "PEM_read_PrivateKey() read unknown data.\n");
      fclose (oo);
      unlink (file_out);
      return 1;
    }

  fclose (oo);
  return 0;
}

int
main (int argc, char **argv)
{
  int ch, ret, dns_to_openssl = -1;

  progname = argv[0];

  while ((ch = getopt (argc, argv, "od")) != -1)
    {
      switch (ch)
	{
	case 'o':
	  if (dns_to_openssl == -1)
	    dns_to_openssl = 1;
	  else
	    usage_exit ();
	  break;

	case 'd':
	  if (dns_to_openssl == -1)
	    dns_to_openssl = 0;
	  else
	    usage_exit ();
	  break;

	default:
	  usage_exit ();
	}
    }
  argc -= optind;
  argv += optind;

  if (argc != 2 || dns_to_openssl == -1)
      usage_exit ();

  init_DH_constants ();

  /* Convert. */
  if (dns_to_openssl)
    ret = convert_dns_to_openssl (argv[0], argv[1]);
  else
    ret = convert_openssl_to_dns (argv[0], argv[1]);

  if (ret)
    fprintf (stderr, "Operation failed.\n");

  return 0;
}

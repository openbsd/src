/*	$OpenBSD: x509.c,v 1.12 1999/07/17 21:54:39 niklas Exp $	*/
/*	$EOM: x509.c,v 1.17 1999/07/17 20:44:12 niklas Exp $	*/

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
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <gmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ssl/x509.h>
#include <ssl/x509_vfy.h>
#include <ssl/pem.h>

#include "sysdep.h"

#include "conf.h"
#include "exchange.h"
#include "hash.h"
#include "ike_auth.h"
#include "sa.h"
#include "ipsec.h"
#include "log.h"
#include "x509.h"

/* 
 * X509_STOREs do not support subjectAltNames, so we have to build
 * our own hash table.
 */

/*
 * XXX Actually this store is not really useful, we never use it as we have
 * our own hash table.  It also gets collisons if we have several certificates
 * only differing in subjectAltName.
 */
static X509_STORE *x509_certs;
static X509_STORE *x509_cas;

/* Initial number of bits used as hash.  */
#define INITIAL_BUCKET_BITS 6

struct x509_hash {
  LIST_ENTRY (x509_hash) link;

  X509 *cert;
};

static LIST_HEAD (x509_list, x509_hash) *x509_tab;

/* Works both as a maximum index and a mask.  */
static int bucket_mask;

u_int16_t
x509_hash (u_int8_t *id, size_t len)
{
  int i;
  u_int16_t bucket = 0;

  /* XXX We might resize if we are crossing a certain threshold.  */
  len &= ~2;
  for (i = 0; i < len; i += 2)
    {
      /* Doing it this way avoids alignment problems.  */
      bucket ^= (id[i] + 1) * (id[i + 1]  + 257);
    }

  bucket &= bucket_mask;

  return bucket;
}

void
x509_hash_init ()
{
  int i;

  bucket_mask = (1 << INITIAL_BUCKET_BITS) - 1;
  x509_tab = malloc ((bucket_mask + 1) * sizeof (struct x509_list));
  if (!x509_tab)
    log_fatal ("x509_hash_init: malloc (%d) failed",
	       (bucket_mask + 1) * sizeof (struct x509_list));
  for (i = 0; i <= bucket_mask; i++)
    {
      LIST_INIT (&x509_tab[i]);
    }
}

/* Lookup a certificate by an ID blob.  */
X509 *
x509_hash_find (u_int8_t *id, size_t len)
{
  struct x509_hash *cert;
  u_int8_t *cid;
  size_t clen;

  for (cert = LIST_FIRST (&x509_tab[x509_hash (id, len)]); cert;
       cert = LIST_NEXT (cert, link))
    {
      if (!x509_cert_get_subject (cert->cert, &cid, &clen))
	continue;

      if (clen != len || memcmp (id, cid, len) != 0)
	{
	  free (cid);
	  continue;
	}
      free (cid);

      log_debug (LOG_CRYPTO, 70, "x509_hash_find: return X509 %p", cert->cert);
      return cert->cert;
    }

  log_debug (LOG_CRYPTO, 70, "x509_hash_find: no certificate matched query");
  return 0;
}

int
x509_hash_enter (X509 *cert)
{
  u_int16_t bucket = 0;
  u_int8_t *id;
  u_int32_t len;
  struct x509_hash *certh;

  if (!x509_cert_get_subject (cert, &id, &len))
    {
      log_print ("x509_hash_enter: can not retrieve subjectAltName");
      return 0;
    }

  certh = malloc (sizeof *certh);
  if (!certh)
    {
      free (id);
      log_error ("x509_hash_enter: malloc (%d) failed", sizeof *certh);
      return 0;
    }
  memset (certh, 0, sizeof *certh);

  certh->cert = cert;

  bucket = x509_hash (id, len);
  free (id);

  LIST_INSERT_HEAD (&x509_tab[bucket], certh, link);
  log_debug (LOG_CRYPTO, 70, "x509_hash_enter: cert %p added to bucket %d", 
	     cert, bucket);
  return 1;
}

/* X509 Certificate Handling functions.  */

int
x509_read_from_dir (X509_STORE *ctx, char *name, int hash)
{
  DIR *dir;
  struct dirent *file;
  BIO *certh;
  X509 *cert;
  char fullname[PATH_MAX];
  int off, size;

  if (strlen (name) >= sizeof fullname - 1)
    {
      log_print ("x509_read_from_dir: directory name too long");
      return 0;
    }

  log_debug (LOG_CRYPTO, 40, "x509_read_from_dir: reading certs from %s",
	     name);
  
  dir = opendir (name);
  if (!dir)
    {
      log_error ("x509_read_from_dir: opendir (\"%s\") failed", name);
      return 0;
    }

  strncpy (fullname, name, sizeof fullname - 1);
  fullname[sizeof fullname - 1] = 0;
  off = strlen (fullname);
  size = sizeof fullname - off - 1;

  while ((file = readdir (dir)) != NULL)
    {
      if (file->d_type != DT_REG && file->d_type != DT_LNK)
	continue;

      log_debug (LOG_CRYPTO, 60, "x509_read_from_dir: reading certificate %s",
		 file->d_name);

      certh = BIO_new (BIO_s_file ());
      if (!certh)
	{
	  log_error ("x509_read_from_dir: BIO_new (BIO_s_file ()) failed");
	  continue;
	}

      strncpy (fullname + off, file->d_name, size);
      fullname[off + size] = 0;

      if (BIO_read_filename (certh, fullname) == -1)
	{
	  BIO_free (certh);
	  log_error ("x509_read_from_dir: BIO_read_filename () failed");
	  continue;
	}

      cert = PEM_read_bio_X509 (certh, NULL, NULL);
      BIO_free (certh);
      if (cert == NULL)
	{
	  log_error ("x509_read_from_dir: PEM_read_bio_X509 (%s) failed",
		     file->d_name);
	  continue;
	}

      if (!X509_STORE_add_cert (ctx, cert))
	{
	  /*
	   * This is actually expected if we have several certificates only
	   * differing in subjectAltName, which is not an something that is
	   * strange.  Consider multi-homed machines.
	   */
	  log_debug (LOG_CRYPTO, 50,
		     "x509_read_from_dir: X509_STORE_add_cert (%s) failed",
		     file->d_name);
	}

      if (hash && !x509_hash_enter (cert))
	log_print ("x509_read_from_dir: X509_hash_enter (%s) failed",
		   file->d_name);
    }

  closedir (dir);

  return 1;
}

/* Initialize our databases and load our own certificates.  */
int
x509_cert_init (void)
{
  char *dirname;

  x509_hash_init ();

  /* Process client certificates we will accept.  */
  dirname = conf_get_str ("X509-certificates", "Cert-directory");
  if (!dirname)
    {
      log_print ("x509_cert_init: no Cert-directory");
      return 0;
    }

  x509_certs = X509_STORE_new ();
  if (!x509_certs)
    {
      log_print ("x509_cert_init: creating new X509_STORE failed");
      return 0;
    }

  if (!x509_read_from_dir (x509_certs, dirname, 1))
    {
      log_print ("x509_cert_init: x509_read_from_dir failed");
      return 0;
    }

  /* Process CA certificates we will trust.  */
  dirname = conf_get_str ("X509-certificates", "CA-directory");
  if (!dirname)
    {
      log_print ("x509_cert_init: no CA-directory");
      return 0;
    }

  x509_cas = X509_STORE_new ();
  if (!x509_cas)
    {
      log_print ("x509_cert_init: creating new X509_STORE failed");
      return 0;
    }

  if (!x509_read_from_dir (x509_cas, dirname, 0))
    {
      log_print ("x509_cert_init: x509_read_from_dir failed");
      return 0;
    }

  return 1;
}

void *
x509_cert_get (u_int8_t *asn, u_int32_t len)
{
  return x509_from_asn (asn, len);
}

int
x509_cert_validate (void *scert)
{
  X509_STORE_CTX csc;
  X509_NAME *issuer, *subject;
  X509 *cert = (X509 *)scert;
  EVP_PKEY *key;
  int res;

  /*
   * Validate the peer certificate by checking with the CA certificates we
   * trust.
   */
  X509_STORE_CTX_init (&csc, x509_cas, cert, NULL);
  res = X509_verify_cert (&csc);
  X509_STORE_CTX_cleanup (&csc);

  /* Return if validation succeeded or self-signed certs are not accepted.  */
  if (res || !conf_get_str ("X509-certificates", "Accept-self-signed"))
    return res;

  issuer = X509_get_issuer_name (cert);
  subject = X509_get_subject_name (cert);
  
  if (!issuer || !subject || X509_name_cmp (issuer, subject))
    return 0;

  key = X509_get_pubkey (cert);
  if (!key)
    return 0;

  if (X509_verify (cert, key) == -1)
    return 0;

  return 1;
}

int
x509_cert_insert (void *scert)
{
  X509 *cert = X509_dup ((X509 *)scert);
  int res;

  if (!cert)
    {
      log_print ("x509_cert_insert: X509_dup failed");
      return 0;
    }

  res = x509_hash_enter (cert);
  if (!res)
    X509_free (cert);

  return res;
}

void
x509_cert_free (void *cert)
{
  X509_free ((X509 *)cert);
}

/* Validate the BER Encoding of a RDNSequence in the CERT_REQ payload.  */
int
x509_certreq_validate (u_int8_t *asn, u_int32_t len)
{
  int res = 1;
  /*  struct norm_type name = SEQOF ("issuer", RDNSequence);

  if (!asn_template_clone (&name, 1)
      || (asn = asn_decode_sequence (asn, len, &name)) == 0)
    {
      log_print ("x509_certreq_validate: can not decode 'acceptable CA' info");
      res = 0;
    }
    asn_free (&name); */

  /* XXX - not supported directly in SSL - later */

  return res;
}

/* Decode the BER Encoding of a RDNSequence in the CERT_REQ payload.  */
void *
x509_certreq_decode (u_int8_t *asn, u_int32_t len)
{
  /* XXX This needs to be done later.
  struct norm_type aca = SEQOF ("aca", RDNSequence);
  struct norm_type *tmp;
  struct x509_aca naca, *ret;

  if (!asn_template_clone (&aca, 1)
      || (asn = asn_decode_sequence (asn, len, &aca)) == 0)
    {
      log_print ("x509_certreq_validate: can not decode 'acceptable CA' info");
      goto fail;
    }
  memset (&naca, 0, sizeof (naca));

  tmp = asn_decompose ("aca.RelativeDistinguishedName.AttributeValueAssertion",
		       &aca);
  if (!tmp)
    goto fail;
  x509_get_attribval (tmp, &naca.name1);

  tmp = asn_decompose ("aca.RelativeDistinguishedName[1]"
		       ".AttributeValueAssertion", &aca);
  if (tmp)
    x509_get_attribval (tmp, &naca.name2);
  
  asn_free (&aca);

  ret = malloc (sizeof (struct x509_aca));
  if (ret)
    memcpy (ret, &naca, sizeof (struct x509_aca));
  else
    {
      log_error ("x509_certreq_decode: malloc (%d) failed",
		 sizeof (struct x509_aca));
      x509_free_aca (&aca);
    }

  return ret;

 fail:
 asn_free (&aca); */
  return 0;
}

void
x509_free_aca (void *blob)
{
  struct x509_aca *aca = blob;

  if (aca->name1.type)
    free (aca->name1.type);
  if (aca->name1.val)
    free (aca->name1.val);

  if (aca->name2.type)
    free (aca->name2.type);
  if (aca->name2.val)
    free (aca->name2.val);
}

X509 *
x509_from_asn (u_char *asn, u_int len)
{
  BIO *certh;
  X509 *scert = NULL;

  certh = BIO_new (BIO_s_mem ());
  if (!certh)
    {
      log_error ("X509_from_asn: BIO_new (BIO_s_mem ()) failed");
      return NULL;
    }
	  
  if (BIO_write (certh, asn, len) == -1)
    {
      log_error ("X509_from_asn: BIO_write failed\n");
      goto end;
    }

  scert = d2i_X509_bio (certh, NULL);
  if (!scert)
    {
      log_print ("X509_from_asn: d2i_X509_bio failed\n");
      goto end;
    }

 end:
  BIO_free (certh);
  return scert;
}

/*
 * Check that a certificate has a subjectAltName and that it matches our ID.
 */
int
x509_check_subjectaltname (u_char *id, u_int id_len, X509 *scert)
{
  u_int8_t *altname;
  u_int32_t altlen;
  int type, idtype, ret;

  type = x509_cert_subjectaltname (scert, &altname, &altlen);
  if (!type)
    {
      log_print ("x509_check_subjectaltname: can't access subjectAltName");
      return 0;
    }

  /* 
   * Now that we have the X509 certicate in native form, get the
   * subjectAltName extension and verify that it matches our ID.
   */

  /* XXX Get type of ID.  */
  idtype = id[0];
  id += ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ;
  id_len -= ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ;

  ret = 0;
  switch (idtype)
    {
    case IPSEC_ID_IPV4_ADDR:
      if (type == X509v3_IPV4_ADDR) 
	ret = 1;
      break;
    case IPSEC_ID_FQDN:
      if (type == X509v3_DNS_NAME) 
	ret = 1;
      break;
    case IPSEC_ID_USER_FQDN:
      if (type == X509v3_RFC_NAME) 
	ret = 1;
      break;
    default:
      ret = 0;
      break;
    }

  if (!ret)
    {
      log_debug (LOG_CRYPTO, 50,
		 "X509_check_subjectaltname: "
		 "our ID type does not match X509 cert ID type");
      return 0;
    }

  if (altlen != id_len || memcmp (altname, id, id_len) != 0)
    {
      log_debug (LOG_CRYPTO, 50,
		 "X509_check_subjectaltname: "
		 "our ID does not match X509 cert ID");
      return 0;
    }

  return 1;
}

/*
 * Obtain a certificate from an acceptable CA.
 * XXX We don't check if the certificate we find is from an accepted CA.
 */
int
x509_cert_obtain (u_int8_t *id, size_t id_len, void *data, u_int8_t **cert,
		  u_int32_t *certlen)
{
  struct x509_aca *aca = data;
  X509 *scert;
  u_char *p;

  if (aca)
    log_debug (LOG_CRYPTO, 60,
	       "x509_cert_obtain: acceptable certificate authorities here");

  /* We need our ID to find a certificate.  */
  if (!id)
    {
      log_print ("X509_cert_obtain: ID is missing");
      return 0;
    }

  scert = x509_hash_find (id, id_len);
  if (!scert)
    return 0;

  if (!x509_check_subjectaltname (id, id_len, scert))
    {
      log_print ("X509_cert_obtain: subjectAltName does not match id");
      free (*cert);
      return 0;
    }

  *certlen = i2d_X509 (scert, NULL);
  p = *cert = malloc (*certlen);
  if (!p)
    {
      log_error ("X509_cert_obtain: malloc (%d) failed", *certlen);
      return 0;
    }
  *certlen = i2d_X509 (scert, &p);

  return 1;
}

/* Returns a pointer to the subjectAltName information of X509 certificate.  */
int
x509_cert_subjectaltname (X509 *scert, u_int8_t **altname, u_int32_t *len)
{
  X509_EXTENSION *subjectaltname;
  u_int8_t *sandata;
  int extpos;
  int santype, sanlen;

  extpos = X509_get_ext_by_NID (scert, NID_subject_alt_name, -1);
  if (extpos == -1)
    {
      log_print ("X509_cert_subjectaltname: "
		 "certificate does not contain subjectAltName");
      return 0;
    }

  subjectaltname = X509_get_ext (scert, extpos);

  if (!subjectaltname || !subjectaltname->value
      || !subjectaltname->value->data || subjectaltname->value->length < 4)
    {
      log_print ("X509_check_subjectaltname: "
		 "invalid subjectaltname extension");
      return 0;
    }

  /* SSL does not handle unknown ASN stuff well, do it by hand.  */
  sandata = subjectaltname->value->data;
  santype = sandata[2] & 0x3f;
  sanlen = sandata[3];
  sandata += 4;

  if (sanlen + 4 != subjectaltname->value->length) 
    {
      log_print ("X509_check_subjectaltname: subjectaltname invalid length");
      return 0;
    }
  
  *len = sanlen;
  *altname = sandata;

  return santype;
}

int
x509_cert_get_subject (void *scert, u_int8_t **id, u_int32_t *id_len)
{
  X509 *cert = scert;
  int type;
  u_int8_t *altname;
  u_int32_t altlen;

  type = x509_cert_subjectaltname (cert, &altname, &altlen);

  switch (type)
    {
    case X509v3_IPV4_ADDR:
      {
	char buf[ISAKMP_ID_DATA_OFF + 4];
	
	/* XXX sizeof IPV4_ADDR, how any better?  */
	if (altlen != 4)
	  {
	    log_print ("x509_cert_get_subject: length != IP4addr: %d",
		       altlen);
	    return 0;
	  }

	SET_ISAKMP_ID_TYPE (buf, IPSEC_ID_IPV4_ADDR);
	SET_IPSEC_ID_PROTO (buf + ISAKMP_ID_DOI_DATA_OFF, 0);
	SET_IPSEC_ID_PORT (buf + ISAKMP_ID_DOI_DATA_OFF, 0);
	memcpy (buf + ISAKMP_ID_DATA_OFF, altname, altlen);

	*id_len = ISAKMP_ID_DATA_OFF + 4 - ISAKMP_GEN_SZ;
	*id = malloc (*id_len);
	if (!*id) 
	  {
	    log_print ("x509_cert_get_subject: malloc (%d) failed", *id_len);
	    return 0;
	  }
	memcpy (*id, buf + ISAKMP_GEN_SZ, *id_len);
      }
      break;
    default:
      log_print ("x509_cert_get_subject: unsupported subjectAltName type: %d",
		 type);
      return 0;
    }

  return 1;
}

int
x509_cert_get_key (void *scert, void *keyp)
{
  X509 *cert = scert;
  EVP_PKEY *key;

  key = X509_get_pubkey (cert);

  /* Check if we got the right key type */
  if (key->type != EVP_PKEY_RSA)
    {
      log_print ("x509_cert_get_key: public key is not a RSA key");
      X509_free (cert);
      return 0;
    }

  *(RSA **)keyp = RSAPublicKey_dup (key->pkey.rsa);

  return *(RSA **)keyp == NULL ? 0 : 1;
}

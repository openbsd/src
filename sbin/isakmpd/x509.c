/*	$OpenBSD: x509.c,v 1.7 1999/03/24 15:00:36 niklas Exp $	*/
/*	$EOM: x509.c,v 1.10 1999/03/13 17:43:20 niklas Exp $	*/

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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gmp.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "sysdep.h"

#include "conf.h"
#include "exchange.h"
#include "hash.h"
#include "ike_auth.h"
#include "sa.h"
#include "ipsec.h"
#include "log.h"
#include "asn.h"
#include "asn_useful.h"
#include "pkcs.h"
#include "x509.h"

/* X509 Certificate Handling functions */

/* Validate the BER Encoding of a RDNSequence in the CERT_REQ payload */

int
x509_certreq_validate (u_int8_t *asn, u_int32_t len)
{
  struct norm_type name = SEQOF ("issuer", RDNSequence);
  int res = 1;

  if (asn_template_clone (&name, 1) == NULL ||
      (asn = asn_decode_sequence (asn, len, &name)) == NULL)
    {
      log_print ("x509_certreq_validate: can not decode 'acceptable CA' info");
      res = 0;
    }
  asn_free (&name);

  return res;
}

/* Decode the BER Encoding of a RDNSequence in the CERT_REQ payload */

void *
x509_certreq_decode (u_int8_t *asn, u_int32_t len)
{
  struct norm_type aca = SEQOF ("aca", RDNSequence);
  struct norm_type *tmp;
  struct x509_aca naca, *ret;

  if (asn_template_clone (&aca, 1) == NULL ||
      (asn = asn_decode_sequence (asn, len, &aca)) == NULL)
    {
      log_print ("x509_certreq_validate: can not decode 'acceptable CA' info");
      goto fail;
    }
  memset (&naca, 0, sizeof (naca));

  tmp = asn_decompose ("aca.RelativeDistinguishedName.AttributeValueAssertion", &aca);
  if (tmp == NULL)
    goto fail;
  x509_get_attribval (tmp, &naca.name1);

  tmp = asn_decompose ("aca.RelativeDistinguishedName[1].AttributeValueAssertion", &aca);
  if (tmp != NULL)
    x509_get_attribval (tmp, &naca.name2);
  
  asn_free (&aca);

  if ((ret = malloc (sizeof (struct x509_aca))) != NULL)
    memcpy (ret, &naca, sizeof (struct x509_aca));
  else
    x509_free_aca (&aca);

  return ret;

 fail:
  asn_free (&aca);
  return NULL;
}

void
x509_free_aca (void *blob)
{
  struct x509_aca *aca = blob;

  if (aca->name1.type != NULL)
    free (aca->name1.type);
  if (aca->name1.val != NULL)
    free (aca->name1.val);

  if (aca->name2.type != NULL)
    free (aca->name2.type);
  if (aca->name2.val != NULL)
    free (aca->name2.val);
}

/* 
 * Obtain a Certificate from an acceptable Certification Authority.
 * XXX - this is where all the magic should happen, but yet here
 * you will find nothing :-\
 */

int
x509_cert_obtain (struct exchange *exchange, void *data, u_int8_t **cert,
		  u_int32_t *certlen)
{
  struct x509_aca *aca = data;
  struct ipsec_exch *ie = exchange->data;
  char *certfile;
  int fd, res = 0;
  struct stat st;

  if (aca != NULL)
    log_debug (LOG_CRYPTO, 60, "x509_cert_obtain: (%s) %s, (%s) %s",
	       asn_parse_objectid (asn_ids, aca->name1.type), aca->name1.val,
	       asn_parse_objectid (asn_ids, aca->name2.type), aca->name2.val);

  /* XXX - this needs to be changed - but how else shoud I know */
  switch (ie->ike_auth->id)
    {
    case IKE_AUTH_RSA_SIG:
      if ((certfile = conf_get_str ("rsa_sig", "cert")) == NULL)
	return 0;
      break;
    default:
      return 0;
    }

  if (stat (certfile, &st) == -1)
    {
      log_error ("x509_cert_obtain: failed to state %s", certfile);
      return 0;
    }

  *certlen = st.st_size;

  if ((fd = open (certfile, O_RDONLY)) == -1)
    {
      log_error ("x509_cert_obtain: failed to open %s", certfile);
      return 0;
    }
  
  if ((*cert = malloc (st.st_size)) == NULL)
    {
      log_print ("x509_cert_obtain: out of memory");
      res = 0;
      goto done;
    }

  if (read (fd, *cert, st.st_size) != st.st_size)
    {
      log_print ("x509_cert_obtain: cert file ended early");
      free (*cert);
      res = 0;
      goto done;
    }

  {
    /* 
     * XXX - assumes IPv4 here, assumes a certificate with an extension
     * type of subjectAltName at the end - this can go once the saved
     * certificate is only used with one host with a fixed IP address
     */
    u_int8_t *id_cert, *asn, *id;
    size_t id_len;
    u_int32_t id_cert_len;

    /* XXX - assumes IPv4 */
    id = exchange->initiator ? exchange->id_i : exchange->id_r;
    id_len = exchange->initiator ? exchange->id_i_len : exchange->id_r_len;

    /* XXX - we need our ID to set that in the cert */
    if (id != NULL) 
      {
	/* XXX - get to address ? */
	id += 4; id_len -= 4;
	
	/* Get offset into data structure where the IP is saved */
	asn = *cert;
	id_cert_len = asn_get_data_len (NULL, &asn, &id_cert);
	asn = id_cert;
	id_cert_len = asn_get_data_len (NULL, &asn, &id_cert);
	id_cert += id_cert_len - 4;
	memcpy (id_cert, id, 4); 
      }
  }
 
  res = 1;

 done:
  close (fd);

  return res;
}

/* Retrieve the public key from a X509 Certificate */
int
x509_cert_get_key (u_int8_t *asn, u_int32_t asnlen, void *blob)
{
  struct rsa_public_key *key = blob;
  struct x509_certificate cert;

  if (!x509_decode_certificate (asn, asnlen, &cert))
    return 0;

  /* XXX - perhaps put into pkcs ? */
  mpz_init_set (key->n, cert.key.n);
  mpz_init_set (key->e, cert.key.e);

  x509_free_certificate (&cert);

  return 1;
}

/*
 * Retrieve the public key from a X509 Certificate
 * XXX - look at XXX below.
 */

int
x509_cert_get_subject (u_int8_t *asn, u_int32_t asnlen, 
		       u_int8_t **subject, u_int32_t *subjectlen)
{
  struct x509_certificate cert;

  if (!x509_decode_certificate (asn, asnlen, &cert))
    return 0;

  if (cert.extension.type == NULL || cert.extension.val == NULL)
    goto fail;

  log_debug (LOG_CRYPTO, 60, "x509_cert_get_subject: Extension Type %s = %s",
	     cert.extension.type, 
	     asn_parse_objectid (asn_ids, cert.extension.type));

  if (strcmp (ASN_ID_SUBJECT_ALT_NAME, cert.extension.type))
    {
      log_print ("x509_cert_get_subject: extension type != subjectAltName");
      goto fail;
    }

  /* 
   * XXX Evil**3, due to lack of time the IP encoding of subjectAltName
   * is supposed to be: 0x30 0x06 0x087 0x04 aa bb cc dd, where the IPV4
   * IP number is aa.bb.cc.dd.
   */

  if (asn_get_len (cert.extension.val) != 8 || cert.extension.val[3] != 4)
    {
      log_print ("x509_cert_get_subject: subjectAltName uses "
		 "unhandled encoding");
      goto fail;
    }

  /* XXX - 4 bytes for IPV4 address */
  *subject = malloc (4);
  if (*subject == NULL)
    {
      log_print ("x509_cert_get_subject: out of memory");
      goto fail;
    }
  *subjectlen = 4;
  memcpy (*subject, cert.extension.val + 4, *subjectlen);

  x509_free_certificate (&cert);

  return 1;

 fail:
  x509_free_certificate (&cert);
  return 0;
}

/* Initalizes the struct x509_attribval from a AtributeValueAssertion */

void
x509_get_attribval (struct norm_type *obj, struct x509_attribval *a)
{
  struct norm_type *tmp;

  tmp = asn_decompose ("AttributeValueAssertion.AttributeType", obj);
  if (tmp != NULL && tmp->data != NULL)
    a->type = strdup ((char *)tmp->data);

  tmp = asn_decompose ("AttributeValueAssertion.AttributeValue", obj);
  if (tmp != NULL && tmp->data != NULL)
    a->val = strdup ((char *)tmp->data);
}

/* Set's norm_type with values from x509_attribval */

void
x509_set_attribval (struct norm_type *obj, struct x509_attribval *a)
{
  struct norm_type *tmp;

  tmp = asn_decompose ("AttributeValueAssertion.AttributeType", obj);
  tmp->data = strdup (a->type);
  tmp->len = strlen (tmp->data);
  tmp = asn_decompose ("AttributeValueAssertion.AttributeValue", obj);
  tmp->type = TAG_PRINTSTRING;
  tmp->data = strdup (a->val);
  tmp->len = strlen (tmp->data);
}

void
x509_free_attribval (struct x509_attribval *a)
{
  if (a->type != NULL)
    free (a->type);
  if (a->val != NULL)
    free (a->val);
}

void
x509_free_certificate (struct x509_certificate *cert)
{
  pkcs_free_public_key (&cert->key);
  if (cert->signaturetype != NULL)
    free (cert->signaturetype);
  if (cert->start != NULL)
    free (cert->start);
  if (cert->end != NULL)
    free (cert->end);

  x509_free_attribval (&cert->issuer1);
  x509_free_attribval (&cert->issuer2);
  x509_free_attribval (&cert->subject1);
  x509_free_attribval (&cert->subject2);
  x509_free_attribval (&cert->extension);
}

int
x509_decode_certificate (u_int8_t *asn, u_int32_t asnlen, 
			      struct x509_certificate *rcert)
{
  struct norm_type cert = SEQ ("cert", Certificate);
  struct norm_type *tmp;
  u_int8_t *data;
  u_int32_t datalen;

  /* Get access to the inner Certificate */
  if (!x509_validate_signed (asn, asnlen, NULL, &data, &datalen))
    return 0;
  
  memset (rcert, 0, sizeof (*rcert));

  if (asn_template_clone (&cert, 1) == NULL || 
      asn_decode_sequence (data, datalen, &cert) == NULL)
    goto fail;

  tmp = asn_decompose ("cert.subjectPublicKeyInfo.subjectPublicKey", &cert);
  if (!pkcs_public_key_from_asn (&rcert->key, tmp->data + 1, tmp->len - 1))
    goto fail;
  
  tmp = asn_decompose ("cert.version", &cert);
  rcert->version = mpz_get_ui (tmp->data);
  tmp = asn_decompose ("cert.serialNumber", &cert);
  rcert->serialnumber = mpz_get_ui (tmp->data);
  tmp = asn_decompose ("cert.signature.algorithm", &cert);
  rcert->signaturetype = strdup ((char *)tmp->data);

  tmp = asn_decompose ("cert.issuer.RelativeDistinguishedName."
		       "AttributeValueAssertion", &cert);
  x509_get_attribval (tmp, &rcert->issuer1);
  tmp = asn_decompose ("cert.issuer.RelativeDistinguishedName[1]."
		       "AttributeValueAssertion", &cert);
  if (tmp != NULL)
    x509_get_attribval (tmp, &rcert->issuer2);
  else
    rcert->issuer2.type = NULL;

  tmp = asn_decompose ("cert.subject.RelativeDistinguishedName."
		       "AttributeValueAssertion", &cert);
  x509_get_attribval (tmp, &rcert->subject1);
  tmp = asn_decompose ("cert.subject.RelativeDistinguishedName[1]."
		       "AttributeValueAssertion", &cert);
  if (tmp != NULL)
    x509_get_attribval (tmp, &rcert->subject2);
  else
    rcert->subject2.type = NULL;

  tmp = asn_decompose ("cert.validity.notBefore", &cert);
  rcert->start = strdup ((char *)tmp->data);
  tmp = asn_decompose ("cert.validity.notAfter", &cert);
  rcert->end = strdup ((char *)tmp->data);

  /* For x509v3 there might be an extension, try to decode it */
  tmp = asn_decompose ("cert.extension", &cert);
  if (tmp && tmp->data && rcert->version == 2)
    x509_decode_cert_extension (tmp->data, tmp->len, rcert);
 
  asn_free (&cert);
  return 1;

 fail:
  x509_free_certificate (rcert);
  asn_free (&cert);
  return 0;
}

int
x509_encode_certificate (struct x509_certificate *rcert,
			 u_int8_t **asn, u_int32_t *asnlen)
{
  struct norm_type cert = SEQ ("cert", Certificate);
  struct norm_type *tmp;
  u_int8_t *data, *new_buf;
  mpz_t num;

  if (asn_template_clone (&cert, 1) == NULL)
    goto fail;

  if (rcert->extension.type != NULL && rcert->extension.val != NULL)
    {
      u_int8_t *asn;
      u_int32_t asnlen;

      tmp = asn_decompose ("cert.extension", &cert);
      if (x509_encode_cert_extension (rcert, &asn, &asnlen))
	{
	  tmp->data = asn;
	  tmp->len = asnlen;
	}
    }

  tmp = asn_decompose ("cert.subjectPublicKeyInfo.algorithm.parameters",
		       &cert);
  tmp->type = TAG_NULL;
  tmp = asn_decompose ("cert.subjectPublicKeyInfo.algorithm.algorithm",
		       &cert);
  tmp->data = strdup (ASN_ID_RSAENCRYPTION);
  tmp->len = strlen (tmp->data);

  tmp = asn_decompose ("cert.subjectPublicKeyInfo.subjectPublicKey", &cert);
  data = pkcs_public_key_to_asn (&rcert->key);
  if (data == NULL)
    goto fail;

  /* This is a BIT STRING add 0 octet for padding */
  tmp->len = asn_get_len (data);
  new_buf = realloc (data, tmp->len + 1);
  if (new_buf == NULL)
    {
      free (data);
      goto fail;
    }
  data = new_buf;
  memmove (data + 1, data, tmp->len);
  data[0] = 0;
  tmp->data = data;
  tmp->len++;
  
  mpz_init (num);
  tmp = asn_decompose ("cert.version", &cert);
  mpz_set_ui (num, rcert->version);
  if (!pkcs_mpz_to_norm_type (tmp, num))
    {
      mpz_clear (num);
      goto fail;
    }

  tmp = asn_decompose ("cert.serialNumber", &cert);
  mpz_set_ui (num, rcert->serialnumber);
  if (!pkcs_mpz_to_norm_type (tmp, num))
    {
      mpz_clear (num);
      goto fail;
    }
  mpz_clear (num);

  tmp = asn_decompose ("cert.signature.parameters", &cert);
  tmp->type = TAG_NULL;
  tmp = asn_decompose ("cert.signature.algorithm", &cert);
  tmp->data = strdup (rcert->signaturetype);
  tmp->len = strlen ((char *)tmp->data);

  tmp = asn_decompose ("cert.issuer.RelativeDistinguishedName."
		       "AttributeValueAssertion", &cert);
  x509_set_attribval (tmp, &rcert->issuer1);
  tmp = asn_decompose ("cert.issuer.RelativeDistinguishedName[1]."
		       "AttributeValueAssertion", &cert);
  x509_set_attribval (tmp, &rcert->issuer2);

  tmp = asn_decompose ("cert.subject.RelativeDistinguishedName."
		       "AttributeValueAssertion", &cert);
  x509_set_attribval (tmp, &rcert->subject1);
  tmp = asn_decompose ("cert.subject.RelativeDistinguishedName[1]."
		       "AttributeValueAssertion", &cert);
  x509_set_attribval (tmp, &rcert->subject2);

  tmp = asn_decompose ("cert.validity.notBefore", &cert);
  tmp->data = strdup (rcert->start);
  tmp->len = strlen ((char *)tmp->data);

  tmp = asn_decompose ("cert.validity.notAfter", &cert);
  tmp->data = strdup (rcert->end);
  tmp->len = strlen ((char *)tmp->data);

  *asn = asn_encode_sequence (&cert, NULL);
  if (*asn == NULL)
    goto fail;

  *asnlen = asn_get_len (*asn);

  asn_free (&cert);
  return 1;

 fail:
  asn_free (&cert);
  return 0;
}

/*
 * Decode an Extension to a X509 certificate.
 * XXX - We ignore the critical boolean
 */

int
x509_decode_cert_extension (u_int8_t *asn, u_int32_t asnlen,
			    struct x509_certificate *cert)
{
  struct norm_type *tmp;
  struct norm_type ex = SEQOF ("ex", Extensions);

  /* Implicit tagging for extension */
  ex.class = ADD_EXP (3, UNIVERSAL);

  if (asn_template_clone (&ex, 1) == NULL ||
      asn_decode_sequence (asn, asnlen, &ex) == NULL)
    {
      asn_free (&ex);
      return 0;
    }

  tmp = asn_decompose ("ex.extension.extnValue", &ex);
  if (!tmp || tmp->data == NULL || asn_get_len (tmp->data) != tmp->len)
    goto fail;
  cert->extension.val = malloc (tmp->len);
  if (cert->extension.val == 0)
    goto fail;
  memcpy (cert->extension.val, tmp->data, tmp->len);

  tmp = asn_decompose ("ex.extension.extnId", &ex);
  if (!tmp || tmp->data == NULL)
    goto fail;
  cert->extension.type = strdup (tmp->data);
  if (cert->extension.type == NULL)
    {
      free (cert->extension.val);
      cert->extension.val = NULL;
      goto fail;
    }

  asn_free (&ex);
  return 1;

 fail:
  asn_free (&ex);
  return 0;
}

/* 
 * Encode a Cert Extension - XXX - only one extension per certificate 
 * XXX - We tag everything as critical
 */

int
x509_encode_cert_extension (struct x509_certificate *cert,
			    u_int8_t **asn, u_int32_t *asnlen)
{
  struct norm_type ex = SEQ ("ex", Extensions);
  struct norm_type *tmp;
  ex.class = ADD_EXP (3, UNIVERSAL);

  if (asn_template_clone (&ex ,1) == NULL)
    goto fail;

  tmp = asn_decompose ("ex.extension.extnId", &ex);
  tmp->data = strdup (cert->extension.type);
  tmp->len = strlen (tmp->data);

  /* XXX - we mark every extension as critical */
  tmp = asn_decompose ("ex.extension.critical", &ex);
  if ((tmp->data = malloc (1)) == NULL)
    goto fail;
  *(u_int8_t *)tmp->data = 0xFF;
  tmp->len = 1;

  tmp = asn_decompose ("ex.extension.extnValue", &ex);
  if ((tmp->data = malloc (asn_get_len (cert->extension.val))) == NULL)
    goto fail;
  tmp->len = asn_get_len (cert->extension.val);
  memcpy (tmp->data, cert->extension.val, tmp->len);

  *asn = asn_encode_sequence (&ex, NULL);
  if (*asn == NULL)
    goto fail;

  *asnlen = asn_get_len (*asn);
  
  asn_free (&ex);
  return 1;
 fail:
  asn_free (&ex);
  return 0;
}

/* 
 * Checks the signature on an ASN.1 Signed Type. If the passed Key is
 * NULL we just unwrap the inner object and return it.
 */

int
x509_validate_signed (u_int8_t *asn, u_int32_t asnlen,
		      struct rsa_public_key *key, u_int8_t **data, 
		      u_int32_t *datalen)
{
  struct norm_type sig = SEQ("signed", Signed);
  struct norm_type digest = SEQ("digest", DigestInfo);
  struct norm_type *tmp;
  struct hash *hash = NULL;
  int res;
  u_int8_t *dec;
  u_int16_t declen;

  if (asn_template_clone (&sig, 1) == NULL)
      /* Failed, probably memory allocation, free what we got anyway */
    goto fail;

  if (asn_decode_sequence (asn, asnlen, &sig) == NULL)
    {
      log_print ("x509_validate_signed: input data could not be decoded");
      goto fail;
    }

  tmp = asn_decompose ("signed.algorithm.algorithm", &sig);

  if (!strcmp ((char *)tmp->data, ASN_ID_MD5WITHRSAENC))
    {
      hash = hash_get (HASH_MD5);
    }
  else
    {
      char *id = asn_parse_objectid (asn_ids, tmp->data);
      log_print ("x509_validate_signed: can not handle SigType %s",
		 id == NULL ? tmp->data : id);
      goto fail;
    }

  if (hash == NULL)
    goto fail;

  tmp = asn_decompose ("signed.data", &sig);
  /* Hash the data */
  hash->Init (hash->ctx);
  hash->Update (hash->ctx, tmp->data, tmp->len);
  hash->Final (hash->digest, hash->ctx);

  *data = tmp->data;
  *datalen = tmp->len;

  /* Used to unwrap the SIGNED object around the Certificate */
  if (key == NULL)
    {
      asn_free (&sig);
      return 1;
    }

  tmp = asn_decompose ("signed.encrypted", &sig);
  /* 
   * tmp->data is a BIT STRING, the first octet in the BIT STRING gives
   * the padding bits at the end. Per definition there are no padding
   * bits at the end in this case, so just skip it.
   */
  if (!pkcs_rsa_decrypt (PKCS_PRIVATE, key, NULL, tmp->data + 1,
			 &dec, &declen))
    goto fail;

  if (asn_template_clone (&digest, 1) == NULL || 
      asn_decode_sequence (dec, declen, &digest) == NULL)
    {
      asn_free (&digest);
      goto fail;
    }
  tmp = asn_decompose ("digest.digestAlgorithm.algorithm", &digest);
  if (strcmp (ASN_ID_MD5, (char *)tmp->data))
    {
      log_print ("x509_validate_signed: DigestAlgorithm is not MD5");
      res = 0;
    }
  else
    {
      tmp = asn_decompose ("digest.digest", &digest);
      if (tmp->len != hash->hashsize ||
	  memcmp (tmp->data, hash->digest, tmp->len))
	{
	  log_print ("x509_validate_signed: Digest does not match Data");
	  res = 0;
	}
      else
	res = 1;
    }

  asn_free (&digest);
  asn_free (&sig);
  return res;

 fail:
  asn_free (&sig);
  return 0;
}

/*
 * Create an ASN Signed Structure from the data passed in data
 * and return the result in asn.
 * At the moment the used hash is MD5, this is the only common
 * hash between us and X509.
 */

int
x509_create_signed (u_int8_t *data, u_int32_t datalen,
		    struct rsa_private_key *key, u_int8_t **asn, 
		    u_int32_t *asnlen)
{
  struct norm_type digest = SEQ ("digest", DigestInfo);
  struct norm_type sig = SEQ ("signed", Signed);
  struct norm_type *tmp;
  struct hash *hash;
  u_int8_t *diginfo, *enc;
  u_int32_t enclen;
  int res = 0;
  
  /* Hash the Data */
  hash = hash_get (HASH_MD5);
  hash->Init (hash->ctx);
  hash->Update (hash->ctx, data, datalen);
  hash->Final (hash->digest, hash->ctx);

  if (asn_template_clone (&digest, 1) == NULL)
    goto fail;

  tmp = asn_decompose ("digest.digest", &digest);
  tmp->len = hash->hashsize;
  tmp->data = malloc (hash->hashsize);
  if (tmp->data == NULL)
    goto fail;
  memcpy (tmp->data, hash->digest, hash->hashsize);

  tmp = asn_decompose ("digest.digestAlgorithm.parameters", &digest);
  tmp->type = TAG_NULL;
  tmp = asn_decompose ("digest.digestAlgorithm.algorithm", &digest);
  tmp->data = strdup (ASN_ID_MD5);
  tmp->len = strlen (tmp->data);

  /* ASN encode Digest Information */
  if ((diginfo = asn_encode_sequence (&digest, NULL)) == NULL)
    goto fail;

  /* Encrypt the Digest Info with Private Key */
  res = pkcs_rsa_encrypt (PKCS_PRIVATE, NULL, key, diginfo, 
			  asn_get_len (diginfo), &enc, &enclen);
  free (diginfo);
  if (!res)
    goto fail;
  res = 0;

  if (asn_template_clone (&sig, 1) == NULL)
    goto fail2;

  tmp = asn_decompose ("signed.algorithm.parameters", &sig);
  tmp->type = TAG_NULL;
  tmp = asn_decompose ("signed.algorithm.algorithm", &sig);
  tmp->data = strdup (ASN_ID_MD5WITHRSAENC);
  tmp->len = strlen (tmp->data);

  /* The type is BITSTING, i.e. first octet need to be zero */
  tmp = asn_decompose ("signed.encrypted", &sig);
  tmp->data = malloc (enclen + 1);
  if (tmp->data == NULL)
    {
      free (enc);
      goto fail2;
    }
  tmp->len = enclen + 1;
  memcpy (tmp->data + 1, enc, enclen);
  *(char *)tmp->data = 0;
  free (enc);

  tmp = asn_decompose ("signed.data", &sig);
  tmp->data = data;
  tmp->len = datalen;

  *asn = asn_encode_sequence (&sig, NULL);
  if (*asn == NULL)
    goto fail2;
  *asnlen = asn_get_len (*asn);

  /* This is the data we have been given, we can not free it in asn_free */
  tmp->data = NULL;
  res = 1;	/* Successfull */
 fail2:
  asn_free (&sig);
 fail:
  asn_free (&digest);
  return res;
}

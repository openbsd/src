/*	$OpenBSD: ike_auth.c,v 1.12 1999/04/19 21:08:44 niklas Exp $	*/
/*	$EOM: ike_auth.c,v 1.29 1999/04/18 15:17:51 niklas Exp $	*/

/*
 * Copyright (c) 1998, 1999 Niklas Hallqvist.  All rights reserved.
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

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "asn.h"
#include "cert.h"
#include "conf.h"
#include "constants.h"
#include "exchange.h"
#include "gmp.h"
#include "gmp_util.h"
#include "hash.h"
#include "ike_auth.h"
#include "ipsec.h"
#include "ipsec_doi.h"
#include "log.h"
#include "message.h"
#include "pkcs.h"
#include "prf.h"
#include "transport.h"
#include "util.h"

static u_int8_t *enc_gen_skeyid (struct exchange *, size_t *);
static u_int8_t *pre_shared_gen_skeyid (struct exchange *, size_t *);
static u_int8_t *sig_gen_skeyid (struct exchange *, size_t *);

static int pre_shared_decode_hash (struct message *);
static int rsa_sig_decode_hash (struct message *);
static int pre_shared_encode_hash (struct message *);
static int rsa_sig_encode_hash (struct message *);

static int ike_auth_hash (struct exchange *, u_int8_t *);

static struct ike_auth ike_auth[] = {
  {
    IKE_AUTH_PRE_SHARED, pre_shared_gen_skeyid, pre_shared_decode_hash,
    pre_shared_encode_hash
  },
  {
    IKE_AUTH_DSS, sig_gen_skeyid, pre_shared_decode_hash,
    pre_shared_encode_hash
  },
  {
    IKE_AUTH_RSA_SIG, sig_gen_skeyid, rsa_sig_decode_hash,
    rsa_sig_encode_hash
  },
  {
    IKE_AUTH_RSA_ENC, enc_gen_skeyid, pre_shared_decode_hash,
    pre_shared_encode_hash
  },
  {
    IKE_AUTH_RSA_ENC_REV, enc_gen_skeyid, pre_shared_decode_hash,
    pre_shared_encode_hash
  },
};

struct ike_auth *
ike_auth_get (u_int16_t id)
{
  int i;

  for (i = 0; i < sizeof ike_auth / sizeof ike_auth[0]; i++)
    if (id == ike_auth[i].id)
      return &ike_auth[i];
  return 0;
}

/*
 * Find and decode the configured key (pre-shared or public) for the
 * peer denoted by ID.  Stash the len in KEYLEN.
 */
static char *
ike_auth_get_key (char *id, size_t *keylen)
{
  char *key, *buf;

  /* Get the pre-shared key for our peer.  */
  key = conf_get_str (id, "Authentication");
  if (!key)
    {
      log_print ("ike_auth_get_key: no key found for peer \"%s\"", id);
      return 0;
    }

  /* If the key starts with 0x it is in hex format.  */
  if (strncasecmp (key, "0x", 2) == 0)
    {
      *keylen = (strlen (key) - 1) / 2;
      buf = malloc (*keylen);
      if (!buf)
	{
	  log_print ("ike_auth_get_key: malloc (%d) failed", *keylen);
	  return 0;
	}
      if (hex2raw (key + 2, buf, *keylen))
	{
	  free (buf);
	  log_print ("ike_auth_get_key: invalid hex key %s", key);
	  return 0;
	}
      key = buf;
    }
  else
    *keylen = strlen (key);

  return key;
}

static u_int8_t *
pre_shared_gen_skeyid (struct exchange *exchange, size_t *sz)
{
  struct prf *prf;
  struct ipsec_exch *ie = exchange->data;
  u_int8_t *skeyid;
  u_int8_t *key;
  u_int8_t *buf = 0;
  size_t keylen;

  /* Get the pre-shared key for our peer.  */
  key = ike_auth_get_key (exchange->name, &keylen);
  if (!key)
    return 0;

  prf = prf_alloc (ie->prf_type, ie->hash->type, key, keylen);
  if (buf)
    free (buf);
  if (!prf)
    return 0;

  *sz = prf->blocksize;
  skeyid = malloc (*sz);
  if (!skeyid)
    {
      log_error ("pre_shared_gen_skeyid: malloc (%d) failed", *sz);
      prf_free (prf);
      return 0;
    }

  prf->Init (prf->prfctx);
  prf->Update (prf->prfctx, exchange->nonce_i, exchange->nonce_i_len);
  prf->Update (prf->prfctx, exchange->nonce_r, exchange->nonce_r_len);
  prf->Final (skeyid, prf->prfctx);
  prf_free (prf);

  return skeyid;
}

/* Both DSS & RSA signature authentication uses this algorithm.  */
static u_int8_t *
sig_gen_skeyid (struct exchange *exchange, size_t *sz)
{
  struct prf *prf;
  struct ipsec_exch *ie = exchange->data;
  u_int8_t *skeyid, *key;

  key = malloc (exchange->nonce_i_len + exchange->nonce_r_len);
  if (!key)
    return 0;
  memcpy (key, exchange->nonce_i, exchange->nonce_i_len);
  memcpy (key + exchange->nonce_i_len, exchange->nonce_r,
	  exchange->nonce_r_len);
  prf = prf_alloc (ie->prf_type, ie->hash->type, key,
		   exchange->nonce_i_len + exchange->nonce_r_len);
  free (key);
  if (!prf)
    return 0;

  *sz = prf->blocksize;
  skeyid = malloc (*sz);
  if (!skeyid)
    {
      log_error ("sig_gen_skeyid: malloc (%d) failed", *sz);
      prf_free (prf);
      return 0;
    }

  prf->Init (prf->prfctx);
  prf->Update (prf->prfctx, ie->g_xy, ie->g_x_len);
  prf->Final (skeyid, prf->prfctx);
  prf_free (prf);

  return skeyid;
}

/*
 * Both standard and revised RSA encryption authentication uses this SKEYID
 * computation.
 */
static u_int8_t *
enc_gen_skeyid (struct exchange *exchange, size_t *sz)
{
  struct prf *prf;
  struct ipsec_exch *ie = exchange->data;
  struct hash *hash = ie->hash;
  u_int8_t *skeyid;

  hash->Init (hash->ctx);
  hash->Update (hash->ctx, exchange->nonce_i, exchange->nonce_i_len);
  hash->Update (hash->ctx, exchange->nonce_r, exchange->nonce_r_len);
  hash->Final (hash->digest, hash->ctx);
  prf = prf_alloc (ie->prf_type, hash->type, hash->digest, *sz);
  if (!prf)
    return 0;

  *sz = prf->blocksize;
  skeyid = malloc (*sz);
  if (!skeyid)
    {
      log_error ("enc_gen_skeyid: malloc (%d) failed", *sz);
      prf_free (prf);
      return 0;
    }

  prf->Init (prf->prfctx);
  prf->Update (prf->prfctx, exchange->cookies, ISAKMP_HDR_COOKIES_LEN);
  prf->Final (skeyid, prf->prfctx);
  prf_free (prf);

  return skeyid;
}

static int
pre_shared_decode_hash (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct payload *payload;
  size_t hashsize = ie->hash->hashsize;
  char header[80];
  int initiator = exchange->initiator;
  u_int8_t **hash_p;

  /* Choose the right fields to fill-in.  */
  hash_p = initiator ? &ie->hash_r : &ie->hash_i;

  payload = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_HASH]);
  if (!payload)
    return -1;

  /* Check that the hash is of the correct size.  */
  if (GET_ISAKMP_GEN_LENGTH (payload->p) - ISAKMP_GEN_SZ != hashsize)
    return -1;

  /* XXX Need this hash be in the SA?  */
  *hash_p = malloc (hashsize);
  if (!*hash_p)
    {
      log_error ("pre_shared_decode_hash: malloc (%d) failed", hashsize);
      return -1;
    }

  memcpy (*hash_p, payload->p + ISAKMP_HASH_DATA_OFF, hashsize);
  snprintf (header, 80, "pre_shared_decode_hash: HASH_%c",
	    initiator ? 'R' : 'I');
  log_debug_buf (LOG_MISC, 80, header, *hash_p, hashsize);

  payload->flags |= PL_MARK;

  return 0;
}

/* Decrypt the HASH in SIG, we already need a parsed ID payload.  */
static int
rsa_sig_decode_hash (struct message *msg)
{
  struct cert_handler *handler;
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct payload *p;
  struct rsa_public_key key;
  size_t hashsize = ie->hash->hashsize;
  char header[80];
  int initiator = exchange->initiator;
  u_int8_t **hash_p, *id_cert, *id;
  u_int16_t len;
  u_int32_t id_cert_len;
  size_t id_len;
  int found = 0;

  /* Choose the right fields to fill-in.  */
  hash_p = initiator ? &ie->hash_r : &ie->hash_i;
  id = initiator ? exchange->id_r : exchange->id_i;
  id_len = initiator ? exchange->id_r_len : exchange->id_i_len;

  if (!id || id_len == 0)
    {
      log_print ("rsa_sig_decode_hash: no ID in sa");
      return -1;
    }

  /* Just bother with the ID data field.  */
  id += ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ;
  id_len -= ISAKMP_ID_DATA_OFF - ISAKMP_GEN_SZ;

  for (p = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_CERT]); p;
       p = TAILQ_NEXT (p, link))
    {
      p->flags |= PL_MARK;

      /* When we have found a key, just walk over the rest, marking them.  */
      if (found)
	continue;

      handler = cert_get (GET_ISAKMP_CERT_ENCODING (p->p));
      if (!handler)
	{
	  log_debug (LOG_MISC, 30,
		     "rsa_sig_decode_hash: no handler for %s CERT encoding",
		     constant_lookup (isakmp_certenc_cst,
				      GET_ISAKMP_CERT_ENCODING (p->p)));
	  continue;
	}
  
      if (!handler->cert_get_subject (p->p + ISAKMP_CERT_DATA_OFF, 
				      GET_ISAKMP_GEN_LENGTH (p->p) - 
				      ISAKMP_CERT_DATA_OFF,
				      &id_cert, &id_cert_len))
	{
	  log_print ("rsa_sig_decode_hash: can not get subject from CERT");
	  continue;
	}

      if (id_cert_len != id_len || memcmp (id, id_cert, id_len) != 0)
	{
	  log_print ("rsa_sig_decode_hash: CERT subject does not match ID");
	  free (id_cert);
	  continue;
	}
      free (id_cert);

      if (!handler->cert_get_key (p->p + ISAKMP_CERT_DATA_OFF, 
				  GET_ISAKMP_GEN_LENGTH (p->p) -
				  ISAKMP_CERT_DATA_OFF,
				  &key))
	{
	  log_print ("rsa_sig_decode_hash: decoding payload CERT failed");
	  continue;
	}

      found++;
    }

  /* If no certificate provided a key, try the config file.  */
  if (!found)
    {
#ifdef notyet
    rawkey = ike_auth_get_key (exchange->name, &keylen);
    if (!rawkey)
      {
	log_print ("rsa_sig_decode_hash: no public key found");
	return -1;
      }
#else
      log_print ("rsa_sig_decode_hash: no public key found");
      return -1;
#endif
    }

  p = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_SIG]);
  if (!p)
    {
      log_print ("rsa_sig_decode_hash: missing signature payload");
      pkcs_free_public_key (&key);
      return -1;
    }

  /* Check that the sig is of the correct size.  */
  if (GET_ISAKMP_GEN_LENGTH (p->p) - ISAKMP_SIG_SZ != mpz_sizeinoctets (key.n))
    {
      pkcs_free_public_key (&key);
      log_print ("rsa_sig_decode_hash: "
		 "SIG payload length does not match public key");
      return -1;
    }

  if (!pkcs_rsa_decrypt (PKCS_PRIVATE, &key, 0, p->p + ISAKMP_SIG_DATA_OFF,
			 hash_p, &len))
    {
      pkcs_free_public_key (&key);
      return -1;
    }

  pkcs_free_public_key (&key);
  
  if (len != hashsize)
    {
      free (*hash_p);
      *hash_p = 0;
      return -1;
    }

  snprintf (header, 80, "rsa_sig_decode_hash: HASH_%c", initiator ? 'R' : 'I');
  log_debug_buf (LOG_MISC, 80, header, *hash_p, hashsize);

  p->flags |= PL_MARK;

  return 0;
}

static int
pre_shared_encode_hash (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  size_t hashsize = ie->hash->hashsize;
  char header[80];
  int initiator = exchange->initiator;
  u_int8_t *buf;

  /* XXX - hashsize is not necessarily prf->blocksize.  */
  buf = malloc (ISAKMP_HASH_SZ + hashsize);
  if (!buf)
    {
      log_error ("pre_shared_encode_hash: malloc (%d) failed",
		 ISAKMP_HASH_SZ + hashsize);
      return -1;
    }
  
  if (ike_auth_hash (exchange, buf + ISAKMP_HASH_DATA_OFF) == -1)
    {
      /* XXX Log? */
      free (buf);
      return -1;
    }
    
  snprintf (header, 80, "pre_shared_encode_hash: HASH_%c",
	    initiator ? 'I' : 'R');
  log_debug_buf (LOG_MISC, 80, header, buf + ISAKMP_HASH_DATA_OFF, hashsize);
  if (message_add_payload (msg, ISAKMP_PAYLOAD_HASH, buf,
			   ISAKMP_HASH_SZ + hashsize, 1))
    {
      free (buf);
      return -1;
    }

  return 0;
}

/* Encrypt the HASH into a SIG type.  */
static int
rsa_sig_encode_hash (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  size_t hashsize = ie->hash->hashsize;
  struct cert_handler *handler;
  struct rsa_private_key key;
  char header[80];
  int initiator = exchange->initiator;
  u_int8_t *buf, *asn, *data;
  u_int32_t asnlen, datalen;
  char *keyfile;

  /* XXX This needs to be configureable.  */
  handler = cert_get (ISAKMP_CERTENC_X509_SIG);
  if (!handler)
    {
      /* XXX Log? */
      return -1;
    }
  /* XXX Implicitly uses exchange->id_{i,r}.  */
  if (!handler->cert_obtain (exchange, 0, &data, &datalen))
    {
      /* XXX Log? */
      return -1;
    }

  buf = realloc (data, ISAKMP_CERT_SZ + datalen);
  if (!buf)
    {
      log_error ("rsa_sig_encode_hash: realloc (%p, %d) failed", data,
		 ISAKMP_CERT_SZ + datalen);
      free (data);
      return -1;
    }
  memmove (buf + ISAKMP_CERT_SZ, buf, datalen);
  SET_ISAKMP_CERT_ENCODING (buf, ISAKMP_CERTENC_X509_SIG);
  if (message_add_payload (msg, ISAKMP_PAYLOAD_CERT, buf,
			   ISAKMP_CERT_SZ + datalen, 1))
    {
      free (buf);
      return -1;
    }

  /* XXX - do we want to store our files in ASN.1 ? */
  keyfile = conf_get_str ("RSA_sig", "privkey");
  if (!asn_get_from_file (keyfile, &asn, &asnlen))
    {
      /* XXX Log? */
      return -1;
    }
	
  if (!pkcs_private_key_from_asn (&key, asn, asnlen))
    {
      /* XXX Log? */
      free (asn);
      return -1;
    }
  free (asn);

  /* XXX - hashsize is not necessarily prf->blocksize */
  buf = malloc (hashsize);
  if (!buf)
    {
      /* XXX Log?  */
      pkcs_free_private_key (&key);
      return -1;
    }
  
  if (ike_auth_hash (exchange, buf) == -1)
    {
      /* XXX Log? */
      free (buf);
      pkcs_free_private_key (&key);
      return -1;
    }
    
  snprintf (header, 80, "rsa_sig_encode_hash: HASH_%c", initiator ? 'I' : 'R');
  log_debug_buf (LOG_MISC, 80, header, buf, hashsize);

  if (!pkcs_rsa_encrypt (PKCS_PRIVATE, 0, &key, buf, hashsize, &data,
			 &datalen))
    {
      free (buf);
      pkcs_free_private_key (&key);
      return -1;
    }
  pkcs_free_private_key (&key);
  free (buf);

  buf = realloc (data, ISAKMP_SIG_SZ + datalen);
  if (!buf)
    {
      log_error ("rsa_sig_encode_hash: realloc (%p, %d) failed", data,
		 ISAKMP_SIG_SZ + datalen);
      free (data);
      return -1;
    }
  memmove (buf + ISAKMP_SIG_SZ, buf, datalen);

  snprintf (header, 80, "rsa_sig_encode_hash: SIG_%c", initiator ? 'I' : 'R');
  log_debug_buf (LOG_MISC, 80, header, buf + ISAKMP_SIG_DATA_OFF, datalen);
  if (message_add_payload (msg, ISAKMP_PAYLOAD_SIG, buf,
			   ISAKMP_SIG_SZ + datalen, 1))
    {
      free (buf);
      return -1;
    }

  return 0;
}

int
ike_auth_hash (struct exchange *exchange, u_int8_t *buf)
{
  struct ipsec_exch *ie = exchange->data;
  struct prf *prf;
  struct hash *hash = ie->hash;
  int initiator = exchange->initiator;
  u_int8_t *id;
  size_t id_len;

  /* Choose the right fields to fill-in.  */
  id = initiator ? exchange->id_i : exchange->id_r;
  id_len = initiator ? exchange->id_i_len : exchange->id_r_len;

  /* Allocate the prf and start calculating our HASH.  */
  prf = prf_alloc (ie->prf_type, hash->type, ie->skeyid, ie->skeyid_len);
  if (!prf)
      return -1;

  prf->Init (prf->prfctx);
  prf->Update (prf->prfctx, initiator ? ie->g_xi : ie->g_xr, ie->g_x_len);
  prf->Update (prf->prfctx, initiator ? ie->g_xr : ie->g_xi, ie->g_x_len);
  prf->Update (prf->prfctx,
	       exchange->cookies
	       + (initiator ? ISAKMP_HDR_ICOOKIE_OFF : ISAKMP_HDR_RCOOKIE_OFF),
	       ISAKMP_HDR_ICOOKIE_LEN);
  prf->Update (prf->prfctx,
	       exchange->cookies
	       + (initiator ? ISAKMP_HDR_RCOOKIE_OFF : ISAKMP_HDR_ICOOKIE_OFF),
	       ISAKMP_HDR_ICOOKIE_LEN);
  prf->Update (prf->prfctx, ie->sa_i_b, ie->sa_i_b_len);
  prf->Update (prf->prfctx, id, id_len);
  prf->Final (buf, prf->prfctx);
  prf_free (prf);

  return 0;
}

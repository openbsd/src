/*	$OpenBSD: ike_main_mode.c,v 1.4 1998/11/20 07:34:45 niklas Exp $	*/
/*	$EOM: ike_main_mode.c,v 1.63 1998/11/20 07:17:43 niklas Exp $	*/

/*
 * Copyright (c) 1998 Niklas Hallqvist.  All rights reserved.
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
#include <stdlib.h>
#include <string.h>

#include "attribute.h"
#include "conf.h"
#include "constants.h"
#include "crypto.h"
#include "dh.h"
#include "doi.h"
#include "exchange.h"
#include "hash.h"
#include "ike_auth.h"
#include "ike_main_mode.h"
#include "ipsec.h"
#include "ipsec_doi.h"
#include "isakmp.h"
#include "log.h"
#include "math_group.h"
#include "message.h"
#include "prf.h"
#include "sa.h"
#include "transport.h"
#include "util.h"

static int attribute_unacceptable (u_int16_t, u_int8_t *, u_int16_t, void *);
static int ike_main_mode_validate_prop (struct sa *);
static int initiator_send_SA (struct message *);
static int initiator_recv_SA (struct message *);
static int initiator_send_KE_NONCE (struct message *);
static int initiator_recv_KE_NONCE (struct message *);
static int initiator_send_ID_AUTH (struct message *);
static int initiator_recv_ID_AUTH (struct message *);
static int responder_recv_SA (struct message *);
static int responder_send_SA (struct message *);
static int responder_recv_KE_NONCE (struct message *);
static int responder_send_KE_NONCE (struct message *);
static int responder_recv_ID_AUTH (struct message *);
static int responder_send_ID_AUTH (struct message *);
static int recv_KE_NONCE (struct message *);
static int recv_ID_AUTH (struct message *);
static int send_KE_NONCE (struct message *, size_t);
static int send_ID_AUTH (struct message *);
static int post_exchange_KE_NONCE (struct message *);

int (*ike_main_mode_initiator[]) (struct message *) = {
  initiator_send_SA,
  initiator_recv_SA,
  initiator_send_KE_NONCE,
  initiator_recv_KE_NONCE,
  initiator_send_ID_AUTH,
  initiator_recv_ID_AUTH
};

int (*ike_main_mode_responder[]) (struct message *) = {
  responder_recv_SA,
  responder_send_SA,
  responder_recv_KE_NONCE,
  responder_send_KE_NONCE,
  responder_recv_ID_AUTH,
  responder_send_ID_AUTH
};

/* Offer a set of transforms to the responder.  */
static int
initiator_send_SA (struct message *msg)
{
  struct ipsec_exch *ie = msg->exchange->data;
  u_int8_t *proposal = 0, *sa_buf = 0, *attr;
  u_int8_t **transform = 0;
  size_t transforms_len = 0, proposal_len, sa_len;
  size_t *transform_len = 0;
  struct conf_list *conf, *life_conf;
  struct conf_list_node *xf, *life;
  int i, value, update_nextp;
  struct payload *p;
  struct proto *proto;

  /* Get the list of transforms.  */
  conf = conf_get_list ("Main mode", "Offered-transforms");
  if (!conf)
    return -1;

  transform = calloc (conf->cnt, sizeof *transform);
  if (!transform)
    goto bail_out;
  transform_len = calloc (conf->cnt, sizeof *transform_len);
  if (!transform_len)
    goto bail_out;

  for (xf = TAILQ_FIRST (&conf->fields), i = 0; i < conf->cnt;
       i++, xf = TAILQ_NEXT (xf, link))
    {
      /* XXX The sizing needs to be dynamic.  */
      transform[i]
	= malloc (ISAKMP_TRANSFORM_SA_ATTRS_OFF + 16 * ISAKMP_ATTR_VALUE_OFF);
      if (!transform[i])
	goto bail_out;
      SET_ISAKMP_TRANSFORM_NO (transform[i], i);
      SET_ISAKMP_TRANSFORM_ID (transform[i], IPSEC_TRANSFORM_KEY_IKE);
      SET_ISAKMP_TRANSFORM_RESERVED (transform[i], 0);

      attr = transform[i] + ISAKMP_TRANSFORM_SA_ATTRS_OFF;

      if (attribute_set_constant (xf->field, "ENCRYPTION_ALGORITHM",
				  ike_encrypt_cst,
				  IKE_ATTR_ENCRYPTION_ALGORITHM, &attr))
	goto bail_out;

      if (attribute_set_constant (xf->field, "HASH_ALGORITHM", ike_hash_cst,
				  IKE_ATTR_HASH_ALGORITHM, &attr))
	goto bail_out;

      if (attribute_set_constant (xf->field, "AUTHENTICATION_METHOD",
				  ike_auth_cst, IKE_ATTR_AUTHENTICATION_METHOD,
				  &attr))
	goto bail_out;

      if (attribute_set_constant (xf->field, "GROUP_DESCRIPTION",
				  ike_group_desc_cst,
				  IKE_ATTR_GROUP_DESCRIPTION, &attr))
	{
	  /*
	   * If no group description exists, try looking for a user-defined
	   * one.
	   */
	  if (attribute_set_constant (xf->field, "GROUP_TYPE", ike_group_cst,
				      IKE_ATTR_GROUP_TYPE, &attr))
	    goto bail_out;

#if 0
	  if (attribute_set_bignum (xf->field, "GROUP_PRIME",
				    IKE_ATTR_GROUP_PRIME, &attr))
	    goto bail_out;

	  if (attribute_set_bignum (xf->field, "GROUP_GENERATOR_2",
				    IKE_ATTR_GROUP_GENERATOR_2, &attr))
	    goto bail_out;

	  if (attribute_set_bignum (xf->field, "GROUP_GENERATOR_2",
				    IKE_ATTR_GROUP_GENERATOR_2, &attr))
	    goto bail_out;

	  if (attribute_set_bignum (xf->field, "GROUP_CURVE_A",
				    IKE_ATTR_GROUP_CURVE_A, &attr))
	    goto bail_out;

	  if (attribute_set_bignum (xf->field, "GROUP_CURVE_B",
				    IKE_ATTR_GROUP_CURVE_B, &attr))
	    goto bail_out;
#endif
	}

      /*
       * Life durations are special, we should be able to specify
       * several, one per type.
       */
      life_conf = conf_get_list (xf->field, "Life");
      if (life_conf)
	{
	  for (life = TAILQ_FIRST (&life_conf->fields); life;
	       life = TAILQ_NEXT (life, link))
	    {
	      attribute_set_constant (life->field, "LIFE_TYPE",
				      ike_duration_cst, IKE_ATTR_LIFE_TYPE,
				      &attr);

	      /* XXX Does only handle 16-bit entities!  */
	      value = conf_get_num (life->field, "LIFE_DURATION");
	      if (value)
		attr
		  = attribute_set_basic (attr, IKE_ATTR_LIFE_DURATION, value);
	    }
	}

      attribute_set_constant (xf->field, "PRF", ike_prf_cst, IKE_ATTR_PRF,
			      &attr);

      value = conf_get_num (xf->field, "KEY_LENGTH");
      if (value)
	attr = attribute_set_basic (attr, IKE_ATTR_KEY_LENGTH, value);

      value = conf_get_num (xf->field, "FIELD_SIZE");
      if (value)
	attr = attribute_set_basic (attr, IKE_ATTR_FIELD_SIZE, value);

      value = conf_get_num (xf->field, "GROUP_ORDER");
      if (value)
	attr = attribute_set_basic (attr, IKE_ATTR_GROUP_ORDER, value);

      /* Record the real transform size.  */
      transforms_len += transform_len[i] = attr - transform[i];
    }

  proposal_len = ISAKMP_PROP_SPI_OFF;
  proposal = malloc (proposal_len);
  if (!proposal)
    goto bail_out;
  SET_ISAKMP_PROP_NO (proposal, 1);
  SET_ISAKMP_PROP_PROTO (proposal, ISAKMP_PROTO_ISAKMP);
  SET_ISAKMP_PROP_SPI_SZ (proposal, 0);
  SET_ISAKMP_PROP_NTRANSFORMS (proposal, conf->cnt);

  /* XXX I would like to see this factored out.  */
  proto = calloc (1, sizeof *proto);
  if (!proto)
    goto bail_out;
  proto->no = 1;
  proto->proto = ISAKMP_PROTO_ISAKMP;
  proto->sa = TAILQ_FIRST (&msg->exchange->sa_list);
  TAILQ_INSERT_TAIL (&TAILQ_FIRST (&msg->exchange->sa_list)->protos, proto,
		     link);

  sa_len = ISAKMP_SA_SIT_OFF + IPSEC_SIT_SIT_LEN;
  sa_buf = malloc (sa_len);
  if (!sa_buf)
    goto bail_out;
  SET_ISAKMP_SA_DOI (sa_buf, IPSEC_DOI_IPSEC);
  SET_IPSEC_SIT_SIT (sa_buf + ISAKMP_SA_SIT_OFF, IPSEC_SIT_IDENTITY_ONLY);

  /*
   * Add the payloads.  As this is a SA, we need to recompute the
   * lengths of the payloads containing others.
   */
  if (message_add_payload (msg, ISAKMP_PAYLOAD_SA, sa_buf, sa_len, 1))
    goto bail_out;
  SET_ISAKMP_GEN_LENGTH (sa_buf,
			 sa_len + proposal_len + transforms_len);
  sa_buf = 0;

  if (message_add_payload (msg, ISAKMP_PAYLOAD_PROPOSAL, proposal,
			   proposal_len, 0))
    goto bail_out;
  SET_ISAKMP_GEN_LENGTH (proposal, proposal_len + transforms_len);
  proposal = 0;

  update_nextp = 0;
  for (i = 0; i < conf->cnt; i++)
    {
      if (message_add_payload (msg, ISAKMP_PAYLOAD_TRANSFORM, transform[i],
			       transform_len[i], update_nextp))
	goto bail_out;
      update_nextp = 1;
      transform[i] = 0;
    }

  /* Save SA payload body in ie->sa_i_b, length ie->sa_i_b_len.  */
  ie->sa_i_b_len = sa_len + proposal_len + transforms_len - ISAKMP_GEN_SZ;
  ie->sa_i_b = malloc (ie->sa_i_b_len);
  if (!ie->sa_i_b)
    goto bail_out;
  memcpy (ie->sa_i_b,
	  TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_SA])->p + ISAKMP_GEN_SZ,
	  sa_len - ISAKMP_GEN_SZ);
  memcpy (ie->sa_i_b + sa_len - ISAKMP_GEN_SZ,
	  TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_PROPOSAL])->p,
	  proposal_len);
  transforms_len = 0;
  for (i = 0, p = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_TRANSFORM]);
       i < conf->cnt; i++, p = TAILQ_NEXT (p, link))
    {
      memcpy (ie->sa_i_b + sa_len + proposal_len + transforms_len
	      - ISAKMP_GEN_SZ,
	      p->p, transform_len[i]);
      transforms_len += transform_len[i];
    }

  return 0;

 bail_out:
  if (sa_buf)
    free (sa_buf);
  if (proposal)
    free (proposal);
  if (transform)
    {
      for (i = 0; i < conf->cnt; i++)
	if (transform[i])
	  free (transform[i]);
      free (transform);
    }
  if (transform_len)
    free (transform_len);
  if (conf)
    conf_free_list (conf);
  return -1;
}

/* Figure out what transform the responder chose.  */
static int
initiator_recv_SA (struct message *msg)
{
  struct sa *sa;
  struct proto *proto;
  struct payload *sa_p = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_SA]);
  struct payload *prop = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_PROPOSAL]);
  struct payload *xf = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_TRANSFORM]);

  /*
   * IKE requires that only one SA with only one proposal exists and since
   * we are getting an answer on our transform offer, only one transform.
   */
  if (TAILQ_NEXT (sa_p, link) || TAILQ_NEXT (prop, link)
      || TAILQ_NEXT (xf, link))
    {
      log_print ("initiator_recv_SA: "
		 "multiple SA, proposal or transform payloads in main mode");
      /* XXX Is there a better notification type?  */
      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 0, 0);
      return -1;
    }

  sa = TAILQ_FIRST (&msg->exchange->sa_list);

  /* Build the protection suite in our SA.  */
  if (sa_add_transform (sa, xf, msg->exchange->initiator, &proto))
    {
      /* XXX Log?  */
      return -1;
    }

  /* XXX Check that the chosen transform matches an offer.  */
  ipsec_decode_transform (msg, sa, TAILQ_FIRST (&sa->protos), xf->p);

  /* Mark the SA as handled.  */
  sa_p->flags |= PL_MARK;
  return 0;
}

/* Send our public DH value and a nonce to the responder.  */
static int
initiator_send_KE_NONCE (struct message *msg)
{
  struct ipsec_exch *ie = msg->exchange->data;

  ie->g_x_len = dh_getlen (ie->group);

  /* XXX I want a better way to specify the nonce's size.  */
  return send_KE_NONCE (msg, 16);
}

/* Accept receptor's public DH value and nonce.  */
static int
initiator_recv_KE_NONCE (struct message *msg)
{
  if (recv_KE_NONCE (msg))
    return -1;

  return post_exchange_KE_NONCE (msg);
}

static int
initiator_send_ID_AUTH (struct message *msg)
{
  msg->exchange->flags |= EXCHANGE_FLAG_ENCRYPT;
  return send_ID_AUTH (msg);
}

static int
initiator_recv_ID_AUTH (struct message *msg)
{
  return recv_ID_AUTH (msg);
}

/*
 * Accept a set of transforms offered by the initiator and chose one we can
 * handle.
 */
static int
responder_recv_SA (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct sa *sa = TAILQ_FIRST (&exchange->sa_list);
  struct payload *sa_p = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_SA]);
  struct payload *prop = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_PROPOSAL]);
  struct ipsec_exch *ie = exchange->data;

  /* Mark the SA as handled.  */
  sa_p->flags |= PL_MARK;

  /* IKE requires that only one SA with only one proposal exists.  */
  if (TAILQ_NEXT (sa_p, link) || TAILQ_NEXT (prop, link))
    {
      log_print ("responder_recv_SA: "
		 "multiple SA or proposal payloads in main mode");
      /* XXX Is there a better notification type?  */
      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 0, 0);
      return -1;
    }

  /* Chose a transform from the SA.  */
  if (message_negotiate_sa (msg, ike_main_mode_validate_prop)
      || !TAILQ_FIRST (&sa->protos))
    return -1;

  /* XXX Move into message_negotiate_sa?  */
  ipsec_decode_transform (msg, sa, TAILQ_FIRST (&sa->protos),
			  TAILQ_FIRST (&sa->protos)->chosen->p);

  /*
   * Check that the mandatory attributes: encryption, hash, authentication
   * method and Diffie-Hellman group description, has been supplied.
   */
  if (!exchange->crypto || !ie->hash || !ie->ike_auth || !ie->group)
    {
      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 0, 0);
      return -1;
    }

  /* Save the body for later hash computation.  */
  ie->sa_i_b_len = GET_ISAKMP_GEN_LENGTH (sa_p->p) - ISAKMP_GEN_SZ;
  ie->sa_i_b = malloc (ie->sa_i_b_len);
  if (!ie->sa_i_b)
    {
      /* XXX How to log and notify peer?  */
      return -1;
    }
  memcpy (ie->sa_i_b, sa_p->p + ISAKMP_GEN_SZ, ie->sa_i_b_len);

  return 0;
}

/* Reply with the transform we chose.  */
static int
responder_send_SA (struct message *msg)
{
  /* Add the SA payload with the transform that was chosen.  */
  return message_add_sa_payload (msg);
}

/* Accept initiator's public DH value and nonce.  */
static int
responder_recv_KE_NONCE (struct message *msg)
{
  return recv_KE_NONCE (msg);
}

/* Send our public DH value and a nonce to the initiator.  */
static int
responder_send_KE_NONCE (struct message *msg)
{
  /* XXX Should we really just use the initiator's nonce size?  */
  if (send_KE_NONCE (msg, msg->exchange->nonce_i_len))
    return -1;

  /*
   * Calculate DH values & key material in parallel with the message going
   * on a roundtrip over the wire.
   */
  message_register_post_send (msg,
			      (void (*) (struct message *))
			      post_exchange_KE_NONCE);

  return 0;
}

/* Receive ID and HASH and check that the exchange has been consistent.  */
static int
responder_recv_ID_AUTH (struct message *msg)
{
  return recv_ID_AUTH (msg);
}

static int
responder_send_ID_AUTH (struct message *msg)
{
  return send_ID_AUTH (msg);
}

/* Send our public DH value and a nonce to the peer.  */
static int
send_KE_NONCE (struct message *msg, size_t nonce_sz)
{
  /* Public DH key.  */
  if (ipsec_gen_g_x (msg))
    {
      /* XXX How to log and notify peer?  */
      return -1;
    }

  /* Generate a nonce, and add it to the message.  */
  if (exchange_gen_nonce (msg, nonce_sz))
    {
      /* XXX Log?  */
      return -1;
    }

  /* Try to add certificates which are acceptable for the CERTREQs */
  if (exchange_add_certs (msg))
    {
      /* XXX Log? */
      return -1;
    }

  return 0;
}

/* Receive our peer's public DH value and nonce.  */
static int
recv_KE_NONCE (struct message *msg)
{
  /* Copy out the initiator's DH public value.  */
  if (ipsec_save_g_x (msg))
    {
      /* XXX How to log and notify peer?  */
      return -1;
    }

  /* Copy out the initiator's nonce.  */
  if (exchange_save_nonce (msg))
    {
      /* XXX How to log and notify peer?  */
      return -1;
    }

  /* Copy out the initiator's cert requests.  */
  if (exchange_save_certreq (msg))
    {
      /* XXX How to log and notify peer?  */
      return -1;
    }

  return 0;
}

/*
 * Compute DH values and key material.  This is done in a post-send function
 * as that means we can do parallel work in both the initiator and responder
 * thus speeding up exchanges.
 */
static int
post_exchange_KE_NONCE (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct prf *prf;
  struct hash *hash = ie->hash;
  enum cryptoerr err;

  /* Compute Diffie-Hellman shared value.  */
  ie->g_xy = malloc (ie->g_x_len);
  if (!ie->g_xy)
    {
      /* XXX How to log and notify peer?  */
      return -1;
    }
  dh_create_shared (ie->group, ie->g_xy,
		    exchange->initiator ? ie->g_xr : ie->g_xi);
  log_debug_buf (LOG_MISC, 80, "post_exchange_KE_NONCE: g^xy", ie->g_xy,
		 ie->g_x_len);

  /* Compute the SKEYID depending on the authentication method.  */
  ie->skeyid = ie->ike_auth->gen_skeyid (exchange, &ie->skeyid_len);
  if (!ie->skeyid)
    {
      /* XXX Log and teardown?  */
      return -1;
    }
  log_debug_buf (LOG_MISC, 80, "post_exchange_KE_NONCE: SKEYID",
		 ie->skeyid, ie->skeyid_len);

  /* SKEYID_d.  */
  ie->skeyid_d = malloc (ie->skeyid_len);
  if (!ie->skeyid_d)
    {
      /* XXX Log and teardown?  */
      return -1;
    }
  prf = prf_alloc (ie->prf_type, hash->type, ie->skeyid,
		   ie->skeyid_len);
  if (!prf)
    {
      /* XXX Log and teardown?  */
      return -1;
    }
  prf->Init (prf->prfctx);
  prf->Update (prf->prfctx, ie->g_xy, ie->g_x_len);
  prf->Update (prf->prfctx, exchange->cookies, ISAKMP_HDR_COOKIES_LEN);
  prf->Update (prf->prfctx, "\0", 1);
  prf->Final (ie->skeyid_d, prf->prfctx);
  log_debug_buf (LOG_MISC, 80, "post_exchange_KE_NONCE: SKEYID_d",
		 ie->skeyid_d, ie->skeyid_len);

  /* SKEYID_a.  */
  ie->skeyid_a = malloc (ie->skeyid_len);
  if (!ie->skeyid_a)
    {
      /* XXX Log & teardown?  */
      prf_free (prf);
      return -1;
    }
  prf->Init (prf->prfctx);
  prf->Update (prf->prfctx, ie->skeyid_d, ie->skeyid_len);
  prf->Update (prf->prfctx, ie->g_xy, ie->g_x_len);
  prf->Update (prf->prfctx, exchange->cookies, ISAKMP_HDR_COOKIES_LEN);
  prf->Update (prf->prfctx, "\1", 1);
  prf->Final (ie->skeyid_a, prf->prfctx);
  log_debug_buf (LOG_MISC, 80, "post_exchange_KE_NONCE: SKEYID_a",
		 ie->skeyid_a, ie->skeyid_len);

  /* SKEYID_e.  */
  ie->skeyid_e = malloc (ie->skeyid_len);
  if (!ie->skeyid_e)
    {
      /* XXX Log and teardown?  */
      prf_free (prf);
      return -1;
    }
  prf->Init (prf->prfctx);
  prf->Update (prf->prfctx, ie->skeyid_a, ie->skeyid_len);
  prf->Update (prf->prfctx, ie->g_xy, ie->g_x_len);
  prf->Update (prf->prfctx, exchange->cookies, ISAKMP_HDR_COOKIES_LEN);
  prf->Update (prf->prfctx, "\2", 1);
  prf->Final (ie->skeyid_e, prf->prfctx);
  prf_free (prf);
  log_debug_buf (LOG_MISC, 80, "post_exchange_KE_NONCE: SKEYID_e",
		 ie->skeyid_e, ie->skeyid_len);

  /* Key length determination.  */
  if (!exchange->key_length)
    exchange->key_length = exchange->crypto->keymax;

  /* Derive a longer key from skeyid_e */
  if (ie->skeyid_len < exchange->key_length)
    {
      u_int16_t len, keylen;
      u_int8_t *key, *p;
      
      prf = prf_alloc (ie->prf_type, hash->type, ie->skeyid_e, ie->skeyid_len);
      if (!prf)
	{
	  /* XXX - notify peer */
	  return -1;
	}

      /* Make keylen a multiple of prf->blocksize */
      keylen = exchange->key_length;
      if (keylen % prf->blocksize)
	keylen += prf->blocksize - (keylen % prf->blocksize);

      if ((key = malloc (keylen)) == NULL)
	{
	  /* XXX - notify peer */
	  return -1;
	}

      prf->Init (prf->prfctx);
      prf->Update (prf->prfctx, "\0", 1);
      prf->Final (key, prf->prfctx);

      for (len = prf->blocksize, p = key; len < exchange->key_length; 
	   len += prf->blocksize, p += prf->blocksize)
	{
	  prf->Init (prf->prfctx);
	  prf->Update (prf->prfctx, p, prf->blocksize);
	  prf->Final (p + prf->blocksize, prf->prfctx);
	}
      prf_free (prf);

      /* Set up our keystate using the derived encryption key */
      exchange->keystate
	= crypto_init (exchange->crypto, key, exchange->key_length, &err);

      free (key);
    }
  else
    /* Set up our keystate using the raw skeyid_e */
    exchange->keystate = crypto_init (exchange->crypto, ie->skeyid_e,
				      exchange->key_length, &err);
  /* Special handling for DES weak keys */
  if (!exchange->keystate && err == EWEAKKEY
      && (exchange->key_length << 1) <= ie->skeyid_len)
    {
      log_print ("post_exchange_KE_NONCE: weak key, trying subseq. skeyid_e");
      exchange->keystate = crypto_init (exchange->crypto,
					ie->skeyid_e + exchange->key_length,
					exchange->key_length, &err);
    }

  if (!exchange->keystate)
    {
      log_print ("post_exchange_KE_NONCE: "
		 "exchange->crypto->init () failed: %d", err);

      /*
       * XXX We really need to know if problems are of transient nature
       * or fatal (like failed assertions etc.)
       */
      return -1;
    }

  /* Setup IV.  XXX Only for CBC transforms, no?  */
  hash->Init (hash->ctx);
  hash->Update (hash->ctx, ie->g_xi, ie->g_x_len);
  hash->Update (hash->ctx, ie->g_xr, ie->g_x_len);
  hash->Final (hash->digest, hash->ctx);
  crypto_init_iv (exchange->keystate, hash->digest,
		  exchange->crypto->blocksize);

  return 0;
}

static int
send_ID_AUTH (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  u_int8_t *buf;
  char header[80];
  size_t sz;
  struct sockaddr *src;
  int src_len;
  int initiator = exchange->initiator;
  u_int8_t **id;
  size_t *id_len;

  /* Choose the right fields to fill-in.  */
  id = initiator ? &exchange->id_i : &exchange->id_r;
  id_len = initiator ? &exchange->id_i_len : &exchange->id_r_len;

  /* XXX This begs to be more dynamic.  */
  sz = ISAKMP_ID_DATA_OFF + 4;
  buf = malloc (sz);
  if (!buf)
    {
      /* XXX Log?  */
      return -1;
    }
  msg->transport->vtbl->get_src (msg->transport, &src, &src_len);
  /* XXX Assumes IPv4.  */
  SET_ISAKMP_ID_TYPE (buf, IPSEC_ID_IPV4_ADDR);
  SET_IPSEC_ID_PROTO (buf + ISAKMP_ID_DOI_DATA_OFF, 0);
  SET_IPSEC_ID_PORT (buf + ISAKMP_ID_DOI_DATA_OFF, 0);
  /* Already in network byteorder.  */
  memcpy (buf + ISAKMP_ID_DATA_OFF,
	  &((struct sockaddr_in *)src)->sin_addr.s_addr, sizeof (in_addr_t));
  if (message_add_payload (msg, ISAKMP_PAYLOAD_ID, buf, sz, 1))
    {
      /* XXX Log?  */
      free (buf);
      return -1;
    }
  *id_len = sz - ISAKMP_GEN_SZ;
  *id = malloc (*id_len);
  if (!*id)
    {
      /* XXX Log?  */
      return -1;
    }
  memcpy (*id, buf + ISAKMP_GEN_SZ, *id_len);
  snprintf (header, 80, "send_ID_AUTH: %s",
	    constant_name (ipsec_id_cst, GET_ISAKMP_ID_TYPE (buf)));
  log_debug_buf (LOG_MISC, 40, header, buf + ISAKMP_ID_DATA_OFF,
		 sz - ISAKMP_ID_DATA_OFF);

  if (ie->ike_auth->encode_hash (msg) == -1)
    {
      /* XXX Log? */
      return -1;
    }

  /*
   * XXX Many people say the COMMIT flag is just junk, especially in Phase 1.
   */
#ifdef notyet
  if ((exchange->flags & EXCHANGE_FLAG_COMMITTED) == 0)
    exchange->flags |= EXCHANGE_FLAG_I_COMMITTED;
#endif

  return 0;
}

/* Receive ID and HASH and check that the exchange has been consistent.  */
static int
recv_ID_AUTH (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct payload *payload;
  struct prf *prf;
  struct hash *hash = ie->hash;
  char header[80];
  size_t hashsize = hash->hashsize;
  int initiator = exchange->initiator;
  u_int8_t **hash_p, **id;
  size_t *id_len;

  /* Choose the right fields to fill in */
  hash_p = initiator ? &ie->hash_r : &ie->hash_i;
  id = initiator ? &exchange->id_r : &exchange->id_i;
  id_len = initiator ? &exchange->id_r_len : &exchange->id_i_len;

  /* XXX Do I really have to save the ID in the SA?  */
  payload = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_ID]);
  *id_len = GET_ISAKMP_GEN_LENGTH (payload->p) - ISAKMP_GEN_SZ;
  *id = malloc (*id_len);
  if (!*id)
    {
      /* XXX Log?  */
      return -1;
    }
  memcpy (*id, payload->p + ISAKMP_GEN_SZ, *id_len);
  snprintf (header, 80, "recv_ID_AUTH: %s",
	    constant_name (ipsec_id_cst, GET_ISAKMP_ID_TYPE (payload->p)));
  log_debug_buf (LOG_MISC, 40, header, payload->p + ISAKMP_ID_DATA_OFF,
		 *id_len + ISAKMP_GEN_SZ - ISAKMP_ID_DATA_OFF);
  payload->flags |= PL_MARK;

  /* The decoded hash will be in ie->hash_r or ie->hash_i */
  if (ie->ike_auth->decode_hash (msg) == -1)
    {
      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 0, 0);
      return -1;
    }
  
  /* Allocate the prf and start calculating his HASH.  */
  prf = prf_alloc (ie->prf_type, hash->type, ie->skeyid, ie->skeyid_len);
  if (!prf)
    {
      /* XXX Log?  */
      return -1;
    }
  prf->Init (prf->prfctx);
  prf->Update (prf->prfctx, initiator ? ie->g_xr : ie->g_xi, ie->g_x_len);
  prf->Update (prf->prfctx, initiator ? ie->g_xi : ie->g_xr, ie->g_x_len);
  prf->Update (prf->prfctx,
	       exchange->cookies
	       + (initiator ? ISAKMP_HDR_RCOOKIE_OFF : ISAKMP_HDR_ICOOKIE_OFF),
	       ISAKMP_HDR_ICOOKIE_LEN);
  prf->Update (prf->prfctx,
	       exchange->cookies
	       + (initiator ? ISAKMP_HDR_ICOOKIE_OFF : ISAKMP_HDR_RCOOKIE_OFF),
	       ISAKMP_HDR_ICOOKIE_LEN);
  prf->Update (prf->prfctx, ie->sa_i_b, ie->sa_i_b_len);
  prf->Update (prf->prfctx, *id, *id_len);
  prf->Final (hash->digest, prf->prfctx);
  prf_free (prf);
  snprintf (header, 80, "recv_ID_AUTH: computed HASH_%c",
	    initiator ? 'R' : 'I');
  log_debug_buf (LOG_MISC, 80, header, hash->digest, hashsize);

  /* Check that the hash we got matches the one we computed.  */
  if (memcmp (*hash_p, hash->digest, hashsize) != 0)
    {
      /* XXX Log?  */
      return -1;
    }

  return 0;
}

struct attr_node {
  LIST_ENTRY (attr_node) link;
  u_int16_t type;
};

struct validation_state {
  struct conf_list_node *xf;
  LIST_HEAD (attr_head, attr_node) attrs;
  char *life;
};

static int
ike_main_mode_validate_prop (struct sa *sa)
{
  struct conf_list *conf, *tags;
  struct conf_list_node *xf, *tag;
  struct proto *proto;
  struct validation_state vs;
  struct attr_node *node;

  /* Get the list of transforms.  */
  conf = conf_get_list ("Main mode", "Accepted-transforms");
  if (!conf)
    return 0;

  for (xf = TAILQ_FIRST (&conf->fields); xf; xf = TAILQ_NEXT (xf, link))
    {
      for (proto = TAILQ_FIRST (&sa->protos); proto;
	   proto = TAILQ_NEXT (proto, link))
	{
	  /* Mark all attributes in our policy as unseen.  */
	  LIST_INIT (&vs.attrs);
	  vs.xf = xf;
	  vs.life = 0;
	  if (attribute_map (proto->chosen->p + ISAKMP_TRANSFORM_SA_ATTRS_OFF,
			     GET_ISAKMP_GEN_LENGTH (proto->chosen->p)
			     - ISAKMP_TRANSFORM_SA_ATTRS_OFF,
			     attribute_unacceptable, &vs) == -1)
	    goto try_next;

	  /* Sweep over unseen tags in this section.  */
	  tags = conf_get_tag_list (xf->field);
	  for (tag = TAILQ_FIRST (&tags->fields); tag;
	       tag = TAILQ_NEXT (tag, link))
	    for (node = LIST_FIRST (&vs.attrs); node;
		 node = LIST_NEXT (node, link))
	      LIST_REMOVE (node, link);

	  /* Are there leftover tags in this section?  */
	  node = LIST_FIRST (&vs.attrs);
	  if (node)
	    {
	      /* We have attributes in our policy we have not seen.  */
	      while (node)
		{
		  LIST_REMOVE (node, link);
		  node = LIST_FIRST (&vs.attrs);
		}
	      goto try_next;
	    }
	}

      /* All protocols were OK, we succeeded.  */
      log_debug (LOG_MISC, 20, "ike_main_mode_validate_prop: success");
      conf_free_list (conf);
      return 1;

    try_next:
    }

  log_debug (LOG_MISC, 20, "ike_main_mode_validate_prop: failure");
  conf_free_list (conf);
  return 0;
}

static int
attribute_unacceptable (u_int16_t type, u_int8_t *value, u_int16_t len,
			void *vvs)
{
  struct validation_state *vs = vvs;
  struct conf_list_node *xf = vs->xf;
  struct conf_list *life_conf;
  struct conf_list_node *life;
  char *tag = constant_lookup (ike_attr_cst, type);
  char *str;
  struct constant_map *map;
  struct attr_node *node;
  int rv;

  if (!tag)
    {
      log_print ("attribute_unacceptable: attribute type %d not known", type);
      return 1;
    }

  switch (type)
    {
    case IKE_ATTR_ENCRYPTION_ALGORITHM:
    case IKE_ATTR_HASH_ALGORITHM:
    case IKE_ATTR_AUTHENTICATION_METHOD:
    case IKE_ATTR_GROUP_DESCRIPTION:
    case IKE_ATTR_GROUP_TYPE:
    case IKE_ATTR_PRF:
      str = conf_get_str (xf->field, tag);
      if (!str)
	/* This attribute does not exist in this policy.  */
	return 1;

      map = constant_link_lookup (ike_attr_cst, type);
      if (!map)
	return 1;

      if (constant_value (map, str) == decode_16 (value))
	{
	  /* Mark this attribute as seen.  */
	  node = malloc (sizeof *node);
	  if (!node)
	    {
	      log_print ("attribute_unacceptable: malloc failed");
	      return 1;
	    }
	  node->type = type;
	  LIST_INSERT_HEAD (&vs->attrs, node, link);
	  return 0;
	}
      return 1;

    case IKE_ATTR_GROUP_PRIME:
    case IKE_ATTR_GROUP_GENERATOR_1:
    case IKE_ATTR_GROUP_GENERATOR_2:
    case IKE_ATTR_GROUP_CURVE_A:
    case IKE_ATTR_GROUP_CURVE_B:
      /* XXX Bignums not handled yet.  */
      return 1;

    case IKE_ATTR_LIFE_TYPE:
    case IKE_ATTR_LIFE_DURATION:
      rv = 1;
      life_conf = conf_get_list (xf->field, "Life");
      if (!life_conf)
	/* Life attributes given, but not in our policy.  */
	goto bail_out;

      /*
       * Each lifetime type must match, otherwise we turn the proposal down.
       * In order to do this we need to find the specific section of our
       * policy's "Life" list and match its duration
       */
      vs->life = 0;
      switch (type)
	{
	case IKE_ATTR_LIFE_TYPE:
	  for (life = TAILQ_FIRST (&life_conf->fields); life;
	       life = TAILQ_NEXT (life, link))
	    {
	      str = conf_get_str (life->field, "SA_LIFE_TYPE");
	      if (!str)
		/* XXX Log this?  */
		continue;

	      /*
	       * If this is the type we are looking at, save a pointer
	       * to this section in vs->life.
	       */
	      if (constant_value (ike_duration_cst, str) == decode_16 (value))
		{
		  vs->life = life->field;
		  rv = 0;
		  goto bail_out;
		}
	    }
	  /* XXX Log?  */
	  break;

	case IKE_ATTR_LIFE_DURATION:
	  rv
	    = conf_match_num (vs->life, "SA_LIFE_DURATION", decode_16 (value));
	  break;
	}

    bail_out:
      conf_free_list (life_conf);
      return rv;

    case IKE_ATTR_KEY_LENGTH:
    case IKE_ATTR_FIELD_SIZE:
    case IKE_ATTR_GROUP_ORDER:
      if (conf_match_num (xf->field, tag, decode_16 (value)))
	{
	  /* Mark this attribute as seen.  */
	  node = malloc (sizeof *node);
	  if (!node)
	    {
	      log_print ("attribute_unacceptable: malloc failed");
	      return 1;
	    }
	  node->type = type;
	  LIST_INSERT_HEAD (&vs->attrs, node, link);
	  return 0;
	}
      return 1;
    }
  return 1;
}

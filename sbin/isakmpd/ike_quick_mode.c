/*	$OpenBSD: ike_quick_mode.c,v 1.8 1999/03/31 14:28:34 niklas Exp $	*/
/*	$EOM: ike_quick_mode.c,v 1.71 1999/03/31 14:18:45 niklas Exp $	*/

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

#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "attribute.h"
#include "conf.h"
#include "dh.h"
#include "doi.h"
#include "exchange.h"
#include "hash.h"
#include "ike_quick_mode.h"
#include "ipsec.h"
#include "log.h"
#include "math_group.h"
#include "message.h"
#include "prf.h"
#include "sa.h"
#include "transport.h"

static void gen_g_xy (struct message *);
static int initiator_send_HASH_SA_NONCE (struct message *);
static int initiator_recv_HASH_SA_NONCE (struct message *);
static int initiator_send_HASH (struct message *);
static void post_quick_mode (struct message *);
static int responder_recv_HASH_SA_NONCE (struct message *);
static int responder_send_HASH_SA_NONCE (struct message *);
static int responder_recv_HASH (struct message *);

int (*ike_quick_mode_initiator[]) (struct message *) = {
  initiator_send_HASH_SA_NONCE,
  initiator_recv_HASH_SA_NONCE,
  initiator_send_HASH
};

int (*ike_quick_mode_responder[]) (struct message *) = {
  responder_recv_HASH_SA_NONCE,
  responder_send_HASH_SA_NONCE,
  responder_recv_HASH
};

/*
 * Offer several sets of transforms to the responder.
 * XXX Split this huge function up and look for common code with main mode.
 */
static int
initiator_send_HASH_SA_NONCE (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct doi *doi = exchange->doi;
  struct ipsec_exch *ie = exchange->data;
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  u_int8_t ***transform = 0, ***new_transform;
  u_int8_t **proposal = 0, **new_proposal;
  u_int8_t *sa_buf = 0, *attr, *saved_nextp_sa, *saved_nextp_prop;
  u_int8_t *buf, *id;
  u_int8_t *spi;
  size_t spi_sz, sz;
  struct prf *prf;
  struct hash *hash = hash_get (isa->hash);
  size_t hashsize = hash->hashsize;
  size_t proposal_len = 0, proposals_len = 0, sa_len;
  size_t **transform_len = 0, **new_transform_len;
  size_t *transforms_len = 0, *new_transforms_len;
  int *transform_cnt = 0, *new_transform_cnt;
  char header[80];
  int i, suite_no, prop_no, prot_no, xf_no, value, update_nextp, protocol_num;
  int prop_cnt = 0;
  struct proto *proto;
  struct conf_list *suite_conf = 0, *prot_conf = 0, *xf_conf = 0, *life_conf;
  struct conf_list_node *suite, *prot, *xf, *life;
  struct constant_map *id_map;
  char *protocol_id, *transform_id;
  char *local_id, *remote_id;

  /* We want a HASH payload to start with.  XXX Share with ike_main_mode.c?  */
  buf = malloc (ISAKMP_HASH_SZ + hashsize);
  if (!buf)
    {
      /* XXX Log?  */
      return -1;
    }
  if (message_add_payload (msg, ISAKMP_PAYLOAD_HASH, buf,
			   ISAKMP_HASH_SZ + hashsize, 1))
    {
      /* XXX Log?  */
      free (buf);
      return -1;
    }
    
  /* Get the list of protocol suites.  */
  suite_conf = conf_get_list (exchange->policy, "Suites");
  if (!suite_conf)
    return -1;

  for (suite = TAILQ_FIRST (&suite_conf->fields), suite_no = prop_no = 0;
       suite_no < suite_conf->cnt;
       suite_no++, suite = TAILQ_NEXT (suite, link))
    {
      /* Now get each protocol in this specific protocol suite.  */
      prot_conf = conf_get_list (suite->field, "Protocols");
      if (!prot_conf)
	goto bail_out;

      for (prot = TAILQ_FIRST (&prot_conf->fields), prot_no = 0;
	   prot_no < prot_conf->cnt;
	   prot_no++, prot = TAILQ_NEXT (prot, link))
	{
	  /* Make sure we have a proposal/transform vectors.  */
	  if (prop_no >= prop_cnt)
	    {
	      /* This resize algorithm is completely arbitrary.  */
	      prop_cnt = 2 * prop_cnt + 10;
	      new_proposal = realloc (proposal, prop_cnt * sizeof *proposal);
	      if (!new_proposal)
		goto bail_out;
	      proposal = new_proposal;

	      new_transforms_len = realloc (transforms_len,
					    prop_cnt * sizeof *transforms_len);
	      if (!new_transforms_len)
		goto bail_out;
	      transforms_len = new_transforms_len;

	      new_transform = realloc (transform,
				       prop_cnt * sizeof *transform);
	      if (!new_transform)
		goto bail_out;
	      transform = new_transform;

	      new_transform_cnt = realloc (transform_cnt,
					   prop_cnt * sizeof *transform_cnt);
	      if (!new_transform_cnt)
		goto bail_out;
	      transform_cnt = new_transform_cnt;

	      new_transform_len = realloc (transform_len,
					   prop_cnt * sizeof *transform_len);
	      if (!new_transform_len)
		goto bail_out;
	      transform_len = new_transform_len;
	    }

	  protocol_id = conf_get_str (prot->field, "PROTOCOL_ID");
	  if (!protocol_id)
	    goto bail_out;

	  /* XXX Not too beautiful, but do we have a choice?  */
	  id_map = strcasecmp (protocol_id, "IPSEC_AH") == 0 ? ipsec_ah_cst
	    : strcasecmp (protocol_id, "IPSEC_ESP") == 0 ? ipsec_esp_cst
	    : strcasecmp (protocol_id, "IPCOMP") == 0 ? ipsec_ipcomp_cst : 0;
	  if (!id_map)
	    goto bail_out;

	  /* Now get each transform we offer for this protocol.  */
	  xf_conf = conf_get_list (prot->field, "Transforms");
	  if (!xf_conf)
	    goto bail_out;
	  transform_cnt[prop_no] = xf_conf->cnt;

	  transform[prop_no] = calloc (transform_cnt[prop_no],
				       sizeof **transform);
	  if (!transform[prop_no])
	    goto bail_out;
	  transform_len[prop_no]
	    = calloc (transform_cnt[prop_no], sizeof **transform_len);
	  if (!transform_len[prop_no])
	    goto bail_out;

	  transforms_len[prop_no] = 0;
	  for (xf = TAILQ_FIRST (&xf_conf->fields), xf_no = 0;
	       xf_no < transform_cnt[prop_no];
	       xf_no++, xf = TAILQ_NEXT (xf, link))
	    {

	      /* XXX The sizing needs to be dynamic.  */
	      transform[prop_no][xf_no] = calloc (ISAKMP_TRANSFORM_SA_ATTRS_OFF
						  + 9 * ISAKMP_ATTR_VALUE_OFF,
						  1);
	      if (!transform[prop_no][xf_no])
		goto bail_out;
	      SET_ISAKMP_TRANSFORM_NO (transform[prop_no][xf_no], xf_no + 1);

	      transform_id = conf_get_str (xf->field, "TRANSFORM_ID");
	      if (!transform_id)
		goto bail_out;
	      SET_ISAKMP_TRANSFORM_ID (transform[prop_no][xf_no],
				       constant_value (id_map, transform_id));
	      SET_ISAKMP_TRANSFORM_RESERVED (transform[prop_no][xf_no], 0);

	      attr = transform[prop_no][xf_no] + ISAKMP_TRANSFORM_SA_ATTRS_OFF;

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
					      ipsec_duration_cst,
					      IPSEC_ATTR_SA_LIFE_TYPE, &attr);

		      /* XXX Does only handle 16-bit entities!  */
		      value = conf_get_num (life->field, "LIFE_DURATION", 0);
		      if (value)
			attr
			  = attribute_set_basic (attr,
						 IPSEC_ATTR_SA_LIFE_DURATION,
						 value);
		    }
		}

	      attribute_set_constant (xf->field, "ENCAPSULATION_MODE",
				      ipsec_encap_cst,
				      IPSEC_ATTR_ENCAPSULATION_MODE, &attr);

	      attribute_set_constant (xf->field, "AUTHENTICATION_ALGORITHM",
				      ipsec_auth_cst,
				      IPSEC_ATTR_AUTHENTICATION_ALGORITHM,
				      &attr);

	      attribute_set_constant (xf->field, "GROUP_DESCRIPTION",
				      ike_group_desc_cst,
				      IPSEC_ATTR_GROUP_DESCRIPTION, &attr);

	      value = conf_get_num (xf->field, "KEY_LENGTH", 0);
	      if (value)
		attr = attribute_set_basic (attr, IPSEC_ATTR_KEY_LENGTH,
					    value);

	      value = conf_get_num (xf->field, "KEY_ROUNDS", 0);
	      if (value)
		attr = attribute_set_basic (attr, IPSEC_ATTR_KEY_ROUNDS,
					    value);

	      value = conf_get_num (xf->field, "COMPRESS_DICTIONARY_SIZE", 0);
	      if (value)
		attr
		  = attribute_set_basic (attr,
					 IPSEC_ATTR_COMPRESS_DICTIONARY_SIZE,
					 value);

	      value
		= conf_get_num (xf->field, "COMPRESS_PRIVATE_ALGORITHM", 0);
	      if (value)
		attr
		  = attribute_set_basic (attr,
					 IPSEC_ATTR_COMPRESS_PRIVATE_ALGORITHM,
					 value);

	      /* Record the real transform size.  */
	      transforms_len[prop_no] += (transform_len[prop_no][xf_no]
					  = attr - transform[prop_no][xf_no]);
	    }

	  /*
	   * Get SPI from application.
	   * XXX Should we care about unknown constants?
	   */
	  protocol_num = constant_value (ipsec_proto_cst, protocol_id);
	  spi = doi->get_spi (&spi_sz, protocol_num, msg);
	  if (spi_sz && !spi)
	    goto bail_out;

	  proposal_len = ISAKMP_PROP_SPI_OFF + spi_sz;
	  proposals_len += proposal_len + transforms_len[prop_no];
	  proposal[prop_no] = malloc (proposal_len);
	  if (!proposal[prop_no])
	    goto bail_out;
	  SET_ISAKMP_PROP_NO (proposal[prop_no], suite_no + 1);
	  SET_ISAKMP_PROP_PROTO (proposal[prop_no], protocol_num);

	  /* XXX I would like to see this factored out.  */
	  proto = calloc (1, sizeof *proto);
	  if (!proto)
	    goto bail_out;
	  if (doi->proto_size)
	    {
	      proto->data = calloc (1, doi->proto_size);
	      if (!proto->data)
		goto bail_out;
	    }
	  proto->no = suite_no + 1;
	  proto->proto = protocol_num;
	  proto->sa = TAILQ_FIRST (&exchange->sa_list);
	  TAILQ_INSERT_TAIL (&TAILQ_FIRST (&exchange->sa_list)->protos, proto,
			     link);

	  /* Setup the incoming SPI.  */
	  SET_ISAKMP_PROP_SPI_SZ (proposal[prop_no], spi_sz);
	  memcpy (proposal[prop_no] + ISAKMP_PROP_SPI_OFF, spi, spi_sz);
	  proto->spi_sz[1] = spi_sz;
	  proto->spi[1] = spi;

	  /* Let the DOI get at proto for initializing its own data. */
	  if (doi->proto_init)
	    doi->proto_init (proto, prot->field);

	  SET_ISAKMP_PROP_NTRANSFORMS (proposal[prop_no],
				       transform_cnt[prop_no]);
	  prop_no++;
	}
    }

  sa_len = ISAKMP_SA_SIT_OFF + IPSEC_SIT_SIT_LEN;
  sa_buf = malloc (sa_len);
  if (!sa_buf)
    goto bail_out;
  SET_ISAKMP_SA_DOI (sa_buf, IPSEC_DOI_IPSEC);
  SET_IPSEC_SIT_SIT (sa_buf + ISAKMP_SA_SIT_OFF, IPSEC_SIT_IDENTITY_ONLY);

  /*
   * Add the payloads.  As this is a SA, we need to recompute the
   * lengths of the payloads containing others.  We also need to
   * reset these payload's "next payload type" field.
   */
  if (message_add_payload (msg, ISAKMP_PAYLOAD_SA, sa_buf, sa_len, 1))
    goto bail_out;
  SET_ISAKMP_GEN_LENGTH (sa_buf, sa_len + proposals_len);
  sa_buf = 0;

  update_nextp = 0;
  saved_nextp_sa = msg->nextp;
  for (i = 0; i < prop_no; i++)
    {
      if (message_add_payload (msg, ISAKMP_PAYLOAD_PROPOSAL, proposal[i],
			       proposal_len, update_nextp))
	goto bail_out;
      SET_ISAKMP_GEN_LENGTH (proposal[i], proposal_len + transforms_len[i]);
      proposal[i] = 0;

      update_nextp = 0;
      saved_nextp_prop = msg->nextp;
      for (xf_no = 0; xf_no < transform_cnt[i]; xf_no++)
	{
	  if (message_add_payload (msg, ISAKMP_PAYLOAD_TRANSFORM,
				   transform[i][xf_no],
				   transform_len[i][xf_no], update_nextp))
	    goto bail_out;
	  update_nextp = 1;
	  transform[i][xf_no] = 0;
	}
      msg->nextp = saved_nextp_prop;
      update_nextp = 1;
    }
  msg->nextp = saved_nextp_sa;

  /*
   * Save SA payload body in ie->sa_i_b, length ie->sa_i_b_len.
   */
  ie->sa_i_b = message_copy (msg, ISAKMP_GEN_SZ, &ie->sa_i_b_len);
  if (!ie->sa_i_b)
    goto bail_out;

  /*
   * Generate a nonce, and add it to the message.
   * XXX I want a better way to specify the nonce's size.
   */
  if (exchange_gen_nonce (msg, 16))
    {
      /* XXX Log?  */
      return -1;
    }

  /* Generate optional KEY_EXCH payload.  */
  if (ie->group)
    {
      ie->g_x_len = dh_getlen (ie->group);

      if (ipsec_gen_g_x (msg))
	{
	  /* XXX Log?  */
	  return -1;
	}
    }

  /* Generate optional client ID payloads.  XXX Share with responder.  */
  local_id = conf_get_str (exchange->name, "Local-ID");
  remote_id = conf_get_str (exchange->name, "Remote-ID");
  if (local_id && remote_id)
    {
      id = ipsec_build_id (local_id, &sz);
      if (!id)
	return -1;
      log_debug_buf (LOG_MISC, 90, "initiator_send_HASH_SA_NONCE: IDic", id,
		     sz);
      if (message_add_payload (msg, ISAKMP_PAYLOAD_ID, id, sz, 1))
	{
	  /* XXX Log?  */
	  return -1;
	}

      id = ipsec_build_id (remote_id, &sz);
      if (!id)
	return -1;
      log_debug_buf (LOG_MISC, 90, "initiator_send_HASH_SA_NONCE: IDrc", id,
		     sz);
      if (message_add_payload (msg, ISAKMP_PAYLOAD_ID, id, sz, 1))
	{
	  /* XXX Log?  */
	  return -1;
	}
    }
  /* XXX I do not judge these as errors, are they?  */
  else if (local_id)
    log_print ("initiator_send_HASH_SA_NONCE: "
	       "Local-ID given without Remote-ID for \"%s\"",
	       exchange->name);
  else if (remote_id)
    log_print ("initiator_send_HASH_SA_NONCE: "
	       "Remote-ID given without Local-ID for \"%s\"",
	       exchange->name);

  /* Allocate the prf and start calculating our HASH(1).  XXX Share?  */
  log_debug_buf (LOG_MISC, 90, "initiator_send_HASH_SA_NONCE: SKEYID_a",
		 isa->skeyid_a, isa->skeyid_len);
  prf = prf_alloc (isa->prf_type, hash->type, isa->skeyid_a, isa->skeyid_len);
  if (!prf)
    {
      /* XXX Log?  */
      return -1;
    }
  prf->Init (prf->prfctx);
  log_debug_buf (LOG_MISC, 90, "initiator_send_HASH_SA_NONCE: message_id",
		 exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  prf->Update (prf->prfctx, exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);

  /* Loop over all payloads after HASH(1).  */
  for (i = 2; i < msg->iovlen; i++)
    {
      /* XXX Misleading payload type printouts.  */
      snprintf (header, 80,
		"initiator_send_HASH_SA_NONCE: payload %d after HASH(1)",
		i - 1);
      log_debug_buf (LOG_MISC, 90, header, msg->iov[i].iov_base,
		     msg->iov[i].iov_len);
      prf->Update (prf->prfctx, msg->iov[i].iov_base, msg->iov[i].iov_len);
    }
  prf->Final (buf + ISAKMP_HASH_DATA_OFF, prf->prfctx);
  prf_free (prf);
  log_debug_buf (LOG_MISC, 80, "initiator_send_HASH_SA_NONCE: HASH(1)",
		 buf + ISAKMP_HASH_DATA_OFF, hashsize);

  return 0;

 bail_out:
  if (sa_buf)
    free (sa_buf);
  if (proposal)
    {
      for (i = 0; i < prop_no; i++)
	{
	  if (proposal[i])
	    free (proposal[i]);
	  if (transform[i])
	    {
	      for (xf_no = 0; xf_no < xf_conf->cnt; xf_no++)
		if (transform[i][xf_no])
		  free (transform[i][xf_no]);
	      free (transform[i]);
	    }
	  if (transform_len[i])
	    free (transform_len[i]);
	}
      free (proposal);
      free (transforms_len);
      free (transform);
      free (transform_len);
      free (transform_cnt);
    }
  if (xf_conf)
    conf_free_list (xf_conf);
  if (prot_conf)
    conf_free_list (prot_conf);
  if (suite_conf)
    conf_free_list (suite_conf);
  return -1;
}

/* Figure out what transform the responder chose.  */
static int
initiator_recv_HASH_SA_NONCE (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct sa *sa;
  struct proto *proto, *next_proto;
  struct payload *sa_p = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_SA]);
  struct payload *xf, *idp;
  struct payload *hashp = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_HASH]);
  struct payload *kep = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_KEY_EXCH]);
  struct prf *prf;
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  struct hash *hash = hash_get (isa->hash);
  size_t hashsize = hash->hashsize;
  u_int8_t *rest;
  size_t rest_len;

  /*
   * As we are getting an answer on our transform offer, only one transform
   * should be given.
   *
   * XXX Currently we only support negotiating one SA per quick mode run.
   */
  if (TAILQ_NEXT (sa_p, link))
    {
      log_print ("initiator_recv_HASH_SA_NONCE: "
		 "multiple SA payloads in quick mode");
      /* XXX Is there a better notification type?  */
      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 0, 0);
      return -1;
    }

  sa = TAILQ_FIRST (&msg->exchange->sa_list);

  /* Build the protection suite in our SA.  */
  for (xf = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_TRANSFORM]); xf;
       xf = TAILQ_NEXT (xf, link))
    {
      /*
       * XXX We could check that the proposal each transform belongs to
       * is unique.
       */

      if (sa_add_transform (sa, xf, exchange->initiator, &proto))
	{
	  /* XXX Log?  */
	  return -1;
	}

      /* XXX Check that the chosen transform matches an offer.  */

      ipsec_decode_transform (msg, sa, proto, xf->p);
    }

  /* Now remove offers that we don't need anymore.  */
  for (proto = TAILQ_FIRST (&sa->protos); proto; proto = next_proto)
    {
      next_proto = TAILQ_NEXT (proto, link);
      if (!proto->chosen)
	proto_free (proto);
    }

  /* Mark the SA as handled.  */
  sa_p->flags |= PL_MARK;

  /* Allocate the prf and start calculating our HASH(1).  XXX Share?  */
  log_debug_buf (LOG_MISC, 90, "initiator_recv_HASH_SA_NONCE: SKEYID_a",
		 isa->skeyid_a, isa->skeyid_len);
  prf = prf_alloc (isa->prf_type, hash->type, isa->skeyid_a, isa->skeyid_len);
  if (!prf)
    {
      /* XXX Log?  */
      return -1;
    }
  prf->Init (prf->prfctx);
  log_debug_buf (LOG_MISC, 90, "initiator_recv_HASH_SA_NONCE: message_id",
		 exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  prf->Update (prf->prfctx, exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  log_debug_buf (LOG_MISC, 90, "initiator_recv_HASH_SA_NONCE: NONCE_I_b",
		 exchange->nonce_i, exchange->nonce_i_len);
  prf->Update (prf->prfctx, exchange->nonce_i, exchange->nonce_i_len);
  rest = hashp->p + GET_ISAKMP_GEN_LENGTH (hashp->p);
  rest_len = (GET_ISAKMP_HDR_LENGTH (msg->iov[0].iov_base)
	      - (rest - (u_int8_t*)msg->iov[0].iov_base));
  log_debug_buf (LOG_MISC, 90,
		 "initiator_recv_HASH_SA_NONCE: payloads after HASH(2)", rest,
		 rest_len);
  prf->Update (prf->prfctx, rest, rest_len);
  prf->Final (hash->digest, prf->prfctx);
  prf_free (prf);
  log_debug_buf (LOG_MISC, 80,
		 "initiator_recv_HASH_SA_NONCE: computed HASH(2)",
		 hash->digest, hashsize);
  if (memcmp (hashp->p + ISAKMP_HASH_DATA_OFF, hash->digest, hashsize) != 0)
    {
      /* XXX Log & notify?  */
      return -1;
    }
  /* Mark the HASH as handled.  */
  hashp->flags |= PL_MARK;

  /* XXX Errors possible?  */
  ie->group = group_get (isa->group_desc);

  /* Copy out the initiator's nonce.  */
  if (exchange_save_nonce (msg))
    {
      /* XXX How to log and notify peer?  */
      return -1;
    }

  /* Handle the optional KEY_EXCH payload.  */
  if (kep && ipsec_save_g_x (msg))
    {
      /* XXX How to log and notify peer?  */
      return -1;
    }

  /* Handle optional client ID payloads.  */
  idp = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_ID]);
  if (idp)
    {
      /* XXX We should really compare, not override.  */
      ie->id_ci_sz = GET_ISAKMP_GEN_LENGTH (idp->p);
      ie->id_ci = malloc (ie->id_ci_sz);
      if (!ie->id_ci)
	return -1;
      memcpy (ie->id_ci, idp->p, ie->id_ci_sz);
      idp->flags |= PL_MARK;
      log_debug_buf (LOG_MISC, 90,
		     "initiator_recv_HASH_SA_NONCE: IDci",
		     ie->id_ci + ISAKMP_GEN_SZ, ie->id_ci_sz - ISAKMP_GEN_SZ);

      idp = TAILQ_NEXT (idp, link);
      /* XXX Is IDci without IDcr valid?  */
      if (idp)
	{
	  ie->id_cr_sz = GET_ISAKMP_GEN_LENGTH (idp->p);
	  ie->id_cr = malloc (ie->id_cr_sz);
	  if (!ie->id_cr)
	    return -1;
	  memcpy (ie->id_cr, idp->p, ie->id_cr_sz);
	  idp->flags |= PL_MARK;
	  log_debug_buf (LOG_MISC, 90,
			 "initiator_recv_HASH_SA_NONCE: IDcr",
			 ie->id_cr + ISAKMP_GEN_SZ,
			 ie->id_cr_sz - ISAKMP_GEN_SZ);
	}
    }

  return 0;
}

static int
initiator_send_HASH (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  struct prf *prf;
  u_int8_t *buf;
  struct hash *hash = hash_get (isa->hash);
  size_t hashsize = hash->hashsize;

  /* We want a HASH payload to start with.  XXX Share with ike_main_mode.c?  */
  buf = malloc (ISAKMP_HASH_SZ + hashsize);
  if (!buf)
    {
      /* XXX Log?  */
      return -1;
    }
  if (message_add_payload (msg, ISAKMP_PAYLOAD_HASH, buf,
			   ISAKMP_HASH_SZ + hashsize, 1))
    {
      /* XXX Log?  */
      free (buf);
      return -1;
    }

  /* Allocate the prf and start calculating our HASH(3).  XXX Share?  */
  log_debug_buf (LOG_MISC, 90, "initiator_send_HASH: SKEYID_a", isa->skeyid_a,
		 isa->skeyid_len);
  prf = prf_alloc (isa->prf_type, isa->hash, isa->skeyid_a, isa->skeyid_len);
  if (!prf)
    {
      /* XXX Log?  */
      return -1;
    }
  prf->Init (prf->prfctx);
  prf->Update (prf->prfctx, "\0", 1);
  log_debug_buf (LOG_MISC, 90, "initiator_send_HASH: message_id",
		 exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  prf->Update (prf->prfctx, exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  log_debug_buf (LOG_MISC, 90, "initiator_send_HASH: NONCE_I_b",
		 exchange->nonce_i, exchange->nonce_i_len);
  prf->Update (prf->prfctx, exchange->nonce_i, exchange->nonce_i_len);
  log_debug_buf (LOG_MISC, 90, "initiator_send_HASH: NONCE_R_b",
		 exchange->nonce_r, exchange->nonce_r_len);
  prf->Update (prf->prfctx, exchange->nonce_r, exchange->nonce_r_len);
  prf->Final (buf + ISAKMP_GEN_SZ, prf->prfctx);
  prf_free (prf);
  log_debug_buf (LOG_MISC, 90, "initiator_send_HASH: HASH(3)",
		 buf + ISAKMP_GEN_SZ, hashsize);

  if (ie->group)
    message_register_post_send (msg, gen_g_xy);
  message_register_post_send (msg, post_quick_mode);

  return 0;
}

static void
post_quick_mode (struct message *msg)
{
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct prf *prf;
  struct sa *sa;
  struct proto *proto;
  struct ipsec_proto *iproto = proto->data;
  u_int8_t *keymat;
  int i;

  /*
   * Loop over all SA negotiations and do both an in- and an outgoing SA
   * per protocol.
   */
  for (sa = TAILQ_FIRST (&exchange->sa_list); sa; sa = TAILQ_NEXT (sa, next))
    {
      for (proto = TAILQ_FIRST (&sa->protos); proto;
	   proto = TAILQ_NEXT (proto, link))
	{
	  iproto = proto->data;

	  /*
	   * There are two SAs for each SA negotiation, incoming and outcoing.
	   */
	  for (i = 0; i < 2; i++)
	    {
	      prf = prf_alloc (isa->prf_type, isa->hash, isa->skeyid_d,
			       isa->skeyid_len);
	      if (!prf)
		{
		  /* XXX What to do?  */
		  continue;
		}

	      ie->keymat_len = ipsec_keymat_length (proto);

	      /*
	       * We need to roundup the length of the key material buffer
	       * to a multiple of the PRF's blocksize as it is generated
	       * in chunks of that blocksize.
	       */
	      iproto->keymat[i]
		= malloc (((ie->keymat_len + prf->blocksize - 1)
			   / prf->blocksize) * prf->blocksize);
	      if (!iproto->keymat[i])
		{
		  /* XXX What to do?  */
		  free (prf);
		  continue;
		}

	      for (keymat = iproto->keymat[i];
		   keymat < iproto->keymat[i] + ie->keymat_len;
		   keymat += prf->blocksize)
		{
		  prf->Init (prf->prfctx);

		  if (keymat != iproto->keymat[i])
		    {
		      /* Hash in last round's KEYMAT.  */
		      log_debug_buf (LOG_MISC, 90,
				     "post_quick_mode: last KEYMAT",
				     keymat - prf->blocksize, prf->blocksize);
		      prf->Update (prf->prfctx, keymat - prf->blocksize,
				   prf->blocksize);
		    }

		  /* If PFS is used hash in g^xy.  */
		  if (ie->g_xy)
		    {
		      log_debug_buf (LOG_MISC, 90, "post_quick_mode: g^xy",
				     ie->g_xy, ie->g_x_len);
		      prf->Update (prf->prfctx, ie->g_xy, ie->g_x_len);
		    }
		  log_debug (LOG_MISC, 90,
			     "post_quick_mode: suite %d proto %d", proto->no,
			     proto->proto);
		  prf->Update (prf->prfctx, &proto->proto, 1);
		  log_debug_buf (LOG_MISC, 90, "post_quick_mode: SPI",
				 proto->spi[i], proto->spi_sz[i]);
		  prf->Update (prf->prfctx, proto->spi[i], proto->spi_sz[i]);
		  log_debug_buf (LOG_MISC, 90, "post_quick_mode: Ni_b",
				 exchange->nonce_i, exchange->nonce_i_len);
		  prf->Update (prf->prfctx, exchange->nonce_i,
			       exchange->nonce_i_len);
		  log_debug_buf (LOG_MISC, 90, "post_quick_mode: Nr_b",
				 exchange->nonce_r, exchange->nonce_r_len);
		  prf->Update (prf->prfctx, exchange->nonce_r,
			       exchange->nonce_r_len);
		  prf->Final (keymat, prf->prfctx);
		}
	      prf_free (prf);
	      log_debug_buf (LOG_MISC, 90, "post_quick_mode: KEYMAT",
			     iproto->keymat[i], ie->keymat_len);
	    }
	}
    }
}

/*
 * Accept a set of transforms offered by the initiator and chose one we can
 * handle.
 * XXX Describe in more detail.
 */
static int
responder_recv_HASH_SA_NONCE (struct message *msg)
{
  struct payload *hashp, *kep, *idp;
  struct sa *sa;
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct prf *prf;
  u_int8_t *hash, *my_hash;
  size_t hash_len;
  u_int8_t *pkt = msg->iov[0].iov_base;
  u_int8_t group_desc = 0;
  int retval = -1;
  struct proto *proto;

  hashp = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_HASH]);
  hash = hashp->p;
  hashp->flags |= PL_MARK;

  /* The HASH payload should be the first one.  */
  if (hash != pkt + ISAKMP_HDR_SZ)
    {
      /* XXX Is there a better notification type?  */
      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 0, 0);
      return -1;
    }
  hash_len = GET_ISAKMP_GEN_LENGTH (hash);
  my_hash = malloc (hash_len - ISAKMP_GEN_SZ);
  if (!my_hash)
    {
      /* XXX Log?  */
      return -1;
    }

  /* Check the payload's integrity.  */
  log_debug_buf (LOG_MISC, 90, "responder_recv_HASH_SA_NONCE: SKEYID_a",
		 isa->skeyid_a, isa->skeyid_len);
  prf = prf_alloc (isa->prf_type, isa->hash, isa->skeyid_a, isa->skeyid_len);
  if (!prf)
    {
      /* XXX Log?  */
      return -1;
    }
  prf->Init (prf->prfctx);
  log_debug_buf (LOG_MISC, 90, "responder_recv_HASH_SA_NONCE: message_id",
		 exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  prf->Update (prf->prfctx, exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  log_debug_buf (LOG_MISC, 90,
		 "responder_recv_HASH_SA_NONCE: message after HASH",
		 hash + hash_len,
		 msg->iov[0].iov_len - ISAKMP_HDR_SZ - hash_len);
  prf->Update (prf->prfctx, hash + hash_len,
	       msg->iov[0].iov_len - ISAKMP_HDR_SZ - hash_len);
  prf->Final (my_hash, prf->prfctx);
  prf_free (prf);
  log_debug_buf (LOG_MISC, 90,
		 "responder_recv_HASH_SA_NONCE: computed HASH(1)", my_hash,
		 hash_len - ISAKMP_GEN_SZ);
  if (memcmp (hash + ISAKMP_GEN_SZ, my_hash, hash_len - ISAKMP_GEN_SZ) != 0)
    {
      /* XXX Is there a better notification type?  */
      message_drop (msg, ISAKMP_NOTIFY_INVALID_HASH_INFORMATION, 0, 0, 0);
      return -1;
    }

  if (message_negotiate_sa (msg, 0))
    return -1;

  kep = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_KEY_EXCH]);
  
  for (sa = TAILQ_FIRST (&exchange->sa_list); sa;
       sa = TAILQ_NEXT (sa, next))
    {
      for (proto = TAILQ_FIRST (&sa->protos); proto;
	   proto = TAILQ_NEXT (proto, link))
	/* XXX we need to have some attributes per proto, not all per SA.  */
	ipsec_decode_transform (msg, sa, proto, proto->chosen->p);


      isa = sa->data;

      /* Check the SA for reasonableness.  */

      /* The group description is mandatory if we got a KEY_EXCH payload.  */
      if (kep)
	{
	  if (!isa->group_desc)
	    {
	      message_drop (msg, ISAKMP_NOTIFY_NO_PROPOSAL_CHOSEN, 0, 0, 0);
	      continue;
	    }

	  /* Also, all SAs must have equal groups.  */
	  if (!group_desc)
	    group_desc = isa->group_desc;
	  else if (group_desc != isa->group_desc)
	    {
	      message_drop (msg, ISAKMP_NOTIFY_NO_PROPOSAL_CHOSEN, 0, 0, 0);
	      continue;
	    }
	}

      /* At least one SA was accepted.  */
      retval = 0;
    }

  /* XXX Errors possible?  */
  ie->group = group_get (group_desc);

  /* Copy out the initiator's nonce.  */
  if (exchange_save_nonce (msg))
    {
      /* XXX How to log and notify peer?  */
      return -1;
    }

  /* Handle the optional KEY_EXCH payload.  */
  if (kep && ipsec_save_g_x (msg))
    {
      /* XXX How to log and notify peer?  */
      return -1;
    }

  /* Handle optional client ID payloads.  */
  idp = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_ID]);
  if (idp)
    {
      ie->id_ci_sz = GET_ISAKMP_GEN_LENGTH (idp->p);
      ie->id_ci = malloc (ie->id_ci_sz);
      if (!ie->id_ci)
	retval = 1;
      memcpy (ie->id_ci, idp->p, ie->id_ci_sz);
      idp->flags |= PL_MARK;
      log_debug_buf (LOG_MISC, 90,
		     "responder_recv_HASH_SA_NONCE: IDci",
		     ie->id_ci + ISAKMP_GEN_SZ, ie->id_ci_sz - ISAKMP_GEN_SZ);

      idp = TAILQ_NEXT (idp, link);
      /* XXX Is IDci without IDcr valid?  */
      if (idp)
	{
	  ie->id_cr_sz = GET_ISAKMP_GEN_LENGTH (idp->p);
	  ie->id_cr = malloc (ie->id_cr_sz);
	  if (!ie->id_cr)
	    retval = 1;
	  memcpy (ie->id_cr, idp->p, ie->id_cr_sz);
	  idp->flags |= PL_MARK;
	  log_debug_buf (LOG_MISC, 90,
			 "responder_recv_HASH_SA_NONCE: IDcr",
			 ie->id_cr + ISAKMP_GEN_SZ,
			 ie->id_cr_sz - ISAKMP_GEN_SZ);
	}
    }

  return retval;
}

/* Reply with the transform we chose.  */
static int
responder_send_HASH_SA_NONCE (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  struct prf *prf;
  struct hash *hash = hash_get (isa->hash);
  size_t hashsize = hash->hashsize;
  size_t nonce_sz = exchange->nonce_i_len;
  u_int8_t *buf;
  int initiator = exchange->initiator;
  char header[80];
  int i;
  u_int8_t *id;
  size_t sz;

  /* We want a HASH payload to start with.  XXX Share with ike_main_mode.c?  */
  buf = malloc (ISAKMP_HASH_SZ + hashsize);
  if (!buf)
    {
      /* XXX Log?  */
      return -1;
    }
  if (message_add_payload (msg, ISAKMP_PAYLOAD_HASH, buf,
			   ISAKMP_HASH_SZ + hashsize, 1))
    {
      /* XXX Log?  */
      free (buf);
      return -1;
    }
    
  /* Add the SA payload(s) with the transform(s) that was/were chosen.  */
  if (message_add_sa_payload (msg))
    {
      /* XXX Log?  */
      return -1;
    }

  /* Generate a nonce, and add it to the message.  */
  if (exchange_gen_nonce (msg, nonce_sz))
    {
      /* XXX Log?  */
      return -1;
    }

  /* Generate optional KEY_EXCH payload.  This is known as PFS.  */
  if (ie->group && ipsec_gen_g_x (msg))
    {
      /* XXX Log?  */
      return -1;
    }

  /* If the initiator client ID's were acceptable, just mirror them back.  */
  if (ie->id_ci)
    {
      sz = ie->id_ci_sz;
      id = malloc (sz);
      if (!id)
	{
	  /* XXX Log?  */
	  return -1;
	}
      memcpy (id, ie->id_ci, sz);
      log_debug_buf (LOG_MISC, 90, "responder_send_HASH_SA_NONCE: IDic", id,
		     sz);
      if (message_add_payload (msg, ISAKMP_PAYLOAD_ID, id, sz, 1))
	{
	  /* XXX Log?  */
	  return -1;
	}

      sz = ie->id_cr_sz;
      id = malloc (sz);
      if (!id)
	{
	  /* XXX Log?  */
	  return -1;
	}
      memcpy (id, ie->id_cr, sz);
      log_debug_buf (LOG_MISC, 90, "responder_send_HASH_SA_NONCE: IDrc", id,
		     sz);
      if (message_add_payload (msg, ISAKMP_PAYLOAD_ID, id, sz, 1))
	{
	  /* XXX Log?  */
	  return -1;
	}
    }

  /* Allocate the prf and start calculating our HASH(2).  XXX Share?  */
  log_debug (LOG_MISC, 95, "responder_recv_HASH: isakmp_sa %p isa %p",
	     isakmp_sa, isa);
  log_debug_buf (LOG_MISC, 90, "responder_send_HASH_SA_NONCE: SKEYID_a",
		 isa->skeyid_a, isa->skeyid_len);
  prf = prf_alloc (isa->prf_type, hash->type, isa->skeyid_a, isa->skeyid_len);
  if (!prf)
    {
      /* XXX Log?  */
      return -1;
    }
  prf->Init (prf->prfctx);
  log_debug_buf (LOG_MISC, 90, "responder_send_HASH_SA_NONCE: message_id",
		 exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  prf->Update (prf->prfctx, exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  log_debug_buf (LOG_MISC, 90, "responder_send_HASH_SA_NONCE: NONCE_I_b",
		 exchange->nonce_i, exchange->nonce_i_len);
  prf->Update (prf->prfctx, exchange->nonce_i, exchange->nonce_i_len);

  /* Loop over all payloads after HASH(2).  */
  for (i = 2; i < msg->iovlen; i++)
    {
      /* XXX Misleading payload type printouts.  */
      snprintf (header, 80,
		"responder_send_HASH_SA_NONCE: payload %d after HASH(2)",
		i - 1);
      log_debug_buf (LOG_MISC, 90, header, msg->iov[i].iov_base,
		     msg->iov[i].iov_len);
      prf->Update (prf->prfctx, msg->iov[i].iov_base, msg->iov[i].iov_len);
    }
  prf->Final (buf + ISAKMP_HASH_DATA_OFF, prf->prfctx);
  prf_free (prf);
  snprintf (header, 80, "responder_send_HASH_SA_NONCE: HASH_%c",
	    initiator ? 'I' : 'R');
  log_debug_buf (LOG_MISC, 80, header, buf + ISAKMP_HASH_DATA_OFF, hashsize);

  if (ie->group)
    message_register_post_send (msg, gen_g_xy);

  return 0;
}

static void
gen_g_xy (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;

  /* Compute Diffie-Hellman shared value.  */
  ie->g_xy = malloc (ie->g_x_len);
  if (!ie->g_xy)
    {
      /* XXX How to log and notify peer?  */
      return;
    }
  dh_create_shared (ie->group, ie->g_xy,
		    exchange->initiator ? ie->g_xr : ie->g_xi);
  log_debug_buf (LOG_MISC, 80, "gen_g_xy: g^xy", ie->g_xy, ie->g_x_len);
}

static int
responder_recv_HASH (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  struct prf *prf;
  u_int8_t *hash, *my_hash;
  size_t hash_len;
  struct payload *hashp;

  /* Find HASH(3) and create our own hash, just as big.  */
  hashp = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_HASH]);
  hash = hashp->p;
  hashp->flags |= PL_MARK;
  hash_len = GET_ISAKMP_GEN_LENGTH (hash);
  my_hash = malloc (hash_len - ISAKMP_GEN_SZ);
  if (!my_hash)
    {
      /* XXX Log?  */
      return -1;
    }

  /* Allocate the prf and start calculating our HASH(3).  XXX Share?  */
  log_debug (LOG_MISC, 95, "responder_recv_HASH: isakmp_sa %p isa %p",
	     isakmp_sa, isa);
  log_debug_buf (LOG_MISC, 90, "responder_recv_HASH: SKEYID_a", isa->skeyid_a,
		 isa->skeyid_len);
  prf = prf_alloc (isa->prf_type, isa->hash, isa->skeyid_a, isa->skeyid_len);
  if (!prf)
    {
      /* XXX Log?  */
      free (my_hash);
      return -1;
    }
  prf->Init (prf->prfctx);
  prf->Update (prf->prfctx, "\0", 1);
  log_debug_buf (LOG_MISC, 90, "responder_recv_HASH: message_id",
		 exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  prf->Update (prf->prfctx, exchange->message_id, ISAKMP_HDR_MESSAGE_ID_LEN);
  log_debug_buf (LOG_MISC, 90, "responder_recv_HASH: NONCE_I_b",
		 exchange->nonce_i, exchange->nonce_i_len);
  prf->Update (prf->prfctx, exchange->nonce_i, exchange->nonce_i_len);
  log_debug_buf (LOG_MISC, 90, "responder_recv_HASH: NONCE_R_b",
		 exchange->nonce_r, exchange->nonce_r_len);
  prf->Update (prf->prfctx, exchange->nonce_r, exchange->nonce_r_len);
  prf->Final (my_hash, prf->prfctx);
  prf_free (prf);
  log_debug_buf (LOG_MISC, 90,
		 "responder_recv_HASH: computed HASH(3)", my_hash,
		 hash_len - ISAKMP_GEN_SZ);
  if (memcmp (hash + ISAKMP_GEN_SZ, my_hash, hash_len - ISAKMP_GEN_SZ) != 0)
    {
      /* XXX Is there a better notification type?  */
      message_drop (msg, ISAKMP_NOTIFY_INVALID_HASH_INFORMATION, 0, 0, 0);
      return -1;
    }

  post_quick_mode (msg);

  return 0;
}

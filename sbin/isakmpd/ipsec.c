/*	$OpenBSD: ipsec.c,v 1.6 1999/02/27 09:59:36 niklas Exp $	*/
/*	$EOM: ipsec.c,v 1.83 1999/02/26 14:32:18 niklas Exp $	*/

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

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
#include "ike_quick_mode.h"
#include "ipsec.h"
#include "ipsec_doi.h"
#include "isakmp.h"
#include "log.h"
#include "math_group.h"
#include "message.h"
#include "prf.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"
#include "util.h"

/* The replay window size used for all IPSec protocols if not overridden.  */
#define DEFAULT_REPLAY_WINDOW 16

struct ipsec_decode_arg {
  struct message *msg;
  struct sa *sa;
  struct proto *proto;
};

static int ipsec_debug_attribute (u_int16_t, u_int8_t *, u_int16_t, void *);
static void ipsec_delete_spi (struct sa *, struct proto *, int);
static u_int16_t *ipsec_exchange_script (u_int8_t);
static void ipsec_finalize_exchange (struct message *);
static void ipsec_free_exchange_data (void *);
static void ipsec_free_proto_data (void *);
static void ipsec_free_sa_data (void *);
static struct keystate *ipsec_get_keystate (struct message *);
static u_int8_t *ipsec_get_spi (size_t *, u_int8_t, struct message *);
static int ipsec_initiator (struct message *);
static void ipsec_proto_init (struct proto *, char *);
static int ipsec_responder (struct message *);
static void ipsec_setup_situation (u_int8_t *);
static size_t ipsec_situation_size (void);
static u_int8_t ipsec_spi_size (u_int8_t);
static int ipsec_validate_attribute (u_int16_t, u_int8_t *, u_int16_t, void *);
static int ipsec_validate_exchange (u_int8_t);
static int ipsec_validate_id_information (u_int8_t, u_int8_t *, u_int8_t *,
					  size_t, struct exchange *);
static int ipsec_validate_key_information (u_int8_t *, size_t);
static int ipsec_validate_notification (u_int16_t);
static int ipsec_validate_proto (u_int8_t);
static int ipsec_validate_situation (u_int8_t *, size_t *);
static int ipsec_validate_transform_id (u_int8_t, u_int8_t);

static struct doi ipsec_doi = {
  { 0 }, IPSEC_DOI_IPSEC,
  sizeof (struct ipsec_exch), sizeof (struct ipsec_sa),
  sizeof (struct ipsec_proto),
  ipsec_debug_attribute,
  ipsec_delete_spi,
  ipsec_exchange_script,
  ipsec_finalize_exchange,
  ipsec_free_exchange_data,
  ipsec_free_proto_data,
  ipsec_free_sa_data,
  ipsec_get_keystate,
  ipsec_get_spi,
  ipsec_is_attribute_incompatible,
  ipsec_proto_init,
  ipsec_setup_situation,
  ipsec_situation_size,
  ipsec_spi_size,
  ipsec_validate_attribute,
  ipsec_validate_exchange,
  ipsec_validate_id_information,
  ipsec_validate_key_information,
  ipsec_validate_notification,
  ipsec_validate_proto,
  ipsec_validate_situation,
  ipsec_validate_transform_id,
  ipsec_initiator,
  ipsec_responder
};

int16_t script_quick_mode[] = {
  ISAKMP_PAYLOAD_HASH,		/* Initiator -> responder.  */
  ISAKMP_PAYLOAD_SA,
  ISAKMP_PAYLOAD_NONCE,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_HASH,		/* Responder -> initiator.  */
  ISAKMP_PAYLOAD_SA,
  ISAKMP_PAYLOAD_NONCE,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_HASH,		/* Initiator -> responder.  */
  EXCHANGE_SCRIPT_END
};

int16_t script_new_group_mode[] = {
  ISAKMP_PAYLOAD_HASH,		/* Initiator -> responder.  */
  ISAKMP_PAYLOAD_SA,
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_HASH,		/* Responder -> initiator.  */
  ISAKMP_PAYLOAD_SA,
  EXCHANGE_SCRIPT_END
};

static void
ipsec_finalize_exchange (struct message *msg)
{
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct sa *sa = 0;
  struct proto *proto, *last_proto = 0;
  int initiator = exchange->initiator;
  struct timeval expiration;
  int id;

  switch (exchange->phase)
    {
    case 1:
      /* Move over the name to the SA.  */
      isakmp_sa->name = exchange->name;
      exchange->name = 0;

      switch (exchange->type)
	{
	case ISAKMP_EXCH_ID_PROT:
	case ISAKMP_EXCH_AGGRESSIVE:
	  isa->hash = ie->hash->type;
	  isa->prf_type = ie->prf_type;
	  isa->skeyid_len = ie->skeyid_len;
	  isa->skeyid_d = ie->skeyid_d;
	  isa->skeyid_a = ie->skeyid_a;
	  /* Prevents early free of SKEYID_*.  */
	  ie->skeyid_a = ie->skeyid_d = 0;
	  break;
	}

      /* If a lifetime was negotiated setup the expiration timers.  */
      if (isakmp_sa->seconds)
	{
	  gettimeofday(&expiration, 0);
	  expiration.tv_sec += isakmp_sa->seconds * 9 / 10;
	  isakmp_sa->soft_death
	    = timer_add_event ("sa_soft_expire",
			       (void (*) (void *))sa_soft_expire, isakmp_sa,
			       &expiration);
	  if (!isakmp_sa->soft_death)
	    {
	      /* If we don't give up we might start leaking... */
	      sa_delete (isakmp_sa, 1);
	      return;
	    }

	  gettimeofday(&expiration, 0);
	  expiration.tv_sec += isakmp_sa->seconds;
	  isakmp_sa->death
	    = timer_add_event ("sa_hard_expire",
			       (void (*) (void *))sa_hard_expire, isakmp_sa,
			       &expiration);
	  if (!isakmp_sa->death)
	    {
	      /* If we don't give up we might start leaking... */
	      sa_delete (isakmp_sa, 1);
	      return;
	    }
	}
      break;

    case 2:
      switch (exchange->type)
	{
	case IKE_EXCH_QUICK_MODE:
	  /*
	   * Tell the application(s) about the SPIs and key material.
	   */
	  for (sa = TAILQ_FIRST (&exchange->sa_list); sa;
	       sa = TAILQ_NEXT (sa, next))
	    {
	      /* Move over the name to the SA.  */
	      sa->name = exchange->name;

	      for (proto = TAILQ_FIRST (&sa->protos), last_proto = 0; proto;
		   proto = TAILQ_NEXT (proto, link))
		{
		  if (sysdep_ipsec_set_spi (sa, proto, 0, initiator)
		      || (last_proto
			  && sysdep_ipsec_group_spis (sa, last_proto, proto,
						      0))
		      || sysdep_ipsec_set_spi (sa, proto, 1, initiator)
		      || (last_proto
			  && sysdep_ipsec_group_spis (sa, last_proto, proto,
						      1)))
		    /* XXX Tear down this exchange.  */
		    return;
		  last_proto = proto;
		}

	      /* Figure out the networks.  */
	      isa = sa->data;
	      id = GET_ISAKMP_ID_TYPE (ie->id_ci);
	      switch (id)
		{
		case IPSEC_ID_IPV4_ADDR:
		  isa->src_net
		    = decode_32 ((exchange->initiator ? ie->id_ci : ie->id_cr)
				 + ISAKMP_ID_DATA_OFF);
		  isa->src_mask = 0xffffffff;
		  break;
		case IPSEC_ID_IPV4_ADDR_SUBNET:
		  isa->src_net
		    = decode_32 ((exchange->initiator ? ie->id_ci : ie->id_cr)
				 + ISAKMP_ID_DATA_OFF);
		  isa->src_mask
		    = decode_32 ((exchange->initiator ? ie->id_ci : ie->id_cr)
				 + ISAKMP_ID_DATA_OFF + 4);
		  break;
		}

	      id = GET_ISAKMP_ID_TYPE (ie->id_cr);
	      switch (id)
		{
		case IPSEC_ID_IPV4_ADDR:
		  isa->dst_net
		    = decode_32 ((exchange->initiator ? ie->id_cr : ie->id_ci)
				 + ISAKMP_ID_DATA_OFF);
		  isa->dst_mask = 0xffffffff;
		  break;
		case IPSEC_ID_IPV4_ADDR_SUBNET:
		  isa->dst_net
		    = decode_32 ((exchange->initiator ? ie->id_cr : ie->id_ci)
				 + ISAKMP_ID_DATA_OFF);
		  isa->dst_mask
		    = decode_32 ((exchange->initiator ? ie->id_cr : ie->id_ci)
				 + ISAKMP_ID_DATA_OFF + 4);
		  break;
		}
	      log_debug (LOG_MISC, 50,
			 "ipsec_finalize_exchange: src %x %x dst %x %x",
			 isa->src_net, isa->src_mask, isa->dst_net,
			 isa->dst_mask);

	      if (sysdep_ipsec_enable_sa (sa, initiator))
		/* XXX Tear down this exchange.  */
		return;
	    }
	  exchange->name = 0;
	  break;
	}
    }
}

static void
ipsec_free_exchange_data (void *vie)
{
  struct ipsec_exch *ie = vie;

  if (ie->sa_i_b)
    free (ie->sa_i_b);
  if (ie->id_ci)
    free (ie->id_ci);
  if (ie->id_cr)
    free (ie->id_cr);
  if (ie->g_xi)
    free (ie->g_xi);
  if (ie->g_xr)
    free (ie->g_xr);
  if (ie->g_xy)
    free (ie->g_xy);
  if (ie->skeyid)
    free (ie->skeyid);
  if (ie->skeyid_d)
    free (ie->skeyid_d);
  if (ie->skeyid_a)
    free (ie->skeyid_a);
  if (ie->skeyid_e)
    free (ie->skeyid_e);
  if (ie->hash_i)
    free (ie->hash_i);
  if (ie->hash_r)
    free (ie->hash_r);
}

static void
ipsec_free_sa_data (void *visa)
{
  struct ipsec_sa *isa = visa;

  if (isa->skeyid_a)
    free (isa->skeyid_a);
  if (isa->skeyid_d)
    free (isa->skeyid_d);
}

static void
ipsec_free_proto_data (void *viproto)
{
  struct ipsec_proto *iproto = viproto;
  int i;

  for (i = 0; i < 2; i++)
    if (iproto->keymat[i])
      free (iproto->keymat[i]);
}

static u_int16_t *
ipsec_exchange_script (u_int8_t type)
{
  switch (type)
    {
    case IKE_EXCH_QUICK_MODE:
      return script_quick_mode;
    case IKE_EXCH_NEW_GROUP_MODE:
      return script_new_group_mode;
    }
  return 0;
}

/* Requires doi_init to already have been called.  */
void
ipsec_init ()
{
  doi_register (&ipsec_doi);
}

/* Given a message MSG, return a suitable IV (or rather keystate).  */
static struct keystate *
ipsec_get_keystate (struct message *msg)
{
  struct keystate *ks;
  struct hash *hash;

  /* If we have already have an IV, use it.  */
  if (msg->exchange && msg->exchange->keystate)
    return msg->exchange->keystate;

  /*
   * For phase 2 when no SA yet is setup we need to hash the IV used by
   * the ISAKMP SA concatenated with the message ID, and use that as an
   * IV for further cryptographic operations.
   */
  ks = crypto_clone_keystate (msg->isakmp_sa->keystate);
  if (!ks)
    return 0;

  hash = hash_get (((struct ipsec_sa *)msg->isakmp_sa->data)->hash);
  hash->Init (hash->ctx);
  log_debug_buf (LOG_CRYPTO, 80, "ipsec_get_keystate: final phase 1 IV",
		 ks->riv, ks->xf->blocksize);
  hash->Update (hash->ctx, ks->riv, ks->xf->blocksize);
  log_debug_buf (LOG_CRYPTO, 80, "ipsec_get_keystate: message ID",
		 ((u_int8_t *)msg->iov[0].iov_base)
		 + ISAKMP_HDR_MESSAGE_ID_OFF,
		 ISAKMP_HDR_MESSAGE_ID_LEN);
  hash->Update (hash->ctx,
		((u_int8_t *)msg->iov[0].iov_base) + ISAKMP_HDR_MESSAGE_ID_OFF,
		ISAKMP_HDR_MESSAGE_ID_LEN);
  hash->Final (hash->digest, hash->ctx);
  crypto_init_iv (ks, hash->digest, ks->xf->blocksize);
  log_debug_buf (LOG_CRYPTO, 80, "ipsec_get_keystate: phase 2 IV",
		 hash->digest, ks->xf->blocksize);
  return ks;
}

static void
ipsec_setup_situation (u_int8_t *buf)
{
  SET_IPSEC_SIT_SIT (buf + ISAKMP_SA_SIT_OFF, IPSEC_SIT_IDENTITY_ONLY);
}

static size_t
ipsec_situation_size (void)
{
  return IPSEC_SIT_SIT_LEN;
}

static u_int8_t
ipsec_spi_size (u_int8_t proto)
{
  return IPSEC_SPI_SIZE;
}

static int
ipsec_validate_attribute (u_int16_t type, u_int8_t *value, u_int16_t len,
			  void *vmsg)
{
  struct message *msg = vmsg;

  if ((msg->exchange->phase == 1
       && (type < IKE_ATTR_ENCRYPTION_ALGORITHM
	   || type > IKE_ATTR_GROUP_ORDER))
      || (msg->exchange->phase == 2
	  && (type < IPSEC_ATTR_SA_LIFE_TYPE
	      || type > IPSEC_ATTR_COMPRESS_PRIVATE_ALGORITHM)))
    return -1;
  return 0;
}

static int
ipsec_validate_exchange (u_int8_t exch)
{
  return exch != IKE_EXCH_QUICK_MODE && exch != IKE_EXCH_NEW_GROUP_MODE;
}

static int
ipsec_validate_id_information (u_int8_t type, u_int8_t *extra, u_int8_t *buf,
			       size_t sz, struct exchange *exchange)
{
  u_int8_t proto = GET_IPSEC_ID_PROTO (extra);
  u_int16_t port = GET_IPSEC_ID_PORT (extra);

  log_debug (LOG_MESSAGE, 0, 
	     "ipsec_validate_id_information: proto %d port %d type %d",
	     proto, port, type);
  if (type < IPSEC_ID_IPV4_ADDR || type > IPSEC_ID_KEY_ID)
    return -1;

  switch (type)
    {
    case IPSEC_ID_IPV4_ADDR:
      log_debug_buf (LOG_MESSAGE, 40, "ipsec_validate_id_information: IPv4",
		     buf, 4);
      break;

    case IPSEC_ID_IPV4_ADDR_SUBNET:
      log_debug_buf (LOG_MESSAGE, 40,
		     "ipsec_validate_id_information: IPv4 network/netmask",
		     buf, 8);
      break;

    default:
      break;
    }

  if (exchange->phase == 1
      && (proto != IPPROTO_UDP || port != UDP_DEFAULT_PORT)
      && (proto != 0 || port != 0))
    {
/* XXX SSH's ISAKMP tester fails this test (proto 17 - port 0).  */
#ifdef notyet
      return -1;
#else
      log_print ("ipsec_validate_id_information: "
		 "dubious ID information accepted");
#endif
    }

  /* XXX More checks?  */

  return 0;
}

static int
ipsec_validate_key_information (u_int8_t *buf, size_t sz)
{
  /* XXX Not implemented yet.  */
  return 0;
}

static int
ipsec_validate_notification (u_int16_t type)
{
  return type < IPSEC_NOTIFY_RESPONDER_LIFETIME
    || type > IPSEC_NOTIFY_INITIAL_CONTACT ? -1 : 0;
}

static int
ipsec_validate_proto (u_int8_t proto)
{
  return proto < IPSEC_PROTO_IPSEC_AH || proto > IPSEC_PROTO_IPCOMP ? -1 : 0;
}

static int
ipsec_validate_situation (u_int8_t *buf, size_t *sz)
{
  int sit = GET_IPSEC_SIT_SIT (buf);
  int off;

  if (sit & (IPSEC_SIT_SECRECY | IPSEC_SIT_INTEGRITY))
    {
      /*
       * XXX All the roundups below, round up to 32 bit boundaries given
       * that the situation field is aligned.  This is not necessarily so,
       * but I interpret the drafts as this is like this they want it.
       */
      off = ROUNDUP_32 (GET_IPSEC_SIT_SECRECY_LENGTH (buf));
      off += ROUNDUP_32 (GET_IPSEC_SIT_SECRECY_CAT_LENGTH (buf + off));
      off += ROUNDUP_32 (GET_IPSEC_SIT_INTEGRITY_LENGTH (buf + off));
      off += ROUNDUP_32 (GET_IPSEC_SIT_INTEGRITY_CAT_LENGTH (buf + off));
      *sz = off + IPSEC_SIT_SZ;
    }
  else
    *sz = IPSEC_SIT_SIT_LEN;

  /* Currently only "identity only" situations are supported.  */
#ifdef notdef
  return
    sit & ~(IPSEC_SIT_IDENTITY_ONLY | IPSEC_SIT_SECRECY | IPSEC_SIT_INTEGRITY);
#else
   return sit & ~IPSEC_SIT_IDENTITY_ONLY;
#endif
    return 1;
  return 0;
}

static int
ipsec_validate_transform_id (u_int8_t proto, u_int8_t transform_id)
{
  switch (proto)
    {
      /*
       * As no unexpected protocols can occur, we just tie the default case
       * to the first case, in orer to silence a GCC warning.
       */
    default:
    case ISAKMP_PROTO_ISAKMP:
      return transform_id != IPSEC_TRANSFORM_KEY_IKE;
    case IPSEC_PROTO_IPSEC_AH:
      return
	transform_id < IPSEC_AH_MD5 || transform_id > IPSEC_AH_DES ? -1 : 0;
    case IPSEC_PROTO_IPSEC_ESP:
      return transform_id < IPSEC_ESP_DES_IV64
	|| transform_id > IPSEC_ESP_NULL ? -1 : 0;
    case IPSEC_PROTO_IPCOMP:
      return transform_id < IPSEC_IPCOMP_OUI
	|| transform_id > IPSEC_IPCOMP_V42BIS ? -1 : 0;
    }
}

static int
ipsec_initiator (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  int (**script) (struct message *msg) = 0;

  /* XXX Mostly not implemented yet.  */
  
  /* Check that the SA is coherent with the IKE rules.  */
  if ((exchange->phase == 1 && exchange->type != ISAKMP_EXCH_ID_PROT
       && exchange->type != ISAKMP_EXCH_INFO)
      || (exchange->phase == 2 && exchange->type != IKE_EXCH_QUICK_MODE))
    {
      log_print ("ipsec_initiator: unsupported exchange type %d in phase %d",
		 exchange->type, exchange->phase);
      return -1;
    }
    
  switch (exchange->type)
    {
    case ISAKMP_EXCH_BASE:
      break;
    case ISAKMP_EXCH_ID_PROT:
      script = ike_main_mode_initiator;
      break;
    case ISAKMP_EXCH_AUTH_ONLY:
      log_print ("ipsec_initiator: unuspported exchange type %d",
		 exchange->type);
      return -1;
    case ISAKMP_EXCH_AGGRESSIVE:
      break;
    case ISAKMP_EXCH_INFO:
      message_send_info (msg);
      break;
    case IKE_EXCH_QUICK_MODE:
      script = ike_quick_mode_initiator;
      break;
    case IKE_EXCH_NEW_GROUP_MODE:
      break;
    }

  /* Run the script code for this step.  */
  if (script)
    return script[exchange->step] (msg);

  return 0;
}

static int
ipsec_responder (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  int (**script) (struct message *msg) = 0;

  /* Check that a new exchange is coherent with the IKE rules.  */
  if (exchange->step == 0
      && ((exchange->phase == 1 && exchange->type != ISAKMP_EXCH_ID_PROT
	   && exchange->type != ISAKMP_EXCH_INFO)
	  || (exchange->phase == 2 && exchange->type == ISAKMP_EXCH_ID_PROT)))
    {
      message_drop (msg, ISAKMP_NOTIFY_UNSUPPORTED_EXCHANGE_TYPE, 0, 0, 0);
      return -1;
    }
    
  log_debug (LOG_MISC, 30,
	     "ipsec_responder: phase %d exchange %d step %d", exchange->phase,
	     exchange->type, exchange->step);
  switch (exchange->type)
    {
    case ISAKMP_EXCH_BASE:
    case ISAKMP_EXCH_AUTH_ONLY:
      message_drop (msg, ISAKMP_NOTIFY_UNSUPPORTED_EXCHANGE_TYPE, 0, 0, 0);
      return -1;

    case ISAKMP_EXCH_ID_PROT:
      script = ike_main_mode_responder;
      break;

    case ISAKMP_EXCH_AGGRESSIVE:
      /* XXX Not implemented yet.  */
      break;

    case ISAKMP_EXCH_INFO:
      /* XXX Not implemented yet.  */
      break;

    case IKE_EXCH_QUICK_MODE:
      script = ike_quick_mode_responder;
      break;

    case IKE_EXCH_NEW_GROUP_MODE:
      /* XXX Not implemented yet.  */
      break;
    }

  /* Run the script code for this step.  */
  if (script)
    return script[exchange->step] (msg);

  /*
   * XXX So far we don't accept any proposals for exchanges we don't support.
   */
  if (TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_SA]))
    {
      message_drop (msg, ISAKMP_NOTIFY_NO_PROPOSAL_CHOSEN, 0, 0, 0);
      return -1;
    }
  return 0;
}

static enum hashes from_ike_hash (u_int16_t hash)
{
  switch (hash)
    {
    case IKE_HASH_MD5:
      return HASH_MD5;
    case IKE_HASH_SHA:
      return HASH_SHA1;
    }
  return -1;
}

static enum transform from_ike_crypto (u_int16_t crypto)
{
  /* Coincidentally this is the null operation :-)  */
  return crypto;
}

int
ipsec_is_attribute_incompatible (u_int16_t type, u_int8_t *value,
				 u_int16_t len, void *vmsg)
{
  struct message *msg = vmsg;

  if (msg->exchange->phase == 1)
    {
      switch (type)
	{
	case IKE_ATTR_ENCRYPTION_ALGORITHM:
	  return !crypto_get (from_ike_crypto (decode_16 (value)));
	case IKE_ATTR_HASH_ALGORITHM:
	  return !hash_get (from_ike_hash (decode_16 (value)));
	case IKE_ATTR_AUTHENTICATION_METHOD:
	  return !ike_auth_get (decode_16 (value));
	case IKE_ATTR_GROUP_DESCRIPTION:
	  return decode_16 (value) < IKE_GROUP_DESC_MODP_768
	    || decode_16 (value) > IKE_GROUP_DESC_EC2N_185;
	case IKE_ATTR_GROUP_TYPE:
	  return 1;
	case IKE_ATTR_GROUP_PRIME:
	  return 1;
	case IKE_ATTR_GROUP_GENERATOR_1:
	  return 1;
	case IKE_ATTR_GROUP_GENERATOR_2:
	  return 1;
	case IKE_ATTR_GROUP_CURVE_A:
	  return 1;
	case IKE_ATTR_GROUP_CURVE_B:
	  return 1;
	case IKE_ATTR_LIFE_TYPE:
	  return decode_16 (value) < IKE_DURATION_SECONDS
	    || decode_16 (value) > IKE_DURATION_KILOBYTES;
	case IKE_ATTR_LIFE_DURATION:
	  return 0;
	case IKE_ATTR_PRF:
	  return 1;
	case IKE_ATTR_KEY_LENGTH:
	  /*
	   * Our crypto routines only allows key-lengths which are multiples
	   * of an octet.
	   */
	  return decode_16 (value) % 8 != 0; 
	case IKE_ATTR_FIELD_SIZE:
	  return 1;
	case IKE_ATTR_GROUP_ORDER:
	  return 1;
	}
    }
  else
    {
      switch (type)
	{
	case IPSEC_ATTR_SA_LIFE_TYPE:
	  return decode_16 (value) < IPSEC_DURATION_SECONDS
	    || decode_16 (value) > IPSEC_DURATION_KILOBYTES;
	case IPSEC_ATTR_SA_LIFE_DURATION:
	  return 0;
	case IPSEC_ATTR_GROUP_DESCRIPTION:
	  return decode_16 (value) < IKE_GROUP_DESC_MODP_768
	    || decode_16 (value) > IKE_GROUP_DESC_EC2N_185;
	case IPSEC_ATTR_ENCAPSULATION_MODE:
	  return decode_16 (value) < IPSEC_ENCAP_TUNNEL
	    || decode_16 (value) > IPSEC_ENCAP_TRANSPORT;
	case IPSEC_ATTR_AUTHENTICATION_ALGORITHM:
	  return decode_16 (value) < IPSEC_AUTH_HMAC_MD5
	    || decode_16 (value) > IPSEC_AUTH_KPDK;
	case IPSEC_ATTR_KEY_LENGTH:
	  return 1;
	case IPSEC_ATTR_KEY_ROUNDS:
	  return 1;
	case IPSEC_ATTR_COMPRESS_DICTIONARY_SIZE:
	  return 1;
	case IPSEC_ATTR_COMPRESS_PRIVATE_ALGORITHM:
	  return 1;
	}
    }
  /* XXX Silence gcc.  */
  return 1;
}

int
ipsec_debug_attribute (u_int16_t type, u_int8_t *value, u_int16_t len,
		       void *vmsg)
{
  struct message *msg = vmsg;
  char val[20];

  /* XXX Transient solution.  */
  if (len == 2)
    sprintf (val, "%d", decode_16 (value));
  else if (len == 4)
    sprintf (val, "%d", decode_32 (value));
  else
    sprintf (val, "unrepresentable");

  log_debug (LOG_MESSAGE, 50, "Attribute %s value %s",
	     constant_name (msg->exchange->phase == 1
			    ? ike_attr_cst : ipsec_attr_cst, type),
	     val);
  return 0;
}

int
ipsec_decode_attribute (u_int16_t type, u_int8_t *value, u_int16_t len,
			void *vida)
{
  struct ipsec_decode_arg *ida = vida;
  struct message *msg = ida->msg;
  struct sa *sa = ida->sa;
  struct ipsec_sa *isa = sa->data;
  struct proto *proto = ida->proto;
  struct ipsec_proto *iproto = proto->data;
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  static int lifetype = 0;

  if (exchange->phase == 1)
    {
      switch (type)
	{
	case IKE_ATTR_ENCRYPTION_ALGORITHM:
	  /* XXX Errors possible?  */
	  exchange->crypto = crypto_get (from_ike_crypto (decode_16 (value)));
	  break;
	case IKE_ATTR_HASH_ALGORITHM:
	  /* XXX Errors possible?  */
	  ie->hash = hash_get (from_ike_hash (decode_16 (value)));
	  break;
	case IKE_ATTR_AUTHENTICATION_METHOD:
	  /* XXX Errors possible?  */
	  ie->ike_auth = ike_auth_get (decode_16 (value));
	  break;
	case IKE_ATTR_GROUP_DESCRIPTION:
	  /* XXX Errors possible?  */
	  ie->group = group_get (decode_16 (value));
	  break;
	case IKE_ATTR_GROUP_TYPE:
	  break;
	case IKE_ATTR_GROUP_PRIME:
	  break;
	case IKE_ATTR_GROUP_GENERATOR_1:
	  break;
	case IKE_ATTR_GROUP_GENERATOR_2:
	  break;
	case IKE_ATTR_GROUP_CURVE_A:
	  break;
	case IKE_ATTR_GROUP_CURVE_B:
	  break;
	case IKE_ATTR_LIFE_TYPE:
	  lifetype = decode_16 (value);
	  return 0;
	case IKE_ATTR_LIFE_DURATION:
	  switch (lifetype)
	    {
	    case IKE_DURATION_SECONDS:
	      switch (len)
		{
		case 2:
		  sa->seconds = decode_16 (value);
		  break;
		case 4:
		  sa->seconds = decode_32 (value);
		  break;
		default:
		  /* XXX Log.  */
		}
	      break;
	    case IKE_DURATION_KILOBYTES:
	      switch (len)
		{
		case 2:
		  sa->kilobytes = decode_16 (value);
		  break;
		case 4:
		  sa->kilobytes = decode_32 (value);
		  break;
		default:
		  /* XXX Log.  */
		}
	      break;
	    default:
	      /* XXX Log!  */
	    }
	  break;
	case IKE_ATTR_PRF:
	  break;
	case IKE_ATTR_KEY_LENGTH:
	  exchange->key_length = decode_16 (value) / 8;
	  break;
	case IKE_ATTR_FIELD_SIZE:
	  break;
	case IKE_ATTR_GROUP_ORDER:
	  break;
	}
    }
  else
    {
      switch (type)
	{
	case IPSEC_ATTR_SA_LIFE_TYPE:
	  lifetype = decode_16 (value);
	  return 0;
	case IPSEC_ATTR_SA_LIFE_DURATION:
	  switch (lifetype)
	    {
	    case IPSEC_DURATION_SECONDS:
	      switch (len)
		{
		case 2:
		  sa->seconds = decode_16 (value);
		  break;
		case 4:
		  sa->seconds = decode_32 (value);
		  break;
		default:
		  /* XXX Log.  */
		}
	      break;
	    case IPSEC_DURATION_KILOBYTES:
	      switch (len)
		{
		case 2:
		  sa->kilobytes = decode_16 (value);
		  break;
		case 4:
		  sa->kilobytes = decode_32 (value);
		  break;
		default:
		  /* XXX Log.  */
		}
	      break;
	    default:
	      /* XXX Log!  */
	    }
	  break;
	case IPSEC_ATTR_GROUP_DESCRIPTION:
	  isa->group_desc = decode_16 (value);
	  break;
	case IPSEC_ATTR_ENCAPSULATION_MODE:
	  /* XXX Multiple protocols must have same encapsulation mode, no?  */
	  iproto->encap_mode = decode_16 (value);
	  break;
	case IPSEC_ATTR_AUTHENTICATION_ALGORITHM:
	  iproto->auth = decode_16 (value);
	  break;
	case IPSEC_ATTR_KEY_LENGTH:
	  iproto->keylen = decode_16 (value);
	  break;
	case IPSEC_ATTR_KEY_ROUNDS:
	  iproto->keyrounds = decode_16 (value);
	  break;
	case IPSEC_ATTR_COMPRESS_DICTIONARY_SIZE:
	  break;
	case IPSEC_ATTR_COMPRESS_PRIVATE_ALGORITHM:
	  break;
	}
    }
  lifetype = 0;
  return 0;
}

/*
 * Walk over the attributes of the transform payload found in BUF, and
 * fill out the fields of the SA attached to MSG.  Also mark the SA as
 * processed.
 */
void
ipsec_decode_transform (struct message *msg, struct sa *sa,
			struct proto *proto, u_int8_t *buf)
{
  struct ipsec_exch *ie = msg->exchange->data;
  struct ipsec_decode_arg ida;

  log_debug (LOG_MISC, 20, "ipsec_decode_transform: transform %d chosen",
	     GET_ISAKMP_TRANSFORM_NO (buf));

  ida.msg = msg;
  ida.sa = sa;
  ida.proto = proto;

  /* The default IKE lifetime is 8 hours.  */
  if (sa->phase == 1)
    sa->seconds = 28800;

  /* Extract the attributes and stuff them into the SA.  */
  attribute_map (buf + ISAKMP_TRANSFORM_SA_ATTRS_OFF,
		 GET_ISAKMP_GEN_LENGTH (buf) - ISAKMP_TRANSFORM_SA_ATTRS_OFF,
		 ipsec_decode_attribute, &ida);

  /*
   * If no pseudo-random function was negotiated, it's HMAC.
   * XXX As PRF_HMAC currently is zero, this is a no-op.
   */
  if (!ie->prf_type)
    ie->prf_type = PRF_HMAC;
}

static void
ipsec_delete_spi (struct sa *sa, struct proto *proto, int initiator)
{
  if (sa->phase == 1)
    return;
  /* XXX Error handling?  Is it interesting?  */
  sysdep_ipsec_delete_spi (sa, proto, initiator);
}

static int
ipsec_g_x (struct message *msg, int peer, u_int8_t *buf)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  u_int8_t **g_x;
  int initiator = exchange->initiator ^ peer;
  char header[32];

  g_x = initiator ? &ie->g_xi : &ie->g_xr;
  *g_x = malloc (ie->g_x_len);
  if (!*g_x)
    return -1;
  memcpy (*g_x, buf, ie->g_x_len);
  snprintf (header, 32, "ipsec_g_x: g^x%c", initiator ? 'i' : 'r');
  log_debug_buf (LOG_MISC, 80, header, *g_x, ie->g_x_len);
  return 0;
}

/* Generate our DH value.  */
int
ipsec_gen_g_x (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  u_int8_t *buf;

  buf = malloc (ISAKMP_KE_SZ + ie->g_x_len);
  if (!buf)
    return -1;

  if (message_add_payload (msg, ISAKMP_PAYLOAD_KEY_EXCH, buf,
			   ISAKMP_KE_SZ + ie->g_x_len, 1))
    {
      free (buf);
      return -1;
    }

  dh_create_exchange (ie->group, buf + ISAKMP_KE_DATA_OFF);
  return ipsec_g_x (msg, 0, buf + ISAKMP_KE_DATA_OFF);
}

/* Save the peer's DH value.  */
int
ipsec_save_g_x (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct payload *kep;

  kep = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_KEY_EXCH]);
  kep->flags |= PL_MARK;
  ie->g_x_len = GET_ISAKMP_GEN_LENGTH (kep->p) - ISAKMP_KE_DATA_OFF;

  /* Check that the given length matches the group's expectancy.  */
  if (ie->g_x_len != dh_getlen (ie->group))
    {
      /* XXX Is this a good notify type?  */
      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 0, 0);
      return -1;
    }

  return ipsec_g_x (msg, 1, kep->p + ISAKMP_KE_DATA_OFF);
}

/*
 * Get a SPI for PROTO and the transport MSG passed over.  Store the
 * size where SZ points.  NB!  A zero return is OK if *SZ is zero.
 */
static u_int8_t *
ipsec_get_spi (size_t *sz, u_int8_t proto, struct message *msg)
{
  struct sockaddr *dst;
  int dstlen;
  struct transport *transport = msg->transport;

  if (msg->exchange->phase == 1)
    {
      *sz = 0;
      return 0;
    }
  else
    {
      /* We are the destination in the SA we want a SPI for.  */
      transport->vtbl->get_src (transport, &dst, &dstlen);
      return sysdep_ipsec_get_spi (sz, proto, dst, dstlen);
    }
}

int
ipsec_esp_enckeylength (struct proto *proto)
{
  struct ipsec_proto *iproto = proto->data;

  /* Compute the keylength to use.  */
  switch (proto->id)
    {
    case IPSEC_ESP_DES:
    case IPSEC_ESP_DES_IV32:
    case IPSEC_ESP_DES_IV64:
      return 8;
    case IPSEC_ESP_3DES:
      return 24;
    case IPSEC_ESP_CAST:
      if (!iproto->keylen)
	return 16;
      /* Fallthrough */
    default:
      return iproto->keylen / 8;
    }
}

int
ipsec_esp_authkeylength (struct proto *proto)
{
  struct ipsec_proto *iproto = proto->data;

  switch (iproto->auth)
    {
    case IPSEC_AUTH_HMAC_MD5:
      return 16;
    case IPSEC_AUTH_HMAC_SHA:
      return 20;
    default:
      return 0;
    }
}

int
ipsec_ah_keylength (struct proto *proto)
{
  switch (proto->id)
    {
    case IPSEC_AH_MD5:
      return 16;
    case IPSEC_AH_SHA:
      return 20;
    default:
      return -1;
    }
}

int
ipsec_keymat_length (struct proto *proto)
{
  switch (proto->proto)
    {
    case IPSEC_PROTO_IPSEC_ESP:
      return ipsec_esp_enckeylength (proto) + ipsec_esp_authkeylength (proto);
    case IPSEC_PROTO_IPSEC_AH:
      return ipsec_ah_keylength (proto);
    default:
      return -1;
    }
}

/*
 * Out of a named section SECTION in the configuration file find out
 * the network address and mask as well as the ID type.  Put the info
 * in the areas pointed to by ADDR, MASK and ID respectively.  Return
 * 0 on success and -1 on failure.
 */
int
ipsec_get_id (char *section, int *id, struct in_addr *addr,
	      struct in_addr *mask)
{
  char *type, *address, *netmask;

  type = conf_get_str (section, "ID-type");
  if (!type)
    {
      log_print ("ipsec_get_id: section %s has no \"ID-type\" tag", section);
      return -1;
    }

  *id = constant_value (ipsec_id_cst, type);
  switch (*id)
    {
    case IPSEC_ID_IPV4_ADDR:
      address = conf_get_str (section, "Address");
      if (!address)
	{
	  log_print ("ipsec_get_id: section %s has no \"Address\" tag",
		     section);
	  return -1;
	}

      if (!inet_aton (address, addr))
	{
	  log_print ("ipsec_get_id: invalid address %s in section %s", section,
		     address);
	  return -1;
	}
      break;

#ifdef notyet
    case IPSEC_ID_FQDN:
      return -1;

    case IPSEC_ID_USER_FQDN:
      return -1;
#endif

    case IPSEC_ID_IPV4_ADDR_SUBNET:
      address = conf_get_str (section, "Network");
      if (!address)
	{
	  log_print ("ipsec_get_id: section %s has no \"Network\" tag",
		     section);
	  return -1;
	}

      if (!inet_aton (address, addr))
	{
	  log_print ("ipsec_get_id: invalid section %s network %s", section,
		     address);
	  return -1;
	}

      netmask = conf_get_str (section, "Netmask");
      if (!netmask)
	{
	  log_print ("ipsec_get_id: section %s has no \"Netmask\" tag",
		     section);
	  return -1;
	}

      if (!inet_aton (netmask, mask))
	{
	  log_print ("ipsec_id_build: invalid section %s network %s", section,
		     netmask);
	  return -1;
	}
      break;

#ifdef notyet
    case IPSEC_ID_IPV6_ADDR:
      return -1;

    case IPSEC_ID_IPV6_ADDR_SUBNET:
      return -1;

    case IPSEC_ID_IPV4_RANGE:
      return -1;

    case IPSEC_ID_IPV6_RANGE:
      return -1;

    case IPSEC_ID_DER_ASN1_DN:
      return -1;

    case IPSEC_ID_DER_ASN1_GN:
      return -1;

    case IPSEC_ID_KEY_ID:
      return -1;
#endif

    default:
      log_print ("ipsec_get_id: unknown ID type \"%s\" in section %s", type,
		 section);
      return -1;
    }

  return 0;
}

/*
 * Out of a named section SECTION in the configuration file build an
 * ISAKMP ID payload.  Ths payload size should be stashed in SZ.
 * The caller is responsible for freeing the payload.
 */
u_int8_t *
ipsec_build_id (char *section, size_t *sz)
{
  struct in_addr addr, mask;
  u_int8_t *p;
  int id;

  if (ipsec_get_id (section, &id, &addr, &mask))
    return 0;

  *sz = ISAKMP_ID_SZ;
  switch (id)
    {
    case IPSEC_ID_IPV4_ADDR:
      *sz += sizeof addr;
      break;
    case IPSEC_ID_IPV4_ADDR_SUBNET:
      *sz += sizeof addr + sizeof mask;
      break;
    }

  p = malloc (*sz);
  if (!p)
    {
      log_print ("ipsec_build_id: malloc(%d) failed", *sz);
      return 0;
    }

  SET_ISAKMP_ID_TYPE (p, id);
  SET_ISAKMP_ID_DOI_DATA (p, "\000\000\000");
  
  switch (id)
    {
    case IPSEC_ID_IPV4_ADDR:
      encode_32 (p + ISAKMP_ID_DATA_OFF, ntohl (addr.s_addr));
      break;
    case IPSEC_ID_IPV4_ADDR_SUBNET:
      encode_32 (p + ISAKMP_ID_DATA_OFF, ntohl (addr.s_addr));
      encode_32 (p + ISAKMP_ID_DATA_OFF + 4, ntohl (mask.s_addr));
      break;
    }

  return p;
}

struct dst_spi_proto_arg {
  in_addr_t dst;
  u_int32_t spi;
  u_int8_t proto;
};

static int
ipsec_sa_check (struct sa *sa, void *v_arg)
{
  struct dst_spi_proto_arg *arg = v_arg;
  struct proto *proto;
  struct sockaddr *dst, *src;
  int dstlen, srclen;
  int incoming;

  if (sa->phase != 2)
    return 0;

  sa->transport->vtbl->get_dst (sa->transport, &dst, &dstlen);
  if (((struct sockaddr_in *)dst)->sin_addr.s_addr == arg->dst)
    incoming = 0;
  else
    {
      sa->transport->vtbl->get_src (sa->transport, &src, &srclen);
      if (((struct sockaddr_in *)src)->sin_addr.s_addr == arg->dst)
	incoming = 1;
      else
	return 0;
    }

  for (proto = TAILQ_FIRST (&sa->protos); proto;
       proto = TAILQ_NEXT (proto, link))
    if (proto->proto == arg->proto
	&& memcmp (proto->spi[incoming], &arg->spi, sizeof arg->spi) == 0)
      return 1;
  return 0;
}

struct sa *
ipsec_sa_lookup (in_addr_t dst, u_int32_t spi, u_int8_t proto)
{
  struct dst_spi_proto_arg arg = { dst, spi, proto };

  return sa_find (ipsec_sa_check, &arg);
}

/*
 * IPSec-specific PROTO initializations.  SECTION is only set if we are the
 * initiator thus only usable there.
 * XXX I want to fix this later.
 */
void
ipsec_proto_init (struct proto *proto, char *section)
{
  struct ipsec_proto *iproto = proto->data;

  if (proto->sa->phase == 2 && section)
    iproto->replay_window
      = conf_get_num (section, "ReplayWindow", DEFAULT_REPLAY_WINDOW);
}

/*	$OpenBSD: isakmp_cfg.c,v 1.7 2001/10/26 12:03:07 ho Exp $	*/

/*
 * Copyright (c) 2001 Niklas Hallqvist.  All rights reserved.
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
 * This code was written under funding by Gatespace
 * (http://www.gatespace.com/).
 */

#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "sysdep.h"

#include "attribute.h"
#include "conf.h"
#include "exchange.h"
#include "hash.h"
#include "ipsec.h"
#include "isakmp_fld.h"
#include "isakmp_num.h"
#include "log.h"
#include "message.h"
#include "prf.h"
#include "sa.h"
#include "util.h"

/*
 * Validation script used to test messages for correct content of
 * payloads depending on the exchange type.
 */
int16_t script_transaction[] = {
  ISAKMP_PAYLOAD_ATTRIBUTE,	/* Initiator -> responder.  */
  EXCHANGE_SCRIPT_SWITCH,
  ISAKMP_PAYLOAD_ATTRIBUTE,	/* Responder -> initiator.  */
  EXCHANGE_SCRIPT_END
};

static int decode_attribute (u_int16_t, u_int8_t *, u_int16_t, void *);
static int initiator_send_ATTR (struct message *);
static int initiator_recv_ATTR (struct message *);
static int responder_recv_ATTR (struct message *);
static int responder_send_ATTR (struct message *);

int (*isakmp_cfg_initiator[]) (struct message *) = {
  initiator_send_ATTR,
  initiator_recv_ATTR
};

int (*isakmp_cfg_responder[]) (struct message *) = {
  responder_recv_ATTR,
  responder_send_ATTR
};

/* XXX A lot can be shared with responder_send_ATTR.  */
static int
initiator_send_ATTR (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  struct hash *hash = hash_get (isa->hash);
  struct prf *prf;
  size_t hashsize = hash->hashsize;
  u_int8_t *hashp = 0, *attrp;
  size_t attrlen;

  if (exchange->phase == 2)
    {
      /* We want a HASH payload to start with.  XXX Share with others?  */
      hashp = malloc (ISAKMP_HASH_SZ + hashsize);
      if (!hashp)
	{
	  log_error ("responder_send_ATTR: malloc (%d) failed",
		     ISAKMP_HASH_SZ + hashsize);
	  return -1;
	}
      if (message_add_payload (msg, ISAKMP_PAYLOAD_HASH, hashp,
			       ISAKMP_HASH_SZ + hashsize, 1))
	{
	  free (hashp);
	  return -1;
	}
    }

#ifndef to_be_removed
  attrp = 0;
  attrlen = 0;
#endif

  if (exchange->phase == 2)
    {
      prf = prf_alloc (isa->prf_type, isa->hash, isa->skeyid_a,
		       isa->skeyid_len);
      if (!prf)
	return -1;
      prf->Init (prf->prfctx);
      prf->Update (prf->prfctx, exchange->message_id,
		   ISAKMP_HDR_MESSAGE_ID_LEN);
      prf->Update (prf->prfctx, attrp, attrlen);
      prf->Final (hashp + ISAKMP_GEN_SZ, prf->prfctx);
      prf_free (prf);
    }
  return 0;
}

static int
initiator_recv_ATTR (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
#if 0
  struct payload *p = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_ATTRIBUTE]);
#endif
  struct payload *hashp = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_HASH]);

  if (exchange->phase == 2)
    {
      if (!hashp)
	{
	  /* XXX Should another NOTIFY type be used?  */
	  message_drop (msg, ISAKMP_NOTIFY_INVALID_HASH_INFORMATION, 0, 1, 0);
	  log_print ("initiator_recv_ATTR: phase 2 message missing HASH");
	  return -1;
	}

      /* XXX Verify hash!  */

      /* Mark the HASH as handled.  */
      hashp->flags |= PL_MARK;
    }

  return 0;
}

static int
responder_recv_ATTR (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct payload *attrp
    = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_ATTRIBUTE]);
  struct payload *hashp = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_HASH]);
  struct ipsec_exch *ie = exchange->data;
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  struct prf *prf;
  u_int8_t *hash, *comp_hash;
  size_t hash_len;

  if (exchange->phase == 2)
    {
      if (!hashp)
	{
	  /* XXX Should another NOTIFY type be used?  */
	  message_drop (msg, ISAKMP_NOTIFY_INVALID_HASH_INFORMATION, 0, 1, 0);
	  log_print ("responder_recv_ATTR: phase 2 message missing HASH");
	  return -1;
	}

      hash = hashp->p;
      hash_len = GET_ISAKMP_GEN_LENGTH (hash);
      comp_hash = malloc (hash_len - ISAKMP_GEN_SZ);
      if (!comp_hash)
	{
	  log_error ("responder_recv_ATTR: malloc (%d) failed",
		     hash_len - ISAKMP_GEN_SZ);
	  return -1;
	}

      /* Verify hash!  */
      prf = prf_alloc (isa->prf_type, isa->hash, isa->skeyid_a,
		       isa->skeyid_len);
      if (!prf)
	{
	  free (comp_hash);
	  return -1;
	}
      prf->Init (prf->prfctx);
      prf->Update (prf->prfctx, exchange->message_id,
		   ISAKMP_HDR_MESSAGE_ID_LEN);
      prf->Update (prf->prfctx, hash + hash_len,
		   msg->iov[0].iov_len - ISAKMP_HDR_SZ - hash_len);
      prf->Final (comp_hash, prf->prfctx);
      prf_free (prf);
      if (memcmp (hash + ISAKMP_GEN_SZ, comp_hash, hash_len - ISAKMP_GEN_SZ)
	  != 0)
	{
	  message_drop (msg, ISAKMP_NOTIFY_INVALID_HASH_INFORMATION, 0, 1, 0);
	  free (comp_hash);
	  return -1;
	}
      free (comp_hash);

      /* Mark the HASH as handled.  */
      hashp->flags |= PL_MARK;
    }

  ie->cfg_id = GET_ISAKMP_ATTRIBUTE_ID (attrp->p);

  switch (attrp->p[ISAKMP_ATTRIBUTE_TYPE_OFF])
    {
    case ISAKMP_CFG_REQUEST:
      attribute_map (attrp->p + ISAKMP_ATTRIBUTE_ATTRS_OFF,
		     GET_ISAKMP_GEN_LENGTH (attrp->p)
		     - ISAKMP_TRANSFORM_SA_ATTRS_OFF, decode_attribute, ie);
      break;

#ifdef notyet
    case ISAKMP_CFG_SET:
      break;
#endif

    default:
      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 1, 0);
      log_print ("responder_recv_ATTR: "
		 "unexpected configuration message type %d",
		 attrp->p[ISAKMP_ATTRIBUTE_TYPE_OFF]);
      return -1;
    }

  return 0;
}

/* XXX A lot can be shared with initiator_send_ATTR.  */
static int
responder_send_ATTR (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  struct ipsec_exch *ie = exchange->data;
  struct sa *isakmp_sa = msg->isakmp_sa;
  struct ipsec_sa *isa = isakmp_sa->data;
  struct hash *hash = hash_get (isa->hash);
  struct prf *prf;
  size_t hashsize = hash->hashsize;
  u_int8_t *hashp = 0, *attrp;
  size_t attrlen, off;
  struct isakmp_cfg_attr *attr;
  struct sockaddr *sa;
  u_int32_t value;
  char *id_string;

  /*
   * XXX I can only assume it is the client who was the initiator
   * in phase 1, but I have not thought it through thoroughly.
   */
  id_string = ipsec_id_string (isakmp_sa->id_i, isakmp_sa->id_i_len);
  if (!id_string)
    {
      log_print ("responder_send_ATTR: cannot parse client's ID");
      goto fail;
    }

  if (exchange->phase == 2)
    {
      /* We want a HASH payload to start with.  XXX Share with others?  */
      hashp = malloc (ISAKMP_HASH_SZ + hashsize);
      if (!hashp)
	{
	  log_error ("responder_send_ATTR: malloc (%d) failed",
		     ISAKMP_HASH_SZ + hashsize);
	  goto fail;
	}
      if (message_add_payload (msg, ISAKMP_PAYLOAD_HASH, hashp,
			       ISAKMP_HASH_SZ + hashsize, 1))
	{
	  free (hashp);
	  goto fail;
	}
    }

  /* Compute reply attribute payload length.  */
  attrlen = ISAKMP_ATTRIBUTE_SZ;
  for (attr = LIST_FIRST (&ie->attrs); attr; attr = LIST_NEXT (attr, link))
    {
      switch (attr->type)
	{
	case ISAKMP_CFG_ATTR_INTERNAL_IP4_ADDRESS:
	case ISAKMP_CFG_ATTR_INTERNAL_IP4_NETMASK:
	case ISAKMP_CFG_ATTR_INTERNAL_IP4_DHCP:
	case ISAKMP_CFG_ATTR_INTERNAL_IP4_DNS:
	case ISAKMP_CFG_ATTR_INTERNAL_IP4_NBNS:
	case ISAKMP_CFG_ATTR_INTERNAL_ADDRESS_EXPIRY:
	  attr->length = 4;
	  break;

	case ISAKMP_CFG_ATTR_INTERNAL_IP4_SUBNET:
	  attr->length = 8;
	  break;

	case ISAKMP_CFG_ATTR_APPLICATION_VERSION:
	  /* XXX So far no version identifier of isakmpd here.  */
	  attr->length = 0;
	  break;

	case ISAKMP_CFG_ATTR_SUPPORTED_ATTRIBUTES:
	  attr->length = 2 * 15;
	  break;

	case ISAKMP_CFG_ATTR_INTERNAL_IP6_ADDRESS:
	case ISAKMP_CFG_ATTR_INTERNAL_IP6_NETMASK:
	case ISAKMP_CFG_ATTR_INTERNAL_IP6_DHCP:
	case ISAKMP_CFG_ATTR_INTERNAL_IP6_DNS:
	case ISAKMP_CFG_ATTR_INTERNAL_IP6_NBNS:
	  attr->length = 16;
	  break;

	case ISAKMP_CFG_ATTR_INTERNAL_IP6_SUBNET:
	  attr->length = 17;
	  break;

	default:
	  attr->ignore++;
	  /* XXX Log!  */
	}
      attrlen += ISAKMP_ATTR_SZ + attr->length;
    }

  attrp = calloc (1, attrlen);
  if (!attrp)
    {
      log_error ("responder_send_ATTR: calloc (1, %d) failed", attrlen);
      goto fail;
    }

  if (message_add_payload (msg, ISAKMP_PAYLOAD_ATTRIBUTE, attrp, attrlen, 1))
    {
      free (attrp);
      goto fail;
    }

  SET_ISAKMP_ATTRIBUTE_TYPE (attrp, ISAKMP_CFG_REPLY);
  SET_ISAKMP_ATTRIBUTE_ID (attrp, ie->cfg_id);

  off = ISAKMP_ATTRIBUTE_SZ;
  for (attr = LIST_FIRST (&ie->attrs); attr;
       off += ISAKMP_ATTR_SZ + attr->length, attr = LIST_NEXT (attr, link))
    {
      SET_ISAKMP_ATTR_TYPE (attrp + off, attr->type);
      switch (attr->type)
	{
	case ISAKMP_CFG_ATTR_INTERNAL_IP4_ADDRESS:
	case ISAKMP_CFG_ATTR_INTERNAL_IP6_ADDRESS:
	  sa = conf_get_address (id_string, "Address");
	  if (!sa)
	    {
	      /* XXX What to do?  */
	      attr->length = 0;
	      break;
	    }
	  if ((attr->type == ISAKMP_CFG_ATTR_INTERNAL_IP4_ADDRESS
	       && sa->sa_family != AF_INET)
	      || (attr->type == ISAKMP_CFG_ATTR_INTERNAL_IP6_ADDRESS
		  && sa->sa_family != AF_INET6))
	    {
	      /* XXX What to do?  */
	      free (sa);
	      attr->length = 0;
	      break;
	    }

	  memcpy (attrp + off + ISAKMP_ATTR_VALUE_OFF, sockaddr_addrdata (sa),
		  attr->length);
	  free (sa);
	  break;

	case ISAKMP_CFG_ATTR_INTERNAL_IP4_NETMASK:
	case ISAKMP_CFG_ATTR_INTERNAL_IP6_NETMASK:
	  break;

	case ISAKMP_CFG_ATTR_INTERNAL_IP4_SUBNET:
	case ISAKMP_CFG_ATTR_INTERNAL_IP6_SUBNET:
	  break;

	case ISAKMP_CFG_ATTR_INTERNAL_IP4_DHCP:
	case ISAKMP_CFG_ATTR_INTERNAL_IP6_DHCP:
	  break;

	case ISAKMP_CFG_ATTR_INTERNAL_IP4_DNS:
	case ISAKMP_CFG_ATTR_INTERNAL_IP6_DNS:
	  sa = conf_get_address (id_string, "Nameserver");
	  if (!sa)
	    {
	      /* XXX What to do?  */
	      attr->length = 0;
	      break;
	    }
	  if ((attr->type == ISAKMP_CFG_ATTR_INTERNAL_IP4_DNS
	       && sa->sa_family != AF_INET)
	      || (attr->type == ISAKMP_CFG_ATTR_INTERNAL_IP6_DNS
		  && sa->sa_family != AF_INET6))
	    {
	      /* XXX What to do?  */
	      attr->length = 0;
	      free (sa);
	      break;
	    }

	  memcpy (attrp + off + ISAKMP_ATTR_VALUE_OFF, sockaddr_addrdata (sa),
		  attr->length);
	  free (sa);
	  break;

	case ISAKMP_CFG_ATTR_INTERNAL_IP4_NBNS:
	case ISAKMP_CFG_ATTR_INTERNAL_IP6_NBNS:
	  sa = conf_get_address (id_string, "WINS-server");
	  if (!sa)
	    {
	      /* XXX What to do?  */
	      attr->length = 0;
	      break;
	    }
	  if ((attr->type == ISAKMP_CFG_ATTR_INTERNAL_IP4_NBNS
	       && sa->sa_family != AF_INET)
	      || (attr->type == ISAKMP_CFG_ATTR_INTERNAL_IP6_NBNS
		  && sa->sa_family != AF_INET6))
	    {
	      /* XXX What to do?  */
	      attr->length = 0;
	      free (sa);
	      break;
	    }

	  memcpy (attrp + off + ISAKMP_ATTR_VALUE_OFF, sockaddr_addrdata (sa),
		  attr->length);
	  free (sa);
	  break;

	case ISAKMP_CFG_ATTR_INTERNAL_ADDRESS_EXPIRY:
	  value = conf_get_num (id_string, "Lifetime", 1200);
	  encode_32 (attrp + off + ISAKMP_ATTR_VALUE_OFF, value);
	  break;

	case ISAKMP_CFG_ATTR_APPLICATION_VERSION:
	  /* XXX So far no version identifier of isakmpd here.  */
	  break;

	case ISAKMP_CFG_ATTR_SUPPORTED_ATTRIBUTES:
	  break;

	default:
	}
      SET_ISAKMP_ATTR_LENGTH_VALUE (attrp + off, attr->length);
    }

  if (exchange->phase == 2)
    {
      prf = prf_alloc (isa->prf_type, isa->hash, isa->skeyid_a,
		       isa->skeyid_len);
      if (!prf)
	{
	  /* XXX Log?  */
	  goto fail;
	}
      prf->Init (prf->prfctx);
      prf->Update (prf->prfctx, exchange->message_id,
		   ISAKMP_HDR_MESSAGE_ID_LEN);
      prf->Update (prf->prfctx, attrp, attrlen);
      prf->Final (hashp + ISAKMP_GEN_SZ, prf->prfctx);
      prf_free (prf);
    }

  return 0;

 fail:
  if (id_string)
    free (id_string);
  return -1;
}

/*
 * Decode the attribute of type TYPE with a LEN length value pointed to by
 * VALUE.  VIE is a pointer to the IPsec exchange context holding the
 * attributes indexed by type for easy retrieval.
 */
static int
decode_attribute (u_int16_t type, u_int8_t *value, u_int16_t len, void *vie)
{
  struct ipsec_exch *ie = vie;
  struct isakmp_cfg_attr *attr;

  if (type >= ISAKMP_CFG_ATTR_PRIVATE_MIN
      && type <= ISAKMP_CFG_ATTR_PRIVATE_MAX)
    return 0;
  if (type == 0 || type >= ISAKMP_CFG_ATTR_FUTURE_MIN)
    /* XXX Log!  */
    return -1;

  attr = calloc (1, sizeof *attr);
  if (!attr)
    {
      log_error ("decode_attribute: calloc (1, %d) failed", sizeof *attr);
      return -1;
    }
  attr->type = type;
  attr->length = len;
  if (len)
    {
      attr->value = malloc (len);
      if (!attr->value)
	{
	  log_error ("decode_attribute: malloc (%d) failed", len);
	  free (attr);
	  /* Should we also deallocate all other values?  */
	  return -1;
	}
      memcpy (attr->value, value, len);
    }
  LIST_INSERT_HEAD (&ie->attrs, attr, link);
  return 0;
}

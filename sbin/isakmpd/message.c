/*	$OpenBSD: message.c,v 1.13 1999/04/02 01:09:22 niklas Exp $	*/
/*	$EOM: message.c,v 1.112 1999/04/02 00:58:19 niklas Exp $	*/

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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>

#include "sysdep.h"

#include "attribute.h"
#include "cert.h"
#include "constants.h"
#include "crypto.h"
#include "doi.h"
#include "exchange.h"
#include "field.h"
#include "isakmp.h"
#include "log.h"
#include "message.h"
#include "sa.h"
#include "timer.h"
#include "transport.h"
#include "util.h"

#ifdef __GNUC__
#define INLINE __inline
#else
#define INLINE
#endif

/* A local set datatype, coincidentally fd_set suits our purpose fine.  */
typedef fd_set set;
#define ISSET FD_ISSET
#define SET FD_SET
#define ZERO FD_ZERO

static int message_check_duplicate (struct message *);
static void message_dump_raw (char *, struct message *);
static int message_encrypt (struct message *);
static int message_index_payload (struct message *, struct payload *, u_int8_t,
				  u_int8_t *);
static int message_parse_transform (struct message *, struct payload *,
				    u_int8_t, u_int8_t *);
static int message_validate_cert (struct message *, struct payload *);
static int message_validate_cert_req (struct message *, struct payload *);
static int message_validate_delete (struct message *, struct payload *);
static int message_validate_hash (struct message *, struct payload *);
static int message_validate_id (struct message *, struct payload *);
static int message_validate_key_exch (struct message *, struct payload *);
static int message_validate_nonce (struct message *, struct payload *);
static int message_validate_notify (struct message *, struct payload *);
static int message_validate_proposal (struct message *, struct payload *);
static int message_validate_sa (struct message *, struct payload *);
static int message_validate_sig (struct message *, struct payload *);
static int message_validate_transform (struct message *, struct payload *);
static int message_validate_vendor (struct message *, struct payload *);

static int (*message_validate_payload[]) (struct message *, struct payload *) =
{
  message_validate_sa, message_validate_proposal, message_validate_transform,
  message_validate_key_exch, message_validate_id, message_validate_cert,
  message_validate_cert_req, message_validate_hash, message_validate_sig,
  message_validate_nonce, message_validate_notify, message_validate_delete,
  message_validate_vendor
};

static struct field *fields[] = {
  isakmp_sa_fld, isakmp_prop_fld, isakmp_transform_fld, isakmp_ke_fld,
  isakmp_id_fld, isakmp_cert_fld, isakmp_certreq_fld, isakmp_hash_fld,
  isakmp_sig_fld, isakmp_nonce_fld, isakmp_notify_fld, isakmp_delete_fld,
  isakmp_vendor_fld
};

/*
 * Fields used for checking monotonic increasing of proposal and transform
 * numbers.
 */
static u_int8_t *last_sa = 0;
static int last_prop_no;
static u_int8_t *last_prop = 0;
static int last_xf_no;

/*
 * Allocate a message structure bound to transport T, and with a first
 * segment buffer sized SZ, copied from BUF if given.
 */
struct message *
message_alloc (struct transport *t, u_int8_t *buf, size_t sz)
{
  struct message *msg;
  int i;

  /*
   * We use calloc(3) because it zeroes the structure which we rely on in
   * message_free when determining what sub-allocations to free.
   */
  msg = (struct message *)calloc (1, sizeof *msg);
  if (!msg)
    return 0;
  msg->iov = calloc (1, sizeof *msg->iov);
  if (!msg->iov)
    {
      message_free (msg);
      return 0;
    }
  msg->iov[0].iov_len = sz;
  msg->iov[0].iov_base = malloc (sz);
  if (!msg->iov[0].iov_base)
    {
      message_free (msg);
      return 0;
    }
  msg->iovlen = 1;
  if (buf)
    memcpy (msg->iov[0].iov_base, buf, sz);
  msg->nextp = msg->iov[0].iov_base + ISAKMP_HDR_NEXT_PAYLOAD_OFF;
  msg->transport = t;
  for (i = ISAKMP_PAYLOAD_SA; i < ISAKMP_PAYLOAD_RESERVED_MIN; i++)
    TAILQ_INIT (&msg->payload[i]);
  TAILQ_INIT (&msg->post_send);
  log_debug (LOG_MESSAGE, 90, "message_alloc: allocated %p", msg);
  return msg;
}

/*
 * Allocate a message sutiable for a reply to MSG.  Just allocate an empty
 * ISAKMP header as the first segment.
 */
struct message *
message_alloc_reply (struct message *msg)
{
  struct message *reply;

  reply = message_alloc (msg->transport, 0, ISAKMP_HDR_SZ);
  reply->exchange = msg->exchange;
  reply->isakmp_sa = msg->isakmp_sa;
  return reply;
}

/* Free up all resources used by the MSG message.  */
void
message_free (struct message *msg)
{
  int i;
  struct payload *payload, *next;
  struct post_send *node;

  log_debug (LOG_MESSAGE, 20, "message_free: freeing %p", msg);
  if (!msg)
    return;
  if (msg->orig && msg->orig != msg->iov[0].iov_base)
    free (msg->orig);
  if (msg->iov)
    {
      for (i = 0; i < msg->iovlen; i++)
	if (msg->iov[i].iov_base)
	  free (msg->iov[i].iov_base);
      free (msg->iov);
    }
  if (msg->retrans)
      timer_remove_event (msg->retrans);
  for (i = ISAKMP_PAYLOAD_SA; i < ISAKMP_PAYLOAD_RESERVED_MIN; i++)
    for (payload = TAILQ_FIRST (&msg->payload[i]); payload; payload = next)
      {
	next = TAILQ_NEXT (payload, link);
	free (payload);
      }
  while ((node = TAILQ_FIRST (&msg->post_send)) != 0)
    TAILQ_REMOVE (&msg->post_send, TAILQ_FIRST (&msg->post_send), link);

  /* If we are on the send queue, remove us from there.  */
  if (msg->flags & MSG_IN_TRANSIT)
    TAILQ_REMOVE (&msg->transport->sendq, msg, link);

  free (msg);
}

/*
 * Generic ISAKMP parser.
 * MSG is the ISAKMP message to be parsed.  NEXT is the type of the first
 * payload to be parsed, and it's pointed to by BUF.  ACCEPTED_PAYLOADS
 * tells what payloads are accepted and FUNC is a pointer to a function
 * to be called for each payload found.  Returns the total length of the
 * parsed payloads.
 */
static int
message_parse_payloads (struct message *msg, struct payload *p, u_int8_t next,
			u_int8_t *buf, set *accepted_payloads,
			int (*func) (struct message *, struct payload *,
				     u_int8_t, u_int8_t *))
{
  u_int8_t payload;
  u_int16_t len;
  int sz = 0;

  do
    {
      log_debug (LOG_MESSAGE, 50,
		 "message_parse_payloads: offset 0x%x payload %s",
		 buf - (u_int8_t *)msg->iov[0].iov_base,
		 constant_name (isakmp_payload_cst, next));

      /* Does this payload's header fit?  */
      if (buf + ISAKMP_GEN_SZ
	  > (u_int8_t *)msg->iov[0].iov_base + msg->iov[0].iov_len)
	{
	  log_print ("message_parse_payloads: short message");
	  message_drop (msg, ISAKMP_NOTIFY_UNEQUAL_PAYLOAD_LENGTHS, 0, 0, 1);
	  return -1;
	}

      /* Ponder on the payload that is at BUF...  */
      payload = next;

      /* Look at the next payload's type.  */
      next = GET_ISAKMP_GEN_NEXT_PAYLOAD (buf);
      if (next >= ISAKMP_PAYLOAD_RESERVED_MIN)
	{
	  log_print ("message_parse_payloads: invalid next payload type %d "
		     "in payload of type %d", next, payload);
	  message_drop (msg, ISAKMP_NOTIFY_INVALID_PAYLOAD_TYPE, 0, 0, 1);
	  return -1;
	}

      /* Reserved fields in ISAKMP messages should be zero.  */
      if (GET_ISAKMP_GEN_RESERVED (buf) != 0)
	{
	  log_print ("message_parse_payloads: reserved field non-zero");
	  message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 0, 1);
	  return -1;
	}

      /*
       * Decode the payload length field.
       */
      len = GET_ISAKMP_GEN_LENGTH (buf);

      /*
       * Check if the current payload is one of the accepted ones at this
       * stage.
       */
      if (!ISSET (payload, accepted_payloads))
	{
	  log_print ("message_parse_payloads: payload type %d unexpected",
		     payload);
	  message_drop (msg, ISAKMP_NOTIFY_INVALID_PAYLOAD_TYPE, 0, 0, 1);
	  return -1;
	}

      /* Call the payload handler specified by the caller.  */
      if (func (msg, p, payload, buf))
	return -1;

      /* Advance to next payload.  */
      buf += len;
      sz += len;
    }
  while (next != ISAKMP_PAYLOAD_NONE);
  return sz;
}

/*
 * Parse a proposal payload found in message MSG.  PAYLOAD is always
 * ISAKMP_PAYLOAD_PROPOSAL and ignored in here.  It's needed as the API for
 * message_parse_payloads requires it.  BUF points to the proposal's
 * generic payload header.
 */
static int
message_parse_proposal (struct message *msg, struct payload *p,
			u_int8_t payload, u_int8_t *buf)
{
  set payload_set;

  /* Put the proposal into the proposal bucket.  */
  message_index_payload (msg, p, payload, buf);

  ZERO (&payload_set);
  SET (ISAKMP_PAYLOAD_TRANSFORM, &payload_set);
  if (message_parse_payloads (msg,
			      TAILQ_LAST (&msg->payload
					  [ISAKMP_PAYLOAD_PROPOSAL],
					  payload_head),
			      ISAKMP_PAYLOAD_TRANSFORM,
			      buf + ISAKMP_PROP_SPI_OFF
			      + GET_ISAKMP_PROP_SPI_SZ (buf),
			      &payload_set, message_parse_transform) == -1)
    return -1;

  return 0;
}

static int
message_parse_transform (struct message *msg, struct payload *p,
			 u_int8_t payload, u_int8_t *buf)
{
  /* Put the transform into the transform bucket.  */
  message_index_payload (msg, p, payload, buf);

  log_debug (LOG_MESSAGE, 50, "Transform %d's attributes",
	     GET_ISAKMP_TRANSFORM_NO (buf));
  attribute_map (buf + ISAKMP_TRANSFORM_SA_ATTRS_OFF,
		 GET_ISAKMP_GEN_LENGTH (buf) - ISAKMP_TRANSFORM_SA_ATTRS_OFF,
		 msg->exchange->doi->debug_attribute, msg);

  return 0;
}

/* Validate the certificate payload P in message MSG.  */
static int
message_validate_cert (struct message *msg, struct payload *p)
{
  if (GET_ISAKMP_CERT_ENCODING (p->p) >= ISAKMP_CERTENC_RESERVED_MIN)
    {
      message_drop (msg, ISAKMP_NOTIFY_INVALID_CERT_ENCODING, 0, 0, 1);
      return -1;
    }
  return 0;
}

/* Validate the certificate request payload P in message MSG.  */
static int
message_validate_cert_req (struct message *msg, struct payload *p)
{
  struct cert_handler *cert;
  size_t len = GET_ISAKMP_GEN_LENGTH (p->p)- ISAKMP_CERTREQ_AUTHORITY_OFF;

  if (GET_ISAKMP_CERTREQ_TYPE (p->p) >= ISAKMP_CERTENC_RESERVED_MIN)
    {
      message_drop (msg, ISAKMP_NOTIFY_INVALID_CERT_ENCODING, 0, 0, 1);
      return -1;
    }

  /* 
   * Check the certificate types we support and if an acceptable authority
   * is included in the payload check if it can be decoded
   */
  if ((cert = cert_get (GET_ISAKMP_CERTREQ_TYPE (p->p))) == NULL ||
      (len && !cert->certreq_validate (p->p + ISAKMP_CERTREQ_AUTHORITY_OFF,
				       len)))
    {
      message_drop (msg, ISAKMP_NOTIFY_CERT_TYPE_UNSUPPORTED, 0, 0, 1);
      return -1;
    }
  return 0;
}

/* Validate the delete payload P in message MSG.  */
static int
message_validate_delete (struct message *msg, struct payload *p)
{
  u_int8_t proto = GET_ISAKMP_DELETE_PROTO (p->p);
  struct doi *doi;

  doi = doi_lookup (GET_ISAKMP_DELETE_DOI (p->p));
  if (!doi)
    {
      log_print ("message_validate_delete: DOI not supported");
      message_free (msg);
      return -1;
    }

  if (proto != ISAKMP_PROTO_ISAKMP && doi->validate_proto (proto))
    {
      log_print ("message_validate_delete: protocol not supported");
      message_free (msg);
      return -1;
    }

  /* Validate the SPIs.  */

  return 0;
}

/*
 * Validate the hash payload P in message MSG.  */
static int
message_validate_hash (struct message *msg, struct payload *p)
{
  /* XXX Not implemented yet.  */
  return 0;
}

/* Validate the identification payload P in message MSG.  */
static int
message_validate_id (struct message *msg, struct payload *p)
{
  struct exchange *exchange = msg->exchange;
  size_t len = GET_ISAKMP_GEN_LENGTH (p->p);

  if (exchange->doi
      && exchange->doi->validate_id_information (GET_ISAKMP_ID_TYPE (p->p),
						 p->p + ISAKMP_ID_DOI_DATA_OFF,
						 p->p + ISAKMP_ID_DATA_OFF,
						 len - ISAKMP_ID_DATA_OFF,
						 exchange))
    {
      message_drop (msg, ISAKMP_NOTIFY_INVALID_ID_INFORMATION, 0, 0, 1);
      return -1;
    }
  return 0;
}

/* Validate the key exchange payload P in message MSG.  */
static int
message_validate_key_exch (struct message *msg, struct payload *p)
{
  struct exchange *exchange = msg->exchange;
  size_t len = GET_ISAKMP_GEN_LENGTH (p->p);

  if (exchange->doi
      && exchange->doi->validate_key_information (p->p + ISAKMP_KE_DATA_OFF,
						  len - ISAKMP_KE_DATA_OFF))
    {
      message_drop (msg, ISAKMP_NOTIFY_INVALID_KEY_INFORMATION, 0, 0, 1);
      return -1;
    }
  return 0;
}

/* Validate the nonce payload P in message MSG.  */
static int
message_validate_nonce (struct message *msg, struct payload *p)
{
  /* Nonces require no specific validation.  */
  return 0;
}

/* Validate the notify payload P in message MSG.  */
static int
message_validate_notify (struct message *msg, struct payload *p)
{
  u_int8_t proto = GET_ISAKMP_NOTIFY_PROTO (p->p);
  u_int16_t type = GET_ISAKMP_NOTIFY_MSG_TYPE (p->p);
  struct doi *doi;

  doi = doi_lookup (GET_ISAKMP_NOTIFY_DOI (p->p));
  if (!doi)
    {
      log_print ("message_validate_notify: DOI not supported");
      message_free (msg);
      return -1;
    }

  if (proto != ISAKMP_PROTO_ISAKMP && doi->validate_proto (proto))
    {
      log_print ("message_validate_notify: protocol not supported");
      message_free (msg);
      return -1;
    }

  /* XXX Validate the SPI.  */

  if (type < ISAKMP_NOTIFY_INVALID_PAYLOAD_TYPE
      || (type >= ISAKMP_NOTIFY_RESERVED_MIN
	  && type < ISAKMP_NOTIFY_PRIVATE_MIN)
      || (type >= ISAKMP_NOTIFY_STATUS_RESERVED1_MIN
	  && type <= ISAKMP_NOTIFY_STATUS_RESERVED1_MAX)
      || (type >= ISAKMP_NOTIFY_STATUS_DOI_MIN
	  && type <= ISAKMP_NOTIFY_STATUS_DOI_MAX
	  && doi->validate_notification (type))
      || type >= ISAKMP_NOTIFY_STATUS_RESERVED2_MIN)
    {
      log_print ("message_validate_notify: message type not supported");
      message_free (msg);
      return -1;
    }
  return 0;
}

/* Validate the proposal payload P in message MSG.  */
static int
message_validate_proposal (struct message *msg, struct payload *p)
{
  u_int8_t proto = GET_ISAKMP_PROP_PROTO (p->p);
  u_int8_t *sa = p->context->p;

  if (proto != ISAKMP_PROTO_ISAKMP
      && msg->exchange->doi->validate_proto (proto))
    {
      message_drop (msg, ISAKMP_NOTIFY_INVALID_PROTOCOL_ID, 0, 0, 1);
      return -1;
    }

  /* Check that we get monotonically increasing proposal IDs per SA.  */
  if (sa != last_sa)
    last_sa = sa;
  else if (GET_ISAKMP_PROP_NO (p->p) < last_prop_no)
    {
      message_drop (msg, ISAKMP_NOTIFY_BAD_PROPOSAL_SYNTAX, 0, 0, 1);
      return -1;
    }
  last_prop_no = GET_ISAKMP_PROP_NO (p->p);

  /* XXX Validate the SPI, and other syntactic things.  */

  return 0;
}

/*
 * Validate the SA payload P in message MSG.
 * Aside from normal validation, note what DOI is in use for other
 * validation routines to look at.  Also index the proposal payloads
 * on the fly.
 * XXX This assumes PAYLOAD_SA is always the first payload
 * to be validated, which is true for IKE, except for quick mode where
 * a PAYLOAD_HASH comes first, but in that specific case it does not matter.
 */
static int
message_validate_sa (struct message *msg, struct payload *p)
{
  set payload_set;
  size_t len;
  u_int32_t doi_id;
  struct exchange *exchange = msg->exchange;
  u_int8_t *pkt = msg->iov[0].iov_base;

  doi_id = GET_ISAKMP_SA_DOI (p->p);
  if (!doi_lookup (doi_id))
    {
      log_print ("message_validate_sa: DOI not supported");
      message_drop (msg, ISAKMP_NOTIFY_DOI_NOT_SUPPORTED, 0, 0, 1);
      return -1;
    }

  /*
   * It's time to figure out what SA this message is about.  If it is
   * already set, then we are creating a new phase 1 SA.  Otherwise, lookup
   * the SA using the cookies and the message ID.  If we cannot find
   * it, setup a phase 2 SA.
   * XXX Is this correct?
   */
  if (!exchange)
    {
      if (zero_test (pkt + ISAKMP_HDR_RCOOKIE_OFF, ISAKMP_HDR_RCOOKIE_LEN))
	exchange = exchange_setup_p1 (msg, doi_id);
      else
	exchange = exchange_setup_p2 (msg, doi_id);
      if (!exchange)
	{
	  /* Log?  */
	  message_free (msg);
	  return -1;
	}
    }
  msg->exchange = exchange;

  /*
   * Create a struct sa for each SA payload handed to us unless we are the
   * inititor where we only will count them.
   */
  if (exchange->initiator)
    {
      /* XXX Count SA payloads.  */
    }
  else if (sa_create (exchange, msg->transport))
    {
      /* XXX Remove exchange if we just created it?   */
      message_free (msg);
      return -1;
    }

  if (exchange->phase == 1)
    msg->isakmp_sa = TAILQ_FIRST (&exchange->sa_list);

  /*
   * Let the DOI validate the situation, at the same time it tells us what
   * the length of the situation field is.
   */
  if (exchange->doi->validate_situation (p->p + ISAKMP_SA_SIT_OFF, &len))
    {
      log_print ("message_validate_sa: situation not supported");
      message_drop (msg, ISAKMP_NOTIFY_SITUATION_NOT_SUPPORTED, 0, 0, 1);
      return -1;
    }

  /* Reset the fields we base our proposal & transform number checks on.  */
  last_sa = last_prop = 0;
  last_prop_no = last_xf_no = 0;

  /* Go through the PROPOSAL payloads.  */
  ZERO (&payload_set);
  SET (ISAKMP_PAYLOAD_PROPOSAL, &payload_set);
  if (message_parse_payloads (msg, p, ISAKMP_PAYLOAD_PROPOSAL,
			      p->p + ISAKMP_SA_SIT_OFF + len, &payload_set,
			      message_parse_proposal) == -1)
    return -1;

  return 0;
}

/* Validate the signature payload P in message MSG.  */
static int
message_validate_sig (struct message *msg, struct payload *p)
{
  /* XXX Not implemented yet.  */
  return 0;
}

/* Validate the transform payload P in message MSG.  */
static int
message_validate_transform (struct message *msg, struct payload *p)
{
  u_int8_t proto = GET_ISAKMP_PROP_PROTO (p->context->p);
  u_int8_t *prop = p->context->p;

  if (msg->exchange->doi
      ->validate_transform_id (proto, GET_ISAKMP_TRANSFORM_ID (p->p)))
    {
      message_drop (msg, ISAKMP_NOTIFY_INVALID_TRANSFORM_ID, 0, 0, 1);
      return -1;
    }

  /* Check that the reserved field is zero.  */
  if (!zero_test (p->p + ISAKMP_TRANSFORM_RESERVED_OFF,
		  ISAKMP_TRANSFORM_RESERVED_LEN))
    {
      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 0, 1);
      return -1;
    }

  /*
   * Check that we get monotonically increasing transform numbers per proposal.
   */
  if (prop != last_prop)
    last_prop = prop;
  else if (GET_ISAKMP_TRANSFORM_NO (p->p) <= last_xf_no)
    {
      message_drop (msg, ISAKMP_NOTIFY_BAD_PROPOSAL_SYNTAX, 0, 0, 1);
      return -1;
    }
  last_xf_no = GET_ISAKMP_TRANSFORM_NO (p->p);

  /* Validate the attributes.  */
  if (attribute_map (p->p + ISAKMP_TRANSFORM_SA_ATTRS_OFF,
		     GET_ISAKMP_GEN_LENGTH (p->p)
		     - ISAKMP_TRANSFORM_SA_ATTRS_OFF,
		     msg->exchange->doi->validate_attribute, msg))
    {
      message_drop (msg, ISAKMP_NOTIFY_ATTRIBUTES_NOT_SUPPORTED, 0, 0, 1);
      return -1;
    }

  return 0;
}

/* Validate the vendor payload P in message MSG.  */
static int
message_validate_vendor (struct message *msg, struct payload *p)
{
  /* Vendor IDs are only allowed in phase 1.  */
  if (msg->exchange->phase != 1)
    {
      message_drop (msg, ISAKMP_NOTIFY_INVALID_PAYLOAD_TYPE, 0, 0, 1);
      return -1;
    }

  log_debug (LOG_MESSAGE, 40, "message_validate_vendor: vendor ID seen");
  return 0;
}

/*
 * Add an index-record pointing to the payload at BUF in message MSG
 * to the PAYLOAD bucket of payloads.  This allows us to quickly reference
 * payloads by type.  Also stash the parent payload P link into the new
 * node so we can go from transforms -> payloads -> SAs.
 */
static int
message_index_payload (struct message *msg, struct payload *p,
		       u_int8_t payload, u_int8_t *buf)
{
  struct payload *payload_node;

  /* Put the payload pointer into the right bucket.  */
  payload_node = malloc (sizeof *payload_node);
  if (!payload_node)
    return -1;
  payload_node->p = buf;
  payload_node->context = p;
  payload_node->flags = 0;
  TAILQ_INSERT_TAIL (&msg->payload[payload], payload_node, link);
  return 0;
}

/*
 * Group each payload found in MSG by type for easy reference later.
 * While doing this, validate the generic parts of the message structure too.
 * NEXT is the 1st payload's type.  This routine will also register the
 * computed message length (i.e. without padding) in msg->iov[0].iov_len.
 */
static int
message_sort_payloads (struct message *msg, u_int8_t next)
{
  set payload_set;
  int i, sz;

  ZERO (&payload_set);
  for (i = ISAKMP_PAYLOAD_SA; i < ISAKMP_PAYLOAD_RESERVED_MIN; i++)
    if (i != ISAKMP_PAYLOAD_PROPOSAL && i != ISAKMP_PAYLOAD_TRANSFORM)
      SET (i, &payload_set);
  sz = message_parse_payloads (msg, 0, next,
			       msg->iov[0].iov_base + ISAKMP_HDR_SZ,
			       &payload_set, message_index_payload);
  if (sz == -1)
    return -1;
  msg->iov[0].iov_len = ISAKMP_HDR_SZ + sz;
  SET_ISAKMP_HDR_LENGTH (msg->iov[0].iov_base, ISAKMP_HDR_SZ + sz);
  return 0;
}

/* Run all the generic payload tests that the drafts specify.  */
static int
message_validate_payloads (struct message *msg)
{
  int i;
  struct payload *p;

  for (i = ISAKMP_PAYLOAD_SA; i < ISAKMP_PAYLOAD_RESERVED_MIN; i++)
    for (p = TAILQ_FIRST (&msg->payload[i]); p; p = TAILQ_NEXT (p, link))
      {
	log_debug (LOG_MESSAGE, 60,
		   "message_validate_payloads: payload %s at %p of message %p",
		   constant_name (isakmp_payload_cst, i), p->p, msg);
	field_dump_payload (fields[i - ISAKMP_PAYLOAD_SA], p->p);
	if (message_validate_payload[i - ISAKMP_PAYLOAD_SA] (msg, p))
	  return -1;
      }
  return 0;
}

/*
 * All incoming messages go through here.  We do generic validity checks
 * and try to find or establish SAs.  Last but not least we try to find
 * the exchange this message, MSG, is part of, and feed it there.
 */
int
message_recv (struct message *msg)
{
  u_int8_t *buf = msg->iov[0].iov_base;
  size_t sz = msg->iov[0].iov_len;
  u_int8_t exch_type;
  int setup_isakmp_sa, msgid_is_zero;
  u_int8_t flags;
  struct keystate *ks = 0;

  /* Possibly dump a raw hex image of the message to the log channel.  */
  message_dump_raw ("message_recv", msg);

  /* Messages shorter than an ISAKMP header are bad.  */
  if (sz < ISAKMP_HDR_SZ || sz != GET_ISAKMP_HDR_LENGTH (buf))
    {
      log_print ("message_recv: bad message length");
      message_drop (msg, ISAKMP_NOTIFY_UNEQUAL_PAYLOAD_LENGTHS, 0, 0, 1);
      return -1;
    }

  /*
   * If the responder cookie is zero, this is a request to setup an ISAKMP SA.
   * Otherwise the cookies should refer to an existing ISAKMP SA.
   *
   * XXX This is getting ugly, please reread later to see if it can be made
   * nicer.
   */
  setup_isakmp_sa = zero_test (buf + ISAKMP_HDR_RCOOKIE_OFF,
			       ISAKMP_HDR_RCOOKIE_LEN);
  if (setup_isakmp_sa)
    {
      /*
       * This might be a retransmission of a former ISAKMP SA setup message.
       * If so, just drop it.
       * XXX Must we really look in both the SA and exchange pools?
       */
      if (exchange_lookup_from_icookie (buf + ISAKMP_HDR_ICOOKIE_OFF)
	  || sa_lookup_from_icookie (buf + ISAKMP_HDR_ICOOKIE_OFF))
	{
	  log_debug (LOG_MESSAGE, 90,
		     "message_recv: dropping setup for existing SA");
	  message_free (msg);
	  return -1;
	}
    }
  else
    {
      msg->isakmp_sa = sa_lookup_by_header (buf, 0);

      /*
       * If we cannot find an ISAKMP SA out of the cookies, this is either
       * a responder's first reply, and we need to upgrade our exchange,
       * or it's just plain invalid cookies.
       */
      if (!msg->isakmp_sa)
	{
	  msg->exchange
	    = exchange_lookup_from_icookie (buf + ISAKMP_HDR_ICOOKIE_OFF);
	  if (msg->exchange)
	    exchange_upgrade_p1 (msg);
	  else
	    {
	      log_print ("message_recv: invalid cookie");
	      message_drop (msg, ISAKMP_NOTIFY_INVALID_COOKIE, 0, 0, 1);
	      return -1;
	    }
#if 0
	  msg->isakmp_sa
	    = sa_lookup_from_icookie (buf + ISAKMP_HDR_ICOOKIE_OFF);
	  if (msg->isakmp_sa)
	    sa_isakmp_upgrade (msg);
#endif
	}
      msg->exchange = exchange_lookup (buf, 1);
    }

  if (message_check_duplicate (msg))
    return -1;

  if (GET_ISAKMP_HDR_NEXT_PAYLOAD (buf) >= ISAKMP_PAYLOAD_RESERVED_MIN)
    {
      log_print ("message_recv: invalid payload type %d in ISAKMP header",
		 GET_ISAKMP_HDR_NEXT_PAYLOAD (buf));
      message_drop (msg, ISAKMP_NOTIFY_INVALID_PAYLOAD_TYPE, 0, 0, 1);
      return -1;
    }

  /* Validate that the message is of version 1.0.  */
  if (ISAKMP_VERSION_MAJOR (GET_ISAKMP_HDR_VERSION (buf)) != 1)
    {
      log_print ("message_recv: invalid version major %d",
		 ISAKMP_VERSION_MAJOR (GET_ISAKMP_HDR_VERSION (buf)));
      message_drop (msg, ISAKMP_NOTIFY_INVALID_MAJOR_VERSION, 0, 0, 1);
      return -1;
    }

  if (ISAKMP_VERSION_MINOR (GET_ISAKMP_HDR_VERSION (buf)) != 0)
    {
      log_print ("message_recv: invalid version minor %d",
		 ISAKMP_VERSION_MINOR (GET_ISAKMP_HDR_VERSION (buf)));
      message_drop (msg, ISAKMP_NOTIFY_INVALID_MINOR_VERSION, 0, 0, 1);
      return -1;
    }

  /*
   * Validate the exchange type.  If it's a DOI-specified exchange wait until
   * after all payloads have been seen for the validation as the SA payload
   * might not yet have been parsed, thus the DOI might be unknown.
   */
  exch_type = GET_ISAKMP_HDR_EXCH_TYPE (buf);
  if (exch_type == ISAKMP_EXCH_NONE
      || (exch_type >= ISAKMP_EXCH_FUTURE_MIN &&
	  exch_type <= ISAKMP_EXCH_FUTURE_MAX)
      || (setup_isakmp_sa && exch_type >= ISAKMP_EXCH_DOI_MIN))
    {
      log_print ("message_recv: invalid exchange type %s",
		 constant_name (isakmp_exch_cst, exch_type));
      message_drop (msg, ISAKMP_NOTIFY_INVALID_EXCHANGE_TYPE, 0, 0, 1);
      return -1;
    }

  /*
   * Check for unrecognized flags, or the encryption flag when we don't
   * have an ISAKMP SA to decrypt with.
   */
  flags = GET_ISAKMP_HDR_FLAGS (buf);
  if (flags
      & ~(ISAKMP_FLAGS_ENC | ISAKMP_FLAGS_COMMIT | ISAKMP_FLAGS_AUTH_ONLY))
    {
      log_print ("message_recv: invalid flags 0x%x",
		 GET_ISAKMP_HDR_FLAGS (buf));
      message_drop (msg, ISAKMP_NOTIFY_INVALID_FLAGS, 0, 0, 1);
      return -1;
    }

  /* If we are about to setup an ISAKMP SA, the message ID must be zero.  */
  msgid_is_zero = zero_test (buf + ISAKMP_HDR_MESSAGE_ID_OFF,
			     ISAKMP_HDR_MESSAGE_ID_LEN);
  if (setup_isakmp_sa && !msgid_is_zero)
    {
      log_print ("message_recv: invalid message id");
      message_drop (msg, ISAKMP_NOTIFY_INVALID_MESSAGE_ID, 0, 0, 1);
      return -1;
    }

  if (!setup_isakmp_sa && msgid_is_zero)
    {
      msg->exchange = exchange_lookup (buf, 0);
      if (!msg->exchange)
	{
	  log_print ("message_recv: phase 1 message after ISAKMP SA is ready");

	  /*
	   * Retransmit final message of the exchange that set up the
	   * ISAKMP SA.
	   */
	  if (msg->isakmp_sa->last_sent_in_setup)
	    {
	      log_debug (LOG_MESSAGE, 80,
			 "message_recv: resending last message from phase 1");
	      message_send (msg->isakmp_sa->last_sent_in_setup);
	    }

	  return -1;
	}
    }

  if (flags & ISAKMP_FLAGS_ENC)
    {
      /* Decrypt rest of message using a DOI-specified IV.  */
      ks = msg->isakmp_sa->doi->get_keystate (msg);
      if (!ks)
	{
	  message_free (msg);
	  return -1;
	}
      msg->orig = malloc (sz);
      if (!msg->orig)
	{
	  message_free (msg);
	  return -1;
	}
      memcpy (msg->orig, buf, sz);
      crypto_decrypt (ks, buf + ISAKMP_HDR_SZ, sz - ISAKMP_HDR_SZ);
    }
  else
    msg->orig = buf;
  msg->orig_sz = sz;

  /*
   * Check the overall payload structure at the same time as indexing them by
   * type.
   */
  if (GET_ISAKMP_HDR_NEXT_PAYLOAD (buf) != ISAKMP_PAYLOAD_NONE
      && message_sort_payloads (msg, GET_ISAKMP_HDR_NEXT_PAYLOAD (buf)))
    return -1;

  /*
   * Run generic payload tests now.  If anything fails these checks, the
   * message needs either to be retained for later duplicate checks or
   * freed entirely.
   * XXX Should SAs and even transports be cleaned up then too?
   */
  if (message_validate_payloads (msg))
    return -1;

  /*
   * Now we can validate DOI-specific exchange types.  If we have no SA
   * DOI-specific exchange types are definitely wrong.
   */
  if (exch_type >= ISAKMP_EXCH_DOI_MIN && exch_type <= ISAKMP_EXCH_DOI_MAX
      && (!msg->exchange || msg->exchange->doi->validate_exchange (exch_type)))
    {
      log_print ("message_recv: invalid DOI exchange type %d", exch_type);
      message_drop (msg, ISAKMP_NOTIFY_INVALID_EXCHANGE_TYPE, 0, 0, 1);
      return -1;
    }

  /* If we have not found an exchange by now something is definitely wrong.  */
  if (!msg->exchange)
    {
      log_print ("message_recv: no exchange");
      /* XXX I got here in an informational exchange.. I should not!  */
      message_drop (msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 0, 1);
      return -1;
    }

  /* Make sure the IV we used gets saved in the proper SA.  */
  if (!msg->exchange->keystate && ks)
    {
      msg->exchange->keystate = ks;
      msg->exchange->crypto = ks->xf;
    }

  /* Handle the flags.  */
  if (flags & ISAKMP_FLAGS_ENC)
    msg->exchange->flags |= EXCHANGE_FLAG_ENCRYPT;
  if ((msg->exchange->flags & EXCHANGE_FLAG_COMMITTED) == 0
      && (flags & ISAKMP_FLAGS_COMMIT))
    msg->exchange->flags |= EXCHANGE_FLAG_HE_COMMITTED;

  /* OK let the exchange logic do the rest.  */
  exchange_run (msg);

  return 0;
}

/* Queue up message MSG for transmittal.  */
void
message_send (struct message *msg)
{
  struct exchange *exchange = msg->exchange;

  /* Reset the retransmission event.  */
  msg->retrans = 0;

  /*
   * If the ISAKMP SA has set up encryption, encrypt the message.
   * However, in a retransmit, it is already encrypted.
   */
  if ((msg->flags & MSG_ENCRYPTED) == 0
      && exchange->flags & EXCHANGE_FLAG_ENCRYPT)
    {
      if (!exchange->keystate)
	{
	  exchange->keystate = exchange->doi->get_keystate (msg);
	  exchange->crypto = exchange->keystate->xf;
	  exchange->flags |= EXCHANGE_FLAG_ENCRYPT;
	}

      if (message_encrypt (msg))
	{
	  /* XXX Log.  */
	  return;
	}
    }

  /* Keep the COMMIT bit on.  */
  if (exchange && exchange->flags & EXCHANGE_FLAG_COMMITTED)
    SET_ISAKMP_HDR_FLAGS (msg->iov[0].iov_base,
			  GET_ISAKMP_HDR_FLAGS (msg->iov[0].iov_base)
			  | ISAKMP_FLAGS_COMMIT);

  message_dump_raw ("message_send", msg);
  msg->flags |= MSG_IN_TRANSIT;
  TAILQ_INSERT_TAIL (&msg->transport->sendq, msg, link);
}

/*
 * Setup the ISAKMP message header for message MSG.  EXCHANGE is the exchange
 * type, FLAGS are the ISAKMP header flags and MSG_ID is message ID
 * identifying the exchange.
 */
void
message_setup_header (struct message *msg, u_int8_t exchange, u_int8_t flags,
		      u_int8_t *msg_id)
{
  u_int8_t *buf = msg->iov[0].iov_base;

  SET_ISAKMP_HDR_ICOOKIE (buf, msg->exchange->cookies);
  SET_ISAKMP_HDR_RCOOKIE (buf,
			  msg->exchange->cookies + ISAKMP_HDR_ICOOKIE_LEN);
  SET_ISAKMP_HDR_NEXT_PAYLOAD (buf, ISAKMP_PAYLOAD_NONE);
  SET_ISAKMP_HDR_VERSION (buf, ISAKMP_VERSION_MAKE (1, 0));
  SET_ISAKMP_HDR_EXCH_TYPE (buf, exchange);
  SET_ISAKMP_HDR_FLAGS (buf, flags);
  SET_ISAKMP_HDR_MESSAGE_ID (buf, msg_id);
  SET_ISAKMP_HDR_LENGTH (buf, msg->iov[0].iov_len);
}

/*
 * Add the payload of type PAYLOAD in BUF sized SZ to the MSG message.
 * The caller thereby is released from the responsibility of freeing BUF,
 * unless we return a failure of course.  If LINK is set the former
 * payload's "next payload" field to PAYLOAD.
 *
 * XXX We might want to resize the iov array several slots at a time.
 */
int
message_add_payload (struct message *msg, u_int8_t payload, u_int8_t *buf,
		     size_t sz, int link)
{
  struct iovec *new_iov;
  struct payload *payload_node;

  payload_node = malloc (sizeof *payload_node);
  if (!payload_node)
    return -1;
  new_iov
    = (struct iovec *)realloc (msg->iov, (msg->iovlen + 1) * sizeof *msg->iov);
  if (!new_iov)
    {
      free (payload_node);
      return -1;
    }
  msg->iov = new_iov;
  new_iov[msg->iovlen].iov_base = buf;
  new_iov[msg->iovlen].iov_len = sz;
  msg->iovlen++;
  if (link)
    *msg->nextp = payload;
  msg->nextp = buf + ISAKMP_GEN_NEXT_PAYLOAD_OFF;
  *msg->nextp = ISAKMP_PAYLOAD_NONE;
  SET_ISAKMP_GEN_RESERVED (buf, 0);
  SET_ISAKMP_GEN_LENGTH (buf, sz);
  SET_ISAKMP_HDR_LENGTH (msg->iov[0].iov_base,
			 GET_ISAKMP_HDR_LENGTH (msg->iov[0].iov_base) + sz);

  /*
   * For the sake of exchange_validate we index the payloads even in outgoing
   * messages, however context and flags are uninteresting in this situation.
   */
  payload_node->p = buf;
  TAILQ_INSERT_TAIL (&msg->payload[payload], payload_node, link);
  return 0;
}

/* XXX Move up when ready.  */
struct info_args {
  char discr;
  u_int32_t doi;
  u_int8_t proto;
  u_int16_t spi_sz;
  union {
    struct {
      u_int16_t msg_type;
      u_int8_t *spi;
    } n;
    struct {
      u_int16_t nspis;
      u_int8_t **spi;
    } d;
  } u;
};

/*
 * As a reaction to the incoming message MSG create an informational exchange
 * protected by ISAKMP_SA and send a notify payload of type NOTIFY, with
 * fields initialized from SA.  INCOMING is true if the SPI field should be
 * filled with the incoming SPI and false if it is to be filled with the
 * outgoing one.
 *
 * XXX Should we handle sending multiple notify payloads?  The draft allows
 * it, but do we need it?  Furthermore, should we not return a success
 * status value?
 */
void
message_send_notification (struct message *msg, struct sa *isakmp_sa,
			   u_int16_t notify, struct proto *proto,
			   int incoming)
{
  struct info_args args;

  args.discr = 'N';
  args.doi = proto ? proto->sa->doi->id : 0;
  args.proto = proto ? proto->proto : 0;
  args.spi_sz = proto ? proto->spi_sz[incoming] : 0;
  args.u.n.msg_type = notify;
  args.u.n.spi = proto ? proto->spi[incoming] : 0;
  if (isakmp_sa->flags & SA_FLAG_READY)
    exchange_establish_p2 (isakmp_sa, ISAKMP_EXCH_INFO, 0, &args, 0 ,0);
  else
    exchange_establish_p1 (msg->transport, ISAKMP_EXCH_INFO,
			   msg->exchange->doi->id, 0, &args, 0, 0);
}

void
message_send_info (struct message *msg)
{
  u_int8_t *buf;
  size_t sz;
  struct info_args *args = msg->extra;
  u_int8_t payload;

  sz = (args->discr == 'N' ? ISAKMP_NOTIFY_SPI_OFF + args->spi_sz
	: ISAKMP_DELETE_SPI_OFF + args->u.d.nspis * args->spi_sz);
  buf = calloc (1, sz);
  switch (args->discr)
    {
    case 'N':
      /* Build the NOTIFY payload.  */
      payload = ISAKMP_PAYLOAD_NOTIFY;
      SET_ISAKMP_NOTIFY_DOI (buf, args->doi);
      SET_ISAKMP_NOTIFY_PROTO (buf, args->proto);
      SET_ISAKMP_NOTIFY_SPI_SZ (buf, args->spi_sz);
      SET_ISAKMP_NOTIFY_MSG_TYPE (buf, args->u.n.msg_type);
      memcpy (buf + ISAKMP_NOTIFY_SPI_OFF, args->u.n.spi, args->spi_sz);
      break;

    case 'D':
    default:			/* Silence GCC.  */
      /* Build the DELETE payload.  */
      payload = ISAKMP_PAYLOAD_DELETE;
      SET_ISAKMP_DELETE_DOI (buf, args->doi);
      SET_ISAKMP_DELETE_PROTO (buf, args->proto);
      SET_ISAKMP_DELETE_SPI_SZ (buf, args->spi_sz);
      SET_ISAKMP_DELETE_NSPIS (buf, args->u.d.nspis);
      memcpy (buf + ISAKMP_DELETE_SPI_OFF, args->u.d.spi,
	      args->u.d.nspis * args->spi_sz);
      break;
    }

  if (message_add_payload (msg, payload, buf, sz, 1))
    {
      /* Log!  */
      message_free (msg);
      return;
    }
}

/*
 * Drop the MSG message due to reason given in NOTIFY.  If NOTIFY is set
 * send out a notification to the originator.
 */
void
message_drop (struct message *msg, int notify, struct proto *proto,
	      int initiator, int clean)
{
  struct transport *t = msg->transport;
  struct sockaddr *dst;
  int dst_len;

  t->vtbl->get_dst (t, &dst, &dst_len);

  /* XXX Assumes IPv4.  */
  log_print ("dropped message from %s port %d due to notification type %s",
	     inet_ntoa (((struct sockaddr_in *)dst)->sin_addr),
	     ntohs (((struct sockaddr_in *)dst)->sin_port),
	     constant_name (isakmp_notify_cst, notify));

  /*
   * If specified and we have at least an ISAKMP SA, return a notification.
   * XXX how to setup my SA.
   */
  if (notify && msg->isakmp_sa)
    message_send_notification (msg, msg->isakmp_sa, notify, proto, initiator);
  if (clean)
    message_free (msg);
}

/*
 * If the user demands debug printouts, printout MSG with as much detail
 * as we can without resorting to per-payload handling.
 */
static void
message_dump_raw (char *header, struct message *msg)
{
  int i, j, k = 0;
  char buf[80], *p = buf;

  /* XXX Should we use LOG_MESSAGE instead?  */
  log_debug (LOG_TRANSPORT, 10, "%s: message %p", header, msg);
  field_dump_payload (isakmp_hdr_fld, msg->iov[0].iov_base);
  for (i = 0; i < msg->iovlen; i++)
    for (j = 0; j < msg->iov[i].iov_len; j++)
      {
	sprintf(p, "%02x", ((u_int8_t *)msg->iov[i].iov_base)[j]);
	p += 2;
	if (++k % 32 == 0)
	  {
	    *p = '\0';
	    log_debug (LOG_TRANSPORT, 10, "%s", buf);
	    p = buf;
	  }
	else if (k % 4 == 0)
	  *p++ = ' ';
      }
  *p = '\0';
  if (p != buf)
    log_debug (LOG_TRANSPORT, 10, "%s", buf);
}

/*
 * Encrypt an outgoing message MSG.  As outgoing messages are represented
 * with an iovec with one segment per payload, we need to coalesce them
 * into just une buffer containing all payloads and some padding before
 * we encrypt.
 */
static int
message_encrypt (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  size_t sz = 0;
  u_int8_t *buf;
  int i;

  /* If no payloads, nothing to do.  */
  if (msg->iovlen == 1)
    return 0;

  /*
   * For encryption we need to put all payloads together in a single buffer.
   * This buffer should be padded to the current crypto transform's blocksize.
   */
  for (i = 1; i < msg->iovlen; i++)
    sz += msg->iov[i].iov_len;
  sz = ((sz + exchange->crypto->blocksize - 1) / exchange->crypto->blocksize)
    * exchange->crypto->blocksize;
  buf = realloc (msg->iov[1].iov_base, sz);
  if (!buf)
    return -1;
  msg->iov[1].iov_base = buf;
  for (i = 2; i < msg->iovlen; i++)
    {
      memcpy (buf + msg->iov[1].iov_len, msg->iov[i].iov_base,
	      msg->iov[i].iov_len);
      msg->iov[1].iov_len += msg->iov[i].iov_len;
      free (msg->iov[i].iov_base);
    }

  /* Pad with zeroes.  */
  memset (buf + msg->iov[1].iov_len, '\0', sz - msg->iov[1].iov_len);
  msg->iov[1].iov_len = sz;
  msg->iovlen = 2;

  SET_ISAKMP_HDR_FLAGS (msg->iov[0].iov_base,
			GET_ISAKMP_HDR_FLAGS (msg->iov[0].iov_base)
			| ISAKMP_FLAGS_ENC);
  SET_ISAKMP_HDR_LENGTH (msg->iov[0].iov_base, ISAKMP_HDR_SZ + sz);
  crypto_encrypt (exchange->keystate, buf, msg->iov[1].iov_len);
  msg->flags |= MSG_ENCRYPTED;

  /* Update the IV so we can decrypt the next incoming message.  */
  crypto_update_iv (exchange->keystate);

  return 0;
}

/*
 * Check whether the message MSG is a duplicate of the last one negotiating
 * this specific SA.
 */
static int
message_check_duplicate (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  size_t sz = msg->iov[0].iov_len;
  u_int8_t *pkt = msg->iov[0].iov_base;

  /* If no SA has been found, we cannot test, thus it's good.  */
  if (!exchange)
    return 0;

  log_debug (LOG_MESSAGE, 90, "message_check_duplicate: last_received 0x%x",
	     exchange->last_received);
  if (exchange->last_received)
    {
      log_debug_buf (LOG_MESSAGE, 95, "message_check_duplicate: last_received",
		     exchange->last_received->orig,
		     exchange->last_received->orig_sz);
      /* Is it a duplicate, lose the new one.  */
      if (sz == exchange->last_received->orig_sz
	  && memcmp (pkt, exchange->last_received->orig, sz) == 0)
	{
	  log_debug (LOG_MESSAGE, 80, "message_check_duplicate: dropping dup");
	  /* XXX Should we do an early retransmit of our last message?  */
	  message_free (msg);
	  return -1;
	}
    }

  /*
   * As this new message is an indication that state is moving forward
   * at the peer, remove the retransmit timer on our last message.
   */
  if (exchange->last_sent)
    {
      message_free (exchange->last_sent);
      exchange->last_sent = 0;
    }

  return 0;
}

/* Helper to message_negotiate_sa.  */
static INLINE struct payload *
step_transform (struct payload *tp, struct payload **propp,
		struct payload **sap)
{
  tp = TAILQ_NEXT (tp, link);
  if (tp)
    {
      *propp = tp->context;
      *sap = (*propp)->context;
    }
  return tp;
}

/*
 * Pick out the first transforms out of MSG (which should contain at least one
 * SA payload) we accept as a full protection suite.
 */
int
message_negotiate_sa (struct message *msg,
		      int (*validate) (struct exchange *, struct sa *))
{
  struct payload *tp, *propp, *sap, *next_tp = 0, *next_propp, *next_sap;
  struct payload *saved_tp = 0, *saved_propp = 0, *saved_sap = 0;
  struct sa *sa;
  struct proto *proto;
  int suite_ok_so_far = 0;
  struct exchange *exchange = msg->exchange;
  u_int8_t *spi;
  size_t spi_sz;

  /*
   * This algorithm is a weird bottom-up thing... mostly due to the
   * payload links pointing upwards.
   *
   * The algorithm goes something like this:
   * Foreach transform
   *   If transform is compatible
   *     Remember that this protocol can work
   *     Skip to last transform of this protocol
   *   If next transform belongs to a new protocol inside the same suite
   *     If no transform was found for the current protocol
   *       Forget all earlier transforms for protocols in this suite
   *       Skip to last transform of this suite
   *   If next transform belongs to a new suite
   *     If the current protocol had an OK transform
   *       Skip to the last transform of this SA
   *   If the next transform belongs to a new SA
   *     If no transforms have been chosen
   *       Issue a NO_PROPOSAL_CHOSEN notification
   */

  sa = TAILQ_FIRST (&exchange->sa_list);
  for (tp = TAILQ_FIRST (&msg->payload[ISAKMP_PAYLOAD_TRANSFORM]); tp;
       tp = next_tp)
    {
      propp = tp->context;
      sap = propp->context;
      sap->flags |= PL_MARK;
      next_tp = step_transform (tp, &next_propp, &next_sap);

      /* For each transform, see if it is compatible.  */
      if (!attribute_map (tp->p + ISAKMP_TRANSFORM_SA_ATTRS_OFF,
			  GET_ISAKMP_GEN_LENGTH (tp->p)
			  - ISAKMP_TRANSFORM_SA_ATTRS_OFF,
			  exchange->doi->is_attribute_incompatible, msg))
	{
	  log_debug (LOG_MESSAGE, 30,
		     "message_negotiate_sa: "
		     "transform %d proto %d proposal %d ok",
		     GET_ISAKMP_TRANSFORM_NO (tp->p),
		     GET_ISAKMP_PROP_PROTO (propp->p),
		     GET_ISAKMP_PROP_NO (propp->p));
	  if (sa_add_transform (sa, tp, exchange->initiator, &proto))
	    goto cleanup;
	  suite_ok_so_far = 1;

	  saved_tp = next_tp;
	  saved_propp = next_propp;
	  saved_sap = next_sap;
	  /* Skip to last transform of this protocol proposal.  */
	  while ((next_tp = step_transform (tp, &next_propp, &next_sap))
		 && next_propp == propp)
	    tp = next_tp;
	}

    retry_transform:
      /*
       * Figure out if we will be looking at a new protocol proposal
       * inside the current protection suite.
       */
      if (next_tp && propp != next_propp && sap == next_sap
	  && (GET_ISAKMP_PROP_NO (propp->p)
	      == GET_ISAKMP_PROP_NO (next_propp->p)))
	{
	  if (!suite_ok_so_far)
	    {
	      log_debug (LOG_MESSAGE, 30,
			 "message_negotiate_sa: proto %d proposal %d failed",
			 GET_ISAKMP_PROP_PROTO (propp->p),
			 GET_ISAKMP_PROP_NO (propp->p));
	      /* Remove potentially succeeded choices from the SA.  */
	      while (TAILQ_FIRST (&sa->protos))
		TAILQ_REMOVE (&sa->protos, TAILQ_FIRST (&sa->protos), link);
	      
	      /* Skip to the last transform of this protection suite.  */
	      while ((next_tp = step_transform (tp, &next_propp, &next_sap))
		     && (GET_ISAKMP_PROP_NO (next_propp->p)
			 == GET_ISAKMP_PROP_NO (propp->p))
		     && next_sap == sap)
		tp = next_tp;
	    }
	  suite_ok_so_far = 0;
	}

      /* Figure out if we will be looking at a new protection suite.  */
      if (!next_tp
	  || (propp != next_propp
	      && (GET_ISAKMP_PROP_NO (propp->p)
		  != GET_ISAKMP_PROP_NO (next_propp->p)))
	  || sap != next_sap)
	{
	  /*
	   * Check if the suite we just considered was OK, if so we check
	   * it against the accepted ones. 
	   */
	  if (suite_ok_so_far)
	    {
	      if (!validate || validate (exchange, sa))
		{
		  log_debug (LOG_MESSAGE, 30,
			     "message_negotiate_sa: proposal %d succeeded",
			     GET_ISAKMP_PROP_NO (propp->p));

		  /* Record the other guy's (i.e. our outgoing) SPI.  */
		  spi_sz = GET_ISAKMP_PROP_SPI_SZ (propp->p);
		  if (spi_sz)
		    {
		      spi = malloc (spi_sz);
		      if (!spi)
			goto cleanup;
		      memcpy (spi, propp->p + ISAKMP_PROP_SPI_OFF, spi_sz);
		    }
		  else
		    spi = 0;
		  TAILQ_FIRST (&sa->protos)->spi[0] = spi;
		  log_debug_buf (LOG_MESSAGE, 40, "message_negotiate_sa: SPI",
				 spi, spi_sz);

		  /* Skip to the last transform of this SA.  */
		  while ((next_tp
			  = step_transform (tp, &next_propp, &next_sap))
			 && next_sap == sap)
		    tp = next_tp;
		}
	      else
		{
		  /* Backtrack.  */
		  log_debug (LOG_MESSAGE, 30,
			     "message_negotiate_sa: proposal %d failed", 
			     GET_ISAKMP_PROP_NO (propp->p));
		  next_tp = saved_tp;
		  next_propp = saved_propp;
		  next_sap = saved_sap;
		  suite_ok_so_far = 0;

		  /* Remove potentially succeeded choices from the SA.  */
		  while (TAILQ_FIRST (&sa->protos))
		    TAILQ_REMOVE (&sa->protos, TAILQ_FIRST (&sa->protos),
				  link);
		  goto retry_transform;
		}
	    }
	}

      /* Have we walked all the proposals of an SA?  */
      if (!next_tp || sap != next_sap)
	{
	  if (!suite_ok_so_far)
	    {
	      /*
	       * XXX We cannot possibly call this a drop... seeing we just turn
	       * down one of the offers, can we?  I suggest renaming
	       * message_drop to something else.
	       */
	      message_drop (msg, ISAKMP_NOTIFY_NO_PROPOSAL_CHOSEN, 0, 0, 0);
	    }
	  sa = TAILQ_NEXT (sa, next);
	}
    }
  return 0;

 cleanup:
  /* Remove potentially succeeded choices from the SA.  */
  while (TAILQ_FIRST (&sa->protos))
    TAILQ_REMOVE (&sa->protos, TAILQ_FIRST (&sa->protos), link);
  return -1;
}

int
message_add_sa_payload (struct message *msg)
{
  struct exchange *exchange = msg->exchange;
  u_int8_t *sa_buf, *saved_nextp_sa, *saved_nextp_prop;
  size_t sa_len, extra_sa_len;
  int i, nprotos = 0;
  struct proto *proto;
  u_int8_t **transforms = 0, **proposals = 0;
  size_t *transform_lens = 0, *proposal_lens = 0;
  struct sa *sa;
  struct doi *doi = exchange->doi;
  u_int8_t *spi = 0;
  size_t spi_sz;

  /*
   * Generate SA payloads.
   */
  for (sa = TAILQ_FIRST (&exchange->sa_list); sa;
       sa = TAILQ_NEXT (sa, next))
    {
      /* Setup a SA payload.  */
      sa_len = ISAKMP_SA_SIT_OFF + doi->situation_size ();
      extra_sa_len = 0;
      sa_buf = malloc (sa_len);
      if (!sa_buf)
	goto cleanup;
      SET_ISAKMP_SA_DOI (sa_buf, doi->id);
      doi->setup_situation (sa_buf);

      /* Count transforms.  */
      nprotos = 0;
      for (proto = TAILQ_FIRST (&sa->protos); proto;
	   proto = TAILQ_NEXT (proto, link))
	nprotos++;

      /* Allocate transient transform and proposal payload/size vectors.  */
      transforms = calloc (nprotos, sizeof *transforms);
      if (!transforms)
        goto cleanup;
      transform_lens = calloc (nprotos, sizeof *transform_lens);
      if (!transform_lens)
        goto cleanup;
      proposals = calloc (nprotos, sizeof *proposals);
      if (!proposals)
        goto cleanup;
      proposal_lens = calloc (nprotos, sizeof *proposal_lens);
      if (!proposal_lens)
        goto cleanup;

      /* Pick out the chosen transforms.  */
      for (proto = TAILQ_FIRST (&sa->protos), i = 0; proto;
	   proto = TAILQ_NEXT (proto, link), i++)
	{
	  transform_lens[i] = GET_ISAKMP_GEN_LENGTH (proto->chosen->p);
	  transforms[i] = malloc (transform_lens[i]);
	  if (!transforms[i])
	    goto cleanup;

	  /* Get incoming SPI from application.  */
	  if (doi->get_spi)
	    {
	      spi = doi->get_spi (&spi_sz,
				  GET_ISAKMP_PROP_PROTO (proto->chosen
							 ->context->p),
				  msg);
	      if (spi_sz && !spi)
		goto cleanup;
	      proto->spi[1] = spi;
	      proto->spi_sz[1] = spi_sz;
	    }
	  else
	    spi_sz = 0;

	  proposal_lens[i] = ISAKMP_PROP_SPI_OFF + spi_sz;
	  proposals[i] = malloc (proposal_lens[i]);
	  if (!proposals[i])
	    goto cleanup;

	  memcpy (transforms[i], proto->chosen->p, transform_lens[i]);
	  memcpy (proposals[i], proto->chosen->context->p,
		  ISAKMP_PROP_SPI_OFF);
	  SET_ISAKMP_PROP_NTRANSFORMS (proposals[i], 1);
	  SET_ISAKMP_PROP_SPI_SZ (proposals[i], spi_sz);
	  if (spi_sz)
	    memcpy (proposals[i] + ISAKMP_PROP_SPI_OFF, spi, spi_sz);
	  extra_sa_len += proposal_lens[i] + transform_lens[i];
	}

      /*
       * Add the payloads.  As this is a SA, we need to recompute the
       * lengths of the payloads containing others.  We also need to
       * reset these payload's "next payload type" field.
       */
      if (message_add_payload (msg, ISAKMP_PAYLOAD_SA, sa_buf, sa_len, 1))
	goto cleanup;
      SET_ISAKMP_GEN_LENGTH (sa_buf, sa_len + extra_sa_len);
      sa_buf = 0;

      saved_nextp_sa = msg->nextp;
      for (proto = TAILQ_FIRST (&sa->protos), i = 0; proto;
	   proto = TAILQ_NEXT (proto, link), i++)
	{
	  if (message_add_payload (msg, ISAKMP_PAYLOAD_PROPOSAL, proposals[i],
				   proposal_lens[i], i > 1))
	    goto cleanup;
	  SET_ISAKMP_GEN_LENGTH (proposals[i],
				 proposal_lens[i] + transform_lens[i]);
	  proposals[i] = 0;

	  saved_nextp_prop = msg->nextp;
	  if (message_add_payload (msg, ISAKMP_PAYLOAD_TRANSFORM,
				   transforms[i], transform_lens[i], 0))
	    goto cleanup;
	  msg->nextp = saved_nextp_prop;
	  transforms[i] = 0;
	}
      msg->nextp = saved_nextp_sa;

      /* Free the temporary allocations made above.  */
      free (transforms);
      free (transform_lens);
      free (proposals);
      free (proposal_lens);
    }
  return 0;

 cleanup:
  if (sa_buf)
    free (sa_buf);
  for (i = 0; i < nprotos; i++)
    {
      if (transforms[i])
	free (transforms[i]);
      if (proposals[i])
	free (proposals[i]);
    }
  if (transforms)
    free (transforms);
  if (transform_lens)
    free (transform_lens);
  if (proposals)
    free (proposals);
  if (proposal_lens)
    free (proposal_lens);
  return -1;
}

/*
 * Return a copy of MSG's constants starting from OFFSET and stash the size
 * in SZP.  It is the callers responsibility to free this up.
 */
u_int8_t *
message_copy (struct message *msg, size_t offset, size_t *szp)
{
  int i, skip = 0;
  size_t sz = 0;
  ssize_t start = -1;
  u_int8_t *buf, *p;

  /* Calculate size of message and where we want to start to copy.  */
  for (i = 1; i < msg->iovlen; i++)
    {
      sz += msg->iov[i].iov_len;
      if (sz <= offset)
	skip = i;
      else if (start < 0)
	start = offset - (sz - msg->iov[i].iov_len);
    }

  /* Allocate and copy.  */
  *szp = sz - offset;
  buf = malloc (*szp);
  if (!buf)
    return 0;
  p = buf;
  for (i = skip + 1; i < msg->iovlen; i++)
    {
      memcpy (p, msg->iov[i].iov_base + start, msg->iov[i].iov_len - start);
      p += msg->iov[i].iov_len - start;
      start = 0;
    }
  return buf;
}

/* Register a post-send function POST_SEND with message MSG.  */
int
message_register_post_send (struct message *msg,
			    void (*post_send) (struct message *))
{
  struct post_send *node;

  node = malloc (sizeof *node);
  if (!node)
    return -1;
  node->func = post_send;
  TAILQ_INSERT_TAIL (&msg->post_send, node, link);
  return 0;
}

/* Run the post-send functions of message MSG.  */
void
message_post_send (struct message *msg)
{
  struct post_send *node;

  while ((node = TAILQ_FIRST (&msg->post_send)) != 0)
    {
      TAILQ_REMOVE (&msg->post_send, node, link);
      node->func (msg);
      free (node);
    }
}

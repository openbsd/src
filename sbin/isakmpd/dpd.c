/*	$OpenBSD: dpd.c,v 1.2 2004/06/20 17:17:34 ho Exp $	*/

/*
 * Copyright (c) 2004 Håkan Olsson.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

#include <sys/types.h>
#include <stdlib.h>

#include "sysdep.h"

#include "dpd.h"
#include "exchange.h"
#include "ipsec.h"
#include "isakmp_fld.h"
#include "log.h"
#include "message.h"
#include "sa.h"
#include "timer.h"
#include "util.h"

/* From RFC 3706.  */
#define DPD_MAJOR		0x01
#define DPD_MINOR		0x00
#define DPD_SEQNO_SZ		4

static const char dpd_vendor_id[] = {
	0xAF, 0xCA, 0xD7, 0x13, 0x68, 0xA1, 0xF1,	/* RFC 3706 */
	0xC9, 0x6B, 0x86, 0x96, 0xFC, 0x77, 0x57,
	DPD_MAJOR,
	DPD_MINOR
};

int16_t script_dpd[] = {
	ISAKMP_PAYLOAD_NOTIFY,	/* Initiator -> responder.  */
	ISAKMP_PAYLOAD_HASH,
	EXCHANGE_SCRIPT_SWITCH,
	ISAKMP_PAYLOAD_NOTIFY,	/* Responder -> initiator.  */
	ISAKMP_PAYLOAD_HASH,
	EXCHANGE_SCRIPT_END
};

static int	dpd_initiator_send_notify(struct message *);
static int	dpd_initiator_recv_ack(struct message *);
static int	dpd_responder_recv_notify(struct message *);
static int	dpd_responder_send_ack(struct message *);
static void	dpd_event(void *);

int (*isakmp_dpd_initiator[])(struct message *) = {
	dpd_initiator_send_notify,
	dpd_initiator_recv_ack
};

int (*isakmp_dpd_responder[])(struct message *) = {
	dpd_responder_recv_notify,
	dpd_responder_send_ack
};

/* Add the DPD VENDOR ID payload.  */
int
dpd_add_vendor_payload(struct message *msg)
{
	u_int8_t *buf;
	size_t buflen = sizeof dpd_vendor_id + ISAKMP_GEN_SZ;

	buf = malloc(buflen);
	if (!buf) {
		log_error("dpd_add_vendor_payload: malloc(%lu) failed",
		    (unsigned long)buflen);
		return -1;
	}

	SET_ISAKMP_GEN_LENGTH(buf, buflen);
	memcpy(buf + ISAKMP_VENDOR_ID_OFF, dpd_vendor_id,
	    sizeof dpd_vendor_id);
	if (message_add_payload(msg, ISAKMP_PAYLOAD_VENDOR, buf, buflen, 1)) {
		free(buf);
		return -1;
	}

	return 0;
}

/*
 * Check an incoming message for DPD capability markers.
 */
void
dpd_check_vendor_payload(struct message *msg, struct payload *p)
{
	u_int8_t *pbuf = p->p;
	size_t vlen;

	/* Already checked? */
	if (msg->exchange->flags & EXCHANGE_FLAG_DPD_CAP_PEER) {
		/* Just mark it as handled and return.  */
		p->flags |= PL_MARK;
		return;
	}

	vlen = GET_ISAKMP_GEN_LENGTH(pbuf) - ISAKMP_GEN_SZ;
	if (vlen != sizeof dpd_vendor_id) {
		LOG_DBG((LOG_EXCHANGE, 90,
		    "dpd_check_vendor_payload: bad size %d != %d", vlen,
		    sizeof dpd_vendor_id));
		return;
	}

	if (memcmp(dpd_vendor_id, pbuf + ISAKMP_GEN_SZ, vlen) == 0) {
		/* This peer is DPD capable.  */
		msg->exchange->flags |= EXCHANGE_FLAG_DPD_CAP_PEER;
		LOG_DBG((LOG_EXCHANGE, 10, "dpd_check_vendor_payload: "
		    "DPD capable peer detected"));
		p->flags |= PL_MARK;
		return;
	}

	return;
}

static int
dpd_add_notify(struct message *msg, u_int16_t type, u_int32_t seqno)
{
	struct sa *isakmp_sa = msg->isakmp_sa;
	char *buf;
	u_int32_t buflen;

	if (!isakmp_sa)	{
		log_print("dpd_add_notify: no isakmp_sa");
		return -1;
	}

	buflen = ISAKMP_NOTIFY_SZ + ISAKMP_HDR_COOKIES_LEN + DPD_SEQNO_SZ;
	buf = malloc(buflen);
	if (!buf) {
		log_error("dpd_add_notify: malloc(%d) failed",
		    ISAKMP_NOTIFY_SZ + DPD_SEQNO_SZ);
		return -1;
	}

	SET_ISAKMP_NOTIFY_DOI(buf, IPSEC_DOI_IPSEC);
	SET_ISAKMP_NOTIFY_PROTO(buf, ISAKMP_PROTO_ISAKMP);
	SET_ISAKMP_NOTIFY_SPI_SZ(buf, ISAKMP_HDR_COOKIES_LEN);
	SET_ISAKMP_NOTIFY_MSG_TYPE(buf, type);
	memcpy(buf + ISAKMP_NOTIFY_SPI_OFF, isakmp_sa->cookies,
	    ISAKMP_HDR_COOKIES_LEN);

	memcpy(buf + ISAKMP_NOTIFY_SPI_OFF + ISAKMP_HDR_COOKIES_LEN, &seqno,
	    sizeof (u_int32_t));

	if (message_add_payload(msg, ISAKMP_PAYLOAD_NOTIFY, buf, buflen, 1)) {
		free(buf);
		return -1;
	}

	return 0;
}

static int
dpd_initiator_send_notify(struct message *msg)
{
	if (!msg->isakmp_sa) {
		log_print("dpd_initiator_send_notify: no isakmp_sa");
		return -1;
	}

	if (msg->isakmp_sa->dpd_seq == 0) {
		/* RFC 3706: first seq# should be random, with MSB zero. */
		getrandom((u_int8_t *)&msg->isakmp_sa->seq,
		    sizeof msg->isakmp_sa->seq);
		msg->isakmp_sa->dpd_seq &= 0x7FFF;
	} else
		msg->isakmp_sa->dpd_seq++;

	return dpd_add_notify(msg, ISAKMP_NOTIFY_STATUS_DPD_R_U_THERE,
	    msg->isakmp_sa->dpd_seq);
}

static int
dpd_initiator_recv_ack(struct message *msg)
{
	struct payload	*p = payload_first(msg, ISAKMP_PAYLOAD_NOTIFY);
	struct sa	*isakmp_sa = msg->isakmp_sa;
	struct timeval	 tv;
	u_int32_t	 rseq;

	if (msg->exchange->phase != 2) {
		message_drop(msg, ISAKMP_NOTIFY_INVALID_EXCHANGE_TYPE, 0, 1,
		    0);
		return -1;
	}

	if (GET_ISAKMP_NOTIFY_MSG_TYPE(p->p)
	    != ISAKMP_NOTIFY_STATUS_DPD_R_U_THERE_ACK) {
		message_drop(msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 1, 0);
		return -1;
	}

	/* Presumably, we've been through message_validate_notify().  */

	/* Validate the SPI. Perhaps move to message_validate_notify().  */
	if (memcmp(p->p + ISAKMP_NOTIFY_SPI_OFF, isakmp_sa->cookies,
	    ISAKMP_HDR_COOKIES_LEN) != 0) {
		log_print("dpd_initiator_recv_ack: bad cookies");
		message_drop(msg, ISAKMP_NOTIFY_INVALID_SPI, 0, 1, 0);
		return -1;
	}

	/* Check the seqno.  */
	memcpy(p->p + ISAKMP_NOTIFY_SPI_OFF + ISAKMP_HDR_COOKIES_LEN, &rseq,
	    sizeof rseq);
	rseq = ntohl(rseq);

	if (isakmp_sa->seq != rseq) {
		log_print("dpd_initiator_recv_ack: bad seqno %u, expected %u",
		    rseq, isakmp_sa->seq);
		message_drop(msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 1, 0);
		return -1;
	}

	/* Peer is alive. Reset timer.  */
	gettimeofday(&tv, 0);
	tv.tv_sec += DPD_DEFAULT_WORRY_METRIC; /* XXX Configurable */

	isakmp_sa->dpd_nextev = timer_add_event("dpd_event", dpd_event,
	    isakmp_sa, &tv);
	if (!isakmp_sa->dpd_nextev) 
		log_print("dpd_initiator_recv_ack: timer_add_event "
		    "failed");
	else
		sa_reference(isakmp_sa);

	/* Mark handled.  */
	p->flags |= PL_MARK;

	return 0;
}

static int
dpd_responder_recv_notify(struct message *msg)
{
	struct payload	*p = payload_first(msg, ISAKMP_PAYLOAD_NOTIFY);
	struct sa	*isakmp_sa = msg->isakmp_sa;
	struct timeval	 tv;
	u_int32_t	 rseq;

	if (msg->exchange->phase != 2) {
		message_drop(msg, ISAKMP_NOTIFY_INVALID_EXCHANGE_TYPE, 0, 1,
		    0);
		return -1;
	}

	if (GET_ISAKMP_NOTIFY_MSG_TYPE(p->p) != 
	    ISAKMP_NOTIFY_STATUS_DPD_R_U_THERE)	{
		message_drop(msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 1, 0);
		return -1;
	}

	/* Presumably, we've gone through message_validate_notify().  */
	/* XXX */

	/* Validate the SPI. Perhaps move to message_validate_notify().  */
	if (memcmp(p->p + ISAKMP_NOTIFY_SPI_OFF, isakmp_sa->cookies,
	    ISAKMP_HDR_COOKIES_LEN) != 0) {
		log_print("dpd_initiator_recv_notify: bad cookies");
		message_drop(msg, ISAKMP_NOTIFY_INVALID_SPI, 0, 1, 0);
		return -1;
	}

	/* Get the seqno.  */
	memcpy(p->p + ISAKMP_NOTIFY_SPI_OFF + ISAKMP_HDR_COOKIES_LEN, &rseq,
	    sizeof rseq);
	rseq = ntohl(rseq);

	/* Check increasing seqno.  */
	if (rseq <= isakmp_sa->dpd_rseq) {
		log_print("dpd_initiator_recv_notify: bad seqno (%u <= %u)",
		    rseq, isakmp_sa->dpd_rseq);
		message_drop(msg, ISAKMP_NOTIFY_PAYLOAD_MALFORMED, 0, 1, 0);
		return -1;
	}
	isakmp_sa->dpd_rseq = rseq;

	/*
	 * Ok, now we know the peer is alive, in case we're wondering.
	 * If so, reset timers, etc... here.
	 */
	if (isakmp_sa->dpd_nextev) {
		timer_remove_event(isakmp_sa->dpd_nextev);
		sa_release(isakmp_sa);

		gettimeofday(&tv, 0);
		tv.tv_sec += DPD_DEFAULT_WORRY_METRIC; /* XXX Configurable */

		isakmp_sa->dpd_nextev = timer_add_event("dpd_event", dpd_event,
		    isakmp_sa, &tv);
		if (!isakmp_sa->dpd_nextev) 
			log_print("dpd_responder_recv_notify: timer_add_event "
			    "failed");
		else
			sa_reference(isakmp_sa);
	}

	/* Mark handled.  */
	p->flags |= PL_MARK;

	return 0;
}

static int
dpd_responder_send_ack(struct message *msg)
{
	if (!msg->isakmp_sa)
		return -1;

	return dpd_add_notify(msg, ISAKMP_NOTIFY_STATUS_DPD_R_U_THERE_ACK,
	    msg->isakmp_sa->dpd_rseq);
}

static void
dpd_event(void *v_sa)
{
	struct sa	*sa = v_sa;

	sa->dpd_nextev = 0;
	sa_release(sa);

	if ((sa->flags & SA_FLAG_DPD) == 0)
		return;

	/* Create a new DPD exchange.  XXX */
}
	

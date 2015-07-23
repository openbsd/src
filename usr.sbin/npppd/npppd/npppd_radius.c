/* $Id: npppd_radius.c,v 1.8 2015/07/23 09:04:06 yasuoka Exp $ */
/*-
 * Copyright (c) 2009 Internet Initiative Japan Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE"AUTHOR" AND CONTRIBUTORS AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 *	RFC 2865 Remote Authentication Dial In User Service (RADIUS)
 *	RFC 2866 RADIUS Accounting
 *	RFC 2868 RADIUS Attributes for Tunnel Protocol Support
 *	RFC 2869 RADIUS Extensions
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/syslog.h>
#include <netinet/in.h>
#include <net/if_dl.h>
#include <stdio.h>
#include <netdb.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <radius.h>

#include <event.h>

#include "radius_req.h"
#include "npppd_local.h"
#include "npppd_radius.h"

#ifdef NPPPD_RADIUS_DEBUG
#define NPPPD_RADIUS_DBG(x) 	ppp_log x
#define NPPPD_RADIUS_ASSERT(x)	ASSERT(x)
#else
#define NPPPD_RADIUS_DBG(x) 
#define NPPPD_RADIUS_ASSERT(x)
#endif

static int l2tp_put_tunnel_attributes(RADIUS_PACKET *, void *);
static int pptp_put_tunnel_attributes(RADIUS_PACKET *, void *);
static int radius_acct_request(npppd *, npppd_ppp *, int );
static void npppd_ppp_radius_acct_reqcb(void *, RADIUS_PACKET *, int, RADIUS_REQUEST_CTX);

/***********************************************************************
 * RADIUS common functions
 ***********************************************************************/
/**
 * Retribute Framed-IP-Address and Framed-IP-Netmask attribute of from 
 * the given RADIUS packet and set them as the fields of ppp context.
 */ 
void
ppp_proccess_radius_framed_ip(npppd_ppp *_this, RADIUS_PACKET *pkt)
{
	struct in_addr ip4;
	
	if (radius_get_ipv4_attr(pkt, RADIUS_TYPE_FRAMED_IP_ADDRESS, &ip4)
	    == 0)
		_this->realm_framed_ip_address = ip4;

	_this->realm_framed_ip_netmask.s_addr = 0xffffffffL;
#ifndef	NPPPD_COMPAT_4_2
	if (radius_get_ipv4_attr(pkt, RADIUS_TYPE_FRAMED_IP_NETMASK, &ip4)
	    == 0)
		_this->realm_framed_ip_netmask = ip4;
#endif
}

/***********************************************************************
 * RADIUS Accounting Events
 ***********************************************************************/

/** Called by PPP on start */
void
npppd_ppp_radius_acct_start(npppd *pppd, npppd_ppp *ppp)
{
	NPPPD_RADIUS_DBG((ppp, LOG_INFO, "%s()", __func__));

	if (ppp->realm == NULL || !npppd_ppp_is_realm_radius(pppd, ppp))
		return;
	radius_acct_request(pppd, ppp, 0);
}

/** Called by PPP on stop*/
void
npppd_ppp_radius_acct_stop(npppd *pppd, npppd_ppp *ppp)
{
	NPPPD_RADIUS_DBG((ppp, LOG_INFO, "%s()", __func__));

	if (ppp->realm == NULL || !npppd_ppp_is_realm_radius(pppd, ppp))
		return;
	radius_acct_request(pppd, ppp, 1);
}

/** Called by radius_req.c */
static void
npppd_ppp_radius_acct_reqcb(void *context, RADIUS_PACKET *pkt, int flags,
    RADIUS_REQUEST_CTX ctx)
{
	u_int ppp_id;

	ppp_id = (uintptr_t)context;
	if ((flags & RADIUS_REQUEST_TIMEOUT) != 0) {
		log_printf(LOG_WARNING, "ppp id=%u radius accounting request "
		    "failed: no response from the server.", ppp_id);
	}
	else if ((flags & RADIUS_REQUEST_ERROR) != 0)
		log_printf(LOG_WARNING, "ppp id=%u radius accounting request "
		    "failed: %m", ppp_id);
	else if ((flags & RADIUS_REQUEST_CHECK_AUTHENTICATOR_NO_CHECK) == 0 &&
	    (flags & RADIUS_REQUEST_CHECK_AUTHENTICATOR_OK) == 0)
		log_printf(LOG_WARNING, "ppp id=%d radius accounting request "
		    "failed: the server responses with bad authenticator",
		    ppp_id);
	else {
#ifdef NPPPD_RADIUS_DEBUG
		log_printf(LOG_DEBUG, "ppp id=%u radius accounting request "
		    "succeeded.", ppp_id);
#endif
		return;
		/* NOTREACHED */
	}
	if (radius_request_can_failover(ctx)) {
		if (radius_request_failover(ctx) == 0) {
			struct sockaddr *sa;
			char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];

			sa = radius_get_server_address(ctx);
			if (getnameinfo(sa, sa->sa_len, hbuf, sizeof(hbuf),
			    sbuf, sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV)
			    != 0) {
				strlcpy(hbuf, "unknown", sizeof(hbuf));
				strlcpy(sbuf, "", sizeof(sbuf));
			}
			log_printf(LOG_DEBUG, "ppp id=%u "
			    "fail over to %s:%s for radius accounting request",
			    ppp_id, hbuf, sbuf);
		} else {
			log_printf(LOG_WARNING, "ppp id=%u "
			    "failed to fail over for radius accounting request",
			    ppp_id);
		}
	}
}

/***********************************************************************
 * RADIUS attributes
 ***********************************************************************/
#define	ATTR_INT32(_a,_v)						\
	do {								\
		if (radius_put_uint32_attr(radpkt, (_a), (_v)) != 0)	\
			goto fail; 					\
	} while (0 /* CONSTCOND */)
#define	ATTR_STR(_a,_v)							\
	do {								\
		if (radius_put_string_attr(radpkt, (_a), (_v)) != 0)	\
		    goto fail; 					\
	} while (0 /* CONSTCOND */)

static int
radius_acct_request(npppd *pppd, npppd_ppp *ppp, int stop)
{
	RADIUS_PACKET *radpkt;
	RADIUS_REQUEST_CTX radctx;
	radius_req_setting *rad_setting;
	char buf[128];

	if (ppp->username[0] == '\0')
		return 0;

	radpkt = NULL;
	radctx = NULL;
	rad_setting = npppd_auth_radius_get_radius_acct_setting(ppp->realm);
	if (!radius_req_setting_has_server(rad_setting))
		return 0;
	if ((radpkt = radius_new_request_packet(RADIUS_CODE_ACCOUNTING_REQUEST))
	    == NULL)
		goto fail;

	if (radius_prepare(rad_setting, (void *)(uintptr_t)ppp->id, &radctx,
	    npppd_ppp_radius_acct_reqcb) != 0)
		goto fail;

    /* NAS Information */
	/*
	 * RFC 2865 "5.4.  NAS-IP-Address" or RFC 3162 "2.1. NAS-IPv6-Address"
	 */
	if (radius_prepare_nas_address(rad_setting, radpkt) != 0)
		goto fail;

	/* RFC 2865 "5.41. NAS-Port-Type" */
	ATTR_INT32(RADIUS_TYPE_NAS_PORT_TYPE, RADIUS_NAS_PORT_TYPE_VIRTUAL);

	/* RFC 2865 "5.5. NAS-Port" */
	ATTR_INT32(RADIUS_TYPE_NAS_PORT, ppp->id);
	    /* npppd has no physical / virtual ports in design. */

	/* RFC 2865 5.31. Calling-Station-Id */
	if (ppp->calling_number[0] != '\0')
		ATTR_STR(RADIUS_TYPE_CALLING_STATION_ID, ppp->calling_number);

    /* Tunnel Protocol Information */
	switch (ppp->tunnel_type) {
	case NPPPD_TUNNEL_L2TP:
		/* RFC 2868 3.1. Tunnel-Type */
		ATTR_INT32(RADIUS_TYPE_TUNNEL_TYPE, RADIUS_TUNNEL_TYPE_L2TP);
		if (l2tp_put_tunnel_attributes(radpkt, ppp->phy_context) != 0)
			goto fail;
		break;
	case NPPPD_TUNNEL_PPTP:
		/* RFC 2868 3.1. Tunnel-Type */
		ATTR_INT32(RADIUS_TYPE_TUNNEL_TYPE, RADIUS_TUNNEL_TYPE_PPTP);
		if (pptp_put_tunnel_attributes(radpkt, ppp->phy_context) != 0)
			goto fail;
		break;
	}

    /* Framed Protocol (PPP) Information */
	/* RFC 2865 5.1 User-Name */
	ATTR_STR(RADIUS_TYPE_USER_NAME, ppp->username);

	/* RFC 2865 "5.7. Service-Type" */
	ATTR_INT32(RADIUS_TYPE_SERVICE_TYPE, RADIUS_SERVICE_TYPE_FRAMED);

	/* RFC 2865 "5.8. Framed-Protocol" */
	ATTR_INT32(RADIUS_TYPE_FRAMED_PROTOCOL, RADIUS_FRAMED_PROTOCOL_PPP);

	/* RFC 2865 "5.8. Framed-IP-Address" */
	if (ppp->acct_framed_ip_address.s_addr != INADDR_ANY)
		ATTR_INT32(RADIUS_TYPE_FRAMED_IP_ADDRESS,
		    ntohl(ppp->acct_framed_ip_address.s_addr));

    /* Accounting */
	/* RFC 2866  5.1. Acct-Status-Type */
	ATTR_INT32(RADIUS_TYPE_ACCT_STATUS_TYPE, (stop)
	    ? RADIUS_ACCT_STATUS_TYPE_STOP : RADIUS_ACCT_STATUS_TYPE_START);

	/* RFC 2866  5.2.  Acct-Delay-Time */
	ATTR_INT32(RADIUS_TYPE_ACCT_DELAY_TIME, 0);

	if (stop) {
		/* RFC 2866  5.3 Acct-Input-Octets */
		ATTR_INT32(RADIUS_TYPE_ACCT_INPUT_OCTETS,
		    (uint32_t)(ppp->ibytes & 0xFFFFFFFFU));	/* LSB 32bit */

		/* RFC 2866  5.4 Acct-Output-Octets */
		ATTR_INT32(RADIUS_TYPE_ACCT_OUTPUT_OCTETS,
		    (uint32_t)(ppp->obytes & 0xFFFFFFFFU));	/* LSB 32bit */
	}

	/* RFC 2866  5.5 Acct-Session-Id */
	snprintf(buf, sizeof(buf), "%08X%08X", pppd->boot_id, ppp->id);
	ATTR_STR(RADIUS_TYPE_ACCT_SESSION_ID, buf);

	/* RFC 2866 5.6.  Acct-Authentic */
	ATTR_INT32(RADIUS_TYPE_ACCT_AUTHENTIC, RADIUS_ACCT_AUTHENTIC_RADIUS);

	if (stop) {
		/* RFC 2866 5.7. Acct-Session-Time */
		ATTR_INT32(RADIUS_TYPE_ACCT_SESSION_TIME,
		    ppp->end_monotime - ppp->start_monotime);

		/* RFC 2866  5.8 Acct-Input-Packets */
		ATTR_INT32(RADIUS_TYPE_ACCT_INPUT_PACKETS, ppp->ipackets);

		/* RFC 2866  5.9 Acct-Output-Packets */
		ATTR_INT32(RADIUS_TYPE_ACCT_OUTPUT_PACKETS, ppp->opackets);

		/* RFC 2866  5.10. Acct-Terminate-Cause */
		if (ppp->terminate_cause != 0)
			ATTR_INT32(RADIUS_TYPE_ACCT_TERMINATE_CAUSE,
			    ppp->terminate_cause);

		/* RFC 2869  5.1 Acct-Input-Gigawords */
		ATTR_INT32(RADIUS_TYPE_ACCT_INPUT_GIGAWORDS, ppp->ibytes >> 32);

		/* RFC 2869  5.2 Acct-Output-Gigawords */
		ATTR_INT32(RADIUS_TYPE_ACCT_OUTPUT_GIGAWORDS,
		    ppp->obytes >> 32);
	}

	radius_set_accounting_request_authenticator(radpkt,
	    radius_get_server_secret(radctx));

	/* Send the request */
	radius_request(radctx, radpkt);

	return 0;

fail:
	ppp_log(ppp, LOG_WARNING, "radius accounting request failed: %m");

	if (radctx != NULL)
		radius_cancel_request(radctx);
	if (radpkt != NULL)
		radius_delete_packet(radpkt);

	return -1;
}

#ifdef USE_NPPPD_PPTP
#include "pptp.h"
#endif

static int
pptp_put_tunnel_attributes(RADIUS_PACKET *radpkt, void *call0)
{
#ifdef USE_NPPPD_PPTP
	pptp_call *call = call0;
	pptp_ctrl *ctrl;
	char hbuf[NI_MAXHOST], buf[128];

	ctrl = call->ctrl;

	/* RFC 2868  3.2.  Tunnel-Medium-Type */
	switch (ctrl->peer.ss_family) {
	case AF_INET:
		ATTR_INT32(RADIUS_TYPE_TUNNEL_MEDIUM_TYPE,
		    RADIUS_TUNNEL_MEDIUM_TYPE_IPV4);
		break;

	case AF_INET6:
		ATTR_INT32(RADIUS_TYPE_TUNNEL_MEDIUM_TYPE,
		    RADIUS_TUNNEL_MEDIUM_TYPE_IPV6);
		break;

	default:
		return -1;
	}

	/* RFC 2868  3.3.  Tunnel-Client-Endpoint */
	if (getnameinfo((struct sockaddr *)&ctrl->peer, ctrl->peer.ss_len, hbuf,
	    sizeof(hbuf), NULL, 0, NI_NUMERICHOST))
		return 1;
	ATTR_STR(RADIUS_TYPE_TUNNEL_CLIENT_ENDPOINT, hbuf);

	/* RFC 2868  3.4.  Tunnel-Server-Endpoint */
	if (getnameinfo((struct sockaddr *)&ctrl->our, ctrl->our.ss_len, hbuf,
	    sizeof(hbuf), NULL, 0, NI_NUMERICHOST))
		return 1;
	ATTR_STR(RADIUS_TYPE_TUNNEL_SERVER_ENDPOINT, hbuf);

	/* RFC 2868  3.7.  Tunnel-Assignment-ID */
	snprintf(buf, sizeof(buf), "PPTP-CALL-%d", call->id);
	ATTR_STR(RADIUS_TYPE_TUNNEL_ASSIGNMENT_ID, buf);

	/* RFC 2867  4.1. Acct-Tunnel-Connection   */
	snprintf(buf, sizeof(buf), "PPTP-CTRL-%d", ctrl->id);
	ATTR_STR(RADIUS_TYPE_ACCT_TUNNEL_CONNECTION, buf);

	return 0;
fail:
#endif
	return 1;
}

#ifdef USE_NPPPD_L2TP
#include "l2tp.h"
#endif

static int
l2tp_put_tunnel_attributes(RADIUS_PACKET *radpkt, void *call0)
{
#ifdef USE_NPPPD_L2TP
	l2tp_call *call = call0;
	l2tp_ctrl *ctrl;
	char hbuf[NI_MAXHOST], buf[128];

	ctrl = call->ctrl;

	/* RFC 2868  3.2.  Tunnel-Medium-Type */
	switch (ctrl->peer.ss_family) {
	case AF_INET:
		ATTR_INT32(RADIUS_TYPE_TUNNEL_MEDIUM_TYPE,
		    RADIUS_TUNNEL_MEDIUM_TYPE_IPV4);
		break;

	case AF_INET6:
		ATTR_INT32(RADIUS_TYPE_TUNNEL_MEDIUM_TYPE,
		    RADIUS_TUNNEL_MEDIUM_TYPE_IPV6);
		break;

	default:
		return -1;
	}

	/* RFC 2868  3.3.  Tunnel-Client-Endpoint */
	if (getnameinfo((struct sockaddr *)&ctrl->peer, ctrl->peer.ss_len, hbuf,
	    sizeof(hbuf), NULL, 0, NI_NUMERICHOST))
		return 1;
	ATTR_STR(RADIUS_TYPE_TUNNEL_CLIENT_ENDPOINT, hbuf);

	/* RFC 2868  3.4.  Tunnel-Server-Endpoint */
	if (getnameinfo((struct sockaddr *)&ctrl->sock, ctrl->sock.ss_len, hbuf,
	    sizeof(hbuf), NULL, 0, NI_NUMERICHOST))
		return 1;
	ATTR_STR(RADIUS_TYPE_TUNNEL_SERVER_ENDPOINT, hbuf);

	/* RFC 2868  3.7.  Tunnel-Assignment-ID */
	snprintf(buf, sizeof(buf), "L2TP-CALL-%d", call->id);
	ATTR_STR(RADIUS_TYPE_TUNNEL_ASSIGNMENT_ID, buf);

	/* RFC 2867  4.1. Acct-Tunnel-Connection   */
	snprintf(buf, sizeof(buf), "L2TP-CTRL-%d", ctrl->id);
	ATTR_STR(RADIUS_TYPE_ACCT_TUNNEL_CONNECTION, buf);

	return 0;
fail:
#endif
	return 1;
}

/*	$OpenBSD: auth.c,v 1.1 2005/01/28 14:05:40 claudio Exp $ */

/*
 * Copyright (c) 2004, 2005 Esben Norby <esben.norby@ericsson.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>

#include "ospfd.h"
#include "ospf.h"
#include "log.h"
#include "ospfe.h"

int
auth_validate(struct ospf_hdr *pkt, const struct iface *iface)
{
	if (ntohs(pkt->auth_type) != (u_int16_t)iface->auth_type) {
		log_debug("auth_validate: wrong auth type, interface %s",
		    iface->name);
		return (-1);
	}

	switch (iface->auth_type) {
	case AUTH_NONE:
		break;
	case AUTH_SIMPLE:
		if (bcmp(pkt->auth_key.simple, iface->auth_key,
		    sizeof(pkt->auth_key.simple))) {
			log_debug("auth_validate: wrong password, interface %s",
			    iface->name);
			return (-1);
		}
		/* clear the key before chksum */
		bzero(pkt->auth_key.simple,
		     sizeof(pkt->auth_key.simple));
		break;
	case AUTH_CRYPT:
		log_debug("auth_validate: not supported, interface %s",
		    iface->name);
		return (-1);
	default:
		log_debug("auth_validate: unknown auth type, interface %s",
		    iface->name);
		return (-1);
	}

	if (in_cksum(pkt, ntohs(pkt->len))) {
		log_debug("recv_packet: invalid checksum, interface %s",
		    iface->name);
		return (-1);
	}

	return (0);
}

int
auth_gen(void *buf, u_int16_t len, const struct iface *iface)
{
	struct ospf_hdr	*ospf_hdr = buf;

	/* update length, and checksum */
	ospf_hdr->len = htons(len);
	ospf_hdr->chksum = in_cksum(buf, len);

	switch (iface->auth_type) {
	case AUTH_NONE:
		break;
	case AUTH_SIMPLE:
		strncpy(ospf_hdr->auth_key.simple, iface->auth_key,
		    sizeof(ospf_hdr->auth_key.simple));
		break;
	case AUTH_CRYPT:
		log_debug("auth_gen: not supported, interface %s",
		    iface->name);
		return (-1);
	default:
		log_debug("auth_gen: unknown auth type, interface %s",
		    iface->name);
		return (-1);
	}

	return (0);
}

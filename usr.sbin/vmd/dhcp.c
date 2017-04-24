/*	$OpenBSD: dhcp.c,v 1.3 2017/04/24 07:14:27 reyk Exp $	*/

/*
 * Copyright (c) 2017 Reyk Floeter <reyk@openbsd.org>
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

#include <net/if.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "proc.h"
#include "vmd.h"
#include "dhcp.h"
#include "virtio.h"

static const uint8_t broadcast[6] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
extern struct vmd *env;

ssize_t
dhcp_request(struct vionet_dev *dev, char *buf, size_t buflen, char **obuf)
{
	unsigned char		*respbuf = NULL;
	ssize_t			 offset, respbuflen = 0;
	struct packet_ctx	 pc;
	struct dhcp_packet	 req, resp;
	struct in_addr		 in, mask;
	size_t			 resplen, o;

	if (buflen < (ssize_t)(BOOTP_MIN_LEN + sizeof(struct ether_header)))
		return (-1);

	memset(&pc, 0, sizeof(pc));
	if ((offset = decode_hw_header(buf, buflen, 0, &pc, HTYPE_ETHER)) < 0)
		return (-1);

	if (memcmp(pc.pc_smac, dev->mac, ETHER_ADDR_LEN) != 0 ||
	    memcmp(pc.pc_dmac, broadcast, ETHER_ADDR_LEN) != 0)
		return (-1);

	if ((offset = decode_udp_ip_header(buf, buflen, offset, &pc)) < 0)
		return (-1);

	if (ntohs(ss2sin(&pc.pc_src)->sin_port) != CLIENT_PORT ||
	    ntohs(ss2sin(&pc.pc_dst)->sin_port) != SERVER_PORT)
		return (-1);

	memset(&req, 0, sizeof(req));
	memcpy(&req, buf + offset, buflen - offset);

	if (req.op != BOOTREQUEST ||
	    req.htype != pc.pc_htype ||
	    req.hlen != ETHER_ADDR_LEN ||
	    memcmp(dev->mac, req.chaddr, req.hlen) != 0)
		return (-1);

	/* Ignore unsupported requests for now */
	if (req.ciaddr.s_addr != 0 || req.file[0] != '\0' || req.hops != 0)
		return (-1);

	memset(&resp, 0, sizeof(resp));
	resp.op = BOOTREPLY;
	resp.htype = req.htype;
	resp.hlen = req.hlen;
	resp.xid = req.xid;

	if ((in.s_addr = vm_priv_addr(&env->vmd_cfg.cfg_localprefix,
	    dev->vm_vmid, dev->idx, 1)) == 0)
		return (-1);
	memcpy(&resp.yiaddr, &in, sizeof(in));
	memcpy(&ss2sin(&pc.pc_dst)->sin_addr, &in, sizeof(in));
	ss2sin(&pc.pc_dst)->sin_port = htons(CLIENT_PORT);

	if ((in.s_addr = vm_priv_addr(&env->vmd_cfg.cfg_localprefix,
	    dev->vm_vmid, dev->idx, 0)) == 0)
		return (-1);
	memcpy(&resp.siaddr, &in, sizeof(in));
	memcpy(&ss2sin(&pc.pc_src)->sin_addr, &in, sizeof(in));
	ss2sin(&pc.pc_src)->sin_port = htons(SERVER_PORT);

	/* Packet is already allocated */
	if (*obuf != NULL)
		goto fail;

	buflen = 0;
	respbuflen = DHCP_MTU_MAX;
	if ((respbuf = calloc(1, respbuflen)) == NULL)
		goto fail;

	memcpy(&pc.pc_dmac, dev->mac, sizeof(pc.pc_dmac));
	memcpy(&resp.chaddr, dev->mac, resp.hlen);
	memcpy(&pc.pc_smac, dev->mac, sizeof(pc.pc_smac));
	pc.pc_smac[5]++;
	if ((offset = assemble_hw_header(respbuf, respbuflen, 0,
	    &pc, HTYPE_ETHER)) < 0) {
		log_debug("%s: assemble_hw_header failed", __func__);
		goto fail;
	}

	/* BOOTP uses a 64byte vendor field instead of the DHCP options */
	resplen = BOOTP_MIN_LEN;

	/* Add BOOTP Vendor Extensions (DHCP options) */
	o = 0;
	memcpy(&resp.options,
	    DHCP_OPTIONS_COOKIE, DHCP_OPTIONS_COOKIE_LEN);
	o+= DHCP_OPTIONS_COOKIE_LEN;

	resp.options[o++] = DHO_SUBNET_MASK;
	resp.options[o++] = sizeof(mask);
	mask.s_addr = htonl(0xfffffffe);
	memcpy(&resp.options[o], &mask, sizeof(mask));
	o += sizeof(mask);

	resp.options[o++] = DHO_ROUTERS;
	resp.options[o++] = sizeof(in);
	memcpy(&resp.options[o], &in, sizeof(in));
	o += sizeof(in);

	resp.options[o++] = DHO_DOMAIN_NAME_SERVERS;
	resp.options[o++] = sizeof(in);
	memcpy(&resp.options[o], &in, sizeof(in));
	o += sizeof(in);

	resp.options[o++] = DHO_END;

	resplen = offsetof(struct dhcp_packet, options) + o;

	/* Minimum packet size */
	if (resplen < BOOTP_MIN_LEN)
		resplen = BOOTP_MIN_LEN;

	if ((offset = assemble_udp_ip_header(respbuf, respbuflen, offset, &pc,
	    (unsigned char *)&resp, resplen)) < 0) {
		log_debug("%s: assemble_udp_ip_header failed", __func__);
		goto fail;
	}

	memcpy(respbuf + offset, &resp, resplen);
	respbuflen = offset + resplen;

	*obuf = respbuf;
	return (respbuflen);
 fail:
	free(respbuf);
	return (0);
}


/*	$OpenBSD: if_vxlan.h,v 1.6 2014/12/19 17:14:40 tedu Exp $	*/

/*
 * Copyright (c) 2013 Reyk Floeter <reyk@openbsd.org>
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

#ifndef _NET_VXLAN_H
#define _NET_VXLAN_H

#define VXLANMTU		1492
#define VXLAN_HDRLEN		8
#define VXLAN_PORT		4789

struct vxlan_header {
	u_int32_t		vxlan_flags;
#define	VXLAN_FLAGS_VNI		0x08000000
#define	VXLAN_RESERVED1		0xf7ffffff
	u_int32_t		vxlan_id;
#define VXLAN_VNI		0xffffff00
#define VXLAN_VNI_S		8
#define VXLAN_RESERVED2		0x000000ff
} __packed;

struct vxlanudpiphdr {
	struct ipovly		ui_i;
	struct udphdr		ui_u;
	struct vxlan_header	ui_v;
} __packed;

#ifdef _KERNEL
struct vxlan_softc {
	struct arpcom		 sc_ac;
	struct ifmedia		 sc_media;

	struct ip_moptions	 sc_imo;
	void			*sc_ahcookie;
	void			*sc_lhcookie;
	void			*sc_dhcookie;

	struct sockaddr_storage	 sc_src;
	struct sockaddr_storage	 sc_dst;
	in_port_t		 sc_dstport;
	u_int			 sc_rdomain;
	u_int32_t		 sc_vnetid;
	u_int8_t		 sc_ttl;

	LIST_ENTRY(vxlan_softc)	 sc_entry;
};

extern int vxlan_enable;

int		 vxlan_lookup(struct mbuf *, struct udphdr *, int,
		    struct sockaddr *);
struct sockaddr *vxlan_tag_find(struct mbuf *);
struct sockaddr	*vxlan_tag_get(struct mbuf *, int);
void		 vxlan_tag_delete(struct mbuf *);

#endif /* _KERNEL */

#endif /* _NET_VXLAN_H */

/*	$OpenBSD: if_vxlan.h,v 1.11 2016/09/03 13:46:57 reyk Exp $	*/

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

#define VXLAN_VNI_MAX		0x00ffffff	/* 24bit vnetid */
#define VXLAN_VNI_MIN		0x00000000	/* 24bit vnetid */
#define VXLAN_VNI_UNSET		0x01ffffff	/* used internally */
#define VXLAN_VNI_ANY		-1ULL		/* -1 accept any vnetid */

struct vxlanudphdr {
	struct udphdr		vu_u;
	struct vxlan_header	vu_v;
} __packed;

#ifdef _KERNEL
extern int vxlan_enable;

int		 vxlan_lookup(struct mbuf *, struct udphdr *, int,
		    struct sockaddr *, struct sockaddr *);
struct sockaddr *vxlan_tag_find(struct mbuf *);
struct sockaddr	*vxlan_tag_get(struct mbuf *, int);
void		 vxlan_tag_delete(struct mbuf *);

#endif /* _KERNEL */

#endif /* _NET_VXLAN_H */

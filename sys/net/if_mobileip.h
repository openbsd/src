/*      $OpenBSD: if_mobileip.h,v 1.1 2018/02/07 01:09:57 dlg Exp $ */

/*
 * Copyright (c) 2016 David Gwynne <dlg@openbsd.org>
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

#ifndef _NET_IF_MOBILEIP_H
#define _NET_IF_MOBILEIP_H

struct mobileip_header {
	uint8_t			mip_proto;	/* original protocol */
	uint8_t			mip_flags;
#define MOBILEIP_SP			0x80	/* src address is present */
	uint16_t		mip_hcrc;	/* header checksum */
	uint32_t		mip_dst;	/* original dst address */
} __packed __aligned(4);

struct mobileip_h_src {
	uint32_t		mip_src;	/* original src address */
} __packed __aligned(4);

#ifdef _KERNEL
void		 mobileipattach(int);
int		 mobileip_input(struct mbuf **, int *, int, int);
int		 mobileip_sysctl(int *, u_int, void *, size_t *,
		     void *, size_t);
#endif /* _KERNEL */

#endif /* _NET_IF_MOBILEIP_H_ */

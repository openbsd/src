/*	$OpenBSD: ikedmode.h,v 1.1 2026/11/09 12:31:36 hshoexer Exp $	*/

/*
 * Copyright (c) 2026 Hans-Joerg Hoexer <hshoexer@yerbouti.franken.de>
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

#ifndef _IKEDMODE_H_
#define _IKEDMODE_H_

#include <sys/types.h>
#include <sys/uio.h>

struct ikev1_datagram;

/* Set when isakmpd runs as the ikev1 process of iked(8).  */
extern int	iked_mode;

int		ikedmode_main(int, char *[]);
int		ikedmode_compose(int, uint16_t, uint32_t, int, const void *,
		    uint16_t);
int		ikedmode_composev(int, uint16_t, const struct iovec *, int);
void		ikedmode_show_sa(uint32_t);

/* transport_imsg.c */
void		transport_imsg_init(void);
void		transport_imsg_recv(struct ikev1_datagram *, u_int8_t *,
		    size_t);
void		transport_imsg_flush(void);

#endif				/* _IKEDMODE_H_ */

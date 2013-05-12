/*	$OpenBSD: config.h,v 1.4 2013/05/12 10:43:45 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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

/* MVME187/MVME197 on-board */

struct ie_configuration {
	int	clun;
	u_int   phys_addr;
	u_char	eaddr[6];
};

extern struct ie_configuration ie_config[];
extern int nie_config;

#define	INTEL_REG_ADDR	0xfff46000

/* MVME376 */

struct le_configuration {
	int	clun;
	u_int	phys_addr;	/* registers */
	u_int	buf_addr;	/* buffers if off-memory */
	u_int	buf_size;	/* buffer memory size */
	u_char	eaddr[6];
};

extern struct le_configuration le_config[];
extern int nle_config;

extern int probe_ethernet(void);
extern void display_ethernet(void);

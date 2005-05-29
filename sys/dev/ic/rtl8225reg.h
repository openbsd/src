/*	$OpenBSD: rtl8225reg.h,v 1.1 2005/05/29 02:54:51 reyk Exp $	*/

/*
 * Copyright (c) 2005 Reyk Floeter <reyk@vantronix.net>
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

#ifndef _DEV_IC_RTL8225REG_H_
#define	_DEV_IC_RTL8225REG_H_

/*
 * Serial bus format for the Realtek RTL8225 Single-chip Transceiver.
 */
#define RTL8225_TWI_DATA_MASK	BITS(31, 4)
#define RTL8225_TWI_ADDR_MASK	BITS(4, 0)

#endif /* _DEV_IC_RTL8225REG_H_ */

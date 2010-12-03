/*	$OpenBSD: mkbcreg.h,v 1.3 2010/12/03 18:29:56 shadchin Exp $  */

/*
 * Copyright (c) 2006, 2007, Joel Sing
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

#include <dev/ic/pckbcvar.h>

/*
 * MACE PS/2 controller register definitions.
 */

#define MKBC_PORTSIZE			0x20

#define MKBC_TX_PORT			0x00
#define MKBC_RX_PORT			0x08
#define MKBC_CONTROL			0x10
#define MKBC_STATUS			0x18

/*
 * Controller status flags
 */
#define MKBC_STATUS_CLOCK_SIGNAL  	0x01
#define MKBC_STATUS_CLOCK_INHIBIT	0x02
#define MKBC_STATUS_TX_INPROGRESS	0x04
#define MKBC_STATUS_TX_EMPTY		0x08
#define MKBC_STATUS_RX_FULL		0x10
#define MKBC_STATUS_RX_INPROGRESS	0x20
#define MKBC_STATUS_ERROR_PARITY	0x40
#define MKBC_STATUS_ERROR_FRAMING	0x80

/*
 * Control bits
 */
#define MKBC_CONTROL_TX_CLOCK_DISABLE	0x01
#define MKBC_CONTROL_TX_ENABLE		0x02
#define MKBC_CONTROL_TX_INT_ENABLE	0x04
#define MKBC_CONTROL_RX_INT_ENABLE	0x08
#define MKBC_CONTROL_RX_CLOCK_ENABLE	0x10
#define MKBC_CONTROL_RESET		0x20

int	mkbc_cnattach(bus_space_tag_t, bus_addr_t);

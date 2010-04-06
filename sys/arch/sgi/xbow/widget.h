/*	$OpenBSD: widget.h,v 1.2 2010/04/06 19:02:57 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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

#ifndef	_WIDGET_H_
#define	_WIDGET_H_

/*
 * Common Widget Registers.  Every widget provides them.
 *
 * Registers are 32 or 64 bit wide (depending on the particular widget
 * or register) on 64 bit boundaries.
 * The widget_{read,write}_[48] functions below hide the addressing
 * games required to perform 32 bit accesses.
 */

#define	WIDGET_ID			0x0000
#define	WIDGET_ID_REV_MASK			0xf0000000
#define	WIDGET_ID_REV_SHIFT			28
#define	WIDGET_ID_REV(wid) \
	(((wid) & WIDGET_ID_REV_MASK) >> WIDGET_ID_REV_SHIFT)
#define	WIDGET_ID_PRODUCT_MASK			0x0ffff000
#define	WIDGET_ID_PRODUCT_SHIFT			12
#define	WIDGET_ID_PRODUCT(wid) \
	(((wid) & WIDGET_ID_PRODUCT_MASK) >> WIDGET_ID_PRODUCT_SHIFT)
#define	WIDGET_ID_VENDOR_MASK			0x00000ffe
#define	WIDGET_ID_VENDOR_SHIFT			1
#define	WIDGET_ID_VENDOR(wid) \
	(((wid) & WIDGET_ID_VENDOR_MASK) >> WIDGET_ID_VENDOR_SHIFT)
#define	WIDGET_STATUS			0x0008
#define	WIDGET_ERR_ADDR_UPPER		0x0010
#define	WIDGET_ERR_ADDR_LOWER		0x0018
#define	WIDGET_CONTROL			0x0020
#define	WIDGET_REQ_TIMEOUT		0x0028
#define	WIDGET_INTDEST_ADDR_UPPER	0x0030
#define	WIDGET_INTDEST_ADDR_LOWER	0x0038
#define	WIDGET_ERR_CMD_WORD		0x0040
#define	WIDGET_LLP_CFG			0x0048
#define	WIDGET_TFLUSH			0x0050

/*
 * Crossbow Specific Registers.
 */

#define	XBOW_WID_ARB_RELOAD		0x0058
#define	XBOW_PERFCNTR_A			0x0060
#define	XBOW_PERFCNTR_B			0x0068
#define	XBOW_NIC			0x0070
#define	XBOW_WIDGET_LINK(w)		(0x0100 + ((w) & 7) * 0x0040)

/*
 * Crossbow Per-widget ``Link'' Register Set.
 */
#define	WIDGET_LINK_IBF			0x0000
#define	WIDGET_LINK_CONTROL		0x0008
#define	WIDGET_CONTROL_ALIVE			0x80000000
#define	WIDGET_LINK_STATUS		0x0010
#define	WIDGET_STATUS_ALIVE			0x80000000
#define	WIDGET_LINK_ARB_UPPER		0x0018
#define	WIDGET_LINK_ARB_LOWER		0x0020
#define	WIDGET_LINK_STATUS_CLEAR	0x0028
#define	WIDGET_LINK_RESET		0x0030
#define	WIDGET_LINK_AUX_STATUS		0x0038

#endif	/* _WIDGET_H_ */

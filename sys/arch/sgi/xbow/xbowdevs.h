/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *	OpenBSD: xbowdevs,v 1.6 2009/10/15 23:42:43 miod Exp 
 */
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

#define	XBOW_VENDOR_SGI	0x0000
#define	XBOW_VENDOR_SGI2	0x0023
#define	XBOW_VENDOR_SGI3	0x0024
#define	XBOW_VENDOR_SGI4	0x0036
#define	XBOW_VENDOR_SGI5	0x02aa

/*
 * List of known products.  Grouped by type.
 */

/*
 * Crossbows
 */

#define	XBOW_PRODUCT_SGI_XBOW	0x0000		/* XBow */
#define	XBOW_PRODUCT_SGI_XXBOW	0xd000		/* XXBow */
#define	XBOW_PRODUCT_SGI_PXBOW	0xd100		/* PXBow */

/*
 * Frame buffers and graphics related devices
 */

#define	XBOW_PRODUCT_SGI5_IMPACT	0xc003		/* ImpactSR */
#define	XBOW_PRODUCT_SGI2_ODYSSEY	0xc013		/* Odyssey */
#define	XBOW_PRODUCT_SGI5_KONA	0xc102		/* Kona */
#define	XBOW_PRODUCT_SGI3_TPU	0xc202		/* TPU */

/*
 * Non-XIO bus bridges
 */

#define	XBOW_PRODUCT_SGI4_BRIDGE	0xc002		/* Bridge */
#define	XBOW_PRODUCT_SGI3_XBRIDGE	0xd002		/* XBridge */
/*
 * PIC is really a single chip but with two widgets headers, and 4 PCI-X
 * slots per widget.
 * The second widget register set uses 0xd112 as the product id.
 */
#define	XBOW_PRODUCT_SGI3_PIC	0xd102		/* PIC */
/*
 * TIO apparently is a next-generation XIO framework; TIO:CP being a TIO
 * variant of PIC with two PCI-X buses, and TIO:CA an AGP bridge.
 * Unlike PIC, the two heads of TIO:CP would appear as two distinct TIO
 * widgets.
 * TIO widgets are supposedly only found on SN2 systems onwards (i.e.
 * ia64-based Altix systems), but in case there is a way to connect TIO
 * nodes to XIO nodes, better identify them properly.
 */
#define	XBOW_PRODUCT_SGI3_TIOCP0	0xe000		/* TIO:CP */
#define	XBOW_PRODUCT_SGI3_TIOCP1	0xe010		/* TIO:CP */
#define	XBOW_PRODUCT_SGI3_TIOCA	0xe020		/* TIO:CA */

/*
 * Octane HEART memory and interrupt controller
 */
#define	XBOW_PRODUCT_SGI4_HEART	0xc001		/* Heart */

/*
 * Miscellaneous widgets
 */

#define	XBOW_PRODUCT_SGI4_HUB	0xc101		/* Hub */
#define	XBOW_PRODUCT_SGI4_BEDROCK	0xc110		/* Bedrock */

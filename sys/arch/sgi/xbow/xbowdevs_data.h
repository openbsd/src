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


/* Descriptions of known devices. */
struct xbow_product {
	uint32_t vendor;
	uint32_t product;
	const char *productname;
};

static const struct xbow_product xbow_products[] = {
	{
	    XBOW_VENDOR_SGI, XBOW_PRODUCT_SGI_XBOW,
	    "XBow",
	},
	{
	    XBOW_VENDOR_SGI, XBOW_PRODUCT_SGI_XXBOW,
	    "XXBow",
	},
	{
	    XBOW_VENDOR_SGI, XBOW_PRODUCT_SGI_PXBOW,
	    "PXBow",
	},
	{
	    XBOW_VENDOR_SGI5, XBOW_PRODUCT_SGI5_IMPACT,
	    "ImpactSR",
	},
	{
	    XBOW_VENDOR_SGI2, XBOW_PRODUCT_SGI2_ODYSSEY,
	    "Odyssey",
	},
	{
	    XBOW_VENDOR_SGI5, XBOW_PRODUCT_SGI5_KONA,
	    "Kona",
	},
	{
	    XBOW_VENDOR_SGI3, XBOW_PRODUCT_SGI3_TPU,
	    "TPU",
	},
	{
	    XBOW_VENDOR_SGI4, XBOW_PRODUCT_SGI4_BRIDGE,
	    "Bridge",
	},
	{
	    XBOW_VENDOR_SGI3, XBOW_PRODUCT_SGI3_XBRIDGE,
	    "XBridge",
	},
	{
	    XBOW_VENDOR_SGI3, XBOW_PRODUCT_SGI3_PIC,
	    "PIC",
	},
	{
	    XBOW_VENDOR_SGI3, XBOW_PRODUCT_SGI3_TIOCP0,
	    "TIO:CP",
	},
	{
	    XBOW_VENDOR_SGI3, XBOW_PRODUCT_SGI3_TIOCP1,
	    "TIO:CP",
	},
	{
	    XBOW_VENDOR_SGI3, XBOW_PRODUCT_SGI3_TIOCA,
	    "TIO:CA",
	},
	{
	    XBOW_VENDOR_SGI4, XBOW_PRODUCT_SGI4_HEART,
	    "Heart",
	},
	{
	    XBOW_VENDOR_SGI4, XBOW_PRODUCT_SGI4_HUB,
	    "Hub",
	},
	{
	    XBOW_VENDOR_SGI4, XBOW_PRODUCT_SGI4_BEDROCK,
	    "Bedrock",
	},
	{ 0, 0, NULL, }
};


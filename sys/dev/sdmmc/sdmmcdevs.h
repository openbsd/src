/*	$OpenBSD: sdmmcdevs.h,v 1.2 2006/06/01 21:45:09 uwe Exp $	*/

/*
 * THIS FILE AUTOMATICALLY GENERATED.  DO NOT EDIT.
 *
 * generated from:
 *		OpenBSD: sdmmcdevs,v 1.2 2006/06/01 21:44:55 uwe Exp 
 */

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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

/*
 * List of known SD card vendors
 */

#define	SDMMC_VENDOR_SYCHIP	0x02db	/* SyChip Inc. */
#define	SDMMC_VENDOR_SPECTEC	0x02fe	/* Spectec Computer Co., Ltd */

/*
 * List of known products, grouped by vendor
 */

/* SyChip Inc. */
#define	SDMMC_CID_SYCHIP_WLAN6060SD	{ NULL, NULL, NULL, NULL }
#define	SDMMC_PRODUCT_SYCHIP_WLAN6060SD	0x544d

/* Spectec Computer Co., Ltd */
#define	SDMMC_CID_SPECTEC_SDW820	{ NULL, NULL, NULL, NULL }
#define	SDMMC_PRODUCT_SPECTEC_SDW820	0x2128

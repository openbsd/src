/* $OpenBSD: wbsioreg.h,v 1.4 2015/01/02 23:02:54 chris Exp $ */
/*
 * Copyright (c) 2008 Mark Kettenis <kettenis@openbsd.org>
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
 * Winbond LPC Super I/O driver registers
 */

/* ISA bus registers */
#define WBSIO_INDEX		0x00	/* Configuration Index Register */
#define WBSIO_DATA		0x01	/* Configuration Data Register */

#define WBSIO_IOSIZE		0x02	/* ISA I/O space size */

#define WBSIO_CONF_EN_MAGIC	0x87	/* enable configuration mode */
#define WBSIO_CONF_DS_MAGIC	0xaa	/* disable configuration mode */

/* Configuration Space Registers */
#define WBSIO_LDN		0x07	/* Logical Device Number */
#define WBSIO_ID		0x20	/* Device ID */
#define WBSIO_REV		0x21	/* Device Revision */

#define WBSIO_ID_W83627HF	0x52
#define WBSIO_ID_W83627THF	0x82
#define WBSIO_ID_W83627EHF	0x88
#define WBSIO_ID_W83627DHG	0xa0
#define WBSIO_ID_W83627DHGP	0xb0
#define WBSIO_ID_W83627UHG	0xa2
#define WBSIO_ID_W83627SF	0x59
#define WBSIO_ID_W83637HF	0x70
#define WBSIO_ID_W83697HF	0x60
#define WBSIO_ID_NCT6776F	0xc3
#define WBSIO_ID_NCT5104D	0xc4

/* Logical Device Number (LDN) Assignments */
#define WBSIO_LDN_HM		0x0b

/* Hardware Monitor Control Registers (LDN B) */
#define WBSIO_HM_ADDR_MSB	0x60	/* Address [15:8] */
#define WBSIO_HM_ADDR_LSB	0x61	/* Address [7:0] */

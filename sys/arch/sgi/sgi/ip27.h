/*	$OpenBSD: ip27.h,v 1.4 2010/05/09 18:37:47 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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
 * Non-XBow related IP27 and IP35 definitions
 */

/* NMI register save areas */

#define	IP27_NMI_KREGS_BASE		0x11400
#define	IP27_NMI_KREGS_SIZE		0x200	/* per CPU */
#define	IP27_NMI_EFRAME_BASE		0x11800

#define	IP35_NMI_KREGS_BASE		0x9000
#define	IP35_NMI_KREGS_SIZE		0x400	/* per CPU */
#define	IP35_NMI_EFRAME_BASE		0xa000

/* IP27 system types */

#define	IP27_SN0	0x00
#define	IP27_SN00	0x01
#define	IP27_O200	IP27_SN00
#define	IP27_O2K	IP27_SN0
#define	IP27_UNKNOWN	0x02

/* IP35 Brick types */

#define	IP35_CBRICK	0x00
#define	IP35_O350	0x02
#define	IP35_FUEL	0x04
#define	IP35_O300	0x08

/*
 * Specific device assignment
 */

#define	IP27_O200_BRIDGE_WIDGET		8
#define	IP27_O2K_BRIDGE_WIDGET		15

#define	IP27_IOC_SLOTNO			2
#define	IP27_IOC2_SLOTNO		6

/*
 * IP27 configuration structure.  Used to tell Origin 200 and Origin 2000
 * apart.
 */

#define	IP27_CONFIG_OFFSET	0x60	/* relative to LBOOTBASE */
#define	IP27_CONFIG_MAGIC	0x69703237636f6e66LL	/* "ip27conf" */

struct ip27_config {
	volatile uint32_t	time_const;
	volatile uint32_t	r10k_sysad;
	volatile uint64_t	magic;
	volatile uint64_t	cpu_hz;
	volatile uint64_t	hub_hz;
	volatile uint64_t	rtc_hz;
	volatile uint32_t	ecc_enable;
	volatile uint32_t	fprom_cyc;
	volatile uint32_t	ip27_subtype;
	volatile uint32_t	cksum;
	volatile uint32_t	flash_count;
	volatile uint32_t	fprom_wr;
	volatile uint32_t	prom_ver;
	volatile uint32_t	prom_rev;
	volatile uint32_t	config_specific;
};

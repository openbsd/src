/*	$OpenBSD: ip30.h,v 1.8 2010/01/19 19:54:24 miod Exp $	*/

/*
 * Copyright (c) 2008, 2009 Miodrag Vallat.
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
 * Physical memory on Octane starts at 512MB.
 *
 * This allows the small windows of all widgets to appear under physical
 * memory, and the Bridge window (#f) to sport the machine PROM at the
 * physical address where the CPU expects it on reset.
 */

#define	IP30_MEMORY_BASE		0x20000000
#define	IP30_MEMORY_ARCBIOS_LIMIT	0x40000000

/*
 * Specific widget assignment
 */

#define	IP30_HEART_WIDGET		8
#define	IP30_BRIDGE_WIDGET		15

/*
 * On-board IOC3 specific GPIO registers wiring
 */

/* Light bar control: 0 to dim, 1 to lit */
#define	IP30_GPIO_WHITE_LED		0	/* actually lightbulbs */
#define	IP30_GPIO_RED_LED		1
/* Classic Octane (1) vs Octane 2 (0), read only */
#define	IP30_GPIO_CLASSIC		2

/*
 * Flash PROM physical address, within BRIDGE widget
 */

#define	IP30_FLASH_BASE			0xc00000
#define	IP30_FLASH_SIZE			0x200000
#define	IP30_FLASH_ALT			0xe00000

/*
 * Multiprocessor configuration area
 */

#define	IP30_MAXCPUS		4

#define MPCONF_BASE		0x0000000000000600UL
#define MPCONF_LEN		0x80

#define MPCONF_MAGIC(i)		((i) * MPCONF_LEN + 0x00)
#define MPCONF_PRID(i)		((i) * MPCONF_LEN + 0x04)
#define MPCONF_PHYSID(i)	((i) * MPCONF_LEN + 0x08)
#define MPCONF_VIRTID(i)	((i) * MPCONF_LEN + 0x0c)
#define MPCONF_SCACHESZ(i)	((i) * MPCONF_LEN + 0x10)
#define MPCONF_FANLOADS(i)	((i) * MPCONF_LEN + 0x14)
#define MPCONF_LAUNCH(i)	((i) * MPCONF_LEN + 0x18)
#define MPCONF_RNDVZ(i)		((i) * MPCONF_LEN + 0x20)
#define MPCONF_STACKADDR(i)	((i) * MPCONF_LEN + 0x40)
#define MPCONF_LPARAM(i)	((i) * MPCONF_LEN + 0x48)
#define MPCONF_RPARAM(i)	((i) * MPCONF_LEN + 0x50)
#define MPCONF_IDLEFLAG(i)	((i) * MPCONF_LEN + 0x58)

#define MPCONF_MAGIC_VAL	0xbaddeed2

/*
 * Global data area
 */

#define	GDA_BASE		0x0000000000000400UL

#define	GDA_MAGIC		0x58464552		/* XFER */

#if !defined(_LOCORE)
struct ip30_gda {
	uint32_t	magic;		/* GDA_MAGIC */
	uint32_t 	promop;
	void		(*nmi_cb)(void);
	uint64_t	masterspid;
	void		*tlb_handlers[3];
	uint64_t	nmi_count;
};
#endif

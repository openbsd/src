/*	$OpenBSD: tcx.c,v 1.2 1998/11/20 15:57:24 deraadt Exp $	*/
/*	$NetBSD: tcx.c,v 1.8 1997/07/29 09:58:14 fair Exp $ */

/* 
 *  Copyright (c) 1996 The NetBSD Foundation, Inc.
 *  All rights reserved.
 * 
 *  This code is derived from software contributed to The NetBSD Foundation
 *  by Paul Kranenburg.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. All advertising materials mentioning features or use of this software
 *     must display the following acknowledgement:
 *         This product includes software developed by the NetBSD
 *         Foundation, Inc. and its contributors.
 *  4. Neither the name of The NetBSD Foundation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 * 
 *  THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 *  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 *  TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 *  PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */ 

/*
 * color display (TCX) driver.
 *
 * Does not handle interrupts, even though they can occur.
 *
 * XXX should defer colormap updates to vertical retrace interrupts
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <machine/fbio.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#ifdef DEBUG
#include <sys/proc.h>
#include <sys/syslog.h>
#endif

#include <vm/vm.h>

#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/fbvar.h>
#include <machine/cpu.h>
#include <machine/conf.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/tcxreg.h>
#include <sparc/dev/sbusvar.h>

#if 0
union cursor_cmap {		/* colormap, like bt_cmap, but tiny */
	u_char	cm_map[2][3];	/* 2 R/G/B entries */
	u_int	cm_chip[2];	/* 2 chip equivalents */
};

struct tcx_cursor {		/* tcx hardware cursor status */
	short	cc_enable;		/* cursor is enabled */
	struct	fbcurpos cc_pos;	/* position */
	struct	fbcurpos cc_hot;	/* hot-spot */
	struct	fbcurpos cc_size;	/* size of mask & image fields */
	u_int	cc_bits[2][32];		/* space for mask & image bits */
	union	cursor_cmap cc_color;	/* cursor colormap */
};
#endif

/* per-display variables */
struct tcx_softc {
	struct	device sc_dev;		/* base device */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	struct	rom_reg sc_physadr[TCX_NREG];	/* phys addr of h/w */
	int	sc_bustype;		/* type of bus we live on */
	volatile struct bt_regs *sc_bt;		/* Brooktree registers */
	volatile struct tcx_thc *sc_thc;	/* THC registers */
	short	sc_blanked;		/* true if blanked */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
};

/* autoconfiguration driver */
static void	tcxattach __P((struct device *, struct device *, void *));
static int	tcxmatch __P((struct device *, void *, void *));
static void	tcx_unblank __P((struct device *));

/* cdevsw prototypes */
cdev_decl(tcx);

struct cfattach tcx_ca = {
	sizeof(struct tcx_softc), tcxmatch, tcxattach
};

struct cfdriver tcx_cd = {
	NULL, "tcx", DV_DULL
};

/* frame buffer generic driver */
static struct fbdriver tcx_fbdriver = {
	tcx_unblank, tcxopen, tcxclose, tcxioctl, tcxmmap
};

extern int fbnode;

static void tcx_reset __P((struct tcx_softc *));
static void tcx_loadcmap __P((struct tcx_softc *, int, int));

#define OBPNAME	"SUNW,tcx"
/*
 * Match a tcx.
 */
int
tcxmatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct cfdata *cf = vcf;
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp(ra->ra_name, OBPNAME))
		return (0);

	/*
	 * Mask out invalid flags from the user.
	 */
	cf->cf_flags &= FB_USERMASK;

	if (ca->ca_bustype == BUS_SBUS)
		return (1);

	return (0);
}

/*
 * Attach a display.
 */
void
tcxattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	register struct tcx_softc *sc = (struct tcx_softc *)self;
	register struct confargs *ca = args;
	register int node = 0, ramsize, i;
	register volatile struct bt_regs *bt;
	struct fbdevice *fb = &sc->sc_fb;
	int isconsole = 0, sbus = 1;
	extern struct tty *fbconstty;

	fb->fb_driver = &tcx_fbdriver;
	fb->fb_device = &sc->sc_dev;
	fb->fb_flags = sc->sc_dev.dv_cfdata->cf_flags;

	/*
	 * XXX - should be set to FBTYPE_TCX.
	 * XXX For CG3 emulation to work in current (96/6) X11 servers,
	 * XXX `fbtype' must point to an "unregocnised" entry.
	 */
	fb->fb_type.fb_type = FBTYPE_RESERVED3;

	if (ca->ca_ra.ra_nreg != TCX_NREG)
		panic("tcx: oops");

	/* Copy register address spaces */
	for (i = 0; i < TCX_NREG; i++)
		sc->sc_physadr[i] = ca->ca_ra.ra_reg[i];

	/* XXX - fix THC and TEC offsets */
	sc->sc_physadr[TCX_REG_TEC].rr_paddr += 0x1000;
	sc->sc_physadr[TCX_REG_THC].rr_paddr += 0x1000;

	sc->sc_bt = bt = (volatile struct bt_regs *)
		mapiodev(&ca->ca_ra.ra_reg[TCX_REG_CMAP], 0, sizeof *sc->sc_bt);
	sc->sc_thc = (volatile struct tcx_thc *)
		mapiodev(&ca->ca_ra.ra_reg[TCX_REG_THC], 0, sizeof *sc->sc_thc);

	switch (ca->ca_bustype) {
	case BUS_SBUS:
		node = ca->ca_ra.ra_node;
		break;

	case BUS_OBIO:
	default:
		printf("TCX on bus 0x%x?\n", ca->ca_bustype);
		return;
	}

	fb->fb_type.fb_depth = node_has_property(node, "tcx-24-bit")
		? 24
		: (node_has_property(node, "tcx-8-bit")
			? 8
			: 8);

	fb_setsize(fb, fb->fb_type.fb_depth, 1152, 900,
		   node, ca->ca_bustype);

	ramsize = fb->fb_type.fb_height * fb->fb_linebytes;
	fb->fb_type.fb_cmsize = 256;
	fb->fb_type.fb_size = ramsize;
	printf(": %s, %d x %d", OBPNAME,
		fb->fb_type.fb_width,
		fb->fb_type.fb_height);

	isconsole = node == fbnode && fbconstty != NULL;

	printf(", id %d, rev %d, sense %d",
		(sc->sc_thc->thc_config & THC_CFG_FBID) >> THC_CFG_FBID_SHIFT,
		(sc->sc_thc->thc_config & THC_CFG_REV) >> THC_CFG_REV_SHIFT,
		(sc->sc_thc->thc_config & THC_CFG_SENSE) >> THC_CFG_SENSE_SHIFT
	);

	/* reset cursor & frame buffer controls */
	tcx_reset(sc);

	/* grab initial (current) color map (DOES THIS WORK?) */
	bt->bt_addr = 0;
	for (i = 0; i < 256 * 3; i++)
		((char *)&sc->sc_cmap)[i] = bt->bt_cmap >> 24;

	/* enable video */
	sc->sc_thc->thc_hcmisc |= THC_MISC_VIDEN;

	if (isconsole) {
		printf(" (console)\n");
	} else
		printf("\n");

	if (sbus)
		sbus_establish(&sc->sc_sd, &sc->sc_dev);
	if (node == fbnode)
		fb_attach(&sc->sc_fb, isconsole);
}

int
tcxopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit >= tcx_cd.cd_ndevs || tcx_cd.cd_devs[unit] == NULL)
		return (ENXIO);
	return (0);
}

int
tcxclose(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	struct tcx_softc *sc = tcx_cd.cd_devs[minor(dev)];

	tcx_reset(sc);
	return (0);
}

int
tcxioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	register caddr_t data;
	int flags;
	struct proc *p;
{
	register struct tcx_softc *sc = tcx_cd.cd_devs[minor(dev)];
	int error;

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGATTR:
#define fba ((struct fbgattr *)data)
		fba->real_type = sc->sc_fb.fb_type.fb_type;
		fba->owner = 0;		/* XXX ??? */
		fba->fbtype = sc->sc_fb.fb_type;
		fba->sattr.flags = 0;
		fba->sattr.emu_type = sc->sc_fb.fb_type.fb_type;
		fba->sattr.dev_specific[0] = -1;
		fba->emu_types[0] = sc->sc_fb.fb_type.fb_type;
		fba->emu_types[1] = FBTYPE_SUN3COLOR;
		fba->emu_types[2] = -1;
#undef fba
		break;

	case FBIOGETCMAP:
		return (bt_getcmap((struct fbcmap *)data, &sc->sc_cmap, 256));

	case FBIOPUTCMAP:
		/* copy to software map */
#define	p ((struct fbcmap *)data)
		error = bt_putcmap(p, &sc->sc_cmap, 256);
		if (error)
			return (error);
		/* now blast them into the chip */
		/* XXX should use retrace interrupt */
		tcx_loadcmap(sc, p->index, p->count);
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = sc->sc_blanked;
		break;

	case FBIOSVIDEO:
		if (*(int *)data)
			tcx_unblank(&sc->sc_dev);
		else if (!sc->sc_blanked) {
			sc->sc_blanked = 1;
			sc->sc_thc->thc_hcmisc &= ~THC_MISC_VIDEN;
			/* Put monitor in `power-saving mode' */
			sc->sc_thc->thc_hcmisc |= THC_MISC_VSYNC_DISABLE;
			sc->sc_thc->thc_hcmisc |= THC_MISC_HSYNC_DISABLE;
		}
		break;

	default:
#ifdef DEBUG
		log(LOG_NOTICE, "tcxioctl(0x%lx) (%s[%d])\n", cmd,
		    p->p_comm, p->p_pid);
#endif
		return (ENOTTY);
	}
	return (0);
}

/*
 * Clean up hardware state (e.g., after bootup or after X crashes).
 */
static void
tcx_reset(sc)
	register struct tcx_softc *sc;
{
	register volatile struct bt_regs *bt;

	/* Enable cursor in Brooktree DAC. */
	bt = sc->sc_bt;
	bt->bt_addr = 0x06 << 24;
	bt->bt_ctrl |= 0x03 << 24;
}

/*
 * Load a subset of the current (new) colormap into the color DAC.
 */
static void
tcx_loadcmap(sc, start, ncolors)
	register struct tcx_softc *sc;
	register int start, ncolors;
{
	register volatile struct bt_regs *bt;
	register u_int *ip, i;
	register int count;

	ip = &sc->sc_cmap.cm_chip[BT_D4M3(start)];	/* start/4 * 3 */
	count = BT_D4M3(start + ncolors - 1) - BT_D4M3(start) + 3;
	bt = sc->sc_bt;
	bt->bt_addr = BT_D4M4(start) << 24;
	while (--count >= 0) {
		i = *ip++;
		/* hardware that makes one want to pound boards with hammers */
		bt->bt_cmap = i;
		bt->bt_cmap = i << 8;
		bt->bt_cmap = i << 16;
		bt->bt_cmap = i << 24;
	}
}

static void
tcx_unblank(dev)
	struct device *dev;
{
	struct tcx_softc *sc = (struct tcx_softc *)dev;

	if (sc->sc_blanked) {
		sc->sc_blanked = 0;
		sc->sc_thc->thc_hcmisc &= ~THC_MISC_VSYNC_DISABLE;
		sc->sc_thc->thc_hcmisc &= ~THC_MISC_HSYNC_DISABLE;
		sc->sc_thc->thc_hcmisc |= THC_MISC_VIDEN;
	}
}

/*
 * Base addresses at which users can mmap() the various pieces of a tcx.
 */
#define	TCX_USER_RAM	0x00000000
#define	TCX_USER_RAM24	0x01000000
#define	TCX_USER_RAM_COMPAT	0x04000000	/* cg3 emulation */
#define	TCX_USER_STIP	0x10000000
#define	TCX_USER_BLIT	0x20000000
#define	TCX_USER_RDFB32	0x28000000
#define	TCX_USER_RSTIP	0x30000000
#define	TCX_USER_RBLIT	0x38000000
#define	TCX_USER_TEC	0x70001000
#define	TCX_USER_BTREGS	0x70002000
#define	TCX_USER_THC	0x70004000
#define	TCX_USER_DHC	0x70008000
#define	TCX_USER_ALT	0x7000a000
#define	TCX_USER_UART	0x7000c000
#define	TCX_USER_VRT	0x7000e000
#define	TCX_USER_ROM	0x70010000

struct mmo {
	u_int	mo_uaddr;	/* user (virtual) address */
	u_int	mo_size;	/* size, or 0 for video ram size */
	u_int	mo_bank;	/* register bank number */
};

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * XXX	needs testing against `demanding' applications (e.g., aviator)
 */
int
tcxmmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	register struct tcx_softc *sc = tcx_cd.cd_devs[minor(dev)];
	register struct mmo *mo;
	register u_int u, sz;
	static struct mmo mmo[] = {
		{ TCX_USER_RAM, 0, TCX_REG_DFB8 },
		{ TCX_USER_RAM24, 0, TCX_REG_DFB24 },
		{ TCX_USER_RAM_COMPAT, 0, TCX_REG_DFB8 },

		{ TCX_USER_STIP, 1, TCX_REG_STIP },
		{ TCX_USER_BLIT, 1, TCX_REG_BLIT },
		{ TCX_USER_RDFB32, 1, TCX_REG_RDFB32 },
		{ TCX_USER_RSTIP, 1, TCX_REG_RSTIP },
		{ TCX_USER_RBLIT, 1, TCX_REG_RBLIT },
		{ TCX_USER_TEC, 1, TCX_REG_TEC },
		{ TCX_USER_BTREGS, 8192 /* XXX */, TCX_REG_CMAP },
		{ TCX_USER_THC, sizeof(struct tcx_thc), TCX_REG_THC },
		{ TCX_USER_DHC, 1, TCX_REG_DHC },
		{ TCX_USER_ALT, 1, TCX_REG_ALT },
		{ TCX_USER_ROM, 65536, TCX_REG_ROM },
	};
#define NMMO (sizeof mmo / sizeof *mmo)

	if (off & PGOFSET)
		panic("tcxmmap");

	if (off < 0)
		return (-1);

	/*
	 * Entries with size 0 map video RAM (i.e., the size in fb data).
	 *
	 * Since we work in pages, the fact that the map offset table's
	 * sizes are sometimes bizarre (e.g., 1) is effectively ignored:
	 * one byte is as good as one page.
	 */
	for (mo = mmo; mo < &mmo[NMMO]; mo++) {
		if ((u_int)off < mo->mo_uaddr)
			continue;
		u = off - mo->mo_uaddr;
		sz = mo->mo_size ? mo->mo_size : sc->sc_fb.fb_type.fb_size;
		if (u < sz)
			return (REG2PHYS(&sc->sc_physadr[mo->mo_bank], u) |
				PMAP_NC);
	}
#ifdef DEBUG
	{
	  register struct proc *p = curproc;	/* XXX */
	  log(LOG_NOTICE, "tcxmmap(0x%x) (%s[%d])\n", off, p->p_comm, p->p_pid);
	}
#endif
	return (-1);	/* not a user-map offset */
}

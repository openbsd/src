/*	$OpenBSD: p9100.c,v 1.1 1999/09/06 03:46:16 jason Exp $	*/

/*
 * Copyright (c) 1999 Jason L. Wright (jason@thought.net)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * color display (p9100) driver.  Based on cgthree.c and the NetBSD
 * p9100 driver.
 *
 * Does not handle interrupts, even though they can occur.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/tty.h>
#include <sys/conf.h>

#include <vm/vm.h>

#include <machine/fbio.h>
#include <machine/autoconf.h>
#include <machine/pmap.h>
#include <machine/fbvar.h>
#include <machine/cpu.h>
#include <machine/conf.h>

#include <sparc/dev/btreg.h>
#include <sparc/dev/btvar.h>
#include <sparc/dev/sbusvar.h>

/* per-display variables */
struct p9100_softc {
	struct	device sc_dev;		/* base device */
	struct	sbusdev sc_sd;		/* sbus device */
	struct	fbdevice sc_fb;		/* frame buffer device */
	struct	rom_reg	sc_phys;	/* phys address description */
	struct	p9100_cmd *sc_cmd;	/* command registers (dac, etc) */
	struct	p9100_ctl *sc_ctl;	/* control registers (draw engine) */
	union	bt_cmap sc_cmap;	/* Brooktree color map */
	u_int32_t	sc_junk;	/* throwaway value */
};

/* autoconfiguration driver */
void	p9100attach(struct device *, struct device *, void *);
int	p9100match(struct device *, void *, void *);
void	p9100unblank(struct device *);

/* cdevsw prototypes */
cdev_decl(p9100);

struct cfattach pnozz_ca = {
	sizeof(struct p9100_softc), p9100match, p9100attach
};

struct cfdriver pnozz_cd = {
	NULL, "pnozz", DV_DULL
};

/* frame buffer generic driver */
struct fbdriver p9100fbdriver = {
	p9100unblank, p9100open, p9100close, p9100ioctl, p9100mmap
};

extern int fbnode;
extern struct tty *fbconstty;

void p9100loadcmap __P((struct p9100_softc *, int, int));
void p9100_set_video __P((struct p9100_softc *, int));
int p9100_get_video __P((struct p9100_softc *));

/*
 * System control and command registers
 * (IBM RGB528 RamDac, p9100, video coprocessor)
 */
struct p9100_ctl {
	/* System control registers: 0x0000 - 0x00ff */
	struct p9100_scr {
		volatile u_int32_t	unused0;
		volatile u_int32_t	scr;		/* system config reg */
		volatile u_int32_t	ir;		/* interrupt reg */
		volatile u_int32_t	ier;		/* interrupt enable */
		volatile u_int32_t	arbr;		/* alt read bank reg */
		volatile u_int32_t	awbr;		/* alt write bank reg */
		volatile u_int32_t	unused1[58];
	} ctl_scr;

	/* Video control registers: 0x0100 - 0x017f */
	struct p9100_vcr {
		volatile u_int32_t	unused0;
		volatile u_int32_t	hcr;		/* horizontal cntr */
		volatile u_int32_t	htr;		/* horizontal total */
		volatile u_int32_t	hsre;		/* horiz sync rising */
		volatile u_int32_t	hbre;		/* horiz blank rising */
		volatile u_int32_t	hbfe;		/* horiz blank fallng */
		volatile u_int32_t	hcp;		/* horiz cntr preload */
		volatile u_int32_t	vcr;		/* vertical cntr */
		volatile u_int32_t	vl;		/* vertical length */
		volatile u_int32_t	vsre;		/* vert sync rising */
		volatile u_int32_t	vbre;		/* vert blank rising */
		volatile u_int32_t	vbfe;		/* vert blank fallng */
		volatile u_int32_t	vcp;		/* vert cntr preload */
		volatile u_int32_t	sra;		/* scrn repaint addr */
		volatile u_int32_t	srtc1;		/* scrn rpnt time 1 */
		volatile u_int32_t	qsf;		/* qsf counter */
		volatile u_int32_t	srtc2;		/* scrn rpnt time 2 */
		volatile u_int32_t	unused1[15];
	} ctl_vcr;

	/* VRAM control registers: 0x0180 - 0x1ff */
	struct p9100_vram {
		volatile u_int32_t	unused0;
		volatile u_int32_t	mc;		/* memory config */
		volatile u_int32_t	rp;		/* refresh period */
		volatile u_int32_t	rc;		/* refresh count */
		volatile u_int32_t	rasmax;		/* ras low maximum */
		volatile u_int32_t	rascur;		/* ras low current */
		volatile u_int32_t	unused1[26];
	} ctl_vram;

	/* IBM RGB528 RAMDAC registers: 0x0200 - 0x3ff */
	struct p9100_dac {
		volatile u_int32_t	pwraddr;	/* wr palette address */
		volatile u_int32_t	paldata;	/* palette data */
		volatile u_int32_t	pixmask;	/* pixel mask */
		volatile u_int32_t	prdaddr;	/* rd palette address */
		volatile u_int32_t	idxlow;		/* reg index low */
		volatile u_int32_t	idxhigh;	/* reg index high */
		volatile u_int32_t	regdata;	/* register data */
		volatile u_int32_t	idxctrl;	/* index control */
		volatile u_int32_t	unused1[120];
	} ctl_dac;

	/* Video coprocessor interface: 0x0400 - 0x1fff */
	volatile u_int32_t	ctl_vci[768];
};

#define	SRTC1_VIDEN	0x00000020


/*
 * Select the appropriate register group within the control registers
 * (must be done before any write to a register within the group, but
 * subsquent writes to the same group do not need to reselect).
 */
#define	P9100_SELECT_SCR(sc)	((sc)->sc_junk = (sc)->sc_ctl->ctl_scr.scr)
#define	P9100_SELECT_VCR(sc)	((sc)->sc_junk = (sc)->sc_ctl->ctl_vcr.hcr)
#define	P9100_SELECT_VRAM(sc)	((sc)->sc_junk = (sc)->sc_ctl->ctl_vram.mc)
#define	P9100_SELECT_DAC(sc)	((sc)->sc_junk = (sc)->sc_ctl->ctl_dac.pwraddr)
#define	P9100_SELECT_VCI(sc)	((sc)->sc_junk = (sc)->sc_ctl->ctl_vci[0])

/*
 * Drawing engine
 */
struct p9100_cmd {
	volatile u_int32_t	cmd_regs[0x800];
};

int
p9100match(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	if (strcmp("p9100", ra->ra_name))
		return (0);
	return (1);
}

/*
 * Attach a display.  We need to notice if it is the console, too.
 */
void
p9100attach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct p9100_softc *sc = (struct p9100_softc *)self;
	struct confargs *ca = args;
	int node = 0, i;
	int isconsole;
	char *cp;

	sc->sc_fb.fb_driver = &p9100fbdriver;
	sc->sc_fb.fb_device = &sc->sc_dev;
	sc->sc_fb.fb_flags = sc->sc_dev.dv_cfdata->cf_flags & FB_USERMASK;
	sc->sc_fb.fb_type.fb_type = FBTYPE_SUN3COLOR;

	sc->sc_phys = ca->ca_ra.ra_reg[2];

	sc->sc_ctl = mapiodev(&(ca->ca_ra.ra_reg[0]), 0,
	    ca->ca_ra.ra_reg[0].rr_len);
	sc->sc_cmd = mapiodev(&(ca->ca_ra.ra_reg[1]), 0,
	    ca->ca_ra.ra_reg[1].rr_len);

	/*
	 * When the ROM has mapped in a p9100 display, the address
	 * maps only the video RAM, so in any case we have to map the
	 * registers ourselves.  We only need the video RAM if we are
	 * going to print characters via rconsole.
	 */
	node = ca->ca_ra.ra_node;
	isconsole = node == fbnode && fbconstty != NULL;
	if (ca->ca_ra.ra_nvaddrs > 0)
		sc->sc_fb.fb_pixels = ca->ca_ra.ra_vaddrs[0];
	if (isconsole && sc->sc_fb.fb_pixels == NULL)
		sc->sc_fb.fb_pixels = mapiodev(&(ca->ca_ra.ra_reg[2]), 0,
		    ca->ca_ra.ra_reg[2].rr_len);

	P9100_SELECT_SCR(sc);
	i = sc->sc_ctl->ctl_scr.scr;
	printf(":%08x", i);
	switch ((i >> 26) & 7) {
	case 5:
		sc->sc_fb.fb_type.fb_depth = 32;
		break;
	case 7:
		sc->sc_fb.fb_type.fb_depth = 24;
		break;
	case 3:
		sc->sc_fb.fb_type.fb_depth = 16;
		break;
	case 2:
	default:
		sc->sc_fb.fb_type.fb_depth = 8;
		break;
	}
	fb_setsize(&sc->sc_fb, sc->sc_fb.fb_type.fb_depth, 800, 600,
	    node, ca->ca_bustype);

	sbus_establish(&sc->sc_sd, &sc->sc_dev);

	sc->sc_fb.fb_type.fb_cmsize = getpropint(node, "cmsize", 256);
	sc->sc_fb.fb_type.fb_size =
	    sc->sc_fb.fb_type.fb_height * sc->sc_fb.fb_linebytes;
	printf(": rev %x, %d x %d, depth %d", i,
	    sc->sc_fb.fb_type.fb_width, sc->sc_fb.fb_type.fb_height,
	    sc->sc_fb.fb_type.fb_depth);

	/* initialize color map */
	cp = &sc->sc_cmap.cm_map[0][0];
	cp[0] = cp[1] = cp[2] = 0;
	for (i = 1, cp = &sc->sc_cmap.cm_map[i][0];
	     i < sc->sc_fb.fb_type.fb_cmsize; cp += 3, i++)
		cp[0] = cp[1] = cp[2] = 0xff;
	p9100loadcmap(sc, 0, 256);

	/* make sure we are not blanked */
	p9100_set_video(sc, 1);

	if (isconsole) {
		printf(" (console)\n");
#ifdef RASTERCONSOLE
		for (i = 0 ; i < sc->sc_fb.fb_type.fb_size; i++) {
			if (sc->sc_fb.fb_pixels[i] == 0) {
				sc->sc_fb.fb_pixels[i] = 1;
			} else if (sc->sc_fb.fb_pixels[i] == (char) 255) {
				sc->sc_fb.fb_pixels[i] = 0;
			}
		}
		p9100loadcmap(sc, 255, 1);
		fbrcons_init(&sc->sc_fb);
#endif
	} else
		printf("\n");

	if (node == fbnode)
		fb_attach(&sc->sc_fb, isconsole);
}

int
p9100open(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = minor(dev);

	if (unit >= pnozz_cd.cd_ndevs || pnozz_cd.cd_devs[unit] == NULL)
		return (ENXIO);
	return (0);
}

int
p9100close(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	return (0);
}

int
p9100ioctl(dev, cmd, data, flags, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct p9100_softc *sc = pnozz_cd.cd_devs[minor(dev)];
	struct fbgattr *fba;
	int error;

	switch (cmd) {

	case FBIOGTYPE:
		*(struct fbtype *)data = sc->sc_fb.fb_type;
		break;

	case FBIOGATTR:
		fba = (struct fbgattr *)data;
		fba->real_type = sc->sc_fb.fb_type.fb_type;
		fba->owner = 0;		/* XXX ??? */
		fba->fbtype = sc->sc_fb.fb_type;
		fba->sattr.flags = 0;
		fba->sattr.emu_type = sc->sc_fb.fb_type.fb_type;
		fba->sattr.dev_specific[0] = -1;
		fba->emu_types[0] = sc->sc_fb.fb_type.fb_type;
		fba->emu_types[1] = -1;
		break;

	case FBIOGETCMAP:
		return (bt_getcmap((struct fbcmap *)data, &sc->sc_cmap, 256));

	case FBIOPUTCMAP:
		/* copy to software map */
#define p ((struct fbcmap *)data)
		error = bt_putcmap(p, &sc->sc_cmap, 256);
		if (error)
			return (error);
		/* now blast them into the chip */
		/* XXX should use retrace interrupt */
		p9100loadcmap(sc, p->index, p->count);
#undef p
		break;

	case FBIOGVIDEO:
		*(int *)data = p9100_get_video(sc);
		break;

	case FBIOSVIDEO:
		p9100_set_video(sc, *(int *)data);
		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

/*
 * Undo the effect of an FBIOSVIDEO that turns the video off.
 */
void
p9100unblank(dev)
	struct device *dev;
{
	p9100_set_video((struct p9100_softc *)dev, 1);
}

void
p9100_set_video(sc, enable)
	struct p9100_softc *sc;
	int enable;
{
	u_int32_t v;

	P9100_SELECT_VCR(sc);
	v = sc->sc_ctl->ctl_vcr.srtc1;
	if (enable)
		v |= SRTC1_VIDEN;
	else
		v &= ~SRTC1_VIDEN;
	sc->sc_ctl->ctl_vcr.srtc1 = v;
}

int
p9100_get_video(sc)
	struct p9100_softc *sc;
{
	return ((sc->sc_ctl->ctl_vcr.srtc1 & SRTC1_VIDEN) != 0);
}

/*
 * Load a subset of the current (new) colormap into the Brooktree DAC.
 */
void
p9100loadcmap(sc, start, ncolors)
	struct p9100_softc *sc;
	int start, ncolors;
{
	u_char *p;

	P9100_SELECT_VRAM(sc);
	P9100_SELECT_VRAM(sc);
	sc->sc_junk = sc->sc_ctl->ctl_dac.pwraddr;
	sc->sc_ctl->ctl_dac.pwraddr = start << 16;

	for (p = sc->sc_cmap.cm_map[start], ncolors *= 3; ncolors-- > 0; p++) {
		/* These generate a short delay between ramdac writes */
		P9100_SELECT_VRAM(sc);
		P9100_SELECT_VRAM(sc);

		sc->sc_junk = sc->sc_ctl->ctl_dac.paldata;
		sc->sc_ctl->ctl_dac.paldata = (*p) << 16;
	}
}

/*
 * Return the address that would map the given device at the given
 * offset, allowing for the given protection, or return -1 for error.
 *
 * The cg3 is mapped starting at 256KB, for pseudo-compatibility with
 * the cg4 (which had an overlay plane in the first 128K and an enable
 * plane in the next 128K).  X11 uses only 256k+ region but tries to
 * map the whole thing, so we repeatedly map the first 256K to the
 * first page of the color screen.  If someone tries to use the overlay
 * and enable regions, they will get a surprise....
 *
 * As well, mapping at an offset of 0x04000000 causes the cg3 to be
 * mapped in flat mode without the cg4 emulation.
 */
int
p9100mmap(dev, off, prot)
	dev_t dev;
	int off, prot;
{
	struct p9100_softc *sc = pnozz_cd.cd_devs[minor(dev)];
#define START		(128*1024 + 128*1024)
#define NOOVERLAY	(0x04000000)

	if (off & PGOFSET)
		panic("p9100mmap");

	if (off < 0)
		return (-1);
#define	CG3_MMAP_OFFSET 0x04000000
	/*
	 * Make Xsun think we are a CG3 (SUN3COLOR)
	 */
	if ((u_int)off >= CG3_MMAP_OFFSET &&
	    (u_int)off < CG3_MMAP_OFFSET + 0x00200000) {
		off -= CG3_MMAP_OFFSET;
		return (REG2PHYS(&sc->sc_phys, off | PMAP_NC));
	}

	return (-1);
}

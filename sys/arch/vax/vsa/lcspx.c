/*	$OpenBSD: lcspx.c,v 1.17 2011/09/20 21:11:33 miod Exp $	*/
/*
 * Copyright (c) 2006 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice, this permission notice, and the disclaimer below
 * appear in all copies.
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
 * Copyright (c) 2004 Blaz Antonic
 * All rights reserved.
 *
 * This software contains code written by Michael L. Hitch.
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
 *    must display the abovementioned copyrights
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <machine/nexus.h>
#include <machine/vsbus.h>
#include <machine/scb.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/ka420.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <dev/ic/bt463reg.h>	/* actually it's a 459 here... */

#define	GPXADDR			0x3c000000	/* XXX */
#define	GPX_ADDER_OFFSET	0x0000		/* XXX */
#include <vax/qbus/qdreg.h>			/* XXX */

#define	LCSPX_REG_ADDR		0x39302000	/* registers */
#define	LCSPX_REG_SIZE		    0x2000
#define	LCSPX_REG1_ADDR		0x39b00000	/* more registers */
#define	LCSPX_RAMDAC_ADDR	0x39b10000	/* RAMDAC */
#define	LCSPX_RAMDAC_INTERLEAVE	0x00004000
#define	LCSPX_FB_ADDR		0x38000000	/* frame buffer */

void	lcspx_attach(struct device *, struct device *, void *);
int	lcspx_vsbus_match(struct device *, void *, void *);
int	lcspx_vxtbus_match(struct device *, void *, void *);

struct	lcspx_screen {
	struct rasops_info ss_ri;
	u_int		ss_fbsize;
	caddr_t		ss_addr;		/* frame buffer address */
	volatile u_int8_t *ss_ramdac[4];
	vaddr_t		ss_reg;
	u_int8_t	ss_cmap[256 * 3];
};

#define	lcspx_reg_read(ss, reg) \
	*(volatile u_int32_t *)((ss)->ss_reg + (reg))
#define	lcspx_reg_write(ss, reg, val) \
	*(volatile u_int32_t *)((ss)->ss_reg + (reg)) = (val)

/* for console */
struct lcspx_screen lcspx_consscr;

struct	lcspx_softc {
	struct device sc_dev;
	struct lcspx_screen *sc_scr;
	int	sc_nscreens;
};

struct cfattach lcspx_vsbus_ca = {
	sizeof(struct lcspx_softc), lcspx_vsbus_match, lcspx_attach,
};

struct cfattach lcspx_vxtbus_ca = {
	sizeof(struct lcspx_softc), lcspx_vxtbus_match, lcspx_attach,
};

struct	cfdriver lcspx_cd = {
	NULL, "lcspx", DV_DULL
};

struct wsscreen_descr lcspx_stdscreen = {
	"std",
};

const struct wsscreen_descr *_lcspx_scrlist[] = {
	&lcspx_stdscreen,
};

const struct wsscreen_list lcspx_screenlist = {
	sizeof(_lcspx_scrlist) / sizeof(struct wsscreen_descr *),
	_lcspx_scrlist,
};

int	lcspx_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	lcspx_mmap(void *, off_t, int);
int	lcspx_alloc_screen(void *, const struct wsscreen_descr *,
	    void **, int *, int *, long *);
void	lcspx_free_screen(void *, void *);
int	lcspx_show_screen(void *, void *, int,
	    void (*) (void *, int, int), void *);

const struct wsdisplay_accessops lcspx_accessops = {
	lcspx_ioctl,
	lcspx_mmap,
	lcspx_alloc_screen,
	lcspx_free_screen,
	lcspx_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	NULL	/* burner */
};

int	lcspx_getcmap(struct lcspx_screen *, struct wsdisplay_cmap *);
void	lcspx_loadcmap(struct lcspx_screen *, int, int);
int	lcspx_putcmap(struct lcspx_screen *, struct wsdisplay_cmap *);
static __inline__
void	lcspx_ramdac_wraddr(struct lcspx_screen *, u_int);
void	lcspx_resetcmap(struct lcspx_screen *);
int	lcspx_setup_screen(struct lcspx_screen *, u_int, u_int);

int
lcspx_vsbus_match(struct device *parent, void *vcf, void *aux)
{
	extern int oldvsbus;
	struct vsbus_softc *sc = (void *)parent;
	struct vsbus_attach_args *va = aux;
	volatile struct adder *adder;
	u_short status;

	if (va->va_paddr != LCSPX_REG_ADDR)
		return (0);

	switch (vax_boardtype) {
	default:
		return (0);

	case VAX_BTYP_420:
	case VAX_BTYP_43:
		/* not present on microvaxes */
		if ((vax_confdata & KA420_CFG_MULTU) != 0)
			return (0);

		if ((vax_confdata & KA420_CFG_VIDOPT) == 0)
			return (0);

		/*
		 * We can not check for video memory at the SPX address,
		 * because if no SPX board is installed, the probe will
		 * just hang.
		 * Instead, check for a GPX board and skip probe if the
		 * video option really is a GPX.
		 */
		adder = (volatile struct adder *)
		    vax_map_physmem(GPXADDR + GPX_ADDER_OFFSET, 1);
		if (adder == NULL)
			return (0);
		adder->status = 0;
		status = adder->status;
		vax_unmap_physmem((vaddr_t)adder, 1);
		if (status != offsetof(struct adder, status))
			return (0);

		/* XXX do not attach if not console... yet */
		if ((vax_confdata & KA420_CFG_L3CON) != 0)
			return (0);

		break;

	case VAX_BTYP_49:
		if ((vax_confdata & 0x12) != 0x02)
			return (0);

		break;
	}

	sc->sc_mask = 0x04;	/* XXX - should be generated */
	scb_fake(0x120, oldvsbus ? 0x14 : 0x15);
	return (20);
}

int
lcspx_vxtbus_match(struct device *parent, void *vcf, void *aux)
{
	struct bp_conf *bp = aux;
	int missing;
	volatile u_int8_t *ch;

	if (strcmp(bp->type, lcspx_cd.cd_name) != 0)
		return (0);

	/*
	 * Check for video memory at SPX address.
	 */
	missing = 0;
	ch = (volatile u_int8_t *)vax_map_physmem(LCSPX_FB_ADDR, 1);
	*ch = 0x01;
	if ((*ch & 0x01) == 0)
		missing = 1;
	else {
		*ch = 0x00;
		if ((*ch & 0x01) != 0)
			missing = 1;
	}
	vax_unmap_physmem((vaddr_t)ch, 1);

	return (missing ? 0 : 1);
}

void
lcspx_attach(struct device *parent, struct device *self, void *aux)
{
	struct lcspx_softc *sc = (struct lcspx_softc *)self;
	struct lcspx_screen *ss;
	struct wsemuldisplaydev_attach_args aa;
	int i, console;
	vaddr_t reg1;
	u_int32_t magic;
	u_int width, height;
	extern struct consdev wsdisplay_cons;

	if (cn_tab == &wsdisplay_cons) {
		switch (vax_boardtype) {
		case VAX_BTYP_420:
		case VAX_BTYP_43:
			console = (vax_confdata & KA420_CFG_L3CON) == 0;
			break;
		case VAX_BTYP_49:
			console = (vax_confdata & 8) == 0;
			break;
		default: /* VXT2000 */
			console = (vax_confdata & 2) != 0;
			break;
		}
	} else
		console = 0;
	if (console) {
		ss = &lcspx_consscr;
		sc->sc_nscreens = 1;
	} else {
		ss = malloc(sizeof(*ss), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (ss == NULL) {
			printf(": can not allocate memory\n");
			return;
		}

		switch (vax_boardtype) {
		case VAX_BTYP_420:
		case VAX_BTYP_43:
			/*
			 * See ``EVIL KLUGE ALERT!'' comments in
			 * lcspxcninit() at the end of this file
			 * for meaningless details.
			 */
			reg1 = vax_map_physmem(LCSPX_REG1_ADDR, 1);
			if (reg1 == 0L) {
				printf(": can not map registers\n");
				goto fail1;
			}
			magic = *(u_int32_t *)(reg1 + 0x11c);
			vax_unmap_physmem(reg1, 1);

			if (magic & 0x80) {
				width = 1280;
				height = 1024;
			} else {
				width = 1024;
				height = 864;
			}
			break;
		default:
			width = 1280;
			height = 1024;
			break;
		}

		ss->ss_reg = vax_map_physmem(LCSPX_REG_ADDR,
		    LCSPX_REG_SIZE / VAX_NBPG);
		if (ss->ss_reg == 0L) {
			printf(": can not map registers\n");
			goto fail1;
		}

		for (i = 0; i < 4; i++) {
			ss->ss_ramdac[i] = (volatile u_int8_t *)vax_map_physmem(
			    LCSPX_RAMDAC_ADDR + i * LCSPX_RAMDAC_INTERLEAVE, 1);
			if (ss->ss_ramdac[i] == NULL) {
				printf(": can not map RAMDAC registers\n");
				goto fail2;
			}
		}

		ss->ss_fbsize = width * height;
		ss->ss_addr = (caddr_t)vax_map_physmem(LCSPX_FB_ADDR,
		    ss->ss_fbsize / VAX_NBPG);
		if (ss->ss_addr == NULL) {
			printf(": can not map frame buffer\n");
			goto fail3;
		}

		if (lcspx_setup_screen(ss, width, height) != 0) {
			printf(": initialization failed\n");
			goto fail3;
		}
	}
	sc->sc_scr = ss;

	printf("\n%s: %dx%dx8 frame buffer\n", self->dv_xname,
	    ss->ss_ri.ri_width, ss->ss_ri.ri_height);

	aa.console = console;
	aa.scrdata = &lcspx_screenlist;
	aa.accessops = &lcspx_accessops;
	aa.accesscookie = sc;
	aa.defaultscreens = 0;

	config_found(self, &aa, wsemuldisplaydevprint);
	return;

fail3:
	vax_unmap_physmem((vaddr_t)ss->ss_addr, ss->ss_fbsize / VAX_NBPG);
fail2:
	for (i = 0; i < 4; i++)
		if (ss->ss_ramdac[i] != NULL)
			vax_unmap_physmem((vaddr_t)ss->ss_ramdac[i], 1);
	vax_unmap_physmem(ss->ss_reg, LCSPX_REG_SIZE / VAX_NBPG);
fail1:
	free(ss, M_DEVBUF);
}

static __inline__ void
lcspx_ramdac_wraddr(struct lcspx_screen *ss, u_int addr)
{
	*(ss->ss_ramdac[BT463_REG_ADDR_LOW]) = addr & 0xff;
	*(ss->ss_ramdac[BT463_REG_ADDR_HIGH]) = (addr >> 8) & 0xff;
}

/*
 * Initialize anything necessary for an emulating wsdisplay to work (i.e.
 * pick a font, initialize a rasops structure, setup the accessops callbacks.)
 */
int
lcspx_setup_screen(struct lcspx_screen *ss, u_int width, u_int height)
{
	struct rasops_info *ri = &ss->ss_ri;

	bzero(ri, sizeof(*ri));
	ri->ri_depth = 8;
	ri->ri_width = width;
	ri->ri_height = height;
	ri->ri_stride = width;
	ri->ri_flg = RI_CLEAR | RI_CENTER;
	ri->ri_bits = (void *)ss->ss_addr;
	ri->ri_hw = ss;

	/*
	 * Enable all planes for reading and writing
	 */
	lcspx_reg_write(ss, 0x1170, 0xffffffff);
	lcspx_reg_write(ss, 0x1174, 0xffffffff);
	lcspx_ramdac_wraddr(ss, 0x0204);	/* plane mask */
	*(ss->ss_ramdac[BT463_REG_IREG_DATA]) = 0xff;

	/*
	 * Ask for an unholy big display, rasops will trim this to more
	 * reasonable values.
	 */
	if (rasops_init(ri, 160, 160) != 0)
		return (-1);

	lcspx_resetcmap(ss);

	lcspx_stdscreen.ncols = ri->ri_cols;
	lcspx_stdscreen.nrows = ri->ri_rows;
	lcspx_stdscreen.textops = &ri->ri_ops;
	lcspx_stdscreen.fontwidth = ri->ri_font->fontwidth;
	lcspx_stdscreen.fontheight = ri->ri_font->fontheight;
	lcspx_stdscreen.capabilities = ri->ri_caps;

	return (0);
}

int
lcspx_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct lcspx_softc *sc = v;
	struct lcspx_screen *ss = sc->sc_scr;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_cmap *cm;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_LCSPX;
		break;

	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = ss->ss_ri.ri_height;
		wdf->width = ss->ss_ri.ri_width;
		wdf->depth = 8;
		wdf->cmsize = 256;
		break;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = ss->ss_ri.ri_stride;
		break;

	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = lcspx_getcmap(ss, cm);
		if (error != 0)
			return (error);
		break;
	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = lcspx_putcmap(ss, cm);
		if (error != 0)
			return (error);
		lcspx_loadcmap(ss, cm->index, cm->count);
		break;

	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;

	default:
		return (-1);
	}

	return (0);
}

paddr_t
lcspx_mmap(void *v, off_t offset, int prot)
{
	struct lcspx_softc *sc = v;
	struct lcspx_screen *ss = sc->sc_scr;

	if (offset >= ss->ss_fbsize || offset < 0)
		return (-1);

	return (LCSPX_FB_ADDR + offset);
}

int
lcspx_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct lcspx_softc *sc = v;
	struct lcspx_screen *ss = sc->sc_scr;
	struct rasops_info *ri = &ss->ss_ri;

	if (sc->sc_nscreens > 0)
		return (ENOMEM);

	*cookiep = ri;
	*curxp = *curyp = 0;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, defattrp);
	sc->sc_nscreens++;

	return (0);
}

void
lcspx_free_screen(void *v, void *cookie)
{
	struct lcspx_softc *sc = v;

	sc->sc_nscreens--;
}

int
lcspx_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return (0);
}

/*
 * Colormap handling routines
 */

int
lcspx_getcmap(struct lcspx_screen *ss, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count, i;
	int error;
	u_int8_t ramp[256], *c, *r;

	if (index >= 256 || count > 256 - index)
		return (EINVAL);

	/* extract reds */
	c = ss->ss_cmap + 0 + index * 3;
	for (i = count, r = ramp; i != 0; i--)
		*r++ = *c, c += 3;
	if ((error = copyout(ramp, cm->red, count)) != 0)
		return (error);

	/* extract greens */
	c = ss->ss_cmap + 1 + index * 3;
	for (i = count, r = ramp; i != 0; i--)
		*r++ = *c, c += 3;
	if ((error = copyout(ramp, cm->green, count)) != 0)
		return (error);

	/* extract blues */
	c = ss->ss_cmap + 2 + index * 3;
	for (i = count, r = ramp; i != 0; i--)
		*r++ = *c, c += 3;
	if ((error = copyout(ramp, cm->blue, count)) != 0)
		return (error);

	return (0);
}

int
lcspx_putcmap(struct lcspx_screen *ss, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count;
	int i, error;
	u_int8_t r[256], g[256], b[256], *nr, *ng, *nb, *c;

	if (index >= 256 || count > 256 - index)
		return (EINVAL);

	if ((error = copyin(cm->red, r, count)) != 0)
		return (error);
	if ((error = copyin(cm->green, g, count)) != 0)
		return (error);
	if ((error = copyin(cm->blue, b, count)) != 0)
		return (error);

	nr = r, ng = g, nb = b;
	c = ss->ss_cmap + index * 3;
	for (i = count; i != 0; i--) {
		*c++ = *nr++;
		*c++ = *ng++;
		*c++ = *nb++;
	}

	return (0);
}

void
lcspx_loadcmap(struct lcspx_screen *ss, int from, int count)
{
	u_int8_t *cmap = ss->ss_cmap;
	int i;

	cmap += from * 3;
	for (i = from; i < from + count; i++) {
		/*
		 * Reprogram the index every iteration, because the RAMDAC
		 * may not be in autoincrement mode. XXX fix this
		 */
		lcspx_ramdac_wraddr(ss, i);
		*(ss->ss_ramdac[BT463_REG_CMAP_DATA]) = *cmap++;
		*(ss->ss_ramdac[BT463_REG_CMAP_DATA]) = *cmap++;
		*(ss->ss_ramdac[BT463_REG_CMAP_DATA]) = *cmap++;
	}
}

void
lcspx_resetcmap(struct lcspx_screen *ss)
{
	bcopy(rasops_cmap, ss->ss_cmap, sizeof(ss->ss_cmap));
	lcspx_loadcmap(ss, 0, 256);
}

/*
 * Console support code
 */

int	lcspxcnprobe(void);
int	lcspxcninit(void);

int
lcspxcnprobe()
{
	extern vaddr_t virtual_avail;
	volatile struct adder *adder;
	volatile u_int8_t *ch;
	u_short status;
	int rc;

	switch (vax_boardtype) {
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		if ((vax_confdata & KA420_CFG_L3CON) != 0)
			break; /* doesn't use graphics console */

		/* not present on microvaxes */
		if ((vax_confdata & KA420_CFG_MULTU) != 0)
			break;

		if ((vax_confdata & KA420_CFG_VIDOPT) == 0)
			break;

		/*
		 * We can not check for video memory at the SPX address,
		 * because if no SPX board is installed, the probe will
		 * just hang.
		 * Instead, check for a GPX board and skip probe if the
		 * video option really is a GPX.
		 */
		ioaccess(virtual_avail, GPXADDR + GPX_ADDER_OFFSET, 1);
		adder = (volatile struct adder *)virtual_avail;
		adder->status = 0;
		status = adder->status;
		iounaccess(virtual_avail, 1);
		if (status != offsetof(struct adder, status))
			break;

		return (1);

	case VAX_BTYP_49:
		if ((vax_confdata & 8) != 0)
			break; /* doesn't use graphics console */

		if ((vax_confdata & 0x12) != 0x02)
			break;

		return (1);

	case VAX_BTYP_VXT:
		if ((vax_confdata & 2) == 0)
			break; /* doesn't use graphics console */

		/*
		 * Check for video memory at SPX address.
		 */
		rc = 0;
		ioaccess(virtual_avail, LCSPX_FB_ADDR, 1);
		ch = (volatile u_int8_t *)virtual_avail;
		*ch = 0x01;
		if ((*ch & 0x01) != 0) {
			*ch = 0x00;
			if ((*ch & 0x01) == 0)
				rc = 1;
		}
		iounaccess(virtual_avail, 1);

		if (rc == 0)
			break;

		return (1);

	default:
		break;
	}

	return (0);
}

/*
 * Called very early to setup the glass tty as console.
 * Because it's called before the VM system is initialized, virtual memory
 * for the framebuffer can be stolen directly without disturbing anything.
 */
int
lcspxcninit()
{
	struct lcspx_screen *ss = &lcspx_consscr;
	extern vaddr_t virtual_avail;
	int i;
	u_int width, height;
	vaddr_t ova, reg1;
	u_int32_t magic;
	long defattr;
	struct rasops_info *ri;

	ova = virtual_avail;

	switch (vax_boardtype) {
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		/*
		 * XXX EVIL KLUGE ALERT!
		 *
		 * The spx jumper settings do not show up in vax_confdata.
		 * I don't know which spx register sports their values,
		 * but I have noticed that, after the ROM has initialized
		 * the board, bit 0x80 at 39b0011c will reflect the
		 * resolution setting.
		 *
		 * This register is not read-only, so one could DEPOSIT
		 * a bogus value in it from the console before starting
		 * OpenBSD.  If you do this, you deserve to be bitten
		 * if things go wrong.
		 */
		reg1 = virtual_avail;
		ioaccess(virtual_avail, LCSPX_REG1_ADDR, 1);
		magic = *(u_int32_t *)(reg1 + 0x11c);
		iounaccess(virtual_avail, 1);

		if (magic & 0x80) {
			width = 1280;
			height = 1024;
		} else {
			width = 1024;
			height = 864;
		}
		break;
	default:
		width = 1280;
		height = 1024;
		break;
	}

	ss->ss_fbsize = width * height;
	ss->ss_addr = (caddr_t)virtual_avail;
	ioaccess(virtual_avail, LCSPX_FB_ADDR, ss->ss_fbsize / VAX_NBPG);
	virtual_avail += ss->ss_fbsize;

	ss->ss_reg = virtual_avail;
	ioaccess(virtual_avail, LCSPX_REG_ADDR, LCSPX_REG_SIZE / VAX_NBPG);
	virtual_avail += LCSPX_REG_SIZE;

	for (i = 0; i < 4; i++) {
		ss->ss_ramdac[i] = (volatile u_int8_t *)virtual_avail;
		ioaccess(virtual_avail,
		    LCSPX_RAMDAC_ADDR + i * LCSPX_RAMDAC_INTERLEAVE, 1);
		virtual_avail += VAX_NBPG;
	}

	virtual_avail = round_page(virtual_avail);

	/* this had better not fail */
	if (lcspx_setup_screen(ss, width, height) != 0) {
		for (i = 3; i >= 0; i--)
			iounaccess((vaddr_t)ss->ss_ramdac[i], 1);
		iounaccess(ss->ss_reg, LCSPX_REG_SIZE / VAX_NBPG);
		iounaccess((vaddr_t)ss->ss_addr, ss->ss_fbsize / VAX_NBPG);
		virtual_avail = ova;
		return (1);
	}

	ri = &ss->ss_ri;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&lcspx_stdscreen, ri, 0, 0, defattr);

	return (0);
}

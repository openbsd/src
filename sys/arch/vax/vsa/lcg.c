/*	$OpenBSD: lcg.c,v 1.15 2010/12/26 15:41:00 miod Exp $	*/
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
 * Copyright (c) 2003, 2004 Blaz Antonic
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

#include <machine/vsbus.h>
#include <machine/scb.h>
#include <machine/sid.h>
#include <machine/cpu.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <vax/vsa/lcgreg.h>

#define	LCG_CONFIG_ADDR	0x200f0010	/* configuration register */
#define	LCG_REG_ADDR	0x20100000	/* registers */
#define	LCG_REG_SIZE	0x4000
#define	LCG_LUT_ADDR	0x21800000	/* colormap */
#define	LCG_LUT_OFFSET	0x0800
#define	LCG_LUT_SIZE	0x0800
#define	LCG_FB_ADDR	0x21801000	/* frame buffer */

int	lcg_match(struct device *, void *, void *);
void	lcg_attach(struct device *, struct device *, void *);

struct	lcg_screen {
	struct rasops_info ss_ri;
	u_int32_t	ss_cfg;
	u_int		ss_width, ss_height, ss_depth;
	u_int		ss_fbsize;		/* visible part only */
	caddr_t		ss_addr;		/* frame buffer address */
	vaddr_t		ss_reg;
	volatile u_int8_t *ss_lut;
	u_int8_t	ss_cmap[256 * 3];
};

/* for console */
struct lcg_screen lcg_consscr;

struct	lcg_softc {
	struct device sc_dev;
	struct lcg_screen *sc_scr;
	int	sc_nscreens;
};

struct cfattach lcg_ca = {
	sizeof(struct lcg_softc), lcg_match, lcg_attach,
};

struct	cfdriver lcg_cd = {
	NULL, "lcg", DV_DULL
};

struct wsscreen_descr lcg_stdscreen = {
	"std",
};

const struct wsscreen_descr *_lcg_scrlist[] = {
	&lcg_stdscreen,
};

const struct wsscreen_list lcg_screenlist = {
	sizeof(_lcg_scrlist) / sizeof(struct wsscreen_descr *),
	_lcg_scrlist,
};

int	lcg_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	lcg_mmap(void *, off_t, int);
int	lcg_alloc_screen(void *, const struct wsscreen_descr *,
	    void **, int *, int *, long *);
void	lcg_free_screen(void *, void *);
int	lcg_show_screen(void *, void *, int,
	    void (*) (void *, int, int), void *);
void	lcg_burner(void *, u_int, u_int);

const struct wsdisplay_accessops lcg_accessops = {
	lcg_ioctl,
	lcg_mmap,
	lcg_alloc_screen,
	lcg_free_screen,
	lcg_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	lcg_burner
};

int	lcg_alloc_attr(void *, int, int, int, long *);
int	lcg_getcmap(struct lcg_screen *, struct wsdisplay_cmap *);
void	lcg_loadcmap(struct lcg_screen *, int, int);
int	lcg_probe_screen(u_int32_t, u_int *, u_int *);
int	lcg_putcmap(struct lcg_screen *, struct wsdisplay_cmap *);
void	lcg_resetcmap(struct lcg_screen *);
int	lcg_setup_screen(struct lcg_screen *);

#define	lcg_read_reg(ss, regno) \
	*(volatile u_int32_t *)((ss)->ss_reg + (regno))
#define	lcg_write_reg(ss, regno, val) \
	*(volatile u_int32_t *)((ss)->ss_reg + (regno)) = (val)

int
lcg_match(struct device *parent, void *vcf, void *aux)
{
	struct vsbus_softc *sc = (void *)parent;
	struct vsbus_attach_args *va = aux;
	vaddr_t cfgreg;
	int depth;
#ifdef PARANOIA
	int missing;
	volatile u_int8_t *ch;
#endif

	if (va->va_paddr != LCG_REG_ADDR)
		return (0);

	switch (vax_boardtype) {
	default:
		return (0);

	case VAX_BTYP_46:
		if ((vax_confdata & 0x40) == 0)
			return (0);
		break;
	case VAX_BTYP_48:
		/* KA48 can't boot without the frame buffer board */
		break;
	}

	/*
	 * Check the configuration register. The frame buffer might not be
	 * an lcg board.
	 */
	cfgreg = vax_map_physmem(LCG_CONFIG_ADDR, 1);
	depth = lcg_probe_screen(*(volatile u_int32_t *)cfgreg, NULL, NULL);
	vax_unmap_physmem(cfgreg, 1);
	if (depth < 0)	/* no lcg frame buffer */
		return (0);

#ifdef PARANOIA
	/*
	 * Check for video memory.
	 * We can not use badaddr() on these models.
	 */
	missing = 0;
	ch = (volatile u_int8_t *)vax_map_physmem(LCG_FB_ADDR, 1);
	*ch = 0x01;
	if ((*ch & 0x01) == 0)
		missing = 1;
	else {
		*ch = 0x00;
		if ((*ch & 0x01) != 0)
			missing = 1;
	}
	vax_unmap_physmem((vaddr_t)ch, 1);
	if (missing != 0)
		return (0);
#endif

	sc->sc_mask = 0x04;	/* XXX - should be generated */
	scb_fake(0x120, 0x15);
	return (20);
}

void
lcg_attach(struct device *parent, struct device *self, void *aux)
{
	struct lcg_softc *sc = (struct lcg_softc *)self;
	struct lcg_screen *ss;
	struct wsemuldisplaydev_attach_args aa;
	vaddr_t tmp;
	u_int32_t cfg;
	int console;
	extern struct consdev wsdisplay_cons;

	console = (vax_confdata & 0x100) == 0 && cn_tab == &wsdisplay_cons;

	/*
	 * Check for a recognized configuration register.
	 * If we do not recognize it, print it and do not attach - so that
	 * this gets noticed...
	 */
	if (!console) {
		tmp = vax_map_physmem(LCG_CONFIG_ADDR, 1);
		if (tmp == NULL) {
			printf("\n%s: can not map configuration register\n",
			    self->dv_xname);
			return;
		}
		cfg = *(volatile u_int32_t *)tmp;
		vax_unmap_physmem(tmp, 1);

		if (lcg_probe_screen(cfg, NULL, NULL) <= 0) {
			printf("\n%s:"
			    " unrecognized configuration register %08x\n",
			    self->dv_xname, cfg);
			return;
		}
	}

	if (console) {
		ss = &lcg_consscr;
		sc->sc_nscreens = 1;
	} else {
		ss = malloc(sizeof(*ss), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (ss == NULL) {
			printf(": can not allocate memory\n");
			return;
		}

		ss->ss_cfg = cfg;
		ss->ss_depth = lcg_probe_screen(ss->ss_cfg,
		    &ss->ss_width, &ss->ss_height);
		ss->ss_fbsize =
		    roundup(ss->ss_width * ss->ss_height, PAGE_SIZE);

		ss->ss_addr = (caddr_t)vax_map_physmem(LCG_FB_ADDR,
		    ss->ss_fbsize / VAX_NBPG);
		if (ss->ss_addr == NULL) {
			printf(": can not map frame buffer\n");
			goto fail1;
		}

		ss->ss_reg = vax_map_physmem(LCG_REG_ADDR,
		    LCG_REG_SIZE / VAX_NBPG);
		if (ss->ss_reg == 0L) {
			printf(": can not map registers\n");
			goto fail2;
		}

		ss->ss_lut = (volatile u_int8_t *)vax_map_physmem(LCG_LUT_ADDR +
		    LCG_LUT_OFFSET, LCG_LUT_SIZE / VAX_NBPG);
		if (ss->ss_lut == NULL) {
			printf(": can not map color LUT\n");
			goto fail3;
		}

		if (lcg_setup_screen(ss) != 0) {
			printf(": initialization failed\n");
			goto fail4;
		}
	}
	sc->sc_scr = ss;

	printf(": %dx%dx%d frame buffer\n",
	    ss->ss_width, ss->ss_height, ss->ss_depth);

	aa.console = console;
	aa.scrdata = &lcg_screenlist;
	aa.accessops = &lcg_accessops;
	aa.accesscookie = sc;
	aa.defaultscreens = 0;

	config_found(self, &aa, wsemuldisplaydevprint);
	return;

fail4:
	vax_unmap_physmem((vaddr_t)ss->ss_lut, LCG_LUT_SIZE / VAX_NBPG);
fail3:
	vax_unmap_physmem(ss->ss_reg, LCG_REG_SIZE / VAX_NBPG);
fail2:
	vax_unmap_physmem((vaddr_t)ss->ss_addr, ss->ss_fbsize / VAX_NBPG);
fail1:
	free(ss, M_DEVBUF);
}

/*
 * Determine if we have a recognized frame buffer, its resolution and
 * color depth.
 */
int
lcg_probe_screen(u_int32_t cfg, u_int *width, u_int *height)
{
	u_int w, h, d = 8;

	switch (vax_boardtype) {
	case VAX_BTYP_46:
		switch (cfg & 0xf0) {
		case 0x00:
			return (-1);	/* no hardware */
		case 0x20:
		case 0x60:
			w = 1024; h = 864;
			break;
		case 0x40:
			w = 1024; h = 768;
			break;
		case 0x80:
			d = 4;
			/* FALLTHROUGH */
		case 0x90:
		case 0xb0:
			w = 1280; h = 1024;
			break;
		default:
			return (0);	/* unknown configuration, please help */
		}
		break;
	case VAX_BTYP_48:
		switch (cfg & 0x07) {
		case 0x00:
			return (-1);	/* no hardware */
		case 0x05:
			w = 1280; h = 1024;
			break;
		case 0x06:
			if (vax_confdata & 0x80) {
				w = 1024; h = 768;
			} else {
				w = 640; h = 480;
			}
			break;
		case 0x07:
			if (vax_confdata & 0x80) {
				w = 1024; h = 864;
			} else {
				w = 1024; h = 768;
			}
			break;
		default:
			return (0);	/* unknown configuration, please help */
		}
		break;
	}

	if (width != NULL)
		*width = w;
	if (height != NULL)
		*height = h;

	return (d);
}

/*
 * Initialize anything necessary for an emulating wsdisplay to work (i.e.
 * pick a font, initialize a rasops structure, setup the accessops callbacks.)
 */
int
lcg_setup_screen(struct lcg_screen *ss)
{
	struct rasops_info *ri = &ss->ss_ri;

	bzero(ri, sizeof(*ri));
	/*
	 * Since the frame buffer memory is byte addressed, even in low-bpp
	 * mode, initialize a 8bpp rasops engine. We will report a shorter
	 * colormap if necessary, which will allow X to do TRT.
	 */
	ri->ri_depth = 8;
	ri->ri_width = ss->ss_width;
	ri->ri_height = ss->ss_height;
	ri->ri_stride = ss->ss_width;
	ri->ri_flg = RI_CLEAR | RI_CENTER;
	ri->ri_bits = (void *)ss->ss_addr;
	ri->ri_hw = ss;

	/*
	 * Ask for an unholy big display, rasops will trim this to more
	 * reasonable values.
	 */
	if (rasops_init(ri, 160, 160) != 0)
		return (-1);

	if (ss->ss_depth < 8) {
		ri->ri_ops.alloc_attr = lcg_alloc_attr;
		ri->ri_caps &= ~WSSCREEN_HILIT;
	}

	lcg_stdscreen.ncols = ri->ri_cols;
	lcg_stdscreen.nrows = ri->ri_rows;
	lcg_stdscreen.textops = &ri->ri_ops;
	lcg_stdscreen.fontwidth = ri->ri_font->fontwidth;
	lcg_stdscreen.fontheight = ri->ri_font->fontheight;
	lcg_stdscreen.capabilities = ri->ri_caps;

	lcg_resetcmap(ss);

	return (0);
}

int
lcg_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct lcg_softc *sc = v;
	struct lcg_screen *ss = sc->sc_scr;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_cmap *cm;
	int error;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_LCG;
		break;

	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = ss->ss_height;
		wdf->width = ss->ss_width;
		wdf->depth = 8;
		wdf->cmsize = 1 << ss->ss_depth;
		break;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = ss->ss_ri.ri_stride;
		break;

	case WSDISPLAYIO_GETCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = lcg_getcmap(ss, cm);
		if (error != 0)
			return (error);
		break;
	case WSDISPLAYIO_PUTCMAP:
		cm = (struct wsdisplay_cmap *)data;
		error = lcg_putcmap(ss, cm);
		if (error != 0)
			return (error);
		lcg_loadcmap(ss, cm->index, cm->count);
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
lcg_mmap(void *v, off_t offset, int prot)
{
	struct lcg_softc *sc = v;
	struct lcg_screen *ss = sc->sc_scr;

	if (offset >= ss->ss_fbsize || offset < 0)
		return (-1);

	return (LCG_FB_ADDR + offset);
}

int
lcg_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct lcg_softc *sc = v;
	struct lcg_screen *ss = sc->sc_scr;
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
lcg_free_screen(void *v, void *cookie)
{
	struct lcg_softc *sc = v;

	sc->sc_nscreens--;
}

int
lcg_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return (0);
}

void
lcg_burner(void *v, u_int on, u_int flags)
{
	struct lcg_softc *sc = v;
	struct lcg_screen *ss = sc->sc_scr;
	u_int32_t vidcfg;

	vidcfg = lcg_read_reg(ss, LCG_REG_VIDEO_CONFIG);
	if (on)
		vidcfg |= VIDEO_ENABLE_VIDEO | VIDEO_SYNC_ENABLE;
	else {
		vidcfg &= ~VIDEO_ENABLE_VIDEO;
		if (flags & WSDISPLAY_BURN_VBLANK)
			vidcfg &= ~VIDEO_SYNC_ENABLE;
	}
	lcg_write_reg(ss, LCG_REG_VIDEO_CONFIG, vidcfg);
}

/*
 * Attribute allocator for 4bpp frame buffers.
 * In such modes, highlighting is not available.
 */
int
lcg_alloc_attr(void *cookie, int fg, int bg, int flg, long *attr)
{
	extern int rasops_alloc_cattr(void *, int, int, int, long *);

	if ((flg & (WSATTR_BLINK | WSATTR_HILIT)) != 0)
		return (EINVAL);

	return (rasops_alloc_cattr(cookie, fg, bg, flg, attr));
}

/*
 * Colormap handling routines
 */

int
lcg_getcmap(struct lcg_screen *ss, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count, i;
	u_int colcount = 1 << ss->ss_depth;
	int error;
	u_int8_t ramp[256], *c, *r;

	if (index >= colcount || count > colcount - index)
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
lcg_putcmap(struct lcg_screen *ss, struct wsdisplay_cmap *cm)
{
	u_int index = cm->index, count = cm->count;
	u_int colcount = 1 << ss->ss_depth;
	int i, error;
	u_int8_t r[256], g[256], b[256], *nr, *ng, *nb, *c;

	if (index >= colcount || count > colcount - index)
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

/* Fill the given colormap (LUT) entry.  */
#define lcg_set_lut_entry(lutptr, cmap, idx, shift)			\
do {									\
	*(lutptr)++ = LUT_ADRS_REG;					\
	*(lutptr)++ = (idx);						\
	*(lutptr)++ = LUT_COLOR_AUTOINC;				\
	*(lutptr)++ = (*(cmap)++) >> (shift);				\
	*(lutptr)++ = LUT_COLOR_AUTOINC;				\
	*(lutptr)++ = (*(cmap)++) >> (shift);				\
	*(lutptr)++ = LUT_COLOR_AUTOINC;				\
	*(lutptr)++ = (*(cmap)++) >> (shift);				\
} while (0)

void
lcg_loadcmap(struct lcg_screen *ss, int from, int count)
{
	const u_int8_t *cmap;
	u_int i;
	volatile u_int8_t *lutptr;
	u_int32_t vidcfg;

	/* partial updates ignored for now */
	cmap = ss->ss_cmap;
	lutptr = ss->ss_lut;
	if (ss->ss_depth == 8) {
		for (i = 0; i < 256; i++) {
			lcg_set_lut_entry(lutptr, cmap, i, 0);
		}
	} else {
		for (i = 0; i < 16; i++) {
			lcg_set_lut_entry(lutptr, cmap, i, 4);
		}
	}

	/*
	 * Wait for retrace
	 */
	while (((vidcfg = lcg_read_reg(ss, LCG_REG_VIDEO_CONFIG)) &
	    VIDEO_VSTATE) != VIDEO_VSYNC)
		DELAY(1);

	vidcfg &= ~(VIDEO_SHIFT_SEL | VIDEO_MEM_REFRESH_SEL_MASK |
	    VIDEO_LUT_SHIFT_SEL);
	/* Do full loads if width is 1024 or 2048, split loads otherwise. */
	if (ss->ss_width == 1024 || ss->ss_width == 2048)
		vidcfg |= VIDEO_SHIFT_SEL | (1 << VIDEO_MEM_REFRESH_SEL_SHIFT) |
		    VIDEO_LUT_SHIFT_SEL;
	else
		vidcfg |= (2 << VIDEO_MEM_REFRESH_SEL_SHIFT);
	vidcfg |= VIDEO_LUT_LOAD_SIZE;	/* 2KB lut */
	lcg_write_reg(ss, LCG_REG_VIDEO_CONFIG, vidcfg);
	lcg_write_reg(ss, LCG_REG_LUT_CONSOLE_SEL, LUT_SEL_COLOR);
	lcg_write_reg(ss, LCG_REG_LUT_COLOR_BASE_W, LCG_LUT_OFFSET);
	/* Wait for end of retrace */
	while (((vidcfg = lcg_read_reg(ss, LCG_REG_VIDEO_CONFIG)) &
	    VIDEO_VSTATE) == VIDEO_VSYNC)
		DELAY(1);
	lcg_write_reg(ss, LCG_REG_LUT_CONSOLE_SEL, LUT_SEL_CONSOLE);
}

void
lcg_resetcmap(struct lcg_screen *ss)
{
	if (ss->ss_depth == 8)
		bcopy(rasops_cmap, ss->ss_cmap, sizeof(ss->ss_cmap));
	else {
		bcopy(rasops_cmap, ss->ss_cmap, 8 * 3);
		bcopy(rasops_cmap + 0xf8 * 3, ss->ss_cmap + 8 * 3, 8 * 3);
	}
	lcg_loadcmap(ss, 0, 1 << ss->ss_depth);
}

/*
 * Console support code
 */

int	lcgcnprobe(void);
int	lcgcninit(void);

int
lcgcnprobe()
{
	extern vaddr_t virtual_avail;
	u_int32_t cfg;
	vaddr_t tmp;
#ifdef PARANOIA
	volatile u_int8_t *ch;
	int rc;
#endif

	switch (vax_boardtype) {
	case VAX_BTYP_46:
		if ((vax_confdata & 0x40) == 0)
			break;	/* no frame buffer */
		/* FALLTHROUGH */
	case VAX_BTYP_48:
		if ((vax_confdata & 0x100) != 0)
			break; /* doesn't use graphics console */

		tmp = virtual_avail;
		ioaccess(tmp, vax_trunc_page(LCG_CONFIG_ADDR), 1);
		cfg = *(volatile u_int32_t *)
		    (tmp + (LCG_CONFIG_ADDR & VAX_PGOFSET));
		iounaccess(tmp, 1);

		if (lcg_probe_screen(cfg, NULL, NULL) <= 0)
			break;	/* no lcg or unsupported configuration */

#ifdef PARANOIA
		/*
		 * Check for video memory.
		 * We can not use badaddr() on these models.
		 */
		rc = 0;
		ioaccess(tmp, LCG_FB_ADDR, 1);
		ch = (volatile u_int8_t *)tmp;
		*ch = 0x01;
		if ((*ch & 0x01) != 0) {
			*ch = 0x00;
			if ((*ch & 0x01) == 0)
				rc = 1;
		}
		iounaccess(tmp, 1);
		if (rc == 0)
			break;
#endif

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
lcgcninit()
{
	struct lcg_screen *ss = &lcg_consscr;
	extern vaddr_t virtual_avail;
	vaddr_t ova;
	long defattr;
	struct rasops_info *ri;

	ova = virtual_avail;
	ioaccess(virtual_avail, vax_trunc_page(LCG_CONFIG_ADDR), 1);
	ss->ss_cfg = *(volatile u_int32_t *)
	    (virtual_avail + (LCG_CONFIG_ADDR & VAX_PGOFSET));
	iounaccess(virtual_avail, 1);

	ss->ss_depth = lcg_probe_screen(ss->ss_cfg,
	    &ss->ss_width, &ss->ss_height);

	ss->ss_fbsize = roundup(ss->ss_width * ss->ss_height, PAGE_SIZE);

	ss->ss_addr = (caddr_t)virtual_avail;
	ioaccess(virtual_avail, LCG_FB_ADDR, ss->ss_fbsize / VAX_NBPG);
	virtual_avail += ss->ss_fbsize;

	ss->ss_reg = virtual_avail;
	ioaccess(virtual_avail, LCG_REG_ADDR, LCG_REG_SIZE / VAX_NBPG);
	virtual_avail += LCG_REG_SIZE;

	ss->ss_lut = (volatile u_int8_t *)virtual_avail;
	ioaccess(virtual_avail, LCG_LUT_ADDR + LCG_LUT_OFFSET,
	    LCG_LUT_SIZE / VAX_NBPG);
	virtual_avail += LCG_LUT_SIZE;

	virtual_avail = round_page(virtual_avail);

	/* this had better not fail */
	if (lcg_setup_screen(ss) != 0) {
		iounaccess((vaddr_t)ss->ss_lut, LCG_LUT_SIZE / VAX_NBPG);
		iounaccess((vaddr_t)ss->ss_reg, LCG_REG_SIZE / VAX_NBPG);
		iounaccess((vaddr_t)ss->ss_addr, ss->ss_fbsize / VAX_NBPG);
		virtual_avail = ova;
		return (1);
	}

	ri = &ss->ss_ri;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&lcg_stdscreen, ri, 0, 0, defattr);

	return (0);
}

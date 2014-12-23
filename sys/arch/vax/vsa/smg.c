/*	$OpenBSD: smg.c,v 1.28 2014/12/23 21:39:12 miod Exp $	*/
/*	$NetBSD: smg.c,v 1.21 2000/03/23 06:46:44 thorpej Exp $ */
/*
 * Copyright (c) 2006, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * Copyright (c) 1998 Ludd, University of Lule}, Sweden.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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
/*
 * Copyright (c) 1996 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1991 University of Utah.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Mark Davies of the Department of Computer
 * Science, Victoria University of Wellington, New Zealand.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: grf_hy.c 1.2 93/08/13$
 *
 *	@(#)grf_hy.c	8.4 (Berkeley) 1/12/94
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <machine/vsbus.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/ka420.h>
#include <machine/scb.h>

#include <uvm/uvm_extern.h>

#include <dev/cons.h>

#include <dev/ic/dc503reg.h>

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>
#include <vax/dec/dzkbdvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <dev/rasops/rasops_masks.h>

/* Screen hardware defs */
#define SM_XWIDTH	1024
#define SM_YWIDTH	864

#define CUR_XBIAS	216	/* Add to cursor position */
#define CUR_YBIAS	33

int	smg_match(struct device *, void *, void *);
void	smg_attach(struct device *, struct device *, void *);

struct	smg_screen {
	struct rasops_info ss_ri;
	caddr_t		ss_addr;		/* frame buffer address */
	struct dc503reg	*ss_cursor;		/* cursor registers */
	u_int16_t	ss_curcmd;
	struct wsdisplay_curpos ss_curpos, ss_curhot;
	u_int16_t	ss_curimg[PCC_CURSOR_SIZE];
	u_int16_t	ss_curmask[PCC_CURSOR_SIZE];
};

/* for console */
struct smg_screen smg_consscr;

struct	smg_softc {
	struct device sc_dev;
	struct smg_screen *sc_scr;
	int	sc_nscreens;
};

struct cfattach smg_ca = {
	sizeof(struct smg_softc), smg_match, smg_attach,
};

struct	cfdriver smg_cd = {
	NULL, "smg", DV_DULL
};

struct wsscreen_descr smg_stdscreen = {
	"std",
};

const struct wsscreen_descr *_smg_scrlist[] = {
	&smg_stdscreen,
};

const struct wsscreen_list smg_screenlist = {
	sizeof(_smg_scrlist) / sizeof(struct wsscreen_descr *),
	_smg_scrlist,
};

int	smg_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	smg_mmap(void *, off_t, int);
int	smg_alloc_screen(void *, const struct wsscreen_descr *,
	    void **, int *, int *, long *);
void	smg_free_screen(void *, void *);
int	smg_show_screen(void *, void *, int,
	    void (*) (void *, int, int), void *);
int	smg_load_font(void *, void *, struct wsdisplay_font *);
int	smg_list_font(void *, struct wsdisplay_font *);
void	smg_burner(void *, u_int, u_int);

const struct wsdisplay_accessops smg_accessops = {
	.ioctl = smg_ioctl,
	.mmap = smg_mmap,
	.alloc_screen = smg_alloc_screen,
	.free_screen = smg_free_screen,
	.show_screen = smg_show_screen,
	.load_font = smg_load_font,
	.list_font = smg_list_font,
	.burn_screen = smg_burner
};

void	smg_blockmove(struct rasops_info *, u_int, u_int, u_int, u_int, u_int,
	    int);
int	smg_copycols(void *, int, int, int, int);
int	smg_erasecols(void *, int, int, int, long);

int	smg_getcursor(struct smg_screen *, struct wsdisplay_cursor *);
int	smg_setup_screen(struct smg_screen *);
int	smg_setcursor(struct smg_screen *, struct wsdisplay_cursor *);
void	smg_updatecursor(struct smg_screen *, u_int);

int
smg_match(struct device *parent, void *vcf, void *aux)
{
	struct cfdata *cf = vcf;
	struct vsbus_attach_args *va = aux;
	volatile short *curcmd;
	volatile short *cfgtst;
	short tmp, tmp2;
	extern struct consdev wsdisplay_cons;

	switch (vax_boardtype) {
	default:
		return (0);

	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		if (va->va_paddr != KA420_CUR_BASE)
			return (0);

		/* not present on microvaxes */
		if ((vax_confdata & KA420_CFG_MULTU) != 0)
			return (0);

		/*
		 * If the color option board is present, do not attach
		 * unless we are explicitely asked to via device flags.
		 */
		if ((vax_confdata & KA420_CFG_VIDOPT) != 0 &&
		    (cf->cf_flags & 1) == 0)
			return (0);
		break;
	}

	/* when already running as console, always fake things */
	if ((vax_confdata & (KA420_CFG_L3CON | KA420_CFG_VIDOPT)) == 0 &&
	    cn_tab == &wsdisplay_cons) {
		struct vsbus_softc *sc = (void *)parent;
		extern int oldvsbus;

		sc->sc_mask = 0x08;
		scb_fake(0x44, oldvsbus ? 0x14 : 0x15);
		return (20);
	} else {
		/*
		 * Try to find the cursor chip by testing the flip-flop.
		 * If nonexistent, no glass tty.
		 */
		curcmd = (short *)va->va_addr;
		cfgtst = (short *)vax_map_physmem(VS_CFGTST, 1);
		curcmd[0] = PCCCMD_HSHI | PCCCMD_FOPB;
		DELAY(300000);
		tmp = cfgtst[0];
		curcmd[0] = PCCCMD_TEST | PCCCMD_HSHI;
		DELAY(300000);
		tmp2 = cfgtst[0];
		vax_unmap_physmem((vaddr_t)cfgtst, 1);

		if (tmp2 != tmp)
			return (20); /* Using periodic interrupt */
		else
			return (0);
	}
}

void
smg_attach(struct device *parent, struct device *self, void *aux)
{
	struct smg_softc *sc = (struct smg_softc *)self;
	struct smg_screen *scr;
	struct wsemuldisplaydev_attach_args aa;
	int console;
	extern struct consdev wsdisplay_cons;

	console = (vax_confdata & (KA420_CFG_L3CON | KA420_CFG_VIDOPT)) == 0 &&
	    cn_tab == &wsdisplay_cons;
	if (console) {
		scr = &smg_consscr;
		sc->sc_nscreens = 1;
	} else {
		scr = malloc(sizeof(*scr), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (scr == NULL) {
			printf(": can not allocate memory\n");
			return;
		}

		scr->ss_addr =
		    (caddr_t)vax_map_physmem(SMADDR, SMSIZE / VAX_NBPG);
		if (scr->ss_addr == NULL) {
			printf(": can not map frame buffer\n");
			free(scr, M_DEVBUF, sizeof(*scr));
			return;
		}

		scr->ss_cursor =
		    (struct dc503reg *)vax_map_physmem(KA420_CUR_BASE, 1);
		if (scr->ss_cursor == NULL) {
			printf(": can not map cursor chip\n");
			vax_unmap_physmem((vaddr_t)scr->ss_addr,
			    SMSIZE / VAX_NBPG);
			free(scr, M_DEVBUF, sizeof(*scr));
			return;
		}

		if (smg_setup_screen(scr) != 0) {
			printf(": initialization failed\n");
			vax_unmap_physmem((vaddr_t)scr->ss_cursor, 1);
			vax_unmap_physmem((vaddr_t)scr->ss_addr,
			    SMSIZE / VAX_NBPG);
			free(scr, M_DEVBUF, sizeof(*scr));
			return;
		}
	}
	sc->sc_scr = scr;

	printf("\n%s: %dx%d on-board monochrome framebuffer\n",
	    self->dv_xname, SM_XWIDTH, SM_YWIDTH);

	aa.console = console;
	aa.scrdata = &smg_screenlist;
	aa.accessops = &smg_accessops;
	aa.accesscookie = sc;
	aa.defaultscreens = 0;

	config_found(self, &aa, wsemuldisplaydevprint);
}

/*
 * Initialize anything necessary for an emulating wsdisplay to work (i.e.
 * pick a font, initialize a rasops structure, setup the accessops callbacks.)
 */
int
smg_setup_screen(struct smg_screen *ss)
{
	struct rasops_info *ri = &ss->ss_ri;

	bzero(ri, sizeof(*ri));
	ri->ri_depth = 1;
	ri->ri_width = SM_XWIDTH;
	ri->ri_height = SM_YWIDTH;
	ri->ri_stride = SM_XWIDTH >> 3;
	ri->ri_flg = RI_CLEAR | RI_CENTER;
	ri->ri_bits = (void *)ss->ss_addr;
	ri->ri_hw = ss;

	/*
	 * Ask for an unholy big display, rasops will trim this to more
	 * reasonable values.
	 */
	if (rasops_init(ri, 160, 160) != 0)
		return (-1);

	ri->ri_ops.copycols = smg_copycols;
	ri->ri_ops.erasecols = smg_erasecols;

	smg_stdscreen.ncols = ri->ri_cols;
	smg_stdscreen.nrows = ri->ri_rows;
	smg_stdscreen.textops = &ri->ri_ops;
	smg_stdscreen.fontwidth = ri->ri_font->fontwidth;
	smg_stdscreen.fontheight = ri->ri_font->fontheight;
	smg_stdscreen.capabilities = ri->ri_caps;

	ss->ss_curcmd = PCCCMD_HSHI;
	ss->ss_cursor->cmdr = ss->ss_curcmd;

	return (0);
}

int
smg_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct smg_softc *sc = v;
	struct smg_screen *ss = sc->sc_scr;
	struct wsdisplay_fbinfo *wdf;
	struct wsdisplay_curpos *pos;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_VAX_MONO;
		break;

	case WSDISPLAYIO_GINFO:
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = ss->ss_ri.ri_height;
		wdf->width = ss->ss_ri.ri_width;
		wdf->depth = ss->ss_ri.ri_depth;
		wdf->cmsize = 0;
		break;

	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = ss->ss_ri.ri_stride;
		break;

	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;

	case WSDISPLAYIO_GCURPOS:
		pos = (struct wsdisplay_curpos *)data;
		pos->x = ss->ss_curpos.x;
		pos->y = ss->ss_curpos.y;
		break;
	case WSDISPLAYIO_SCURPOS:
		pos = (struct wsdisplay_curpos *)data;
		ss->ss_curpos.x = pos->x;
		ss->ss_curpos.y = pos->y;
		smg_updatecursor(ss, WSDISPLAY_CURSOR_DOPOS);
		break;
	case WSDISPLAYIO_GCURMAX:
		pos = (struct wsdisplay_curpos *)data;
		pos->x = pos->y = PCC_CURSOR_SIZE;
	case WSDISPLAYIO_GCURSOR:
		return (smg_getcursor(ss, (struct wsdisplay_cursor *)data));
	case WSDISPLAYIO_SCURSOR:
		return (smg_setcursor(ss, (struct wsdisplay_cursor *)data));
		break;

	default:
		return (-1);
	}

	return (0);
}

paddr_t
smg_mmap(void *v, off_t offset, int prot)
{
	if (offset >= SMSIZE || offset < 0)
		return (-1);

	return (SMADDR + offset);
}

int
smg_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *defattrp)
{
	struct smg_softc *sc = v;
	struct smg_screen *ss = sc->sc_scr;
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
smg_free_screen(void *v, void *cookie)
{
	struct smg_softc *sc = v;

	sc->sc_nscreens--;
}

int
smg_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	return (0);
}

int
smg_load_font(void *v, void *emulcookie, struct wsdisplay_font *font)
{
	struct smg_softc *sc = v;
	struct smg_screen *ss = sc->sc_scr;
	struct rasops_info *ri = &ss->ss_ri;

	return rasops_load_font(ri, emulcookie, font);
}

int
smg_list_font(void *v, struct wsdisplay_font *font)
{
	struct smg_softc *sc = v;
	struct smg_screen *ss = sc->sc_scr;
	struct rasops_info *ri = &ss->ss_ri;

	return rasops_list_font(ri, font);
}

void
smg_burner(void *v, u_int on, u_int flags)
{
	struct smg_softc *sc = v;
	struct smg_screen *ss = sc->sc_scr;

	ss->ss_cursor->cmdr = on ? ss->ss_curcmd :
	    (ss->ss_curcmd & ~(PCCCMD_FOPA | PCCCMD_ENPA)) | PCCCMD_FOPB;
}

int
smg_getcursor(struct smg_screen *ss, struct wsdisplay_cursor *wdc)
{
	int error;

	if (wdc->which & WSDISPLAY_CURSOR_DOCUR)
		wdc->enable = ss->ss_curcmd & PCCCMD_ENPA ? 1 : 0;
	if (wdc->which & WSDISPLAY_CURSOR_DOPOS) {
		wdc->pos.x = ss->ss_curpos.x;
		wdc->pos.y = ss->ss_curpos.y;
	}
	if (wdc->which & WSDISPLAY_CURSOR_DOHOT) {
		wdc->hot.x = ss->ss_curhot.x;
		wdc->hot.y = ss->ss_curhot.y;
	}
	if (wdc->which & WSDISPLAY_CURSOR_DOCMAP) {
		wdc->cmap.index = 0;
		wdc->cmap.count = 0;
	}
	if (wdc->which & WSDISPLAY_CURSOR_DOSHAPE) {
		wdc->size.x = wdc->size.y = PCC_CURSOR_SIZE;
		error = copyout(ss->ss_curimg, wdc->image,
		    sizeof(ss->ss_curimg));
		if (error != 0)
			return (error);
		error = copyout(ss->ss_curmask, wdc->mask,
		    sizeof(ss->ss_curmask));
		if (error != 0)
			return (error);
	}

	return (0);
}

int
smg_setcursor(struct smg_screen *ss, struct wsdisplay_cursor *wdc)
{
	u_int16_t curfg[PCC_CURSOR_SIZE], curmask[PCC_CURSOR_SIZE];
	int error;

	if (wdc->which & WSDISPLAY_CURSOR_DOCMAP) {
		/* No cursor colormap since we are a B&W device. */
		if (wdc->cmap.count != 0)
			return (EINVAL);
	}

	/*
	 * First, do the userland-kernel data transfers, so that we can fail
	 * if necessary before altering anything.
	 */
	if (wdc->which & WSDISPLAY_CURSOR_DOSHAPE) {
		if (wdc->size.x != PCC_CURSOR_SIZE ||
		    wdc->size.y != PCC_CURSOR_SIZE)
			return (EINVAL);
		error = copyin(wdc->image, curfg, sizeof(curfg));
		if (error != 0)
			return (error);
		error = copyin(wdc->mask, curmask, sizeof(curmask));
		if (error != 0)
			return (error);
	}

	/*
	 * Now update our variables...
	 */
	if (wdc->which & WSDISPLAY_CURSOR_DOCUR) {
		if (wdc->enable)
			ss->ss_curcmd |= PCCCMD_ENPB | PCCCMD_ENPA;
		else
			ss->ss_curcmd &= ~(PCCCMD_ENPB | PCCCMD_ENPA);
	}
	if (wdc->which & WSDISPLAY_CURSOR_DOPOS) {
		ss->ss_curpos.x = wdc->pos.x;
		ss->ss_curpos.y = wdc->pos.y;
	}
	if (wdc->which & WSDISPLAY_CURSOR_DOHOT) {
		ss->ss_curhot.x = wdc->hot.x;
		ss->ss_curhot.y = wdc->hot.y;
	}
	if (wdc->which & WSDISPLAY_CURSOR_DOSHAPE) {
		bcopy(curfg, ss->ss_curimg, sizeof ss->ss_curimg);
		bcopy(curmask, ss->ss_curmask, sizeof ss->ss_curmask);
	}

	/*
	 * ...and update the cursor
	 */
	smg_updatecursor(ss, wdc->which);
	
	return (0);
}

void
smg_updatecursor(struct smg_screen *ss, u_int which)
{
	u_int i;

	if (which & (WSDISPLAY_CURSOR_DOPOS | WSDISPLAY_CURSOR_DOHOT)) {
		ss->ss_cursor->xpos =
		    ss->ss_curpos.x - ss->ss_curhot.x + CUR_XBIAS;
		ss->ss_cursor->ypos =
		    ss->ss_curpos.y - ss->ss_curhot.y + CUR_YBIAS;
	}
	if (which & WSDISPLAY_CURSOR_DOSHAPE) {
		ss->ss_cursor->cmdr = ss->ss_curcmd | PCCCMD_LODSA;
		for (i = 0; i < PCC_CURSOR_SIZE; i++)
			ss->ss_cursor->load = ss->ss_curimg[i];
		for (i = 0; i < PCC_CURSOR_SIZE; i++)
			ss->ss_cursor->load = ss->ss_curmask[i];
		ss->ss_cursor->cmdr = ss->ss_curcmd;
	} else
	if (which & WSDISPLAY_CURSOR_DOCUR)
		ss->ss_cursor->cmdr = ss->ss_curcmd;
}

/*
 * Faster console operations
 */

#include <vax/vsa/maskbits.h>

void
smg_blockmove(struct rasops_info *ri, u_int sx, u_int y, u_int dx, u_int cx,
    u_int cy, int rop)
{
	int width;		/* add to get to same position in next line */

	unsigned int *psrcLine, *pdstLine;
				/* pointers to line with current src and dst */
	unsigned int *psrc;	/* pointer to current src longword */
	unsigned int *pdst;	/* pointer to current dst longword */

				/* following used for looping through a line */
	unsigned int startmask, endmask;  /* masks for writing ends of dst */
	int nlMiddle;		/* whole longwords in dst */
	int nl;			/* temp copy of nlMiddle */
	int xoffSrc;		/* offset (>= 0, < 32) from which to
				   fetch whole longwords fetched in src */
	int nstart;		/* number of ragged bits at start of dst */
	int nend;		/* number of ragged bits at end of dst */
	int srcStartOver;	/* pulling nstart bits from src
				   overflows into the next word? */

	width = SM_XWIDTH >> 5;

	/* start at first scanline */
	psrcLine = pdstLine = ((u_int *)ri->ri_bits) + (y * width);

	/* x direction doesn't matter for < 1 longword */
	if (cx <= 32) {
		int srcBit, dstBit;	/* bit offset of src and dst */

		pdstLine += (dx >> 5);
		psrcLine += (sx >> 5);
		psrc = psrcLine;
		pdst = pdstLine;

		srcBit = sx & 0x1f;
		dstBit = dx & 0x1f;

		while (cy--) {
			getandputrop(psrc, srcBit, dstBit, cx, pdst, rop);
			pdst += width;
			psrc += width;
		}
	} else {
		maskbits(dx, cx, startmask, endmask, nlMiddle);
		if (startmask)
			nstart = 32 - (dx & 0x1f);
		else
			nstart = 0;
		if (endmask)
			nend = (dx + cx) & 0x1f;
		else
			nend = 0;

		xoffSrc = ((sx & 0x1f) + nstart) & 0x1f;
		srcStartOver = ((sx & 0x1f) + nstart) > 31;

		if (sx >= dx) {	/* move left to right */
			pdstLine += (dx >> 5);
			psrcLine += (sx >> 5);

			while (cy--) {
				psrc = psrcLine;
				pdst = pdstLine;

				if (startmask) {
					getandputrop(psrc, (sx & 0x1f),
					    (dx & 0x1f), nstart, pdst, rop);
					pdst++;
					if (srcStartOver)
						psrc++;
				}

				/* special case for aligned operations */
				if (xoffSrc == 0) {
					nl = nlMiddle;
					while (nl--) {
						switch (rop) {
						case RR_CLEAR:
							*pdst = 0;
							break;
						case RR_SET:
							*pdst = ~0;
							break;
						default:
							*pdst = *psrc;
							break;
						}
						psrc++;
						pdst++;
					}
				} else {
					nl = nlMiddle + 1;
					while (--nl) {
						switch (rop) {
						case RR_CLEAR:
							*pdst = 0;
							break;
						case RR_SET:
							*pdst = ~0;
							break;
						default:
							getunalignedword(psrc,
							    xoffSrc, *pdst);
							break;
						}
						pdst++;
						psrc++;
					}
				}

				if (endmask) {
					getandputrop(psrc, xoffSrc, 0, nend,
					    pdst, rop);
				}

				pdstLine += width;
				psrcLine += width;
			}
		} else {	/* move right to left */
			pdstLine += ((dx + cx) >> 5);
			psrcLine += ((sx + cx) >> 5);
			/*
			 * If fetch of last partial bits from source crosses
			 * a longword boundary, start at the previous longword
			 */
			if (xoffSrc + nend >= 32)
				--psrcLine;

			while (cy--) {
				psrc = psrcLine;
				pdst = pdstLine;

				if (endmask) {
					getandputrop(psrc, xoffSrc, 0, nend,
					    pdst, rop);
				}

				nl = nlMiddle + 1;
				while (--nl) {
					--psrc;
					--pdst;
					switch (rop) {
					case RR_CLEAR:
						*pdst = 0;
						break;
					case RR_SET:
						*pdst = ~0;
						break;
					default:
						getunalignedword(psrc, xoffSrc,
						    *pdst);
						break;
					}
				}

				if (startmask) {
					if (srcStartOver)
						--psrc;
					--pdst;
					getandputrop(psrc, (sx & 0x1f),
					    (dx & 0x1f), nstart, pdst, rop);
				}

				pdstLine += width;
				psrcLine += width;
			}
		}
	}
}

int
smg_copycols(void *cookie, int row, int src, int dst, int n)
{
	struct rasops_info *ri = cookie;

	n *= ri->ri_font->fontwidth;
	src *= ri->ri_font->fontwidth;
	dst *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	smg_blockmove(ri, src, row, dst, n, ri->ri_font->fontheight,
	    RR_COPY);

	return 0;
}

int
smg_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	int fg, bg;

	ri->ri_ops.unpack_attr(cookie, attr, &fg, &bg, NULL);

	num *= ri->ri_font->fontwidth;
	col *= ri->ri_font->fontwidth;
	row *= ri->ri_font->fontheight;

	smg_blockmove(ri, col, row, col, num, ri->ri_font->fontheight,
	    bg == 0 ? RR_CLEAR : RR_SET);

	return 0;
}

/*
 * Console support code
 */

int	smgcnprobe(void);
int	smgcninit(void);

int
smgcnprobe()
{
	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		if ((vax_confdata & (KA420_CFG_L3CON | KA420_CFG_MULTU)) != 0)
			break; /* doesn't use graphics console */

		if ((vax_confdata & KA420_CFG_VIDOPT) != 0)
			break;	/* there is a color option */

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
smgcninit()
{
	struct smg_screen *ss = &smg_consscr;
	extern vaddr_t virtual_avail;
	vaddr_t ova;
	long defattr;
	struct rasops_info *ri;

	ova = virtual_avail;

	ss->ss_addr = (caddr_t)virtual_avail;
	ioaccess(virtual_avail, SMADDR, SMSIZE / VAX_NBPG);
	virtual_avail += SMSIZE;

	ss->ss_cursor = (struct dc503reg *)virtual_avail;
	ioaccess(virtual_avail, KA420_CUR_BASE, 1);
	virtual_avail += VAX_NBPG;

	virtual_avail = round_page(virtual_avail);

	/* this had better not fail */
	if (smg_setup_screen(ss) != 0) {
		iounaccess((vaddr_t)ss->ss_addr, SMSIZE / VAX_NBPG);
		iounaccess((vaddr_t)ss->ss_cursor, 1);
		virtual_avail = ova;
		return (1);
	}

	ri = &ss->ss_ri;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&smg_stdscreen, ri, 0, 0, defattr);

	return (0);
}

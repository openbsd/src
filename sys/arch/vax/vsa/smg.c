/*	$OpenBSD: smg.c,v 1.12 2006/07/29 14:18:57 miod Exp $	*/
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

#include <uvm/uvm_extern.h>

#include <dev/ic/dc503reg.h>

#include <vax/qbus/dzreg.h>
#include <vax/qbus/dzvar.h>
#include <vax/dec/dzkbdvar.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>
#include <dev/wsfont/wsfont.h>

#include "dzkbd.h"

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
void	smg_burner(void *, u_int, u_int);

const struct wsdisplay_accessops smg_accessops = {
	smg_ioctl,
	smg_mmap,
	smg_alloc_screen,
	smg_free_screen,
	smg_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	smg_burner
};

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

void
smg_attach(struct device *parent, struct device *self, void *aux)
{
	struct smg_softc *sc = (struct smg_softc *)self;
	struct smg_screen *scr;
	struct wsemuldisplaydev_attach_args aa;
	int console;

	console = (vax_confdata & KA420_CFG_L3CON) == 0;
	if (console) {
		scr = &smg_consscr;
		sc->sc_nscreens = 1;
	} else {
		scr = malloc(sizeof(struct smg_screen), M_DEVBUF, M_NOWAIT);
		if (scr == NULL) {
			printf(": can not allocate memory\n");
			return;
		}
		bzero(scr, sizeof(struct smg_screen));

		scr->ss_addr =
		    (caddr_t)vax_map_physmem(SMADDR, SMSIZE / VAX_NBPG);
		if (scr->ss_addr == NULL) {
			printf(": can not map frame buffer\n");
			free(scr, M_DEVBUF);
			return;
		}

		scr->ss_cursor =
		    (struct dc503reg *)vax_map_physmem(KA420_CUR_BASE, 1);
		if (scr->ss_cursor == NULL) {
			printf(": can not map cursor chip\n");
			vax_unmap_physmem((vaddr_t)scr->ss_addr,
			    SMSIZE / VAX_NBPG);
			free(scr, M_DEVBUF);
			return;
		}

		if (smg_setup_screen(scr) != 0) {
			printf(": initialization failed\n");
			vax_unmap_physmem((vaddr_t)scr->ss_cursor, 1);
			vax_unmap_physmem((vaddr_t)scr->ss_addr,
			    SMSIZE / VAX_NBPG);
			free(scr, M_DEVBUF);
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
	 * We can not let rasops select our font, because we need to use
	 * a font with right-to-left bit order on this frame buffer.
	 */
	wsfont_init();
	if ((ri->ri_wsfcookie = wsfont_find(NULL, 8, 15, 0)) <= 0)
		return (-1);
	if (wsfont_lock(ri->ri_wsfcookie, &ri->ri_font,
	    WSDISPLAY_FONTORDER_R2L, WSDISPLAY_FONTORDER_L2R) <= 0)
		return (-1);

	/*
	 * Ask for an unholy big display, rasops will trim this to more
	 * reasonable values.
	 */
	if (rasops_init(ri, 160, 160) != 0)
		return (-1);

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

	return (SMADDR + offset) >> PGSHIFT;
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
 * Console support code
 */

int	smgcnprobe(void);
void	smgcninit(void);

int
smgcnprobe()
{
	switch (vax_boardtype) {
	case VAX_BTYP_410:
	case VAX_BTYP_420:
	case VAX_BTYP_43:
		if ((vax_confdata & (KA420_CFG_L3CON | KA420_CFG_MULTU)) != 0)
			break; /* doesn't use graphics console */

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
void
smgcninit()
{
	struct smg_screen *ss = &smg_consscr;
	extern vaddr_t virtual_avail;
	long defattr;
	struct rasops_info *ri;

	ss->ss_addr = (caddr_t)virtual_avail;
	virtual_avail += SMSIZE;
	ioaccess((vaddr_t)ss->ss_addr, SMADDR, SMSIZE / VAX_NBPG);

	ss->ss_cursor = (struct dc503reg *)virtual_avail;
	virtual_avail += VAX_NBPG;
	ioaccess((vaddr_t)ss->ss_cursor, KA420_CUR_BASE, 1);

	virtual_avail = round_page(virtual_avail);

	/* this had better not fail as we can't recover there */
	if (smg_setup_screen(ss) != 0)
		panic(__func__);

	ri = &ss->ss_ri;
	ri->ri_ops.alloc_attr(ri, 0, 0, 0, &defattr);
	wsdisplay_cnattach(&smg_stdscreen, ri, 0, 0, defattr);
}

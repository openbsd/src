/*	$OpenBSD: sti.c,v 1.1 1998/12/31 03:20:44 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#define STIDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/iomod.h>
#include <machine/autoconf.h>

#include <dev/wscons/wsconsvar.h>
#include <dev/wscons/wscons_emul.h>

#include <hppa/dev/stireg.h>

#include <hppa/dev/cpudevs.h>

struct sti_softc {
	struct device sc_dev;

	u_int sc_flags;
	u_int sc_devtype;
	u_int sc_regs;
	u_int sc_rom;

	int sc_attr;
	struct sti_config sti_config;
	struct sti_fontcfg sti_fontcfg;
	vm_offset_t sc_code;			/* code region allocated */
	void	(*sti_init)   __P((struct sti_initflags *,
				   struct sti_initin *,
				   struct sti_initout *,
				   struct sti_config *));
	void	(*sti_state)  __P((void));
	void	(*sti_font)   __P((struct sti_fontflags *,
				   struct sti_fontin *,
				   struct sti_fontout *,
				   struct sti_config *));
	void	(*sti_bmove)  __P((struct sti_moveflags *,
				   struct sti_movein *,
				   struct sti_moveout *,
				   struct sti_config *));
	void	(*sti_test)   __P((void));
	void	(*sti_fault)  __P((void));
	void	(*sti_inqcfg) __P((struct sti_inquireflags *,
				   struct sti_inquirein *,
				   struct sti_inquireout *,
				   struct sti_config *));
};

/* sti_init() flags */
#define	STI_TEXTMODE	0x01
#define	STI_CLEARSCR	0x02

enum sti_bmove_funcs {
	bmf_clear, bmf_copy, bmf_invert
};

int	stiprobe __P((struct device *, void *, void *));
void	stiattach __P((struct device *, struct device *, void *));

struct cfattach sti_ca = {
	sizeof(struct sti_softc), stiprobe, stiattach
};

struct cfdriver sti_cd = {
	NULL, "sti", DV_DULL
};

void	sti_cursor __P((void *, int, int, int));
void	sti_putstr __P((void *, int, int, char *, int));
void	sti_copycols __P((void *, int, int, int, int));
void	sti_erasecols __P((void *, int, int, int));
void	sti_copyrows __P((void *, int, int, int));
void	sti_eraserows __P((void *, int, int));
void	sti_set_attr __P((void *, int));

struct wscons_emulfuncs	sti_emulfuncs = {
	sti_cursor,
	sti_putstr,
	sti_copycols,
	sti_erasecols,
	sti_copyrows,
	sti_eraserows,
	sti_set_attr
};

u_int stiload __P((vm_offset_t dst, vm_offset_t scode,
		   vm_offset_t ecode, int t));
int sti_init __P((struct sti_softc *sc, int mode));
int sti_inqcfg __P((struct sti_softc *sc, struct sti_inquireout *out));
void sti_bmove __P((struct sti_softc *sc, int, int, int, int, int, int,
		    enum sti_bmove_funcs));
int sti_print __P((void *aux, const char *pnp));
int stiioctl __P((void *v, u_long cmd, caddr_t data, int flag, struct proc *));
int stimmap __P((void *v, off_t offset, int prot));

int
stiprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	register struct confargs *ca = aux;
	u_int rom;
	u_char devtype;
	int rv = 0;

	if (ca->ca_type.iodc_type != HPPA_TYPE_FIO ||
	    (ca->ca_type.iodc_sv_model != HPPA_FIO_GSGC &&
	     ca->ca_type.iodc_sv_model != HPPA_FIO_SGC))
		return 0;

	/*
	 * Locate STI ROM.
	 * On some machines it may not be part of the HPA space.
	 */
	if (!PAGE0->pd_resv2[1])
		rom = ca->ca_hpa;
	else
		rom = PAGE0->pd_resv2[1];

	devtype = STI_DEVTYP(STI_TYPE_BWGRF, rom);
	if ((ca->ca_type.iodc_sv_model == HPPA_FIO_SGC &&
	     STI_ID_HI(STI_TYPE_BWGRF, ca->ca_hpa) == STI_ID_FDDI) ||
	    (devtype != STI_TYPE_BWGRF && devtype != STI_TYPE_WWGRF)) {
#ifdef DEBUG
		printf("sti: not a graphics device (:%x)\n", devtype);
#endif
	} else
		rv = 1;

	return rv;
}

int
sti_init(sc, mode)
	struct sti_softc *sc;
	int mode;
{
	struct sti_initflags flags;
	struct sti_initin input;
	struct sti_initout output;

	bzero(&flags,  sizeof(flags));
	bzero(&input,  sizeof(input));
	bzero(&output, sizeof(output));

	flags.wait = 1;
	flags.hardreset = 0;
	flags.clear = 0;
	flags.cmap_black = 1;
	flags.bus_error_timer = 1;
	if (mode & STI_TEXTMODE) {
		flags.texton = 1;
		flags.no_change_bet = 1;
		flags.no_change_bei = 1;
		flags.init_text_cmap = 1;
	}
	input.text_planes = 1;
	sc->sti_init(&flags, &input, &output, &sc->sti_config);
	return (output.text_planes != input.text_planes || output.errno);
}

int
sti_inqcfg(sc, out)
	struct sti_softc *sc;
	struct sti_inquireout *out;
{
	struct sti_inquireflags flags;
	struct sti_inquirein input;

	bzero(&flags,  sizeof(flags));
	bzero(&input,  sizeof(input));
	bzero(out, sizeof(*out));

	flags.wait = 1;
	sc->sti_inqcfg(&flags, &input, out, &sc->sti_config);

	return out->errno;
}

void
sti_bmove(sc, x1, y1, x2, y2, h, w, f)
	struct sti_softc *sc;
	int x1, y1, x2, y2, h, w;
	enum sti_bmove_funcs f;
{
	struct sti_moveflags flags;
	struct sti_movein input;
	struct sti_moveout output;

	bzero(&flags,  sizeof(flags));
	bzero(&input,  sizeof(input));
	bzero(&output, sizeof(output));

	flags.wait = 1;
	switch (f) {
	case bmf_clear:
		flags.clear = 1;
		input.bg_color = 0;
		break;
	case bmf_copy:
		input.fg_color = 1;
		input.bg_color = 0;
		break;
	case bmf_invert:
		input.fg_color = 0;
		input.bg_color = 1;
		break;
	}
	input.src_x = x1;
	input.src_y = y1;
	input.dest_x = x2;
	input.dest_y = y2;
	input.wheight = h;
	input.wwidth = w;

	sc->sti_bmove(&flags, &input, &output, &sc->sti_config);
#ifdef STIDEBUG
	if (output.errno)
		printf ("%s: sti_bmove returned %d\n",
			sc->sc_dev.dv_xname, output.errno);
#endif
}

void
stiattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wscons_attach_args waa;
	struct sti_inquireout cfg;
	register struct sti_softc *sc = (void *)self;
	register struct confargs *ca = aux;
	register struct wscons_odev_spec *wo;
	register u_int dt, addr;
	int error;

	sc->sc_regs = ca->ca_hpa;
	if (!PAGE0->pd_resv2[1])
		sc->sc_rom = ca->ca_hpa;
	else
		sc->sc_rom = PAGE0->pd_resv2[1];

	dt = sc->sc_devtype = STI_DEVTYP(STI_TYPE_BWGRF, sc->sc_rom);
#if 0
	sc->sti_init = (void *)STI_IGADDR(dt, sc->sc_rom);
	sc->sti_state = (void *)STI_SMADDR(dt, sc->sc_rom);
	sc->sti_font = (void *)STI_FUADDR(dt, sc->sc_rom);
	sc->sti_bmove = (void *)STI_BMADDR(dt, sc->sc_rom);
	sc->sti_test = (void *)STI_STADDR(dt, sc->sc_rom);
	sc->sti_fault = (void *)STI_EHADDR(dt, sc->sc_rom);
	sc->sti_inqcfg = (void *)STI_ICADDR(dt, sc->sc_rom);
#else
	{
		u_int t;
		t = STI_EADDR(dt, sc->sc_rom) - STI_IGADDR(dt, sc->sc_rom);
		t = hppa_round_page(t);
		if (!(sc->sc_code = kmem_malloc(kmem_map, t, 0))) {
			printf(": cannot allocate %d bytes for code\n", t);
			return;
		} else
			pmap_protect(kmem_map->pmap, sc->sc_code,
				     sc->sc_code + t, VM_PROT_ALL);

		t = 0;
		sc->sti_init = (void *)sc->sc_code;
		t += stiload(sc->sc_code + t, STI_IGADDR(dt, sc->sc_rom),
			     STI_SMADDR(dt, sc->sc_rom), dt);
		sc->sti_state = (void *)(sc->sc_code + t);
		t += stiload(sc->sc_code + t, STI_SMADDR(dt, sc->sc_rom),
			     STI_FUADDR(dt, sc->sc_rom), dt);
		sc->sti_font = (void *)(sc->sc_code + t);
		t += stiload(sc->sc_code + t, STI_FUADDR(dt, sc->sc_rom),
			     STI_BMADDR(dt, sc->sc_rom), dt);
		sc->sti_bmove = (void *)(sc->sc_code + t);
		t += stiload(sc->sc_code + t, STI_BMADDR(dt, sc->sc_rom),
			     STI_STADDR(dt, sc->sc_rom), dt);
		sc->sti_test = (void *)(sc->sc_code + t);
		t += stiload(sc->sc_code + t, STI_STADDR(dt, sc->sc_rom),
			     STI_EHADDR(dt, sc->sc_rom), dt);
		sc->sti_fault = (void *)(sc->sc_code + t);
		t += stiload(sc->sc_code + t, STI_EHADDR(dt, sc->sc_rom),
			     STI_ICADDR(dt, sc->sc_rom), dt);
		sc->sti_inqcfg = (void *)(sc->sc_code + t);
		t += stiload(sc->sc_code + t, STI_ICADDR(dt, sc->sc_rom),
			     STI_EADDR(dt, sc->sc_rom), dt);
		fcacheall();
	}
#endif

	/* fill out sti_config */
	bzero(&sc->sti_config, sizeof(sc->sti_config));
	{
		register u_int *p;
		register u_int *q = (u_int *)STI_MMAP(dt, sc->sc_rom);

		for (p = sc->sti_config.regions;
		     p < &sc->sti_config.regions[STI_REGIONS]; p++) {
			struct sti_region r;
			if (dt == STI_TYPE_BWGRF) {
				/* we know that sti_region is 4 bytes */
				*((u_char *)&r + 0) = *q++;
				*((u_char *)&r + 1) = *q++;
				*((u_char *)&r + 2) = *q++;
				*((u_char *)&r + 3) = *q++;
			} else
				*(u_int *)&r = *q++;

			*p = ((p == sc->sti_config.regions)? sc->sc_rom :
			      sc->sc_regs) + (r.offset << PGSHIFT);

			if (r.last)
				break;
#ifdef STIDEBUG
			printf("@0x%05x, sys %d, cache %d, btlb %d, len %d\n",
			       r.offset, r.sysonly, r.cache, r.btlb, r.length);
#endif
		}
	}


	if ((error = sti_init(sc, 0))) {
		printf (": can not initialize (%d)\n", error);
		return;
	}

	if ((error = sti_inqcfg(sc, &cfg))) {
		printf (": error %d inquiring config\n", error);
		return;
	}

	if ((error = sti_init(sc, STI_TEXTMODE))) {
		printf (": can not initialize (%d)\n", error);
		return;
	}

	/* fill out sti_fontcfg */
	addr = STI_FONTAD(dt, sc->sc_rom) & ~3;
	sc->sti_fontcfg.firstc   = STIF_FIRSTC (dt, addr);
	sc->sti_fontcfg.lastc    = STIF_LASTC  (dt, addr);
	sc->sti_fontcfg.ftheight = STIF_FHEIGHT(dt, addr);
	sc->sti_fontcfg.ftwidth  = STIF_FWIDTH (dt, addr);
	sc->sti_fontcfg.ftype    = STIF_FTYPE  (dt, addr);
	sc->sti_fontcfg.bpc      = STIF_BPC    (dt, addr);
	sc->sti_fontcfg.uheight  = STIF_UHEIGHT(dt, addr);
	sc->sti_fontcfg.uoffset  - STIF_UOFFSET(dt, addr);

	printf (": %s rev %d.%d\n"
		"%s: %dx%d frame buffer, %dx%dx%d display, offset %dx%d\n"
		"%s: %dx%d font type %d, %d bpc, charset %d-%d\n",
		cfg.devname, STI_GLOREV(dt, sc->sc_rom) >> 4,
		STI_GLOREV(dt, sc->sc_rom) & 0xf,
		sc->sc_dev.dv_xname, cfg.fbwidth, cfg.fbheight,
		cfg.dwidth, cfg.dheight, cfg.bpp, cfg.owidth, cfg.oheight,
		sc->sc_dev.dv_xname,
		sc->sti_fontcfg.ftwidth, sc->sti_fontcfg.ftheight,
		sc->sti_fontcfg.ftype, sc->sti_fontcfg.bpc,
		sc->sti_fontcfg.firstc, sc->sti_fontcfg.lastc);

	/* attach WSCONS */
	wo = &waa.waa_odev_spec;

	wo->wo_emulfuncs = &sti_emulfuncs;
	wo->wo_emulfuncs_cookie = sc;

	wo->wo_ioctl = stiioctl;
	wo->wo_mmap = stimmap;
	wo->wo_miscfuncs_cookie = sc;

	wo->wo_nrows = 64;
	wo->wo_ncols = 80;
	wo->wo_crow = 64;
	wo->wo_ccol = 0;

	config_found(parent, &waa, sti_print);

	return;
}

u_int
stiload(dst, scode, ecode, t)
	vm_offset_t dst, scode, ecode;
	int t;
{
	vm_offset_t sdst = dst;

#ifdef STIDEBUG
	printf("stiload(%x, %x, %x, %d)\n", dst, scode, ecode, t);
#endif
	while (scode < ecode) {
		if (t == STI_TYPE_BWGRF)
			*((u_char *)dst)++ = *(u_char *)scode;
		else
			*((u_int *)dst)++ = *(u_int *)scode;
		scode += sizeof(u_int);
	}

	return dst - sdst;
}

int
sti_print(aux, pnp)
	void *aux;
	const char *pnp;
{
	if (pnp)
		printf("wscons at %s", pnp);
	return UNCONF;
}

int
stiioctl(v, cmd, data, flag, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	/* register struct sti_softc *sc; */

	return -1;
}

int
stimmap(v, offset, prot)
	void *v;
	off_t offset;
	int prot;
{
	/* XXX not finished */
	return offset;
}

void
sti_cursor(v, on, row, col)
	void *v;
	int on, row, col;
{
	register struct sti_softc *sc = v;

	sti_bmove(sc, row * sc->sti_fontcfg.ftheight, col * sc->sti_fontcfg.ftwidth,
		  row * sc->sti_fontcfg.ftheight, col * sc->sti_fontcfg.ftwidth,
		  sc->sti_fontcfg.ftwidth, sc->sti_fontcfg.ftheight, bmf_invert);
}

void
sti_putstr(v, row, col, cp, len)
	void *v;
	int row, col;
	char *cp;
	int len;
{
	register struct sti_softc *sc = v;
	struct sti_fontflags flags;
	struct sti_fontin input;
	struct sti_fontout output;

	bzero(&flags,  sizeof(flags));
	bzero(&input,  sizeof(input));
	bzero(&output, sizeof(output));

	flags.wait = 1;
	/* XXX does not handle text attributes */
	input.fg_color = 1;
	input.bg_color = 0;
	input.dest_x = col * sc->sti_fontcfg.ftwidth;
	input.dest_y = row * sc->sti_fontcfg.ftheight;
	input.startaddr = STI_FONTAD(sc->sc_devtype, sc->sc_rom);
	for (; *cp; cp++, input.dest_x += sc->sti_fontcfg.ftwidth) {
		input.index = *cp;
		sc->sti_font(&flags, &input, &output, &sc->sti_config);
	}
}

void
sti_copycols(v, row, srccol, dstcol, ncols)
	void *v;
	int row, srccol, dstcol, ncols;
{
	register struct sti_softc *sc = v;

	sti_bmove(sc,
		  row * sc->sti_fontcfg.ftheight, srccol * sc->sti_fontcfg.ftwidth,
		  row * sc->sti_fontcfg.ftheight, dstcol * sc->sti_fontcfg.ftwidth,
		  ncols * sc->sti_fontcfg.ftwidth, sc->sti_fontcfg.ftheight,
		  bmf_copy);
}

void
sti_erasecols(v, row, startcol, ncols)
	void *v;
	int row, startcol, ncols;
{
	register struct sti_softc *sc = v;

	sti_bmove(sc,
		  row * sc->sti_fontcfg.ftheight, startcol * sc->sti_fontcfg.ftwidth,
		  row * sc->sti_fontcfg.ftheight, startcol * sc->sti_fontcfg.ftwidth,
		  ncols * sc->sti_fontcfg.ftwidth, sc->sti_fontcfg.ftheight,
		  bmf_clear);
}

void
sti_copyrows(v, srcrow, dstrow, nrows)
	void *v;
	int srcrow, dstrow, nrows;
{
	register struct sti_softc *sc = v;

	sti_bmove(sc,
		  srcrow * sc->sti_fontcfg.ftheight, 0,
		  dstrow * sc->sti_fontcfg.ftheight, 0,
		  sc->sti_config.fbwidth, nrows + sc->sti_fontcfg.ftheight,
		  bmf_copy);
}

void
sti_eraserows(v, srcrow, nrows)
	void *v;
	int srcrow, nrows;
{
	register struct sti_softc *sc = v;

	sti_bmove(sc,
		  srcrow * sc->sti_fontcfg.ftheight, 0,
		  srcrow * sc->sti_fontcfg.ftheight, 0,
		  sc->sti_config.fbwidth, nrows + sc->sti_fontcfg.ftheight,
		  bmf_clear);
}

void
sti_set_attr(v, val)
	void *v;
	int val;
{
	register struct sti_softc *sc = v;

	sc->sc_attr = val;
}


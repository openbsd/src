/*	$OpenBSD: creator.c,v 1.5 2002/05/21 18:49:00 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/timeout.h>

#include <machine/bus.h>
#include <machine/autoconf.h>
#include <machine/openfirm.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/wscons/wscons_raster.h>
#include <dev/rasops/rasops.h>

/* Number of register sets */
#define	FFB_NREGS		24

/* Register set numbers */
#define	FFB_REG_PROM		0
#define	FFB_REG_DAC		1
#define	FFB_REG_FBC		2
#define	FFB_REG_DFB8R		3
#define	FFB_REG_DFB8G		4
#define	FFB_REG_DFB8B		5
#define	FFB_REG_DFB8X		6
#define	FFB_REG_DFB24		7
#define	FFB_REG_DFB32		8
#define	FFB_REG_SFB8R		9
#define	FFB_REG_SFB8G		10
#define	FFB_REG_SFB8B		11
#define	FFB_REG_SFB8X		12
#define	FFB_REG_SFB32		13
#define	FFB_REG_SFB64		14
#define	FFB_REG_DFB422A		15
#define	FFB_REG_DFB422AD	16
#define	FFB_REG_DFB24B		17
#define	FFB_REG_DFB422B		18
#define	FFB_REG_DFB422BD	19
#define	FFB_REG_SFB8Z		20
#define	FFB_REG_SFB16Z		21
#define	FFB_REG_SFB422		22
#define	FFB_REG_SFB422D		23

struct wsscreen_descr creator_stdscreen = {
	"std",
	0, 0,	/* will be filled in -- XXX shouldn't, it's global. */
	0,
	0, 0,
	WSSCREEN_REVERSE | WSSCREEN_WSCOLORS
};

const struct wsscreen_descr *creator_scrlist[] = {
	&creator_stdscreen,
	/* XXX other formats? */
};

struct wsscreen_list creator_screenlist = {
	sizeof(creator_scrlist) / sizeof(struct wsscreen_descr *), creator_scrlist
};

int creator_ioctl(void *, u_long, caddr_t, int, struct proc *);
int creator_alloc_screen(void *, const struct wsscreen_descr *, void **,
    int *, int *, long *);
void creator_free_screen(void *, void *);
int creator_show_screen(void *, void *, int, void (*cb)(void *, int, int),
    void *);
paddr_t creator_mmap(void *, off_t, int);
static int a2int(char *, int);

struct wsdisplay_accessops creator_accessops = {
	creator_ioctl,
	creator_mmap,
	creator_alloc_screen,
	creator_free_screen,
	creator_show_screen,
	NULL,	/* load_font */
	NULL,	/* scrollback */
	NULL,	/* getchar */
	NULL,	/* burner */
};

struct creator_softc {
	struct device sc_dv;
	bus_space_tag_t sc_bt;
	bus_space_handle_t sc_regs[FFB_NREGS];
	bus_addr_t sc_addrs[FFB_NREGS];
	bus_size_t sc_sizes[FFB_NREGS];
	int sc_height, sc_width, sc_linebytes, sc_depth;
	int sc_nscreens, sc_nreg;
	struct rasops_info sc_rasops;
};

int	creator_match(struct device *, void *, void *);
void	creator_attach(struct device *, struct device *, void *);

struct cfattach creator_ca = {
	sizeof(struct creator_softc), creator_match, creator_attach
};

struct cfdriver creator_cd = {
	NULL, "creator", DV_DULL
};

int
creator_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct mainbus_attach_args *ma = aux;

	if (strcmp(ma->ma_name, "SUNW,ffb") == 0)
		return (1);
	return (0);
}

void
creator_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct creator_softc *sc = (struct creator_softc *)self;
	struct mainbus_attach_args *ma = aux;
	struct wsemuldisplaydev_attach_args waa;
	long defattr;
	extern int fbnode;
	int i, btype, console;

	sc->sc_bt = ma->ma_bustag;
	if (ma->ma_nreg < FFB_NREGS) {
		printf(": expected %d regs, got %d\n", FFB_NREGS, ma->ma_nreg);
		goto fail;
	}

	printf(":");

	btype = getpropint(ma->ma_node, "board_type", 0);
	if ((btype & 7) == 3)
		printf(" Creator3D");
	else
		printf(" Creator");

	printf("\n");

	for (i = 0; i < ma->ma_nreg; i++) {
		if (bus_space_map2(sc->sc_bt, 0,
		    ma->ma_reg[i].ur_paddr, ma->ma_reg[i].ur_len,
		    0, NULL, &sc->sc_regs[i])) {
			printf(": failed to map register set %d\n", i);
			goto fail;
		}
		sc->sc_addrs[i] = ma->ma_reg[i].ur_paddr;
		sc->sc_sizes[i] = ma->ma_reg[i].ur_len;
		sc->sc_nreg = i + 1;
	}

	console = (fbnode == ma->ma_node);

	sc->sc_depth = 32;
	sc->sc_linebytes = 8192;
	sc->sc_height = getpropint(ma->ma_node, "height", 0);
	sc->sc_width = getpropint(ma->ma_node, "width", 0);

	sc->sc_rasops.ri_depth = sc->sc_depth;
	sc->sc_rasops.ri_stride = sc->sc_linebytes;
	sc->sc_rasops.ri_flg = RI_CENTER;
	sc->sc_rasops.ri_bits = (void *)bus_space_vaddr(sc->sc_bt,
	    sc->sc_regs[FFB_REG_DFB32]);
	sc->sc_rasops.ri_width = sc->sc_width;
	sc->sc_rasops.ri_height = sc->sc_height;
	sc->sc_rasops.ri_hw = sc;

	rasops_init(&sc->sc_rasops,
	    a2int(getpropstring(optionsnode, "screen-#rows"), 34),
	    a2int(getpropstring(optionsnode, "screen-#columns"), 80));

	creator_stdscreen.nrows = sc->sc_rasops.ri_rows;
	creator_stdscreen.ncols = sc->sc_rasops.ri_cols;
	creator_stdscreen.textops = &sc->sc_rasops.ri_ops;
	sc->sc_rasops.ri_ops.alloc_attr(&sc->sc_rasops, 0, 0, 0, &defattr);

	if (console) {
		int *ccolp, *crowp;

		if (romgetcursoraddr(&crowp, &ccolp))
			ccolp = crowp = NULL;
		if (ccolp != NULL)
			sc->sc_rasops.ri_ccol = *ccolp;
		if (crowp != NULL)
			sc->sc_rasops.ri_crow = *crowp;

		wsdisplay_cnattach(&creator_stdscreen, &sc->sc_rasops,
		    sc->sc_rasops.ri_ccol, sc->sc_rasops.ri_crow, defattr);
	}

	waa.console = console;
	waa.scrdata = &creator_screenlist;
	waa.accessops = &creator_accessops;
	waa.accesscookie = sc;
	config_found(self, &waa, wsemuldisplaydevprint);

	return;

fail:
	for (i = 0; i < sc->sc_nreg; i++)
		if (sc->sc_regs[i] != 0)
			bus_space_unmap(sc->sc_bt, sc->sc_regs[i],
			    ma->ma_reg[i].ur_len);
}

int
creator_ioctl(v, cmd, data, flags, p)
	void *v;
	u_long cmd;
	caddr_t data;
	int flags;
	struct proc *p;
{
	struct creator_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_UNKNOWN;
		break;
	case WSDISPLAYIO_GINFO:
		wdf = (void *)data;
		wdf->height = sc->sc_height;
		wdf->width  = sc->sc_width;
		wdf->depth  = sc->sc_depth;
		wdf->cmsize = 256;/*XXX*/
		break;
	case WSDISPLAYIO_LINEBYTES:
		*(u_int *)data = sc->sc_linebytes;
		break;

	case WSDISPLAYIO_GETCMAP:
		break;/* XXX */

	case WSDISPLAYIO_PUTCMAP:
		break;/* XXX */

	case WSDISPLAYIO_SVIDEO:
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
	default:
		return -1; /* not supported yet */
        }

	return (0);
}

int
creator_alloc_screen(v, type, cookiep, curxp, curyp, attrp)
	void *v;
	const struct wsscreen_descr *type;
	void **cookiep;
	int *curxp, *curyp;
	long *attrp;
{
	struct creator_softc *sc = v;

	if (sc->sc_nscreens > 0)
		return (ENOMEM);

	*cookiep = &sc->sc_rasops;
	*curyp = 0;
	*curxp = 0;
	sc->sc_rasops.ri_ops.alloc_attr(&sc->sc_rasops, 0, 0, 0, attrp);
	sc->sc_nscreens++;
	return (0);
}

void
creator_free_screen(v, cookie)
	void *v;
	void *cookie;
{
	struct creator_softc *sc = v;

	sc->sc_nscreens--;
}

int
creator_show_screen(v, cookie, waitok, cb, cbarg)
	void *v;
	void *cookie;
	int waitok;
	void (*cb)(void *, int, int);
	void *cbarg;
{
	return (0);
}

paddr_t
creator_mmap(vsc, off, prot)
	void *vsc;
	off_t off;
	int prot;
{
	struct creator_softc *sc = vsc;
	int i;

	for (i = 0; i < sc->sc_nreg; i++) {
		/* Before this set? */
		if (off < sc->sc_addrs[i])
			continue;
		/* After this set? */
		if (off >= (sc->sc_addrs[i] + sc->sc_sizes[i]))
			continue;

		return (bus_space_mmap(sc->sc_bt, sc->sc_addrs[i],
		    off - sc->sc_addrs[i], prot, BUS_SPACE_MAP_LINEAR));
	}

	return (-1);
}

static int
a2int(char *cp, int deflt)
{
	int i = 0;

	if (*cp == '\0')
		return (deflt);
	while (*cp != '\0')
		i = i * 10 + *cp++ - '0';
	return (i);
}

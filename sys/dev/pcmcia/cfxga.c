/*	$OpenBSD: cfxga.c,v 1.7 2006/11/27 11:50:02 miod Exp $	*/

/*
 * Copyright (c) 2005, 2006, Matthieu Herrb and Miodrag Vallat
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHORS DISCLAIM ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Display driver for the Colorgraphic CompactFlash ``VoyagerVGA'' card.
 * based upon the Epson S1D13806 graphics chip.
 *
 * Our goals are:
 * - to provide a somewhat usable emulation mode for extra text display.
 * - to let an application (such as an X server) map the controller registers
 *   in order to do its own display game.
 *
 * Driving this card is somewhat a challenge since:
 * - its video memory is not directly accessible.
 * - no operation can make use of DMA.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>

#include <dev/pcmcia/pcmciavar.h>
#include <dev/pcmcia/pcmciareg.h>

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wsdisplayvar.h>
#include <dev/rasops/rasops.h>

#include <dev/pcmcia/cfxgareg.h>

/*
#define CFXGADEBUG
*/

#ifdef CFXGADEBUG
#define	DPRINTF(arg) printf arg
#else
#define	DPRINTF(arg)
#endif

struct cfxga_screen;

struct cfxga_softc {
	struct device sc_dev;
	struct pcmcia_function *sc_pf;
	int	sc_state;
#define	CS_MAPPED	0x0001
#define	CS_RESET	0x0002

	struct pcmcia_mem_handle sc_pmemh;
	int sc_memwin;
	bus_addr_t sc_offset;

	int sc_mode;

	int sc_nscreens;
	LIST_HEAD(, cfxga_screen) sc_scr;
	struct cfxga_screen *sc_active;

	/* wsdisplay glue */
	struct wsscreen_descr sc_wsd;
	struct wsscreen_list sc_wsl;
	struct wsscreen_descr *sc_scrlist[1];
	struct wsdisplay_emulops sc_ops;
	struct device *sc_wsdisplay;
};

int	cfxga_match(struct device *, void *,  void *);
void	cfxga_attach(struct device *, struct device *, void *);
int	cfxga_detach(struct device *, int);
int	cfxga_activate(struct device *, enum devact);

struct cfattach cfxga_ca = {
	sizeof(struct cfxga_softc), cfxga_match, cfxga_attach,
	cfxga_detach, cfxga_activate
};

struct cfdriver cfxga_cd = {
	NULL, "cfxga", DV_DULL
};

int	cfxga_alloc_screen(void *, const struct wsscreen_descr *, void **,
	    int *, int *, long *);
void	cfxga_burner(void *, u_int, u_int);
void	cfxga_free_screen(void *, void *);
int	cfxga_ioctl(void *, u_long, caddr_t, int, struct proc *);
paddr_t	cfxga_mmap(void *, off_t, int);
int	cfxga_show_screen(void *, void *, int, void (*)(void *, int, int),
	    void *);

struct wsdisplay_accessops cfxga_accessops = {
	cfxga_ioctl,
	cfxga_mmap,
	cfxga_alloc_screen,
	cfxga_free_screen,
	cfxga_show_screen,
	NULL,
	NULL,
	NULL,
	cfxga_burner
};

/*
 * Per-screen structure
 */

struct cfxga_screen {
	LIST_ENTRY(cfxga_screen) scr_link;

	/* parent reference */
	struct cfxga_softc *scr_sc;

	/* raster op glue */
	struct rasops_info scr_ri;
	struct wsdisplay_emulops scr_ops;		/* old ri_ops */

	/* backing memory */
	u_int8_t *scr_mem;
};
	
void	cfxga_copycols(void *, int, int, int, int);
void	cfxga_copyrows(void *, int, int, int);
void	cfxga_do_cursor(struct rasops_info *);
void	cfxga_erasecols(void *, int, int, int, long);
void	cfxga_eraserows(void *, int, int, long);
void	cfxga_putchar(void *, int, int, u_int, long);

int	cfxga_install_function(struct pcmcia_function *);
void	cfxga_remove_function(struct pcmcia_function *);

int	cfxga_expand_char(struct cfxga_softc *, struct cfxga_screen *, u_int,
	    int, int, long);
int	cfxga_memory_rop(struct cfxga_softc *, struct cfxga_screen *, u_int,
	    int, int, int, int);
void	cfxga_reset_video(struct cfxga_softc *);
void	cfxga_reset_and_repaint(struct cfxga_softc *);
int	cfxga_standalone_rop(struct cfxga_softc *, u_int, u_int,
	    int, int, int, int, int32_t);
int	cfxga_synchronize(struct cfxga_softc *);
u_int	cfxga_wait(struct cfxga_softc *, u_int, u_int);

#define	cfxga_clear_screen(sc) \
	cfxga_standalone_rop(sc, OP_SOLID_FILL, 0, 0, 0, 640, 480, 0)
#define	cfxga_repaint_screen(sc) \
	cfxga_memory_rop(sc, sc->sc_active, ROP_SRC, 0, 0, 640, 480)

#define	cfxga_read_1(sc, addr) \
	bus_space_read_1((sc)->sc_pmemh.memt, (sc)->sc_pmemh.memh, \
	    (sc)->sc_offset + (addr))
#define	cfxga_read_2(sc, addr) \
	bus_space_read_2((sc)->sc_pmemh.memt, (sc)->sc_pmemh.memh, \
	    (sc)->sc_offset + (addr))
#define	cfxga_write_1(sc, addr, val) \
	bus_space_write_1((sc)->sc_pmemh.memt, (sc)->sc_pmemh.memh, \
	    (sc)->sc_offset + (addr), (val))
#define	cfxga_write_2(sc, addr, val) \
	bus_space_write_2((sc)->sc_pmemh.memt, (sc)->sc_pmemh.memh, \
	    (sc)->sc_offset + (addr), (val))

#define	cfxga_stop_memory_blt(sc) \
	(void)cfxga_read_2(sc, CFREG_BITBLT_DATA)

/*
 * This card is very poorly engineered, specificationwise. It does not
 * provide any CIS information, and has no vendor/product numbers as
 * well: as such, there is no easy way to differentiate it from any
 * other cheapo PCMCIA card.
 *
 * The best we can do is probe for a chip ID. This is not perfect but better
 * than matching blindly. Of course this requires us to play some nasty games
 * behind the PCMCIA framework to be able to do this probe, and correctly fail
 * if this is not the card we are looking for.
 *
 * In shorter words: some card designers ought to be shot, as a service
 * to the community.
 */

/*
 * Create the necessary pcmcia function structures to alleviate the lack
 * of any CIS information on this device.
 * Actually, we hijack the fake function created by the pcmcia framework.
 */
int
cfxga_install_function(struct pcmcia_function *pf)
{
	struct pcmcia_config_entry *cfe;

	/* Get real. */
	pf->pf_flags &= ~PFF_FAKE;

	/* Tell the pcmcia framework where the CCR is. */
	pf->ccr_base = 0x800;
	pf->ccr_mask = 0x67;

	/* Create a simple cfe. */
	cfe = (struct pcmcia_config_entry *)malloc(sizeof *cfe,
	    M_DEVBUF, M_NOWAIT);
	if (cfe == NULL) {
		DPRINTF(("%s: cfe allocation failed\n", __func__));
		return (ENOMEM);
	}

	bzero(cfe, sizeof *cfe);
	cfe->number = 42;	/* have to put some value... */
	cfe->flags = PCMCIA_CFE_IO16;
	cfe->iftype = PCMCIA_IFTYPE_MEMORY;

	SIMPLEQ_INSERT_TAIL(&pf->cfe_head, cfe, cfe_list);

	pcmcia_function_init(pf, cfe);
	return (0);
}

/*
 * Undo the changes done above.
 * Such a function is necessary since we need a full-blown pcmcia world
 * set up in order to do the device probe, but if we don't match the card,
 * leaving this state will cause trouble during other probes.
 */
void
cfxga_remove_function(struct pcmcia_function *pf)
{
	struct pcmcia_config_entry *cfe;

	/* we are the first and only entry... */
	cfe = SIMPLEQ_FIRST(&pf->cfe_head);
	SIMPLEQ_REMOVE_HEAD(&pf->cfe_head, cfe_list);
	free(cfe, M_DEVBUF);

	/* And we're a figment of the kernel's imagination again. */
	pf->pf_flags |= PFF_FAKE;
}

int 
cfxga_match(struct device *parent, void *match, void *aux)
{
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_function *pf = pa->pf;
	struct pcmcia_mem_handle h;
	int rc;
	int win;
	bus_addr_t ptr;
	u_int8_t id = 0;

	if (pa->product != PCMCIA_PRODUCT_INVALID ||
	    pa->manufacturer != PCMCIA_VENDOR_INVALID)
		return (0);

	/* Only a card with no CIS will have a fake function... */
	if ((pf->pf_flags & PFF_FAKE) == 0)
		return (0);

	if (cfxga_install_function(pf) != 0)
		return (0);

	if (pcmcia_function_enable(pf) != 0) {
		DPRINTF(("%s: function enable failed\n"));
		return (0);
	}

	rc = pcmcia_mem_alloc(pf, CFXGA_MEM_RANGE, &h);
	if (rc != 0)
		goto out;

	rc = pcmcia_mem_map(pf, PCMCIA_MEM_ATTR, 0, CFXGA_MEM_RANGE,
	    &h, &ptr, &win);
	if (rc != 0)
		goto out2;

	id = (bus_space_read_1(h.memt, h.memh, ptr + CFREG_REV) &
	    CR_PRODUCT_MASK) >> CR_PRODUCT_SHIFT;

	pcmcia_mem_unmap(pa->pf, win);
out2:
	pcmcia_mem_free(pa->pf, &h);
out:
	pcmcia_function_disable(pf);
	cfxga_remove_function(pf);

	/*
	 * Be sure to return a value greater than pccom's if we match,
	 * otherwise it can win due to the way config(8) will order devices...
	 */
	return (id == PRODUCT_S1D13806 ? 10 : 0);
}

int
cfxga_activate(struct device *dev, enum devact act)
{
	struct cfxga_softc *sc = (void *)dev;

	switch (act) {
	case DVACT_ACTIVATE:
		if (pcmcia_function_enable(sc->sc_pf) != 0) {
			printf("%s: function enable failed\n",
			    sc->sc_dev.dv_xname);
		} else {
			cfxga_reset_and_repaint(sc);
		}
		break;
	case DVACT_DEACTIVATE:
		pcmcia_function_disable(sc->sc_pf);
		break;
	}
	return (0);
}

void 
cfxga_attach(struct device *parent, struct device *self, void *aux)
{
	struct cfxga_softc *sc = (void *)self;
	struct pcmcia_attach_args *pa = aux;
	struct pcmcia_function *pf = pa->pf;
	struct wsemuldisplaydev_attach_args waa;

	LIST_INIT(&sc->sc_scr);
	sc->sc_nscreens = 0;
	sc->sc_pf = pf;

	if (cfxga_install_function(pf) != 0) {
		printf(": pcmcia function setup failed\n");
		return;
	}

	if (pcmcia_function_enable(pf)) {
		printf(": function enable failed\n");
		return;
	}

	if (pcmcia_mem_alloc(pf, CFXGA_MEM_RANGE, &sc->sc_pmemh) != 0) {
		printf(": can't allocate memory space\n");
		return;
	}

	if (pcmcia_mem_map(pf, PCMCIA_MEM_ATTR, 0, CFXGA_MEM_RANGE,
	    &sc->sc_pmemh, &sc->sc_offset, &sc->sc_memwin) != 0) {
		printf(": can't map frame buffer registers\n");
		pcmcia_mem_free(pf, &sc->sc_pmemh);
		return;
	}

	SET(sc->sc_state, CS_MAPPED);

	printf("\n");

	sc->sc_mode = WSDISPLAYIO_MODE_EMUL;

	cfxga_reset_video(sc);

	strlcpy(sc->sc_wsd.name, "std", sizeof(sc->sc_wsd.name));
	sc->sc_wsd.textops = &sc->sc_ops;

	sc->sc_scrlist[0] = &sc->sc_wsd;
	sc->sc_wsl.nscreens = 1;
	sc->sc_wsl.screens = (const struct wsscreen_descr **)sc->sc_scrlist;

	waa.console = 0;
	waa.scrdata = &sc->sc_wsl;
	waa.accessops = &cfxga_accessops;
	waa.accesscookie = sc;
	waa.defaultscreens = 1;

	if ((sc->sc_wsdisplay =
	    config_found(self, &waa, wsemuldisplaydevprint)) == NULL)
		cfxga_clear_screen(sc);	/* otherwise wscons will do this */
}

int
cfxga_detach(struct device *dev, int flags)
{
	struct cfxga_softc *sc = (void *)dev;

	/*
	 * Detach all children, and hope wsdisplay detach code is correct...
	 */
	if (sc->sc_wsdisplay != NULL) {
		config_detach(sc->sc_wsdisplay, DETACH_FORCE);
		/* sc->sc_wsdisplay = NULL; */
	}

	if (ISSET(sc->sc_state, CS_MAPPED)) {
		pcmcia_mem_unmap(sc->sc_pf, sc->sc_memwin);
		pcmcia_mem_free(sc->sc_pf, &sc->sc_pmemh);
		/* CLR(sc->sc_state, CS_MAPPED); */
	}

	return (0);
}

/*
 * Wscons operations
 */

int
cfxga_alloc_screen(void *v, const struct wsscreen_descr *type, void **cookiep,
    int *curxp, int *curyp, long *attrp)
{
	struct cfxga_softc *sc = v;
	struct cfxga_screen *scr;
	struct rasops_info *ri;

	scr = malloc(sizeof *scr, M_DEVBUF, cold ? M_NOWAIT : M_WAITOK);
	if (scr == NULL)
		return (ENOMEM);
	bzero(scr, sizeof *scr);

	scr->scr_mem = malloc(640 * 480 * 16 / 8, M_DEVBUF,
	    cold ? M_NOWAIT : M_WAITOK);
	if (scr->scr_mem == NULL) {
		free(scr, M_DEVBUF);
		return (ENOMEM);
	}
	bzero(scr->scr_mem, 640 * 480 * 16 / 8);

	ri = &scr->scr_ri;
	ri->ri_hw = (void *)scr;
	ri->ri_bits = scr->scr_mem;
	ri->ri_depth = 16;
	ri->ri_width = 640;
	ri->ri_height = 480;
	ri->ri_stride = 640 * 16 / 8;
	/* ri->ri_flg = RI_FULLCLEAR; */

	/* swap B and R at 16 bpp */
	ri->ri_rnum = 5;
	ri->ri_rpos = 11;
	ri->ri_gnum = 6;
	ri->ri_gpos = 5;
	ri->ri_bnum = 5;
	ri->ri_bpos = 0;

	if (sc->sc_wsd.nrows == 0)
		rasops_init(ri, 100, 100);
	else
		rasops_init(ri, sc->sc_wsd.nrows, sc->sc_wsd.ncols);

	scr->scr_ops = ri->ri_ops;
	ri->ri_ops.copycols = cfxga_copycols;
	ri->ri_ops.copyrows = cfxga_copyrows;
	ri->ri_ops.erasecols = cfxga_erasecols;
	ri->ri_ops.eraserows = cfxga_eraserows;
	ri->ri_ops.putchar = cfxga_putchar;
	ri->ri_do_cursor = cfxga_do_cursor;

	if (sc->sc_wsd.nrows == 0) {
		sc->sc_wsd.nrows = ri->ri_rows;
		sc->sc_wsd.ncols = ri->ri_cols;
		bcopy(&ri->ri_ops, &sc->sc_ops, sizeof(sc->sc_ops));
		sc->sc_wsd.fontwidth = ri->ri_font->fontwidth;
		sc->sc_wsd.fontheight = ri->ri_font->fontheight;
		sc->sc_wsd.capabilities = ri->ri_caps;
	}

	scr->scr_sc = sc;
	LIST_INSERT_HEAD(&sc->sc_scr, scr, scr_link);
	sc->sc_nscreens++;

	ri->ri_ops.alloc_attr(ri, 0, 0, 0, attrp);

	*cookiep = ri;
	*curxp = *curyp = 0;
	
	return (0);
}

void
cfxga_burner(void *v, u_int on, u_int flags)
{
	struct cfxga_softc *sc = (void *)v;
	u_int8_t mode;

	mode = cfxga_read_1(sc, CFREG_MODE) & LCD_MODE_SWIVEL_BIT_0;

	if (on)
		cfxga_write_1(sc, CFREG_MODE, mode | MODE_CRT);
	else
		cfxga_write_1(sc, CFREG_MODE, mode | MODE_NO_DISPLAY);
}

void
cfxga_free_screen(void *v, void *cookie)
{
	struct cfxga_softc *sc = v;
	struct rasops_info *ri = cookie;
	struct cfxga_screen *scr = ri->ri_hw;

	LIST_REMOVE(scr, scr_link);
	sc->sc_nscreens--;

	if (scr == sc->sc_active) {
		sc->sc_active = NULL;
		cfxga_burner(sc, 0, 0);
	}

	free(scr->scr_mem, M_DEVBUF);
	free(scr, M_DEVBUF);
}

int
cfxga_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct cfxga_softc *sc = v;
	struct wsdisplay_fbinfo *wdf;
	int mode;

	switch (cmd) {
	case WSDISPLAYIO_GTYPE:
		*(u_int *)data = WSDISPLAY_TYPE_CFXGA;
		break;

	case WSDISPLAYIO_GINFO:
		/* it's not worth using sc->sc_active->scr_ri fields... */
		wdf = (struct wsdisplay_fbinfo *)data;
		wdf->height = 640;
		wdf->width = 480;
		wdf->depth = 16;
		wdf->cmsize = 0;
		break;

	case WSDISPLAYIO_SMODE:
		mode = *(u_int *)data;
		if (mode == sc->sc_mode)
			break;
		switch (mode) {
		case WSDISPLAYIO_MODE_EMUL:
			cfxga_reset_and_repaint(sc);
			break;
		case WSDISPLAYIO_MODE_MAPPED:
			break;
		default:
			return (EINVAL);
		}
		sc->sc_mode = mode;
		break;

	/* these operations are handled by the wscons code... */
	case WSDISPLAYIO_GVIDEO:
	case WSDISPLAYIO_SVIDEO:
		break;

	/* these operations are not supported... */
	case WSDISPLAYIO_GETCMAP:
	case WSDISPLAYIO_PUTCMAP:
	case WSDISPLAYIO_LINEBYTES:
	case WSDISPLAYIO_GCURPOS:
	case WSDISPLAYIO_SCURPOS:
	case WSDISPLAYIO_GCURMAX:
	case WSDISPLAYIO_GCURSOR:
	case WSDISPLAYIO_SCURSOR:
	default:
		return (-1);
	}

	return (0);
}

paddr_t
cfxga_mmap(void *v, off_t off, int prot)
{
	return (-1);
}

int
cfxga_show_screen(void *v, void *cookie, int waitok,
    void (*cb)(void *, int, int), void *cbarg)
{
	struct cfxga_softc *sc = v;
	struct rasops_info *ri = cookie;
	struct cfxga_screen *scr = ri->ri_hw, *old;

	old = sc->sc_active;
	if (old == scr)
		return (0);

	sc->sc_active = scr;
	cfxga_repaint_screen(sc);

	/* turn back video on as well if necessary... */
	if (old == NULL)
		cfxga_burner(sc, 1, 0);

	return (0);
}

/*
 * Real frame buffer operations
 */

void
cfxga_reset_video(struct cfxga_softc *sc)
{
	/*
	 * Reset controller
	 */

	/* need to write to both REV and MISC at the same time */
	cfxga_write_2(sc, CFREG_REV, 0x80 | (CM_REGSEL << 8));
	delay(25000);	/* maintain reset for a short while */
	/* need to write to both REV and MISC at the same time */
	cfxga_write_2(sc, CFREG_REV, 0 | (CM_MEMSEL << 8));
	delay(25000);
	/* stop any pending blt operation */
	cfxga_write_2(sc, CFREG_BITBLT_CONTROL, 0);
	cfxga_stop_memory_blt(sc);

	/*
	 * Setup video mode.
	 */

	cfxga_write_2(sc, CFREG_MEMCLK, MEMCLK_SRC_CLK3);
	cfxga_write_1(sc, CFREG_LCD_PCLK, LCD_PCLK_SRC_CLKI | LCD_PCLK_DIV_1);
	cfxga_write_1(sc, CFREG_MPLUG_CLK,
	    MPLUG_PCLK_SRC_CLKI2 | MPLUG_PCLK_DIV_1);
	/* MEMCNF and DRAM_RFRSH need to be programmed at the same time */
	cfxga_write_2(sc, CFREG_MEMCNF,
	    MEMCNF_SDRAM_INIT | (DRAM_RFRSH_50MHZ << 8));
	cfxga_write_2(sc, CFREG_DRAM_TIMING, DRAM_TIMING_50MHZ);

	cfxga_write_2(sc, CFREG_CRT_START_LOW, 0);
	cfxga_write_1(sc, CFREG_CRT_START_HIGH, 0);
	cfxga_write_1(sc, CFREG_CRT_PANNING, PIXEL_PANNING_MASK_15BPP);
	cfxga_write_1(sc, CFREG_CRT_FIFO_THRESHOLD_HIGH, 0);
	cfxga_write_1(sc, CFREG_CRT_FIFO_THRESHOLD_LOW, 0);

	cfxga_write_1(sc, CFREG_CRT_CURSOR_CONTROL, CURSOR_INACTIVE);

	cfxga_write_1(sc, CFREG_POWER_CONF, 0);
	cfxga_write_1(sc, CFREG_WATCHDOG, 0);

	/* 640x480x72x16 specific values */
	cfxga_write_1(sc, CFREG_CRTTV_PCLK, CRT_PCLK_SRC_CLKI | CRT_PCLK_DIV_1);
	cfxga_write_1(sc, CFREG_WSTATE, WSTATE_MCLK);
	cfxga_write_1(sc, CFREG_CRT_HWIDTH, (640 / 8) - 1);
	/* HNDISP and HSTART need to be programmed at the same time */
	cfxga_write_2(sc, CFREG_CRT_HNDISP, 23 | (2 << 8));
	cfxga_write_1(sc, CFREG_CRT_HPULSE, 4);
	cfxga_write_2(sc, CFREG_CRT_VHEIGHT, 480 - 1);
	/* VNDISP and VSTART need to be programmed at the same time */
	cfxga_write_2(sc, CFREG_CRT_VNDISP, 39 | (8 << 8));
	cfxga_write_1(sc, CFREG_CRT_VPULSE, 2);
	cfxga_write_1(sc, CFREG_TV_CONTROL,
	    TV_LUMINANCE_FILTER | TV_SVIDEO_OUTPUT);
	cfxga_write_1(sc, CFREG_CRT_MODE, CRT_MODE_16BPP);
	cfxga_write_2(sc, CFREG_CRT_MEMORY, 640);

	cfxga_write_1(sc, CFREG_MODE, MODE_CRT);
	delay(25000);
}

void
cfxga_reset_and_repaint(struct cfxga_softc *sc)
{
	cfxga_reset_video(sc);

	if (sc->sc_active != NULL)
		cfxga_repaint_screen(sc);
	else
		cfxga_clear_screen(sc);
}

/*
 * Wait for the blitter to be in a given state.
 */
u_int
cfxga_wait(struct cfxga_softc *sc, u_int mask, u_int result)
{
	u_int tries;

	for (tries = 10000; tries != 0; tries--) {
		if ((cfxga_read_1(sc, CFREG_BITBLT_CONTROL) & mask) == result)
			break;
		delay(10);
	}

	return (tries);
}

/*
 * Wait for all pending blitter operations to be complete.
 * Returns non-zero if the blitter got stuck.
 */
int
cfxga_synchronize(struct cfxga_softc *sc)
{
	/* Wait for previous operations to complete */
	if (cfxga_wait(sc, BITBLT_ACTIVE, 0) == 0) {
		DPRINTF(("%s: not ready\n", __func__));
		if (ISSET(sc->sc_state, CS_RESET))
			return (EAGAIN);
		else {
			DPRINTF(("%s: resetting...\n", sc->sc_dev.dv_xname));
			SET(sc->sc_state, CS_RESET);
			cfxga_reset_and_repaint(sc);
			CLR(sc->sc_state, CS_RESET);
		}
	}
	cfxga_stop_memory_blt(sc);
	return (0);
}

/*
 * Display a character.
 */
int
cfxga_expand_char(struct cfxga_softc *sc, struct cfxga_screen *scr, u_int uc,
    int x, int y, long attr)
{
	struct rasops_info *ri = &scr->scr_ri;
	struct wsdisplay_font *font = ri->ri_font;
	u_int pos, sts, fifo_avail, chunk;
	u_int8_t *fontbits;
	int bg, fg, ul;
	u_int i;
	int rc;

	pos = (y * 640 + x) * (16 / 8);
	fontbits = (u_int8_t *)(font->data + (uc - font->firstchar) *
	    ri->ri_fontscale);
	rasops_unpack_attr(attr, &fg, &bg, &ul);

	/* Wait for previous operations to complete */
	if ((rc = cfxga_synchronize(sc)) != 0)
		return (rc);

	cfxga_write_2(sc, CFREG_COLOR_EXPANSION,
	    ((font->fontwidth - 1) & 7) | (OP_COLOR_EXPANSION << 8));
	cfxga_write_2(sc, CFREG_BITBLT_SRC_LOW, font->fontwidth <= 8 ? 0 : 1);
	cfxga_write_2(sc, CFREG_BITBLT_SRC_HIGH, 0);
	cfxga_write_2(sc, CFREG_BITBLT_DST_LOW, pos);
	cfxga_write_2(sc, CFREG_BITBLT_DST_HIGH, pos >> 16);
	cfxga_write_2(sc, CFREG_BITBLT_OFFSET, 640);
	cfxga_write_2(sc, CFREG_BITBLT_WIDTH, font->fontwidth - 1);
	cfxga_write_2(sc, CFREG_BITBLT_HEIGHT, font->fontheight - 1);
	cfxga_write_2(sc, CFREG_BITBLT_FG, ri->ri_devcmap[fg]);
	cfxga_write_2(sc, CFREG_BITBLT_BG, ri->ri_devcmap[bg]);
	cfxga_write_2(sc, CFREG_BITBLT_CONTROL,
	    BITBLT_ACTIVE | BITBLT_COLOR_16);

	if (cfxga_wait(sc, BITBLT_ACTIVE, BITBLT_ACTIVE) == 0)
		goto fail;	/* unlikely */
	fifo_avail = 0;

	for (i = font->fontheight; i != 0; i--) {
		/*
		 * Find out how much words we can feed before
		 * a FIFO check is needed.
		 */
		if (fifo_avail == 0) {
			sts = cfxga_read_1(sc, CFREG_BITBLT_CONTROL);
			if ((sts & BITBLT_FIFO_NOT_EMPTY) == 0)
				fifo_avail = font->fontwidth <= 8 ? 2 : 1;
			else if ((sts & BITBLT_FIFO_HALF_FULL) == 0)
				fifo_avail = font->fontwidth <= 8 ? 1 : 0;
			else {
				/*
				 * Let the cheap breathe for a short while.
				 * If this is not enough to free some FIFO
				 * entries, abort the operation.
				 */
				if (cfxga_wait(sc, BITBLT_FIFO_FULL, 0) == 0)
					goto fail;
			}
		}

		if (font->fontwidth <= 8) {
			chunk = *fontbits;
			if (ul && i == 1)
				chunk = 0xff;
		} else {
			chunk = *(u_int16_t *)fontbits;
			if (ul && i == 1)
				chunk = 0xffff;
		}
		cfxga_write_2(sc, CFREG_BITBLT_DATA, chunk);
		fontbits += font->stride;
		fifo_avail--;
	}

	return (0);

fail:
	DPRINTF(("%s: abort\n", __func__));
	cfxga_write_2(sc, CFREG_BITBLT_CONTROL, 0);
	cfxga_stop_memory_blt(sc);
	return (EINTR);
}

/*
 * Copy a memory bitmap to the frame buffer.
 *
 * This is slow - we only use this to repaint the whole frame buffer on
 * screen switches.
 */
int
cfxga_memory_rop(struct cfxga_softc *sc, struct cfxga_screen *scr, u_int rop,
    int x, int y, int cx, int cy)
{
	u_int pos, sts, fifo_avail;
	u_int16_t *data;
	int rc;

	pos = (y * 640 + x) * (16 / 8);
	data = (u_int16_t *)(scr->scr_mem + pos);

	/* Wait for previous operations to complete */
	if ((rc = cfxga_synchronize(sc)) != 0)
		return (rc);

	cfxga_write_2(sc, CFREG_BITBLT_ROP, rop | (OP_WRITE_ROP << 8));
	cfxga_write_2(sc, CFREG_BITBLT_SRC_LOW, 0);
	cfxga_write_2(sc, CFREG_BITBLT_SRC_HIGH, 0);
	cfxga_write_2(sc, CFREG_BITBLT_DST_LOW, pos);
	cfxga_write_2(sc, CFREG_BITBLT_DST_HIGH, pos >> 16);
	cfxga_write_2(sc, CFREG_BITBLT_OFFSET, 640);
	cfxga_write_2(sc, CFREG_BITBLT_WIDTH, cx - 1);
	cfxga_write_2(sc, CFREG_BITBLT_HEIGHT, cy - 1);
	cfxga_write_2(sc, CFREG_BITBLT_CONTROL,
	    BITBLT_ACTIVE | BITBLT_COLOR_16);

	if (cfxga_wait(sc, BITBLT_ACTIVE, BITBLT_ACTIVE) == 0)
		goto fail;	/* unlikely */
	fifo_avail = 0;
	while (cy-- != 0) {
		for (x = 0; x < cx; x++) {
			/*
			 * Find out how much words we can feed before
			 * a FIFO check is needed.
			 */
			if (fifo_avail == 0) {
				sts = cfxga_read_1(sc, CFREG_BITBLT_CONTROL);
				if ((sts & BITBLT_FIFO_NOT_EMPTY) == 0)
					fifo_avail = 16;
				else if ((sts & BITBLT_FIFO_HALF_FULL) == 0)
					fifo_avail = 8;
				else if ((sts & BITBLT_FIFO_FULL) == 0) {
					/* pessimistic but safe choice */
					fifo_avail = 1;
				} else {
					/*
					 * Let the cheap breathe for a short
					 * while. If this is not enough to
					 * free some FIFO entries,
					 * abort the operation.
					 */
					if (cfxga_wait(sc, BITBLT_FIFO_FULL,
					    0) == 0)
						goto fail;
				}
			}

			cfxga_write_2(sc, CFREG_BITBLT_DATA, *data++);
			fifo_avail--;
		}
		data += (640 - cx);
	}

	return (0);

fail:
	DPRINTF(("%s: abort\n", __func__));
	cfxga_write_2(sc, CFREG_BITBLT_CONTROL, 0);
	cfxga_stop_memory_blt(sc);
	return (EINTR);
}

/*
 * Perform an internal frame buffer operation.
 */
int
cfxga_standalone_rop(struct cfxga_softc *sc, u_int op, u_int rop, int x, int y,
    int cx, int cy, int32_t srccolor)
{
	u_int pos;
	int rc;

	pos = (y * 640 + x) * (16 / 8);

	/* Wait for previous operations to complete */
	if ((rc = cfxga_synchronize(sc)) != 0)
		return (rc);

	cfxga_write_2(sc, CFREG_BITBLT_ROP, rop | (op << 8));
	cfxga_write_2(sc, CFREG_BITBLT_SRC_LOW, pos);
	cfxga_write_2(sc, CFREG_BITBLT_SRC_HIGH, pos >> 16);
	cfxga_write_2(sc, CFREG_BITBLT_DST_LOW, pos);
	cfxga_write_2(sc, CFREG_BITBLT_DST_HIGH, pos >> 16);
	cfxga_write_2(sc, CFREG_BITBLT_OFFSET, 640);
	cfxga_write_2(sc, CFREG_BITBLT_WIDTH, cx - 1);
	cfxga_write_2(sc, CFREG_BITBLT_HEIGHT, cy - 1);
	if (op == OP_SOLID_FILL)
		cfxga_write_2(sc, CFREG_BITBLT_FG, (u_int16_t)srccolor);
	cfxga_write_2(sc, CFREG_BITBLT_CONTROL,
	    BITBLT_ACTIVE | BITBLT_COLOR_16);

	return (0);
}

/*
 * Text console raster operations.
 *
 * We shadow all these operations on a memory copy of the frame buffer.
 * Since we are running in emulation mode only, this could be optimized
 * by only storing actual character cell values (a la mda).
 */

void
cfxga_copycols(void *cookie, int row, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct cfxga_screen *scr = ri->ri_hw;
	int x, y, cx, cy;

	(*scr->scr_ops.copycols)(ri, row, src, dst, num);

	if (scr != scr->scr_sc->sc_active)
		return;

	x = dst * ri->ri_font->fontwidth + ri->ri_xorigin;
	y = row * ri->ri_font->fontheight + ri->ri_yorigin;
	cx = num * ri->ri_font->fontwidth;
	cy = ri->ri_font->fontheight;
	cfxga_memory_rop(scr->scr_sc, scr, ROP_SRC, x, y, cx, cy);
}

void
cfxga_copyrows(void *cookie, int src, int dst, int num)
{
	struct rasops_info *ri = cookie;
	struct cfxga_screen *scr = ri->ri_hw;
	int x, y, cx, cy;

	(*scr->scr_ops.copyrows)(ri, src, dst, num);

	if (scr != scr->scr_sc->sc_active)
		return;

	x = ri->ri_xorigin;
	y = dst * ri->ri_font->fontheight + ri->ri_yorigin;
	cx = ri->ri_emuwidth;
	cy = num * ri->ri_font->fontheight;
	cfxga_memory_rop(scr->scr_sc, scr, ROP_SRC, x, y, cx, cy);
}

void
cfxga_do_cursor(struct rasops_info *ri)
{
	struct cfxga_screen *scr = ri->ri_hw;
	int x, y, cx, cy;

	if (scr != scr->scr_sc->sc_active)
		return;

	x = ri->ri_ccol * ri->ri_font->fontwidth + ri->ri_xorigin;
	y = ri->ri_crow * ri->ri_font->fontheight + ri->ri_yorigin;
	cx = ri->ri_font->fontwidth;
	cy = ri->ri_font->fontheight;
	cfxga_standalone_rop(scr->scr_sc, OP_MOVE_POSITIVE_ROP,
	    ROP_ONES ^ ROP_SRC /* i.e. not SRC */,
	    x, y, cx, cy, /* ri->ri_devcmap[WSCOL_BLACK] */ 0);
}

void
cfxga_erasecols(void *cookie, int row, int col, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct cfxga_screen *scr = ri->ri_hw;
	int fg, bg;
	int x, y, cx, cy;

	(*scr->scr_ops.erasecols)(ri, row, col, num, attr);

	if (scr != scr->scr_sc->sc_active)
		return;

	rasops_unpack_attr(attr, &fg, &bg, NULL);
	x = col * ri->ri_font->fontwidth + ri->ri_xorigin;
	y = row * ri->ri_font->fontheight + ri->ri_yorigin;
	cx = num * ri->ri_font->fontwidth;
	cy = ri->ri_font->fontheight;
	cfxga_standalone_rop(scr->scr_sc, OP_SOLID_FILL, 0,
	    x, y, cx, cy, ri->ri_devcmap[bg]);
}

void
cfxga_eraserows(void *cookie, int row, int num, long attr)
{
	struct rasops_info *ri = cookie;
	struct cfxga_screen *scr = ri->ri_hw;
	int fg, bg;
	int x, y, cx, cy;

	(*scr->scr_ops.eraserows)(ri, row, num, attr);

	if (scr != scr->scr_sc->sc_active)
		return;

	rasops_unpack_attr(attr, &fg, &bg, NULL);
	x = ri->ri_xorigin;
	y = row * ri->ri_font->fontheight + ri->ri_yorigin;
	cx = ri->ri_emuwidth;
	cy = num * ri->ri_font->fontheight;
	cfxga_standalone_rop(scr->scr_sc, OP_SOLID_FILL, 0,
	    x, y, cx, cy, ri->ri_devcmap[bg]);
}

void
cfxga_putchar(void *cookie, int row, int col, u_int uc, long attr)
{
	struct rasops_info *ri = cookie;
	struct cfxga_screen *scr = ri->ri_hw;
	int x, y;

	(*scr->scr_ops.putchar)(ri, row, col, uc, attr);

	if (scr != scr->scr_sc->sc_active)
		return;

	x = col * ri->ri_font->fontwidth + ri->ri_xorigin;
	y = row * ri->ri_font->fontheight + ri->ri_yorigin;

	if (uc == ' ') {
		int cx, cy, fg, bg;

		rasops_unpack_attr(attr, &fg, &bg, NULL);
		cx = ri->ri_font->fontwidth;
		cy = ri->ri_font->fontheight;
		cfxga_standalone_rop(scr->scr_sc, OP_SOLID_FILL, 0,
		    x, y, cx, cy, ri->ri_devcmap[bg]);
	} else {
		cfxga_expand_char(scr->scr_sc, scr, uc, x, y, attr);
	}
}

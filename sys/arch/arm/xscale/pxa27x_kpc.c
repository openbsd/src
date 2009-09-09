/*
 * Copyright (c) 2009 Marek Vasut <marex@openbsd.org>
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/timeout.h>
#include <sys/systm.h>

#include <arch/arm/xscale/pxa2x0_gpio.h>
#include <arch/arm/xscale/pxa2x0reg.h>
#include <arch/arm/xscale/pxa2x0var.h>
#include <arch/arm/xscale/pxa27x_kpc.h>

int  pxa27x_kpc_enable(void *, int);
void pxa27x_kpc_setleds(void *, int);
int  pxa27x_kpc_ioctl(void *, u_long, caddr_t, int, struct proc *);

inline void pxa27x_kpc_submit(struct pxa27x_kpc_softc *, int);
int  pxa27x_kpc_intr(void *);

struct cfdriver pxa27x_kpc_cd = {
	NULL, "pxa27x_kpc", DV_DULL,
};

struct wskbd_accessops pxa27x_kpc_accessops = {
	pxa27x_kpc_enable,
	pxa27x_kpc_setleds,
	pxa27x_kpc_ioctl,
};

struct wscons_keydesc pxa27x_kpc_keydesctab[] = {
	{KB_US, 0, 0, 0},
	{0, 0, 0, 0},
};

struct wskbd_mapdata pxa27x_kpc_mapdata = {
	pxa27x_kpc_keydesctab, KB_US,
};

void pxa27x_kpc_cngetc(void *, u_int *, int *);
void pxa27x_kpc_cnpollc(void *, int);
void pxa27x_kpc_cnbell(void *, u_int, u_int, u_int);

struct wskbd_consops pxa27x_kpc_consops = {
	pxa27x_kpc_cngetc,
	pxa27x_kpc_cnpollc,
	pxa27x_kpc_cnbell,
};

int
pxa27x_kpc_match(void *aux)
{
	struct pxaip_attach_args *pxa = aux;
	if (pxa->pxa_addr != PXA2X0_KPC_BASE)
		return 0;	/* Wrong device */

	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) != CPU_ID_PXA27X)
		return 0;	/* Wrong CPU */

	pxa->pxa_size = PXA2X0_KPC_SIZE;
	return 1;
}

void
pxa27x_kpc_attach(struct pxa27x_kpc_softc *sc, void *aux)
{
	struct pxaip_attach_args *pxa = aux;
	struct wskbddev_attach_args a;

	sc->sc_iot = pxa->pxa_sa.sa_iot;
	if (bus_space_map(sc->sc_iot, pxa->pxa_addr, pxa->pxa_size, 0,
	    &sc->sc_ioh) != 0) {
		printf(": can't map regs\n");
		goto err;
	}

	pxa2x0_clkman_config(CKEN_KEY, 1);

	sc->sc_ih = pxa2x0_intr_establish(PXA2X0_INT_KPC, IPL_TTY,
		pxa27x_kpc_intr, sc, sc->sc_dev.dv_xname);
	if (!sc->sc_ih) {
		printf(": can't establish interrupt\n");
		goto err2;
	}

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, KPC_KPC, KPC_MIE | KPC_ME |
		KPC_MS(0xff) | KPC_IMKP | KPC_MI | KPC_MKCN(sc->sc_cols) |
		KPC_MKRN(sc->sc_rows) | KPC_ASACT);
	bus_space_write_4(sc->sc_iot, sc->sc_ioh, KPC_KPKDI, 0x30);

	pxa27x_kpc_keydesctab[0].map_size = sc->sc_ksize;
	pxa27x_kpc_keydesctab[0].map = sc->sc_kcodes;

	a.console	= 1;
	a.keymap	= &pxa27x_kpc_mapdata;
	a.accessops	= &pxa27x_kpc_accessops;
	a.accesscookie	= sc;

	printf("\n");

	wskbd_cnattach(&pxa27x_kpc_consops, sc, &pxa27x_kpc_mapdata);

	sc->sc_wskbddev	= config_found((struct device *)sc, &a, wskbddevprint);

	return;

err2:
	bus_space_unmap(sc->sc_iot, sc->sc_ioh, PXA2X0_KPC_SIZE);
err:
	return;
}

inline void
pxa27x_kpc_submit(struct pxa27x_kpc_softc *sc, int event)
{
#ifdef WSDISPLAY_COMPAT_RAWKBD
	u_char key;
	if (sc->sc_rawkbd) {
		key = sc->sc_key;
		if (event == WSCONS_EVENT_KEY_DOWN)
			key |= 0x80;
		wskbd_rawinput(sc->sc_wskbddev, &key, 1);
	} else
#endif
	wskbd_input(sc->sc_wskbddev, event, sc->sc_key);
}

int
pxa27x_kpc_intr(void *arg)
{
	u_int32_t val;
	int row = -1, col = -1;
	int i = 0;
	struct pxa27x_kpc_softc *sc = arg;

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, KPC_KPC);
	if (!(val & KPC_MI))	/* interrupt didn't happen .. what ?! */
		return 0;

	val = bus_space_read_4(sc->sc_iot, sc->sc_ioh, KPC_KPAS);
	if (val & KPAS_SO)	/* bogus interrupt */
		return 0;

	if (val & KPAS_MUKP) {	/* keypress */
		col = val & KPAS_CP;
		row = (val & KPAS_RP) >> 4;
		for (i = 0; i < sc->sc_ksize; i++)
			if (sc->sc_kmap[i].row == row &&
				sc->sc_kmap[i].col == col) {
				sc->sc_key = sc->sc_kmap[i].key;
				pxa27x_kpc_submit(sc, WSCONS_EVENT_KEY_DOWN);
				break;
			}
	} else	/* no keypress aka keyrelease */
		pxa27x_kpc_submit(sc, WSCONS_EVENT_KEY_UP);

	return 1;
}

int
pxa27x_kpc_enable(void *v, int power)
{
	return 0;
}

void
pxa27x_kpc_setleds(void *v, int power)
{
}

int
pxa27x_kpc_ioctl(void *v, u_long cmd, caddr_t data, int flag, struct proc *p)
{
#ifdef WSDISPLAY_COMPAT_RAWKBD
	struct pxa27x_kpc_softc *sc = v;
#endif

	switch (cmd) {
	case WSKBDIO_GTYPE:
		*(int *)data = WSKBD_TYPE_KPC;
		return 0;
	case WSKBDIO_SETLEDS:
		return 0;
	case WSKBDIO_GETLEDS:
		*(int *)data = 0;
		return 0;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	case WSKBDIO_SETMODE:
		sc->sc_rawkbd = *(int *)data == WSKBD_RAW;
		return 0;
#endif
	}
	return -1;
}

void
pxa27x_kpc_cnbell(void *v, u_int pitch, u_int period, u_int volume)
{
}

void
pxa27x_kpc_cngetc(void *v, u_int *type, int *data)
{
}

void
pxa27x_kpc_cnpollc(void *v, int on)
{
}

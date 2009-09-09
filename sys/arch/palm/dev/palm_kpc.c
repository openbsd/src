/*	$OpenBSD: palm_kpc.c,v 1.3 2009/09/09 12:14:39 marex Exp $	*/

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
#include <sys/systm.h>

#include <machine/machine_reg.h>
#include <machine/palm_var.h>
#include <arch/arm/xscale/pxa2x0_gpio.h>

#include <arch/arm/xscale/pxa27x_kpc.h>

int	palm_kpc_match(struct device *, void *, void *);
void	palm_kpc_attach(struct device *, struct device *, void *);

struct cfattach pxakpc_palm_ca = {
	sizeof(struct pxa27x_kpc_softc),
	palm_kpc_match,
	palm_kpc_attach
};

const keysym_t palmkpc_keycodes[] = {
	KS_KEYCODE(0), KS_s,
	KS_KEYCODE(1), KS_a,
	KS_KEYCODE(2), KS_b,
	KS_KEYCODE(3), KS_c,
	KS_KEYCODE(4), KS_d,
	KS_KEYCODE(5), KS_Return,
	KS_KEYCODE(6), KS_KP_Up,
	KS_KEYCODE(7), KS_KP_Down,
	KS_KEYCODE(8), KS_KP_Left,
	KS_KEYCODE(9), KS_KP_Right,
};

#ifdef WSDISPLAY_COMPAT_RAWKBD
const keysym_t palmkpc_xt_keycodes[] = {
	RAWKEY_s,
	RAWKEY_a,
	RAWKEY_b,
	RAWKEY_c,
	RAWKEY_d,
	RAWKEY_Return,
	RAWKEY_KP_Up,
	RAWKEY_KP_Down,
	RAWKEY_KP_Left,
	RAWKEY_KP_Right,
};
#endif

const struct pxa27x_kpc_keymap palmkpc_keymap[] = {
	{0, 0, 0},
	{0, 1, 1},
	{1, 0, 2},
	{1, 1, 3},
	{1, 2, 4},
	{0, 2, 5},
	{2, 0, 6},
	{2, 2, 7},
	{3, 2, 8},
	{3, 0, 9},
};

int
palm_kpc_match(struct device *parent, void *match, void *aux)
{
	return pxa27x_kpc_match(aux);
}

void
palm_kpc_attach(struct device *parent, struct device *self, void *aux)
{
	struct pxa27x_kpc_softc *sc = (struct pxa27x_kpc_softc *)self;

	pxa2x0_gpio_set_function(100, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(101, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(102, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(97, GPIO_ALT_FN_3_IN);

	pxa2x0_gpio_set_function(103, GPIO_ALT_FN_2_OUT);
	pxa2x0_gpio_set_function(104, GPIO_ALT_FN_2_OUT);
	pxa2x0_gpio_set_function(105, GPIO_ALT_FN_2_OUT);

	sc->sc_rows	= 4;
	sc->sc_cols	= 3;
	sc->sc_kmap	= palmkpc_keymap;
	sc->sc_kcodes	= palmkpc_keycodes;
#ifdef WSDISPLAY_COMPAT_RAWKBD
	sc->sc_xt_kcodes	= palmkpc_xt_keycodes;
#endif
	sc->sc_ksize	= sizeof(palmkpc_keycodes)/sizeof(keysym_t);

	pxa27x_kpc_attach(sc, aux);
}

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

#include <dev/wscons/wsconsio.h>
#include <dev/wscons/wskbdraw.h>
#include <dev/wscons/wskbdvar.h>
#include <dev/wscons/wsksymdef.h>
#include <dev/wscons/wsksymvar.h>

struct pxa27x_kpc_keymap {
	int		row;
	int		col;
	keysym_t	key;
};

struct pxa27x_kpc_softc {
	struct device		sc_dev;
	struct device		*sc_wskbddev;
	void			*sc_ih;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	int			sc_key;
	int			sc_rawkbd;

	int			sc_rows;
	int			sc_cols;
	const struct pxa27x_kpc_keymap	*sc_kmap;
	const keysym_t		*sc_kcodes;
	const keysym_t		*sc_xt_kcodes;
	int			sc_ksize;
};

int  pxa27x_kpc_match(void *);
void pxa27x_kpc_attach(struct pxa27x_kpc_softc *, void *);

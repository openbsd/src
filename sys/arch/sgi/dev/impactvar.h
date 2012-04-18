/*	$OpenBSD: impactvar.h,v 1.1 2012/04/18 17:28:24 miod Exp $	*/

/*
 * Copyright (c) 2010 Miodrag Vallat.
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

struct impact_screen;

struct impact_softc {
	struct device		 sc_dev;
	struct impact_screen	*curscr;
	int			 console;
	int			 nscreens;
};

int	impact_attach_common(struct impact_softc *, bus_space_tag_t,
	    bus_space_handle_t, int, int);
int	impact_cnattach_common(bus_space_tag_t, bus_space_handle_t, int);

struct gio_attach_args;
int	impact_gio_cnprobe(struct gio_attach_args *);
int	impact_gio_cnattach(struct gio_attach_args *);
int	impact_xbow_cnprobe(void);
int	impact_xbow_cnattach(void);

/*	$OpenBSD: wscons_machdep.c,v 1.1 2005/04/01 10:40:48 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>

#include <dev/cons.h>

#include "wsdisplay.h"
#if NWSDISPLAY > 0
#include <dev/wscons/wsdisplayvar.h>
#endif

#include "wskbd.h"
#if NWSKBD > 0
#include <dev/wscons/wskbdvar.h>
#endif

cons_decl(ws);

void
wscnprobe(struct consdev *cp)
{
	/*
	 * Due to various device probe restrictions, the wscons console
	 * can never be enabled early during boot.
	 * It will be enabled as soon as enough wscons components get
	 * attached.
	 * So do nothing there, the switch will occur in
	 * wsdisplay_emul_attach() later.
	 */
}

void
wscninit(struct consdev *cp)
{
}

void
wscnputc(dev_t dev, int i)
{
#if NWSDISPLAY > 0
	wsdisplay_cnputc(dev, i);
#endif
}

int
wscngetc(dev_t dev)
{
#if NWSKBD > 0
	return (wskbd_cngetc(dev));
#else
	return (0);
#endif
}

void
wscnpollc(dev_t dev, int on)
{
#if NWSKBD > 0
	wskbd_cnpollc(dev, on);
#endif
}

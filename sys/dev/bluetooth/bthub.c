/*	$OpenBSD: bthub.c,v 1.1 2007/05/31 04:04:56 uwe Exp $	*/

/*
 * Copyright (c) 2007 Uwe Stuehler <uwe@openbsd.org>
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

#include <netbt/bluetooth.h>

struct bthub_softc {
	struct device sc_dev;
};

int	bthub_match(struct device *, void *, void *);
void	bthub_attach(struct device *, struct device *, void *);
int	bthub_detach(struct device *, int);

struct cfattach bthub_ca = {
	sizeof(struct bthub_softc), bthub_match, bthub_attach, bthub_detach
};

struct cfdriver bthub_cd = {
	NULL, "bthub", DV_DULL
};

int
bthub_match(struct device *parent, void *match, void *aux)
{
	return 1;
}

void
bthub_attach(struct device *parent, struct device *self, void *aux)
{
	bdaddr_t *addr = aux;

	printf(" %02x:%02x:%02x:%02x:%02x:%02x\n",
	    addr->b[5], addr->b[4], addr->b[3],
	    addr->b[2], addr->b[1], addr->b[0]);
}

int
bthub_detach(struct device *self, int flags)
{
	return 0;
}

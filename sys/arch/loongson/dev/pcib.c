/*	$OpenBSD: pcib.c,v 1.1.1.1 2009/12/25 21:04:34 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/isa/isavar.h>

#include <loongson/dev/glxreg.h>
#include <loongson/dev/glxvar.h>

#include "isa.h"

void	pcibattach(struct device *, struct device *, void *);
int	pcib_print(void *, const char *);

void
pcibattach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct isabus_attach_args iba;

	printf("\n");

	/*
	 * Attach the ISA bus behind this bridge.
	 * Note that, since we only have a few legacy I/O devices and
	 * no ISA slots, we can attach immediately.
	 */
	memset(&iba, 0, sizeof(iba));
	iba.iba_busname = "isa";
	iba.iba_iot = pa->pa_iot;
	iba.iba_memt = pa->pa_memt;
#if NISADMA > 0
	iba.iba_dmat = pa->pa_dmat;
#endif
	config_found(self, &iba, pcib_print);
}

int
pcib_print(void *aux, const char *pnp)
{
	/* Only ISAs can attach to pcib's; easy. */
	if (pnp)
		printf("isa at %s", pnp);
	return (UNCONF);
}

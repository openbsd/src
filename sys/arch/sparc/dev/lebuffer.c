/*	$OpenBSD: lebuffer.c,v 1.2 1998/03/09 09:33:39 deraadt Exp $	*/
/*	$NetBSD: lebuffer.c,v 1.3 1997/05/24 20:16:28 pk Exp $ */

/*
 * Copyright (c) 1996 Paul Kranenburg.  All rights reserved.
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
 *	This product includes software developed by Peter Galbavy.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <sparc/autoconf.h>
#include <sparc/cpu.h>

#include <sparc/dev/sbusvar.h>
#include <sparc/dev/lebuffervar.h>
#include <sparc/dev/dmareg.h>/*XXX*/

int	lebufprint __P((void *, const char *));
void	lebufattach __P((struct device *, struct device *, void *));

struct cfattach lebuffer_ca = {
	sizeof(struct lebuf_softc), matchbyname, lebufattach
};

struct cfdriver lebuffer_cd = {
	NULL, "lebuffer", DV_DULL
};

int
lebufprint(aux, name)
	void *aux;
	const char *name;
{
	register struct confargs *ca = aux;

	if (name)
		printf("[%s at %s]", ca->ca_ra.ra_name, name);
	printf(" slot 0x%x offset 0x%x", ca->ca_slot, ca->ca_offset);
	return (UNCONF);
}

/*
 * Attach all the sub-devices we can find
 */
void
lebufattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
#if defined(SUN4C) || defined(SUN4M)
	register struct confargs *ca = aux;
	struct lebuf_softc *sc = (void *)self;
	int node;
	struct confargs oca;
	char *name;
	int sbusburst;

	if (ca->ca_ra.ra_vaddr == NULL || ca->ca_ra.ra_nvaddrs == 0)
		ca->ca_ra.ra_vaddr =
		    mapiodev(ca->ca_ra.ra_reg, 0, ca->ca_ra.ra_len);

	/*
	 * This device's "register space" is just a buffer where the
	 * Lance ring-buffers can be stored. Note the buffer's location
	 * and size, so the `le' driver can pick them up.
	 */
	sc->sc_buffer = (caddr_t)ca->ca_ra.ra_vaddr;
	sc->sc_bufsiz = ca->ca_ra.ra_len;

	/*
	 * Get transfer burst size from PROM
	 */
	sbusburst = ((struct sbus_softc *)parent)->sc_burst;
	if (sbusburst == 0)
		sbusburst = SBUS_BURST_32 - 1; /* 1->16 */

	sc->sc_burst = getpropint(ca->ca_ra.ra_node, "burst-sizes", -1);
	if (sc->sc_burst == -1)
		/* take SBus burst sizes */
		sc->sc_burst = sbusburst;

	/* Clamp at parent's burst sizes */
	sc->sc_burst &= sbusburst;

	printf(": %dK memory\n", sc->sc_bufsiz / 1024);

	node = sc->sc_node = ca->ca_ra.ra_node;

	if (ca->ca_bustype == BUS_SBUS)
		sbus_establish(&sc->sc_sd, &sc->sc_dev);

	/* Propagate bootpath */
	if (ca->ca_ra.ra_bp != NULL)
		oca.ca_ra.ra_bp = ca->ca_ra.ra_bp + 1;
	else
		oca.ca_ra.ra_bp = NULL;

	/* search through children */
	for (node = firstchild(node); node; node = nextsibling(node)) {
		name = getpropstring(node, "name");
		if (!romprop(&oca.ca_ra, name, node))
			continue;

		sbus_translate(parent, &oca);
		oca.ca_bustype = BUS_SBUS;
		(void) config_found(&sc->sc_dev, (void *)&oca, lebufprint);
	}
#endif /* SUN4C || SUN4M */
}

/*	$OpenBSD: raven.c,v 1.6 2004/05/14 20:38:32 miod Exp $ */

/*
 * Copyright (c) 2001 Steve Murphree, Jr.
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
 *	This product includes software developed under OpenBSD for RTMX Inc
 *      by Per Fogelstrom, Opsycon AB.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

/*
 * Motorola 'Raven' ASIC driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>

#include <mvmeppc/dev/ravenreg.h>
#include <mvmeppc/dev/ravenvar.h>

int	 raven_match(struct device *, void *, void *);
void	 raven_attach(struct device *, struct device *, void *);

struct cfattach raven_ca = {
        sizeof(struct raven_softc), raven_match, raven_attach,
};

struct cfdriver raven_cd = {
	NULL, "raven", DV_DULL,
};

int
raven_match(struct device *parent, void *match, void *aux)
{
	struct confargs *ca = aux;
	void *va;
	u_int32_t probe;

	if (strcmp(ca->ca_name, raven_cd.cd_name) != 0)
		return 0;

	if ((va = mapiodev((paddr_t)RAVEN_BASE, RAVEN_SIZE)) == NULL)
		return 0;

	/* check for a live address */
	if (badaddr(va, 4) != 0) {
		unmapiodev(va, RAVEN_SIZE);
		return 0;
	}
	
	/* now check and see if it's a raven ASIC */	
	probe = *(u_int32_t*)va;
	unmapiodev((void *)va, RAVEN_SIZE);

	if (probe != RAVEN_MAGIC)
		return 0;
	
	return 1;
}

/* need to be global for mpcpcibr.c - XXX */
u_int8_t *ravenregs;

void
raven_attach(struct device *parent, struct device *self, void *aux)
{
	struct raven_softc *sc = (void *)self;

	/*
	 * Map Raven registers and MPCIC
	 *
	 * XXX steal them from devio_ex as well!
	 */
	ravenregs = sc->sc_regs = mapiodev((paddr_t)RAVEN_BASE, RAVEN_SIZE);
	if (sc->sc_regs == NULL) {
		printf(": can't map registers!\n");
		return;
	}

	/* set system type */
	system_type = MVME;		/* We are a Motorola MVME SBC */

	printf(": version 0x%x\n", sc->sc_regs[RAVEN_REVID]);

	while (config_found(self, NULL, NULL))
		;
}

/*	$OpenBSD: sgc.c,v 1.1 2005/01/14 22:39:26 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * SGC bus attachment and mapping glue.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/hp300spu.h>

#include <hp300/dev/dmavar.h>

#include <hp300/dev/sgcreg.h>
#include <hp300/dev/sgcvar.h>

int	sgcmatch(struct device *, void *, void *);
void	sgcattach(struct device *, struct device *, void *);
int	sgcprint(void *, const char *);

struct cfattach sgc_ca = {
	sizeof(struct device), sgcmatch, sgcattach
};

struct cfdriver sgc_cd = {
	NULL, "sgc", DV_DULL
};

int
sgcmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
static	int sgc_matched = 0;

	/* Allow only one instance. */
	if (sgc_matched)
		return (0);

	/*
	 * Leave out machines which can not have an SGC bus.
	 */

	if (machineid != HP_400 && machineid != HP_425 &&
	    machineid != HP_433)
		return (0);

	return (sgc_matched = 1);
}

void
sgcattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sgc_attach_args saa;
	caddr_t pa, va;
	int slot, rv;

	printf("\n");

	for (slot = 0; slot < SGC_NSLOTS; slot++) {
		pa = sgc_slottopa(slot);
		va = iomap(pa, PAGE_SIZE);
		if (va == NULL) {
			printf("%s: can't map slot %d\n", self->dv_xname, slot);
			continue;
		}

		/* Check for hardware. */
		rv = badaddr(va);
		iounmap(va, PAGE_SIZE);

		if (rv != 0)
			continue;

		bzero(&saa, sizeof(saa));
		saa.saa_slot = slot;

		/* Attach matching device. */
		config_found(self, &saa, sgcprint);
	}
}

int
sgcprint(aux, pnp)
	void *aux;
	const char *pnp;
{
	struct sgc_attach_args *saa = aux;

	if (pnp)
		printf("unknown SGC card at %s", pnp);
	printf(" slot %d", saa->saa_slot);
	return (UNCONF);
}

/*
 * Convert a slot number to a system physical address.
 * This is needed for bus_space.
 */
void *
sgc_slottopa(int slot)
{
	u_long rval;

	if (slot < 0 || slot > SGC_NSLOTS)
		rval = 0;
	else
		rval = SGC_BASE + (slot * SGC_DEVSIZE);

	return ((void *)rval);
}

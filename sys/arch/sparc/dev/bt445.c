/*	$OpenBSD: bt445.c,v 1.2 2008/12/26 15:33:40 miod Exp $	*/
/*
 * Copyright (c) 2003, Miodrag Vallat.
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
 */

/*
 * A skeleton driver for the Brooktree BT445 ``Chameleon'' RAMDAC found
 * on the early Tadpole SPARCbook 3 machines.
 *
 * It will not perform anything by itself, but frame buffer drivers can use
 * its softc structure to be able to access the RAMDAC themselves - creating
 * a ramdac API for just this specific case of separate attachments is not
 * worth doing.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <machine/autoconf.h>

#include <sparc/dev/bt445reg.h>
#include <sparc/dev/bt445var.h>

void	btcham_attach(struct device *, struct device *, void *);
int	btcham_match(struct device *, void *, void *);

struct	cfattach btcham_ca = {
	sizeof(struct bt445_softc), btcham_match, btcham_attach
};

struct	cfdriver btcham_cd = {
	NULL, "btcham", DV_DULL
};

/*
 * Match a chameleon
 */
int
btcham_match(struct device *parent, void *vcf, void *aux)
{
	struct confargs *ca = aux;
	struct romaux *ra = &ca->ca_ra;

	if (strcmp("bt445", ra->ra_name) != 0)
		return (0);

	return (1);
}

/*
 * Attach a chameleon
 */
void
btcham_attach(struct device *parent, struct device *self, void *args)
{
	struct confargs *ca = args;
	struct bt445_softc *sc = (struct bt445_softc *)self;
	int id, rev;

	/*
	 * Map the registers
	 */
	sc->sc_regs = mapiodev(ca->ca_ra.ra_reg, 0, ca->ca_ra.ra_len);

	/*
	 * Report a few values to please the user.
	 */
	sc->sc_regs[BT445_ADDRESS] = BT445_REGISTER_OFFSET(BT445_ID);
	id = sc->sc_regs[BT445_REGISTER_INDEX(BT445_ID)];

	sc->sc_regs[BT445_ADDRESS] = BT445_REGISTER_OFFSET(BT445_REVISION);
	rev = sc->sc_regs[BT445_REGISTER_INDEX(BT445_REVISION)];

	printf(": id 0x%x, revision 0x%x\n", id, rev);
}

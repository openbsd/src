/*	$OpenBSD: obio.c,v 1.1 2005/04/20 01:00:16 miod Exp $	*/
/*	OpenBSD: obio.c,v 1.16 2004/09/29 07:35:11 miod Exp 	*/

/*
 * Copyright (c) 1993, 1994 Theo de Raadt
 * Copyright (c) 1995, 1997 Paul Kranenburg
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/idt.h>

/* autoconfiguration driver */
static int	obiomatch(struct device *, void *, void *);
static void	obioattach(struct device *, struct device *, void *);

int		obioprint(void *, const char *);
int		obio_scan(struct device *, void *, void *);

struct cfattach obio_ca = {
	sizeof(struct device), obiomatch, obioattach
};

struct cfdriver obio_cd = {
	NULL, "obio", DV_DULL
};

/*
 * A list of the on-board devices in the IDT systems. This is better than
 * having people playing with locators in their kernel configuration
 * files, and necessary because the device tree built by the PROM does not
 * list all on-board devices (audio and floppy are missing).
 */
const struct {
	char *devname;
	paddr_t address;
	int	intr;
} obio_devices[] = {
	{ "tod",	TODCLOCK_BASE,	-1 },
	{ "nvram",	NVRAM_BASE,	-1 },
	{ "zs",		ZS0_BASE,	12 },
	{ "zs",		ZS1_BASE,	12 },
	{ "fdc",	FDC_BASE,	11 },
	{ "audioamd",	AUDIO_BASE,	13 },
	{ "wdsc",	SE_BASE + 0x20,	4 },
	{ "le",		SE_BASE + 0x30,	6 },
	{ NULL,		0 }
};

int
obiomatch(parent, vcf, aux)
	struct device *parent;
	void *vcf, *aux;
{
	register struct cfdata *cf = vcf;
	register struct confargs *ca = aux;
	register struct romaux *ra = &ca->ca_ra;

	return (strcmp(cf->cf_driver->cd_name, ra->ra_name) == 0);
}

int
obioprint(args, obio)
	void *args;
	const char *obio;
{
	register struct confargs *ca = args;

	if (ca->ca_ra.ra_name == NULL)
		ca->ca_ra.ra_name = "<unknown>";

	if (obio)
		printf("%s at %s", ca->ca_ra.ra_name, obio);

	printf(" addr %p", ca->ca_ra.ra_paddr);

	return (UNCONF);
}

void
obioattach(parent, self, args)
	struct device *parent, *self;
	void *args;
{
	struct confargs *ca = args;
	struct confargs oca;
	int i;

	if (self->dv_unit > 0) {
		printf(" unsupported\n");
		return;
	}
	printf("\n");

	for (i = 0; obio_devices[i].devname != NULL; i++) {
		/* fake a rom_reg */
		bzero(&oca, sizeof oca);
		oca.ca_ra.ra_paddr = (caddr_t)obio_devices[i].address;
		oca.ca_ra.ra_vaddr = NULL;
		oca.ca_ra.ra_len = 0;
		oca.ca_ra.ra_nreg = 1;
		oca.ca_ra.ra_iospace = 0;
		oca.ca_ra.ra_intr[0].int_pri = obio_devices[i].intr;
		oca.ca_ra.ra_intr[0].int_vec = -1;
		oca.ca_ra.ra_nintr = oca.ca_ra.ra_intr[0].int_pri < 0 ? 0 : 1;
		oca.ca_ra.ra_name = obio_devices[i].devname;
		if (ca->ca_ra.ra_bp != NULL)
			oca.ca_ra.ra_bp = ca->ca_ra.ra_bp + 1;
		else
			oca.ca_ra.ra_bp = NULL;
		oca.ca_bustype = BUS_OBIO;

		config_found(self, &oca, obioprint);
	}
}

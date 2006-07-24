/*	$OpenBSD: sbi.c,v 1.12 2006/07/24 17:25:11 miod Exp $ */
/*	$NetBSD: sbi.c,v 1.20 1999/08/07 10:36:50 ragge Exp $ */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * Still to do: Write all SBI error handling.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/nexus.h>

static	int sbi_print(void *, const char *);
static	int sbi_match_abus(struct device *, struct cfdata *, void *);
static	int sbi_match_mainbus(struct device *, struct cfdata *, void *);
static	void sbi_attach(struct device *, struct device *, void *);

struct	cfdriver sbi_cd = {
	NULL, "sbi", DV_DULL
};

int
sbi_print(aux, name)
	void *aux;
	const char *name;
{
	struct sbi_attach_args *sa = (struct sbi_attach_args *)aux;
	int unsupp = 0;

	if (name) {
		switch (sa->type) {
		case NEX_MBA:
			printf("mba at %s", name);
			break;
		default:
			printf("unknown device 0x%x at %s", sa->type, name);
			unsupp++;
		}		
	}
	printf(" tr%d", sa->nexnum);
	return (unsupp ? UNSUPP : UNCONF);
}

int
sbi_match_mainbus(parent, cf, aux)
	struct device	*parent;
	struct cfdata *cf;
	void *aux;
{
	struct mainbus_attach_args *maa = aux;

	if (maa->maa_bustype == VAX_SBIBUS)
		return 1;
	return 0;
}

int
sbi_match_abus(parent, cf, aux)
	struct device	*parent;
	struct cfdata *cf;
	void *aux;
{
	struct bp_conf *bp = aux;

	if (strcmp(bp->type, sbi_cd.cd_name) == 0)
		return 1;

	return 0;
}

void
sbi_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	u_int	nexnum, minnex;
	struct	sbi_attach_args sa;

	printf("\n");

#define NEXPAGES (sizeof(struct nexus) / VAX_NBPG)
	minnex = self->dv_unit * NNEXSBI;
	for (nexnum = minnex; nexnum < minnex + NNEXSBI; nexnum++) {
		struct	nexus *nexusP = 0;
		volatile int tmp;

		nexusP = (struct nexus *)vax_map_physmem((paddr_t)NEXA8600 +
		    sizeof(struct nexus) * nexnum, NEXPAGES);
		if (badaddr((caddr_t)nexusP, 4)) {
			vax_unmap_physmem((vaddr_t)nexusP, NEXPAGES);
		} else {
			tmp = nexusP->nexcsr.nex_csr; /* no byte reads */
			sa.type = tmp & 255;

			sa.nexnum = nexnum;
			sa.nexaddr = nexusP;
			config_found(self, (void *)&sa, sbi_print);
		}
	}
}

struct	cfattach sbi_mainbus_ca = {
	sizeof(struct device), sbi_match_mainbus, sbi_attach
};

struct	cfattach sbi_abus_ca = {
	sizeof(struct device), sbi_match_abus, sbi_attach
};

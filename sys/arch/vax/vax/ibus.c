/*	$OpenBSD: ibus.c,v 1.9 2011/09/19 21:53:02 miod Exp $	*/
/*	$NetBSD: ibus.c,v 1.7 2001/02/04 20:36:32 ragge Exp $ */
/*
 * Copyright (c) 1999 Ludd, University of Lule}, Sweden.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/nexus.h>
#include <machine/cpu.h>
#include <machine/sid.h>

static	int ibus_print(void *, const char *);
static	int ibus_match(struct device *, struct cfdata *, void *);
static	void ibus_attach(struct device *, struct device *, void *);

struct	cfdriver ibus_cd = {
	NULL, "ibus", DV_DULL
};

struct	cfattach ibus_ca = {
	sizeof(struct device), (cfmatch_t)ibus_match, ibus_attach
};

int
ibus_print(void *aux, const char *name)
{
	struct bp_conf *bp = aux;

	if (name)
		printf("%s at %s", bp->type, name);

	return (UNCONF);
}


int
ibus_match(struct device *parent, struct cfdata *cf, void *aux)
{
	struct mainbus_attach_args *maa = aux;

	if (maa->maa_bustype == VAX_IBUS)
		return 1;
	return 0;
}

#define	MVNIADDR	0x20084400
#define	SGECADDR	0x20008000
#define	SHACADDR	0x20004200
#define	SHAC1303ADDR	0x20008200

void
ibus_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	struct bp_conf bp;
	vaddr_t va;

	printf("\n");

	/*
	 * There may be a SGEC. Is badaddr() enough here?
	 */
	bp.type = "sgec";
	va = vax_map_physmem(SGECADDR, 1);
	if (badaddr((caddr_t)va, 4) == 0)
		config_found(self, &bp, ibus_print);
	vax_unmap_physmem(va, 1);

	/*
	 * There may be a LANCE.
	 */
	bp.type = "lance";
	va = vax_map_physmem(MVNIADDR, 1);
	if (badaddr((caddr_t)va, 2) == 0)
		config_found(self, &bp, ibus_print);
	vax_unmap_physmem(va, 1);

	/*
	 * The same procedure for SHAC.
	 */
	bp.type = "shac";
	/*
	 * XXX Clearly the address on Cheetah machines varies between models,
	 * XXX but I could only check the address on a 4000 106 so far. -- miod
	 */
	if (vax_boardtype == VAX_BTYP_1303 && vax_cpustype != VAX_STYP_53)
		va = vax_map_physmem(SHAC1303ADDR, 1);
	else
		va = vax_map_physmem(SHACADDR, 1);
	if (badaddr((caddr_t)va + 0x48, 4) == 0)
		config_found(self, &bp, ibus_print);
	vax_unmap_physmem(va, 1);

	/*
	 * All MV's have a Qbus.
	 */
	bp.type = "uba";
	config_found(self, &bp, ibus_print);

}

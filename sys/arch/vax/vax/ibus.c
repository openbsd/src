/*	$OpenBSD: ibus.c,v 1.2 2001/01/28 01:19:59 hugh Exp $	*/
/*	$NetBSD: ibus.c,v 1.2 1999/08/14 18:42:46 ragge Exp $ */
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

#include <machine/ibus.h>

static	int ibus_print __P((void *, const char *));
static	int ibus_match __P((struct device *, struct cfdata *, void *));
static	void ibus_attach __P((struct device *, struct device *, void*));

struct	cfdriver ibus_cd = {
	NULL, "ibus", DV_DULL
};

struct	cfattach ibus_ca = {
	sizeof(struct device), (cfmatch_t)ibus_match, ibus_attach
};

struct ibus_edal *ibus_edal = NULL;

int
ibus_print(aux, name)
	void *aux;
	const char *name;
{
	struct bp_conf *bp = aux;

	if (name)
		printf("device %s at %s", bp->type, name);

	return (UNCONF);
}


int
ibus_match(parent, cf, aux)
	struct	device	*parent;
	struct cfdata *cf;
	void	*aux;
{
	if (vax_bustype == VAX_IBUS)
		return 1;
	return 0;
}

#define	MVNIADDR 0x20084400
#define	SGECADDR 0x20008000
#define SHACADDR 0x20004200

void
ibus_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	struct bp_conf bp;
	vaddr_t va;

	printf("\n");

	if (vax_boardtype == VAX_BTYP_1303)
		ibus_edal = (struct ibus_edal *) vax_map_physmem(0x25c00000, 1);

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

/*
 * Bitwise OR bits into the interrupt mask.
 * Returns the new mask.
 */
unsigned char
ibus_ormask(mask)
	unsigned char mask;
{
	ibus_edal->edal_intmsk |= mask;
	return (ibus_edal->edal_intmsk);
}

/*
 * Sets a new interrupt mask. Returns the old one.
 * Works like spl functions.
 */
unsigned char
ibus_setmask(mask)
	unsigned char mask;
{
	unsigned char ch;

	ch = ibus_edal->edal_intmsk;
	ibus_edal->edal_intmsk = mask;
	return ch;
}

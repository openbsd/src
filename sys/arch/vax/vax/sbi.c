/*	$NetBSD: sbi.c,v 1.14 1996/10/13 03:36:00 christos Exp $ */
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/nexus.h>

static	int sbi_print __P((void *, const char *));
static	int sbi_match __P((struct device *, void *, void *));
static	void sbi_attach __P((struct device *, struct device *, void*));

int
sbi_print(aux, name)
	void *aux;
	const char *name;
{
	struct sbi_attach_args *sa = (struct sbi_attach_args *)aux;
	int unsupp = 0;
	extern int nmba;

	if (name) {
		switch (sa->type) {
		case NEX_MBA:
			printf("mba%d at %s",nmba++, name);
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
sbi_match(parent, cf, aux)
	struct	device	*parent;
	void	*cf, *aux;
{
	struct bp_conf *bp = aux;

	if (strcmp(bp->type, "sbi"))
		return 0;
	return 1;
}

void
sbi_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	u_int	nexnum, maxnex, minnex;
	struct	sbi_attach_args sa;

	printf("\n");

	/*
	 * Now a problem: on different machines with SBI units identifies
	 * in different ways (if they identifies themselves at all).
	 * We have to fake identifying depending on different CPUs.
	 */
	minnex = self->dv_unit * NNEXSBI;
	for (nexnum = minnex; nexnum < minnex + NNEXSBI; nexnum++) {
		volatile int tmp;

		if (badaddr((caddr_t)&nexus[nexnum], 4))
			continue;

		tmp = nexus[nexnum].nexcsr.nex_csr; /* no byte reads */
		sa.type = tmp & 255;

		sa.nexnum = nexnum;
		sa.nexaddr = nexus + nexnum;
		config_found(self, (void*)&sa, sbi_print);
	}
}

struct	cfdriver sbi_cd = {
	NULL, "sbi", DV_DULL
};

struct	cfattach sbi_ca = {
	sizeof(struct device), sbi_match, sbi_attach
};
	

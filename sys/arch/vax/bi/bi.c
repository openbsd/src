/*	$NetBSD: bi.c,v 1.4 1996/10/13 03:34:44 christos Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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
 * VAXBI specific routines.
 */
/*
 * TODO
 *   handle BIbus errors more gracefully.
 */

#include <sys/param.h>
#include <sys/device.h>

#include <machine/mtpr.h>
#include <machine/nexus.h>
#include <machine/cpu.h>

#include <arch/vax/bi/bireg.h>
#include <arch/vax/bi/bivar.h>

static int bi_match __P((struct device *, void *, void *));
static void bi_attach __P((struct device *, struct device *, void*));
static int bi_print __P((void *, const char *));

struct cfdriver bi_cd = {
	NULL, "bi", DV_DULL
};

struct cfattach bi_ca = {
	sizeof(struct bi_softc), bi_match, bi_attach
};

struct bi_list bi_list[] = {
	{BIDT_MS820, 1, "ms820"},
	{BIDT_DRB32, 0, "drb32"},
	{BIDT_DWBUA, 0, "dwbua"},
	{BIDT_KLESI, 0, "klesi"},
	{BIDT_KA820, 1, "ka820"},
	{BIDT_DB88,  0, "db88"},
	{BIDT_CIBCA, 0, "cibca"},
	{BIDT_DMB32, 0, "dmb32"},
	{BIDT_CIBCI, 0, "cibci"},
	{BIDT_KA800, 0, "ka800"},
	{BIDT_KDB50, 0, "kdb50"},
	{BIDT_DWMBA, 0, "dwmba"},
	{BIDT_KFBTA, 0, "kfbta"},
	{BIDT_DEBNK, 0, "debnk"},
	{BIDT_DEBNA, 0, "debna"},
	{0,0,0}
};

int
bi_print(aux, name)
	void *aux;
	const char *name;
{
	struct bi_attach_args *ba = aux;
	struct bi_list *bl;

	if (name) {
		for (bl = &bi_list[0]; bl->bl_nr; bl++)
			if (bl->bl_nr == ba->ba_node->biic.bi_dtype) {
				printf(bl->bl_name);
				break;
			}
		if (bl->bl_nr == 0)
			printf("unknown device 0x%x",
			    ba->ba_node->biic.bi_dtype);
		printf(" at %s", name);
	}
	printf(" node %d", ba->ba_nodenr);
	return bl->bl_havedriver ? UNCONF : UNSUPP;
}

int
bi_match(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct bp_conf *bp = aux;

	if (strcmp(bp->type, "bi"))
		return 0;
	return 1;
}

void
bi_attach(parent, self, aux)
	struct device  *parent, *self;
	void *aux;
{
	struct bp_conf *bp = aux;
	struct bi_softc *bi = (void *)self;
	struct bi_node *binode;
	struct bi_attach_args ba;
	int nodenr;

	printf("\n");
	binode = bi->bi_base = (struct bi_node *)bp->bp_addr;

	ba.ba_intcpu = 1 << mastercpu;
	for (nodenr = 0; nodenr < NNODEBI; nodenr++) {
		if (badaddr((caddr_t)&binode[nodenr], 4))
			continue;

		ba.ba_node = &binode[nodenr];
		ba.ba_nodenr = nodenr;
		config_found(self, &ba, bi_print);
	}
}

void
bi_buserr()
{
	panic("bi_buserr");
}

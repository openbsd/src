/*	$OpenBSD: bi.c,v 1.8 2008/05/15 22:17:08 brad Exp $ */
/*	$NetBSD: bi.c,v 1.17 2001/11/13 12:51:34 lukem Exp $ */
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
#include <sys/systm.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/bi/bireg.h>
#include <dev/bi/bivar.h>

static int bi_print(void *, const char *);

struct bi_list bi_list[] = {
	{BIDT_MS820, DT_HAVDRV, "ms820"},
	{BIDT_DRB32, DT_UNSUPP, "drb32"},
	{BIDT_DWBUA, DT_HAVDRV|DT_ADAPT, "dwbua"},
	{BIDT_KLESI, DT_HAVDRV|DT_ADAPT, "klesi"},
	{BIDT_KA820, DT_HAVDRV, "ka820"},
	{BIDT_DB88,  DT_HAVDRV|DT_QUIET, "db88"},
	{BIDT_CIBCA, DT_UNSUPP, "cibca"},
	{BIDT_DMB32, DT_UNSUPP, "dmb32"},
	{BIDT_CIBCI, DT_UNSUPP, "cibci"},
	{BIDT_KA800, DT_UNSUPP, "ka800"},
	{BIDT_KDB50, DT_HAVDRV|DT_VEC, "kdb50"},
	{BIDT_DWMBA, DT_HAVDRV|DT_QUIET, "dwmba"},
	{BIDT_KFBTA, DT_UNSUPP, "kfbta"},
	{BIDT_DEBNK, DT_HAVDRV|DT_VEC, "debnk"},
	{BIDT_DEBNA, DT_HAVDRV|DT_VEC, "debna"},
	{0,0,0}
};

int
bi_print(aux, name)
	void *aux;
	const char *name;
{
	struct bi_attach_args *ba = aux;
	struct bi_list *bl;
	u_int16_t nr;

	nr = bus_space_read_2(ba->ba_iot, ba->ba_ioh, 0);
	for (bl = &bi_list[0]; bl->bl_nr; bl++)
		if (bl->bl_nr == nr)
			break;

	if (name) {
		if (bl->bl_nr == 0)
			printf("unknown device 0x%x", nr);
		else
			printf(bl->bl_name);
		printf(" at %s", name);
	}
	printf(" node %d", ba->ba_nodenr);
	if (bl->bl_havedriver & DT_VEC)
		printf(" vec %d", ba->ba_ivec & 511);
#ifdef DEBUG
	if (bus_space_read_4(ba->ba_iot, ba->ba_ioh, BIREG_SADR) &&
	    bus_space_read_4(ba->ba_iot, ba->ba_ioh, BIREG_EADR))
		printf(" [sadr %x eadr %x]",
		    bus_space_read_4(ba->ba_iot, ba->ba_ioh, BIREG_SADR),
		    bus_space_read_4(ba->ba_iot, ba->ba_ioh, BIREG_EADR));
#endif
	if (bl->bl_havedriver & DT_QUIET)
		printf("\n");
	return bl->bl_havedriver & DT_QUIET ? QUIET :
	    bl->bl_havedriver & DT_HAVDRV ? UNCONF : UNSUPP;
}

void
bi_attach(sc)
	struct bi_softc *sc;
{
	struct bi_attach_args ba;
	int nodenr;

	printf("\n");

	ba.ba_iot = sc->sc_iot;
	ba.ba_busnr = sc->sc_busnr;
	ba.ba_dmat = sc->sc_dmat;
	ba.ba_intcpu = sc->sc_intcpu;
	ba.ba_icookie = sc;
	/*
	 * Interrupt numbers. Assign them as described in 
	 * VAX 8800 system maintenance manual; this means like nexus
	 * adapters have them assigned.
	 * XXX - must address Unibus adapters.
	 */
	for (nodenr = 0; nodenr < NNODEBI; nodenr++) {
		if (bus_space_map(sc->sc_iot, sc->sc_addr + BI_NODE(nodenr),
		    BI_NODESIZE, 0, &ba.ba_ioh)) {
			printf("bi_attach: bus_space_map failed, node %d\n", 
			    nodenr);
			return;
		}
		if (badaddr((caddr_t)ba.ba_ioh, 4) ||
		    (bus_space_read_2(ba.ba_iot, ba.ba_ioh, 0) == 0)) {
			bus_space_unmap(ba.ba_iot, ba.ba_ioh, BI_NODESIZE);
			continue;
		}
		ba.ba_nodenr = nodenr;
		ba.ba_ivec = sc->sc_lastiv + 64 + 4 * nodenr; /* all on spl5 */
		config_found(&sc->sc_dev, &ba, bi_print);
	}
}

/* $OpenBSD: tsc.c,v 1.10 2005/12/13 01:16:11 martin Exp $ */
/* $NetBSD: tsc.c,v 1.3 2000/06/25 19:17:40 thorpej Exp $ */

/*-
 * Copyright (c) 1999 by Ross Harvey.  All rights reserved.
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
 *	This product includes software developed by Ross Harvey.
 * 4. The name of Ross Harvey may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ROSS HARVEY ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURP0SE
 * ARE DISCLAIMED.  IN NO EVENT SHALL ROSS HARVEY BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/malloc.h>

#include <machine/autoconf.h>
#include <machine/rpb.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <alpha/pci/tsreg.h>
#include <alpha/pci/tsvar.h>

#ifdef DEC_6600
#include <alpha/pci/pci_6600.h>
#endif

#define tsc() { Generate ctags(1) key. }

int	tscmatch(struct device *, void *, void *);
void	tscattach(struct device *, struct device *, void *);

struct cfattach tsc_ca = {
	sizeof(struct tsc_softc), tscmatch, tscattach,
};

struct cfdriver tsc_cd = {
        NULL, "tsc", DV_DULL,
};

struct tsp_config tsp_configuration[2];

static int tscprint(void *, const char *pnp);

int	tspmatch(struct device *, void *, void *);
void	tspattach(struct device *, struct device *, void *);

struct cfattach tsp_ca = {
	sizeof(struct tsp_softc), tspmatch, tspattach,
};

struct cfdriver tsp_cd = {
        NULL, "tsp", DV_DULL,
};


static int tspprint(void *, const char *pnp);

#if	0
static int tsp_bus_get_window(int, int,
	struct alpha_bus_space_translation *);
#endif

/* There can be only one */
static int tscfound;

/* Which hose is the display console connected to? */
int tsp_console_hose;

int
tscmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct mainbus_attach_args *ma = aux;

	return cputype == ST_DEC_6600
	    && strcmp(ma->ma_name, tsc_cd.cd_name) == 0
	    && !tscfound;
}

void tscattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	int i;
	int nbus;
	u_int64_t csc, aar;
	struct tsp_attach_args tsp;
	struct mainbus_attach_args *ma = aux;

	tscfound = 1;

	csc = LDQP(TS_C_CSC);

	nbus = 1 + (CSC_BC(csc) >= 2);
	printf(": 21272 Chipset, Cchip rev %d\n"
		"%s%d: %c Dchips, %d memory bus%s of %d bytes\n",
		(int)MISC_REV(LDQP(TS_C_MISC)),
		ma->ma_name, ma->ma_slot, "2448"[CSC_BC(csc)],
		nbus, nbus > 1 ? "es" : "", 16 + 16 * ((csc & CSC_AW) != 0));
	printf("%s%d: arrays present: ", ma->ma_name, ma->ma_slot);
	for(i = 0; i < 4; ++i) {
		aar = LDQP(TS_C_AAR0 + i * TS_STEP);
		printf("%s%dMB%s", i ? ", " : "", (8 << AAR_ASIZ(aar)) & ~0xf,
		    aar & AAR_SPLIT ? " (split)" : "");
	}
	printf(", Dchip 0 rev %d\n", (int)LDQP(TS_D_DREV) & 0xf);

	bzero(&tsp, sizeof tsp);
	tsp.tsp_name = "tsp";
	config_found(self, &tsp, NULL);

	if(LDQP(TS_C_CSC) & CSC_P1P) {
		++tsp.tsp_slot;
		config_found(self, &tsp, tscprint);
	}
}

static int
tscprint(aux, p)
	void *aux;
	const char *p;
{
	register struct tsp_attach_args *tsp = aux;

	if(p)
		printf("%s%d at %s", tsp->tsp_name, tsp->tsp_slot, p);
	return UNCONF;
}

#define tsp() { Generate ctags(1) key. }

int
tspmatch(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct tsp_attach_args *t = aux;

	return  cputype == ST_DEC_6600
	    && strcmp(t->tsp_name, tsp_cd.cd_name) == 0;
}

void
tspattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcibus_attach_args pba;
	struct tsp_attach_args *t = aux;
	struct tsp_config *pcp;

	printf("\n");
	pcp = tsp_init(1, t->tsp_slot);

	tsp_dma_init(pcp);
	
	/*
	 * Do PCI memory initialization that needs to be deferred until
	 * malloc is safe.  On the Tsunami, we need to do this after
	 * DMA is initialized, as well.
	 */
	tsp_bus_mem_init2(pcp);

	pci_6600_pickintr(pcp);

	pba.pba_busname = "pci";
	pba.pba_iot = &pcp->pc_iot;
	pba.pba_memt = &pcp->pc_memt;
	pba.pba_dmat =
	    alphabus_dma_get_tag(&pcp->pc_dmat_direct, ALPHA_BUS_PCI);
	pba.pba_pc = &pcp->pc_pc;
	pba.pba_bus = 0;
#ifdef	notyet
	pba.pba_flags = PCI_FLAGS_IO_ENABLED | PCI_FLAGS_MEM_ENABLED |
	    PCI_FLAGS_MRL_OKAY | PCI_FLAGS_MRM_OKAY | PCI_FLAGS_MWI_OKAY;
#endif
	config_found(self, &pba, tspprint);
}

struct tsp_config *
tsp_init(mallocsafe, n)
	int mallocsafe;
	int n;	/* Pchip number */
{
	struct tsp_config *pcp;

	KASSERT((n | 1) == 1);
	pcp = &tsp_configuration[n];
	pcp->pc_pslot = n;
	pcp->pc_iobase = TS_Pn(n, 0);
	pcp->pc_csr = S_PAGE(TS_Pn(n, P_CSRBASE));
	snprintf(pcp->pc_io_ex_name, sizeof pcp->pc_io_ex_name,
	    "tsp%d_bus_io", n);
	snprintf(pcp->pc_mem_ex_name, sizeof pcp->pc_mem_ex_name,
	    "tsp%d_bus_mem", n);
	    
	if (!pcp->pc_initted) {
		tsp_bus_io_init(&pcp->pc_iot, pcp);
		tsp_bus_mem_init(&pcp->pc_memt, pcp);
#if 0
		alpha_bus_window_count[ALPHA_BUS_TYPE_PCI_IO] = 1;
		alpha_bus_window_count[ALPHA_BUS_TYPE_PCI_MEM] = 1;

		alpha_bus_get_window = tsp_bus_get_window;
#endif 
	}
	pcp->pc_mallocsafe = mallocsafe;
	tsp_pci_init(&pcp->pc_pc, pcp);
	alpha_pci_chipset = &pcp->pc_pc;
	alpha_pci_chipset->pc_name = "tsunami";
	alpha_pci_chipset->pc_mem = P_PCI_MEM;
	alpha_pci_chipset->pc_ports = P_PCI_IO;
	alpha_pci_chipset->pc_hae_mask = 0;
	alpha_pci_chipset->pc_dense = TS_P0(0);
	alpha_pci_chipset->pc_bwx = 1;
	pcp->pc_initted = 1;
	return pcp;
}

static int
tspprint(aux, p)
	void *aux;
	const char *p;
{
	register struct pcibus_attach_args *pci = aux;

	if(p)
		printf("%s at %s", pci->pba_busname, p);
	printf(" bus %d", pci->pba_bus);
	return UNCONF;
}

#if 0
static int
tsp_bus_get_window(type, window, abst)
	int type, window;
	struct alpha_bus_space_translation *abst;
{
	struct tsp_config *tsp = &tsp_configuration[tsp_console_hose];
	bus_space_tag_t st;
	int error;

	switch (type) {
	case ALPHA_BUS_TYPE_PCI_IO:
		st = &tsp->pc_iot;
		break;

	case ALPHA_BUS_TYPE_PCI_MEM:
		st = &tsp->pc_memt;
		break;

	default:
		panic("tsp_bus_get_window");
	}

	error = alpha_bus_space_get_window(st, window, abst);
	if (error)
		return (error);

	abst->abst_sys_start = TS_PHYSADDR(abst->abst_sys_start);
	abst->abst_sys_end = TS_PHYSADDR(abst->abst_sys_end);

	return (0);
}
#endif

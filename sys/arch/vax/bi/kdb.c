/*	$OpenBSD: kdb.c,v 1.5 2001/11/06 19:53:17 miod Exp $ */
/*	$NetBSD: kdb.c,v 1.5 1997/01/11 11:34:39 ragge Exp $ */
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
 * KDB50 disk device driver
 */
/*
 * TODO
 *   Implement node reset routine.
 *   Nices hardware error handling.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <uvm/uvm_extern.h>

#include <machine/sid.h>
#include <machine/pte.h>
#include <machine/pcb.h>
#include <machine/trap.h>
#include <machine/scb.h>

#include <vax/bi/bireg.h>
#include <vax/bi/bivar.h>
#include <vax/bi/kdbreg.h>

#include <vax/mscp/mscp.h>
#include <vax/mscp/mscpvar.h>
#include <vax/mscp/mscpreg.h>

#define     b_forw  b_hash.le_next
/*
 * Software status, per controller.
 */
struct	kdb_softc {
	struct	device sc_dev;		/* Autoconfig info */
	struct	ivec_dsp sc_ivec;	/* Interrupt vector handler */
	struct	mscp_pack sc_kdb;	/* Struct for kdb communication */
	struct	mscp_softc *sc_softc;	/* MSCP info (per mscpvar.h) */
	struct	kdb_regs *sc_kr;	/* KDB controller registers */
	struct	mscp *sc_mscp;		/* Keep pointer to active mscp */
};

int	kdbmatch __P((struct device *, void *, void *));
void	kdbattach __P((struct device *, struct device *, void *));
void	kdbreset __P((int));
void	kdbintr __P((int));
void	kdbctlrdone __P((struct device *, int));
int	kdbprint __P((void *, const char *));
void	kdbsaerror __P((struct device *, int));
int	kdbgo __P((struct device *, struct buf *));

struct	cfdriver kdb_cd = {
	NULL, "kdb", DV_DULL
};

struct	cfattach kdb_ca = {
	sizeof(struct kdb_softc), kdbmatch, kdbattach
};

/*
 * More driver definitions, for generic MSCP code.
 */
struct	mscp_ctlr kdb_mscp_ctlr = {
	kdbctlrdone,
	kdbgo,
	kdbsaerror,
};

int
kdbprint(aux, name)
	void	*aux;
	const char	*name;
{
	if (name)
		printf("%s: mscpbus", name);
	return UNCONF;
}

/*
 * Poke at a supposed KDB to see if it is there.
 */
int
kdbmatch(parent, match, aux)
	struct	device *parent;
	void	*match, *aux;
{
	struct  cfdata *cf = match;
	struct bi_attach_args *ba = aux;

        if (ba->ba_node->biic.bi_dtype != BIDT_KDB50)
                return 0;

        if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != ba->ba_nodenr)
                return 0;

        return 1;
}

void
kdbattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct	kdb_softc *sc = (void *)self;
	struct	bi_attach_args *ba = aux;
	struct	mscp_attach_args ma;
	extern  struct ivec_dsp idsptch;
	volatile int i = 10000;

	printf("\n");
	bcopy(&idsptch, &sc->sc_ivec, sizeof(struct ivec_dsp));
	scb->scb_nexvec[1][ba->ba_nodenr] = &sc->sc_ivec;
	sc->sc_ivec.hoppaddr = kdbintr;
	sc->sc_ivec.pushlarg = self->dv_unit;
	sc->sc_kr = (void *)ba->ba_node;

	bzero(&sc->sc_kdb, sizeof (struct mscp_pack));

	ma.ma_mc = &kdb_mscp_ctlr;
	ma.ma_type = MSCPBUS_DISK|MSCPBUS_KDB;
	ma.ma_uuda = (struct mscp_pack *)kvtophys(&sc->sc_kdb);
	ma.ma_uda = &sc->sc_kdb;
	ma.ma_ip = &sc->sc_kr->kdb_ip;
	ma.ma_sa = &sc->sc_kr->kdb_sa;
	ma.ma_sw = &sc->sc_kr->kdb_sw;
	ma.ma_softc = &sc->sc_softc;
	ma.ma_ivec = (int)&scb->scb_nexvec[1][ba->ba_nodenr] - (int)scb;
	ma.ma_ctlrnr = ba->ba_nodenr;
	sc->sc_kr->kdb_bi.bi_csr |= BICSR_NRST;
	while (i--) /* Need delay??? */
		;
	sc->sc_kr->kdb_bi.bi_intrdes = ba->ba_intcpu;
	sc->sc_kr->kdb_bi.bi_bcicsr |= BCI_STOPEN | BCI_IDENTEN | BCI_UINTEN |
	    BCI_INTEN;
	sc->sc_kr->kdb_bi.bi_uintrcsr = ma.ma_ivec;
	config_found(&sc->sc_dev, &ma, kdbprint);
}

int
kdbgo(usc, bp)
	struct device *usc;
	struct buf *bp;
{
	struct kdb_softc *sc = (void *)usc;
	struct mscp_softc *mi = sc->sc_softc;
	struct mscp *mp = (void *)bp->b_actb;
        struct  pcb *pcb;
        pt_entry_t *pte;
        int     pfnum, npf, o, i;
	unsigned info = 0;
        caddr_t addr;

	o = (int)bp->b_un.b_addr & PGOFSET;
	npf = btoc(bp->b_bcount + o) + 1;
	addr = bp->b_un.b_addr;

        /*
         * Get a pointer to the pte pointing out the first virtual address.
         * Use different ways in kernel and user space.
         */
        if ((bp->b_flags & B_PHYS) == 0) {
                pte = kvtopte(addr);
        } else {
                pcb = bp->b_proc->p_vmspace->vm_map.pmap->pm_pcb;
                pte = uvtopte(addr, pcb);
        }

        /*
         * When we are doing DMA to user space, be sure that all pages
         * we want to transfer to is mapped. WHY DO WE NEED THIS???
         * SHOULDN'T THEY ALWAYS BE MAPPED WHEN DOING THIS???
         */
        for (i = 0; i < (npf - 1); i++) {
                if ((pte + i)->pg_pfn == 0) {
                        int rv;
                        rv = vm_fault(&bp->b_proc->p_vmspace->vm_map,
                            (unsigned)addr + i * NBPG,
                            VM_PROT_READ|VM_PROT_WRITE, FALSE);
                        if (rv)
                                panic("KDB DMA to nonexistent page, %d", rv);
                }
        }
	/*
	 * pte's for userspace isn't necessary positioned
	 * in consecutive physical pages. We check if they 
	 * are, otherwise we need to copy the pte's to a
	 * physically contigouos page area.
	 * XXX some copying here may be unneccessary. Subject to fix.
	 */
	if (bp->b_flags & B_PHYS) {
		int i = kvtophys(pte);
		unsigned k;

		if (trunc_page(i) != trunc_page(kvtophys(pte) + npf * 4)) {
			info = (unsigned)malloc(2 * NBPG, M_DEVBUF, M_WAITOK);
			k = (info + PGOFSET) & ~PGOFSET;
			bcopy(pte, (void *)k, NBPG);
			i = kvtophys(k);
		}
		mp->mscp_seq.seq_mapbase = i;
	} else
		mp->mscp_seq.seq_mapbase = (unsigned)pte;
	mscp_dgo(mi, KDB_MAP | o, info, bp);
	return 1;
}

void
kdbsaerror(usc, doreset)
	struct device *usc;
	int doreset;
{
	struct	kdb_softc *sc = (void *)usc;
	register int code = sc->sc_kr->kdb_sa;

	if ((code & MP_ERR) == 0)
		return;
	printf("%s: controller error, sa=0x%x\n", sc->sc_dev.dv_xname, code);
	/* What to do now??? */
}

/*
 * Interrupt routine.  Depending on the state of the controller,
 * continue initialisation, or acknowledge command and response
 * interrupts, and process responses.
 */
void
kdbintr(ctlr)
	int	ctlr;
{
	struct kdb_softc *sc = kdb_cd.cd_devs[ctlr];
	struct	uba_softc *uh;
	struct mscp_pack *ud;

	if (sc->sc_kr->kdb_sa & MP_ERR) {	/* ctlr fatal error */
		kdbsaerror(&sc->sc_dev, 1);
		return;
	}
	mscp_intr(sc->sc_softc);
}

#ifdef notyet
/*
 * The KDB50 has been reset.  Reinitialise the controller
 * and requeue outstanding I/O.
 */
void
kdbreset(ctlr)
	int ctlr;
{
	register struct kdb_softc *sc;

	sc = kdb_cd.cd_devs[ctlr];
	printf(" kdb%d", ctlr);


	/* reset queues and requeue pending transfers */
	mscp_requeue(sc->sc_softc);

	/*
	 * If it fails to initialise we will notice later and
	 * try again (and again...).  Do not call kdbstart()
	 * here; it will be done after the controller finishes
	 * initialisation.
	 */
	if (kdbinit(sc))
		printf(" (hung)");
}
#endif

void
kdbctlrdone(usc, info)
	struct device *usc;
	int info;
{
	if (info)
		free((void *)info, NBPG * 2);
}

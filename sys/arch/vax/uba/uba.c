/*      $NetBSD: uba.c,v 1.22 1996/04/08 18:37:34 ragge Exp $      */

/*
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)uba.c	7.10 (Berkeley) 12/16/90
 *      @(#)autoconf.c  7.20 (Berkeley) 5/9/91
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/nexus.h>
#include <machine/sid.h>
#include <machine/scb.h>
#include <machine/trap.h>
#include <machine/frame.h>

#include <vax/uba/ubareg.h>
#include <vax/uba/ubavar.h>

extern int cold;

volatile int rbr,rcvec;

int	uba_match __P((struct device *, void *, void *));
void	uba_attach __P((struct device *, struct device *, void *));
void	ubascan __P((struct device *, void *));
int	ubaprint __P((void *, const char *));
void	uba_dw780int __P((int));
void	ubaerror __P((int, struct uba_softc *, int *, int *,
	    struct uba_regs *));
void	ubainit __P((struct uba_softc *));
void	ubastray __P((int));
void	unifind __P((struct uba_softc *, caddr_t));
void	ubapurge __P((struct uba_ctlr *));
void	ubainitmaps __P((struct uba_softc *));
int	qbgetpri __P((void));
int	ubamem __P((int, int, int, int));
void    uba_dw780int __P((int));


struct	cfdriver uba_cd = {
	NULL, "uba", DV_DULL, 1
};

struct	cfattach uba_ca = {
	sizeof(struct uba_softc), uba_match, uba_attach
};

/* 
 * Stray interrupt vector handler, used when nowhere else to go to.
 */
void
ubastray(arg)
	int arg;
{
	struct	callsframe *cf = FRAMEOFFSET(arg);
	struct	uba_softc *sc = uba_cd.cd_devs[arg];
	struct  uba_regs *ur = sc->uh_uba;
	int	vektor;

	rbr = mfpr(PR_IPL);
#ifdef DW780
	if (sc->uh_type == DW780)
		vektor = ur->uba_brrvr[rbr - 0x14] >> 2;
	else
#endif
		vektor = (cf->ca_pc - (unsigned)&sc->uh_idsp[0]) >> 4;

	if (cold) {
#ifdef DW780
		if (sc->uh_type != DW780)
#endif
			rcvec = vektor;
	} else 
		printf("uba%d: unexpected interrupt, vector %o, br %d\n",
		    arg, vektor << 2, rbr - 20);
}

/*
 * Find devices on a UNIBUS.
 * Uses per-driver routine to set <br,cvec> into <r11,r10>,
 * and then fills in the tables, with help from a per-driver
 * slave initialization routine.
 */
void
unifind(uhp0, pumem)
	struct uba_softc *uhp0;
	caddr_t pumem;
{
	register struct uba_device *ui;
	register struct uba_ctlr *um;
	register struct uba_softc *uhp = uhp0;
	volatile struct uba_regs *ubar = uhp->uh_uba;
	u_short *reg, *ap, addr;
	struct uba_driver *udp;
	int i;
	volatile extern int rbr, rcvec;

#define	ubaddr(uhp, off)    (u_short *)((int)(uhp)->uh_iopage + ubdevreg(off))
	/*
	 * Check each unibus mass storage controller.
	 * For each one which is potentially on this uba,
	 * see if it is really there, and if it is record it and
	 * then go looking for slaves.
	 */
	for (um = ubminit; (udp = um->um_driver); um++) {
		if ((um->um_ubanum != uhp->uh_dev.dv_unit &&
		    um->um_ubanum != '?') || um->um_alive)
			continue;
		addr = (u_short)(u_long)um->um_addr;
		/*
		 * use the particular address specified first,
		 * or if it is given as "0", of there is no device
		 * at that address, try all the standard addresses
		 * in the driver til we find it
		 */
	    for (ap = udp->ud_addr; addr || (addr = *ap++); addr = 0) {
		reg = ubaddr(uhp, addr);
		if (badaddr((caddr_t)reg, 2))
			continue;

#if DW780
		if (uhp->uh_type == DW780 && ubar->uba_sr) {
			ubar->uba_sr = ubar->uba_sr;
			continue;
		}
#endif
		rcvec = 0x200;
		i = (*udp->ud_probe)((caddr_t)reg, um->um_ctlr, um, uhp);
#if DW780
		if (uhp->uh_type == DW780 && ubar->uba_sr) {
			ubar->uba_sr = ubar->uba_sr;
			continue;
		}
#endif
		if (i == 0)
			continue;
		printf("%s%d at uba%d csr %o ",
		    udp->ud_mname, um->um_ctlr, uhp->uh_dev.dv_unit, addr);
		if (rcvec == 0) {
			printf("zero vector\n");
			continue;
		}
		if (rcvec == 0x200) {
			printf("didn't interrupt\n");
			continue;
		}
		printf("vec %o, ipl %x\n", rcvec << 2, rbr);
		um->um_alive = 1;
		um->um_ubanum = uhp->uh_dev.dv_unit;
		um->um_hd = uhp;
		um->um_addr = (caddr_t)reg;
		udp->ud_minfo[um->um_ctlr] = um;
		uhp->uh_idsp[rcvec].hoppaddr = um->um_intr;
		uhp->uh_idsp[rcvec].pushlarg = um->um_ctlr;
		for (ui = ubdinit; ui->ui_driver; ui++) {
			int t;

			if (ui->ui_driver != udp || ui->ui_alive ||
			    (ui->ui_ctlr != um->um_ctlr && ui->ui_ctlr != '?') ||
			    (ui->ui_ubanum != uhp->uh_dev.dv_unit &&
			    ui->ui_ubanum != '?'))
				continue;
			t = ui->ui_ctlr;
			ui->ui_ctlr = um->um_ctlr;
			if ((*udp->ud_slave)(ui, (caddr_t)reg) == 0)
				ui->ui_ctlr = t;
			else {
				ui->ui_alive = 1;
				ui->ui_ubanum = uhp->uh_dev.dv_unit;
				ui->ui_hd = uhp;
				ui->ui_addr = (caddr_t)reg;
				ui->ui_physaddr = pumem + ubdevreg(addr);
				if (ui->ui_dk && dkn < DK_NDRIVE)
					ui->ui_dk = dkn++;
				else
					ui->ui_dk = -1;
				ui->ui_mi = um;
				/* ui_type comes from driver */
				udp->ud_dinfo[ui->ui_unit] = ui;
				printf("%s%d at %s%d slave %d",
				    udp->ud_dname, ui->ui_unit,
				    udp->ud_mname, um->um_ctlr, ui->ui_slave);
				(*udp->ud_attach)(ui);
				printf("\n");
			}
		}
		break;
	    }
	}
}


#ifdef DW780
char	ubasr_bits[] = UBASR_BITS;
#endif

#define	spluba	splbio		/* IPL 17 */

/*
 * Do transfer on device argument.  The controller
 * and uba involved are implied by the device.
 * We queue for resource wait in the uba code if necessary.
 * We return 1 if the transfer was started, 0 if it was not.
 *
 * The onq argument must be zero iff the device is not on the
 * queue for this UBA.  If onq is set, the device must be at the
 * head of the queue.  In any case, if the transfer is started,
 * the device will be off the queue, and if not, it will be on.
 *
 * Drivers that allocate one BDP and hold it for some time should
 * set ud_keepbdp.  In this case um_bdp tells which BDP is allocated
 * to the controller, unless it is zero, indicating that the controller
 * does not now have a BDP.
 */
int
ubaqueue(ui, onq)
	register struct uba_device *ui;
	int onq;
{
	register struct uba_ctlr *um = ui->ui_mi;
	register struct uba_softc *uh;
	register struct uba_driver *ud;
	register int s, unit;

	uh = uba_cd.cd_devs[um->um_ubanum];
	ud = um->um_driver;
	s = spluba();
	/*
	 * Honor exclusive BDP use requests.
	 */
	if ((ud->ud_xclu && uh->uh_users > 0) || uh->uh_xclu)
		goto rwait;
	if (ud->ud_keepbdp) {
		/*
		 * First get just a BDP (though in fact it comes with
		 * one map register too).
		 */
		if (um->um_bdp == 0) {
			um->um_bdp = uballoc(um->um_ubanum,
				(caddr_t)0, 0, UBA_NEEDBDP|UBA_CANTWAIT);
			if (um->um_bdp == 0)
				goto rwait;
		}
		/* now share it with this transfer */
		um->um_ubinfo = ubasetup(um->um_ubanum,
			um->um_tab.b_actf->b_actf,
			um->um_bdp|UBA_HAVEBDP|UBA_CANTWAIT);
	} else
		um->um_ubinfo = ubasetup(um->um_ubanum,
			um->um_tab.b_actf->b_actf, UBA_NEEDBDP|UBA_CANTWAIT);
	if (um->um_ubinfo == 0)
		goto rwait;
	uh->uh_users++;
	if (ud->ud_xclu)
		uh->uh_xclu = 1;
	splx(s);
	if (ui->ui_dk >= 0) {
		unit = ui->ui_dk;
		dk_busy |= 1<<unit;
		dk_xfer[unit]++;
		dk_wds[unit] += um->um_tab.b_actf->b_actf->b_bcount>>6;
	}
	if (onq)
		uh->uh_actf = ui->ui_forw;
	(*ud->ud_dgo)(um);
	return (1);
rwait:
	if (!onq) {
		ui->ui_forw = NULL;
		if (uh->uh_actf == NULL)
			uh->uh_actf = ui;
		else
			uh->uh_actl->ui_forw = ui;
		uh->uh_actl = ui;
	}
	splx(s);
	return (0);
}

void
ubadone(um)
	struct uba_ctlr *um;
{
	struct uba_softc *uh = uba_cd.cd_devs[um->um_ubanum];

	if (um->um_driver->ud_xclu)
		uh->uh_xclu = 0;
	uh->uh_users--;
	if (um->um_driver->ud_keepbdp)
		um->um_ubinfo &= ~BDPMASK;	/* keep BDP for misers */
	ubarelse(um->um_ubanum, &um->um_ubinfo);
}

/*
 * Allocate and setup UBA map registers, and bdp's
 * Flags says whether bdp is needed, whether the caller can't
 * wait (e.g. if the caller is at interrupt level).
 * Return value encodes map register plus page offset,
 * bdp number and number of map registers.
 */
int
ubasetup(uban, bp, flags)
	struct	buf *bp;
	int	uban, flags;
{
	struct uba_softc *uh = uba_cd.cd_devs[uban];
	struct pte *pte, *io;
	int npf;
	int pfnum, temp;
	int reg, bdp;
	unsigned v;
	struct proc *rp;
	int a, o, ubinfo;

#ifdef DW730
	if (uh->uh_type == DW730)
		flags &= ~UBA_NEEDBDP;
#endif
#ifdef QBA
	if (uh->uh_type == QBA)
		flags &= ~UBA_NEEDBDP;
#endif
	o = (int)bp->b_un.b_addr & PGOFSET;
	npf = btoc(bp->b_bcount + o) + 1;
	if (npf > UBA_MAXNMR)
		panic("uba xfer too big");
	a = spluba();
	while ((reg = rmalloc(uh->uh_map, (long)npf)) == 0) {
		if (flags & UBA_CANTWAIT) {
			splx(a);
			return (0);
		}
		uh->uh_mrwant++;
		sleep((caddr_t)&uh->uh_mrwant, PSWP);
	}
	if ((flags & UBA_NEED16) && reg + npf > 128) {
		/*
		 * Could hang around and try again (if we can ever succeed).
		 * Won't help any current device...
		 */
		rmfree(uh->uh_map, (long)npf, (long)reg);
		splx(a);
		return (0);
	}
	bdp = 0;
	if (flags & UBA_NEEDBDP) {
		while ((bdp = ffs((long)uh->uh_bdpfree)) == 0) {
			if (flags & UBA_CANTWAIT) {
				rmfree(uh->uh_map, (long)npf, (long)reg);
				splx(a);
				return (0);
			}
			uh->uh_bdpwant++;
			sleep((caddr_t)&uh->uh_bdpwant, PSWP);
		}
		uh->uh_bdpfree &= ~(1 << (bdp-1));
	} else if (flags & UBA_HAVEBDP)
		bdp = (flags >> 28) & 0xf;
	splx(a);
	reg--;
	ubinfo = UBAI_INFO(o, reg, npf, bdp);
	temp = (bdp << 21) | UBAMR_MRV;
	if (bdp && (o & 01))
		temp |= UBAMR_BO;
	if ((bp->b_flags & B_PHYS) == 0)
		pte = (struct pte *)kvtopte(bp->b_un.b_addr);
	else {
		struct	pte *hej;
		int	i;

		rp = bp->b_proc;
		v = btop((u_int)bp->b_un.b_addr&0x3fffffff);

		/*
		 * It may be better to use pmap_extract() here
		 * somewhere, but so far we do it "the hard way" :)
		 */
		if (((u_int)bp->b_un.b_addr < 0x40000000) ||
		    ((u_int)bp->b_un.b_addr > 0x7fffffff))
			hej = rp->p_vmspace->vm_pmap.pm_pcb->P0BR;
		else
			hej = rp->p_vmspace->vm_pmap.pm_pcb->P1BR;

		pte = &hej[v];
		for (i = 0; i < (npf - 1); i++) {
			if ((pte + i)->pg_pfn == 0) {
				int rv;

				rv = vm_fault(&rp->p_vmspace->vm_map,
				    (u_int)bp->b_un.b_addr + i * NBPG,
				    VM_PROT_READ, FALSE);
				if (rv)
					panic("DMA to nonexistent page");
			}
		}
	}
	io = &uh->uh_mr[reg];
	while (--npf > 0) {
		pfnum = pte->pg_pfn;
		if (pfnum == 0)
			panic("uba zero uentry");
		pte++;
		*(int *)io++ = pfnum | temp;
	}
	*(int *)io = 0;
	return (ubinfo);
}

/*
 * Non buffer setup interface... set up a buffer and call ubasetup.
 */
int
uballoc(uban, addr, bcnt, flags)
	caddr_t addr;
	int	uban, bcnt, flags;
{
	struct buf ubabuf;

	ubabuf.b_un.b_addr = addr;
	ubabuf.b_flags = B_BUSY;
	ubabuf.b_bcount = bcnt;
	/* that's all the fields ubasetup() needs */
	return (ubasetup(uban, &ubabuf, flags));
}
 
/*
 * Release resources on uba uban, and then unblock resource waiters.
 * The map register parameter is by value since we need to block
 * against uba resets on 11/780's.
 */
void
ubarelse(uban, amr)
	int uban, *amr;
{
	register struct uba_softc *uh = uba_cd.cd_devs[uban];
	register int bdp, reg, npf, s;
	int mr;
 
	/*
	 * Carefully see if we should release the space, since
	 * it may be released asynchronously at uba reset time.
	 */
	s = spluba();
	mr = *amr;
	if (mr == 0) {
		/*
		 * A ubareset() occurred before we got around
		 * to releasing the space... no need to bother.
		 */
		splx(s);
		return;
	}
	*amr = 0;
	bdp = UBAI_BDP(mr);
	if (bdp) {
		switch (uh->uh_type) {
#ifdef DWBUA
		case DWBUA:
			BUA(uh->uh_uba)->bua_dpr[bdp] |= BUADPR_PURGE;
			break;
#endif
#ifdef DW780
		case DW780:
			uh->uh_uba->uba_dpr[bdp] |= UBADPR_BNE;
			break;
#endif
#ifdef DW750
		case DW750:
			uh->uh_uba->uba_dpr[bdp] |=
			    UBADPR_PURGE|UBADPR_NXM|UBADPR_UCE;
			break;
#endif
		default:
			break;
		}
		uh->uh_bdpfree |= 1 << (bdp-1);		/* atomic */
		if (uh->uh_bdpwant) {
			uh->uh_bdpwant = 0;
			wakeup((caddr_t)&uh->uh_bdpwant);
		}
	}
	/*
	 * Put back the registers in the resource map.
	 * The map code must not be reentered,
	 * nor can the registers be freed twice.
	 * Unblock interrupts once this is done.
	 */
	npf = UBAI_NMR(mr);
	reg = UBAI_MR(mr) + 1;
	rmfree(uh->uh_map, (long)npf, (long)reg);
	splx(s);

	/*
	 * Wakeup sleepers for map registers,
	 * and also, if there are processes blocked in dgo(),
	 * give them a chance at the UNIBUS.
	 */
	if (uh->uh_mrwant) {
		uh->uh_mrwant = 0;
		wakeup((caddr_t)&uh->uh_mrwant);
	}
	while (uh->uh_actf && ubaqueue(uh->uh_actf, 1))
		;
}

void
ubapurge(um)
	register struct uba_ctlr *um;
{
	register struct uba_softc *uh = um->um_hd;
	register int bdp = UBAI_BDP(um->um_ubinfo);

	switch (uh->uh_type) {
#ifdef DWBUA
	case DWBUA:
		BUA(uh->uh_uba)->bua_dpr[bdp] |= BUADPR_PURGE;
		break;
#endif
#ifdef DW780
	case DW780:
		uh->uh_uba->uba_dpr[bdp] |= UBADPR_BNE;
		break;
#endif
#ifdef DW750
	case DW750:
		uh->uh_uba->uba_dpr[bdp] |= UBADPR_PURGE|UBADPR_NXM|UBADPR_UCE;
		break;
#endif
	default:
		break;
	}
}

void
ubainitmaps(uhp)
	register struct uba_softc *uhp;
{

	if (uhp->uh_memsize > UBA_MAXMR)
		uhp->uh_memsize = UBA_MAXMR;
	rminit(uhp->uh_map, (long)uhp->uh_memsize, (long)1, "uba", UAMSIZ);
	switch (uhp->uh_type) {
#ifdef DWBUA
	case DWBUA:
		uhp->uh_bdpfree = (1<<NBDPBUA) - 1;
		break;
#endif
#ifdef DW780
	case DW780:
		uhp->uh_bdpfree = (1<<NBDP780) - 1;
		break;
#endif
#ifdef DW750
	case DW750:
		uhp->uh_bdpfree = (1<<NBDP750) - 1;
		break;
#endif
	default:
		break;
	}
}

/*
 * Generate a reset on uba number uban.  Then
 * call each device that asked to be called during attach,
 * giving it a chance to clean up so as to be able to continue.
 */
void
ubareset(uban)
	int uban;
{
	register struct uba_softc *uh = uba_cd.cd_devs[uban];
	int s, i;

	s = spluba();
	uh->uh_users = 0;
	uh->uh_zvcnt = 0;
	uh->uh_xclu = 0;
	uh->uh_actf = uh->uh_actl = 0;
	uh->uh_bdpwant = 0;
	uh->uh_mrwant = 0;
	ubainitmaps(uh);
	wakeup((caddr_t)&uh->uh_bdpwant);
	wakeup((caddr_t)&uh->uh_mrwant);
	printf("%s: reset", uh->uh_dev.dv_xname);
	ubainit(uh);
#ifdef notyet
	ubameminit(uban);
#endif
	for (i = 0; i < uh->uh_resno; i++)
		(*uh->uh_reset[i])(uh->uh_resarg[i]);
	printf("\n");
	splx(s);
}

/*
 * Init a uba.
 */
void
ubainit(uhp)
	struct uba_softc *uhp;
{
	volatile struct uba_regs *ur = uhp->uh_uba;

	switch (uhp->uh_type) {
#ifdef DWBUA
	case DWBUA:
		BUA(uba)->bua_csr |= BUACSR_UPI;
		/* give devices time to recover from power fail */
		DELAY(500000);
		break;
#endif
#if DW780
	case DW780:
		ur->uba_cr = UBACR_ADINIT;
		ur->uba_cr = UBACR_IFS|UBACR_BRIE|UBACR_USEFIE|UBACR_SUEFIE;
		while ((ur->uba_cnfgr & UBACNFGR_UBIC) == 0)
			;
		break;
#endif
#ifdef DW750
	case DW750:
#endif
#ifdef DW730
	case DW730:
#endif
#ifdef QBA
	case QBA:
#endif
#if DW750 || DW730 || QBA
		mtpr(0, PR_IUR);
		/* give devices time to recover from power fail */

/* THIS IS PROBABLY UNNECESSARY */
		DELAY(500000);
/* END PROBABLY UNNECESSARY */

#ifdef QBA
		/*
		 * Re-enable local memory access
		 * from the Q-bus.
		 */
		if (uhp->uh_type == QBA)
			*((u_short *)(uhp->uh_iopage + QIPCR)) = Q_LMEAE;
#endif QBA
		break;
#endif DW750 || DW730 || QBA
	}
}

#ifdef QBA
/*
 * Determine the interrupt priority of a Q-bus
 * peripheral.  The device probe routine must spl6(),
 * attempt to make the device request an interrupt,
 * delaying as necessary, then call this routine
 * before resetting the device.
 */
int
qbgetpri()
{
#ifdef notyet
	int pri;
	extern int cvec;

	panic("qbgetpri");
	for (pri = 0x17; pri > 0x14; ) {
		if (cvec && cvec != 0x200)	/* interrupted at pri */
			break;
		pri--;
		splx(pri - 1);
	}
	(void) spl0();
	return (pri);
#else
	return 0x17;
#endif
}
#endif

#ifdef DW780
int	ubawedgecnt = 10;
int	ubacrazy = 500;
int	zvcnt_max = 5000;	/* in 8 sec */
/*
 * This routine is called by the locore code to process a UBA
 * error on an 11/780 or 8600.  The arguments are passed
 * on the stack, and value-result (through some trickery).
 * In particular, the uvec argument is used for further
 * uba processing so the result aspect of it is very important.
 * It must not be declared register.
 */
/*ARGSUSED*/
void
ubaerror(uban, uh, ipl, uvec, uba)
	register int uban;
	register struct uba_softc *uh;
	int *ipl, *uvec;
	register struct uba_regs *uba;
{
	register sr, s;

	if (*uvec == 0) {
		/*
		 * Declare dt as unsigned so that negative values
		 * are handled as >8 below, in case time was set back.
		 */
		u_long	dt = time.tv_sec - uh->uh_zvtime;

		uh->uh_zvtotal++;
		if (dt > 8) {
			uh->uh_zvtime = time.tv_sec;
			uh->uh_zvcnt = 0;
		}
		if (++uh->uh_zvcnt > zvcnt_max) {
			printf("uba%d: too many zero vectors (%d in <%d sec)\n",
				uban, uh->uh_zvcnt, (int)dt + 1);
			printf("\tIPL 0x%x\n\tcnfgr: %b  Adapter Code: 0x%x\n",
				*ipl, uba->uba_cnfgr&(~0xff), UBACNFGR_BITS,
				uba->uba_cnfgr&0xff);
			printf("\tsr: %b\n\tdcr: %x (MIC %sOK)\n",
				uba->uba_sr, ubasr_bits, uba->uba_dcr,
				(uba->uba_dcr&0x8000000)?"":"NOT ");
			ubareset(uban);
		}
		return;
	}
	if (uba->uba_cnfgr & NEX_CFGFLT) {
		printf("uba%d: sbi fault sr=%b cnfgr=%b\n",
		    uban, uba->uba_sr, ubasr_bits,
		    uba->uba_cnfgr, NEXFLT_BITS);
		ubareset(uban);
		*uvec = 0;
		return;
	}
	sr = uba->uba_sr;
	s = spluba();
	printf("uba%d: uba error sr=%b fmer=%x fubar=%o\n",
	    uban, uba->uba_sr, ubasr_bits, uba->uba_fmer, 4*uba->uba_fubar);
	splx(s);
	uba->uba_sr = sr;
	*uvec &= UBABRRVR_DIV;
	if (++uh->uh_errcnt % ubawedgecnt == 0) {
		if (uh->uh_errcnt > ubacrazy)
			panic("uba crazy");
		printf("ERROR LIMIT ");
		ubareset(uban);
		*uvec = 0;
		return;
	}
	return;
}
#endif

/*
 * Look for devices with unibus memory, allow them to configure, then disable
 * map registers as necessary.  Called during autoconfiguration and ubareset.
 * The device ubamem routine returns 0 on success, 1 on success if it is fully
 * configured (has no csr or interrupt, so doesn't need to be probed),
 * and -1 on failure.
 */
#ifdef notyet
ubameminit(uban)
{
	register struct uba_device *ui;
	register struct uba_softc *uh = uba_cd.cd_devs[uban];
	caddr_t umembase, addr;
#define	ubaoff(off)	((int)(off) & 0x1fff)

	umembase = uh->uh_iopage;
	uh->uh_lastmem = 0;
	for (ui = ubdinit; ui->ui_driver; ui++) {
		if (ui->ui_ubanum != uban && ui->ui_ubanum != '?')
			continue;
		if (ui->ui_driver->ud_ubamem) {
			/*
			 * During autoconfiguration, need to fudge ui_addr.
			 */
			addr = ui->ui_addr;
			ui->ui_addr = umembase + ubaoff(addr);
			switch ((*ui->ui_driver->ud_ubamem)(ui, uban)) {
			case 1:
				ui->ui_alive = 1;
				/* FALLTHROUGH */
			case 0:
				ui->ui_ubanum = uban;
				break;
			}
			ui->ui_addr = addr;
		}
	}
#ifdef DW780
jdhfgsjdkfhgsdjkfghak
	/*
	 * On a DW780, throw away any map registers disabled by rounding
	 * the map disable in the configuration register
	 * up to the next 8K boundary, or below the last unibus memory.
	 */
	if (uh->uh_type == DW780) {
		register i;

		i = btop(((uh->uh_lastmem + 8191) / 8192) * 8192);
		while (i)
			(void) rmget(uh->uh_map, 1, i--);
	}
#endif
}
#endif

/*
 * Allocate UNIBUS memory.  Allocates and initializes
 * sufficient mapping registers for access.  On a 780,
 * the configuration register is setup to disable UBA
 * response on DMA transfers to addresses controlled
 * by the disabled mapping registers.
 * On a DW780, should only be called from ubameminit, or in ascending order
 * from 0 with 8K-sized and -aligned addresses; freeing memory that isn't
 * the last unibus memory would free unusable map registers.
 * Doalloc is 1 to allocate, 0 to deallocate.
 */
int
ubamem(uban, addr, npg, doalloc)
	int uban, addr, npg, doalloc;
{
	register struct uba_softc *uh = uba_cd.cd_devs[uban];
	register int a;
	int s;

	a = (addr >> 9) + 1;
	s = spluba();
	if (doalloc)
		panic("uba: rmget");
/*		a = rmget(uh->uh_map, npg, a); */
	else
		rmfree(uh->uh_map, (long)npg, (long)a);
	splx(s);
	if (a) {
		register int i, *m;

		m = (int *)&uh->uh_mr[a - 1];
		for (i = 0; i < npg; i++)
			*m++ = 0;	/* All off, especially 'valid' */
		i = addr + npg * 512;
		if (doalloc && i > uh->uh_lastmem)
			uh->uh_lastmem = i;
		else if (doalloc == 0 && i == uh->uh_lastmem)
			uh->uh_lastmem = addr;
#ifdef DW780
		/*
		 * On a 780, set up the map register disable
		 * field in the configuration register.  Beware
		 * of callers that request memory ``out of order''
		 * or in sections other than 8K multiples.
		 * Ubameminit handles such requests properly, however.
		 */
		if (uh->uh_type == DW780) {
			i = uh->uh_uba->uba_cr &~ 0x7c000000;
			i |= ((uh->uh_lastmem + 8191) / 8192) << 26;
			uh->uh_uba->uba_cr = i;
		}
#endif
	}
	return (a);
}

#include "ik.h"
#include "vs.h"
#if NIK > 0 || NVS > 0
/*
 * Map a virtual address into users address space. Actually all we
 * do is turn on the user mode write protection bits for the particular
 * page of memory involved.
 */
maptouser(vaddress)
	caddr_t vaddress;
{

	kvtopte(vaddress)->pg_prot = (PG_UW >> 27);
}

unmaptouser(vaddress)
	caddr_t vaddress;
{

	kvtopte(vaddress)->pg_prot = (PG_KW >> 27);
}
#endif

#ifdef DW780
void
uba_dw780int(uba)
	int	uba;
{
	int	br, svec, vec, arg;
	struct	uba_softc *sc = uba_cd.cd_devs[uba];
	struct	uba_regs *ur = sc->uh_uba;
	void	(*func) __P((int));

	br = mfpr(PR_IPL);
	svec = ur->uba_brrvr[br - 0x14];
	if (svec < 0) {
		ubaerror(uba, sc, &br, &svec, ur);
		if (svec == 0)
			return;
	}
	vec = svec >> 2;
	if (cold)
		rcvec = vec;
	func = sc->uh_idsp[vec].hoppaddr;
	arg = sc->uh_idsp[vec].pushlarg;
	(*func)(arg);
}
#endif

/*
 * The match routine checks which UBA adapter number it is, to
 * be sure to use correct interrupt vectors.
 */
int
uba_match(parent, vcf, aux)
	struct	device *parent;
	void *vcf, *aux;
{
	struct sbi_attach_args *sa = (struct sbi_attach_args *)aux;
	struct cfdata *cf = vcf;

	if ((cf->cf_loc[0] != sa->nexnum) && (cf->cf_loc[0] > -1 ))
		return 0;

	switch (sa->type) {
	case NEX_UBA0:
		sa->nexinfo = 0;
		break;
	case NEX_UBA1:
		sa->nexinfo = 1;
		break;
	case NEX_UBA2:
		sa->nexinfo = 2;
		break;
	case NEX_UBA3:
		sa->nexinfo = 3;
		break;
	
	default:
		return 0;
	}
	return 1;
}

/*
 * The attach routines:
 *   Allocates interrupt vectors.
 *   Puts correct (cpu-specific) addresses in uba_softc.
 *   Calls the scan routine to search for uba devices.
 */
void
uba_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sbi_attach_args *sa = (struct sbi_attach_args *)aux;
	struct uba_regs *ubar = (struct uba_regs *)sa->nexaddr;
	struct uba_softc *sc = (struct uba_softc *)self;
	vm_offset_t	min, max, ubaphys, ubaiophys;
	extern	struct	ivec_dsp idsptch;

	printf("\n");
	/*
	 * Allocate place for unibus memory in virtual space.
	 * This is done with kmem_suballoc() but after that
	 * never used in the vm system. Is it OK to do so?
	 */
	(void)kmem_suballoc(kernel_map, &min, &max,
	    (UBAPAGES + UBAIOPAGES) * NBPG, FALSE);
	sc->uh_mem = (caddr_t)min;
	sc->uh_uba = (void*)ubar;
	sc->uh_memsize = UBAPAGES;
	sc->uh_iopage = (void *)min + (sc->uh_memsize * NBPG);
	sc->uh_iarea = (void *)scb + NBPG + sa->nexinfo * NBPG;
	sc->uh_resno = 0;
	/*
	 * Create interrupt dispatchers for this uba.
	 */
#define	NO_IVEC	128
	{
		vm_offset_t	iarea;
		int	i;

		iarea = kmem_alloc(kernel_map,
		    NO_IVEC * sizeof(struct ivec_dsp));
		sc->uh_idsp = (struct ivec_dsp *)iarea;

		for (i = 0; i < NO_IVEC; i++) {
			bcopy(&idsptch, &sc->uh_idsp[i],
			    sizeof(struct ivec_dsp));
			sc->uh_idsp[i].pushlarg = sc->uh_dev.dv_unit;
			sc->uh_idsp[i].hoppaddr = ubastray;
			sc->uh_iarea[i] = (unsigned int)&sc->uh_idsp[i];
		}
	}

	switch (cpunumber) {
#if VAX780 || VAX8600
	case VAX_780:
	case VAX_8600:
		sc->uh_mr = (void *)ubar->uba_map;
		sc->uh_type = DW780;
		sc->uh_physuba = (struct uba_regs *)kvtophys(sa->nexaddr);
		if (parent->dv_unit == 0) {
			ubaphys = UMEMA8600(sa->nexinfo);
			ubaiophys = UMEMA8600(sa->nexinfo) + (UBAPAGES * NBPG);
		} else {
			ubaphys = UMEMB8600(sa->nexinfo);
			ubaiophys = UMEMB8600(sa->nexinfo) + (UBAPAGES * NBPG);
		}
		bcopy(&idsptch, &sc->uh_dw780, sizeof(struct ivec_dsp));
		sc->uh_dw780.pushlarg = sc->uh_dev.dv_unit;
		sc->uh_dw780.hoppaddr = uba_dw780int;
		scb->scb_nexvec[0][sa->nexnum] = scb->scb_nexvec[1][sa->nexnum]
		    = scb->scb_nexvec[2][sa->nexnum]
		    = scb->scb_nexvec[3][sa->nexnum] = &sc->uh_dw780;
		break;
#endif
#if VAX750
	case VAX_750:
		sc->uh_mr = (void *)ubar->uba_map;
		sc->uh_type = DW750;
		sc->uh_physuba = (struct uba_regs *)kvtophys(sa->nexaddr);
		ubaphys = UMEM750(sa->nexinfo);
		ubaiophys = UMEM750(sa->nexinfo) + (UBAPAGES * NBPG);
		break;
#endif
#if VAX630 || VAX410
	case VAX_78032:
		switch (cpu_type) {
#if VAX630
		case VAX_630:
			sc->uh_mr = (void *)sa->nexaddr;
			sc->uh_type = QBA;
			sc->uh_physuba = (void*)QBAMAP630;
			ubaphys = QMEM630;
			ubaiophys = QIOPAGE630;
			break;
#endif
		default:
			ubaphys = QMEM630;
			ubaiophys = QIOPAGE630;
		};
		break;
#endif
#if VAX650
	case VAX_650:
		sc->uh_mr = (void *)sa->nexaddr;
		sc->uh_type = QBA;
		sc->uh_physuba = (void*)QBAMAP630; /* XXX */
		ubaphys = QMEM630; /* XXX */
		ubaiophys = QIOPAGE630; /* XXX */
		break;
#endif
	default:
		printf("Bad luck, this cputype does not support UBA's\n");
		return;
	};
	/*
	 * Map uba space in kernel virtual; especially i/o space.
	 */
	pmap_map(min, ubaphys, ubaphys + (UBAPAGES * NBPG),
	    VM_PROT_READ|VM_PROT_WRITE);
	pmap_map(min + (UBAPAGES * NBPG), ubaiophys, ubaiophys + 
	    (UBAIOPAGES * NBPG), VM_PROT_READ|VM_PROT_WRITE);
#if VAX630 || VAX650
	/* Enable access to local memory. */
	if (cpu_type == VAX_630 || cpunumber == VAX_650)
		*((u_short *)(sc->uh_iopage + QIPCR)) = Q_LMEAE;
#endif
	/*
	 * Initialize the UNIBUS, by freeing the map
	 * registers and the buffered data path registers
	 */
	sc->uh_map = (struct map *)malloc((u_long)
	    (UAMSIZ * sizeof(struct map)), M_DEVBUF, M_NOWAIT);
	bzero((caddr_t)sc->uh_map, (unsigned)(UAMSIZ * sizeof (struct map)));
	ubainitmaps(sc);

	/*
	 * Set last free interrupt vector for devices with
	 * programmable interrupt vectors.  Use is to decrement
	 * this number and use result as interrupt vector.
	 */
	sc->uh_lastiv = 0x200;

#ifdef DWBUA
        if (sc->uh_type == DWBUA)
                BUA(ubar)->bua_offset = (int)sc->uh_vec - (int)&scb[0];
#endif

#ifdef DW780
        if (sc->uh_type == DW780) {
                ubar->uba_sr = ubar->uba_sr;
                ubar->uba_cr = UBACR_IFS|UBACR_BRIE;
        }
#endif
#ifdef notyet
	/*
	 * First configure devices that have unibus memory,
	 * allowing them to allocate the correct map registers.
	 */
	ubameminit(uhp->uh_dev.dv_unit);
#endif
	/*
	 * Map the first page of UNIBUS i/o space to the first page of memory
	 * for devices which will need to dma output to produce an interrupt.
	 * ??? - Why? This is rpb page... /ragge
	 */
	*(int *)(&sc->uh_mr[0]) = UBAMR_MRV;

	/*
	 * Now start searching for devices.
	 */
	unifind(sc, (caddr_t)ubaiophys);/* Some devices are not yet converted */
	config_scan(ubascan,self);

#ifdef DW780
	if (sc->uh_type == DW780)
		ubar->uba_cr = UBACR_IFS | UBACR_BRIE |
		    UBACR_USEFIE | UBACR_SUEFIE |
		    (ubar->uba_cr & 0x7c000000);
#endif

}

void
ubascan(parent, match)
	struct device *parent;
	void *match;
{
	struct	device *dev = match;
	struct	cfdata *cf = dev->dv_cfdata;
	struct	uba_softc *sc = (struct uba_softc *)parent;
	volatile struct	uba_regs *ubar = sc->uh_uba;
	struct	uba_attach_args ua;
	int	i;

	ua.ua_addr = (caddr_t)ubaddr(sc, cf->cf_loc[0]);
	ua.ua_reset = NULL;

	if (badaddr(ua.ua_addr, 2))
		goto forgetit;

#ifdef DW780
	if (sc->uh_type == DW780 && ubar->uba_sr) {
	        ubar->uba_sr = ubar->uba_sr;
	        goto forgetit;
	}
#endif
	rcvec = 0x200;
	i = (*cf->cf_attach->ca_match) (parent, dev, &ua);

#ifdef DW780
	if (sc->uh_type == DW780 && ubar->uba_sr) {
	        ubar->uba_sr = ubar->uba_sr;
	        goto forgetit;
	}
#endif
	if (i == 0)
		goto forgetit;

	if (rcvec == 0 || rcvec == 0x200)
		goto fail;
		
	sc->uh_idsp[rcvec].hoppaddr = ua.ua_ivec;
	sc->uh_idsp[rcvec].pushlarg = dev->dv_unit;
	if (ua.ua_reset) { /* device wants ubaeset */
		if (sc->uh_resno == 0) {
			sc->uh_reset = malloc(1024, M_DEVBUF, M_NOWAIT);
			sc->uh_resarg = malloc(256, M_DEVBUF, M_NOWAIT);
		}
		sc->uh_resarg[sc->uh_resno] = dev->dv_unit;
		sc->uh_reset[sc->uh_resno++] = ua.ua_reset;
	}
	ua.ua_br = rbr;
	ua.ua_cvec = rcvec;
	ua.ua_iaddr = dev->dv_cfdata->cf_loc[0];

	config_attach(parent, dev, &ua, ubaprint);
	return;

fail:
	printf("%s at %s csr %o %s\n", dev->dv_cfdata->cf_driver->cd_name, 
	    parent->dv_xname, dev->dv_cfdata->cf_loc[0], 
	    rcvec ? "didn't interrupt\n" : "zero vector\n");

forgetit:
	free(dev, M_DEVBUF);
}

/*
 * Called when a device needs more than one interrupt vector.
 * (Like DHU11, DMF32). Argument is the device's softc, vector
 * number and a function pointer to the interrupt catcher.
 */
void
ubasetvec(dev, vec, func)
	struct	device *dev;
	int	vec;
	void	(*func) __P((int));
{
	struct	uba_softc *sc = (void *)dev->dv_parent;

	sc->uh_idsp[vec].hoppaddr = func;
	sc->uh_idsp[vec].pushlarg = dev->dv_unit;
}

/*
 * Print out some interesting info common to all unibus devices.
 */
int
ubaprint(aux, uba)
	void *aux;
	const char *uba;
{
	struct uba_attach_args *ua = aux;

	printf(" csr %o vec %o ipl %x", ua->ua_iaddr,
	    ua->ua_cvec << 2, ua->ua_br);
	return UNCONF;
}

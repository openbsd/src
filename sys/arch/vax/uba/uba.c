/*	$OpenBSD: uba.c,v 1.22 2006/01/20 23:27:26 miod Exp $	   */
/*	$NetBSD: uba.c,v 1.43 2000/01/24 02:40:36 matt Exp $	   */
/*
 * Copyright (c) 1996 Jonathan Stone.
 * Copyright (c) 1994, 1996 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1982, 1986 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)autoconf.c	7.20 (Berkeley) 5/9/91
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/systm.h>
#include <sys/extent.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/dkstat.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/device.h>

#include <uvm/uvm_extern.h>

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

volatile int /* rbr, rcvec,*/ svec;

static	int ubasearch(struct device *, struct cfdata *, void *);
static	int ubaprint(void *, const char *);
#if 0
static	void ubastray(int);
#endif
static	void ubainitmaps(struct uba_softc *);

extern struct cfdriver uba_cd;

#define spluba	spl7

#if defined(DW780) || defined(DW750)

int	dw_match(struct device *, struct cfdata *, void *);

int
dw_match(parent, cf, aux)
	struct	device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct sbi_attach_args *sa = (struct sbi_attach_args *)aux;

	if ((cf->cf_loc[0] != sa->nexnum) && (cf->cf_loc[0] > -1 ))
		return 0;

	/*
	 * The uba type is actually only telling where the uba 
	 * space is in nexus space.
	 */
	if ((sa->type & ~3) != NEX_UBA0)
		return 0;

	return 1;
}
#endif

#ifdef DW780
/*
 * The DW780 are directly connected to the SBI on 11/780 and 8600.
 */
void	dw780_attach(struct device *, struct device *, void *);
void	dw780_beforescan(struct uba_softc *);
void	dw780_afterscan(struct uba_softc *);
int	dw780_errchk(struct uba_softc *);
void	dw780_init(struct uba_softc *);
void	dw780_purge(struct uba_softc *, int);
void	uba_dw780int(int);
static	void ubaerror(struct uba_softc *, int *, int *);

struct	cfattach uba_sbi_ca = {
	sizeof(struct uba_softc), dw_match, dw780_attach
};

char	ubasr_bits[] = UBASR_BITS;

void
dw780_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct uba_softc *sc = (void *)self;
	struct sbi_attach_args *sa = aux;
	int ubaddr = sa->type & 3;
	int i;

	printf(": DW780\n");

	/*
	 * Fill in bus specific data.
	 */
	sc->uh_uba = (void *)sa->nexaddr;
	sc->uh_nbdp = NBDP780;
	sc->uh_nr = sa->nexnum * (parent->dv_unit + 1);
	sc->uh_beforescan = dw780_beforescan;
	sc->uh_afterscan = dw780_afterscan;
	sc->uh_errchk = dw780_errchk;
	sc->uh_ubapurge = dw780_purge;
	sc->uh_ubainit = dw780_init;
	sc->uh_type = DW780;
	sc->uh_memsize = UBAPAGES;
	sc->uh_ibase = VAX_NBPG + ubaddr * VAX_NBPG;
	sc->uh_mr = sc->uh_uba->uba_map;

	for (i = 0; i < 4; i++)
		scb_vecalloc(256 + i * 64 + sa->nexnum * 4, uba_dw780int,
		    sc->uh_dev.dv_unit, SCB_ISTACK);

	uba_attach(sc, (parent->dv_unit ? UMEMB8600(ubaddr) :
	    UMEMA8600(ubaddr)) + (UBAPAGES * VAX_NBPG));
}

void
dw780_beforescan(sc)
	struct uba_softc *sc;
{
	volatile int *hej = &sc->uh_uba->uba_sr;

	*hej = *hej;
	sc->uh_uba->uba_cr = UBACR_IFS|UBACR_BRIE;
}

void
dw780_afterscan(sc)
	struct uba_softc *sc;
{
	sc->uh_uba->uba_cr = UBACR_IFS | UBACR_BRIE |
	    UBACR_USEFIE | UBACR_SUEFIE |
	    (sc->uh_uba->uba_cr & 0x7c000000);
}

/*
 * On DW780 badaddr() in uba space sets a bit in uba_sr instead of
 * doing a machine check.
 */
int
dw780_errchk(sc)
	struct uba_softc *sc;
{
	volatile int *hej = &sc->uh_uba->uba_sr;

	if (*hej) {
		*hej = *hej;
		return 1;
	}
	return 0;
}

void
uba_dw780int(uba)
	int	uba;
{
	int	br, vec;
	struct	uba_softc *sc = uba_cd.cd_devs[uba];
	struct	uba_regs *ur = sc->uh_uba;

	br = mfpr(PR_IPL);
	vec = ur->uba_brrvr[br - 0x14];
	if (vec <= 0) {
		ubaerror(sc, &br, (int *)&vec);
		if (svec == 0)
			return;
	}
	if (cold)
		scb_fake(vec + sc->uh_ibase, br);
	else {
		struct ivec_dsp *scb_vec = (struct ivec_dsp *)((int)scb + 512 + vec * 4);
		(*scb_vec->hoppaddr)(scb_vec->pushlarg);

	}
}

void
dw780_init(sc)
	struct uba_softc *sc;
{
	sc->uh_uba->uba_cr = UBACR_ADINIT;
	sc->uh_uba->uba_cr = UBACR_IFS|UBACR_BRIE|UBACR_USEFIE|UBACR_SUEFIE;
	while ((sc->uh_uba->uba_cnfgr & UBACNFGR_UBIC) == 0)
		;
}

void
dw780_purge(sc, bdp)
	struct uba_softc *sc;
	int bdp;
{
	sc->uh_uba->uba_dpr[bdp] |= UBADPR_BNE;
}

int	ubawedgecnt = 10;
int	ubacrazy = 500;
int	zvcnt_max = 5000;	/* in 8 sec */
int	ubaerrcnt;
/*
 * This routine is called by the locore code to process a UBA
 * error on an 11/780 or 8600.	The arguments are passed
 * on the stack, and value-result (through some trickery).
 * In particular, the uvec argument is used for further
 * uba processing so the result aspect of it is very important.
 * It must not be declared register.
 */
/*ARGSUSED*/
void
ubaerror(uh, ipl, uvec)
	register struct uba_softc *uh;
	int *ipl, *uvec;
{
	struct	uba_regs *uba = uh->uh_uba;
	register int sr, s;

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
			printf("%s: too many zero vectors (%d in <%d sec)\n",
				uh->uh_dev.dv_xname, uh->uh_zvcnt, (int)dt + 1);
			printf("\tIPL 0x%x\n\tcnfgr: %b	 Adapter Code: 0x%x\n",
				*ipl, uba->uba_cnfgr&(~0xff), UBACNFGR_BITS,
				uba->uba_cnfgr&0xff);
			printf("\tsr: %b\n\tdcr: %x (MIC %sOK)\n",
				uba->uba_sr, ubasr_bits, uba->uba_dcr,
				(uba->uba_dcr&0x8000000)?"":"NOT ");
			ubareset(uh->uh_dev.dv_unit);
		}
		return;
	}
	if (uba->uba_cnfgr & NEX_CFGFLT) {
		printf("%s: sbi fault sr=%b cnfgr=%b\n",
		    uh->uh_dev.dv_xname, uba->uba_sr, ubasr_bits,
		    uba->uba_cnfgr, NEXFLT_BITS);
		ubareset(uh->uh_dev.dv_unit);
		*uvec = 0;
		return;
	}
	sr = uba->uba_sr;
	s = spluba();
	printf("%s: uba error sr=%b fmer=%x fubar=%o\n", uh->uh_dev.dv_xname,
	    uba->uba_sr, ubasr_bits, uba->uba_fmer, 4*uba->uba_fubar);
	splx(s);
	uba->uba_sr = sr;
	*uvec &= UBABRRVR_DIV;
	if (++ubaerrcnt % ubawedgecnt == 0) {
		if (ubaerrcnt > ubacrazy)
			panic("uba crazy");
		printf("ERROR LIMIT ");
		ubareset(uh->uh_dev.dv_unit);
		*uvec = 0;
		return;
	}
	return;
}
#endif

#ifdef DW750
/*
 * The DW780 and DW750 are quite similar to their function from
 * a programmers point of view. Differencies are number of BDP's
 * and bus status/command registers, the latter are (partly) IPR's
 * on 750.
 */
void	dw750_attach(struct device *, struct device *, void *);
void	dw750_init(struct uba_softc *);
void	dw750_purge(struct uba_softc *, int);

struct	cfattach uba_cmi_ca = {
	sizeof(struct uba_softc), dw_match, dw750_attach
};

void
dw750_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct uba_softc *sc = (void *)self;
	struct sbi_attach_args *sa = aux;
	int ubaddr = sa->nexinfo & 1;

	printf(": DW750\n");

	/*
	 * Fill in bus specific data.
	 */
	sc->uh_uba = (void *)sa->nexaddr;
	sc->uh_nbdp = NBDP750;
	sc->uh_nr = sa->nexnum;
	sc->uh_ubapurge = dw750_purge;
	sc->uh_ubainit = dw750_init;
	sc->uh_type = DW750;
	sc->uh_memsize = UBAPAGES;
	sc->uh_mr = sc->uh_uba->uba_map;

	uba_attach(sc, UMEM750(ubaddr) + (UBAPAGES * VAX_NBPG));
}

void
dw750_init(sc)
	struct uba_softc *sc;
{
	mtpr(0, PR_IUR);
	DELAY(500000);
}

void
dw750_purge(sc, bdp)
	struct uba_softc *sc;
	int bdp;
{
	sc->uh_uba->uba_dpr[bdp] |= UBADPR_PURGE | UBADPR_NXM | UBADPR_UCE;
}
#endif

#ifdef QBA
/*
 * The Q22 bus is the main IO bus on MicroVAX II/MicroVAX III systems.
 * It has an address space of 4MB (22 address bits), therefore the name,
 * and is hardware compatible with all 16 and 18 bits Q-bus devices.
 * This driver can only handle map registers up to 1MB due to map info
 * storage, but that should be enough for normal purposes.
 */
int	qba_match(struct device *, struct cfdata *, void *);
void	qba_attach(struct device *, struct device *, void *);
void	qba_beforescan(struct uba_softc*);
void	qba_init(struct uba_softc*);

struct	cfattach uba_mainbus_ca = {
	sizeof(struct uba_softc), qba_match, qba_attach
};

int
qba_match(parent, vcf, aux)
	struct device *parent;
	struct cfdata *vcf;
	void *aux;
{
	struct	bp_conf *bp = aux;

	if (strcmp(bp->type, "uba"))
		return 0;

	return 1;
}

void
qba_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct uba_softc *sc = (void *)self;

	printf(": Q22\n");
	/*
	 * Fill in bus specific data.
	 */
/*	sc->uh_uba not used; no regs */
/*	sc->uh_nbdp is 0; Qbus has no BDP's */
/*	sc->uh_nr is 0; there can be only one! */
/*	sc->uh_afterscan; not used */
/*	sc->uh_errchk; not used */
	sc->uh_beforescan = qba_beforescan;
	sc->uh_ubainit = qba_init;
	sc->uh_type = QBA;
	sc->uh_memsize = QBAPAGES;
	/*
	 * Map in the UBA page map into kernel space. On other UBAs,
	 * the map registers are in the bus IO space.
	 */
	sc->uh_mr = (void *)vax_map_physmem(QBAMAP,
	    (QBAPAGES * sizeof(pt_entry_t)) / VAX_NBPG);

	uba_attach(sc, QIOPAGE);
}

/*
 * Called when the QBA is set up; to enable DMA access from
 * QBA devices to main memory.
 */
void
qba_beforescan(sc)
	struct uba_softc *sc;
{
	*((u_short *)(sc->uh_iopage + QIPCR)) = Q_LMEAE;
}

void
qba_init(sc)
	struct uba_softc *sc;
{
	mtpr(0, PR_IUR);
	DELAY(500000);
	qba_beforescan(sc);
}
#endif
#ifdef DW730
struct	cfattach uba_dw730_ca = {
	sizeof(struct uba_softc), dw730_match, dw730_attach
};
#endif
#if 0
/* 
 * Stray interrupt vector handler, used when nowhere else to go to.
 */
void
ubastray(arg)
	int arg;
{
	struct	callsframe *cf = FRAMEOFFSET(arg);
	struct	uba_softc *sc = uba_cd.cd_devs[arg];
	int	vektor;

	rbr = mfpr(PR_IPL);
#ifdef DW780
	if (sc->uh_type == DW780)
		vektor = svec >> 2;
	else
#endif
		vektor = (cf->ca_pc - (unsigned)&sc->uh_idsp[0]) >> 4;

	if (cold) {
#ifdef DW780
		if (sc->uh_type != DW780)
#endif
			rcvec = vektor;
	} else 
		printf("uba%d: unexpected interrupt, vector 0x%x, br 0x%x\n",
		    arg, svec, rbr);
}
#endif
/*
 * Do transfer on device argument.  The controller
 * and uba involved are implied by the device.
 * We queue for resource wait in the uba code if necessary.
 * We return 1 if the transfer was started, 0 if it was not.
 *
 * The onq argument must be zero iff the device is not on the
 * queue for this UBA.	If onq is set, the device must be at the
 * head of the queue.  In any case, if the transfer is started,
 * the device will be off the queue, and if not, it will be on.
 *
 * Drivers that allocate one BDP and hold it for some time should
 * set ud_keepbdp.  In this case um_bdp tells which BDP is allocated
 * to the controller, unless it is zero, indicating that the controller
 * does not now have a BDP.
 */
int
ubaqueue(uu, bp)
	register struct uba_unit *uu;
	struct buf *bp;
{
	register struct uba_softc *uh;
	register int s;

	uh = (void *)((struct device *)(uu->uu_softc))->dv_parent;
	s = spluba();
	/*
	 * Honor exclusive BDP use requests.
	 */
	if ((uu->uu_xclu && uh->uh_users > 0) || uh->uh_xclu)
		goto rwait;
	if (uu->uu_keepbdp) {
		/*
		 * First get just a BDP (though in fact it comes with
		 * one map register too).
		 */
		if (uu->uu_bdp == 0) {
			uu->uu_bdp = uballoc(uh, (caddr_t)0, 0,
			    UBA_NEEDBDP|UBA_CANTWAIT);
			if (uu->uu_bdp == 0)
				goto rwait;
		}
		/* now share it with this transfer */
		uu->uu_ubinfo = ubasetup(uh, bp,
		    uu->uu_bdp|UBA_HAVEBDP|UBA_CANTWAIT);
	} else
		uu->uu_ubinfo = ubasetup(uh, bp, UBA_NEEDBDP|UBA_CANTWAIT);
	if (uu->uu_ubinfo == 0)
		goto rwait;
	uh->uh_users++;
	if (uu->uu_xclu)
		uh->uh_xclu = 1;

	splx(s);
	return (1);

rwait:
	SIMPLEQ_INSERT_TAIL(&uh->uh_resq, uu, uu_resq);
	splx(s);
	return (0);
}

void
ubadone(uu)
	struct uba_unit *uu;
{
	struct uba_softc *uh = (void *)((struct device *)
	    (uu->uu_softc))->dv_parent;

	if (uu->uu_xclu)
		uh->uh_xclu = 0;
	uh->uh_users--;
	if (uu->uu_keepbdp)
		uu->uu_ubinfo &= ~BDPMASK;	/* keep BDP for misers */
	ubarelse(uh, &uu->uu_ubinfo);
}

/*
 * Allocate and setup UBA map registers, and bdp's
 * Flags says whether bdp is needed, whether the caller can't
 * wait (e.g. if the caller is at interrupt level).
 * Return value encodes map register plus page offset,
 * bdp number and number of map registers.
 */
int
ubasetup(uh, bp, flags)
	struct	uba_softc *uh;
	struct	buf *bp;
	int	flags;
{
	int npf;
	int temp;
	int reg, bdp;
	int a, o, ubinfo;
	vaddr_t addr;

	if (uh->uh_nbdp == 0)
		flags &= ~UBA_NEEDBDP;

	o = (int)bp->b_data & VAX_PGOFSET;
	npf = vax_btoc(bp->b_bcount + o) + 1;
	if (npf > UBA_MAXNMR)
		panic("uba xfer too big");
	a = spluba();

	error = extent_alloc(uh->uh_ext, npf * VAX_NBPG, VAX_NBPG, 0,
	    EX_NOBOUNDARY, (flags & UBA_CANTWAIT) ? EX_NOWAIT : EX_WAITOK,
	    (u_long *)addr);

	if (error != 0) {
		splx(a);
		return (0);
	}

	reg = vax_btoc(addr);
	if ((flags & UBA_NEED16) && reg + npf > 128) {
		/*
		 * Could hang around and try again (if we can ever succeed).
		 * Won't help any current device...
		 */
		extent_free(uh->uh_ext, (u_long)addr, npf * VAX_NBPG,
		    EX_NOWAIT);
		splx(a);
		return (0);
	}
	bdp = 0;
	if (flags & UBA_NEEDBDP) {
		while ((bdp = ffs((long)uh->uh_bdpfree)) == 0) {
			if (flags & UBA_CANTWAIT) {
				extent_free(uh->uh_ext, (u_long)addr,
				    npf * VAX_NBPG, EX_NOWAIT);
				splx(a);
				return (0);
			}
			uh->uh_bdpwant++;
			tsleep((caddr_t)&uh->uh_bdpwant, PSWP, "ubasetup", 0);
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

	disk_reallymapin(bp, uh->uh_mr, reg, temp | PG_V);

	return (ubinfo);
}

/*
 * Non buffer setup interface... set up a buffer and call ubasetup.
 */
int
uballoc(uh, addr, bcnt, flags)
	struct	uba_softc *uh;
	caddr_t addr;
	int	bcnt, flags;
{
	struct buf ubabuf;

	ubabuf.b_data = addr;
	ubabuf.b_flags = B_BUSY;
	ubabuf.b_bcount = bcnt;
	/* that's all the fields ubasetup() needs */
	return (ubasetup(uh, &ubabuf, flags));
}
 
/*
 * Release resources on uba uban, and then unblock resource waiters.
 * The map register parameter is by value since we need to block
 * against uba resets on 11/780's.
 */
void
ubarelse(uh, amr)
	struct	uba_softc *uh;
	int	*amr;
{
	struct uba_unit *uu;
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
		if (uh->uh_ubapurge)
			(*uh->uh_ubapurge)(uh, bdp);

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
	extent_free(uh->uh_ext, reg * VAX_NBPG, npf * VAX_NBPG, EX_NOWAIT);
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
	while ((uu = SIMPLEQ_FIRST(&uh->uh_resq)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&uh->uh_resq, uu_resq);
		if ((*uu->uu_ready)(uu) == 0)
			break;
	}
}

void
ubainitmaps(uhp)
	register struct uba_softc *uhp;
{
	int error;

	if (uhp->uh_memsize > UBA_MAXMR)
		uhp->uh_memsize = UBA_MAXMR;
	uhp->uh_ext = extent_create("uba", 0, uhp->uh_memsize * VAX_NBPG,
	    M_DEVBUF, uhp->uh_extspace, EXTENT_FIXED_STORAGE_SIZE(UAMSIZ),
	    EX_NOWAIT);
	uhp->uh_bdpfree = (1 << uhp->uh_nbdp) - 1;
}

/*
 * Generate a reset on uba number uban.	 Then
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
	SIMPLEQ_INIT(&uh->uh_resq);
	uh->uh_bdpwant = 0;
	uh->uh_mrwant = 0;
	ubainitmaps(uh);
	wakeup((caddr_t)&uh->uh_bdpwant);
	wakeup((caddr_t)&uh->uh_mrwant);
	printf("%s: reset", uh->uh_dev.dv_xname);
	(*uh->uh_ubainit)(uh);

	for (i = 0; i < uh->uh_resno; i++)
		(*uh->uh_reset[i])(uh->uh_resarg[i]);
	printf("\n");
	splx(s);
}

#ifdef notyet
/*
 * Determine the interrupt priority of a Q-bus
 * peripheral.	The device probe routine must spl6(),
 * attempt to make the device request an interrupt,
 * delaying as necessary, then call this routine
 * before resetting the device.
 */
int
qbgetpri()
{
	int pri;

	for (pri = 0x17; pri > 0x14; ) {
		if (rcvec && rcvec != 0x200)	/* interrupted at pri */
			break;
		pri--;
		splx(pri - 1);
	}
	spl0();
	return (pri);
}
#endif

/*
 * The common attach routines:
 *   Allocates interrupt vectors.
 *   Puts correct values in uba_softc.
 *   Calls the scan routine to search for uba devices.
 */
void
uba_attach(sc, iopagephys)
	struct uba_softc *sc;
	paddr_t iopagephys;
{

	/*
	 * Set last free interrupt vector for devices with
	 * programmable interrupt vectors.  Use is to decrement
	 * this number and use result as interrupt vector.
	 */
	sc->uh_lastiv = 0x200;
	SIMPLEQ_INIT(&sc->uh_resq);

	/*
	 * Allocate place for unibus memory in virtual space.
	 */
	sc->uh_iopage = (caddr_t)vax_map_physmem(iopagephys, UBAIOPAGES);
	if (sc->uh_iopage == 0)
		return;	/* vax_map_physmem() will complain for us */
	/*
	 * Initialize the UNIBUS, by freeing the map
	 * registers and the buffered data path registers
	 */
	sc->uh_extspace = (char *)malloc(EXTENT_FIXED_STORAGE_SIZE(UAMSIZ),
	    M_DEVBUF, M_NOWAIT);
	if (sc->uh_extspace == NULL)
		panic("uba_attach");
	ubainitmaps(sc);

	/*
	 * Map the first page of UNIBUS i/o space to the first page of memory
	 * for devices which will need to dma to produce an interrupt.
	 */
	*(int *)(&sc->uh_mr[0]) = UBAMR_MRV;

	if (sc->uh_beforescan)
		(*sc->uh_beforescan)(sc);
	/*
	 * Now start searching for devices.
	 */
	config_search(ubasearch,(struct device *)sc, NULL);

	if (sc->uh_afterscan)
		(*sc->uh_afterscan)(sc);
}

int
ubasearch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{
	struct	uba_softc *sc = (struct uba_softc *)parent;
	struct	uba_attach_args ua;
	int	i, vec, br;

	ua.ua_addr = (caddr_t)((int)sc->uh_iopage + ubdevreg(cf->cf_loc[0]));
	ua.ua_reset = NULL;

	if (badaddr(ua.ua_addr, 2) || (sc->uh_errchk ? (*sc->uh_errchk)(sc):0))
		goto forgetit;

	scb_vecref(0, 0); /* Clear vector ref */
	i = (*cf->cf_attach->ca_match) (parent, cf, &ua);

	if (sc->uh_errchk)
		if ((*sc->uh_errchk)(sc))
			goto forgetit;
	if (i == 0)
		goto forgetit;

	i = scb_vecref(&vec, &br);
	if (i == 0)
		goto fail;
	if (vec == 0)
		goto fail;
		
	scb_vecalloc(vec, ua.ua_ivec, cf->cf_unit, SCB_ISTACK);
	if (ua.ua_reset) { /* device wants ubareset */
		if (sc->uh_resno == 0) {
			sc->uh_reset = malloc(1024, M_DEVBUF, M_NOWAIT);
			if (sc->uh_reset == NULL)
				panic("ubasearch");
			sc->uh_resarg = (int *)sc->uh_reset + 128;
		}
#ifdef DIAGNOSTIC
		if (sc->uh_resno > 127) {
			printf("%s: Expand reset table, skipping reset %s%d\n",
			    sc->uh_dev.dv_xname, cf->cf_driver->cd_name,
			    cf->cf_unit);
		} else
#endif
		{
			sc->uh_resarg[sc->uh_resno] = cf->cf_unit;
			sc->uh_reset[sc->uh_resno++] = ua.ua_reset;
		}
	}
	ua.ua_br = br;
	ua.ua_cvec = vec;
	ua.ua_iaddr = cf->cf_loc[0];

	config_attach(parent, cf, &ua, ubaprint);
	return 0;

fail:
	printf("%s%d at %s csr %o %s\n",
	    cf->cf_driver->cd_name, cf->cf_unit, parent->dv_xname,
	    cf->cf_loc[0], (i ? "zero vector" : "didn't interrupt"));

forgetit:
	return 0;
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

	printf(" csr %o vec %d ipl %x", ua->ua_iaddr,
	    ua->ua_cvec & 511, ua->ua_br);
	return UNCONF;
}

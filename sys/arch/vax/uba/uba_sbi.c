/*	$OpenBSD: uba_sbi.c,v 1.5 2008/08/18 23:09:42 miod Exp $	*/
/*	$NetBSD: uba_sbi.c,v 1.1 1999/06/21 16:23:01 ragge Exp $	   */
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
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#define _VAX_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/mtpr.h>
#include <machine/nexus.h>
#include <machine/cpu.h>
#include <machine/sgmap.h>
#include <machine/scb.h>

#include <arch/vax/qbus/ubavar.h>

#include <arch/vax/uba/uba_common.h>

/* Some SBI-specific defines */
#define UBASIZE		(UBAPAGES * VAX_NBPG)
#define UMEMA8600(i)   	(0x20100000+(i)*0x40000)
#define	UMEMB8600(i)	(0x22100000+(i)*0x40000)

/*
 * Some status registers.
 */
#define UBACNFGR_UBIC  0x00010000      /* unibus init complete */
#define UBACNFGR_BITS \
"\40\40PARFLT\37WSQFLT\36URDFLT\35ISQFLT\34MXTFLT\33XMTFLT\30ADPDN\27ADPUP\23UBINIT\22UBPDN\21UBIC"

#define UBACR_IFS      0x00000040      /* interrupt field switch */
#define UBACR_BRIE     0x00000020      /* BR interrupt enable */
#define UBACR_USEFIE   0x00000010      /* UNIBUS to SBI error field IE */
#define UBACR_SUEFIE   0x00000008      /* SBI to UNIBUS error field IE */
#define UBACR_ADINIT   0x00000001      /* adapter init */

#define UBADPR_BNE     0x80000000      /* buffer not empty - purge */

#define UBABRRVR_DIV    0x0000ffff      /* device interrupt vector field */

#define UBASR_BITS \
"\20\13RDTO\12RDS\11CRD\10CXTER\7CXTMO\6DPPE\5IVMR\4MRPF\3LEB\2UBSTO\1UBSSYNTO"

char    ubasr_bits[] = UBASR_BITS;

/*
 * The DW780 are directly connected to the SBI on 11/780 and 8600.
 */
static	int	dw780_match(struct device *, struct cfdata *, void *);
static	void	dw780_attach(struct device *, struct device *, void *);
static	void	dw780_init(struct uba_softc*);
static	void    dw780_beforescan(struct uba_softc *);
static	void    dw780_afterscan(struct uba_softc *);
static	int     dw780_errchk(struct uba_softc *);
static	void    uba_dw780int(int);
static  void	ubaerror(struct uba_softc *, int *, int *);
#ifdef notyet
static	void	dw780_purge(struct uba_softc *, int);
#endif

struct	cfattach uba_sbi_ca = {
	sizeof(struct uba_vsoftc), dw780_match, dw780_attach
};

extern	struct vax_bus_space vax_mem_bus_space;

volatile int svec;

int
dw780_match(parent, cf, aux)
	struct device *parent;
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

void
dw780_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct uba_vsoftc *sc = (void *)self;
	struct sbi_attach_args *sa = aux;
	int ubaddr = sa->type & 3;
	int i;

	printf(": DW780\n");
	/*
	 * Fill in bus specific data.
	 */
	sc->uv_sc.uh_ubainit = dw780_init;
#ifdef notyet
	sc->uv_sc.uh_ubapurge = dw780_purge;
#endif
	sc->uv_sc.uh_beforescan = dw780_beforescan;
	sc->uv_sc.uh_afterscan = dw780_afterscan;
	sc->uv_sc.uh_errchk = dw780_errchk;
	sc->uv_sc.uh_iot = &vax_mem_bus_space;
	sc->uv_sc.uh_dmat = &sc->uv_dmat;
	sc->uv_uba = (void *)sa->nexaddr;
	sc->uh_ibase = VAX_NBPG + ubaddr * VAX_NBPG;

	/*
	 * Set up dispatch vectors for DW780.
	 */
	for (i = 0; i < 4; i++)
		scb_vecalloc(256 + i * 64 + sa->nexnum * 4, uba_dw780int,
		    sc->uv_sc.uh_dev.dv_unit, SCB_ISTACK);
	/*
	 * Fill in variables used by the sgmap system.
	 */
	sc->uv_size = UBASIZE;		/* Size in bytes of Qbus space */
	sc->uv_addr = (paddr_t)sc->uv_uba->uba_map;

	uba_dma_init(sc);
	uba_attach(&sc->uv_sc, (parent->dv_unit ? UMEMB8600(ubaddr) :
	    UMEMA8600(ubaddr)) + (UBAPAGES * VAX_NBPG));
}

void
dw780_beforescan(sc)
	struct uba_softc *sc;
{
	struct uba_vsoftc *vc = (void *)sc;
	volatile int *hej = &vc->uv_uba->uba_sr;

	*hej = *hej;
	vc->uv_uba->uba_cr = UBACR_IFS|UBACR_BRIE;
}

void
dw780_afterscan(sc)
	struct uba_softc *sc;
{
	struct uba_vsoftc *vc = (void *)sc;

	vc->uv_uba->uba_cr = UBACR_IFS | UBACR_BRIE |
	    UBACR_USEFIE | UBACR_SUEFIE |
	    (vc->uv_uba->uba_cr & 0x7c000000);
}


int
dw780_errchk(sc)
	struct uba_softc *sc;
{
	struct uba_vsoftc *vc = (void *)sc;
	volatile int *hej = &vc->uv_uba->uba_sr;

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
	int	br, vec, arg;
	struct	uba_vsoftc *vc = uba_cd.cd_devs[uba];
	struct	uba_regs *ur = vc->uv_uba;
	void	(*func)(int);

	br = mfpr(PR_IPL);
	vec = ur->uba_brrvr[br - 0x14];
	if (vec <= 0) {
		ubaerror(&vc->uv_sc, &br, (int *)&vec);
		if (svec == 0)
			return;
	}
	if (cold)
		scb_fake(vec + vc->uh_ibase, br);
	else {
		struct ivec_dsp *scb_vec = (struct ivec_dsp *)((int)scb + 512);
		func = scb_vec[vec/4].hoppaddr;
		arg = scb_vec[vec/4].pushlarg;
		(*func)(arg);
		scb_vec[vec/4].ev->ec_count++;
	}
}

void
dw780_init(sc)
	struct uba_softc *sc;
{
	struct uba_vsoftc *vc = (void *)sc;

	vc->uv_uba->uba_cr = UBACR_ADINIT;
	vc->uv_uba->uba_cr = UBACR_IFS|UBACR_BRIE|UBACR_USEFIE|UBACR_SUEFIE;
	while ((vc->uv_uba->uba_cnfgr & UBACNFGR_UBIC) == 0)
		;
}

#ifdef notyet
void
dw780_purge(sc, bdp)
	struct uba_softc *sc;
	int bdp;
{
	struct uba_vsoftc *vc = (void *)sc;

	vc->uv_uba->uba_dpr[bdp] |= UBADPR_BNE;
}
#endif

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
	struct uba_vsoftc *vc = (void *)uh;
	struct	uba_regs *uba = vc->uv_uba;
	register int sr, s;

	if (*uvec == 0) {
		/*
		 * Declare dt as unsigned so that negative values
		 * are handled as >8 below, in case time was set back.
		 */
		u_long	dt = time.tv_sec - vc->uh_zvtime;

		vc->uh_zvtotal++;
		if (dt > 8) {
			vc->uh_zvtime = time.tv_sec;
			vc->uh_zvcnt = 0;
		}
		if (++vc->uh_zvcnt > zvcnt_max) {
			printf("%s: too many zero vectors (%d in <%d sec)\n",
				vc->uv_sc.uh_dev.dv_xname, vc->uh_zvcnt, (int)dt + 1);
			printf("\tIPL 0x%x\n\tcnfgr: %b	 Adapter Code: 0x%x\n",
				*ipl, uba->uba_cnfgr&(~0xff), UBACNFGR_BITS,
				uba->uba_cnfgr&0xff);
			printf("\tsr: %b\n\tdcr: %x (MIC %sOK)\n",
				uba->uba_sr, ubasr_bits, uba->uba_dcr,
				(uba->uba_dcr&0x8000000)?"":"NOT ");
			ubareset(vc->uv_sc.uh_dev.dv_unit);
		}
		return;
	}
	if (uba->uba_cnfgr & NEX_CFGFLT) {
		printf("%s: sbi fault sr=%b cnfgr=%b\n",
		    vc->uv_sc.uh_dev.dv_xname, uba->uba_sr, ubasr_bits,
		    uba->uba_cnfgr, NEXFLT_BITS);
		ubareset(vc->uv_sc.uh_dev.dv_unit);
		*uvec = 0;
		return;
	}
	sr = uba->uba_sr;
	s = spluba();
	printf("%s: uba error sr=%b fmer=%x fubar=%o\n", vc->uv_sc.uh_dev.dv_xname,
	    uba->uba_sr, ubasr_bits, uba->uba_fmer, 4*uba->uba_fubar);
	splx(s);
	uba->uba_sr = sr;
	*uvec &= UBABRRVR_DIV;
	if (++ubaerrcnt % ubawedgecnt == 0) {
		if (ubaerrcnt > ubacrazy)
			panic("uba crazy");
		printf("ERROR LIMIT ");
		ubareset(vc->uv_sc.uh_dev.dv_unit);
		*uvec = 0;
	}
}

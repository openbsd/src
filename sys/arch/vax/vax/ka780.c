/*	$OpenBSD: ka780.c,v 1.10 2008/08/18 23:05:38 miod Exp $	*/
/*	$NetBSD: ka780.c,v 1.14 1999/08/07 10:36:49 ragge Exp $ */
/*-
 * Copyright (c) 1982, 1986, 1988 The Regents of the University of California.
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
 *	@(#)ka780.c	7.4 (Berkeley) 5/9/91
 */

/*
 * 780-specific code.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>

#include <machine/nexus.h>
#include <machine/sid.h>
#include <machine/cpu.h>
#include <machine/clock.h>

static	void	ka780_memerr(void);
static	int	ka780_mchk(caddr_t);
static	void	ka780_conf(void);
static	int mem_sbi_match(struct device *, struct cfdata *, void *);
static	void mem_sbi_attach(struct device *, struct device *, void *);

struct	cfattach mem_sbi_ca = {
	sizeof(struct mem_softc), mem_sbi_match, mem_sbi_attach
};

int	
mem_sbi_match(parent, cf, aux)
	struct	device	*parent;
	struct cfdata *cf;
	void	*aux;
{
	struct	sbi_attach_args *sa = (struct sbi_attach_args *)aux;

	if (cf->cf_loc[SBICF_TR] != sa->nexnum && cf->cf_loc[SBICF_TR] > -1)
		return 0;

	switch (sa->type) {
	case NEX_MEM4:
	case NEX_MEM4I:
	case NEX_MEM16:
	case NEX_MEM16I:
		sa->nexinfo = M780C;
		break;

	case NEX_MEM64I:
	case NEX_MEM64L:
	case NEX_MEM64LI:
	case NEX_MEM256I:
	case NEX_MEM256L:
	case NEX_MEM256LI:
		sa->nexinfo = M780EL;
		break;

	case NEX_MEM64U:
	case NEX_MEM64UI:
	case NEX_MEM256U:
	case NEX_MEM256UI:
		sa->nexinfo = M780EU;
		break;
 
	default:
		return 0;
	}
	return 1;
}


/*
 * Declaration of 780-specific calls.
 */
struct	cpu_dep ka780_calls = {
	0,
	ka780_mchk,
	ka780_memerr,
	ka780_conf,
	generic_clkread,
	generic_clkwrite,
	2,	/* ~VUPS */
	5,	/* SCB pages */
	NULL,
	NULL,
	NULL,
	NULL,
	hardclock
};

/*
 * Memory controller register usage varies per controller.
 */
struct	mcr780 {
	int	mc_reg[4];
};

#define M780_ICRD	0x40000000	/* inhibit crd interrupts, in [2] */
#define M780_HIER	0x20000000	/* high error rate, in reg[2] */
#define M780_ERLOG	0x10000000	/* error log request, in reg[2] */
/* on a 780, memory crd's occur only when bit 15 is set in the SBIER */
/* register; bit 14 there is an error bit which we also clear */
/* these bits are in the back of the ``red book'' (or in the VMS code) */

#define M780C_INH(mcr)	\
	((mcr)->mc_reg[2] = (M780_ICRD|M780_HIER|M780_ERLOG)); \
	    mtpr(0, PR_SBIER);
#define M780C_ENA(mcr)	\
	((mcr)->mc_reg[2] = (M780_HIER|M780_ERLOG)); mtpr(3<<14, PR_SBIER);
#define M780C_ERR(mcr)	\
	((mcr)->mc_reg[2] & (M780_ERLOG))

#define M780C_SYN(mcr)	((mcr)->mc_reg[2] & 0xff)
#define M780C_ADDR(mcr) (((mcr)->mc_reg[2] >> 8) & 0xfffff)

#define M780EL_INH(mcr) \
	((mcr)->mc_reg[2] = (M780_ICRD|M780_HIER|M780_ERLOG)); \
	    mtpr(0, PR_SBIER);
#define M780EL_ENA(mcr) \
	((mcr)->mc_reg[2] = (M780_HIER|M780_ERLOG)); mtpr(3<<14, PR_SBIER);
#define M780EL_ERR(mcr) \
	((mcr)->mc_reg[2] & (M780_ERLOG))

#define M780EL_SYN(mcr)		((mcr)->mc_reg[2] & 0x7f)
#define M780EL_ADDR(mcr)	(((mcr)->mc_reg[2] >> 11) & 0x1ffff)

#define M780EU_INH(mcr) \
	((mcr)->mc_reg[3] = (M780_ICRD|M780_HIER|M780_ERLOG)); \
	    mtpr(0, PR_SBIER);
#define M780EU_ENA(mcr) \
	((mcr)->mc_reg[3] = (M780_HIER|M780_ERLOG)); mtpr(3<<14, PR_SBIER);
#define M780EU_ERR(mcr) \
	((mcr)->mc_reg[3] & (M780_ERLOG))

#define M780EU_SYN(mcr)		((mcr)->mc_reg[3] & 0x7f)
#define M780EU_ADDR(mcr)	(((mcr)->mc_reg[3] >> 11) & 0x1ffff)

/* enable crd interrrupts */
void
mem_sbi_attach(parent, self, aux)
	struct	device	*parent, *self;
	void	*aux;
{
	struct	sbi_attach_args *sa = (struct sbi_attach_args *)aux;
	struct	mem_softc *sc = (void *)self;
	struct mcr780 *mcr = (void *)sa->nexaddr;

	sc->sc_memaddr = sa->nexaddr;
	sc->sc_memtype = sa->nexinfo;
	sc->sc_memnr = sa->type;

	printf(": ");
	switch (sc->sc_memtype) {

	case M780C:
		printf("standard");
		M780C_ENA(mcr);
		break;

	case M780EL:
		printf("(el) ");
		M780EL_ENA(mcr);
		if (sc->sc_memnr != NEX_MEM64I && sc->sc_memnr != NEX_MEM256I)
			break;

	case M780EU:
		printf("(eu)");
		M780EU_ENA(mcr);
		break;
	}
	printf("\n");
}

/* log crd errors */
void
ka780_memerr()
{
	extern struct cfdriver mem_cd;
	struct	mem_softc *sc;
	register struct mcr780 *mcr;
	register int m;

	for (m = 0; m < mem_cd.cd_ndevs; m++) {
		if (mem_cd.cd_devs[m] == 0)
			continue;

		sc = (void *)mem_cd.cd_devs[m];
		mcr = (struct mcr780 *)sc->sc_memaddr;
		switch (sc->sc_memtype) {

		case M780C:
			if (M780C_ERR(mcr)) {
				printf("%s: soft ecc addr %x syn %x\n",
				    sc->sc_dev.dv_xname, M780C_ADDR(mcr),
				    M780C_SYN(mcr));
#ifdef TRENDATA
				memlog(m, mcr);
#endif
				M780C_INH(mcr);
			}
			break;

		case M780EL:
			if (M780EL_ERR(mcr)) {
				printf("%s: soft ecc addr %x syn %x\n",
				    sc->sc_dev.dv_xname, M780EL_ADDR(mcr),
				    M780EL_SYN(mcr));
				M780EL_INH(mcr);
			}
			if (sc->sc_memnr != NEX_MEM64I &&
			    sc->sc_memnr != NEX_MEM256I)
				break;

		case M780EU:
			if (M780EU_ERR(mcr)) {
				printf("%s: soft ecc addr %x syn %x\n",
				    sc->sc_dev.dv_xname, M780EU_ADDR(mcr),
				    M780EU_SYN(mcr));
				M780EU_INH(mcr);
			}
			break;
		}
	}
}

#ifdef TRENDATA
/*
 * Figure out what chip to replace on Trendata boards.
 * Assumes all your memory is Trendata or the non-Trendata
 * memory never fails..
 */
struct {
	u_char	m_syndrome;
	char	m_chip[4];
} memlogtab[] = {
	0x01,	"C00",	0x02,	"C01",	0x04,	"C02",	0x08,	"C03",
	0x10,	"C04",	0x19,	"L01",	0x1A,	"L02",	0x1C,	"L04",
	0x1F,	"L07",	0x20,	"C05",	0x38,	"L00",	0x3B,	"L03",
	0x3D,	"L05",	0x3E,	"L06",	0x40,	"C06",	0x49,	"L09",
	0x4A,	"L10",	0x4c,	"L12",	0x4F,	"L15",	0x51,	"L17",
	0x52,	"L18",	0x54,	"L20",	0x57,	"L23",	0x58,	"L24",
	0x5B,	"L27",	0x5D,	"L29",	0x5E,	"L30",	0x68,	"L08",
	0x6B,	"L11",	0x6D,	"L13",	0x6E,	"L14",	0x70,	"L16",
	0x73,	"L19",	0x75,	"L21",	0x76,	"L22",	0x79,	"L25",
	0x7A,	"L26",	0x7C,	"L28",	0x7F,	"L31",	0x80,	"C07",
	0x89,	"U01",	0x8A,	"U02",	0x8C,	"U04",	0x8F,	"U07",
	0x91,	"U09",	0x92,	"U10",	0x94,	"U12",	0x97,	"U15",
	0x98,	"U16",	0x9B,	"U19",	0x9D,	"U21",	0x9E,	"U22",
	0xA8,	"U00",	0xAB,	"U03",	0xAD,	"U05",	0xAE,	"U06",
	0xB0,	"U08",	0xB3,	"U11",	0xB5,	"U13",	0xB6,	"U14",
	0xB9,	"U17",	0xBA,	"U18",	0xBC,	"U20",	0xBF,	"U23",
	0xC1,	"U25",	0xC2,	"U26",	0xC4,	"U28",	0xC7,	"U31",
	0xE0,	"U24",	0xE3,	"U27",	0xE5,	"U29",	0xE6,	"U30"
};

memlog(m, mcr)
	int m;
	struct mcr780 *mcr;
{
	register i;

	for (i = 0; i < (sizeof (memlogtab) / sizeof (memlogtab[0])); i++)
		if ((u_char)(M780C_SYN(mcr)) == memlogtab[i].m_syndrome) {
			printf (
	"mcr%d: replace %s chip in %s bank of memory board %d (0-15)\n",
				m,
				memlogtab[i].m_chip,
				(M780C_ADDR(mcr) & 0x8000) ? "upper" : "lower",
				(M780C_ADDR(mcr) >> 16));
			return;
		}
	printf ("mcr%d: multiple errors, not traceable\n", m);
	break;
}
#endif /* TRENDATA */

char *mc780[]={"0","1","2","3","4","5","6","7","8","9","10","11","12","13",
	"14","15"};

struct mc780frame {
	int	mc8_bcnt;		/* byte count == 0x28 */
	int	mc8_summary;		/* summary parameter (as above) */
	int	mc8_cpues;		/* cpu error status */
	int	mc8_upc;		/* micro pc */
	int	mc8_vaviba;		/* va/viba register */
	int	mc8_dreg;		/* d register */
	int	mc8_tber0;		/* tbuf error reg 0 */
	int	mc8_tber1;		/* tbuf error reg 1 */
	int	mc8_timo;		/* timeout address divided by 4 */
	int	mc8_parity;		/* parity */
	int	mc8_sbier;		/* sbi error register */
	int	mc8_pc;			/* trapped pc */
	int	mc8_psl;		/* trapped psl */
};

int
ka780_mchk(cmcf)
	caddr_t cmcf;
{
	register struct mc780frame *mcf = (struct mc780frame *)cmcf;
	register int type = mcf->mc8_summary;
	register int sbifs;

	printf("machine check %x: %s%s\n", type, mc780[type&0xf],
	    (type&0xf0) ? " abort" : " fault"); 
	printf("\tcpues %x upc %x va/viba %x dreg %x tber %x %x\n",
	   mcf->mc8_cpues, mcf->mc8_upc, mcf->mc8_vaviba,
	   mcf->mc8_dreg, mcf->mc8_tber0, mcf->mc8_tber1);
	sbifs = mfpr(PR_SBIFS);
	printf("\ttimo %x parity %x sbier %x pc %x psl %x sbifs %x\n",
	   mcf->mc8_timo*4, mcf->mc8_parity, mcf->mc8_sbier,
	   mcf->mc8_pc, mcf->mc8_psl, sbifs);
	/* THE FUNNY BITS IN THE FOLLOWING ARE FROM THE ``BLACK BOOK'' */
	/* AND SHOULD BE PUT IN AN ``sbi.h'' */
	mtpr(sbifs &~ 0x2000000, PR_SBIFS);
	mtpr(mfpr(PR_SBIER) | 0x70c0, PR_SBIER);
	return (MCHK_PANIC);
}

struct ka78x {
	unsigned snr:12,
		 plant:3,
		 eco:8,
		 v785:1,
		 type:8;
};

void
ka780_conf()
{
	extern	char cpu_model[];
	struct	ka78x *ka78 = (void *)&vax_cpudata;

	/* Enable cache */
	mtpr(0x200000, PR_SBIMT);

	printf("cpu: %s, serial number %d(%d), hardware ECO level %d(%d)\n",
	    &cpu_model[4], ka78->snr, ka78->plant, ka78->eco >> 4, ka78->eco);
	if (mfpr(PR_ACCS) & 255) {
		printf("cpu: FPA present, enabling.\n");
		mtpr(0x8000, PR_ACCS);
	} else
		printf("cpu: no FPA\n");

}

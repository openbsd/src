/*      $NetBSD: autoconf.c,v 1.13 1996/05/19 16:43:53 ragge Exp $      */

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

 /* All bugs are subject to removal without further notice */
		

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/conf.h>

#include <machine/cpu.h>
#include <machine/sid.h>
#include <machine/param.h>
#include <machine/vmparam.h>
#include <machine/nexus.h>
#include <machine/ioa.h>
#include <machine/ka750.h>
#include <machine/ka650.h>
#include <machine/uvaxII.h>

#include <vax/vax/gencons.h>

#include <vm/vm.h>

#define	BACKPLANE	0
#define	BIBUSS		1
#define	SBIBUSS		2

struct bp_conf {
	char *type;
	int num;
	int partyp;
};

extern int cold;

void    notsupp_steal_pages __P((void));


#ifdef VAX8600 /* XXX These are in ka860 also */
void    ka86_conf __P((struct device *, struct device *, void *));
int     ka86_clock __P((void));
void    ka86_memenable __P((struct sbi_attach_args *, struct device *));
void    ka86_memerr __P((void));
int     ka86_mchk __P((caddr_t));
void    ka86_steal_pages __P((void));
#endif
#ifdef VAX780 /* XXX These are in ka780 also */
void    ka780_conf __P((struct device *, struct device *, void *));
int     ka780_clock __P((void));
void    ka780_memenable __P((struct sbi_attach_args *, void *));
void    ka780_memerr __P((void));
int     ka780_mchk __P((caddr_t));
void    ka780_steal_pages __P((void));
#endif
#ifdef	VAX750
int	nexty750[]={ NEX_MEM16,	NEX_MEM16,	NEX_MEM16,	NEX_MEM16,
		NEX_MBA,	NEX_MBA, 	NEX_MBA,	NEX_MBA,
		NEX_UBA0,	NEX_UBA1,	NEX_ANY,	NEX_ANY,
		NEX_ANY,	NEX_ANY,	NEX_ANY,	NEX_ANY};
#endif
#if VAX730
int   nexty730[NNEX730] = {
	NEX_MEM16,      NEX_ANY,	NEX_ANY,	NEX_ANY,
	NEX_ANY,	NEX_ANY,	NEX_ANY,	NEX_ANY,
};
#endif

struct	cpu_dep	cpu_calls[VAX_MAX+1]={
		/* Type 0,noexist */
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#ifdef	VAX780	/* Type 1, 11/{780,782,785} */
	{ka780_steal_pages, ka780_clock, ka780_mchk, ka780_memerr, ka780_conf},
#else
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#endif
#ifdef  VAX750	/* Type 2, 11/750 */
	{ka750_steal_pages, ka750_clock, ka750_mchk, ka750_memerr, ka750_conf},
#else
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#endif
#ifdef	VAX730	/* Type 3, 11/{730,725}, ceauciesco-vax */
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#else
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#endif
#ifdef	VAX8600	/* Type 4, 8600/8650 (11/{790,795}) */
	{ka86_steal_pages, ka86_clock, ka86_mchk, ka86_memerr, ka86_conf},
#else
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#endif
#ifdef	VAX8200	/* Type 5, 8200, 8300, 8350 */
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#else
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#endif
#ifdef	VAX8800	/* Type 6, 85X0, 8700, 88X0 */
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#else
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#endif
#ifdef	VAX610	/* Type 7, KA610 */
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#else
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#endif
#ifdef  VAX630  /* Type 8, KA630 or KA410 (uVAX II) */
	{uvaxII_steal_pages, uvaxII_clock, uvaxII_mchk, uvaxII_memerr,
	    uvaxII_conf},
#else
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#endif
		/* Type 9, not used */
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#ifdef	VAX650  /* Type 10, KA65X (uVAX III) */
	{uvaxIII_steal_pages, uvaxIII_clock, uvaxIII_mchk, uvaxIII_memerr,
	    uvaxIII_conf},
#else
	{notsupp_steal_pages, NULL, NULL, NULL, NULL },
#endif
};

void
notsupp_steal_pages()
{
	printf("This cputype not supported.\n");
	asm("halt");
}

void	gencnslask __P((void));

void
configure()
{
	extern int boothowto;


	if (config_rootfound("backplane", NULL) == NULL)
		panic("backplane not configured");

#if GENERIC
	if ((boothowto & RB_ASKNAME) == 0)
		setroot();
	setconf();
#else
	setroot();
#endif
	/*
	 * Configure swap area and related system
	 * parameter based on device(s) used.
	 */
	gencnslask(); /* XXX inte g|ras h{r */
	swapconf();
	cold = 0;
	mtpr(GC_CCF, PR_TXDB);	/* Clear cold start flag in cpu */
}

int	printut __P((void *, const char *));
int	backplane_match __P((struct device *, void *, void *));
void	backplane_attach __P((struct device *, struct device *, void *));
int
printut(aux, hej)
	void *aux;
	const char *hej;
{
	struct bp_conf *bp = aux;
	if (hej)
		printf("printut %s %s %d\n",hej, bp->type, bp->num);
	return (UNSUPP);
}

int
backplane_match(parent, gcf, aux)
	struct	device	*parent;
	void	*gcf, *aux;
{
	struct  cfdata  *cf = gcf;

	if (cf->cf_unit == 0 &&
	    strcmp(cf->cf_driver->cd_name, "backplane") == 0)
		return 1; /* First (and only) backplane */

	return (0);
}

static  void find_sbi __P((struct device *, struct bp_conf *,
	    int (*) __P((void *, char *))));


void
backplane_attach(parent, self, hej)
	struct	device	*parent, *self;
	void	*hej;
{
	struct bp_conf bp;
	int i, ccpu, cmem, cbi, csbi;

	printf("\n");

	switch (cpunumber) {
	case VAX_750:
	case VAX_650:
	case VAX_78032:
	case VAX_780:
		cmem = cbi = 0;
		ccpu = csbi = 1;
		break;

	case VAX_8600:
		cmem = ccpu = 1;
		cbi = csbi = 0;
		break;
	default:
		cmem = ccpu = cbi = csbi = 0;
	}

	bp.partyp = BACKPLANE;
	bp.type = "cpu";
	for (i = 0; i < ccpu; i++) {
		bp.num = i;
		config_found(self, &bp, printut);
	}
	bp.type = "mem";
	for (i = 0; i < cmem; i++) {
		bp.num = i;
		config_found(self, &bp, printut);
	}
	bp.type = "bi";
	for (i = 0; i < cbi; i++) {
		bp.num = i;
		config_found(self, &bp, printut);
	}
	bp.type = "sbi";
	for(i = 0; i < csbi; i++) {
		bp.num = i;
		config_found(self, &bp, printut);
	}
#if VAX8600
	if (cpunumber == VAX_8600)
		find_sbi(self, &bp, printut);
#endif
}

#if VAX8600
void
find_sbi(self, bp, print)
	struct	device *self;
	struct	bp_conf *bp;
	int	(*print) __P((void *, char *));
{
	volatile int tmp;
	volatile struct	sbia_regs *sbiar;
	extern	struct ioa *ioa;
	int	type, i;

	for (i = 0; i < MAXNIOA; i++) {
		if (badaddr((caddr_t)&ioa[i], 4))
			continue;
		tmp = ioa[i].ioacsr.ioa_csr;
		type = tmp & IOA_TYPMSK;

		switch (type) {

		case IOA_SBIA:
			bp->type = "sbi";
			bp->num = i;
			config_found(self, bp, printut);
			sbiar = (void *)&ioa[i];
			sbiar->sbi_errsum = -1;
			sbiar->sbi_error = 0x1000;
			sbiar->sbi_fltsts = 0xc0000;
			break;

		default:
			printf("IOAdapter %x unsupported\n", type);
			break;
		}
	}
}
#endif

int     cpu_match __P((struct  device  *, void *, void *));
void	cpu_attach __P((struct  device  *, struct  device  *, void *));


int
cpu_match(parent, gcf, aux)
	struct  device  *parent;
	void    *gcf, *aux;
{
	struct  cfdata  *cf = gcf;
	struct bp_conf *bp = aux;

	if (strcmp(bp->type, "cpu"))
		return 0;

	switch (cpunumber) {
#if VAX750 || VAX630 || VAX650 || VAX780 || VAX8600
	case VAX_750:
	case VAX_78032:
	case VAX_650:
	case VAX_780:
	case VAX_8600:
		if(cf->cf_unit == 0 && bp->partyp == BACKPLANE)
			return 1;
		break;
#endif
	};

	return 0;
}

void
cpu_attach(parent, self, aux)
	struct  device  *parent, *self;
	void    *aux;
{
	(*cpu_calls[cpunumber].cpu_conf)(parent, self, aux);
}

int     mem_match __P((struct  device  *, void  *, void *));
void	mem_attach __P((struct  device  *, struct  device  *, void *));

int
mem_match(parent, gcf, aux)
	struct  device  *parent;
	void    *gcf, *aux;
{
	struct  cfdata  *cf = gcf;
	struct	sbi_attach_args *sa = (struct sbi_attach_args *)aux;
	struct	bp_conf	*bp = aux;

#if VAX8600
	if (cpunumber == VAX_8600 && !strcmp(parent->dv_xname, "backplane0")) {
		if (strcmp(bp->type, "mem"))
			return 0;
		return 1;
	}
#endif
	if ((cf->cf_loc[0] != sa->nexnum) && (cf->cf_loc[0] > -1))
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

void
mem_attach(parent, self, aux)
	struct  device  *parent, *self;
	void    *aux;
{
	struct	sbi_attach_args *sa = (struct sbi_attach_args *)aux;
	struct	mem_softc *sc = (void *)self;

#if VAX8600
	if (cpunumber == VAX_8600) {
		ka86_memenable(0, 0);
		printf("\n");
		return;
	}
#endif
	sc->sc_memaddr = sa->nexaddr;
	sc->sc_memtype = sa->nexinfo;
	sc->sc_memnr = sa->type;

	switch (cpunumber) {
#ifdef VAX750
	case VAX_750:
		ka750_memenable(sa, (void *)sc);
		break;
#endif
#ifdef VAX780
	case VAX_780:
		ka780_memenable(sa, sc);
		break;
#endif
	default:
		break;
	}

}

struct	cfdriver backplane_cd = {
	NULL, "backplane", DV_DULL
};

struct	cfattach backplane_ca = {
	sizeof(struct device), backplane_match, backplane_attach
};

struct	cfdriver cpu_cd = {
	NULL, "cpu", DV_CPU
};

struct	cfattach cpu_backplane_ca = {
	sizeof(struct device), cpu_match, cpu_attach
};

struct  cfdriver mem_cd = {
	NULL, "mem", DV_CPU
};

struct	cfattach mem_backplane_ca = {
	sizeof(struct device), mem_match, mem_attach
};

struct	cfattach mem_sbi_ca = {
	sizeof(struct device), mem_match, mem_attach
};

/*      $NetBSD: autoconf.c,v 1.5 1995/12/13 18:45:57 ragge Exp $      */

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
		

#include "sys/param.h"
#include "machine/cpu.h"
#include "machine/sid.h"
#include "sys/types.h"
#include "sys/device.h"
#include "sys/reboot.h"
#include "sys/conf.h"
#include "machine/param.h"
#include "machine/vmparam.h"
#include "machine/nexus.h"
#include "machine/ka750.h"
#include "machine/../vax/gencons.h"
#include "vm/vm.h"

#define	BACKPLANE	0
#define	BIBUSS		1
#define	SBIBUSS		2

struct bp_conf {
	char *type;
	int num;
	int partyp;
};

extern int cold;

int	cpu_notsupp(),cpu_notgen();
#ifdef	VAX750
int	ka750_mchk(),ka750_memerr(),ka750_clock(),ka750_conf();
int	ka750_steal_pages();
int	nexty750[]={ NEX_MEM16,	NEX_MEM16,	NEX_MEM16,	NEX_MEM16,
		NEX_MBA,	NEX_MBA, 	NEX_MBA,	NEX_MBA,
		NEX_UBA0,	NEX_UBA1,	NEX_ANY,	NEX_ANY,
		NEX_ANY,	NEX_ANY,	NEX_ANY,	NEX_ANY};
#endif
#if VAX730
int	ka750_steal_pages();
int   nexty730[NNEX730] = {
	NEX_MEM16,      NEX_ANY,	NEX_ANY,	NEX_ANY,
	NEX_ANY,	NEX_ANY,	NEX_ANY,	NEX_ANY,
};
#endif
#if VAX630
int	uvaxII_steal_pages();
int     uvaxII_mchk(), uvaxII_memerr(), uvaxII_clock(), uvaxII_conf();
#endif
#if VAX650
int	uvaxIII_steal_pages();
int     uvaxIII_mchk(), uvaxIII_memerr(), uvaxIII_clock(), uvaxIII_conf();
#endif

struct	cpu_dep	cpu_calls[VAX_MAX+1]={
		/* Type 0,noexist */
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#ifdef	VAX780	/* Type 1, 11/{780,782,785} */
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#else
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#endif
#ifdef  VAX750	/* Type 2, 11/750 */
	ka750_steal_pages,ka750_clock,ka750_mchk,ka750_memerr,ka750_conf,
#else
	cpu_notgen,cpu_notgen,cpu_notgen,cpu_notgen,cpu_notgen,
#endif
#ifdef	VAX730	/* Type 3, 11/{730,725}, ceauciesco-vax */
	ka730_steal_pages,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#else
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#endif
#ifdef	VAX8600	/* Type 4, 8600/8650 (11/{790,795}) */
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#else
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#endif
#ifdef	VAX8200	/* Type 5, 8200, 8300, 8350 */
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#else
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#endif
#ifdef	VAX8800	/* Type 6, 85X0, 8700, 88X0 */
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#else
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#endif
#ifdef	VAX610	/* Type 7, KA610 */
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#else
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#endif
#ifdef  VAX630  /* Type 8, KA630 or KA410 (uVAX II) */
	uvaxII_steal_pages, uvaxII_clock, uvaxII_mchk, uvaxII_memerr,
	    uvaxII_conf,
#else
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#endif
		/* Type 9, not used */
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#ifdef	VAX650  /* Type 10, KA65X (uVAX III) */
	uvaxIII_steal_pages, uvaxIII_clock, uvaxIII_mchk, uvaxIII_memerr,
	    uvaxIII_conf,
#else
	cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,cpu_notsupp,
#endif
};

cpu_notgen()
{
	conout("This cputype not generated.\n");
	asm("halt");
}
cpu_notsupp()
{
	conout("This cputype not supported.\n");
	asm("halt");
}

configure()
{
	extern int boothowto;


	if (!config_rootfound("backplane", NULL))
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


int
printut(aux, hej)
	void *aux;
	char *hej;
{
	if (hej)
		printf("printut %s\n",hej);
	return (UNSUPP);
}

int
backplane_match(parent, cf, aux)
	struct	device	*parent;
	struct	cfdata	*cf;
	void	*aux;
{
	if (cf->cf_unit == 0 &&
	    strcmp(cf->cf_driver->cd_name, "backplane") == 0)
		return 1; /* First (and only) backplane */

	return (0);
}

void
backplane_attach(parent, self, hej)
	struct	device	*parent, *self;
	void	*hej;
{
	struct bp_conf bp;
	int i, ccpu, cmem, cbi, csbi;

	printf("\n");

	switch(cpunumber){
	case VAX_750:
	case VAX_650:
	case VAX_78032:
		cmem = cbi = 0;
		ccpu = csbi = 1;
		break;
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
}

int
cpu_match(parent, cf, aux)
	struct  device  *parent;
	struct  cfdata  *cf;
	void    *aux;
{
	struct bp_conf *bp = aux;

	if (strcmp(cf->cf_driver->cd_name, "cpu"))
		return 0;

	switch (cpunumber) {
#if VAX750 || VAX630 || VAX650
	case VAX_750:
	case VAX_78032:
	case VAX_650:
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

int nmcr = 0;

int
mem_match(parent, cf, aux)
	struct  device  *parent;
	struct  cfdata  *cf;
	void    *aux;
{
	struct sbi_attach_args *sa = (struct sbi_attach_args *)aux;

	if ((cf->cf_loc[0] != sa->nexnum) && (cf->cf_loc[0] > -1))
		return 0; /* memory doesn't match spec's */
	switch (sa->type) {
	case NEX_MEM16:
		return 1;
	}
	return 0;
}

void
mem_attach(parent, self, aux)
	struct  device  *parent, *self;
	void    *aux;
{
	struct sbi_attach_args *sa = (struct sbi_attach_args *)aux;

	switch (cpunumber) {
#ifdef VAX750
	case VAX_750:
		ka750_memenable(sa, self);
		break;
#endif

	default:
		break;
	}

}

struct	cfdriver backplanecd =
	{ 0, "backplane", backplane_match, backplane_attach,
		DV_DULL, sizeof(struct device) };

struct	cfdriver cpucd =
	{ 0, "cpu", cpu_match, cpu_attach, DV_CPU, sizeof(struct device) };


struct  cfdriver memcd =
	{ 0, "mem", mem_match, mem_attach, DV_CPU, sizeof(struct device) };



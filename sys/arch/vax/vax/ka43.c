/*	$NetBSD: ka43.c,v 1.3 1996/10/13 03:35:43 christos Exp $ */
/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/pmap.h>
#include <machine/nexus.h>
#include <machine/uvax.h>
#include <machine/ka43.h>
#include <machine/clock.h>
#include <machine/ka650.h>	/* cache ??? */

#define	xtrace(x)

void	ka43_conf __P((struct device*, struct device*, void*));
void	ka43_steal_pages __P((void));

void	ka43_memerr __P((void));
int	ka43_mchk __P((caddr_t));

struct	ka43_cpu   *ka43_cpuptr = (void*)KA43_CPU_BASE;
struct	ka43_clock *ka43_clkptr = (void*)KA43_WAT_BASE;

extern int uVAX_fillmap __P((struct uc_map *));

struct uc_map ka43_map[] = {
	{ KA43_CFGTST,		KA43_CFGTST,	4,		0 },
	{ KA43_ROM_BASE,	KA43_ROM_END,	KA43_ROM_SIZE,	0 },
	{ KA43_CPU_BASE,	KA43_CPU_END,	KA43_CPU_SIZE,	0 },
	{ KA43_CT2_BASE,	KA43_CT2_END,	KA43_CT2_SIZE,	0 },
	{ KA43_CH2_CREG,	KA43_CH2_CREG,	4,		0 },
	{ KA43_NWA_BASE,	KA43_NWA_END,	KA43_NWA_SIZE,	0 },
	{ KA43_SER_BASE,	KA43_SER_END,	KA43_SER_SIZE,	0 },
	{ KA43_WAT_BASE,	KA43_WAT_END,	KA43_WAT_SIZE,	0 },
	{ KA43_SCS_BASE,	KA43_SCS_END,	KA43_SCS_SIZE,	0 },
	{ KA43_LAN_BASE,	KA43_LAN_END,	KA43_LAN_SIZE,	0 },
	{ KA43_CUR_BASE,	KA43_CUR_END,	KA43_CUR_SIZE,	0 },
	{ KA43_DMA_BASE,	KA43_DMA_END,	KA43_DMA_SIZE,	0 },
	{ KA43_VME_BASE,	KA43_VME_END,	KA43_VME_SIZE,	0 },
	/*
	 * there's more to come, eg. framebuffers (GPX/SPX)
	 */
	{0, 0, 0, 0},
};

#define CH1_BITS \
	"\020\015BCHIT\014BUSERR\013PPERR\012DPERR\011TPERR\010TRAP1" \
	"\007TRAP2\006INTR\005HIT\004REFRESH\003FLUSH\002ENABLE\001FORCEHIT"

#define CH2_BITS \
	"\020\010TPE\007DPE\006MISS\005DIRTY\004CERR\003LERR\002SERR\001ENAB"

void
ka43_memerr()
{
	int mapen;
	int *ch2reg;

	printf("memory error!\n");
	printf("primary cache status: %b\n", mfpr(PR_PCSTS), CH1_BITS);

	mapen = mfpr(PR_MAPEN);
	if (mapen) 
		ch2reg = (void*)uvax_phys2virt(KA43_CH2_CREG);
	else 
		ch2reg = (void*)KA43_CH2_CREG;
	printf("secondary cache status: %b\n", *ch2reg, CH2_BITS);
}

static char *mcc43[] = {
	"no error (0)",
	"FPA signalled protocoll error",
	"FPA signalled illegal opcode",
	"FPA detected parity error",
	"FPA returned unknown status",
	"FPA result has parity error",
	"unused (6)",
	"unused (7)",
	"MMU error (TLB miss)",
	"MMU error (TLB hit)",
	"HW interrupt at unused IPL",
	"impossible microcode state",
	"undefined trap code (i-box)",
	"undefined control store address",
	"unused (14)",
	"unused (15)",
	"PC tag or data parity error",
	"data bus parity error",
	"data bus error (NXM)",
	"undefined data bus state",
};

int
ka43_mchk(addr)
	caddr_t addr;
{
	struct {
	  int bcount;	/* byte count (0x18) */
	  int mcc;	/* "R"-flag and machine check code */
	  int mrva;	/* most recent virtual address */
	  int viba;	/* contents of VIBA register */
	  int sisr;	/* ICCS bit 6 and SISR bits 15:0 */
	  int isd;	/* internal state */
	  int scr;	/* shift count register */
	  int pc;	/* program counter */
	  int psl;	/* processor status longword */
	} *p = (void*)addr;

	printf("machine check: 0x%x\n", p->mcc);
	printf("reason: %s\n", mcc43[p->mcc & 0xff]);

	printf("bcount:0x%x, check-code:0x%x, virtaddr:0x%x\n",
	       p->bcount, p->mcc, p->mrva);
	printf("pc:0x%x, psl:0x%x, viba: %x, state: %x\n",
	       p->pc, p->psl, p->viba, p->isd);

	return (-1);
}

int
ka43_setup(uc,flags)
	struct uvax_calls *uc;
	int flags;
{
	uc->uc_name = "ka43";

	uc->uc_phys2virt = NULL;
	uc->uc_physmap = ka43_map;

	uc->uc_steal_pages = ka43_steal_pages;
	uc->uc_conf = ka43_conf;
	uc->uc_clkread = ka43_clkread;
	uc->uc_clkwrite = ka43_clkwrite;

	uc->uc_memerr = ka43_memerr;
	uc->uc_mchk = ka43_mchk;

	uc->uc_intreq = (void*)KA43_INTREQ;
	uc->uc_intclr = (void*)KA43_INTCLR;
	uc->uc_intmsk = (void*)KA43_INTMSK;

	uc->uc_busTypes = VAX_VSBUS;
}

ka43_discache()
{
	int *ctag;
	int *creg;
	int mapen;
	int i;

	xtrace(("ka43_discache()\n"));
	return (0);

	/*
	 * first disable primary cache
	 */
#if 0
	mtpr(0, PR_PCSTS);
	mtpr(0, PR_PCERR);
	mtpr(0, PR_PCIDX);
	mtpr(0, PR_PCTAG);
#else
	i = mfpr(PR_PCSTS);
	mtpr((i & ~2), PR_PCSTS);
	printf("pcsts: %x --> %x\n", i, mfpr(PR_PCSTS));
#endif
	/*
	 * now secondary cache
	 */
	mapen = mfpr(PR_MAPEN);
	if (mapen) {
		ctag = (void*)uvax_phys2virt(KA43_CT2_BASE);
		creg = (void*)uvax_phys2virt(KA43_CH2_CREG);
	} else {
		ctag = (void*)KA43_CT2_BASE;
		creg = (void*)KA43_CH2_CREG;
	}
	i = *creg;
	*creg = (i & ~1);
	printf("creg: %x --> %x\n", i, *creg);
	
	xtrace(("ka43_discache() done.\n"));
}

ka43_encache()
{
	int *ctag;
	int *creg;
	int mapen;
	int i;

	xtrace(("ka43_encache()\n"));

	ka43_discache();

	/*
	 * first enable primary cache
	 */
	printf("P-0");
	i = mfpr(PR_PCSTS);
	mtpr((i & ~2), PR_PCSTS);
	mtpr(0, PR_PCSTS);
	printf("P-1");
#if 1
	mtpr(KA43_PCS_ENABLE | KA43_PCS_FLUSH | KA43_PCS_REFRESH, PR_PCSTS);
#else
	mtpr(KA43_PCS_ENABLE, PR_PCSTS);
#endif
	printf("P-2");

	/*
	 * now secondary cache
	 */
	mapen = mfpr(PR_MAPEN);
	if (mapen) {
		ctag = (void*)uvax_phys2virt(KA43_CT2_BASE);
		creg = (void*)uvax_phys2virt(KA43_CH2_CREG);
	} else {
		ctag = (void*)KA43_CT2_BASE;
		creg = (void*)KA43_CH2_CREG;
	}
	printf("ctag: %x, creg: %x\n", ctag, creg);
	printf("S-1");
	i = *creg;
	printf("creg=[%x] ", *creg);
#if 0
	*creg = (i & ~1);
	printf("creg=[%x] ", *creg);
	printf("S-2");
	for (i = 0; i < KA43_CT2_SIZE; i += 4)		/* Quadword entries */
		ctag[i/4] = 0;				/* reset lower half */
	printf("S-3");
	i = *creg;
	printf("creg=[%x] ", *creg);
	*creg = (i & ~1);
	printf("creg=[%x] ", *creg);
	printf("S-4");
	/* *creg = 1; */
	printf("S-5");
#endif
	xtrace(("ka43_encache() done.\n"));

	printf("primary cache status: %b\n", mfpr(PR_PCSTS), CH1_BITS);
	printf("secondary cache status: %b\n", *creg, CH2_BITS);
}

void
ka43_conf(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	extern char cpu_model[];
	extern int vax_siedata;

	if (vax_siedata & 0x02)		/* "single-user" flag */
		strcpy(cpu_model,"VAXstation 3100 model 76");
	else if (vax_siedata & 0x01)	/* "multiuser" flag */
		strcpy(cpu_model,"MicroVAX 3100 model 76(?)");
	else
		strcpy(cpu_model, "unknown KA43 board");

	printf(": %s\n", cpu_model);

	ka43_encache();
}


/*
 *
 */
u_long le_iomem;		/* base addr of RAM -- CPU's view */
u_long le_ioaddr;		/* base addr of RAM -- LANCE's view */

void
ka43_steal_pages()
{
	extern  vm_offset_t avail_start, virtual_avail, avail_end;
	int	junk;
	int	i;
	struct {
	  u_long     :2;
	  u_long data:8;
	  u_long     :22;
	} *p;
	int *srp;	/* Scratch Ram */
	int *pctl;	/* parity control register */
	char *q = (void*)&srp;
	char line[20];

	ka43_encache();

	pctl = (void*)KA43_PARCTL;
	printf("parctl: 0x%x\n", *pctl);
#if 0
	*pctl = KA43_PCTL_DPEN | KA43_PCTL_CPEN;
#else
	*pctl = KA43_PCTL_CPEN;
#endif
	printf("new value for parctl: ");
	gets(line);
	*pctl = *line - '0';
	printf("parctl: 0x%x\n", *pctl);

	srp = NULL;
	p = (void*)KA43_SCR;
	for (i=0; i<4; i++) {
	  printf("p[%d] = %x, ", i, p[i].data);
	  q[i]  = p[i].data;
	}
	p = (void*)KA43_SCRLEN;
	printf("\nlen = %d\n", p->data);
	printf("srp = 0x%x\n", srp);

	for (i=0; i<0x2; i++) {
	  printf("%x:0x%x ", i*4, srp[i]);
	  if ((i & 0x07) == 0x07)
	    printf("\n");
 	}
	printf("\n");

	printf ("ka43_steal_pages: avail_end=0x%x\n", avail_end);

	/* 
	 * SCB is already copied/initialized at addr avail_start
	 * by pmap_bootstrap(), but it's not yet mapped. Thus we use
	 * the MAPPHYS() macro to reserve these two pages and to
	 * perform the mapping. The mapped address is assigned to junk.
	 */
	MAPPHYS(junk, 2, VM_PROT_READ|VM_PROT_WRITE);

	/*
	 * At top of physical memory there are some console-prom and/or
	 * restart-specific data. Make this area unavailable.
	 */
#if 1
	avail_end -= 10 * NBPG;
#endif

	/*
	 * If we need to map physical areas also, we can decrease avail_end
	 * (the highest available memory-address), copy the stuff into the
	 * gap between and use pmap_map to map it...
	 *
	 * Don't use the MAPPHYS macro here, since this uses and changes(!)
	 * the value of avail_start. Use MAPVIRT even if it's name misleads.
	 */
	avail_end &= ~0xffff;
	avail_end -= (64 * 1024);

	avail_end = 0xf00000;
	le_ioaddr = 0xf40000;

	MAPVIRT(le_iomem, (64 * 1024)/NBPG);
	pmap_map((vm_offset_t)le_iomem, le_ioaddr, le_ioaddr + 0xffff,
		 VM_PROT_READ|VM_PROT_WRITE);

	if (1 || le_ioaddr > 0xffffff) {
		le_ioaddr &= 0xffffff;
		*pctl |= KA43_PCTL_DMA;
	}
	printf("le_iomem: %x, le_ioaddr: %x, parctl:%x\n",
	       le_iomem, le_ioaddr, *pctl);

	/*
	 * now map in anything listed in ka43_map...
	 */
	uVAX_fillmap(ka43_map);

	/*
	 * Clear restart and boot in progress flags in the CPMBX. 
	 */
	ka43_clkptr->cpmbx = ka43_clkptr->cpmbx & 0xF0;

	/*
	 * Enable memory parity error detection and clear error bits.
	 */
	ka43_cpuptr->ka43_mser = 0x01; 
	/* (UVAXIIMSER_PEN | UVAXIIMSER_MERR | UVAXIIMSER_LEB); */

	/*
	 * MM is not yet enabled, thus we still used the physical addresses,
	 * but before leaving this routine, we need to reset them to virtual.
	 */
	ka43_cpuptr = (void*)uvax_phys2virt(KA43_CPU_BASE);
	ka43_clkptr = (void*)uvax_phys2virt(KA43_WAT_BASE);

	printf ("steal_pages done.\n");
}

/*
 * define what we need and overwrite the uVAX_??? names
 */

#define NEED_UVAX_GENCLOCK
#define NEED_UVAX_PROTOCLOCK

#define uVAX_clock	ka43_clock
#define uVAX_clkptr	ka43_clkptr
#define uVAX_clkread	ka43_clkread
#define uVAX_clkwrite	ka43_clkwrite
#define uVAX_genclock	ka43_genclock

#include <arch/vax/vax/uvax_proto.c>

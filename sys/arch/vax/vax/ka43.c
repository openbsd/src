/*	$OpenBSD: ka43.c,v 1.4 1999/01/11 05:12:08 millert Exp $ */
/*	$NetBSD: ka43.c,v 1.5 1997/04/18 18:53:38 ragge Exp $ */
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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
#include <machine/cpu.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/pmap.h>
#include <machine/nexus.h>
#include <machine/uvax.h>
#include <machine/ka43.h>
#include <machine/clock.h>

void	ka43_conf __P((struct device*, struct device*, void*));
void	ka43_steal_pages __P((void));

int	ka43_mchk __P((caddr_t));
void	ka43_memerr __P((void));

int	ka43_clear_errors __P((void));

int	ka43_cache_init __P((void));	/* "int mapen" as argument? */
int	ka43_cache_reset __P((void));
int	ka43_cache_enable __P((void));
int	ka43_cache_disable __P((void));
int	ka43_cache_invalidate __P((void));

static struct uc_map ka43_map[] = {
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

struct	cpu_dep ka43_calls = {
	ka43_steal_pages,
	no_nicr_clock,
	ka43_mchk,
	ka43_memerr,
	ka43_conf,
	chip_clkread,
	chip_clkwrite,
	7,	/* 7.6 VUP */
	(void*)KA43_INTREQ,
	(void*)KA43_INTCLR,
	(void*)KA43_INTMSK,
	ka43_map,
};

/*
 * ka43_steal_pages() is called with MMU disabled, after that call MMU gets
 * enabled. Thus we initialize these four pointers with physical addresses,
 * but before leving ka43_steal_pages() we reset them to virtual addresses.
 */
struct	ka43_cpu   *ka43_cpu	= (void*)KA43_CPU_BASE;

u_int	*ka43_creg = (void*)KA43_CH2_CREG;
u_int	*ka43_ctag = (void*)KA43_CT2_BASE;

#define KA43_MC_RESTART	0x00008000	/* Restart possible*/
#define KA43_PSL_FPDONE	0x00010000	/* First Part Done */

struct ka43_mcframe {		/* Format of RigelMAX machine check frame: */
	int	mc43_bcnt;	/* byte count, always 24 (0x18) */
	int	mc43_code;	/* machine check type code and restart bit */
	int	mc43_addr;	/* most recent (faulting?) virtual address */
	int	mc43_viba;	/* contents of VIBA register */
	int	mc43_sisr;	/* ICCS bit 6 and SISR bits 15:0 */
	int	mc43_istate;	/* internal state */
	int	mc43_sc;	/* shift count register */
	int	mc43_pc;	/* trapped PC */
	int	mc43_psl;	/* trapped PSL */
};

static char *ka43_mctype[] = {
	"no error (0)",			/* Code 0: No error */
	"FPA: protocol error",		/* Code 1-5: FPA errors */
	"FPA: illegal opcode",
	"FPA: operand parity error",
	"FPA: unknown status",
	"FPA: result parity error",
	"unused (6)",			/* Code 6-7: Unused */
	"unused (7)",
	"MMU error (TLB miss)",		/* Code 8-9: MMU errors */
	"MMU error (TLB hit)",
	"HW interrupt at unused IPL",	/* Code 10: Interrupt error */
	"MOVCx impossible state",	/* Code 11-13: Microcode errors */
	"undefined trap code (i-box)",
	"undefined control store address",
	"unused (14)",			/* Code 14-15: Unused */
	"unused (15)",
	"PC tag or data parity error",	/* Code 16: Cache error */
	"data bus parity error",	/* Code 17: Read error */
	"data bus error (NXM)",		/* Code 18: Write error */
	"undefined data bus state",	/* Code 19: Bus error */
};
#define MC43_MAX	19

static int ka43_error_count = 0;

int
ka43_mchk(addr)
	caddr_t addr;
{
	register struct ka43_mcframe *mcf = (void*)addr;

	mtpr(0x00, PR_MCESR);	/* Acknowledge the machine check */
	printf("machine check %d (0x%x)\n", mcf->mc43_code, mcf->mc43_code);
	printf("reason: %s\n", ka43_mctype[mcf->mc43_code & 0xff]);
	if (++ka43_error_count > 10) {
		printf("error_count exceeded: %d\n", ka43_error_count);
		return (-1);
	}

	/*
	 * If either the Restart flag is set or the First-Part-Done flag
	 * is set, and the TRAP2 (double error) bit is not set, the the
	 * error is recoverable.
	 */
	if (mfpr(PR_PCSTS) & KA43_PCS_TRAP2) {
		printf("TRAP2 (double error) in ka43_mchk.\n");
		panic("unrecoverable state in ka43_mchk.");
		return (-1);
	}
	if ((mcf->mc43_code & KA43_MC_RESTART) || 
	    (mcf->mc43_psl & KA43_PSL_FPDONE)) {
		printf("ka43_mchk: recovering from machine-check.\n");
		ka43_cache_reset();	/* reset caches */
		return (0);		/* go on; */
	}

	/*
	 * Unknown error state, panic/halt the machine!
	 */
	printf("ka43_mchk: unknown error state!\n");
	return (-1);
}

void
ka43_memerr()
{
	/*
	 * Don\'t know what to do here. So just print some messages
	 * and try to go on...
	 */
	printf("memory error!\n");
	printf("primary cache status: %b\n", mfpr(PR_PCSTS), KA43_PCSTS_BITS);
	printf("secondary cache status: %b\n", *ka43_creg, KA43_SESR_BITS);
}

int
ka43_cache_init()
{
	return (ka43_cache_reset());
}

int
ka43_clear_errors()
{
	int val = *ka43_creg;
	val |= KA43_SESR_SERR | KA43_SESR_LERR | KA43_SESR_CERR;
	*ka43_creg = val;
}

int
ka43_cache_reset()
{
	/*
	 * resetting primary and secondary caches is done in three steps:
	 *	1. disable both caches
	 *	2. manually clear secondary cache
	 *	3. enable both caches
	 */
	ka43_cache_disable();
	ka43_cache_invalidate();
	ka43_cache_enable();

	printf("primary cache status: %b\n", mfpr(PR_PCSTS), KA43_PCSTS_BITS);
	printf("secondary cache status: %b\n", *ka43_creg, KA43_SESR_BITS);
	printf("cpu status: parctl=0x%x, hltcod=0x%x\n", 
	       ka43_cpu->parctl, ka43_cpu->hltcod);

	return (0);
}

int
ka43_cache_disable()
{
	int i, val;

	/*
	 * first disable primary cache and clear error flags
	 */
	mtpr(KA43_PCS_REFRESH, PR_PCSTS);	/* disable primary cache */
	val = mfpr(PR_PCSTS);
	mtpr(val, PR_PCSTS);			/* clear error flags */

	/*
	 * now disable secondary cache and clear error flags
	 */
	val = *ka43_creg & ~KA43_SESR_CENB;	/* BICL !!! */
	*ka43_creg = val;			/* disable secondary cache */
	val = KA43_SESR_SERR | KA43_SESR_LERR | KA43_SESR_CERR;
	*ka43_creg = val;			/* clear error flags */

	return (0);
}

int
ka43_cache_invalidate()
{
	int i, val;

	val = KA43_PCTAG_PARITY;	/* clear valid flag, set parity bit */
	for (i = 0; i < 256; i++) {	/* 256 Quadword entries */
		mtpr(i*8, PR_PCIDX);	/* write index of tag */
		mtpr(val, PR_PCTAG);	/* write value into tag */
	}
	val = KA43_PCS_FLUSH | KA43_PCS_REFRESH;
	mtpr(val, PR_PCSTS);		/* flush primary cache */

	/*
	 * Rigel\'s secondary cache doesn\'t implement a valid-flag.
	 * Thus we initialize all entries with out-of-range/dummy
	 * addresses which will never be referenced (ie. never hit).
	 * After enabling cache we also access 128K of memory starting
	 * at 0x00 so that secondary cache will be filled with these
	 * valid addresses...
	 */
	val = 0xff;
	/* if (memory > 28 MB) val = 0x55; */
	printf("clearing tags...\n");
	for (i = 0; i < KA43_CT2_SIZE; i+= 4) {	/* Quadword entries ?? */
		ka43_ctag[i/4] = val;		/* reset upper and lower */
	}

	return (0);
}


int
ka43_cache_enable()
{
	volatile char *membase = (void*)0x80000000;	/* physical 0x00 */
	int i, val;

	val = KA43_PCS_FLUSH | KA43_PCS_REFRESH;
	mtpr(val, PR_PCSTS);		/* flush primary cache */

	/*
	 * now we enable secondary cache and access first 128K of memory
	 * so that secondary cache gets really initialized and holds
	 * valid addresses/data...
	 */
	*ka43_creg = KA43_SESR_CENB;	/* enable secondary cache */
	for (i=0; i<128*1024; i++) {
		val += membase[i];	/* some dummy operation... */
	}

	val = KA43_PCS_ENABLE | KA43_PCS_REFRESH;
	mtpr(val, PR_PCSTS);		/* enable primary cache */

	return (0);
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

	/*
	 * ka43_conf() gets called with MMU enabled, now it's save to
	 * init/reset the caches.
	 */
	ka43_cache_init();
}


/*
 * The interface for communication with the LANCE ethernet controller
 * is setup in the xxx_steal_pages() routine. We decrease highest
 * available address by 64K and use this area as communication buffer.
 */
u_long le_iomem;		/* base addr of RAM -- CPU's view */
u_long le_ioaddr;		/* base addr of RAM -- LANCE's view */

void
ka43_steal_pages()
{
	extern	vm_offset_t avail_start, virtual_avail, avail_end;
        extern  short *clk_page;
        extern  int clk_adrshift, clk_tweak;
	int	junk, val;
	int	i;

	printf ("ka43_steal_pages: avail_end=0x%x\n", avail_end);

	/* 
	 * SCB is already copied/initialized at addr avail_start
	 * by pmap_bootstrap(), but it's not yet mapped. Thus we use
	 * the MAPPHYS() macro to reserve these two pages and to
	 * perform the mapping. The mapped address is assigned to junk.
	 */
	MAPPHYS(junk, 2, VM_PROT_READ|VM_PROT_WRITE);

        clk_adrshift = 1;       /* Addressed at long's... */
        clk_tweak = 2;          /* ...and shift two */
        MAPVIRT(clk_page, 2);
        pmap_map((vm_offset_t)clk_page, (vm_offset_t)KA43_WAT_BASE,
            (vm_offset_t)KA43_WAT_BASE + NBPG, VM_PROT_READ|VM_PROT_WRITE);

#if 0
	/*
	 * At top of physical memory there are some console-prom and/or
	 * restart-specific data. Make this area unavailable.
	 */
	avail_end -= 64 * NBPG;		/* scratch RAM ??? */
	avail_end = 0x00FC0000;		/* XXX: for now from ">>> show mem" */

This is no longer neccessary since the memsize in RPB does not include
these unavailable pages. Only valid/available pages are counted in RPB.

#endif

	/*
	 * If we need to map physical areas also, we can decrease avail_end
	 * (the highest available memory-address), copy the stuff into the
	 * gap between and use pmap_map to map it. This is done for LANCE's
	 * 64K communication area.
	 *
	 * Don't use the MAPPHYS macro here, since this uses and changes(!)
	 * the value of avail_start. Use MAPVIRT even if it's name misleads.
	 */
	avail_end -= (64 * 1024);	/* reserve 64K */
	avail_end &= ~0xffff;		/* force proper (quad?) alignment */

	/*
	 * Oh holy shit! It took me over one year(!) to find out that
	 * the 3100/76 has to use diag-mem instead of physical memory
	 * for communication with LANCE (using phys-mem results in
	 * parity errors and mchk exceptions with code 17 (0x11)).
	 *
	 * Many thanks to Matt Thomas, without his help it could have
	 * been some more years...  ;-)
	 */
	le_ioaddr = avail_end | KA43_DIAGMEM;	/* ioaddr in diag-mem!!! */
	MAPVIRT(le_iomem, (64 * 1024)/NBPG);
	pmap_map((vm_offset_t)le_iomem, le_ioaddr, le_ioaddr + 0xffff,
		 VM_PROT_READ|VM_PROT_WRITE);

	/*
	 * if LANCE\'s io-buffer is above 16 MB, then the appropriate flag
	 * in the parity control register has to be set (it works as an
	 * additional address bit). In any case, don\'t enable CPEN and
	 * DPEN in the PARCTL register, somewhow they are internally managed
	 * by the RIGEL chip itself!?!
	 */
	val = ka43_cpu->parctl & 0x03;	/* read the old value */
	if (le_ioaddr & (1 << 24))	/* if RAM above 16 MB */
		val |= KA43_PCTL_DMA;	/* set LANCE DMA flag */
	ka43_cpu->parctl = val;		/* and write new value */
	le_ioaddr &= 0xffffff;		/* Lance uses 24-bit addresses */

	/*
	 * now map in anything listed in ka43_map...
	 */
	uvax_fillmap();

	/*
	 * Clear restart and boot in progress flags in the CPMBX. 
	 */
	((struct ka43_clock *)KA43_WAT_BASE)->cpmbx =
	    ((struct ka43_clock *)KA43_WAT_BASE)->cpmbx & 0xF0;

#if 0
	/*
	 * Clear all error flags, not really neccessary here, this will
	 * be done by ka43_cache_init() anyway...
	 */
	ka43_clear_errors();
#endif

	/*
	 * MM is not yet enabled, thus we still used the physical addresses,
	 * but before leaving this routine, we need to reset them to virtual.
	 */
	ka43_cpu    = (void*)uvax_phys2virt(KA43_CPU_BASE);
	ka43_creg   = (void*)uvax_phys2virt(KA43_CH2_CREG);
	ka43_ctag   = (void*)uvax_phys2virt(KA43_CT2_BASE);
}

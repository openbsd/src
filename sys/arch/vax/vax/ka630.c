/*	$NetBSD: ka630.c,v 1.4 1996/10/13 03:35:44 christos Exp $	*/
/*-
 * Copyright (c) 1982, 1988, 1990, 1993
 * 	The Regents of the University of California.  All rights reserved.
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
 *      @(#)ka630.c     7.8 (Berkeley) 5/9/91
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/time.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/pmap.h>
#include <machine/nexus.h>
#include <machine/uvax.h>
#include <machine/ka630.h>
#include <machine/clock.h>

struct uvaxIIcpu *uvaxIIcpu_ptr;

struct	ka630clock *ka630_clkptr = KA630CLK;

static void ka630_conf __P((struct device *, struct device *, void *));
static void ka630_memerr __P((void));
static int ka630_mchk __P((caddr_t));
static void ka630_steal_pages __P((void));


int
ka630_setup(uc,flags)
        struct uvax_calls *uc;
        int flags;
{
        uc->uc_name = "ka630";

        uc->uc_phys2virt = NULL;
        uc->uc_physmap = NULL;     /* ptv_map ? p2v_map */

        uc->uc_steal_pages = ka630_steal_pages;
        uc->uc_conf = ka630_conf;
        uc->uc_clkread = ka630_clkread;
        uc->uc_clkwrite = ka630_clkwrite;
        
#ifdef notyet
        uc->uc_memerr = ka630_memerr;
        uc->uc_mchk = ka630_mchk;
#endif  
        
        uc->uc_busTypes = VAX_UNIBUS;
} 

/*
 * uvaxII_conf() is called by cpu_attach to do the cpu_specific setup.
 */
void
ka630_conf(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	extern char cpu_model[];

	strcpy(cpu_model,"MicroVAX II");
	printf(": %s\n", cpu_model);
}

/* log crd errors */
uvaxII_memerr()
{
	printf("memory err!\n");
}

#define NMC78032 10
char *mc78032[] = {
	0,		"immcr (fsd)",	"immcr (ssd)",	"fpu err 0",
	"fpu err 7",	"mmu st(tb)",	"mmu st(m=0)",	"pte in p0",
	"pte in p1",	"un intr id",
};

struct mc78032frame {
	int	mc63_bcnt;		/* byte count == 0xc */
	int	mc63_summary;		/* summary parameter */
	int	mc63_mrvaddr;		/* most recent vad */
	int	mc63_istate;		/* internal state */
	int	mc63_pc;		/* trapped pc */
	int	mc63_psl;		/* trapped psl */
};

uvaxII_mchk(cmcf)
	caddr_t cmcf;
{
	register struct mc78032frame *mcf = (struct mc78032frame *)cmcf;
	register u_int type = mcf->mc63_summary;

	printf("machine check %x", type);
	if (type < NMC78032 && mc78032[type])
		printf(": %s", mc78032[type]);
	printf("\n\tvap %x istate %x pc %x psl %x\n",
	    mcf->mc63_mrvaddr, mcf->mc63_istate,
	    mcf->mc63_pc, mcf->mc63_psl);
	if (uvaxIIcpu_ptr && uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_MERR) {
		printf("\tmser=0x%x ", uvaxIIcpu_ptr->uvaxII_mser);
		if (uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_CPUE)
			printf("page=%d", uvaxIIcpu_ptr->uvaxII_cear);
		if (uvaxIIcpu_ptr->uvaxII_mser & UVAXIIMSER_DQPE)
			printf("page=%d", uvaxIIcpu_ptr->uvaxII_dear);
		printf("\n");
	}
	return (-1);
}

void
ka630_steal_pages()
{
	extern  vm_offset_t avail_start, virtual_avail, avail_end;
	int	junk;

	/*
	 * MicroVAX II: get 10 pages from top of memory,
	 * map in Qbus map registers, cpu and clock registers.
	 */
	avail_end -= 10;

	MAPPHYS(junk, 2, VM_PROT_READ|VM_PROT_WRITE);
	MAPVIRT(nexus, btoc(0x400000));
	pmap_map((vm_offset_t)nexus, 0x20088000, 0x20090000,
	    VM_PROT_READ|VM_PROT_WRITE);

	MAPVIRT(uvaxIIcpu_ptr, 1);
	pmap_map((vm_offset_t)uvaxIIcpu_ptr, (vm_offset_t)UVAXIICPU,
	    (vm_offset_t)UVAXIICPU + NBPG, VM_PROT_READ|VM_PROT_WRITE);

	MAPVIRT(ka630_clkptr, 1);
	pmap_map((vm_offset_t)ka630_clkptr, (vm_offset_t)KA630CLK,
	    (vm_offset_t)KA630CLK + NBPG, VM_PROT_READ|VM_PROT_WRITE);

	/*
	 * Clear restart and boot in progress flags
	 * in the CPMBX.
	 /
	ka630clk_ptr->cpmbx = (ka630clk_ptr->cpmbx & KA630CLK_LANG);

	/*
	 * Enable memory parity error detection and clear error bits.
	 */
	uvaxIIcpu_ptr->uvaxII_mser = (UVAXIIMSER_PEN | UVAXIIMSER_MERR |
	    UVAXIIMSER_LEB);

}
#define uVAX_gettodr    ka630_gettodr
#define uVAX_settodr    ka630_settodr
#define	uVAX_clkptr	ka630_clkptr
#define	uVAX_genclock	ka630_genclock
#define	uVAX_clock	ka630clock
#define	uVAX_clkread	ka630_clkread
#define	uVAX_clkwrite	ka630_clkwrite

#include <arch/vax/vax/uvax_proto.c>

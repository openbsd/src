/*	$NetBSD: uvax.c,v 1.3 1996/10/13 03:36:03 christos Exp $ */
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

/*
 * generic(?) MicroVAX and VAXstation support
 *
 * There are similarities to struct cpu_calls[] in autoconf.c
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/device.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/sid.h>
#include <machine/pmap.h>
#include <machine/nexus.h>
#include <machine/uvax.h>

#define xtrace(x)
#define xdebug(x)


struct uvax_calls guc;			/* Generic uVAX Calls */
/* struct uvax_calls *ucp = &guc;	/* not yet public !!! */
static int uvax_callsSetup = 0;		/* not yet setup */

u_long	uVAX_phys2virt __P((u_long, struct uc_map *));

/* u_long	uVAX_physmap;	/* XXX  another ugly hack... */
int 
uvax_notavail(s)
	char *s;
{
	printf("\"%s()\" not available for uVAX (%s)\n", s, guc.uc_name);
	/*
	 * should we panic() here???
	 */
	return(0);
}

int
uvax_setup(flags)
	int flags;
{
	/*
	 * insert some defaults here !!!
	 */

	/*
	 * Now call the specific routines to overwrite these defaults
	 */
	switch (vax_boardtype) {
#ifdef VAX630
	case VAX_BTYP_630:
		ka630_setup(&guc, flags);
		break;
#endif
#ifdef VAX410
	case VAX_BTYP_410:
		ka410_setup(&guc, flags);
		break;
#endif
#ifdef VAX43
	case VAX_BTYP_43:
		ka43_setup(&guc, flags);
		break;
#endif
	default:
		printf("don't know how to handle 0x%x\n", vax_boardtype);
		printf("Let's try using the defaults...\n");
	}
	uvax_callsSetup = 1;
}

/*
 * XXX_steal_pages() is the first cpu/board specific function to be called.
 * Thus we use this call to setup the dispatch structure for further use.
 *
 * We should have a special setup-routine !!!
 */
void
uvax_steal_pages()
{
	if (uvax_callsSetup == 0)
		uvax_setup(0);

	/*
	 * now that specific functions are inserted, we can call 'em
	 */
	if (guc.uc_steal_pages) {
		(guc.uc_steal_pages)(); 
		return;
	}
	uvax_notavail("uc_steal_pages");
}

u_long	
uvax_phys2virt(paddr)
	u_long paddr;
{
	if (guc.uc_phys2virt)
		return ((guc.uc_phys2virt)(paddr));
	if (guc.uc_physmap)
		return (uVAX_phys2virt(paddr, guc.uc_physmap));
	uvax_notavail("uc_phys2virt");
	return (0);
}

void
uvax_conf(parent, self, aux)
	struct	device *parent, *self;
	void	*aux;
{
	if (guc.uc_conf) {
		(guc.uc_conf)(parent, self, aux);
		return;
	}
	uvax_notavail("uc_conf");
}

void
uvax_memerr()
{
	xtrace(("uvax_memerr()\n"));

	if (guc.uc_memerr) {
		(guc.uc_memerr)(); 
		return;
	}
	uvax_notavail("uc_memerr");
}

int
uvax_mchk(addr)
	caddr_t addr;
{
	xtrace(("uvax_mchk(0x%x)\n", addr));

	if (guc.uc_mchk)
		return ((guc.uc_mchk)(addr));
	uvax_notavail("uc_mchk");
	return (-1);
}

int
uvax_clkread(base)
	time_t base;
{
	if (guc.uc_clkread)
		return ((guc.uc_clkread)(base));
	uvax_notavail("uc_clkread");
}

void
uvax_clkwrite()
{
	if (guc.uc_clkwrite)
		(guc.uc_clkwrite)();
	else
		uvax_notavail("uc_clkwrite");
	return;
}

/*
 * NB: mapping should/must be done in chunks of PAGE_SIZE (ie. 1024), 
 *     while pmap_map() expects size to be in chunks of NBPG (ie. 512).
 * 
 * Thus we round down the start-address to be aligned wrt PAGE_SIZE and
 * the end-address up to be just beyond the next multiple of PAGE_SIZE.
 * size is the number of bytes between start and end expressed in NBPG.
 */
int 
uVAX_old_fillmap(um)
	struct uc_map *um;
{
	extern  vm_offset_t avail_start, virtual_avail, avail_end;
	register struct uc_map *p;
	register u_int base, end, size;

	for (p = um; p->um_base != 0; p++) {
		base = p->um_base & ~PAGE_SIZE;		/* round base down */
		end = ROUND_PAGE(p->um_end + 1) - 1;	/* round end up */
		size = (end - base + 1) / NBPG;		/* size in pages */
		MAPVIRT(p->um_virt, size);
		pmap_map((vm_offset_t)p->um_virt, base, end, 
			 VM_PROT_READ|VM_PROT_WRITE);

		xdebug(("uVAX_fillmap: %x:%x[%x] (%x:%x[%x]) --> %x\n", 
			p->um_base, p->um_end, p->um_size, 
			base, end, size, p->um_virt));

	}
}

/*
 * NB: mapping should/must be done in chunks of PAGE_SIZE (ie. 1024), 
 *     while pmap_map() expects size to be in chunks of NBPG (ie. 512).
 * 
 * Thus we round down the start-address to be aligned wrt PAGE_SIZE and
 * the end-address up to be just beyond the next multiple of PAGE_SIZE.
 * size is the number of bytes between start and end expressed in NBPG.
 */
int 
uVAX_fillmap(um)
	struct uc_map *um;
{
	extern  vm_offset_t avail_start, virtual_avail, avail_end;
	register struct uc_map *p;
	register u_int base, end, off, size;

	for (p = um; p->um_base != 0; p++) {
		base = TRUNC_PAGE(p->um_base);		/* round base down */
		off = p->um_base - base;
		size = ROUND_PAGE(off + p->um_size);
		if (size < PAGE_SIZE) {
			printf("invalid size %d in uVAX_fillmap\n", size);
			size = PAGE_SIZE;
		}
		end = base + size - 1;
		MAPVIRT(p->um_virt, size/NBPG);
		pmap_map((vm_offset_t)p->um_virt, base, end, 
			 VM_PROT_READ|VM_PROT_WRITE);

		xdebug(("uVAX_fillmap: %x:%x[%x] (%x:%x[%x]) --> %x\n", 
			p->um_base, p->um_end, p->um_size, 
			base, end, size, p->um_virt));

	}
}

u_long
uVAX_phys2virt(phys,um)
	u_long phys;
	struct uc_map *um;
{
	register struct uc_map *p;
	u_long virt = 0;

	for (p = um; p->um_base != 0; p++) {
		if (p->um_base > phys || p->um_end < phys)
			continue;
		virt = p->um_virt + (phys - trunc_page(p->um_base));
		break;
	}

	if (virt == 0) {
		printf("invalid argument 0x%x to uvax_phys2virt()\n", phys);
		/* should we panic() here ??? */
	}

	return (virt);
}

/*	$OpenBSD: cats_machdep.c,v 1.9 2004/09/16 21:52:50 miod Exp $	*/
/*	$NetBSD: cats_machdep.c,v 1.50 2003/10/04 14:28:28 chris Exp $	*/

/*
 * Copyright (c) 1997,1998 Mark Brinicombe.
 * Copyright (c) 1997,1998 Causality Limited.
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Machine dependant functions for kernel setup for EBSA285 core architecture
 * using cyclone firmware
 *
 * Created      : 24/11/97
 */

#include <sys/cdefs.h>
#include "isadma.h"

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/reboot.h>
#include <sys/termios.h>

#include <dev/cons.h>

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>

#include <machine/bootconfig.h>
#define	_ARM32_BUS_DMA_PRIVATE
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/intr.h>
#include <arm/undefined.h>
#include <arm/machdep.h>
 
#include <machine/cyclone_boot.h>
#include <arm/footbridge/dc21285mem.h>
#include <arm/footbridge/dc21285reg.h>

#include "isa.h"
#if NISA > 0
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#endif

/* Kernel text starts at the base of the kernel address space. */
#define	KERNEL_TEXT_BASE	(KERNEL_BASE + 0x00000000)
#define	KERNEL_VM_BASE		(KERNEL_BASE + 0x01000000)

/*
 * The range 0xf1000000 - 0xfcffffff is available for kernel VM space
 * Footbridge registers and I/O mappings occupy 0xfd000000 - 0xffffffff
 */

/*
 * Size of available KVM space, note that growkernel will grow into this.
 */
#define KERNEL_VM_SIZE	0x0C000000

/*
 * Address to call from cpu_reset() to reset the machine.
 * This is machine architecture dependant as it varies depending
 * on where the ROM appears when you turn the MMU off.
 */

u_int cpu_reset_address = DC21285_ROM_BASE;

u_int dc21285_fclk = FCLK;

/* Define various stack sizes in pages */
#define IRQ_STACK_SIZE	1
#define ABT_STACK_SIZE	1
#ifdef IPKDB
#define UND_STACK_SIZE	2
#else
#define UND_STACK_SIZE	1
#endif

struct ebsaboot ebsabootinfo;
BootConfig bootconfig;		/* Boot config storage */
static char bootargs[MAX_BOOT_STRING + 1];
char *boot_args = NULL;
char *boot_file = NULL;

vm_offset_t physical_start;
vm_offset_t physical_freestart;
vm_offset_t physical_freeend;
vm_offset_t physical_end;
u_int free_pages;
vm_offset_t pagetables_start;
int physmem = 0;

/*int debug_flags;*/
#ifndef PMAP_STATIC_L1S
int max_processes = 64;			/* Default number */
#endif	/* !PMAP_STATIC_L1S */

/* Physical and virtual addresses for some global pages */
pv_addr_t systempage;
pv_addr_t irqstack;
pv_addr_t undstack;
pv_addr_t abtstack;
extern pv_addr_t kernelstack;

vm_offset_t msgbufphys;

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
extern u_int undefined_handler_address;

#ifdef PMAP_DEBUG
extern int pmap_debug_level;
#endif

#define KERNEL_PT_SYS		0	/* L2 table for mapping zero page */
#define KERNEL_PT_KERNEL	1	/* L2 table for mapping kernel */
#define	KERNEL_PT_KERNEL_NUM	2

/* now this could move into something more generic */
					/* L2 tables for mapping kernel VM */
#define	KERNEL_PT_VMDATA	(KERNEL_PT_KERNEL + KERNEL_PT_KERNEL_NUM)
#define	KERNEL_PT_VMDATA_NUM	4	/* 16MB kernel VM !*/
#define NUM_KERNEL_PTS		(KERNEL_PT_VMDATA + KERNEL_PT_VMDATA_NUM)

pv_addr_t kernel_pt_table[NUM_KERNEL_PTS];

extern struct user *proc0paddr;

/* Prototypes */

void consinit(void);

int fcomcnattach(u_int iobase, int rate,tcflag_t cflag);
int fcomcndetach(void);

static void process_kernel_args(char *);
extern void configure(void);

/* A load of console goo. */
#include "vga.h"
#if (NVGA > 0)
#include <dev/ic/mc6845reg.h>
#include <dev/ic/pcdisplayvar.h>
#include <dev/ic/vgareg.h>
#include <dev/ic/vgavar.h>
#endif

#include "pckbc.h"
#if (NPCKBC > 0)
#include <dev/ic/i8042reg.h>
#include <dev/ic/pckbcvar.h>
#endif

#include "com.h"
#if (NCOM > 0)
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
#ifndef CONCOMADDR
#define CONCOMADDR 0x3f8
#endif
#endif

#ifndef CONSDEVNAME
#define CONSDEVNAME "vga"
#endif

#define CONSPEED B38400
#ifndef CONSPEED
#define CONSPEED B9600	/* TTYDEF_SPEED */
#endif
#ifndef CONMODE
#define CONMODE ((TTYDEF_CFLAG & ~(CSIZE | CSTOPB | PARENB)) | CS8) /* 8N1 */
#endif

int comcnspeed = CONSPEED;
int comcnmode = CONMODE;


/*
 * void cpu_reboot(int howto, char *bootstr)
 *
 * Reboots the system
 *
 * Deal with any syncing, unmounting, dumping and shutdown hooks,
 * then reset the CPU.
 */

void
boot(howto)
	int howto;
{
#ifdef DEBUG
	/* info */
	printf("boot: howto=%08x curproc=%p\n", howto, curproc);
#endif

	/*
	 * If we are still cold then hit the air brakes
	 * and crash to earth fast
	 */
	if (cold) {
		doshutdownhooks();
		/*
		 * If the system is cold, just halt, unless the user
		 * explicitly asked for reboot.
		 */
		if ((howto & (RB_HALT | RB_USERREQ)) != RB_USERREQ) {
			printf("The operating system has halted.\n");
			printf("Please press any key to reboot.\n\n");
			cngetc();
		}
		printf("rebooting...\n");
		cpu_reset();
		/*NOTREACHED*/
	}

	/* Disable console buffering */
/*	cnpollc(1);*/

	/*
	 * If RB_NOSYNC was not specified sync the discs.
	 * Note: Unless cold is set to 1 here, syslogd will die during the unmount.
	 * It looks like syslogd is getting woken up only to find that it cannot
	 * page part of the binary in as the filesystem has been unmounted.
	 */
	if (!(howto & RB_NOSYNC))
		bootsync();

	/* Say NO to interrupts */
	splhigh();

	/* Do a dump if requested. */
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
		dumpsys();
	
	/* Run any shutdown hooks */
	doshutdownhooks();

	/* Make sure IRQ's are disabled */
	IRQdisable;

	if (howto & RB_HALT) {
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	}

	printf("rebooting...\n");
	cpu_reset();
	/*NOTREACHED*/
}

/*
 * Mapping table for core kernel memory. This memory is mapped at init
 * time with section mappings.
 */
struct l1_sec_map {
	vm_offset_t	va;
	vm_offset_t	pa;
	vm_size_t	size;
	vm_prot_t	prot;
	int		cache;
} l1_sec_table[] = {
	/* Map 1MB for CSR space */
	{ DC21285_ARMCSR_VBASE,			DC21285_ARMCSR_BASE,
	    DC21285_ARMCSR_VSIZE,		VM_PROT_READ|VM_PROT_WRITE,
	    PTE_NOCACHE },

	/* Map 1MB for fast cache cleaning space */
	{ DC21285_CACHE_FLUSH_VBASE,		DC21285_SA_CACHE_FLUSH_BASE,
	    DC21285_CACHE_FLUSH_VSIZE,		VM_PROT_READ|VM_PROT_WRITE,
	    PTE_CACHE },

	/* Map 1MB for PCI IO space */
	{ DC21285_PCI_IO_VBASE,			DC21285_PCI_IO_BASE,
	    DC21285_PCI_IO_VSIZE,		VM_PROT_READ|VM_PROT_WRITE,
	    PTE_NOCACHE },

	/* Map 1MB for PCI IACK space */
	{ DC21285_PCI_IACK_VBASE,		DC21285_PCI_IACK_SPECIAL,
	    DC21285_PCI_IACK_VSIZE,		VM_PROT_READ|VM_PROT_WRITE,
	    PTE_NOCACHE },

	/* Map 16MB of type 1 PCI config access */
	{ DC21285_PCI_TYPE_1_CONFIG_VBASE,	DC21285_PCI_TYPE_1_CONFIG,
	    DC21285_PCI_TYPE_1_CONFIG_VSIZE,	VM_PROT_READ|VM_PROT_WRITE,
	    PTE_NOCACHE },

	/* Map 16MB of type 0 PCI config access */
	{ DC21285_PCI_TYPE_0_CONFIG_VBASE,	DC21285_PCI_TYPE_0_CONFIG,
	    DC21285_PCI_TYPE_0_CONFIG_VSIZE,	VM_PROT_READ|VM_PROT_WRITE,
	    PTE_NOCACHE },

	/* Map 1MB of 32 bit PCI address space for ISA MEM accesses via PCI */
	{ DC21285_PCI_ISA_MEM_VBASE,		DC21285_PCI_MEM_BASE,
	    DC21285_PCI_ISA_MEM_VSIZE,		VM_PROT_READ|VM_PROT_WRITE,
	    PTE_NOCACHE },

	{ 0, 0, 0, 0, 0 }
};

/*
 * u_int initarm(struct ebsaboot *bootinfo)
 *
 * Initial entry point on startup. This gets called before main() is
 * entered.
 * It should be responsible for setting up everything that must be
 * in place when main is called.
 * This includes
 *   Taking a copy of the boot configuration structure.
 *   Initialising the physical console so characters can be printed.
 *   Setting up page tables for the kernel
 *   Relocating the kernel to the bottom of physical memory
 */

u_int
initarm(bootargs)
	void *bootargs;
{
	struct ebsaboot *bootinfo = bootargs;
	int loop;
	int loop1;
	u_int l1pagetable;
	pv_addr_t kernel_l1pt;
	extern u_int cpu_get_control(void);

	/*
	 * Heads up ... Setup the CPU / MMU / TLB functions
	 */
	set_cpufuncs();

	/* Copy the boot configuration structure */
	ebsabootinfo = *bootinfo;

	if (ebsabootinfo.bt_fclk >= 50000000
	    && ebsabootinfo.bt_fclk <= 66000000)
		dc21285_fclk = ebsabootinfo.bt_fclk;

	/* Fake bootconfig structure for the benefit of pmap.c */
	/* XXX must make the memory description h/w independant */
	bootconfig.dramblocks = 1;
	bootconfig.dram[0].address = ebsabootinfo.bt_memstart;
	bootconfig.dram[0].pages = (ebsabootinfo.bt_memend
	    - ebsabootinfo.bt_memstart) / PAGE_SIZE;

	/*
	 * Initialise the diagnostic serial console
	 * This allows a means of generating output during initarm().
	 * Once all the memory map changes are complete we can call consinit()
	 * and not have to worry about things moving.
	 */
#ifdef FCOM_INIT_ARM
	fcomcnattach(DC21285_ARMCSR_BASE, comcnspeed, comcnmode);
#endif

	/* Talk to the user */
	printf("OpenBSD/cats booting ...\n");

	if (ebsabootinfo.bt_magic != BT_MAGIC_NUMBER_EBSA
	    && ebsabootinfo.bt_magic != BT_MAGIC_NUMBER_CATS)
		panic("Incompatible magic number passed in boot args");

#ifdef DEBUG
	for (loop = 0; loop < 8; ++loop) {
		printf("%08x\n", *(((int *)bootinfo)+loop));
	}
#endif

	/*
	 * Ok we have the following memory map
	 *
	 * virtual address == physical address apart from the areas:
	 * 0x00000000 -> 0x000fffff which is mapped to
	 * top 1MB of physical memory
	 * 0x00100000 -> 0x0fffffff which is mapped to
	 * physical addresses 0x00100000 -> 0x0fffffff
	 * 0x10000000 -> 0x1fffffff which is mapped to
	 * physical addresses 0x00000000 -> 0x0fffffff
	 * 0x20000000 -> 0xefffffff which is mapped to
	 * physical addresses 0x20000000 -> 0xefffffff
	 * 0xf0000000 -> 0xf03fffff which is mapped to
	 * physical addresses 0x00000000 -> 0x003fffff
	 *
	 * This means that the kernel is mapped suitably for continuing
	 * execution, all I/O is mapped 1:1 virtual to physical and
	 * physical memory is accessible.
	 *
	 * The initarm() has the responsibility for creating the kernel
	 * page tables.
	 * It must also set up various memory pointers that are used
	 * by pmap etc. 
	 */

	/*
	 * Examine the boot args string for options we need to know about
	 * now.
	 */
	process_kernel_args((char *)ebsabootinfo.bt_args);

#ifdef DEBUG
	printf("initarm: Configuring system ...\n");
#endif

	/*
	 * Set up the variables that define the availablilty of
	 * physical memory
	 */
	physical_start = ebsabootinfo.bt_memstart;
	physical_freestart = physical_start;
	physical_end = ebsabootinfo.bt_memend;
	physical_freeend = physical_end;
	free_pages = (physical_end - physical_start) / PAGE_SIZE;
    
	physmem = (physical_end - physical_start) / PAGE_SIZE;

#ifdef DEBUG
	/* Tell the user about the memory */
	printf("physmemory: %d pages at 0x%08lx -> 0x%08lx\n", physmem,
	    physical_start, physical_end - 1);
#endif

	/*
	 * Ok the kernel occupies the bottom of physical memory.
	 * The first free page after the kernel can be found in
	 * ebsabootinfo->bt_memavail
	 * We now need to allocate some fixed page tables to get the kernel
	 * going.
	 * We allocate one page directory and a number page tables and store
	 * the physical addresses in the kernel_pt_table array.
	 *
	 * Ok the next bit of physical allocation may look complex but it is
	 * simple really. I have done it like this so that no memory gets
	 * wasted during the allocation of various pages and tables that are
	 * all different sizes.
	 * The start addresses will be page aligned.
	 * We allocate the kernel page directory on the first free 16KB boundry
	 * we find.
	 * We allocate the kernel page tables on the first 4KB boundry we find.
	 * Since we allocate at least 3 L2 pagetables we know that we must
	 * encounter at least one 16KB aligned address.
	 */

#ifdef VERBOSE_INIT_ARM
	printf("Allocating page tables");
#endif

	/* Update the address of the first free page of physical memory */
	physical_freestart = ebsabootinfo.bt_memavail;
	free_pages -= (physical_freestart - physical_start) / PAGE_SIZE;
	
#ifdef VERBOSE_INIT_ARM
	printf(" above %p\n", (void *)physical_freestart);
#endif
	/* Define a macro to simplify memory allocation */
#define	valloc_pages(var, np)			\
	alloc_pages((var).pv_pa, (np));	\
	(var).pv_va = KERNEL_BASE + (var).pv_pa - physical_start;

#define alloc_pages(var, np)			\
	(var) = physical_freestart;		\
	physical_freestart += ((np) * PAGE_SIZE);\
	free_pages -= (np);			\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	loop1 = 0;
	kernel_l1pt.pv_pa = 0;
	for (loop = 0; loop <= NUM_KERNEL_PTS; ++loop) {
		/* Are we 16KB aligned for an L1 ? */
		if ((physical_freestart & (L1_TABLE_SIZE - 1)) == 0
		    && kernel_l1pt.pv_pa == 0) {
			valloc_pages(kernel_l1pt, L1_TABLE_SIZE / PAGE_SIZE);
		} else {
			valloc_pages(kernel_pt_table[loop1],
					L2_TABLE_SIZE / PAGE_SIZE);
			++loop1;			
		}
	}

#ifdef DIAGNOSTIC
	/* This should never be able to happen but better confirm that. */
	if (!kernel_l1pt.pv_pa || (kernel_l1pt.pv_pa & (L1_TABLE_SIZE-1)) != 0)
		panic("initarm: Failed to align the kernel page directory");
#endif

	/*
	 * Allocate a page for the system page mapped to V0x00000000
	 * This page will just contain the system vectors and can be
	 * shared by all processes.
	 */
	alloc_pages(systempage.pv_pa, 1);

	/* Allocate stacks for all modes */
	valloc_pages(irqstack, IRQ_STACK_SIZE);
	valloc_pages(abtstack, ABT_STACK_SIZE);
	valloc_pages(undstack, UND_STACK_SIZE);
	valloc_pages(kernelstack, UPAGES);

#ifdef VERBOSE_INIT_ARM
	printf("IRQ stack: p0x%08lx v0x%08lx\n", irqstack.pv_pa, irqstack.pv_va); 
	printf("ABT stack: p0x%08lx v0x%08lx\n", abtstack.pv_pa, abtstack.pv_va); 
	printf("UND stack: p0x%08lx v0x%08lx\n", undstack.pv_pa, undstack.pv_va); 
	printf("SVC stack: p0x%08lx v0x%08lx\n", kernelstack.pv_pa, kernelstack.pv_va); 
#endif

	alloc_pages(msgbufphys, round_page(MSGBUFSIZE) / PAGE_SIZE);

	/*
	 * Ok we have allocated physical pages for the primary kernel
	 * page tables
	 */

#ifdef VERBOSE_INIT_ARM
	printf("Creating L1 page table\n");
#endif

	/*
	 * Now we start consturction of the L1 page table
	 * We start by mapping the L2 page tables into the L1.
	 * This means that we can replace L1 mappings later on if necessary
	 */
	l1pagetable = kernel_l1pt.pv_pa;

	/* Map the L2 pages tables in the L1 page table */
	pmap_link_l2pt(l1pagetable, 0x00000000,
	    &kernel_pt_table[KERNEL_PT_SYS]);

	for (loop = 0; loop < KERNEL_PT_KERNEL_NUM; loop++)
		pmap_link_l2pt(l1pagetable, KERNEL_BASE + loop * 0x00400000,
		    &kernel_pt_table[KERNEL_PT_KERNEL + loop]);

	for (loop = 0; loop < KERNEL_PT_VMDATA_NUM; ++loop)
		pmap_link_l2pt(l1pagetable, KERNEL_VM_BASE + loop * 0x00400000,
		    &kernel_pt_table[KERNEL_PT_VMDATA + loop]);

	/* update the top of the kernel VM */
	pmap_curmaxkvaddr =
	    KERNEL_VM_BASE + (KERNEL_PT_VMDATA_NUM * 0x00400000);
	
#ifdef VERBOSE_INIT_ARM
	printf("Mapping kernel\n");
#endif

	/* Now we fill in the L2 pagetable for the kernel static code/data */
	{
		extern char etext[], _end[];
		size_t textsize = (u_int32_t) etext - KERNEL_BASE;
		size_t totalsize = (u_int32_t) _end - KERNEL_BASE;
		u_int logical;
		
		textsize = round_page(textsize);
		totalsize = round_page(totalsize);

		logical = pmap_map_chunk(l1pagetable, KERNEL_BASE,
		    physical_start, textsize,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

		(void) pmap_map_chunk(l1pagetable, KERNEL_BASE + logical,
		    physical_start + logical, totalsize - textsize,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	}

	/*
	 * PATCH PATCH ...
	 *
	 * Fixup the first word of the kernel to be the instruction
	 * add pc, pc, #0x41000000
	 *
	 * This traps the case where the CPU core resets due to bus contention
	 * on a prototype CATS system and will reboot into the firmware.
	 */
	*((u_int *)KERNEL_TEXT_BASE) = 0xe28ff441;

#ifdef VERBOSE_INIT_ARM
	printf("Constructing L2 page tables\n");
#endif

	/* Map the boot arguments page */
	pmap_map_entry(l1pagetable, ebsabootinfo.bt_vargp,
	    ebsabootinfo.bt_pargp, VM_PROT_READ, PTE_CACHE);

	/* Map the stack pages */
	pmap_map_chunk(l1pagetable, irqstack.pv_va, irqstack.pv_pa,
	    IRQ_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, abtstack.pv_va, abtstack.pv_pa,
	    ABT_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, undstack.pv_va, undstack.pv_pa,
	    UND_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, kernelstack.pv_va, kernelstack.pv_pa,
	    UPAGES * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	pmap_map_chunk(l1pagetable, kernel_l1pt.pv_va, kernel_l1pt.pv_pa,
	    L1_TABLE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);

	for (loop = 0; loop < NUM_KERNEL_PTS; ++loop) {
		pmap_map_chunk(l1pagetable, kernel_pt_table[loop].pv_va,
		    kernel_pt_table[loop].pv_pa, L2_TABLE_SIZE,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	}

	/* Map the vector page. */
	pmap_map_entry(l1pagetable, vector_page, systempage.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);

	/* Map the core memory needed before autoconfig */
	loop = 0;
	while (l1_sec_table[loop].size) {
		vm_size_t sz;

#ifdef VERBOSE_INIT_ARM
		printf("%08lx -> %08lx @ %08lx\n", l1_sec_table[loop].pa,
		    l1_sec_table[loop].pa + l1_sec_table[loop].size - 1,
		    l1_sec_table[loop].va);
#endif
		for (sz = 0; sz < l1_sec_table[loop].size; sz += L1_S_SIZE)
			pmap_map_section(l1pagetable,
			    l1_sec_table[loop].va + sz,
			    l1_sec_table[loop].pa + sz,
			    l1_sec_table[loop].prot,
			    l1_sec_table[loop].cache);
		++loop;
	}

	/*
	 * Now we have the real page tables in place so we can switch to them.
	 * Once this is done we will be running with the REAL kernel page tables.
	 */
#ifdef VERBOSE_INIT_ARM
	/* checking sttb address */
	printf("setttb address = %p\n", cpufuncs.cf_setttb);

	printf("kernel_l1pt=0x%08x old = 0x%08x, phys = 0x%08x\n",
			((uint*)kernel_l1pt.pv_va)[0xf00],
			((uint*)ebsabootinfo.bt_l1)[0xf00],
			((uint*)kernel_l1pt.pv_pa)[0xf00]);

	printf("old pt @ %p, new pt @ %p\n", (uint*)kernel_l1pt.pv_pa, (uint*)ebsabootinfo.bt_l1);

	printf("Enabling System access\n");
#endif
	/* 
	 * enable the system bit in the control register, otherwise we can't
	 * access the kernel after the switch to the new L1 table
	 * I suspect cyclone hid this problem, by enabling the ROM bit
	 * Note can not have both SYST and ROM enabled together, the results
	 * are "undefined"
	 */
	cpu_control(CPU_CONTROL_SYST_ENABLE | CPU_CONTROL_ROM_ENABLE,
	    CPU_CONTROL_SYST_ENABLE);
#ifdef VERBOSE_INIT_ARM
	printf("switching domains\n");
#endif
	/* be a client to all domains */
	cpu_domains(0x55555555);
	/* Switch tables */
#ifdef VERBOSE_INIT_ARM
	printf("switching to new L1 page table\n");
#endif

	/*
	 * Ok the DC21285 CSR registers are about to be moved.
	 * Detach the diagnostic serial port.
	 */
#ifdef FCOM_INIT_ARM
	fcomcndetach();
#endif
	
	setttb(kernel_l1pt.pv_pa);

	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2));
	/*
	 * Moved from cpu_startup() as data_abort_handler() references
	 * this during uvm init
	 */
	proc0paddr = (struct user *)kernelstack.pv_va;
	proc0.p_addr = proc0paddr;
	/*
	 * XXX this should only be done in main() but it useful to
	 * have output earlier ...
	 */
	consinit();

#ifdef VERBOSE_INIT_ARM
	printf("bootstrap done.\n");
#endif

	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif 
	}

	arm32_vector_init(ARM_VECTORS_LOW, ARM_VEC_ALL);

	/*
	 * Pages were allocated during the secondary bootstrap for the
	 * stacks for different CPU modes.
	 * We must now set the r13 registers in the different CPU modes to
	 * point to these stacks.
	 * Since the ARM stacks use STMFD etc. we must set r13 to the top end
	 * of the stack memory.
	 */
#ifdef VERBOSE_INIT_ARM
	printf("init subsystems: stacks ");
#endif

	set_stackptr(PSR_IRQ32_MODE,
	    irqstack.pv_va + IRQ_STACK_SIZE * PAGE_SIZE);
	set_stackptr(PSR_ABT32_MODE,
	    abtstack.pv_va + ABT_STACK_SIZE * PAGE_SIZE);
	set_stackptr(PSR_UND32_MODE,
	    undstack.pv_va + UND_STACK_SIZE * PAGE_SIZE);

	/*
	 * Well we should set a data abort handler.
	 * Once things get going this will change as we will need a proper handler.
	 * Until then we will use a handler that just panics but tells us
	 * why.
	 * Initialisation of the vectors will just panic on a data abort.
	 * This just fills in a slighly better one.
	 */
#ifdef VERBOSE_INIT_ARM
	printf("vectors ");
#endif
	data_abort_handler_address = (u_int)data_abort_handler;
	prefetch_abort_handler_address = (u_int)prefetch_abort_handler;
	undefined_handler_address = (u_int)undefinedinstruction_bounce;

	/* At last !
	 * We now have the kernel in physical memory from the bottom upwards.
	 * Kernel page tables are physically above this.
	 * The kernel is mapped to KERNEL_TEXT_BASE
	 * The kernel data PTs will handle the mapping of 0xf1000000-0xf3ffffff
	 * The page tables are mapped to 0xefc00000
	 */

	/* Initialise the undefined instruction handlers */
#ifdef VERBOSE_INIT_ARM
	printf("undefined ");
#endif
	undefined_init();

	/* Load memory into UVM. */
#ifdef VERBOSE_INIT_ARM
	printf("page ");
#endif
	uvm_setpagesize();	/* initialize PAGE_SIZE-dependent variables */

	/* XXX Always one RAM block -- nuke the loop. */
	for (loop = 0; loop < bootconfig.dramblocks; loop++) {
		paddr_t start = (paddr_t)bootconfig.dram[loop].address;
		paddr_t end = start + (bootconfig.dram[loop].pages * PAGE_SIZE);
#if NISADMA > 0
		paddr_t istart, isize;
		extern struct arm32_dma_range *footbridge_isa_dma_ranges;
		extern int footbridge_isa_dma_nranges;
#endif

		if (start < physical_freestart)
			start = physical_freestart;
		if (end > physical_freeend)
			end = physical_freeend;

#if 0
		printf("%d: %lx -> %lx\n", loop, start, end - 1);
#endif

#if NISADMA > 0
		if (arm32_dma_range_intersect(footbridge_isa_dma_ranges,
					      footbridge_isa_dma_nranges,
					      start, end - start,
					      &istart, &isize)) {
			/*
			 * Place the pages that intersect with the
			 * ISA DMA range onto the ISA DMA free list.
			 */
#if 0
			printf("    ISADMA 0x%lx -> 0x%lx\n", istart,
			    istart + isize - 1);
#endif
			uvm_page_physload(atop(istart),
			    atop(istart + isize), atop(istart),
			    atop(istart + isize), VM_FREELIST_ISADMA);

			/*
			 * Load the pieces that come before the
			 * intersection onto the default free list.
			 */
			if (start < istart) {
#if 0
				printf("    BEFORE 0x%lx -> 0x%lx\n",
				    start, istart - 1);
#endif
				uvm_page_physload(atop(start),
				    atop(istart), atop(start),
				    atop(istart), VM_FREELIST_DEFAULT);
			}

			/*
			 * Load the pieces that come after the
			 * intersection onto the default free list.
			 */
			if ((istart + isize) < end) {
#if 0
				printf("     AFTER 0x%lx -> 0x%lx\n",
				    (istart + isize), end - 1);
#endif
				uvm_page_physload(atop(istart + isize),
				    atop(end), atop(istart + isize), 
				    atop(end), VM_FREELIST_DEFAULT);
			}
		} else {
			uvm_page_physload(atop(start), atop(end),
			    atop(start), atop(end), VM_FREELIST_DEFAULT);
		}
#else /* NISADMA > 0 */
		uvm_page_physload(atop(start), atop(end),
		    atop(start), atop(end), VM_FREELIST_DEFAULT);
#endif /* NISADMA > 0 */
	}

	/* Boot strap pmap telling it where the kernel page table is */
#ifdef VERBOSE_INIT_ARM
	printf("pmap ");
#endif
	pmap_bootstrap((pd_entry_t *)kernel_l1pt.pv_va, KERNEL_VM_BASE,
	    KERNEL_VM_BASE + KERNEL_VM_SIZE);

	/* Setup the IRQ system */
#ifdef VERBOSE_INIT_ARM
	printf("irq ");
#endif
	footbridge_intr_init();
#ifdef VERBOSE_INIT_ARM
	printf("done.\n");
#endif

#ifdef IPKDB
	/* Initialise ipkdb */
	ipkdb_init();
	if (boothowto & RB_KDB)
		ipkdb_connect(0);
#endif

#if 0
#if NKSYMS || defined(DDB) || defined(LKM)
#ifdef __ELF__
	/* ok this is really rather sick, in ELF what happens is that the
	 * ELF symbol table is added after the text section.
	 */
	ksyms_init(0, NULL, NULL);	/* XXX */
#else
	{
		extern int end;
		extern int *esym;

		ksyms_init(*(int *)&end, ((int *)&end) + 1, esym);
	}
#endif /* __ELF__ */
#endif
#endif

#ifdef DDB
	db_machine_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif

	/* We return the new stack pointer address */
	return(kernelstack.pv_va + USPACE_SVC_STACK_TOP);
}

char *console = CONSDEVNAME;

static void
process_kernel_args(args)
	char *args;
{

	boothowto = 0;

	/* Make a local copy of the bootargs */
	strncpy(bootargs, args, MAX_BOOT_STRING);

	args = bootargs;
	boot_file = bootargs;

	if (strncmp(args, "(hd0)", 5)== 0)
		boot_file = "wd0";
	
	/* Skip the kernel image filename, or 'setargs' */
	while (*args != ' ' && *args != 0)
		++args;
	if (*args != 0)
		*args++ = 0;


	while (*args == ' ')
		++args;
	boot_args = args;

	while (*args == '-') {
		while (*args && *args != ' ') {
			switch (*args++) {
			case 'a':
				boothowto |= RB_ASKNAME;
				break;
			case 's':
				boothowto |= RB_SINGLE;
				break;
			case 'd':
				boothowto |= RB_KDB;
				break;
			case 'c':
				boothowto |= RB_CONFIG;
				break;
			case 'v':
				console = "vga";
				break;
			case 'f':
				console = "fcom";
				break;
			default:
				break;
			}
		}
		while (*args == ' ')
			++args;
	}

#ifdef DEBUG
	/* XXX too early for console */
	printf("bootfile: %s\n", boot_file);
	printf("bootargs: %s\n", boot_args);
#endif
}

extern struct bus_space footbridge_pci_io_bs_tag;
extern struct bus_space footbridge_pci_mem_bs_tag;
void footbridge_pci_bs_tag_init(void);

void
consinit(void)
{
	static int consinit_called = 0;

	if (consinit_called != 0)
		return;

	consinit_called = 1;

#if NISA > 0
	/* Initialise the ISA subsystem early ... */
	isa_footbridge_init(DC21285_PCI_IO_VBASE, DC21285_PCI_ISA_MEM_VBASE);
#endif

	footbridge_pci_bs_tag_init();

	get_bootconf_option(boot_args, "console", BOOTOPT_TYPE_STRING,
	    &console);

	if (strncmp(console, "fcom", 4) == 0
	    || strncmp(console, "diag", 4) == 0) {
		fcomcnattach(DC21285_ARMCSR_VBASE, comcnspeed, comcnmode);
	}
#if (NVGA > 0)
	else if (strncmp(console, "vga", 3) == 0) {
		if (0 == vga_cnattach(&footbridge_pci_io_bs_tag,
		    &footbridge_pci_mem_bs_tag, - 1, 1)) {
#if (NPCKBC > 0)
		pckbc_cnattach(&isa_io_bs_tag, IO_KBD, KBCMDP, PCKBC_KBD_SLOT);
#endif	/* NPCKBC */
		} else {
			/* fall back to serial if no video present */
			fcomcnattach(DC21285_ARMCSR_VBASE, comcnspeed,
			    comcnmode);
		}
	}
#endif	/* NVGA */
#if (NCOM > 0)
	else if (strncmp(console, "com", 3) == 0) {
		if (comcnattach(&isa_io_bs_tag, CONCOMADDR, comcnspeed,
		    COM_FREQ, comcnmode))
			panic("can't init serial console @%x", CONCOMADDR);
	}
#endif
	/* Don't know what console was requested so use the fall back. */
	else
		fcomcnattach(DC21285_ARMCSR_VBASE, comcnspeed, comcnmode);
}

/* End of ebsa285_machdep.c */

/* $NetBSD: machdep.c,v 1.6 1996/03/13 21:32:39 mark Exp $ */

/*
 * Copyright (c) 1994-1996 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * machdep.c
 *
 * Machine dependant functions for kernel setup
 *
 * This file needs a lot of work. 
 *
 * Created      : 17/09/94
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/callout.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/buf.h>
#include <sys/map.h>
#include <sys/exec.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/device.h>
#include <sys/sysctl.h>
#include <sys/syscallargs.h>

#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm_kern.h>

#include <machine/signal.h>
#include <machine/frame.h>
#include <machine/bootconfig.h>
#include <machine/katelib.h>
#include <machine/cpu.h>
#include <machine/pte.h>
#include <machine/vidc.h>
#include <machine/iomd.h>
#include <machine/io.h>
#include <machine/irqhandler.h>
#include <machine/undefined.h>
#include <machine/rtc.h>

#include "hydrabus.h"

/* Describe different actions to take when boot() is called */

#define ACTION_HALT   0x01	/* Halt and boot */
#define ACTION_REBOOT 0x02	/* Halt and request RiscBSD reboot */
#define ACTION_KSHELL 0x04	/* Call kshell */
#define ACTION_DUMP   0x08	/* Dump the system to the dump dev */

#define HALT_ACTION	ACTION_HALT | ACTION_KSHELL	/* boot(RB_HALT) */
#define REBOOT_ACTION	ACTION_REBOOT			/* boot(0) */
#define PANIC_ACTION	ACTION_HALT | ACTION_KSHELL	/* panic() */

BootConfig bootconfig;		/* Boot config storage */
videomemory_t videomemory;	/* Video memory descriptor */

vm_offset_t physical_start;
vm_offset_t physical_freestart;
vm_offset_t physical_freeend;
vm_offset_t physical_end;
int physical_memoryblock;
u_int free_pages;
vm_offset_t pagetables_start;
int physmem = 0;

int debug_flags;
int max_processes;
int cpu_cache;
int cpu_ctrl;

u_int ramdisc_size;		/* Ramdisc size */

u_int kmodule_base;
u_int kmodule_size;

u_int videodram_size;		/* Amount of DRAM to reserve for video */
vm_offset_t videodram_start;

vm_offset_t physical_pt_start;
vm_offset_t virtual_pt_end;

u_int *cursor_data;		/* Will move to the vidc code */

typedef struct {
	vm_offset_t physical;
	vm_offset_t virtual;
} pv_addr_t;

pv_addr_t systempage;
pv_addr_t irqstack;
pv_addr_t undstack;
pv_addr_t abtstack;
pv_addr_t kernelstack;
#if NHYDRABUS > 0
pv_addr_t hydrascratch;
#endif

pt_entry_t kernel_pt_table[15];

/* the following is used externally (sysctl_hw) */
char machine[] = "arm32";	/* cpu "architecture" */

char *boot_args;

extern pt_entry_t msgbufpte;
int	msgbufmapped;
vm_offset_t msgbufphys;

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
extern u_int undefined_handler_address;

extern int pmap_debug_level;

#define KERNEL_PT_PAGEDIR   0
#define KERNEL_PT_PDE       1
#define KERNEL_PT_PTE       2
#define KERNEL_PT_VMEM      3
#define KERNEL_PT_SYS       4
#define KERNEL_PT_KERNEL    5
#define KERNEL_PT_VMDATA0   6
#define KERNEL_PT_VMDATA1   7
#define KERNEL_PT_VMDATA2   8
#define KERNEL_PT_VMDATA3   9
#define KERNEL_PT_VMDATA4  10
#define KERNEL_PT_VMDATA5  11
#define KERNEL_PT_VMDATA6  12
#define KERNEL_PT_VMDATA7  13
#define KERNEL_PT_VMDATA7  13
#define KERNEL_PT_KSTACK   14

struct user *proc0paddr;

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif

int cold = 1;

/* Prototypes */

void boot0	__P((void));
void bootsync	__P((void));

char *strstr	__P((char */*s1*/, char */*s2*/));

void physconputchar		__P((char));
void physcon_display_base	__P((u_int));
void consinit			__P((void));

void map_section	__P((vm_offset_t, vm_offset_t, vm_offset_t));
void map_pagetable	__P((vm_offset_t, vm_offset_t, vm_offset_t));
void map_entry		__P((vm_offset_t, vm_offset_t va, vm_offset_t));
void map_entry_ro	__P((vm_offset_t, vm_offset_t, vm_offset_t));

void pmap_bootstrap		__P((vm_offset_t /*kernel_l1pt*/));
void process_kernel_args	__P((void));
u_long strtoul			__P((const char */*s*/, char **/*ptr*/, int /*base*/));
caddr_t allocsys		__P((caddr_t /*v*/));
void identify_cpu		__P((void));
void data_abort_handler		__P((trapframe_t */*frame*/));
void prefetch_abort_handler	__P((trapframe_t */*frame*/));
void undefinedinstruction_bounce	__P((trapframe_t */*frame*/));
void set_boot_devs		__P((void));
void configure			__P((void));
void zero_page_readonly		__P((void));
void zero_page_readwrite	__P((void));
u_int disassemble		__P((u_int /*addr*/));
int setup_cursor		__P((void));
void init_fpe_state		__P((struct proc *));

void pmap_debug	__P((int /*level*/));
void dumpsys	__P((void));
void hydrastop	__P((void));

void vtbugreport __P((void));

/*
 * Debug function just to park the CPU
 *
 * This should be updated to power down an ARM7500
 */

void
halt()
{
	while (1);
}


/*
 * void boot(int howto)
 *
 * Reboots the system
 *
 * This gets called when a reboot is request by the user or after a panic.
 * Call boot0() will reboot the machine. For the moment we will try and be
 * clever and return to the booting environment. This may work if we
 * have be booted with the Kate boot loader as long as we have not messed
 * the system up to much. Until we have our own memory management running
 * this should work. The only use of being able to return (to RISC OS)
 * is so I don't have to wait while the machine reboots.
 */

/* NOTE: These variables will be removed, well some of them */

extern u_int spl_mask;
extern u_int current_mask;
struct pcb dumppcb;
extern u_int arm700bugcount;
extern int ioctlconsolebug;

void
boot(howto)
	int howto;
{
	int loop;
	int action;

/* Debugging here */

	if (curproc == NULL)
		printf("curproc = 0 - must have been in cpu_idle()\n");

/*	if (panicstr)
		printf("ioctlconsolebug=%d %08x\n", ioctlconsolebug, ioctlconsolebug);*/

/*	if (curpcb)
		printf("curpcb=%08x pcb_sp=%08x pcb_und_sp=%08x\n", curpcb, curpcb->pcb_sp, curpcb->pcb_und_sp);*/

#if NHYDRABUS > 0
/*
 * If we are halting the master then we should halt the slaves :-)
 * otherwise it can get a bit disconcerting to have 4 other
 * processor still tearing away doing things.
 */

	hydrastop();
#endif

/* Debug info */

	printf("boot: howto=%08x %08x curproc=%08x\n", howto, spl_mask, (u_int)curproc);

	printf("current_mask=%08x spl_mask=%08x\n", current_mask, spl_mask);
	printf("ipl_bio=%08x ipl_net=%08x ipl_tty=%08x ipl_clock=%08x ipl_imp=%08x\n",
            irqmasks[IPL_BIO], irqmasks[IPL_NET], irqmasks[IPL_TTY],
            irqmasks[IPL_CLOCK], irqmasks[IPL_IMP]);

	dump_spl_masks();

/*	vtbugreport();*/

/* Did we encounter the ARM700 bug we discovered ? */

	if (arm700bugcount > 0)
		printf("ARM700 PREFETCH/SWI bug count = %d\n", arm700bugcount);

/* Disable console buffering */

	cnpollc(1);

/* If we are still cold then hit the air brakes */

	if (cold) {
		printf("Halted while still in the ICE age.\n");
		printf("Hit a key to reboot\n");
		cngetc();
		boot0();
	}

/*
 * Depending on how we got here and with what intructions, choose
 * the actions to take. (See the actions defined above)
 */
 
	if (panicstr)
		action = PANIC_ACTION;
	else if (howto & RB_HALT)
		action = HALT_ACTION;
	else
		action = REBOOT_ACTION;

/*
 * If RB_NOSYNC was not specified sync the discs.
 * Note: Unless cold is set to 1 here, syslogd will die during the unmount.
 * It looks like syslogd is getting woken up only to find that it cannot
 * page part of the binary in as the filesystem has been unmounted.
 */


	if (!(howto & RB_NOSYNC)) {
		cold = 1;		/* no sleeping etc. */
		bootsync();
	}

/* Say NO to interrupts */

	splhigh();
	
#ifdef KSHELL

/* Now enter our crude debug shell if required. Soon to be replaced with DDB */

	if (action & ACTION_KSHELL)
		shell();
#else
	if (action & ACTION_KSHELL) {
		printf("Halted.\n");
		printf("Hit a key to reboot ");
		cngetc();
	}
#endif

/* Auto reboot overload protection */

/*
 * This code stops the kernel entering an endless loop of reboot - panic
 * cycles. This will only effect kernels that have been configured to
 * reboot on a panic and will have the effect of stopping further reboots
 * after it has rebooted 16 times after panics and clean halt or reboot
 * will reset the counter.
 */

/*
 * Have we done 16 reboots in a row ? If so halt rather than reboot
 * since 16 panics in a row without 1 clean halt means something is
 * seriously wrong
 */

	if (cmos_read(RTC_ADDR_REBOOTCNT) > 16)
		action = (action & ~ACTION_REBOOT) | ACTION_HALT;

/*
 * If we are rebooting on a panic then up the reboot count otherwise reset
 * This will thus be reset if the kernel changes the boot action from
 * reboot to halt due to too any reboots.
 */
 
	if ((action & ACTION_REBOOT) && panicstr)
		cmos_write(RTC_ADDR_REBOOTCNT,
		   cmos_read(RTC_ADDR_REBOOTCNT) + 1);
	else
		cmos_write(RTC_ADDR_REBOOTCNT, 0);

/*
 * If we need a RiscBSD reboot, request it but setting a bit in the CMOS RAM
 * This can be detected by the RiscBSD boot loader during a RISC OS boot
 * No other way to do this as RISC OS is in ROM.
 */

	if (action & ACTION_REBOOT)
		cmos_write(RTC_ADDR_BOOTOPTS,
		    cmos_read(RTC_ADDR_BOOTOPTS) | 0x02);

/* If we need to do a dump, do it */

	if ((howto & RB_DUMP) && (action & ACTION_DUMP)) {
		savectx(&dumppcb);
		dumpsys();
	}

/* Run any shutdown hooks */

	printf("Running shutdown hooks ...\n");
	doshutdownhooks();

/* Make sure IRQ's are disabled */

	IRQdisable;

/* Tell the user we are booting */

	printf("boot...");

/* Give the user time to read the last couple of lines of text. */

	for (loop = 5; loop > 0; --loop) {
		printf("%d..", loop);
		delay(500000);
	}

	boot0();
}


/* Sync the discs and unmount the filesystems */

void
bootsync(void)
{
	int iter;
	int nbusy;
	struct buf *bp;
	static int bootsyncdone = 0;

	if (bootsyncdone) return;

	bootsyncdone = 1;

/* Make sure we can still manage to do things */

	if (GetCPSR() & I32_bit) {
/*
 * If we get then boot has been called with out RB_NOSYNC and interrupts were
 * disabled. This means the boot() call did not come from a user process e.g.
 * shutdown, but must have come from somewhere in the kernel.
 */

		IRQenable;
		printf("Warning IRQ's disabled during boot()\n");
	}

	vfs_shutdown();
}


/*
 * Estimated loop for n microseconds
 */

/* Need to re-write this to use the timers */

/* One day soon I will actually do this */

void
delay(n)
	u_int n;
{
	u_int i;

	while (--n > 0)
		for (i = 8; --i;);
}


/*
 * u_int initarm(BootConfig *bootconf)
 *
 * Initial entry point on startup. This gets called before main() is
 * entered.
 * It should be responcible for setting up everything that must be
 * in place when main is called.
 * This includes
 *   Taking a copy of the boot configuration structure.
 *   Initialising the physical console so characters can be printed.
 *   Setting up page tables for the kernel
 *   Relocating the kernel to the bottom of physical memory
 */

/* This routine is frightening mess ! This is what my mind looks like -mark */

u_int
initarm(bootconf)
	BootConfig *bootconf;
{
	int loop;
	int loop1;
	u_int logical;
	u_int physical;
	u_int kerneldatasize;
	u_int l1pagetable;
	u_int l2pagetable;
	extern char page0[], page0_end[];
	struct exec *kernexec = (struct exec *)KERNEL_BASE;

/* Copy the boot configuration structure */

	bootconfig = *bootconf;

/*
 * Initialise the video memory descriptor
 *
 * This will change in the future to correctly report DRAM as well
 * but for the moment hardwire it. This will allow the console code
 * to use the structure now.
 *
 * Note: all references to the video memory virtual/physical address
 * should go via this structure.
 */

/*
 * In the future ...
 *
 * All console output will be postponed until the primary bootstrap
 * has been completed so that we have had a chance to reserve some
 * memory for the video system if we do not have separate VRAM.
 */
 
	videomemory.vidm_vbase = bootconfig.display_start;
	videomemory.vidm_pbase = VRAM_BASE;
	videomemory.vidm_type = VIDEOMEM_TYPE_VRAM;
	videomemory.vidm_size = bootconfig.display_size;

/*
 * Initialise the physical console
 * This is done in main() but for the moment we do it here so that
 * we can use printf in initarm() before main() has been called.
 */

	consinit();

/* Talk to the user */

	printf("initarm...\n");

	printf("Kernel loaded from file %s\n", bootconfig.kernelname);
	printf("Kernel arg string %s\n", (char *)bootconfig.argvirtualbase);

	printf("\nBoot configuration structure reports the following memory\n");

	printf("  DRAM block 0a at %08x size %08x  DRAM block 0b at %08x size %08x\n\r",
	    bootconfig.dram[0].address,
	    bootconfig.dram[0].pages * bootconfig.pagesize,
	    bootconfig.dram[1].address,
	    bootconfig.dram[1].pages * bootconfig.pagesize);
	printf("  DRAM block 1a at %08x size %08x  DRAM block 1b at %08x size %08x\n\r",
	    bootconfig.dram[2].address,
	    bootconfig.dram[2].pages * bootconfig.pagesize,
	    bootconfig.dram[3].address,
	    bootconfig.dram[3].pages * bootconfig.pagesize);
	printf("  VRAM block 0  at %08x size %08x\n\r",
	    bootconfig.vram[0].address,
	    bootconfig.vram[0].pages * bootconfig.pagesize);

	printf(" videomem = %08x %08x\n", bootconfig.display_start, videomemory.vidm_vbase);

/* Check to make sure the page size is correct */

	if (NBPG != bootconfig.pagesize)
		panic("Page size is not %d bytes\n", NBPG);

/*
 * Ok now we have the hard bit.
 * We have the kernel allocated up high. The rest of the memory map is
 * available. We are still running on RISC OS page tables.
 *
 * We need to construct new page tables move the kernel in physical
 * memory and switch to them.
 *
 * The booter will have left us 6 pages at the top of memory.
 * Two of these are used as L2 page tables and the other 4 form the L1
 * page table.
 */

/*
 * Ok we must construct own own page table tables.
 * Once we have these we can reorganise the memory as required
 */

/*
 * We better check to make sure the booter has set up the scratch
 * area for us correctly. We use this area to create temporary pagetables
 * while we reorganise the memory map.
 */

	if ((bootconfig.scratchphysicalbase & 0x3fff) != 0)
		panic("initarm: Scratch area not aligned on 16KB boundry\n");

	if ((bootconfig.scratchsize < 0xc000) != 0)
		panic("initarm: Scratch area too small (need >= 48KB)\n");

/*
 * Ok start the primary bootstrap.
 * The primary bootstrap basically replaces the booter page tables with
 * new ones that it creates in the boot scratch area. These page tables
 * map the rest of the physical memory into the virtaul memory map.
 * This allows low physical memory to be accessed to create the
 * kernels page tables, relocate the kernel code from high physical
 * memory to low physical memory etc.
 */

	printf("initarm: Primary bootstrap ... ");

/*
 * Update the videomemory structure to reflect the mapping changes
 */
 
	videomemory.vidm_vbase = VMEM_VBASE;
	videomemory.vidm_pbase = VRAM_BASE;
	videomemory.vidm_type = VIDEOMEM_TYPE_VRAM;
	videomemory.vidm_size = bootconfig.vram[0].pages * NBPG;

	kerneldatasize = bootconfig.kernsize + bootconfig.argsize;

	l2pagetable = bootconfig.scratchvirtualbase;
	l1pagetable = l2pagetable + 0x4000;

/*
 * Now we construct a L2 pagetables for the VRAM, the current kernel memory
 * and the new kernel memory
 */

	for (logical = 0; logical < 0x200000; logical += NBPG) {
		map_entry(l2pagetable + 0x1000, logical,
		    bootconfig.vram[0].address + logical);
		map_entry(l2pagetable + 0x1000, logical + 0x200000,
		    bootconfig.vram[0].address + logical);
	}

	for (logical = 0; logical < kerneldatasize + bootconfig.scratchsize;
	    logical += NBPG) {
		map_entry(l2pagetable + 0x3000, logical,
		    bootconfig.kernphysicalbase + logical);
	}

#if NHYDRABUS > 0
	for (logical = 0; logical < 0x200000; logical += NBPG) {
		map_entry(l2pagetable + 0x2000, logical,
		    bootconfig.dram[0].address + logical + NBPG);
	}
#else
	for (logical = 0; logical < 0x200000; logical += NBPG) {
		map_entry(l2pagetable + 0x2000, logical,
		    bootconfig.dram[0].address + logical);
	}
#endif

/*
 * Now we construct the L1 pagetable. This only needs the minimum to
 * keep us going until we can contruct the proper kernel L1 page table.
 */

	map_section(l1pagetable, VIDC_BASE,  VIDC_HW_BASE);
	map_section(l1pagetable, IOMD_BASE,  IOMD_HW_BASE);

	map_pagetable(l1pagetable, 0x00000000,
	    bootconfig.scratchphysicalbase + 0x2000);
	map_pagetable(l1pagetable, KERNEL_BASE,
	    bootconfig.scratchphysicalbase + 0x3000);
	map_pagetable(l1pagetable, VMEM_VBASE,
	    bootconfig.scratchphysicalbase + 0x1000);

/* Print some debugging info */

/*
	printf("page tables look like this ...\n");
	printf("V0x00000000 - %08x\n", ReadWord(l1pagetable + 0x0000));
	printf("V0x03500000 - %08x\n", ReadWord(l1pagetable + 0x00d4));
	printf("V0x00200000 - %08x\n", ReadWord(l1pagetable + 0x0080));
	printf("V0xf4000000 - %08x\n", ReadWord(l1pagetable + 0x3d00));
	printf("V0xf0000000 - %08x\n", ReadWord(l1pagetable + 0x3c00));
	printf("page dir = P%08x\n", bootconfig.scratchphysicalbase + 0x4000);
	printf("l1= V%08x\n", l1pagetable);
*/

/*
 * Pheww right we are ready to switch page tables !!!
 * The L1 table is at bootconfig.scratchphysicalbase + 0x4000
 */
 
/* Switch tables */

	setttb(bootconfig.scratchphysicalbase + 0x4000);

/* Since we have mapped the VRAM up into kernel space we must now update the
 * the bootconfig and display structures by hand.
 */

	bootconfig.display_start = VMEM_VBASE;
	physcon_display_base(VMEM_VBASE);

	printf("done.\n");

/*
 * Ok we have finished the primary boot strap. All this has done is to
 * allow us to access all the physical memory from known virtual
 * location. We also now know that all the used pages are at the top
 * of the physical memory and where they are in the virtual memory map.
 *
 * This should be the stage we are at at the end of the bootstrap when
 * we have a two stage booter.
 *
 * The secondary bootstrap has the responcibility to sort locating the
 * kernel to the correct address and for creating the kernel page tables.
 * It must also set up various memory pointers that are used by pmap etc.  
 */

	process_kernel_args();

	printf("initarm: Secondary bootstrap ... ");

/* Zero down the memory we mapped in for the secondary bootstrap */

	bzero(0x00000000, 0x200000);

/* Set up the variables that define the availablilty of physcial memory */

	physical_start = bootconfig.dram[0].address;
	physical_freestart = physical_start;
	physical_end = bootconfig.dram[bootconfig.dramblocks - 1].address
	    + bootconfig.dram[bootconfig.dramblocks - 1].pages * NBPG;
	physical_freeend = physical_end;
	physical_memoryblock = 0;
	free_pages = bootconfig.drampages;
    
	for (loop = 0; loop < bootconfig.dramblocks; ++loop)
		physmem += bootconfig.dram[loop].pages;
    
/*
 * Reserve some pages at the top of the memory for later use
 *
 * This area is not currently used but could be used for the allocation
 * of L1 page tables for each process.
 * The size of this memory would be determined by the maximum number of
 * processes.
 *
 * For the moment we just reserve a few pages just to make sure the
 * system copes.
 */

	physical_freeend -= videodram_size;
	free_pages -= (videodram_size / NBPG);
	videodram_start = physical_freeend;

	physical_freeend -= PD_SIZE * max_processes;
	free_pages -= 4 * max_processes;
	pagetables_start = physical_freeend;

/* Right We have the bottom meg of memory mapped to 0x00000000
 * so was can get at it. The kernel will ocupy the start of it.
 * After the kernel/args we allocate some the the fixed page tables
 * we need to get the system going.
 * We allocate one page directory and 8 page tables and store the
 * physical addresses in the kernel_pt_table array.
 * Must remember that neither the page L1 or L2 page tables are the same
 * size as a page !
 *
 * Ok the next bit of physical allocate may look complex but it is
 * simple really. I have done it like this so that no memory gets wasted
 * during the allocate of various pages and tables that are all different
 * sizes.
 * The start address will be page aligned.
 * We allocate the kernel page directory on the first free 16KB boundry
 * we find.
 * We allocate the kernel page tables on the first 1KB boundry we find.
 * We allocate 9 PT's. This means that in the process we
 * KNOW that we will encounter at least 1 16KB boundry.
 *
 * Eventually if the top end of the memory gets used for process L1 page
 * tables the kernel L1 page table may be moved up there.
 */

/*
 * The Simtec Hydra board needs a 2MB aligned page for bootstrapping.
 * Simplest thing is to nick the bottom page of physical memory.
 */

#if NHYDRABUS > 0
	hydrascratch.physical = physical_start;
	physical_start += NBPG;
	--free_pages;
#endif

	physical = physical_start + kerneldatasize;
/*	printf("physical=%08x next_phys=%08x\n", physical, pmap_next_phys_page(physical - NBPG));*/
	loop1 = 1;
	kernel_pt_table[0] = 0;
	for (loop = 0; loop < 15; ++loop) {
		if ((physical & (PD_SIZE-1)) == 0 && kernel_pt_table[0] == 0) {
			kernel_pt_table[KERNEL_PT_PAGEDIR] = physical;
			bzero((char *)physical - physical_start, PD_SIZE);
			physical += PD_SIZE; 
		} else {
			kernel_pt_table[loop1] = physical;
			bzero((char *)physical - physical_start, PT_SIZE);
			physical += PT_SIZE;
			++loop1;
		}
	}

/* A bit of debugging info */

/*
	for (loop=0; loop < 10; ++loop)
		printf("%d - P%08x\n", loop, kernel_pt_table[loop]);
*/

/* This should never be able to happen but better confirm that. */

	if ((kernel_pt_table[0] & (PD_SIZE-1)) != 0)
		panic("initarm: Failed to align the kernel page directory\n");

/* Update the address of the first free page of physical memory */

	physical_freestart = physical;
/*	printf("physical_fs=%08x next_phys=%08x\n", (u_int)physical_freestart, (u_int)pmap_next_phys_page(physical_freestart - NBPG));*/
	free_pages -= (physical - physical_start) / NBPG;

/* Allocate a page for the system page mapped to 0x00000000 */

	systempage.physical = physical_freestart;
	physical_freestart += NBPG;
/*	printf("(0)physical_fs=%08x next_phys=%08x\n", (u_int)physical_freestart, (u_int)pmap_next_phys_page(physical_freestart - NBPG));*/
	--free_pages;
	bzero((char *)systempage.physical - physical_start, NBPG);

/* Allocate another 3 pages for the stacks in different CPU modes. */

	irqstack.physical = physical_freestart;
	physical_freestart += NBPG;
	abtstack.physical = physical_freestart;
	physical_freestart += NBPG;
	undstack.physical = physical_freestart;
	physical_freestart += NBPG;
	bzero((char *)irqstack.physical - physical_start, 3*NBPG);
	free_pages -= 3;
	irqstack.virtual = KERNEL_BASE + irqstack.physical-physical_start;
	abtstack.virtual = KERNEL_BASE + abtstack.physical-physical_start;
	undstack.virtual = KERNEL_BASE + undstack.physical-physical_start;
/*	printf("(1)physical_fs=%08x next_phys=%08x\n", (u_int)physical_freestart, (u_int)pmap_next_phys_page(physical_freestart - NBPG));*/

	kernelstack.physical = physical_freestart;
	physical_freestart += UPAGES * NBPG;
	bzero((char *)kernelstack.physical - physical_start, UPAGES * NBPG);
	free_pages -= UPAGES;

/*	printf("(2)physical_fs=%08x next_phys=%08x\n", (u_int)physical_freestart, (u_int)pmap_next_phys_page(physical_freestart - NBPG));*/


	kernelstack.virtual = KERNEL_BASE + kernelstack.physical
	    - physical_start;

	msgbufphys = physical_freestart;
	physical_freestart += round_page(sizeof(struct msgbuf));
	free_pages -= round_page(sizeof(struct msgbuf)) / NBPG;

/*	printf("physical_fs=%08x next_phys=%08x\n", (u_int)physical_freestart, (u_int)pmap_next_phys_page(physical_freestart - NBPG));*/

/* Ok we have allocated physical pages for the primary kernel page tables */

/* Now we fill in the L2 pagetable for the kernel code/data */

	l2pagetable = kernel_pt_table[KERNEL_PT_KERNEL] - physical_start;

	if (N_GETMAGIC(kernexec[0]) == ZMAGIC) {
/*		printf("[ktext read-only] ");
		printf("[%08x %08x %08x] \n", (u_int)kerneldatasize, (u_int)kernexec->a_text,
		    (u_int)(kernexec->a_text+kernexec->a_data+kernexec->a_bss));*/
/*		printf("physical start=%08x physical freestart=%08x hydra phys=%08x\n", physical_start, physical_freestart, hydrascratch.physical);*/
		for (logical = 0; logical < 0x00/*kernexec->a_text*/;
		    logical += NBPG)
			map_entry_ro(l2pagetable, logical, physical_start
			    + logical);
		for (; logical < kerneldatasize; logical += NBPG)
			map_entry(l2pagetable, logical, physical_start
			    + logical);
	} else
		for (logical = 0; logical < kerneldatasize; logical += NBPG)
			map_entry(l2pagetable, logical, physical_start
			    + logical);

/* Map the stack pages */

	map_entry(l2pagetable, irqstack.physical-physical_start,
	    irqstack.physical);
	map_entry(l2pagetable, abtstack.physical-physical_start,
	    abtstack.physical); 
	map_entry(l2pagetable, undstack.physical-physical_start,
	    undstack.physical); 
	map_entry(l2pagetable, kernelstack.physical - physical_start,
	    kernelstack.physical); 
	map_entry(l2pagetable, kernelstack.physical + NBPG - physical_start,
	    kernelstack.physical + NBPG); 

	l2pagetable = kernel_pt_table[KERNEL_PT_KSTACK] - physical_start;

	map_entry(l2pagetable, 0x003fe000, kernelstack.physical);
	map_entry(l2pagetable, 0x003ff000, kernelstack.physical + NBPG);

/* Now we fill in the L2 pagetable for the VRAM */

/*
 * Current architectures mean that the VRAM is always in 1 continuous
 * bank.
 * This means that we can just map the 2 meg that the VRAM would occupy.
 * In theory we don't need a page table for VRAM, we could section map
 * it but we would need the page tables if DRAM was in use.
 */

	l2pagetable = kernel_pt_table[KERNEL_PT_VMEM] - physical_start;

	for (logical = 0; logical < 0x200000; logical += NBPG) {
		map_entry(l2pagetable, logical, bootconfig.vram[0].address
		    + logical);
		map_entry(l2pagetable, logical + 0x200000,
		    bootconfig.vram[0].address + logical);
	}

/* Map entries in the page table used to map PDE's */

	l2pagetable = kernel_pt_table[KERNEL_PT_PDE] - physical_start;
	map_entry(l2pagetable, 0x0000000,
	    kernel_pt_table[KERNEL_PT_PAGEDIR]);
	map_entry(l2pagetable, 0x0001000,
	    kernel_pt_table[KERNEL_PT_PAGEDIR] + 0x1000);
	map_entry(l2pagetable, 0x0002000,
	    kernel_pt_table[KERNEL_PT_PAGEDIR] + 0x2000);
	map_entry(l2pagetable, 0x0003000,
	    kernel_pt_table[KERNEL_PT_PAGEDIR] + 0x3000);

/*
 * Map entries in the page table used to map PTE's
 * Basically every kernel page table gets mapped here
 */

	l2pagetable = kernel_pt_table[KERNEL_PT_PTE] - physical_start;
	map_entry(l2pagetable, (KERNEL_BASE >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_KERNEL]);
	map_entry(l2pagetable, (PAGE_DIRS_BASE >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_PDE]);
	map_entry(l2pagetable, (PROCESS_PAGE_TBLS_BASE >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_PTE]);
	map_entry(l2pagetable, (VMEM_VBASE >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_VMEM]);
	map_entry(l2pagetable, (0x00000000 >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_SYS]);
	map_entry(l2pagetable, ((KERNEL_VM_BASE + 0x00000000) >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_VMDATA0]);
	map_entry(l2pagetable, ((KERNEL_VM_BASE + 0x00400000) >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_VMDATA1]);
	map_entry(l2pagetable, ((KERNEL_VM_BASE + 0x00800000) >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_VMDATA2]);
	map_entry(l2pagetable, ((KERNEL_VM_BASE + 0x00c00000) >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_VMDATA3]);
	map_entry(l2pagetable, ((KERNEL_VM_BASE + 0x01000000) >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_VMDATA4]);
	map_entry(l2pagetable, ((KERNEL_VM_BASE + 0x01400000) >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_VMDATA5]);
	map_entry(l2pagetable, ((KERNEL_VM_BASE + 0x01800000) >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_VMDATA6]);
	map_entry(l2pagetable, ((KERNEL_VM_BASE + 0x01c00000) >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_VMDATA7]);
	map_entry(l2pagetable, ((0xef800000) >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_KSTACK]);

	map_entry(l2pagetable, (0xf5000000 >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_PAGEDIR] + 0x0000);
	map_entry(l2pagetable, (0xf5400000 >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_PAGEDIR] + 0x1000);
	map_entry(l2pagetable, (0xf5800000 >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_PAGEDIR] + 0x2000);
	map_entry(l2pagetable, (0xf5c00000 >> (PGSHIFT-2)),
	    kernel_pt_table[KERNEL_PT_PAGEDIR] + 0x3000);

/*
 * Map the system page in the kernel page table for the bottom 1Meg
 * of the virtual memory map.
 */

	l2pagetable = kernel_pt_table[KERNEL_PT_SYS] - physical_start;
	map_entry(l2pagetable, 0x0000000, systempage.physical);

/* Now we construct the L1 pagetable */

	l1pagetable = kernel_pt_table[KERNEL_PT_PAGEDIR] - physical_start;

/* Map the VIDC20, IOMD, COMBO and podules */

/* Map the VIDC20 */

	map_section(l1pagetable, VIDC_BASE, VIDC_HW_BASE);

/* Map the IOMD (and SLOW and MEDIUM simple podules) */

	map_section(l1pagetable, IOMD_BASE, IOMD_HW_BASE);

/* Map the COMBO (and module space) */

	map_section(l1pagetable, IO_BASE, IO_HW_BASE);

/* Map the L2 pages tables in the L1 page table */

	map_pagetable(l1pagetable, 0x00000000,
	    kernel_pt_table[KERNEL_PT_SYS]);
	map_pagetable(l1pagetable, 0xef800000,
	    kernel_pt_table[KERNEL_PT_KSTACK]);
	map_pagetable(l1pagetable, KERNEL_BASE,
	    kernel_pt_table[KERNEL_PT_KERNEL]);
	map_pagetable(l1pagetable, KERNEL_VM_BASE + 0x00000000,
	    kernel_pt_table[KERNEL_PT_VMDATA0]);
	map_pagetable(l1pagetable, KERNEL_VM_BASE + 0x00400000,
	    kernel_pt_table[KERNEL_PT_VMDATA1]);
	map_pagetable(l1pagetable, KERNEL_VM_BASE + 0x00800000,
	    kernel_pt_table[KERNEL_PT_VMDATA2]);
	map_pagetable(l1pagetable, KERNEL_VM_BASE + 0x00c00000,
	    kernel_pt_table[KERNEL_PT_VMDATA3]);
	map_pagetable(l1pagetable, KERNEL_VM_BASE + 0x01000000,
	    kernel_pt_table[KERNEL_PT_VMDATA4]);
	map_pagetable(l1pagetable, KERNEL_VM_BASE + 0x01400000,
	    kernel_pt_table[KERNEL_PT_VMDATA5]);
	map_pagetable(l1pagetable, KERNEL_VM_BASE + 0x01800000,
	    kernel_pt_table[KERNEL_PT_VMDATA6]);
	map_pagetable(l1pagetable, KERNEL_VM_BASE + 0x01c00000,
	    kernel_pt_table[KERNEL_PT_VMDATA7]);
	map_pagetable(l1pagetable, PAGE_DIRS_BASE,
	    kernel_pt_table[KERNEL_PT_PDE]);
	map_pagetable(l1pagetable, PROCESS_PAGE_TBLS_BASE,
	    kernel_pt_table[KERNEL_PT_PTE]);
	map_pagetable(l1pagetable, VMEM_VBASE,
	    kernel_pt_table[KERNEL_PT_VMEM]);

/* Bit more debugging info */

/*
	printf("page tables look like this ...\n");
	printf("V0x00000000 - %08x\n", ReadWord(l1pagetable + 0x0000));
	printf("V0x03200000 - %08x\n", ReadWord(l1pagetable + 0x00c8));
	printf("V0x03500000 - %08x\n", ReadWord(l1pagetable + 0x00d4));
	printf("V0xf0000000 - %08x\n", ReadWord(l1pagetable + 0x3c00));
	printf("V0xf1000000 - %08x\n", ReadWord(l1pagetable + 0x3c40));
	printf("V0xf2000000 - %08x\n", ReadWord(l1pagetable + 0x3c80));
	printf("V0xf3000000 - %08x\n", ReadWord(l1pagetable + 0x3cc0));
	printf("V0xf3300000 - %08x\n", ReadWord(l1pagetable + 0x3ccc));
	printf("V0xf4000000 - %08x\n", ReadWord(l1pagetable + 0x3d00));
	printf("V0xf6000000 - %08x\n", ReadWord(l1pagetable + 0x3d80));
*/
/*	printf("V0xefc00000 - %08x\n", ReadWord(l1pagetable + 0x3bf8));
	printf("V0xef800000 - %08x\n", ReadWord(l1pagetable + 0x3bfc));*/

/*
 * Now we have the real page tables in place so we can switch to them.
 * Once this is done we will be running with the REAL kernel page tables.
 */

/*
 * The last thing we must do is copy the kernel down to the new memory.
 * This copies all our kernel data structures and variables as well
 * which is why it is left to the last moment.
 */

	printf("mapping ... ");

	bcopy((char *)KERNEL_BASE, (char *)0x00000000, kerneldatasize);

/* Switch tables */

	setttb(kernel_pt_table[KERNEL_PT_PAGEDIR]);

	printf("done.\n");

/* Right set up the vectors at the bottom of page 0 */

	bcopy(page0, (char *)0x00000000, page0_end - page0);

/*
 * Pages were allocated during the secondary bootstrap for the
 * stacks for different CPU modes.
 * We must now set the r13 registers in the different CPU modes to
 * point to these stacks.
 * Since the ARM stacks use STMFD etc. we must set r13 to the top end
 * of the stack memory.
 */

#ifdef DIAGNOSTIC
	printf("IRQ stack V%08x P%08x\n", (u_int) irqstack.virtual,
	    (u_int) irqstack.physical);
	printf("ABT stack V%08x P%08x\n", (u_int) abtstack.virtual,
	    (u_int) abtstack.physical);
	printf("UND stack V%08x P%08x\n", (u_int) undstack.virtual,
	    (u_int) undstack.physical);
#endif

	printf("init subsystems: stacks ");

	set_stackptr(PSR_IRQ32_MODE, irqstack.virtual + NBPG);
	set_stackptr(PSR_ABT32_MODE, abtstack.virtual + NBPG);
	set_stackptr(PSR_UND32_MODE, undstack.virtual + NBPG);

	if (pmap_debug_level >= 0)
		printf("kstack V%08x P%08x\n", (int) kernelstack.virtual,
		    (int) kernelstack.physical);

/*
 * Well we should set a data abort handler.
 * Once things get going this will change as we will need a proper handler.
 * Until then we will use a handler that just panics but tells us
 * why.
 * Initialisation of the vectors will just panic on a data abort.
 * This just fills in a slighly better one.
 */

	printf("vectors ");
	data_abort_handler_address = (u_int)data_abort_handler;
	prefetch_abort_handler_address = (u_int)prefetch_abort_handler;
	undefined_handler_address = (u_int)undefinedinstruction_bounce;

/* Diagnostic stuff. while writing the boot code */

/*
	for (loop = 0x0; loop < 0x1000; ++loop) {
		if (ReadWord(PAGE_DIRS_BASE + loop * 4) != 0)
			printf("Pagetable for V%08x = %08x\n", loop << 20,
			    ReadWord(0xf2000000 + loop * 4));
	}

*/
 
/* Diagnostic stuff. while writing the boot code */

/*
	for (loop = 0x0; loop < 0x400; ++loop) {
		if (ReadWord(kernel_pt_table[KERNEL_PT_PTE] + loop * 4) != 0)
			printf("Pagetable for V%08x P%08x = %08x\n",
			    loop << 22, kernel_pt_table[KERNEL_PT_PTE]+loop*4,
			    ReadWord(kernel_pt_table[KERNEL_PT_PTE]+loop * 4));
	}
*/

/* At last !
 * We now have the kernel in physical memory from the bottom upwards.
 * Kernel page tables are physically above this.
 * The kernel is mapped to 0xf0000000
 * The kernel data PTs will handle the mapping of 0xf1000000-0xf1ffffff
 * 2Meg of VRAM is mapped to 0xf4000000
 * The kernel page directory is mapped to 0xf3000000
 * The page tables are mapped to 0xefc00000
 * The IOMD is mapped to 0xf6000000
 * The VIDC is mapped to 0xf6100000
 */

/* Initialise the undefined instruction handlers */

	printf("undefined ");
	undefined_init();

/* Boot strap pmap telling it where the kernel page table is */

	printf("pmap ");
	pmap_bootstrap(PAGE_DIRS_BASE);

/* Setup the IRQ system */

	printf("irq ");
	irq_init();
	printf("done.\n");

#ifdef DDB
	printf("ddb: ");
	db_machine_init();
	ddb_init();

	if (boothowto & RB_KDB)
		Debugger();
#endif

/* We return the new stack pointer address */
	return(kernelstack.virtual + USPACE_SVC_STACK_TOP);
}


/*
 * void cpu_startup(void)
 *
 * Machine dependant startup code. 
 *
 */

void
cpu_startup()
{
	int loop;
	vm_offset_t minaddr;
	vm_offset_t maxaddr;
	caddr_t sysbase;
	caddr_t size;
	vm_size_t bufsize;
	int base, residual;

/* Set the cpu control register */

	cpu_ctrl = CPU_CONTROL_MMU_ENABLE | CPU_CONTROL_32BP_ENABLE
		   | CPU_CONTROL_32BD_ENABLE | CPU_CONTROL_SYST_ENABLE;
                                        
	if (cpu_cache & 1)
		cpu_ctrl |= CPU_CONTROL_IDC_ENABLE;
	if (cpu_cache & 2)
		cpu_ctrl |= CPU_CONTROL_WBUF_ENABLE;

	if (!(cpu_cache & 4))
		cpu_ctrl |= CPU_CONTROL_CPCLK;

#ifdef CPU_LATE_ABORT
	cpu_ctrl |= CPU_CONTROL_LABT_ENABLE;
#endif

/* Clear out the cache */

	idcflush();
    
	cpu_control(cpu_ctrl);

/* All domains MUST be clients, permissions are VERY important */

	cpu_domains(DOMAIN_CLIENT);

/* Lock down zero page */

	zero_page_readonly();

/*
 * Initialize error message buffer (at end of core).
 */

/* msgbufphys was setup during the secondary boot strap */

	for (loop = 0; loop < btoc(sizeof(struct msgbuf)); ++loop)
		pmap_enter(pmap_kernel(),
		    (vm_offset_t)((caddr_t)msgbufp + loop * NBPG),
		    msgbufphys + loop * NBPG, VM_PROT_ALL, TRUE);

	msgbufmapped = 1;

/*
 * Identify ourselves for the msgbuf (everything printed earlier will
 * not be buffered).
 */
 
	printf(version);

	printf("screen: %d x %d x %d\n", bootconfig.width + 1, bootconfig.height + 1, bootconfig.framerate);

	if (cmos_read(RTC_ADDR_REBOOTCNT) > 0)
		printf("Warning: REBOOTCNT = %d\n", cmos_read(RTC_ADDR_REBOOTCNT));

	printf("real mem = %d (%d pages)\n", arm_page_to_byte(physmem), physmem);

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
                                    
	size = allocsys((caddr_t)0);
	sysbase = (caddr_t)kmem_alloc(kernel_map, round_page(size));
	if (sysbase == 0)
		panic("cpu_startup: no room for system tables %d bytes required", size);
	if ((caddr_t)((allocsys(sysbase) - sysbase)) != size)
		panic("cpu_startup: system table size inconsistency");

/*
 * Now allocate buffers proper.  They are different than the above
 * in that they usually occupy more virtual memory than physical.
 */

	bufsize = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
				   &maxaddr, bufsize, TRUE);
	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(bufsize),
	    (vm_offset_t)0, &minaddr, bufsize, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");

	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}

	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (loop = 0; loop < nbuf; ++loop) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;

/*
 * First <residual> buffers get (base+1) physical pages
 * allocated for them.  The rest get (base) physical pages.
 *
 * The rest of each buffer occupies virtual space,
 * but has no physical memory allocated for it.
 */

		curbuf = (vm_offset_t)buffers + loop * MAXBSIZE;
		curbufsize = CLBYTES * (loop < residual ? base+1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}

/*
 * Allocate a submap for exec arguments.  This map effectively
 * limits the number of processes exec'ing at any time.
 */

	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16*NCARGS, TRUE);

/*
 * Allocate a submap for physio
 */

	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);

/*
 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
 * we use the more space efficient malloc in place of kmem_alloc.
 */

	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
				   M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE);

/*
    printf("mb_buf: map=%08x maxaddr = %08x mbutl = %08x\n", mb_map, maxaddr, mbutl);
*/

/*
 * Initialise callouts
 */

	callfree = callout;

	for (loop = 1; loop < ncallout; ++loop)
		callout[loop - 1].c_next = &callout[loop];

	printf("avail mem = %d (%d pages)\n", (int)ptoa(cnt.v_free_count),
	    (int)ptoa(cnt.v_free_count) / NBPG);
	printf("using %d buffers containing %d bytes of memory\n",
	    nbuf, bufpages * CLBYTES);

/*
 * Set up buffers, so they can be used to read disk labels.
 */

	bufinit();

	proc0paddr = (struct user *)kernelstack.virtual;
	proc0.p_addr = proc0paddr;

	curpcb = &proc0.p_addr->u_pcb;
	curpcb->pcb_flags = 0;
	curpcb->pcb_und_sp = (u_int)proc0.p_addr + USPACE_UNDEF_STACK_TOP;
	curpcb->pcb_sp = (u_int)proc0.p_addr + USPACE_SVC_STACK_TOP;
	curpcb->pcb_pagedir = (pd_entry_t *)pmap_extract(kernel_pmap,
	    (vm_offset_t)(kernel_pmap)->pm_pdir);
	    
	proc0.p_md.md_regs = (struct trapframe *)curpcb->pcb_sp - 1;
#if 0
/* Hack proc0 */
                                 
	proc0paddr = (struct user *)kernelstack.virtual;
	proc0.p_addr = proc0paddr;

	curpcb = &proc0.p_addr->u_pcb;
	proc0.p_addr->u_pcb.pcb_flags = 0;
	proc0.p_addr->u_pcb.pcb_und_sp = (u_int)proc0.p_addr + USPACE_UNDEF_STACK_TOP;

	proc0.p_addr->u_pcb.pcb_pagedir = (pd_entry_t *)pmap_extract(kernel_pmap,
	    (vm_offset_t)(kernel_pmap)->pm_pdir);
#endif
/*
 * Install an IRQ handler on the VSYNC interrupt to reboot if the
 * middle mouse button is pressed.
 */

	setup_cursor();

/* Allocate memory for the kmodule area if required */

	if (kmodule_size) {
		kmodule_size = round_page(kmodule_size);
		kmodule_base = (u_int)kmem_alloc(kernel_map, kmodule_size);
		if (kmodule_base)
			printf("KMODULE SPACE = %08x\n", kmodule_base);
	        else
			printf("\x1b[31mNO KMODULE SPACE\n\x1b[0m");
	}

/*
 * Configure the hardware
 */
 
	configure();

/* Set the root, swap and dump devices from the boot args */

	set_boot_devs();

	dump_spl_masks();

	cold = 0;	/* We are warm now ... */
}


/*
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * We call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */

caddr_t
allocsys(v)
	register caddr_t v;
{

#define valloc(name, type, num) \
    (caddr_t)(name) = (type *)v; \
    v = (caddr_t)((name) + (num));

    valloc(callout, struct callout, ncallout);
    valloc(swapmap, struct map, nswapmap = maxproc * 2);

#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif
                                                                         
/*
 * Determine how many buffers to allocate.  We use 10% of the
 * first 2MB of memory, and 5% of the rest, with a minimum of 16
 * buffers.  We allocate 1/2 as many swap buffer headers as file
 * i/o buffers.
 */

	if (bufpages == 0)
		if (physmem < arm_byte_to_page(2 * 1024 * 1024))
			bufpages = physmem / (10 * CLSIZE);
		else
			bufpages = (arm_byte_to_page(2 * 1024 * 1024)
			         + physmem) / (20 * CLSIZE);

#ifdef DIAGNOSTIC
	if (bufpages == 0)
		panic("bufpages = 0\n");
#endif

	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) & ~1;       /* force even */
		if (nswbuf > 256)
			nswbuf = 256;           /* sanity */
	}

	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);

	return(v);
}


/* A few functions that are used to help construct the page tables
 * during the bootstrap process.
 */

void
map_section(pagetable, va, pa)
	vm_offset_t pagetable;
	vm_offset_t va;
	vm_offset_t pa; 
{
	if ((va & 0xfffff) != 0)
		panic("initarm: Cannot allocate 1MB section on non 1MB boundry\n");

	((u_int *)pagetable)[(va >> 20)] = L1_SEC((pa & PD_MASK));
}


void
map_pagetable(pagetable, va, pa)
	vm_offset_t pagetable;
	vm_offset_t va;
	vm_offset_t pa;
{
	if ((pa & 0xc00) != 0)
		panic("pagetables should be group allocated on pageboundry");
	((u_int *)pagetable)[(va >> 20) + 0] = L1_PTE((pa & PG_FRAME) + 0x000);
	((u_int *)pagetable)[(va >> 20) + 1] = L1_PTE((pa & PG_FRAME) + 0x400);
	((u_int *)pagetable)[(va >> 20) + 2] = L1_PTE((pa & PG_FRAME) + 0x800);
	((u_int *)pagetable)[(va >> 20) + 3] = L1_PTE((pa & PG_FRAME) + 0xc00);
}


void
map_entry(pagetable, va, pa)
	vm_offset_t pagetable;
	vm_offset_t va;
	vm_offset_t pa;
{
	WriteWord(pagetable + ((va >> 10) & 0x00000ffc),
	    L2_PTE((pa & PG_FRAME), AP_KRW));
}


void
map_entry_nc(pagetable, va, pa)
	vm_offset_t pagetable;
	vm_offset_t va;
	vm_offset_t pa;
{
	WriteWord(pagetable + ((va >> 10) & 0x00000ffc),
	    L2_PTE_NC((pa & PG_FRAME), AP_KRW));
}


void
map_entry_ro(pagetable, va, pa)
	vm_offset_t pagetable;
	vm_offset_t va;
	vm_offset_t pa;
{
	WriteWord(pagetable + ((va >> 10) & 0x00000ffc),
	    L2_PTE((pa & PG_FRAME), AP_KR));
}


int wdresethack = 1;

void
process_kernel_args()
{
	char *ptr;
	char *args;

/* Ok now we will check the arguments for interesting parameters. */

	args = (char *)bootconfig.argvirtualbase;
	max_processes = 64;
	boothowto = 0;
	kmodule_size = 0;
	cpu_cache = 0x03;
	debug_flags = 0;
	videodram_size = 0;
    
/* Skip the first parameter (the boot loader filename) */

	while (*args != ' ' && *args != 0)
		++args;

	while (*args == ' ')
		++args;

/* Skip the kernel image filename */

	while (*args != ' ' && *args != 0)
		++args;

	while (*args == ' ')
		++args;

	boot_args = NULL;

	if (*args != 0) {
		boot_args = args;

		if (strstr(args, "nocache"))
			cpu_cache &= ~1;

		if (strstr(args, "nowritebuf"))
			cpu_cache &= ~2;

		if (strstr(args, "fpaclk2"))
			cpu_cache |= 4;

		ptr = strstr(args, "maxproc=");
		if (ptr) {
			max_processes = (int)strtoul(ptr + 8, NULL, 10);
			if (max_processes < 16)
				max_processes = 16;
			if (max_processes > 256)
				max_processes = 256;
			printf("Maximum \"in memory\" processes = %d\n",
			    max_processes);
		}
		ptr = strstr(args, "ramdisc=");
		if (ptr) {
			ramdisc_size = (u_int)strtoul(ptr + 8, NULL, 10);
			ramdisc_size *= 1024;
			if (ramdisc_size < 32*1024)
				ramdisc_size = 32*1024;
			if (ramdisc_size > 2048*1024)
				ramdisc_size = 2048*1024;
		}
		ptr = strstr(args, "kmodule=");
		if (ptr) {
			kmodule_size = (u_int)strtoul(ptr + 8, NULL, 10);
			kmodule_size *= 1024;
			if (kmodule_size < 4*1024)
				kmodule_size = 4*1024;
			if (kmodule_size > 256*1024)
				kmodule_size = 256*1024;
		}
		ptr = strstr(args, "videodram=");
		if (ptr) {
			videodram_size = (u_int)strtoul(ptr + 10, NULL, 10);
			/* Round to 4K page */
			videodram_size *= 1024;
			videodram_size = round_page(videodram_size);
			if (videodram_size > 1024*1024)
				videodram_size = 1024*1024;
			printf("VIDEO DRAM = %d\n", videodram_size);
		}
		if (strstr(args, "single"))
			boothowto |= RB_SINGLE;
		if (strstr(args, "kdb"))
			boothowto |= RB_KDB;
		ptr = strstr(args, "pmapdebug=");
		if (ptr) {
			pmap_debug_level = (int)strtoul(ptr + 10, NULL, 10);
			pmap_debug(pmap_debug_level);
			debug_flags |= 0x01;
		}
		if (strstr(args, "termdebug"))
			debug_flags |= 0x02;
		if (strstr(args, "nowdreset"))
			wdresethack = 0;
		if (strstr(args, "notermcls"))
			debug_flags |= 0x04;
	}
}

/* This should happen in the console code - This really must move soon */

int
setup_cursor()
{

/* The cursor currently gets set up here. slightly wasteful on memory */

/*
 * This should be done in the vidc code as that is responcible for the cursor.
 * This will probably happen when the vidc code is separated from the console
 * (currently work in progress)
 */

	cursor_data = (u_int *)kmem_alloc(kernel_map, NBPG);
/*	printf("Cursor data page = V%08x P%08x\n", cursor_data, pmap_extract(kernel_pmap, (vm_offset_t)cursor_data));*/
	WriteWord(IOMD_CURSINIT, pmap_extract(kernel_pmap,
		(vm_offset_t)cursor_data));
	return(0);
}


/*
 * Clear registers on exec
 */

void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	register struct trapframe *tf;

	if (pmap_debug_level >= -1)
		printf("setregs: ip=%08x sp=%08x proc=%08x\n",
		    (u_int) pack->ep_entry, (u_int) stack, (u_int) p);

	tf = p->p_md.md_regs;

	if (pmap_debug_level >= -1)
		printf("mdregs=%08x pc=%08x lr=%08x sp=%08x\n",
		    (u_int) tf, tf->tf_pc, tf->tf_usr_lr, tf->tf_usr_sp);

	tf->tf_r11 = 0;				/* bottom of the fp chain */
	tf->tf_r12 = 0;				/* ??? */
	tf->tf_pc = pack->ep_entry;
	tf->tf_usr_lr = pack->ep_entry;
	tf->tf_svc_lr = 0x77777777;		/* Something we can see */
	tf->tf_usr_sp = stack;
	tf->tf_r10 = 0xaa55aa55;		/* Something we can see */
	tf->tf_spsr = PSR_USR32_MODE;

	p->p_addr->u_pcb.pcb_flags = 0;

	retval[1] = 0;
}


/*
 * Modify the current mapping for zero page to make it read only
 *
 * This routine is only used until things start forking. Then new
 * system pages are mapped read only in pmap_enter().
 */

void
zero_page_readonly()
{
	WriteWord(0xefc00000, L2_PTE((systempage.physical & PG_FRAME), AP_KR));
	tlbflush();
}


/*
 * Modify the current mapping for zero page to make it read/write
 *
 * This routine is only used until things start forking. Then system
 * pages belonging to user processes are never made writable.
 */

void
zero_page_readwrite()
{
	WriteWord(0xefc00000, L2_PTE((systempage.physical & PG_FRAME), AP_KRW));
	tlbflush();
}


/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn resets the signal mask, the stack, and the
 * frame pointer, it returns to the user specified pc.
 */

void
sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig;
	int mask;
	u_long code;
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack;
	extern char sigcode[], esigcode[];

	if (pmap_debug_level >= 0)
		printf("Sendsig: sig=%d mask=%08x catcher=%08x code=%08x\n",
		    sig, mask, (u_int)catcher, (u_int)code);

	tf = p->p_md.md_regs;
	oonstack = psp->ps_sigstk.ss_flags & SA_ONSTACK;

/*
 * Allocate space for the signal handler context.
 */

	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - sizeof(struct sigframe));
		psp->ps_sigstk.ss_flags |= SA_ONSTACK;
	} else {
		fp = (struct sigframe *)tf->tf_usr_sp - 1;
	}

/* 
 * Build the argument list for the signal handler.
 */

	frame.sf_signum = sig;

	frame.sf_code = code;
	frame.sf_scp = &fp->sf_sc;
	frame.sf_handler = catcher;

/*
 * Build the signal context to be used by sigreturn.
 */

	frame.sf_sc.sc_onstack = oonstack;
	frame.sf_sc.sc_mask   = mask;
	frame.sf_sc.sc_r0     = tf->tf_r0;
	frame.sf_sc.sc_r1     = tf->tf_r1;
	frame.sf_sc.sc_r2     = tf->tf_r2;
	frame.sf_sc.sc_r3     = tf->tf_r3;
	frame.sf_sc.sc_r4     = tf->tf_r4;
	frame.sf_sc.sc_r5     = tf->tf_r5;
	frame.sf_sc.sc_r6     = tf->tf_r6;
	frame.sf_sc.sc_r7     = tf->tf_r7;
	frame.sf_sc.sc_r8     = tf->tf_r8;
	frame.sf_sc.sc_r9     = tf->tf_r9;
	frame.sf_sc.sc_r10    = tf->tf_r10;
	frame.sf_sc.sc_r11    = tf->tf_r11;
	frame.sf_sc.sc_r12    = tf->tf_r12;
	frame.sf_sc.sc_usr_sp = tf->tf_usr_sp;
	frame.sf_sc.sc_usr_lr = tf->tf_usr_lr;
	frame.sf_sc.sc_svc_lr = tf->tf_svc_lr;
	frame.sf_sc.sc_pc     = tf->tf_pc;
	frame.sf_sc.sc_spsr   = tf->tf_spsr;

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
/*
 * Process has trashed its stack; give it an illegal
 * instruction to halt it in its tracks.
 */

		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

/*
 * Build context to run handler in.
 */

	tf->tf_r0 = frame.sf_signum;
	tf->tf_r1 = frame.sf_code;
	tf->tf_r2 = (u_int)frame.sf_scp;
	tf->tf_r3 = (u_int)frame.sf_handler;
	tf->tf_usr_sp = (int)fp;
	tf->tf_pc = (int)(((char *)PS_STRINGS) - (esigcode - sigcode));

	if (pmap_debug_level >= 0)
		printf("Sendsig: sig=%d pc=%08x\n", sig, tf->tf_pc);
}



/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psr to gain improper privileges or to cause
 * a machine fault.
 */

int
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext *scp, context;
/*	register struct sigframe *fp;*/
	register struct trapframe *tf;

	if (pmap_debug_level >= 0)
		printf("sigreturn: context=%08x\n", (int)SCARG(uap, sigcntxp));

	tf = p->p_md.md_regs;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */

	scp = SCARG(uap, sigcntxp);

	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

/*
 * Check for security violations.
 */

/* Make sure the processor mode has not been tampered with */

	if ((context.sc_spsr & PSR_MODE) != PSR_USR32_MODE)
		return(EINVAL);

	if (context.sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SA_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SA_ONSTACK;
	p->p_sigmask = context.sc_mask & ~sigcantmask;

	/*
	 * Restore signal context.
	 */

	tf->tf_r0    = context.sc_r0;
	tf->tf_r1    = context.sc_r1;
	tf->tf_r2    = context.sc_r2;
	tf->tf_r3    = context.sc_r3;
	tf->tf_r4    = context.sc_r4;
	tf->tf_r5    = context.sc_r5;
	tf->tf_r6    = context.sc_r6;
	tf->tf_r7    = context.sc_r7;
	tf->tf_r8    = context.sc_r8;
	tf->tf_r9    = context.sc_r9;
	tf->tf_r10   = context.sc_r10;
	tf->tf_r11   = context.sc_r11;
	tf->tf_r12   = context.sc_r12;
	tf->tf_usr_sp = context.sc_usr_sp;
	tf->tf_usr_lr = context.sc_usr_lr;
	tf->tf_svc_lr = context.sc_svc_lr;
	tf->tf_pc    = context.sc_pc;
	tf->tf_spsr  = context.sc_spsr;

#ifdef VALIDATE_TRAPFRAME
	validate_trapframe(tf, 6);
#endif
	
	return (EJUSTRETURN);
}

/*
 * machine dependent system variables.
 */

int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
	printf("cpu_sysctl: Currently stoned - Cannot support the operation\n");
	return(EOPNOTSUPP);
}


/*
 * Ok these are some development functions. They map blocks of memory
 * into the video ram virtual memory.
 * The idea is to follow this with a call to the vidc device to
 * reinitialise the vidc20 for the new video ram.
 */

/* Map DRAM into the video memory */

int
vmem_mapdram()
{
	u_int l2pagetable;
	u_int logical;

	if (videodram_start == 0 || videodram_size == 0)
		return(ENOMEM);

/* Flush any video data in the cache */

	idcflush();

/* Get the level 2 pagetable for the video memory */

	l2pagetable = pmap_pte(kernel_pmap, videomemory.vidm_vbase);

/* Map a block of DRAM into the video memory area */

	for (logical = 0; logical < 0x200000; logical += NBPG) {
		map_entry(l2pagetable, logical, videodram_start
		    + logical);
		map_entry(l2pagetable, logical + 0x200000,
		    videodram_start + logical);
	}

/* Flush the TLB so we pick up the new mappings */

	tlbflush();

/* Rebuild the video memory descriptor */

	videomemory.vidm_vbase = VMEM_VBASE;
	videomemory.vidm_pbase = videodram_start;
	videomemory.vidm_type = VIDEOMEM_TYPE_DRAM;
	videomemory.vidm_size = videodram_size;

/* Reinitialise the video system */

/*	video_reinit();*/
	return(0);
}


/* Map VRAM into the video memory */

int
vmem_mapvram()
{
	u_int l2pagetable;
	u_int logical;

	if (bootconfig.vram[0].address == 0 || bootconfig.vram[0].pages == 0)
		return(ENOMEM);

/* Flush any video data in the cache */

	idcflush();

/* Get the level 2 pagetable for the video memory */

	l2pagetable = pmap_pte(kernel_pmap, videomemory.vidm_vbase);

/* Map the VRAM into the video memory area */

	for (logical = 0; logical < 0x200000; logical += NBPG) {
		map_entry(l2pagetable, logical, bootconfig.vram[0].address
		    + logical);
		map_entry(l2pagetable, logical + 0x200000,
		    bootconfig.vram[0].address + logical);
	}

/* Flush the TLB so we pick up the new mappings */

	tlbflush();

/* Rebuild the video memory descriptor */

	videomemory.vidm_vbase = VMEM_VBASE;
	videomemory.vidm_pbase = VRAM_BASE;
	videomemory.vidm_type = VIDEOMEM_TYPE_VRAM;
	videomemory.vidm_size = bootconfig.vram[0].pages * NBPG;

/* Reinitialise the video system */

/*	video_reinit();*/
	return(0);
}


/* Set the cache behaviour for the video memory */

int
vmem_cachectl(flag)
	int flag;
{
	u_int l2pagetable;
	u_int logical;

	if (bootconfig.vram[0].address == 0 || bootconfig.vram[0].pages == 0)
		return(ENOMEM);

/* Flush any video data in the cache */

	idcflush();

/* Get the level 2 pagetable for the video memory */

	l2pagetable = pmap_pte(kernel_pmap, videomemory.vidm_vbase);

/* Map the VRAM into the video memory area */

	if (flag & 1) {
		for (logical = 0; logical < 0x200000; logical += NBPG) {
			map_entry(l2pagetable, logical, bootconfig.vram[0].address
			    + logical);
			map_entry(l2pagetable, logical + 0x200000,
			    bootconfig.vram[0].address + logical);
		}
	} else {
		for (logical = 0; logical < 0x200000; logical += NBPG) {
			map_entry_nc(l2pagetable, logical, bootconfig.vram[0].address
			    + logical);
			map_entry_nc(l2pagetable, logical + 0x200000,
			    bootconfig.vram[0].address + logical);
		}
	}

/* Flush the TLB so we pick up the new mappings */

	tlbflush();

	return(0);
}

#if 0
extern int vtvalbug;
extern char *vtlastbug;
extern u_int vtbugaddr;
extern u_int vtbugcaddr;

void
vtbugreport()
{
	printf("vtvalbug = %d\n", vtvalbug);
	if (vtlastbug)
		printf("vtlastbug = %s\n", vtlastbug);
	printf("vtbugaddr = %08x\n", vtbugaddr);
	printf("vtbugcaddr = %08x\n", vtbugcaddr);
}
#endif

/* End of machdep.c */

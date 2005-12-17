/*	$OpenBSD: zaurus_machdep.c,v 1.22 2005/12/17 07:31:27 miod Exp $	*/
/*	$NetBSD: lubbock_machdep.c,v 1.2 2003/07/15 00:25:06 lukem Exp $ */

/*
 * Copyright (c) 2002, 2003  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Machine dependant functions for kernel setup for 
 * Intel DBPXA250 evaluation board (a.k.a. Lubbock).
 * Based on iq80310_machhdep.c
 */
/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
 * Machine dependant functions for kernel setup for Intel IQ80310 evaluation
 * boards using RedBoot firmware.
 */

/*
 * DIP switches:
 *
 * S19: no-dot: set RB_KDB.  enter kgdb session.
 * S20: no-dot: set RB_SINGLE. don't go multi user mode.
 */

#include <sys/param.h>
#include <sys/device.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/msgbuf.h>
#include <sys/reboot.h>
#include <sys/termios.h>

#include <uvm/uvm_extern.h>

#include <sys/conf.h>
#include <sys/queue.h>
#include <sys/device.h>
#include <dev/cons.h>
#include <dev/ic/smc91cxxreg.h>

#include <machine/db_machdep.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#ifdef KGDB
#include <sys/kgdb.h>
#endif

#include <machine/bootconfig.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <arm/undefined.h>
#include <arm/machdep.h>

#include <arm/xscale/pxa2x0reg.h>
#include <arm/xscale/pxa2x0var.h>
#include <arm/xscale/pxa2x0_gpio.h>
#include <arm/sa11x0/sa1111_reg.h>
#include <machine/zaurus_reg.h>
#include <machine/zaurus_var.h>

#include <zaurus/dev/zaurus_scoopreg.h>

#include "apm.h"
#if NAPM > 0
#include <zaurus/dev/zaurus_apm.h>
#endif

#include "wsdisplay.h"

/* Kernel text starts 2MB in from the bottom of the kernel address space. */
#define	KERNEL_TEXT_BASE	(KERNEL_BASE + 0x00200000)
#define	KERNEL_VM_BASE		(KERNEL_BASE + 0x04000000)

/*
 * The range 0xc1000000 - 0xccffffff is available for kernel VM space
 * Core-logic registers and I/O mappings occupy 0xfd000000 - 0xffffffff
 */
/*
#define KERNEL_VM_SIZE		0x0C000000
*/
#define KERNEL_VM_SIZE		0x10000000


/*
 * Address to call from cpu_reset() to reset the machine.
 * This is machine architecture dependant as it varies depending
 * on where the ROM appears when you turn the MMU off.
 */

u_int cpu_reset_address = 0;

/* Define various stack sizes in pages */
#define IRQ_STACK_SIZE	1
#define ABT_STACK_SIZE	1
#ifdef IPKDB
#define UND_STACK_SIZE	2
#else
#define UND_STACK_SIZE	1
#endif

int zaurusmod;

BootConfig bootconfig;		/* Boot config storage */
char *boot_args = NULL;
char *boot_file = NULL;

paddr_t physical_start;
paddr_t physical_freestart;
paddr_t physical_freeend;
paddr_t physical_end;
u_int free_pages;
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
pv_addr_t minidataclean;

paddr_t msgbufphys;

extern u_int data_abort_handler_address;
extern u_int prefetch_abort_handler_address;
extern u_int undefined_handler_address;

#ifdef PMAP_DEBUG
extern int pmap_debug_level;
#endif

#define KERNEL_PT_SYS		0	/* Page table for mapping proc0 zero page */
#define KERNEL_PT_KERNEL	1	/* Page table for mapping kernel */
#define	KERNEL_PT_KERNEL_NUM	32
#define KERNEL_PT_VMDATA	(KERNEL_PT_KERNEL+KERNEL_PT_KERNEL_NUM)
				        /* Page tables for mapping kernel VM */
#define	KERNEL_PT_VMDATA_NUM	8	/* start with 32MB of KVM */
#define NUM_KERNEL_PTS		(KERNEL_PT_VMDATA + KERNEL_PT_VMDATA_NUM)

pv_addr_t kernel_pt_table[NUM_KERNEL_PTS];

extern struct user *proc0paddr;

/* Prototypes */

#define	BOOT_STRING_MAGIC 0x4f425344

char	bootargs[MAX_BOOT_STRING];
void	process_kernel_args(char *);

void	consinit(void);
void	early_clkman(u_int, int);
void	kgdb_port_init(void);
void	change_clock(uint32_t v);

bs_protos(bs_notimpl);

#include "com.h"
#if NCOM > 0
#include <dev/ic/comvar.h>
#include <dev/ic/comreg.h>
#endif

#ifndef CONSPEED
#define CONSPEED B9600	/* What RedBoot uses */
#endif
#ifndef CONMODE
#define CONMODE ((TTYDEF_CFLAG & ~(CSIZE | CSTOPB | PARENB)) | CS8) /* 8N1 */
#endif

int comcnspeed = CONSPEED;
int comcnmode = CONMODE;


/*
 * void boot(int howto, char *bootstr)
 *
 * Reboots the system
 *
 * Deal with any syncing, unmounting, dumping and shutdown hooks,
 * then reset the CPU.
 */
void
boot(int howto)
{
	/*
	 * If we are still cold then hit the air brakes
	 * and crash to earth fast
	 */
	if (cold) {
		doshutdownhooks();
		if ((howto & (RB_HALT | RB_USERREQ)) != RB_USERREQ) {
			printf("The operating system has halted.\n");
			printf("Please press any key to reboot.\n\n");
			cngetc();
		}
		printf("rebooting...\n");
		delay(6000000);
#if NAPM > 0
		zapm_restart();
#endif
		printf("reboot failed; spinning\n");
		while(1);
		/*NOTREACHED*/
	}

	/* Disable console buffering */
/*	cnpollc(1);*/

	/*
	 * If RB_NOSYNC was not specified sync the discs.
	 * Note: Unless cold is set to 1 here, syslogd will die during the
	 * unmount.  It looks like syslogd is getting woken up only to find
	 * that it cannot page part of the binary in as the filesystem has
	 * been unmounted.
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
#if NAPM > 0
		if (howto & RB_POWERDOWN) {

			printf("\nAttempting to power down...\n");
			delay(6000000);
			zapm_poweroff();
		}
#endif

		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	}

	printf("rebooting...\n");
	delay(6000000);
#if NAPM > 0
	zapm_restart();
#endif
	printf("reboot failed; spinning\n");
	while(1);
	/*NOTREACHED*/
}

static __inline
pd_entry_t *
read_ttb(void)
{
  long ttb;

  __asm __volatile("mrc	p15, 0, %0, c2, c0, 0" : "=r" (ttb));


  return (pd_entry_t *)(ttb & ~((1<<14)-1));
}

/*
 * Mapping table for core kernel memory. These areas are mapped in
 * init time at fixed virtual address with section mappings. 
 */
struct l1_sec_map {
	vaddr_t	va;
	vaddr_t	pa;
	vsize_t	size;
	int flags;
} l1_sec_table[] = {
    {
	    ZAURUS_GPIO_VBASE,
	    PXA2X0_GPIO_BASE,
	    PXA2X0_GPIO_SIZE,
	    PTE_NOCACHE,
    },
    {
	    ZAURUS_CLKMAN_VBASE,
	    PXA2X0_CLKMAN_BASE,
	    PXA2X0_CLKMAN_SIZE,
	    PTE_NOCACHE,
    },
    {
	    ZAURUS_INTCTL_VBASE,
	    PXA2X0_INTCTL_BASE,
	    PXA2X0_INTCTL_SIZE,
	    PTE_NOCACHE,
    },
    {
	    ZAURUS_SCOOP0_VBASE,
	    C3000_SCOOP0_BASE,
	    SCOOP_SIZE,
	    PTE_NOCACHE,
    },
    {
	    ZAURUS_SCOOP1_VBASE,
	    trunc_page(C3000_SCOOP1_BASE),
	    round_page(SCOOP_SIZE),
	    PTE_NOCACHE,
    },
    {0, 0, 0, 0,}
};

static void
map_io_area(paddr_t pagedir)
{
	int loop;

	/*
	 * Map devices we can map w/ section mappings.
	 */
	loop = 0;
	while (l1_sec_table[loop].size) {
		vsize_t sz;

#define VERBOSE_INIT_ARM
#ifdef VERBOSE_INIT_ARM
		printf("%08lx -> %08lx @ %08lx\n", l1_sec_table[loop].pa,
		    l1_sec_table[loop].pa + l1_sec_table[loop].size - 1,
		    l1_sec_table[loop].va);
#endif
		for (sz = 0; sz < l1_sec_table[loop].size; sz += L1_S_SIZE)
			pmap_map_section(pagedir, l1_sec_table[loop].va + sz,
			    l1_sec_table[loop].pa + sz,
			    VM_PROT_READ|VM_PROT_WRITE,
			    l1_sec_table[loop].flags);
		++loop;
	}
}

/*
 * simple memory mapping function used in early bootstrap stage
 * before pmap is initialized.
 * size and cacheability are ignored and map one section with nocache.
 */
static vaddr_t section_free = ZAURUS_VBASE_FREE;

static int
bootstrap_bs_map(void *t, bus_addr_t bpa, bus_size_t size,
    int cacheable, bus_space_handle_t *bshp)
{
	u_long startpa;
	vaddr_t va;
	pd_entry_t *pagedir = read_ttb();
	/* This assumes PA==VA for page directory */

	va = section_free;
	section_free += L1_S_SIZE;

	startpa = trunc_page(bpa);
	pmap_map_section((vaddr_t)pagedir, va, startpa, 
	    VM_PROT_READ | VM_PROT_WRITE, PTE_NOCACHE);
	cpu_tlb_flushD();

	*bshp = (bus_space_handle_t)(va + (bpa - startpa));

	return(0);
}

static void
copy_io_area_map(pd_entry_t *new_pd)
{
	pd_entry_t *cur_pd = read_ttb();
	vaddr_t va;

	for (va = ZAURUS_IO_AREA_VBASE;
	     (cur_pd[va>>L1_S_SHIFT] & L1_TYPE_MASK) == L1_TYPE_S;
	     va += L1_S_SIZE) {

		new_pd[va>>L1_S_SHIFT] = cur_pd[va>>L1_S_SHIFT];
		if (va == (0 - L1_S_SIZE))
			break; /* STUPID */

	}
}

/* XXX tidy up! */
void green_on(int virt);
void
green_on(int virt)
{
	/* clobber green led p */
	volatile u_int16_t *p;
	if (virt)
		p = (u_int16_t *)(ZAURUS_SCOOP0_VBASE+SCOOP_GPWR);
	else
		p = (u_int16_t *)(C3000_SCOOP0_BASE+SCOOP_GPWR);

	*p = *p | (1<<SCOOP0_LED_GREEN);
}
void irda_on(int virt);
void
irda_on(int virt)
{
	/* clobber IrDA led p */
	volatile u_int16_t *p;
	/* XXX scoop1 registers are not page-aligned! */
	int ofs = C3000_SCOOP1_BASE - trunc_page(C3000_SCOOP1_BASE);

	if (virt)
		p = (u_int16_t *)(ZAURUS_SCOOP1_VBASE+ofs+SCOOP_GPWR);
	else
		p = (u_int16_t *)(C3000_SCOOP1_BASE+SCOOP_GPWR);

	*p = *p & ~(1<<SCOOP1_IR_ON);
}

#if 0
void sysprobe(void);
void
sysprobe(void)
{
	u_int32_t *p;

	p = (void *)0x48000014; /* MECR */
	printf("MECR %x\n", *p);

	p = (void *)0x48000028; /* MCMEM0 */
	printf("MCMEM0 %x\n", *p); 
	p = (void *)0x4800002C; /* MCMEM1 */
	printf("MCMEM1 %x\n", *p); 

	p = (void *)0x48000030; /* MCATTx */
	printf("MCATT0 %x\n", *p); 
	p = (void *)0x48000034; /* MCATTx */
	printf("MCATT1 %x\n", *p); 

	p = (void *)0x48000038; /* MCIOx */
	printf("MCIO0 %x\n", *p); 
	p = (void *)0x4800003C; /* MCIOx */
	printf("MCIO1 %x\n", *p); 
}
#endif

/*
 * u_int initarm(...)
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
initarm(void *arg)
{
	extern vaddr_t xscale_cache_clean_addr;
	int loop;
	int loop1;
	u_int l1pagetable;
	pv_addr_t kernel_l1pt;
	paddr_t memstart;
	psize_t memsize;
	extern u_int32_t esym;	/* &_end if no symbols are loaded */

#if 0
	int led_data = 0;
#endif
#ifdef DIAGNOSTIC
	extern vsize_t xscale_minidata_clean_size; /* used in KASSERT */
#endif
	/* early bus_space_map support */
	struct bus_space tmp_bs_tag;
	int	(*map_func_save)(void *, bus_addr_t, bus_size_t, int, 
	    bus_space_handle_t *);


#if 0
	/* XXX */
	/* start 32.768KHz OSC */
	ioreg_write(PXA2X0_CLKMAN_BASE + 0x08, 2);
#endif

	/*
	 * Heads up ... Setup the CPU / MMU / TLB functions
	 */
	if (set_cpufuncs())
		panic("cpu not recognized!");

	/* Get ready for splfoo() */
	pxa2x0_intr_bootstrap(PXA2X0_INTCTL_BASE);

#if 0
	/* Calibrate the delay loop. */
#endif

	/*
	 * Okay, RedBoot has provided us with the following memory map:
	 *
	 * Physical Address Range     Description 
	 * -----------------------    ---------------------------------- 
	 * 0x00000000 - 0x01ffffff    flash Memory   (32MB)
	 * 0x04000000 - 0x05ffffff    Application flash Memory  (32MB)
	 * 0x08000000 - 0x080000ff    I/O baseboard registers
	 * 0x0a000000 - 0x0a0fffff    SRAM (1MB)
	 * 0x0c000000 - 0x0c0fffff    Ethernet Controller
	 * 0x0e000000 - 0x0e0fffff    Ethernet Controller (Attribute)
	 * 0x10000000 - 0x103fffff    SA-1111 Companion Chip
	 * 0x14000000 - 0x17ffffff    Expansion Card (64MB)
	 * 0x40000000 - 0x480fffff    Processor Registers
	 * 0xa0000000 - 0xa3ffffff    SDRAM Bank 0 (64MB)
	 *
	 *
	 * Virtual Address Range    X C B  Description 
	 * -----------------------  - - -  ---------------------------------- 
	 * 0x00000000 - 0x00003fff  N Y Y  SDRAM 
	 * 0x00004000 - 0x000fffff  N Y N  Boot ROM
	 * 0x00100000 - 0x01ffffff  N N N  Application Flash
	 * 0x04000000 - 0x05ffffff  N N N  Exp Application Flash
	 * 0x08000000 - 0x080fffff  N N N  I/O baseboard registers
	 * 0x0a000000 - 0x0a0fffff  N N N  SRAM
	 * 0x40000000 - 0x480fffff  N N N  Processor Registers
	 * 0xa0000000 - 0xa000ffff  N Y N  RedBoot SDRAM 
	 * 0xa0017000 - 0xa3ffffff  Y Y Y  SDRAM
	 * 0xc0000000 - 0xcfffffff  Y Y Y  Cache Flush Region 
	 * (done by this routine)
	 * 0xfd000000 - 0xfd0000ff  N N N  I/O baseboard registers
	 * 0xfd100000 - 0xfd2fffff  N N N  Processor Registers.
	 * 0xfd200000 - 0xfd2fffff  N N N  0x10800000 registers
	 *
	 * The first level page table is at 0xa0004000.  There are also
	 * 2 second-level tables at 0xa0008000 and 0xa0008400.
	 *
	 */

	{
		/*
		 * Tweak RedBoot's pagetable so that we can access to
		 * some registers at same VA before and after installing 
		 * our page table. 
		 */
		paddr_t ttb = (paddr_t)read_ttb();

		map_io_area(ttb);
		cpu_tlb_flushD();
	}

	/*
	 * Examine the boot args string for options we need to know about
	 * now.
	 */
	/* XXX should really be done after setting up the console, but we
	 * XXX need to parse the console selection flags right now. */
	process_kernel_args((char *)0xa0200000 - MAX_BOOT_STRING - 1);
#ifdef RAMDISK_HOOKS
        boothowto |= RB_DFLTROOT;
#endif /* RAMDISK_HOOKS */

	/*
	 * This test will work for now but has to be revised when support
	 * for other models is added.
	 */
	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X)
		zaurusmod = ZAURUS_C3000;
	else
		zaurusmod = ZAURUS_C860;

	/* setup GPIO for BTUART, in case bootloader doesn't take care of it */
	pxa2x0_gpio_bootstrap(ZAURUS_GPIO_VBASE);
#if 0
	pxa2x0_gpio_set_function(42, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(43, GPIO_ALT_FN_2_OUT);
	pxa2x0_gpio_set_function(44, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(45, GPIO_ALT_FN_2_OUT);

	/* FFUART */
	pxa2x0_gpio_set_function(34, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(39, GPIO_ALT_FN_2_OUT);
	pxa2x0_gpio_set_function(35, GPIO_ALT_FN_1_IN);
	pxa2x0_gpio_set_function(40, GPIO_ALT_FN_2_OUT);
	pxa2x0_gpio_set_function(41, GPIO_ALT_FN_2_OUT);

	/* STUART */
	pxa2x0_gpio_set_function(46, GPIO_ALT_FN_2_IN);
	pxa2x0_gpio_set_function(47, GPIO_ALT_FN_1_OUT);
#endif

	/* tell com to drive STUART in slow infrared mode */
	comsiraddr = (bus_addr_t)PXA2X0_STUART_BASE;

#if 1
	/* turn on clock to UART block.
	   XXX this should not be necessary, consinit() will do it */
	early_clkman(CKEN_FFUART | CKEN_BTUART | CKEN_STUART, 1);
#endif

	green_on(0);

	/*
	 * Temporarily replace bus_space_map() functions so that
	 * console devices can get mapped.
	 *
	 * Note that this relies upon the fact that both regular
	 * and a4x bus_space tags use the same map function.
	 */
	tmp_bs_tag = pxa2x0_bs_tag;
	tmp_bs_tag.bs_map = bootstrap_bs_map;
	map_func_save = pxa2x0_bs_tag.bs_map;
	pxa2x0_a4x_bs_tag.bs_map = pxa2x0_bs_tag.bs_map = bootstrap_bs_map;

	/* setup a serial console for very early boot */
	consinit();
#ifdef KGDB
	kgdb_port_init();
#endif


	/* Talk to the user */
	printf("\nOpenBSD/zaurus booting ...\n");

	{
		/* XXX - all Zaurus have this for now, fix memory sizing */
		memstart = 0xa0000000;
		memsize =  0x04000000; /* 64MB */
	}

#if 0
	{
		volatile int *p;
		char *membase;
		char *memmax;
		int  chunksize = 0x02000000;
		printf("probing memory");

		membase = (char *)0xa0000000;
		memmax  = (char *)0xc0000000;
		for (p = (int *)membase;
		    p < (int *)memmax;
		    p = (int *) (((char *)p) + chunksize)) {
			printf ("cbase %p\n", p);
			p[0] = 0x12345678;
			p[1] = 0x12345678;
			if ((p[0] != 0x12345678) || (p[1] != 0x12345678))
				break;
		}
			memsize = ((char *)p) - membase;

		printf("probing memory done found memsize %d\n", memsize);
	}
#else
#endif

#define DEBUG
#ifdef DEBUG
	printf("initarm: Configuring system ...\n");
#endif

	/* Fake bootconfig structure for the benefit of pmap.c */
	/* XXX must make the memory description h/w independant */
	bootconfig.dramblocks = 1;
	bootconfig.dram[0].address = memstart;
	bootconfig.dram[0].pages = memsize / PAGE_SIZE;

	/*
	 * Set up the variables that define the availablilty of
	 * physical memory.  For now, we're going to set
	 * physical_freestart to 0xa0200000 (where the kernel
	 * was loaded), and allocate the memory we need downwards.
	 * If we get too close to the page tables that RedBoot
	 * set up, we will panic.  We will update physical_freestart
	 * and physical_freeend later to reflect what pmap_bootstrap()
	 * wants to see.
	 *
	 * XXX pmap_bootstrap() needs an enema.
	 */
	physical_start = bootconfig.dram[0].address;
	physical_end = physical_start + (bootconfig.dram[0].pages * PAGE_SIZE);

	physical_freestart = 0xa0009000UL;
	physical_freeend = 0xa0200000UL;

	physmem = (physical_end - physical_start) / PAGE_SIZE;

#ifdef DEBUG
	/* Tell the user about the memory */
	printf("physmemory: %d pages at 0x%08lx -> 0x%08lx\n", physmem,
	    physical_start, physical_end - 1);
#endif

	/*
	 * Okay, the kernel starts 2MB in from the bottom of physical
	 * memory.  We are going to allocate our bootstrap pages downwards
	 * from there.
	 *
	 * We need to allocate some fixed page tables to get the kernel
	 * going.  We allocate one page directory and a number of page
	 * tables and store the physical addresses in the kernel_pt_table
	 * array.
	 *
	 * The kernel page directory must be on a 16K boundary.  The page
	 * tables must be on 4K bounaries.  What we do is allocate the
	 * page directory on the first 16K boundary that we encounter, and
	 * the page tables on 4K boundaries otherwise.  Since we allocate
	 * at least 3 L2 page tables, we are guaranteed to encounter at
	 * least one 16K aligned region.
	 */

#ifdef VERBOSE_INIT_ARM
	printf("Allocating page tables\n");
#endif

	free_pages = (physical_freeend - physical_freestart) / PAGE_SIZE;

#ifdef VERBOSE_INIT_ARM
	printf("freestart = 0x%08lx, free_pages = %d (0x%08x)\n",
	       physical_freestart, free_pages, free_pages);
#endif

	/* Define a macro to simplify memory allocation */
#define	valloc_pages(var, np)				\
	alloc_pages((var).pv_pa, (np));			\
	(var).pv_va = KERNEL_BASE + (var).pv_pa - physical_start;

#define alloc_pages(var, np)				\
	physical_freeend -= ((np) * PAGE_SIZE);		\
	if (physical_freeend < physical_freestart)	\
		panic("initarm: out of memory");	\
	(var) = physical_freeend;			\
	free_pages -= (np);				\
	memset((char *)(var), 0, ((np) * PAGE_SIZE));

	loop1 = 0;
	kernel_l1pt.pv_pa = 0;
	for (loop = 0; loop <= NUM_KERNEL_PTS; ++loop) {
		/* Are we 16KB aligned for an L1 ? */
		if (((physical_freeend - L1_TABLE_SIZE) & (L1_TABLE_SIZE - 1)) == 0
		    && kernel_l1pt.pv_pa == 0) {
			valloc_pages(kernel_l1pt, L1_TABLE_SIZE / PAGE_SIZE);
		} else {
			valloc_pages(kernel_pt_table[loop1],
			    L2_TABLE_SIZE / PAGE_SIZE);
			++loop1;
		}
	}

	/* This should never be able to happen but better confirm that. */
	if (!kernel_l1pt.pv_pa || (kernel_l1pt.pv_pa & (L1_TABLE_SIZE-1)) != 0)
		panic("initarm: Failed to align the kernel page directory");

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

	/* Allocate enough pages for cleaning the Mini-Data cache. */
	KASSERT(xscale_minidata_clean_size <= PAGE_SIZE);
	valloc_pages(minidataclean, 1);

#ifdef VERBOSE_INIT_ARM
	printf("IRQ stack: p0x%08lx v0x%08lx\n", irqstack.pv_pa,
	    irqstack.pv_va); 
	printf("ABT stack: p0x%08lx v0x%08lx\n", abtstack.pv_pa,
	    abtstack.pv_va); 
	printf("UND stack: p0x%08lx v0x%08lx\n", undstack.pv_pa,
	    undstack.pv_va); 
	printf("SVC stack: p0x%08lx v0x%08lx\n", kernelstack.pv_pa,
	    kernelstack.pv_va); 
#endif

	/*
	 * XXX Defer this to later so that we can reclaim the memory
	 * XXX used by the RedBoot page tables.
	 */
	alloc_pages(msgbufphys, round_page(MSGBUFSIZE) / PAGE_SIZE);

	/*
	 * Ok we have allocated physical pages for the primary kernel
	 * page tables
	 */

#ifdef VERBOSE_INIT_ARM
	printf("Creating L1 page table at 0x%08lx\n", kernel_l1pt.pv_pa);
#endif

	/*
	 * Now we start construction of the L1 page table
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

	for (loop = 0; loop < KERNEL_PT_VMDATA_NUM; loop++)
		pmap_link_l2pt(l1pagetable, KERNEL_VM_BASE + loop * 0x00400000,
		    &kernel_pt_table[KERNEL_PT_VMDATA + loop]);

	/* update the top of the kernel VM */
	pmap_curmaxkvaddr =
	    KERNEL_VM_BASE + (KERNEL_PT_VMDATA_NUM * 0x00400000);

#ifdef VERBOSE_INIT_ARM
	printf("Mapping kernel\n");
#endif

	/* Now we fill in the L2 pagetable for the kernel static code/data
	 * and the symbol table. */
	{
		extern char etext[];
		size_t textsize = (u_int32_t) etext - KERNEL_TEXT_BASE;
		size_t totalsize = esym - KERNEL_TEXT_BASE;
		u_int logical;

		textsize = (textsize + PGOFSET) & ~PGOFSET;
		totalsize = (totalsize + PGOFSET) & ~PGOFSET;
		
		logical = 0x00200000;	/* offset of kernel in RAM */

		logical += pmap_map_chunk(l1pagetable, KERNEL_BASE + logical,
		    physical_start + logical, textsize,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
		pmap_map_chunk(l1pagetable, KERNEL_BASE + logical,
		    physical_start + logical, totalsize - textsize,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	}

#ifdef VERBOSE_INIT_ARM
	printf("Constructing L2 page tables\n");
#endif

	/* Map the stack pages */
	pmap_map_chunk(l1pagetable, irqstack.pv_va, irqstack.pv_pa,
	    IRQ_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, abtstack.pv_va, abtstack.pv_pa,
	    ABT_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, undstack.pv_va, undstack.pv_pa,
	    UND_STACK_SIZE * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
	pmap_map_chunk(l1pagetable, kernelstack.pv_va, kernelstack.pv_pa,
	    UPAGES * PAGE_SIZE, VM_PROT_READ | VM_PROT_WRITE, PTE_CACHE);

	pmap_map_chunk(l1pagetable, kernel_l1pt.pv_va, kernel_l1pt.pv_pa,
	    L1_TABLE_SIZE, VM_PROT_READ | VM_PROT_WRITE, PTE_PAGETABLE);

	for (loop = 0; loop < NUM_KERNEL_PTS; ++loop) {
		pmap_map_chunk(l1pagetable, kernel_pt_table[loop].pv_va,
		    kernel_pt_table[loop].pv_pa, L2_TABLE_SIZE,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_PAGETABLE);
	}

	/* Map the Mini-Data cache clean area. */
	xscale_setup_minidata(l1pagetable, minidataclean.pv_va,
	    minidataclean.pv_pa);

	/* Map the vector page. */
#if 1
	/* MULTI-ICE requires that page 0 is NC/NB so that it can download the
	 * cache-clean code there.  */
	pmap_map_entry(l1pagetable, vector_page, systempage.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_NOCACHE);
#else
	pmap_map_entry(l1pagetable, vector_page, systempage.pv_pa,
	    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
#endif

	/*
	 * map integrated peripherals at same address in l1pagetable
	 * so that we can continue to use console.
	 */
	copy_io_area_map((pd_entry_t *)l1pagetable);

	/*
	 * Give the XScale global cache clean code an appropriately
	 * sized chunk of unmapped VA space starting at 0xff000000
	 * (our device mappings end before this address).
	 */
	xscale_cache_clean_addr = 0xff000000U;

	/*
	 * Now we have the real page tables in place so we can switch to them.
	 * Once this is done we will be running with the REAL kernel page
	 * tables.
	 */

	/*
	 * Update the physical_freestart/physical_freeend/free_pages
	 * variables.
	 */
	{
		physical_freestart = physical_start +
		    (((esym + PGOFSET) & ~PGOFSET) - KERNEL_BASE);
		physical_freeend = physical_end;
		free_pages =
		    (physical_freeend - physical_freestart) / PAGE_SIZE;
	}

	/* be a client to all domains */
	cpu_domains(0x55555555);
	/* Switch tables */
#ifdef VERBOSE_INIT_ARM
	printf("freestart = 0x%08lx, free_pages = %d (0x%x)\n",
	       physical_freestart, free_pages, free_pages);
	printf("switching to new L1 page table  @%#lx...", kernel_l1pt.pv_pa);
#endif

	/* set new intc register address so that splfoo() doesn't
	   touch illegal address.  */
	pxa2x0_intr_bootstrap(ZAURUS_INTCTL_VBASE);

	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT);
	setttb(kernel_l1pt.pv_pa);
	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2));

	/*
	 * Moved from cpu_startup() as data_abort_handler() references
	 * this during uvm init
	 */
	proc0paddr = (struct user *)kernelstack.pv_va;
	proc0.p_addr = proc0paddr;

#ifdef VERBOSE_INIT_ARM
	printf("bootstrap done.\n");
#endif

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
	 * Once things get going this will change as we will need a proper
	 * handler.
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

	/* Initialise the undefined instruction handlers */
#ifdef VERBOSE_INIT_ARM
	printf("undefined ");
#endif
	undefined_init();

	/* Load memory into UVM. */
#ifdef VERBOSE_INIT_ARM
	printf("page ");
#endif
	uvm_setpagesize();        /* initialize PAGE_SIZE-dependent variables */
	uvm_page_physload(atop(physical_freestart), atop(physical_freeend),
	    atop(physical_freestart), atop(physical_freeend),
	    VM_FREELIST_DEFAULT);

	/* Boot strap pmap telling it where the kernel page table is */
#ifdef VERBOSE_INIT_ARM
	printf("pmap ");
#endif
	pmap_bootstrap((pd_entry_t *)kernel_l1pt.pv_va, KERNEL_VM_BASE,
	    KERNEL_VM_BASE + KERNEL_VM_SIZE);

#ifdef __HAVE_MEMORY_DISK__
	md_root_setconf(memory_disk, sizeof memory_disk);
#endif

#ifdef IPKDB
	/* Initialise ipkdb */
	ipkdb_init();
	if (boothowto & RB_KDB)
		ipkdb_connect(0);
#endif

#ifdef KGDB
	if (boothowto & RB_KDB) {
		kgdb_debug_init = 1;
		kgdb_connect(1);
	}
#endif

	/*
	 * Restore proper bus_space operation, now that pmap is initialized.
	 */
	pxa2x0_a4x_bs_tag.bs_map = pxa2x0_bs_tag.bs_map = map_func_save;

#ifdef DDB
	db_machine_init();

	/* Firmware doesn't load symbols. */
	ddb_init();

	if (boothowto & RB_KDB)
		Debugger();
#endif

	/* We return the new stack pointer address */
	return(kernelstack.pv_va + USPACE_SVC_STACK_TOP);
}

const char *console = "glass";

void
process_kernel_args(char *args)
{
	char *cp = args;

	if (cp == NULL || *(int *)cp != BOOT_STRING_MAGIC) {
		boothowto = RB_AUTOBOOT;
		return;
	}

	/* Eat the cookie */
	*(int *)cp = 0;
	cp += sizeof(int);

	boothowto = 0;

	/* Make a local copy of the bootargs */
	strncpy(bootargs, cp, MAX_BOOT_STRING - sizeof(int));

	cp = bootargs;
	boot_file = bootargs;

	/* Skip the kernel image filename */
	while (*cp != ' ' && *cp != 0)
		++cp;

	if (*cp != 0)
		*cp++ = 0;

	while (*cp == ' ')
		++cp;

	boot_args = cp;

	printf("bootfile: %s\n", boot_file);
	printf("bootargs: %s\n", boot_args);

	/* Setup pointer to boot flags */
	while (*cp != '-')
		if (*cp++ == '\0')
			return;

	for (;*++cp;) {
		int fl;

		fl = 0;
		switch(*cp) {
		case 'a':
			fl |= RB_ASKNAME;
			break;
		case 'c':
			fl |= RB_CONFIG;
			break;
		case 'd':
			fl |= RB_KDB;
			break;
		case 's':
			fl |= RB_SINGLE;
			break;
		/* XXX undocumented console switching flags */
		case '0':
			console = "ffuart";
			break;
		case '1':
			console = "btuart";
			break;
		case '2':
			console = "stuart";
			break;
		default:
			printf("unknown option `%c'\n", *cp);
			break;
		}
		boothowto |= fl;
	}
}

#ifdef KGDB
#ifndef KGDB_DEVNAME
#define KGDB_DEVNAME "ffuart"
#endif
const char kgdb_devname[] = KGDB_DEVNAME;

#if (NCOM > 0)
#ifndef KGDB_DEVMODE
#define KGDB_DEVMODE ((TTYDEF_CFLAG & ~(CSIZE | CSTOPB | PARENB)) | CS8) /* 8N1 */
#endif
int comkgdbmode = KGDB_DEVMODE;
#endif /* NCOM */

#endif /* KGDB */

void
consinit(void)
{
#if NCOM > 0
	static int consinit_called = 0;
	paddr_t paddr;
	u_int cken = 0;

	if (consinit_called != 0)
		return;

	consinit_called = 1;

#ifdef KGDB
	if (strcmp(kgdb_devname, console) == 0) {
		/* port is reserved for kgdb */
	} else
#endif
	if (strcmp(console, "ffuart") == 0) {
		paddr = PXA2X0_FFUART_BASE;
		cken = CKEN_FFUART;
	} else if (strcmp(console, "btuart") == 0) {
		paddr = PXA2X0_BTUART_BASE;
		cken = CKEN_BTUART;
	} else if (strcmp(console, "stuart") == 0) {
		paddr = PXA2X0_STUART_BASE;
		cken = CKEN_STUART;
		irda_on(0);
	}
	if (cken != 0 && comcnattach(&pxa2x0_a4x_bs_tag, paddr, comcnspeed,
	    PXA2X0_COM_FREQ, comcnmode) == 0) {
		early_clkman(cken, 1);
	}
#endif /* NCOM */
}

#ifdef KGDB
void
kgdb_port_init(void)
{
#if (NCOM > 0) && defined(COM_PXA2X0)
	paddr_t paddr;
	u_int cken;

	if (strcmp(kgdb_devname, "ffuart") == 0) {
		paddr = PXA2X0_FFUART_BASE;
		cken = CKEN_FFUART;
	} else if (strcmp(kgdb_devname, "btuart") == 0) {
		paddr = PXA2X0_BTUART_BASE;
		cken = CKEN_BTUART;
	} else if (strcmp(kgdb_devname, "stuart") == 0) {
		paddr = PXA2X0_STUART_BASE;
		cken = CKEN_STUART;
		irda_on(0);
	} else
		return;

	if (com_kgdb_attach_pxa2x0(&pxa2x0_a4x_bs_tag, paddr,
	    kgdb_rate, PXA2X0_COM_FREQ, COM_TYPE_PXA2x0, comkgdbmode) == 0) {
		early_clkman(cken, 1);
	}
#endif
}
#endif

/* same as pxa2x0_clkman, but before autoconf */
void
early_clkman(u_int clk, int enable)
{
	u_int32_t rv;

	rv = ioreg_read(ZAURUS_CLKMAN_VBASE + CLKMAN_CKEN);
	if (enable)
		rv |= clk;
	else
		rv &= ~clk;
	ioreg_write(ZAURUS_CLKMAN_VBASE + CLKMAN_CKEN, rv);
}

int glass_console = 0;

void
board_startup(void)
{
	extern int lcd_cnattach(void (*)(u_int, int));
	extern bus_addr_t comconsaddr;

#if NWSDISPLAY > 0
	/*
	 * Try to attach the display console now that VM services
	 * are available.
	 */

	if ((cputype & ~CPU_ID_XSCALE_COREREV_MASK) == CPU_ID_PXA27X) {
		if (strcmp(console, "glass") == 0) {
			printf("attempting to switch console to lcd screen\n");
			glass_console = 1;
		}
		if (glass_console == 1 && lcd_cnattach(early_clkman) == 0) {
			/*
			 * Kill the existing serial console.
			 * XXX need to bus_space_unmap resources and disable
			 *     clocks...
			 */
			comconsaddr = 0;

			/*
			 * Display the copyright notice again on the new console
			 */
			extern const char copyright[];
			printf("%s\n", copyright);
		}
	}
#endif

        if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}
}

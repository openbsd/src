/*	$OpenBSD	*/
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

#include <sys/cdefs.h>
#include "rd.h"
#include "lcd.h"

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

BootConfig bootconfig;		/* Boot config storage */
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
pv_addr_t minidataclean;

vm_offset_t msgbufphys;

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
#define	KERNEL_PT_VMDATA_NUM	8	/* start with 16MB of KVM */
#define NUM_KERNEL_PTS		(KERNEL_PT_VMDATA + KERNEL_PT_VMDATA_NUM)

pv_addr_t kernel_pt_table[NUM_KERNEL_PTS];

extern struct user *proc0paddr;

/* Prototypes */

void	zaurus_reset(void);
void	zaurus_powerdown(void);

#if 0
void	process_kernel_args(char *);
void	parse_mi_bootargs(char *args);
#endif

void	consinit(void);
void	kgdb_port_init(void);
void	change_clock(uint32_t v);

bs_protos(bs_notimpl);

#include "com.h"
#if NCOM > 0
#include <dev/ic/comreg.h>
#include <dev/ic/comvar.h>
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
 *
 */
void
zaurus_reset(void)
{
	bus_space_tag_t bust = &pxa2x0_bs_tag;
	bus_space_handle_t bush_pow;
	bus_space_handle_t bush_mc;
	int rv;

	if (bus_space_map(bust, PXA2X0_POWMAN_BASE, PXA2X0_POWMAN_SIZE, 0,
	    &bush_pow))
		panic("pxa2x0_gpio_boot: failed to map POWMAN");

	if (bus_space_map(bust, PXA2X0_MEMCTL_BASE, PXA2X0_MEMCTL_SIZE, 0,
	    &bush_mc))
		panic("zaurus_reset: failed to map MEMCTL");

	bus_space_write_4(bust, bush_pow, POWMAN_RCSR,
	    RCSR_GPR | RCSR_SMR | RCSR_WDR | RCSR_HWR);

	rv = bus_space_read_4(bust, bush_mc, MEMCTL_MSC0);
        if ((rv & 0xffff0000) == 0x7ff00000)
		bus_space_write_4(bust, bush_mc, MEMCTL_MSC0,
		    (rv & 0xffff) | 0x7ee00000);

	pxa2x0_gpio_set_function(89, GPIO_OUT | GPIO_SET);

	/* Wait for the external reset circuit to kick the CPU. */
	delay(1000000);
}

/*
 *
 */
void
zaurus_powerdown(void)
{

	/*
	 * XXX most of the time this does the right thing, but
	 * XXX sometimes the zaurus can't be turned on by pressing
	 * XXX the power button until the battery is replaced.
	 */
	pxa2x0_watchdog_boot();
}

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
#ifdef DIAGNOSTIC
	/* info */
	printf("boot: howto=%08x curproc=%p\n", howto, curproc);
#endif

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
		zaurus_reset();
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
		if (howto & RB_POWERDOWN) {

			printf("\nAttempting to power down...\n");
			delay(500000);
			zaurus_powerdown();
		}

		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	}

	printf("rebooting...\n");
	zaurus_reset();
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
	    LUBBOCK_OBIO_VBASE,
	    LUBBOCK_OBIO_PBASE,
	    LUBBOCK_OBIO_SIZE,
	    PTE_NOCACHE,
    },
    {
	    LUBBOCK_GPIO_VBASE,
	    PXA2X0_GPIO_BASE,
	    PXA2X0_GPIO_SIZE,
	    PTE_NOCACHE,
    },
    {
	    LUBBOCK_CLKMAN_VBASE,
	    PXA2X0_CLKMAN_BASE,
	    PXA2X0_CLKMAN_SIZE,
	    PTE_NOCACHE,
    },
    {
	    LUBBOCK_INTCTL_VBASE,
	    PXA2X0_INTCTL_BASE,
	    PXA2X0_INTCTL_SIZE,
	    PTE_NOCACHE,
    },
    {
	    LUBBOCK_INTCTL_VBASE,
	    PXA2X0_INTCTL_BASE,
	    PXA2X0_INTCTL_SIZE,
	    PTE_NOCACHE,
    },
    {
	    LUBBOCK_AGPIO_VBASE,
	    0x10800000,
	    0x00010000, /* XXX */
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
		vm_size_t sz;

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
static vaddr_t section_free = LUBBOCK_VBASE_FREE;

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
	    VM_PROT_READ|VM_PROT_WRITE, PTE_NOCACHE);
	cpu_tlb_flushD();

	*bshp = (bus_space_handle_t)(va + (bpa - startpa));

	return(0);
}

static void
copy_io_area_map(pd_entry_t *new_pd)
{
	pd_entry_t *cur_pd = read_ttb();
	vaddr_t va;

	for (va = LUBBOCK_IO_AREA_VBASE;
	     (cur_pd[va>>L1_S_SHIFT] & L1_TYPE_MASK) == L1_TYPE_S;
	     va += L1_S_SIZE) {

		new_pd[va>>L1_S_SHIFT] = cur_pd[va>>L1_S_SHIFT];
		if (va == (0 - L1_S_SIZE))
			break; /* STUPID */

	}
}

void green_on(int virt);
void
green_on(int virt)
{
	/* clobber green led p */
	volatile u_int16_t *p;
	if (virt)
		p = (u_int16_t *)(LUBBOCK_AGPIO_VBASE+0x24);
	else
		p = (u_int16_t *)0x10800024;

	*p = *p | 2;
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

#if 0
	int led_data = 0;
#endif
#ifdef DIAGNOSTIC
	extern vsize_t xscale_minidata_clean_size; /* used in KASSERT */
#endif
	struct bus_space tmp_bs_tag;
	int	(*map_func_save)(void *, bus_addr_t, bus_size_t, int, 
	    bus_space_handle_t *);
#if 0
#define LEDSTEP_P() 	ioreg_write(LUBBOCK_OBIO_PBASE+LUBBOCK_HEXLED, led_data++)
#define LEDSTEP() hex_led(led_data++)
#else
#define LEDSTEP_P() 	
#define LEDSTEP()
#endif



	/* use physical address until pagetable is set */
	LEDSTEP_P();

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
	LEDSTEP_P();

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

	/* setup GPIO for BTUART, in case bootloader doesn't take care of it */
	pxa2x0_gpio_bootstrap(LUBBOCK_GPIO_VBASE);
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
#endif

#if 1
	/* turn on clock to UART block.
	   XXX: this should not be done here. */
	ioreg_write(LUBBOCK_CLKMAN_VBASE+CLKMAN_CKEN, CKEN_FFUART|CKEN_BTUART |
	    ioreg_read(LUBBOCK_CLKMAN_VBASE+CLKMAN_CKEN));
#endif

	green_on(0);

	LEDSTEP();

	tmp_bs_tag = pxa2x0_bs_tag;
	tmp_bs_tag.bs_map = bootstrap_bs_map;
	map_func_save = pxa2x0_a4x_bs_tag.bs_map;
	pxa2x0_a4x_bs_tag.bs_map = bootstrap_bs_map;

	LEDSTEP();


	consinit();
	LEDSTEP();
#ifdef KGDB
	kgdb_port_init();
	LEDSTEP();
#endif


	/* Talk to the user */
	printf("\nOpenBSD/zaurus booting ...\n");

	/* Tweak memory controller */
	{
		/* Modify access timing for CS3 (91c96) */

		uint32_t tmp = 
			ioreg_read(PXA2X0_MEMCTL_BASE+MEMCTL_MSC1);
		ioreg_write(PXA2X0_MEMCTL_BASE+MEMCTL_MSC1,
			     (tmp & 0xffff) | (0x3881<<16));
		/* RRR=3, RDN=8, RDF=8
		 * XXX: can be faster?
		 */
	}


	/* Initialize for PCMCIA/CF sockets */
	{
		uint32_t tmp;

		/* Activate two sockets.
		   XXX: This code segment should be moved to
		        pcmcia MD attach routine.
		   XXX: These bits should be toggled based on
		        existene of PCMCIA/CF cards
		*/
		ioreg_write(PXA2X0_MEMCTL_BASE+MEMCTL_MECR,
			     MECR_NOS|MECR_CIT);

		tmp = ioreg_read(LUBBOCK_SACC_PBASE+SACCSBI_SKCR);
		ioreg_write(LUBBOCK_SACC_PBASE+SACCSBI_SKCR,
			     (tmp & ~(1<<4)) | (1<<0));
	}

#if 0
	/*
	 * Examine the boot args string for options we need to know about
	 * now.
	 */
	process_kernel_args((char *)nwbootinfo.bt_args);
#else
	boothowto = RB_AUTOBOOT;
#endif
#ifdef RAMDISK_HOOKS
        boothowto |= RB_DFLTROOT;
#endif /* RAMDISK_HOOKS */


	{
		int processor_card_id;

		processor_card_id = 0x000f & 
			ioreg_read(LUBBOCK_OBIO_VBASE+LUBBOCK_MISCRD);
		switch(processor_card_id){
		case 0:
			/* Cotulla */
			memstart = 0xa0000000;
			memsize =  0x02000000; /* 32MB -phone */ 
			memsize =  0x04000000; /* 64MB */
			break;
		case 1:
			/* XXX: Sabiani */
			memstart = 0xa0000000;
			memsize = 0x04000000; /* 64MB */
			break;
		default:
			/* XXX: Unknown  */
			memstart = 0xa0000000;
			memsize = 0x04000000; /* 64MB */
		}
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

	LEDSTEP();

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

	/* Now we fill in the L2 pagetable for the kernel static code/data */
	{
		extern char etext[], _end[];
		size_t textsize = (u_int32_t) etext - KERNEL_TEXT_BASE;
		size_t totalsize = (u_int32_t) _end - KERNEL_TEXT_BASE;
		u_int logical;

		textsize = (textsize + PGOFSET) & ~PGOFSET;
		totalsize = (totalsize + PGOFSET) & ~PGOFSET;
		
		logical = 0x00200000;	/* offset of kernel in RAM */

		logical += pmap_map_chunk(l1pagetable, KERNEL_BASE + logical,
		    physical_start + logical, textsize,
		    VM_PROT_READ|VM_PROT_WRITE, PTE_CACHE);
		logical += pmap_map_chunk(l1pagetable, KERNEL_BASE + logical,
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
		extern char _end[];

		physical_freestart = physical_start +
		    (((((u_int32_t) _end) + PGOFSET) & ~PGOFSET) -
		     KERNEL_BASE);
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

	LEDSTEP();

	/* set new intc register address so that splfoo() doesn't
	   touch illegal address.  */
	pxa2x0_intr_bootstrap(LUBBOCK_INTCTL_VBASE);

	LEDSTEP();

	cpu_domains((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT);
	setttb(kernel_l1pt.pv_pa);
	cpu_tlb_flushID();
	cpu_domains(DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2));
	LEDSTEP();

	/*
	 * Moved from cpu_startup() as data_abort_handler() references
	 * this during uvm init
	 */
	proc0paddr = (struct user *)kernelstack.pv_va;
	proc0.p_addr = proc0paddr;

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
	LEDSTEP();
	pmap_bootstrap((pd_entry_t *)kernel_l1pt.pv_va, KERNEL_VM_BASE,
	    KERNEL_VM_BASE + KERNEL_VM_SIZE);
	LEDSTEP();

#ifdef __HAVE_MEMORY_DISK__
	md_root_setconf(memory_disk, sizeof memory_disk);
#endif

#if 0
	/* XXX - drahn */
	{
		uint16_t sw = ioreg16_read(LUBBOCK_OBIO_VBASE+LUBBOCK_USERSW);

		if (0 == (sw & (1<<13))) /* check S19 */
			boothowto |= RB_KDB;
		if (0 == (sw & (1<<12))) /* S20 */
			boothowto |= RB_SINGLE;
	}
#endif

	LEDSTEP();

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

#ifdef DDB
	db_machine_init();

	/* Firmware doesn't load symbols. */
	ddb_init();

	if (boothowto & RB_KDB)
		Debugger();
#endif

	pxa2x0_a4x_bs_tag.bs_map = map_func_save ;

	/* We return the new stack pointer address */
	return(kernelstack.pv_va + USPACE_SVC_STACK_TOP);
}

#if 0
void
process_kernel_args(char *args)
{

	boothowto = 0;

	/* Make a local copy of the bootargs */
	strncpy(bootargs, args, MAX_BOOT_STRING);

	args = bootargs;
	boot_file = bootargs;

	/* Skip the kernel image filename */
	while (*args != ' ' && *args != 0)
		++args;

	if (*args != 0)
		*args++ = 0;

	while (*args == ' ')
		++args;

	boot_args = args;

	printf("bootfile: %s\n", boot_file);
	printf("bootargs: %s\n", boot_args);

	parse_mi_bootargs(boot_args);
}
#endif

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
	static int consinit_called = 0;
	uint32_t ckenreg = ioreg_read(LUBBOCK_CLKMAN_VBASE+CLKMAN_CKEN);
#if 0
	char *console = CONSDEVNAME;
#endif

	if (consinit_called != 0)
		return;

	consinit_called = 1;

#if NCOM > 0

#ifdef FFUARTCONSOLE
	/* Check switch. */
	/*
	if (0 == (ioreg_read(LUBBOCK_OBIO_VBASE+LUBBOCK_USERSW) & (1<<15))) {
	*/
	if (0) {
		/* We don't use FF serial when S17=no-dot position */
	}
#ifdef KGDB
	else if (0 == strcmp(kgdb_devname, "ffuart")) {
		/* port is reserved for kgdb */
	}
#endif
	else if (0 == comcnattach(&pxa2x0_a4x_bs_tag, PXA2X0_FFUART_BASE, 
		     comcnspeed, PXA2X0_COM_FREQ, comcnmode)) {
#if 0
		/* XXX: can't call pxa2x0_clkman_config yet */
		pxa2x0_clkman_config(CKEN_FFUART, 1);
#else
		ioreg_write(LUBBOCK_CLKMAN_VBASE+CLKMAN_CKEN,
		    ckenreg|CKEN_FFUART);
#endif

		return;
	}
#endif /* FFUARTCONSOLE */

#ifdef BTUARTCONSOLE
#ifdef KGDB
	if (0 == strcmp(kgdb_devname, "btuart")) {
		/* port is reserved for kgdb */
	} else
#endif
	if (0 == comcnattach(&pxa2x0_a4x_bs_tag, PXA2X0_BTUART_BASE,
		comcnspeed, PXA2X0_COM_FREQ, comcnmode)) {
		ioreg_write(LUBBOCK_CLKMAN_VBASE+CLKMAN_CKEN,
		    ckenreg|CKEN_BTUART);
		return;
	}
#endif /* BTUARTCONSOLE */


#endif /* NCOM */

}

#ifdef KGDB
void
kgdb_port_init(void)
{
#if (NCOM > 0) && defined(COM_PXA2X0)
	paddr_t paddr = 0;
	enum pxa2x0_uart_id uart_id;
	uint32_t ckenreg = ioreg_read(LUBBOCK_CLKMAN_VBASE+CLKMAN_CKEN);

	if (0 == strcmp(kgdb_devname, "ffuart")) {
		paddr = PXA2X0_FFUART_BASE;
		clenreg |= CKEN_FFUART;
	}
	else if (0 == strcmp(kgdb_devname, "btuart")) {
		paddr = PXA2X0_BTUART_BASE;
		clenreg |= CKEN_BTUART;
	}

	if (paddr &&
	    0 == com_kgdb_attach_pxa2x0(&pxa2x0_a4x_bs_tag, paddr,
		kgdb_rate, PXA2X0_COM_FREQ, COM_TYPE_PXA2x0, comkgdbmode)) {

		ioreg_write(LUBBOCK_CLKMAN_VBASE+CLKMAN_CKEN, ckenreg);
	}
#endif
}
#endif

#if 0
/*
 * display a number in hex LED.
 * a digit is blank when the corresponding bit in arg blank is 1
 */
unsigned short led_control_value = 0;

void
hex_led_blank(uint32_t value, int blank)
{
	int save = disable_interrupts(I32_bit);

	ioreg_write(LUBBOCK_OBIO_VBASE+0x10, value);
	led_control_value = (led_control_value & 0xff)
		| ((blank & 0xff)<<8);
	ioreg_write(LUBBOCK_OBIO_VBASE+0x40, led_control_value);
	restore_interrupts(save);
}
#endif

/*
 * Cotulla's integrated ICU doesn't have IRQ0..7, so
 * we map software interrupts to bit 0..3
 */
#define SI_TO_IRQBIT(si)  (1U<<(si))

void
pxa2x0_setipl(int new)
{
	current_spl_level = new;
	intr_mask = pxa2x0_imask[current_spl_level];
	write_icu( SAIPIC_MR, intr_mask );
}


void
pxa2x0_splx(int new)
{
	int psw;

	psw = disable_interrupts(I32_bit);
	pxa2x0_setipl(new);
	restore_interrupts(psw);

	/* If there are software interrupts to process, do it. */
	if (softint_pending & intr_mask)
		pxa2x0_do_pending();
}


int
pxa2x0_splraise(int ipl)
{
	int	old, psw;

	old = current_spl_level;
	if( ipl > current_spl_level ){
		psw = disable_interrupts(I32_bit);
		pxa2x0_setipl(ipl);
		restore_interrupts(psw);
	}

	return (old);
}

int
pxa2x0_spllower(int ipl)
{
	int old = current_spl_level;
	int psw = disable_interrupts(I32_bit);
	pxa2x0_splx(ipl);
	restore_interrupts(psw);
	return(old);
}

void
pxa2x0_setsoftintr(int si)
{
	#if 0
	atomic_set_bit( (u_int *)&softint_pending, SI_TO_IRQBIT(si) );
	#else
		softint_pending |=  SI_TO_IRQBIT(si);
	#endif

	/* Process unmasked pending soft interrupts. */
	if ( softint_pending & intr_mask )
		pxa2x0_do_pending();
}

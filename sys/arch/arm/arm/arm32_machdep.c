/*	$OpenBSD: arm32_machdep.c,v 1.34 2010/06/27 03:03:48 thib Exp $	*/
/*	$NetBSD: arm32_machdep.c,v 1.42 2003/12/30 12:33:15 pk Exp $	*/

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
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
 * Machine dependant functions for kernel setup
 *
 * Created      : 17/09/94
 * Updated	: 18/04/01 updated for new wscons
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/reboot.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/buf.h>
#include <sys/msg.h>
#include <sys/msgbuf.h>
#include <sys/device.h>
#include <uvm/uvm.h>
#include <uvm/uvm_extern.h>
#include <sys/sysctl.h>

#include <dev/cons.h>

#include <arm/machdep.h>
#include <machine/bootconfig.h>
#include <machine/conf.h>

#ifdef CONF_HAVE_APM
#include "apm.h"
#else
#define NAPM	0
#endif
#include "rd.h"

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

extern int physmem;

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

#ifdef  BUFPAGES
int     bufpages = BUFPAGES;
#else
int     bufpages = 0;
#endif
int     bufcachepercent = BUFCACHEPERCENT;

struct uvm_constraint_range  dma_constraint = { 0x0, (paddr_t)-1 };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

int cold = 1;

pv_addr_t kernelstack;

/* the following is used externally (sysctl_hw) */
char	machine[] = MACHINE;		/* from <machine/param.h> */

/* Our exported CPU info; we can have only one. */
struct cpu_info cpu_info_store;

caddr_t	msgbufaddr;
extern paddr_t msgbufphys;

int kernel_debug = 0;

struct user *proc0paddr;

/* exported variable to be filled in by the bootloaders */
char *booted_kernel;

#ifdef APERTURE
#ifdef INSECURE
int allowaperture = 1;
#else
int allowaperture = 0;
#endif
#endif

#if defined(__zaurus__)
/* Permit console keyboard to do a nice halt. */
int kbd_reset;
int lid_suspend;
extern int xscale_maxspeed;
#endif

struct consdev *cn_tab;

/* Prototypes */

void data_abort_handler		(trapframe_t *frame);
void prefetch_abort_handler	(trapframe_t *frame);
extern void configure		(void);

/*
 * arm32_vector_init:
 *
 *	Initialize the vector page, and select whether or not to
 *	relocate the vectors.
 *
 *	NOTE: We expect the vector page to be mapped at its expected
 *	destination.
 */
void
arm32_vector_init(vaddr_t va, int which)
{
	extern unsigned int page0[], page0_data[];
	unsigned int *vectors = (int *) va;
	unsigned int *vectors_data = vectors + (page0_data - page0);
	int vec;

	/*
	 * Loop through the vectors we're taking over, and copy the
	 * vector's insn and data word.
	 */
	for (vec = 0; vec < ARM_NVEC; vec++) {
		if ((which & (1 << vec)) == 0) {
			/* Don't want to take over this vector. */
			continue;
		}
		vectors[vec] = page0[vec];
		vectors_data[vec] = page0_data[vec];
	}

	/* Now sync the vectors. */
	cpu_icache_sync_range(va, (ARM_NVEC * 2) * sizeof(u_int));

	vector_page = va;

	if (va == ARM_VECTORS_HIGH) {
		/*
		 * Assume the MD caller knows what it's doing here, and
		 * really does want the vector page relocated.
		 *
		 * Note: This has to be done here (and not just in
		 * cpu_setup()) because the vector page needs to be
		 * accessible *before* cpu_startup() is called.
		 * Think ddb(9) ...
		 *
		 * NOTE: If the CPU control register is not readable,
		 * this will totally fail!  We'll just assume that
		 * any system that has high vector support has a
		 * readable CPU control register, for now.  If we
		 * ever encounter one that does not, we'll have to
		 * rethink this.
		 */
		cpu_control(CPU_CONTROL_VECRELOC, CPU_CONTROL_VECRELOC);
	}
}

/*
 * Debug function just to park the CPU
 */

void
halt()
{
	while (1)
		cpu_sleep(0);
}


/* Sync the discs and unmount the filesystems */

void
bootsync(int howto)
{
	static int bootsyncdone = 0;

	if (bootsyncdone) return;

	bootsyncdone = 1;

	/* Make sure we can still manage to do things */
	if (__get_cpsr() & I32_bit) {
		/*
		 * If we get here then boot has been called without RB_NOSYNC
		 * and interrupts were disabled. This means the boot() call
		 * did not come from a user process e.g. shutdown, but must
		 * have come from somewhere in the kernel.
		 */
		IRQenable;
		printf("Warning IRQ's disabled during boot()\n");
	}

	vfs_shutdown();

	/*
	 * If we've been adjusting the clock, the todr
	 * will be out of synch; adjust it now unless
	 * the system has been sitting in ddb.
	 */
	if ((howto & RB_TIMEBAD) == 0) {
		resettodr();
	} else {
		printf("WARNING: not updating battery clock\n");
	}
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
	u_int loop;
	paddr_t minaddr;
	paddr_t maxaddr;

	proc0paddr = (struct user *)kernelstack.pv_va;
	proc0.p_addr = proc0paddr;

	/* Set the cpu control register */
	cpu_setup(boot_args);

	/* Lock down zero page */
	vector_page_setprot(VM_PROT_READ);

	/*
	 * Give pmap a chance to set up a few more things now the vm
	 * is initialised
	 */
	pmap_postinit();

	/*
	 * Allow per-board specific initialization
	 */
	board_startup();

	/*
	 * Initialize error message buffer (at end of core).
	 */

	/* msgbufphys was setup during the secondary boot strap */
	for (loop = 0; loop < atop(MSGBUFSIZE); ++loop)
		pmap_kenter_pa((vaddr_t)msgbufaddr + loop * PAGE_SIZE,
		    msgbufphys + loop * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE);
	pmap_update(pmap_kernel());
	initmsgbuf(msgbufaddr, round_page(MSGBUFSIZE));

	/*
	 * Identify ourselves for the msgbuf (everything printed earlier will
	 * not be buffered).
	 */
	printf(version);

	printf("real mem  = %u (%uMB)\n", ptoa(physmem),
	    ptoa(physmem)/1024/1024);

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
				   VM_PHYS_SIZE, 0, FALSE, NULL);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit(); 

	printf("avail mem = %lu (%uMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free)/1024/1024);

	curpcb = &proc0.p_addr->u_pcb;
	curpcb->pcb_flags = 0;
	curpcb->pcb_un.un_32.pcb32_und_sp = (u_int)proc0.p_addr +
	    USPACE_UNDEF_STACK_TOP;
	curpcb->pcb_un.un_32.pcb32_sp = (u_int)proc0.p_addr +
	    USPACE_SVC_STACK_TOP;
	pmap_set_pcb_pagedir(pmap_kernel(), curpcb);

        curpcb->pcb_tf = (struct trapframe *)curpcb->pcb_un.un_32.pcb32_sp - 1;
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
#if NAPM > 0
	extern int cpu_apmwarn;
#endif

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_DEBUG:
		return(sysctl_int(oldp, oldlenp, newp, newlen, &kernel_debug));

	case CPU_CONSDEV: {
		dev_t consdev;
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
			sizeof consdev));
	}
	case CPU_BOOTED_KERNEL: {
		if (booted_kernel != NULL && booted_kernel[0] != '\0')
			return sysctl_rdstring(oldp, oldlenp, newp,
			    booted_kernel);
		return (EOPNOTSUPP);
	}

	case CPU_ALLOWAPERTURE:
#ifdef APERTURE
		if (securelevel > 0)
			return (sysctl_int_lower(oldp, oldlenp, newp, newlen,
			    &allowaperture));
		else
			return (sysctl_int(oldp, oldlenp, newp, newlen,
			    &allowaperture));
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif

#if NAPM > 0
	case CPU_APMWARN:
		return (sysctl_int(oldp, oldlenp, newp, newlen, &cpu_apmwarn));
#endif
#if defined(__zaurus__)
	case CPU_KBDRESET:
		if (securelevel > 0)
			return (sysctl_rdint(oldp, oldlenp, newp,
			    kbd_reset));
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &kbd_reset));
	case CPU_LIDSUSPEND:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &lid_suspend));
	case CPU_MAXSPEED:
	{
		extern void pxa2x0_maxspeed(int *);
		int err = EINVAL;

		if (!newp && newlen == 0)
			return (sysctl_int(oldp, oldlenp, 0, 0,
			    &xscale_maxspeed));
		err = (sysctl_int(oldp, oldlenp, newp, newlen,
		    &xscale_maxspeed));
		pxa2x0_maxspeed(&xscale_maxspeed);
		return err;
	}
#endif

	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

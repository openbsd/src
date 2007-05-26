/*	$OpenBSD: arm32_machdep.c,v 1.25 2007/05/26 20:56:49 drahn Exp $	*/
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
caddr_t allocsys(caddr_t);

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

#ifdef  BUFPAGES
int     bufpages = BUFPAGES;
#else
int     bufpages = 0;
#endif
int     bufcachepercent = BUFCACHEPERCENT;

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

/* Touch pad scaling disable flag and scaling parameters. */
extern int zts_rawmode;
struct ztsscale {
	int ts_minx;
	int ts_maxx;
	int ts_miny;
	int ts_maxy;
};
extern struct ztsscale zts_scale;
extern int xscale_maxspeed;
#endif

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
	caddr_t sysbase;
	caddr_t size;

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
	for (loop = 0; loop < btoc(MSGBUFSIZE); ++loop)
		pmap_kenter_pa((vaddr_t)msgbufaddr + loop * PAGE_SIZE,
		    msgbufphys + loop * PAGE_SIZE, VM_PROT_READ|VM_PROT_WRITE);
	pmap_update(pmap_kernel());
	initmsgbuf(msgbufaddr, round_page(MSGBUFSIZE));

	/*
	 * Look at arguments passed to us and compute boothowto.
	 * Default to SINGLE and ASKNAME if no args or
	 * SINGLE and DFLTROOT if this is a ramdisk kernel.
	 */
#ifdef RAMDISK_HOOKS
	boothowto = RB_SINGLE | RB_DFLTROOT;
#endif /* RAMDISK_HOOKS */

	/*
	 * Identify ourselves for the msgbuf (everything printed earlier will
	 * not be buffered).
	 */
	printf(version);

	printf("real mem  = %u (%uMB)\n", ctob(physmem),
	    ctob(physmem)/1024/1024);

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	size = allocsys(NULL);
	sysbase = (caddr_t)uvm_km_zalloc(kernel_map, round_page((vaddr_t)size));
	if (sysbase == 0)
		panic(
		    "cpu_startup: no room for system tables; %d bytes required",
		    (u_int)size);
	if ((caddr_t)((allocsys(sysbase) - sysbase)) != size)
		panic("cpu_startup: system table size inconsistency");

	/*
	 * Determine how many buffers to allocate.
	 * We allocate bufcachepercent% of memory for buffer space.
	 */
	if (bufpages == 0)
		bufpages = physmem * bufcachepercent / 100;

	/* Restrict to at most 25% filled kvm */
	if (bufpages >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) / PAGE_SIZE / 4) 
		bufpages = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    PAGE_SIZE / 4;

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
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
#include "zts.h"
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
		
	case CPU_ZTSRAWMODE:
#if NZTS > 0
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &zts_rawmode));
#else
		return (EINVAL);
#endif /* NZTS > 0 */
	case CPU_ZTSSCALE:
	{
		int err = EINVAL;
#if NZTS > 0
		struct ztsscale *p = newp;
		struct ztsscale ts;
		int s;

		if (!newp && newlen == 0)
			return (sysctl_struct(oldp, oldlenp, 0, 0,
			    &zts_scale, sizeof zts_scale));

		if (!(newlen == sizeof zts_scale &&
		    p->ts_minx < p->ts_maxx && p->ts_miny < p->ts_maxy &&
		    p->ts_minx >= 0 && p->ts_maxx >= 0 &&
		    p->ts_miny >= 0 && p->ts_maxy >= 0 &&
		    p->ts_minx < 32768 && p->ts_maxx < 32768 &&
		    p->ts_miny < 32768 && p->ts_maxy < 32768))
			return (EINVAL);

		ts = zts_scale;
		err = sysctl_struct(oldp, oldlenp, newp, newlen,
		    &ts, sizeof ts);
		if (err == 0) {
			s = splhigh();
			zts_scale = ts;
			splx(s);
		}
#endif /* NZTS > 0 */
		return (err);
	}
#endif

	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
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
allocsys(caddr_t v)
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))

#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

	return v;
}

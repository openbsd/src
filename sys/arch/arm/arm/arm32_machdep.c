/*	$OpenBSD: arm32_machdep.c,v 1.7 2005/01/24 22:20:32 uwe Exp $	*/
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

#include <arm/katelib.h>
#include <arm/machdep.h>
#include <machine/bootconfig.h>

#include "rd.h"

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

extern int physmem;
caddr_t allocsys(caddr_t);

#ifdef  NBUF
int     nbuf = NBUF;
#else
int     nbuf = 0;
#endif

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

#if NMD > 0 && defined(MEMORY_DISK_HOOKS) && !defined(MEMORY_DISK_ROOT_SIZE)
extern size_t md_root_size;		/* Memory disc size */
#endif	/* NMD && MEMORY_DISK_HOOKS && !MEMORY_DISK_ROOT_SIZE */

pv_addr_t kernelstack;

/* the following is used externally (sysctl_hw) */
char	machine[] = MACHINE;		/* from <machine/param.h> */
char	machine_arch[] = MACHINE_ARCH;	/* from <machine/param.h> */

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
bootsync(void)
{
	static int bootsyncdone = 0;

	if (bootsyncdone) return;

	bootsyncdone = 1;

	/* Make sure we can still manage to do things */
	if (GetCPSR() & I32_bit) {
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
	vsize_t bufsize;
	int base, residual;

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

	printf("real mem  = %u (%uK)\n", ctob(physmem), ctob(physmem)/1024);


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
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	bufsize = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *)&buffers, round_page(bufsize),
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	    UVM_ADV_NORMAL, 0)) != 0)
		panic("cpu_startup: cannot allocate UVM space for buffers");
	minaddr = (vaddr_t)buffers;
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}

	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (loop = 0; loop < nbuf; ++loop) {
		vsize_t curbufsize;
		vaddr_t curbuf;
		struct vm_page *pg;

		/*
		 * Each buffer has MAXBSIZE bytes of VM space allocated.  Of
		 * that MAXBSIZE space, we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vaddr_t) buffers + (loop * MAXBSIZE);
		curbufsize = NBPG * ((loop < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("cpu_startup: not enough memory for buffer cache");
			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
				VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	pmap_update(pmap_kernel());

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

	printf("avail mem = %lu (%uK)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free)/1024);
	printf("using %d buffers containing %u bytes (%uK) of memory\n",
	    nbuf, bufpages * PAGE_SIZE, bufpages * PAGE_SIZE / 1024);

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
	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
	case CPU_DEBUG:
		return(sysctl_int(oldp, oldlenp, newp, newlen, &kernel_debug));

#if 0
	case CPU_BOOTED_DEVICE:
		if (booted_device != NULL)
			return (sysctl_rdstring(oldp, oldlenp, newp,
			    booted_device->dv_xname));
		return (EOPNOTSUPP);
#endif

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
			return (sysctl_rdint(oldp, oldlenp, newp,
			    allowaperture));
		else
			return (sysctl_int(oldp, oldlenp, newp, newlen,
			    &allowaperture));
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

#if 0
/*
 * machine dependent system variables.
 */
static int
sysctl_machdep_booted_device(SYSCTLFN_ARGS)
{
	struct sysctlnode node;

	if (booted_device == NULL)
		return (EOPNOTSUPP);

	node = *rnode;
	node.sysctl_data = booted_device->dv_xname;
	node.sysctl_size = strlen(booted_device->dv_xname) + 1;
	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}

static int
sysctl_machdep_booted_kernel(SYSCTLFN_ARGS)
{
	struct sysctlnode node;

	if (booted_kernel == NULL || booted_kernel[0] == '\0')
		return (EOPNOTSUPP);

	node = *rnode;
	node.sysctl_data = booted_kernel;
	node.sysctl_size = strlen(booted_kernel) + 1;
	return (sysctl_lookup(SYSCTLFN_CALL(&node)));
}

static int
sysctl_machdep_powersave(SYSCTLFN_ARGS)
{
	struct sysctlnode node = *rnode;
	int error, newval;

	newval = cpu_do_powersave;
	node.sysctl_data = &newval;
	if (cpufuncs.cf_sleep == (void *) cpufunc_nullop)
		node.sysctl_flags &= ~SYSCTL_READWRITE;
	error = sysctl_lookup(SYSCTLFN_CALL(&node));
	if (error || newp == NULL || newval == cpu_do_powersave)
		return (error);

	if (newval < 0 || newval > 1)
		return (EINVAL);
	cpu_do_powersave = newval;

	return (0);
}

SYSCTL_SETUP(sysctl_machdep_setup, "sysctl machdep subtree setup")
{

	sysctl_createv(SYSCTL_PERMANENT,
		       CTLTYPE_NODE, "machdep", NULL,
		       NULL, 0, NULL, 0,
		       CTL_MACHDEP, CTL_EOL);

	sysctl_createv(SYSCTL_PERMANENT|SYSCTL_READWRITE,
		       CTLTYPE_INT, "debug", NULL,
		       NULL, 0, &kernel_debug, 0,
		       CTL_MACHDEP, CPU_DEBUG, CTL_EOL);
	sysctl_createv(SYSCTL_PERMANENT,
		       CTLTYPE_STRING, "booted_device", NULL,
		       sysctl_machdep_booted_device, 0, NULL, 0,
		       CTL_MACHDEP, CPU_BOOTED_DEVICE, CTL_EOL);
	sysctl_createv(SYSCTL_PERMANENT,
		       CTLTYPE_STRING, "booted_kernel", NULL,
		       sysctl_machdep_booted_kernel, 0, NULL, 0,
		       CTL_MACHDEP, CPU_BOOTED_KERNEL, CTL_EOL);
	sysctl_createv(SYSCTL_PERMANENT,
		       CTLTYPE_STRUCT, "console_device", NULL,
		       sysctl_consdev, 0, NULL, sizeof(dev_t),
		       CTL_MACHDEP, CPU_CONSDEV, CTL_EOL);
	sysctl_createv(SYSCTL_PERMANENT|SYSCTL_READWRITE,
		       CTLTYPE_INT, "powersave", NULL,
		       sysctl_machdep_powersave, 0, &cpu_do_powersave, 0,
		       CTL_MACHDEP, CPU_POWERSAVE, CTL_EOL);
}
#endif

#if 1
void
parse_mi_bootargs(args)
	char *args;
{
	int integer;

	if (get_bootconf_option(args, "single", BOOTOPT_TYPE_BOOLEAN, &integer)
	    || get_bootconf_option(args, "-s", BOOTOPT_TYPE_BOOLEAN, &integer))
		if (integer)
			boothowto |= RB_SINGLE;
	if (get_bootconf_option(args, "kdb", BOOTOPT_TYPE_BOOLEAN, &integer)
	    || get_bootconf_option(args, "-k", BOOTOPT_TYPE_BOOLEAN, &integer))
		if (integer)
			boothowto |= RB_KDB;
	if (get_bootconf_option(args, "ask", BOOTOPT_TYPE_BOOLEAN, &integer)
	    || get_bootconf_option(args, "-a", BOOTOPT_TYPE_BOOLEAN, &integer))
		if (integer)
			boothowto |= RB_ASKNAME;

#ifdef PMAP_DEBUG
	if (get_bootconf_option(args, "pmapdebug", BOOTOPT_TYPE_INT, &integer)) {
		pmap_debug_level = integer;
		pmap_debug(pmap_debug_level);
	}
#endif	/* PMAP_DEBUG */

/*	if (get_bootconf_option(args, "nbuf", BOOTOPT_TYPE_INT, &integer))
		bufpages = integer;*/

}
#endif

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
	/*
	 * Determine how many buffers to allocate.  We use 10% of the
	 * first 2MB of memory, and 5% of the rest, with a minimum of 16
	 * buffers.  We allocate 1/2 as many swap buffer headers as file
	 * i/o buffers.
	 */
	if (bufpages == 0) {
		if (physmem < btoc(2 * 1024 * 1024))
			bufpages = physmem / 10;
		else
			bufpages = (btoc(2 * 1024 * 1024) + physmem) *
			    bufcachepercent / 100;
	}
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

	/* Restrict to at most 35% filled kvm */
	/* XXX - This needs UBC... */
	if (nbuf >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) / MAXBSIZE * 35 / 100) 
		nbuf = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 35 / 100;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / PAGE_SIZE)
		bufpages = nbuf * MAXBSIZE / PAGE_SIZE;

	valloc(buf, struct buf, nbuf);
	return v;
}

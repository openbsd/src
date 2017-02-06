/* $OpenBSD: machdep.c,v 1.10 2017/02/06 19:23:45 patrick Exp $ */
/*
 * Copyright (c) 2014 Patrick Wildt <patrick@blueri.se>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/timetc.h>
#include <sys/sched.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/reboot.h>
#include <sys/mount.h>
#include <sys/exec.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/kcore.h>
#include <sys/core.h>
#include <sys/msgbuf.h>
#include <sys/buf.h>
#include <sys/termios.h>

#include <net/if.h>
#include <uvm/uvm.h>
#include <dev/cons.h>
#include <dev/clock_subr.h>
#include <dev/ofw/fdt.h>
#include <dev/ofw/openfirm.h>
#include <machine/param.h>
#include <machine/kcore.h>
#include <machine/bootconfig.h>
#include <machine/bus.h>
#include <arm64/arm64/arm64var.h>

#include <machine/db_machdep.h>
#include <ddb/db_extern.h>

char *boot_args = NULL;
char *boot_file = "";

extern uint64_t esym;

int stdout_node = 0;

void (*cpuresetfn)(void);
void (*powerdownfn)(void);

int cold = 1;

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;

int physmem;

struct consdev *cn_tab;

caddr_t msgbufaddr;
paddr_t msgbufphys;

struct user *proc0paddr;

struct uvm_constraint_range  dma_constraint = { 0x0, (paddr_t)-1 };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

/* the following is used externally (sysctl_hw) */
char    machine[] = MACHINE;            /* from <machine/param.h> */
char    cpu_model[] = "arm64"; // XXX FIX
extern todr_chip_handle_t todr_handle;

int safepri = 0;

struct cpu_info cpu_info_primary;
struct cpu_info *cpu_info[MAXCPUS] = { &cpu_info_primary };

/*
 * inittodr:
 *
 *      Initialize time from the time-of-day register.
 */
#define MINYEAR         2003    /* minimum plausible year */
void
inittodr(time_t base)
{
	time_t deltat;
	struct timeval rtctime;
	struct timespec ts;
	int badbase;

	if (base < (MINYEAR - 1970) * SECYR) {
		printf("WARNING: preposterous time in file system\n");
		/* read the system clock anyway */
		base = (MINYEAR - 1970) * SECYR;
		badbase = 1;
	} else
		badbase = 0;

	if (todr_handle == NULL ||
	    todr_gettime(todr_handle, &rtctime) != 0 ||
	    rtctime.tv_sec == 0) {
		/*
		 * Believe the time in the file system for lack of
		 * anything better, resetting the TODR.
		 */
		rtctime.tv_sec = base;
		rtctime.tv_usec = 0;
		if (todr_handle != NULL && !badbase) {
			printf("WARNING: preposterous clock chip time\n");
			resettodr();
		}
		ts.tv_sec = rtctime.tv_sec;
		ts.tv_nsec = rtctime.tv_usec * 1000;
		tc_setclock(&ts);
		goto bad;
	} else {
		ts.tv_sec = rtctime.tv_sec;
		ts.tv_nsec = rtctime.tv_usec * 1000;
		tc_setclock(&ts);
	}

	if (!badbase) {
		/*
		 * See if we gained/lost two or more days; if
		 * so, assume something is amiss.
		 */
		deltat = rtctime.tv_sec - base;
		if (deltat < 0)
			deltat = -deltat;
		if (deltat < 2 * SECDAY)
			return;         /* all is well */
		printf("WARNING: clock %s %ld days\n",
		    rtctime.tv_sec < base ? "lost" : "gained",
		    (long)deltat / SECDAY);
	}
 bad:
	printf("WARNING: CHECK AND RESET THE DATE!\n");
}

void *
fdt_find_cons(const char *name)
{
	char *alias = "serial0";
	char buf[128];
	char *stdout = NULL;
	char *p;
	void *node;

	/* First check if "stdout-path" is set. */
	node = fdt_find_node("/chosen");
	if (node) {
		if (fdt_node_property(node, "stdout-path", &stdout) > 0) {
			if (strchr(stdout, ':') != NULL) {
				strlcpy(buf, stdout, sizeof(buf));
				if ((p = strchr(buf, ':')) != NULL)
					*p = '\0';
				stdout = buf;
			}
			if (stdout[0] != '/') {
				/* It's an alias. */
				alias = stdout;
				stdout = NULL;
			}
		}
	}

	/* Perform alias lookup if necessary. */
	if (stdout == NULL) {
		node = fdt_find_node("/aliases");
		if (node)
			fdt_node_property(node, alias, &stdout);
	}

	/* Lookup the physical address of the interface. */
	if (stdout) {
		node = fdt_find_node(stdout);
		if (node && fdt_is_compatible(node, name)) {
			stdout_node = OF_finddevice(stdout);
			return (node);
		}
	}

	return (NULL);
}

extern void	com_fdt_init_cons(void);
extern void	pluart_init_cons(void);
void
consinit()
{
	static int consinit_called = 0;

	if (consinit_called != 0)
		return;

	consinit_called = 1;

	com_fdt_init_cons();
	pluart_init_cons();
}

void
cpu_idle_enter()
{
}

void
cpu_idle_cycle()
{
	restore_daif(0x0); // enable interrupts
	__asm volatile("wfi");
}

void
cpu_idle_leave()
{
}


// XXX what? - not really used
struct trapframe  proc0tf;
void
cpu_startup()
{

	u_int loop;
	paddr_t minaddr;
	paddr_t maxaddr;

	proc0.p_addr = proc0paddr;


	/*
	 * Initialize error message buffer (at end of core).
	 */

	/* msgbufphys was setup during the secondary boot strap */
	for (loop = 0; loop < atop(MSGBUFSIZE); ++loop)
		pmap_kenter_pa((vaddr_t)msgbufaddr + loop * PAGE_SIZE,
		    msgbufphys + loop * PAGE_SIZE, PROT_READ | PROT_WRITE);
	pmap_update(pmap_kernel());
	initmsgbuf(msgbufaddr, round_page(MSGBUFSIZE));

	/*
	 * Identify ourselves for the msgbuf (everything printed earlier will
	 * not be buffered).
	 */
	printf("%s", version);

	printf("real mem  = %lu (%luMB)\n", ptoa(physmem),
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

	printf("avail mem = %lu (%luMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free)/1024/1024);

	curpcb = &proc0.p_addr->u_pcb;
	curpcb->pcb_flags = 0;
	curpcb->pcb_tf = &proc0tf;

	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}
}

/*
 * machine dependent system variables.
 */

int
cpu_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);		/* overloaded */

	switch (name[0]) {
		// none supported currently
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

void bootsync(int);
void dumpsys(void);

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
		if (!TAILQ_EMPTY(&alldevs))
			config_suspend(TAILQ_FIRST(&alldevs), DVACT_POWERDOWN);
		if ((howto & (RB_HALT | RB_USERREQ)) != RB_USERREQ) {
			printf("The operating system has halted.\n");
			printf("Please press any key to reboot.\n\n");
			cngetc();
		}
		printf("rebooting...\n");
		delay(500000);
		if (cpuresetfn)
			(*cpuresetfn)();
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
		bootsync(howto);

	if_downall();

	uvm_shutdown();

	/* Say NO to interrupts */
	splhigh();

	/* Do a dump if requested. */
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
		dumpsys();

	/* Run any shutdown hooks */
	if (!TAILQ_EMPTY(&alldevs))
		config_suspend(TAILQ_FIRST(&alldevs), DVACT_POWERDOWN);

	/* Make sure IRQ's are disabled */
	// FIXME

	if (howto & RB_HALT) {
		if (howto & RB_POWERDOWN) {

			printf("\nAttempting to power down...\n");
			delay(500000);
			if (powerdownfn)
				(*powerdownfn)();
		}

		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	}

	printf("rebooting...\n");
	delay(500000);
	if (cpuresetfn)
		(*cpuresetfn)();
	printf("reboot failed; spinning\n");
	while(1);
	/*NOTREACHED*/
}

/* Sync the discs and unmount the filesystems */

void
bootsync(int howto)
{
	static int bootsyncdone = 0;

	if (bootsyncdone) return;

	bootsyncdone = 1;

#if 0
	/* Make sure we can still manage to do things */
	if (__get_daif() & I_bit) {
		/*
		 * If we get here then boot has been called without RB_NOSYNC
		 * and interrupts were disabled. This means the boot() call
		 * did not come from a user process e.g. shutdown, but must
		 * have come from somewhere in the kernel.
		 */
		IRQenable;
		printf("Warning IRQ's disabled during boot()\n");
	}
#endif

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

void
setregs(struct proc *p, struct exec_package *pack, u_long stack,
    register_t *retval)
{
	struct trapframe *tf;

	tf = p->p_addr->u_pcb.pcb_tf;

	memset (tf,0, sizeof(*tf));
	tf->tf_sp = stack;
	tf->tf_lr = pack->ep_entry;
	tf->tf_elr = pack->ep_entry; /* ??? */
	tf->tf_spsr = PSR_M_EL0t;
}

void
need_resched(struct cpu_info *ci)
{
	ci->ci_want_resched = 1;

	/* There's a risk we'll be called before the idle threads start */
	if (ci->ci_curproc) {
		aston(ci->ci_curproc);
		//cpu_kick(ci); /* multiprocessor only ?? */
	}
}

int	cpu_dumpsize(void);
u_long	cpu_dump_mempagecnt(void);

paddr_t dumpmem_paddr;
vaddr_t dumpmem_vaddr;
psize_t dumpmem_sz;

/*
 * These variables are needed by /sbin/savecore
 */
u_long	dumpmag = 0x8fca0101;	/* magic number */
int 	dumpsize = 0;		/* pages */
long	dumplo = 0; 		/* blocks */

/*
 * cpu_dump: dump the machine-dependent kernel core dump headers.
 */
int
cpu_dump(void)
{
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
	char buf[dbtob(1)];
	kcore_seg_t *segp;
	cpu_kcore_hdr_t *cpuhdrp;
	phys_ram_seg_t *memsegp;
	// caddr_t va;
	// int i;

	dump = bdevsw[major(dumpdev)].d_dump;

	memset(buf, 0, sizeof buf);
	segp = (kcore_seg_t *)buf;
	cpuhdrp = (cpu_kcore_hdr_t *)&buf[ALIGN(sizeof(*segp))];
	memsegp = (phys_ram_seg_t *)&buf[ALIGN(sizeof(*segp)) +
	    ALIGN(sizeof(*cpuhdrp))];

	/*
	 * Generate a segment header.
	 */
	CORE_SETMAGIC(*segp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	segp->c_size = dbtob(1) - ALIGN(sizeof(*segp));

	/*
	 * Add the machine-dependent header info.
	 */
	cpuhdrp->kernelbase = KERNEL_BASE;
	cpuhdrp->kerneloffs = 0; // XXX
	cpuhdrp->staticsize = 0; // XXX
	cpuhdrp->pmap_kernel_l1 = 0; // XXX
	cpuhdrp->pmap_kernel_l1 = 0; // XXX

#if 0
	/*
	 * Fill in the memory segment descriptors.
	 */
	for (i = 0; i < mem_cluster_cnt; i++) {
		memsegp[i].start = mem_clusters[i].start;
		memsegp[i].size = mem_clusters[i].size & PMAP_PA_MASK;
	}

	/*
	 * If we have dump memory then assume the kernel stack is in high
	 * memory and bounce
	 */
	if (dumpmem_vaddr != 0) {
		memcpy((char *)dumpmem_vaddr, buf, sizeof(buf));
		va = (caddr_t)dumpmem_vaddr;
	} else {
		va = (caddr_t)buf;
	}
	return (dump(dumpdev, dumplo, va, dbtob(1)));
#endif
	return ENOSYS;
}


/*
 * This is called by main to set dumplo and dumpsize.
 * Dumps always skip the first PAGE_SIZE of disk space
 * in case there might be a disk label stored there.
 * If there is extra space, put dump at the end to
 * reduce the chance that swapping trashes it.
 */
void
dumpconf(void)
{
	int nblks, dumpblks;	/* size of dump area */

	if (dumpdev == NODEV ||
	    (nblks = (bdevsw[major(dumpdev)].d_psize)(dumpdev)) == 0)
		return;
	if (nblks <= ctod(1))
		return;

	dumpblks = cpu_dumpsize();
	if (dumpblks < 0)
		return;
	dumpblks += ctod(cpu_dump_mempagecnt());

	/* If dump won't fit (incl. room for possible label), punt. */
	if (dumpblks > (nblks - ctod(1)))
		return;

	/* Put dump at end of partition */
	dumplo = nblks - dumpblks;

	/* dumpsize is in page units, and doesn't include headers. */
	dumpsize = cpu_dump_mempagecnt();
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
#define BYTES_PER_DUMP  MAXPHYS /* must be a multiple of pagesize */

void
dumpsys(void)
{
	u_long totalbytesleft, bytes, i, n, memseg;
	u_long maddr;
	daddr_t blkno;
	void *va;
	int (*dump)(dev_t, daddr_t, caddr_t, size_t);
	int error;

	/* Save registers. */
	// XXX
	//savectx(&dumppcb);

	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo <= 0 || dumpsize == 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);

#ifdef UVM_SWAP_ENCRYPT
	uvm_swap_finicrypt_all();
#endif

	error = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (error == -1) {
		printf("area unavailable\n");
		return;
	}

	if ((error = cpu_dump()) != 0)
		goto err;

	totalbytesleft = ptoa(cpu_dump_mempagecnt());
	blkno = dumplo + cpu_dumpsize();
	dump = bdevsw[major(dumpdev)].d_dump;
	error = 0;

	bytes = n = i = memseg = 0;
	maddr = 0;
	va = 0;
#if 0
	for (memseg = 0; memseg < mem_cluster_cnt; memseg++) {
		maddr = mem_clusters[memseg].start;
		bytes = mem_clusters[memseg].size;

		for (i = 0; i < bytes; i += n, totalbytesleft -= n) {
			/* Print out how many MBs we have left to go. */
			if ((totalbytesleft % (1024*1024)) < BYTES_PER_DUMP)
				printf("%ld ", totalbytesleft / (1024 * 1024));

			/* Limit size for next transfer. */
			n = bytes - i;
			if (n > BYTES_PER_DUMP)
				n = BYTES_PER_DUMP;
			if (maddr > 0xffffffff) {
				va = (void *)dumpmem_vaddr;
				if (n > dumpmem_sz)
					n = dumpmem_sz;
				memcpy(va, (void *)PMAP_DIRECT_MAP(maddr), n);
			} else {
				va = (void *)PMAP_DIRECT_MAP(maddr);
			}

			error = (*dump)(dumpdev, blkno, va, n);
			if (error)
				goto err;
			maddr += n;
			blkno += btodb(n);		/* XXX? */

#if 0	/* XXX this doesn't work.  grr. */
			/* operator aborting dump? */
			if (sget() != NULL) {
				error = EINTR;
				break;
			}
#endif
		}
	}
#endif

 err:
	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case EINTR:
		printf("aborted from console\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
	printf("\n\n");
	delay(5000000);		/* 5 seconds */
}


/// XXX ?
/*
 * Size of memory segments, before any memory is stolen.
 */
phys_ram_seg_t mem_clusters[VM_PHYSSEG_MAX];
int     mem_cluster_cnt;
/// XXX ?

/*
 * cpu_dumpsize: calculate size of machine-dependent kernel core dump headers.
 */
int
cpu_dumpsize(void)
{
	int size;

	size = ALIGN(sizeof(kcore_seg_t)) +
	    ALIGN(mem_cluster_cnt * sizeof(phys_ram_seg_t));
	if (roundup(size, dbtob(1)) != dbtob(1))
		return (-1);

	return (1);
}

u_long
cpu_dump_mempagecnt()
{
	return 0;
}


void
install_coproc_handler()
{
}

int64_t dcache_line_size;	/* The minimum D cache line size */
int64_t icache_line_size;	/* The minimum I cache line size */
int64_t idcache_line_size;	/* The minimum cache line size */
int64_t dczva_line_size;	/* The size of cache line the dc zva zeroes */

void
cache_setup(void)
{
	int dcache_line_shift, icache_line_shift, dczva_line_shift;
	uint32_t ctr_el0;
	uint32_t dczid_el0;

	ctr_el0 = READ_SPECIALREG(ctr_el0);

	/* Read the log2 words in each D cache line */
	dcache_line_shift = CTR_DLINE_SIZE(ctr_el0);
	/* Get the D cache line size */
	dcache_line_size = sizeof(int) << dcache_line_shift;

	/* And the same for the I cache */
	icache_line_shift = CTR_ILINE_SIZE(ctr_el0);
	icache_line_size = sizeof(int) << icache_line_shift;

	idcache_line_size = MIN(dcache_line_size, icache_line_size);

	dczid_el0 = READ_SPECIALREG(dczid_el0);

	/* Check if dc zva is not prohibited */
	if (dczid_el0 & DCZID_DZP)
		dczva_line_size = 0;
	else {
		/* Same as with above calculations */
		dczva_line_shift = DCZID_BS_SIZE(dczid_el0);
		dczva_line_size = sizeof(int) << dczva_line_shift;
	}
}

void	collect_kernel_args(char *);
void	process_kernel_args(void);

void
initarm(struct arm64_bootparams *abp)
{
	//struct efi_map_header *efihdr;
	vaddr_t vstart, vend;
	struct cpu_info *pcpup;
	long kvo = abp->kern_delta;
	//caddr_t kmdp;
	paddr_t memstart;
	psize_t memsize;
	void *config = abp->arg2;
	void *fdt = NULL;
	int (*map_func_save)(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);
	int (*map_a4x_func_save)(bus_space_tag_t, bus_addr_t, bus_size_t, int,
	    bus_space_handle_t *);

	// NOTE that 1GB of ram is mapped in by default in
	// the bootstrap memory config, so nothing is necessary
	// until pmap_bootstrap_finalize is called??
	if (!fdt_init(config) || fdt_get_size(config) == 0)
		panic("initarm: no FDT");

	struct fdt_reg reg;
	void *node;

	node = fdt_find_node("/chosen");
	if (node != NULL) {
		char *args, *duid;
		int len;

		len = fdt_node_property(node, "bootargs", &args);
		if (len > 0)
			collect_kernel_args(args);

		len = fdt_node_property(node, "openbsd,bootduid", &duid);
		if (len == sizeof(bootduid))
			memcpy(bootduid, duid, sizeof(bootduid));
	}

	node = fdt_find_node("/memory");
	if (node == NULL || fdt_get_reg(node, 0, &reg))
		panic("initarm: no memory specificed");

	memstart = reg.addr;
	memsize = reg.size;
	if (reg.size >  0x80000000)
		memsize = 0x80000000;

	/* Set the pcpu data, this is needed by pmap_bootstrap */
	// smp
	pcpup = &cpu_info_primary;

	/*
	 * Set the pcpu pointer with a backup in tpidr_el1 to be
	 * loaded when entering the kernel from userland.
	 */
	__asm __volatile(
	    "mov x18, %0 \n"
	    "msr tpidr_el1, %0" :: "r"(pcpup));

	cache_setup();

	{
	extern char bootargs[MAX_BOOT_STRING];
	printf("memsize %llx %llx bootargs [%s]\n", memstart, memsize, bootargs);
	}
	process_kernel_args();

	// XXX
	paddr_t pmap_steal_avail(size_t size, int align, void **kva);

	void _start(void);
	long kernbase = (long)&_start & ~0x00fff;

	/* Bootstrap enough of pmap  to enter the kernel proper */
	vstart = pmap_bootstrap(kvo, abp->kern_l1pt,
	    kernbase, esym,
	    memstart, memstart + memsize);

	// XX correctly sized?
	proc0paddr = (struct user *)abp->kern_stack;

	msgbufaddr = (caddr_t)vstart;
	msgbufphys = pmap_steal_avail(round_page(MSGBUFSIZE), PAGE_SIZE, NULL);
	vstart += round_page(MSGBUFSIZE);

	zero_page = vstart;
	vstart += PAGE_SIZE;
	copy_src_page = vstart;
	vstart += PAGE_SIZE;
	copy_dst_page = vstart;
	vstart += PAGE_SIZE;

	/*
	 * Allocate pages for an FDT copy.
	 */
	if (fdt_get_size(config) != 0) {
		uint32_t csize, size = round_page(fdt_get_size(config));
		vaddr_t va;

		paddr_t fpa =  pmap_steal_avail(size, PAGE_SIZE, NULL);
		memcpy((void*)fpa, config, size); // copy to physical address
		for (va = (vaddr_t)vstart, csize = size;
		    csize > 0;
		    csize -= PAGE_SIZE, va += PAGE_SIZE, fpa += PAGE_SIZE)
		{
		    pmap_kenter_cache(va, fpa, PROT_READ, PMAP_CACHE_WB);
		}

		fdt = (void *)vstart;
		vstart += size;
	}

	/*
	 * Managed KVM space is what we have claimed up to end of
	 * mapped kernel buffers.
	 */
	{
	// export back to pmap
	extern vaddr_t virtual_avail, virtual_end;
	virtual_avail = vstart;
	vend = VM_MAX_KERNEL_ADDRESS; // XXX
	virtual_end = vend;
	}

	/* Now we can reinit the FDT, using the virtual address. */
	if (fdt)
		fdt_init(fdt);

	// XXX
	int pmap_bootstrap_bs_map(bus_space_tag_t t, bus_addr_t bpa,
	    bus_size_t size, int flags, bus_space_handle_t *bshp);

	map_func_save = arm64_bs_tag._space_map;
	map_a4x_func_save = arm64_a4x_bs_tag._space_map;

	arm64_bs_tag._space_map = pmap_bootstrap_bs_map;
	arm64_a4x_bs_tag._space_map = pmap_bootstrap_bs_map;

	// cninit
	consinit();

	arm64_bs_tag._space_map = map_func_save;
	arm64_a4x_bs_tag._space_map = map_a4x_func_save;

	/* XXX */
	pmap_avail_fixup();

	uvmexp.pagesize = PAGE_SIZE;
	uvm_setpagesize();

	pmap_physload_avail();

#ifdef DDB
	db_machine_init();

	/* Firmware doesn't load symbols. */
	ddb_init();

	if (boothowto & RB_KDB)
		Debugger();
#endif

	softintr_init();
	splraise(IPL_IPI);
}

int comcnspeed = B115200;
char bootargs[MAX_BOOT_STRING];

void
collect_kernel_args(char *args)
{
	/* Make a local copy of the bootargs */
	strncpy(bootargs, args, MAX_BOOT_STRING - sizeof(int));
}
void
process_kernel_args(void)
{
	char *cp = bootargs;

	if (cp[0] == '\0') {
		boothowto = RB_AUTOBOOT;
		return;
	}

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
		case '1':
			comcnspeed = B115200;
			break;
		case '9':
			comcnspeed = B9600;
			break;
		default:
			printf("unknown option `%c'\n", *cp);
			break;
		}
		boothowto |= fl;
	}
}

/*
 * allow bootstrap to steal KVA after machdep has given it back to pmap.
 * XXX - need a mechanism to prevent this from being used too early or late.
 */
int
pmap_bootstrap_bs_map(bus_space_tag_t t, bus_addr_t bpa, bus_size_t size,
    int flags, bus_space_handle_t *bshp)
{
	u_long startpa, pa, endpa;
	vaddr_t va;

	extern vaddr_t virtual_avail, virtual_end;

	va = virtual_avail; // steal memory from virtual avail.

	if (va == 0)
		panic("pmap_bootstrap_bs_map, no virtual avail");

	startpa = trunc_page(bpa);
	endpa = round_page((bpa + size));

	*bshp = (bus_space_handle_t)(va + (bpa - startpa));

	for (pa = startpa; pa < endpa; pa += PAGE_SIZE, va += PAGE_SIZE)
		pmap_kenter_cache(va, pa, PROT_READ | PROT_WRITE,
		    PMAP_CACHE_CI);

	virtual_avail = va;

	return 0;
}

// debug function, not certain where this should go

void
dumpregs(struct trapframe *frame)
{
	int i;
	for (i = 0; i < 30; i+=2) {
		printf("x%02d: 0x%016llx 0x%016llx\n",
		    i, frame->tf_x[i], frame->tf_x[i+1]);
	}
	printf("sp: 0x%016llx\n", frame->tf_sp);
	printf("lr: 0x%016llx\n", frame->tf_lr);
	printf("pc: 0x%016llx\n", frame->tf_elr);
	printf("spsr: 0x%016llx\n", frame->tf_spsr);
}

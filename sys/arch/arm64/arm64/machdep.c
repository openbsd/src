/* $OpenBSD: machdep.c,v 1.36 2018/07/04 22:26:20 drahn Exp $ */
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
#include <machine/vfp.h>
#include <arm64/arm64/arm64var.h>

#include <machine/db_machdep.h>
#include <ddb/db_extern.h>

#include <dev/acpi/efi.h>

char *boot_args = NULL;
char *boot_file = "";

uint8_t *bootmac = NULL;

extern uint64_t esym;

int stdout_node;
int stdout_speed;

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

static int
atoi(const char *s)
{
	int n, neg;

	n = 0;
	neg = 0;

	while (*s == '-') {
		s++;
		neg = !neg;
	}

	while (*s != '\0') {
		if (*s < '0' || *s > '9')
			break;

		n = (10 * n) + (*s - '0');
		s++;
	}

	return (neg ? -n : n);
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
				if ((p = strchr(buf, ':')) != NULL) {
					*p++ = '\0';
					stdout_speed = atoi(p);
				}
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
extern void	imxuart_init_cons(void);
extern void	pluart_init_cons(void);
extern void	simplefb_init_cons(bus_space_tag_t);

void
consinit(void)
{
	static int consinit_called = 0;

	if (consinit_called != 0)
		return;

	consinit_called = 1;

	com_fdt_init_cons();
	imxuart_init_cons();
	pluart_init_cons();
	simplefb_init_cons(&arm64_bs_tag);
}

void
cpu_idle_enter()
{
}

void
cpu_idle_cycle()
{
	restore_daif(0x0); // enable interrupts
	__asm volatile("dsb sy");
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
	 * Give pmap a chance to set up a few more things now the vm
	 * is initialised
	 */
	pmap_postinit();

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

void dumpsys(void);

int	waittime = -1;

__dead void
boot(int howto)
{
	if (cold) {
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown(curproc);

		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}
	if_downall();

	uvm_shutdown();
	splhigh();
	cold = 1;

	if ((howto & RB_DUMP) != 0)
		dumpsys();

haltsys:
	config_suspend_all(DVACT_POWERDOWN);

	if ((howto & RB_HALT) != 0) {
		if ((howto & RB_POWERDOWN) != 0) {
			printf("\nAttempting to power down...\n");
			delay(500000);
			if (powerdownfn)
				(*powerdownfn)();
		}

		printf("\n");
		printf("The operating system has halted.\n");
		printf("Please press any key to reboot.\n\n");
		cngetc();
	}

	printf("rebooting...\n");
	delay(500000);
	if (cpuresetfn)
		(*cpuresetfn)();
	printf("reboot failed; spinning\n");
	for (;;)
		continue;
	/* NOTREACHED */
}

/* Sync the discs and unmount the filesystems */

void
setregs(struct proc *p, struct exec_package *pack, u_long stack,
    register_t *retval)
{
	struct trapframe *tf;

	/* If we were using the FPU, forget about it. */
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		vfp_discard(p);
	p->p_addr->u_pcb.pcb_flags &= ~PCB_FPU;

	tf = p->p_addr->u_pcb.pcb_tf;

	memset (tf,0, sizeof(*tf));
	tf->tf_sp = stack;
	tf->tf_lr = pack->ep_entry;
	tf->tf_elr = pack->ep_entry; /* ??? */
	tf->tf_spsr = PSR_M_EL0t;

	retval[1] = 0;
}

void
need_resched(struct cpu_info *ci)
{
	ci->ci_want_resched = 1;

	/* There's a risk we'll be called before the idle threads start */
	if (ci->ci_curproc) {
		aston(ci->ci_curproc);
		cpu_kick(ci);
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

uint64_t mmap_start;
uint32_t mmap_size;
uint32_t mmap_desc_size;
uint32_t mmap_desc_ver;

EFI_MEMORY_DESCRIPTOR *mmap;

void	remap_efi_runtime(EFI_PHYSICAL_ADDRESS);

void	collect_kernel_args(char *);
void	process_kernel_args(void);

void
initarm(struct arm64_bootparams *abp)
{
	vaddr_t vstart, vend;
	struct cpu_info *pcpup;
	long kvo = abp->kern_delta;
	//caddr_t kmdp;
	paddr_t memstart, memend;
	void *config = abp->arg2;
	void *fdt = NULL;
	EFI_PHYSICAL_ADDRESS system_table = 0;
	int (*map_func_save)(bus_space_tag_t, bus_addr_t, bus_size_t, int,
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
		char *prop;
		int len;
		static uint8_t lladdr[6];

		len = fdt_node_property(node, "bootargs", &prop);
		if (len > 0)
			collect_kernel_args(prop);

		len = fdt_node_property(node, "openbsd,bootduid", &prop);
		if (len == sizeof(bootduid))
			memcpy(bootduid, prop, sizeof(bootduid));

		len = fdt_node_property(node, "openbsd,bootmac", &prop);
		if (len == sizeof(lladdr)) {
			memcpy(lladdr, prop, sizeof(lladdr));
			bootmac = lladdr;
		}

		len = fdt_node_property(node, "openbsd,uefi-mmap-start", &prop);
		if (len == sizeof(mmap_start))
			mmap_start = bemtoh64((uint64_t *)prop);
		len = fdt_node_property(node, "openbsd,uefi-mmap-size", &prop);
		if (len == sizeof(mmap_size))
			mmap_size = bemtoh32((uint32_t *)prop);
		len = fdt_node_property(node, "openbsd,uefi-mmap-desc-size", &prop);
		if (len == sizeof(mmap_desc_size))
			mmap_desc_size = bemtoh32((uint32_t *)prop);
		len = fdt_node_property(node, "openbsd,uefi-mmap-desc-ver", &prop);
		if (len == sizeof(mmap_desc_ver))
			mmap_desc_ver = bemtoh32((uint32_t *)prop);

		len = fdt_node_property(node, "openbsd,uefi-system-table", &prop);
		if (len == sizeof(system_table))
			system_table = bemtoh64((uint64_t *)prop);
	}

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

	process_kernel_args();

	void _start(void);
	long kernbase = (long)&_start & ~0x00fff;

	/* The bootloader has loaded us into a 64MB block. */
	memstart = KERNBASE + kvo;
	memend = memstart + 64 * 1024 * 1024;

	/* Bootstrap enough of pmap to enter the kernel proper. */
	vstart = pmap_bootstrap(kvo, abp->kern_l1pt,
	    kernbase, esym, memstart, memend);

	// XX correctly sized?
	proc0paddr = (struct user *)abp->kern_stack;

	msgbufaddr = (caddr_t)vstart;
	msgbufphys = pmap_steal_avail(round_page(MSGBUFSIZE), PAGE_SIZE, NULL);
	vstart += round_page(MSGBUFSIZE);

	zero_page = vstart;
	vstart += MAXCPUS * PAGE_SIZE;
	copy_src_page = vstart;
	vstart += MAXCPUS * PAGE_SIZE;
	copy_dst_page = vstart;
	vstart += MAXCPUS * PAGE_SIZE;

	/* Relocate the FDT to safe memory. */
	if (fdt_get_size(config) != 0) {
		uint32_t csize, size = round_page(fdt_get_size(config));
		paddr_t pa;
		vaddr_t va;

		pa = pmap_steal_avail(size, PAGE_SIZE, NULL);
		memcpy((void *)pa, config, size); /* copy to physical */
		for (va = vstart, csize = size; csize > 0;
		    csize -= PAGE_SIZE, va += PAGE_SIZE, pa += PAGE_SIZE)
			pmap_kenter_cache(va, pa, PROT_READ, PMAP_CACHE_WB);

		fdt = (void *)vstart;
		vstart += size;
	}

	/* Relocate the EFI memory map too. */
	if (mmap_start != 0) {
		uint32_t csize, size = round_page(mmap_size);
		paddr_t pa, startpa, endpa;
		vaddr_t va;

		startpa = trunc_page(mmap_start);
		endpa = round_page(mmap_start + mmap_size);
		for (pa = startpa, va = vstart; pa < endpa;
		    pa += PAGE_SIZE, va += PAGE_SIZE)
			pmap_kenter_cache(va, pa, PROT_READ, PMAP_CACHE_WB);
		pa = pmap_steal_avail(size, PAGE_SIZE, NULL);
		memcpy((void *)pa, (caddr_t)vstart + (mmap_start - startpa),
		    mmap_size); /* copy to physical */
		pmap_kremove(vstart, endpa - startpa);

		for (va = vstart, csize = size; csize > 0;
		    csize -= PAGE_SIZE, va += PAGE_SIZE, pa += PAGE_SIZE)
			pmap_kenter_cache(va, pa, PROT_READ | PROT_WRITE, PMAP_CACHE_WB);

		mmap = (void *)vstart;
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
	arm64_bs_tag._space_map = pmap_bootstrap_bs_map;

	// cninit
	consinit();

	arm64_bs_tag._space_map = map_func_save;

	/* Remap EFI runtime. */
	if (mmap_start != 0 && system_table != 0)
		remap_efi_runtime(system_table);

	/* XXX */
	pmap_avail_fixup();

	uvmexp.pagesize = PAGE_SIZE;
	uvm_setpagesize();

	/* Make what's left of the initial 64MB block available to UVM. */
	pmap_physload_avail();

	/* Make all other physical memory available to UVM. */
	if (mmap && mmap_desc_ver == EFI_MEMORY_DESCRIPTOR_VERSION) {
		EFI_MEMORY_DESCRIPTOR *desc = mmap;
		int i;

		/*
		 * Load all memory marked as EfiConventionalMemory.
		 * Don't bother with blocks smaller than 64KB.  The
		 * initial 64MB memory block should be marked as
		 * EfiLoaderData so it won't be added again here.
		 */
		for (i = 0; i < mmap_size / mmap_desc_size; i++) {
			printf("type 0x%x pa 0x%llx va 0x%llx pages 0x%llx attr 0x%llx\n",
			    desc->Type, desc->PhysicalStart,
			    desc->VirtualStart, desc->NumberOfPages,
			    desc->Attribute);
			if (desc->Type == EfiConventionalMemory &&
			    desc->NumberOfPages >= 16) {
				uvm_page_physload(atop(desc->PhysicalStart),
				    atop(desc->PhysicalStart) +
				    desc->NumberOfPages,
				    atop(desc->PhysicalStart),
				    atop(desc->PhysicalStart) +
				    desc->NumberOfPages, 0);
				physmem += desc->NumberOfPages;
			}
			desc = NextMemoryDescriptor(desc, mmap_desc_size);
		}
	} else {
		paddr_t start, end;
		int i;

		node = fdt_find_node("/memory");
		if (node == NULL)
			panic("%s: no memory specified", __func__);

		for (i = 0; i < VM_PHYSSEG_MAX; i++) {
			if (fdt_get_reg(node, i, &reg))
				break;
			if (reg.size == 0)
				continue;

			start = reg.addr;
			end = MIN(reg.addr + reg.size, (paddr_t)-PAGE_SIZE);

			/*
			 * The intial 64MB block is not excluded, so we need
			 * to make sure we don't add it here.
			 */
			if (start < memend && end > memstart) {
				if (start < memstart) {
					uvm_page_physload(atop(start),
					    atop(memstart), atop(start),
					    atop(memstart), 0);
					physmem += atop(memstart - start);
				}
				if (end > memend) {
					uvm_page_physload(atop(memend),
					    atop(end), atop(memend),
					    atop(end), 0);
					physmem += atop(end - memend);
				}
			} else {
				uvm_page_physload(atop(start), atop(end),
				    atop(start), atop(end), 0);
				physmem += atop(end - start);
			}
		}
	}

#ifdef DDB
	db_machine_init();

	/* Firmware doesn't load symbols. */
	ddb_init();

	if (boothowto & RB_KDB)
		db_enter();
#endif

	softintr_init();
	splraise(IPL_IPI);
}

void
remap_efi_runtime(EFI_PHYSICAL_ADDRESS system_table)
{
	EFI_SYSTEM_TABLE *st = (EFI_SYSTEM_TABLE *)system_table;
	EFI_RUNTIME_SERVICES *rs;
	EFI_STATUS status;
	EFI_MEMORY_DESCRIPTOR *src;
	EFI_MEMORY_DESCRIPTOR *dst;
	EFI_PHYSICAL_ADDRESS phys_start = ~0ULL;
	EFI_PHYSICAL_ADDRESS phys_end = 0;
	EFI_VIRTUAL_ADDRESS virt_start;
	vsize_t space;
	int i, count = 0;
	paddr_t pa;

	/*
	 * Pick a random address somewhere in the lower half of the
	 * usable virtual address space.
	 */
	space = 3 * (VM_MAX_ADDRESS - VM_MIN_ADDRESS) / 4;
	virt_start = VM_MIN_ADDRESS +
	    ((vsize_t)arc4random_uniform(space >> PAGE_SHIFT) << PAGE_SHIFT);

	/* Make sure the EFI system table is mapped. */
	pmap_map_early(system_table, sizeof(EFI_SYSTEM_TABLE));
	rs = st->RuntimeServices;

	/*
	 * Make sure memory for EFI runtime services is mapped.  We
	 * only map normal memory at this point and pray that the
	 * SetVirtualAddressMap call doesn't need anything else.
	 */
	src = mmap;
	for (i = 0; i < mmap_size / mmap_desc_size; i++) {
		if (src->Attribute & EFI_MEMORY_RUNTIME) {
			if (src->Attribute & EFI_MEMORY_WB) {
				pmap_map_early(src->PhysicalStart,
				    src->NumberOfPages * PAGE_SIZE);
				phys_start = MIN(phys_start,
				    src->PhysicalStart);
				phys_end = MAX(phys_end, src->PhysicalStart +
				    src->NumberOfPages * PAGE_SIZE);
			}
			count++;
		}
		src = NextMemoryDescriptor(src, mmap_desc_size);
	}

	/* Allocate memory descriptors for new mappings. */
	pa = pmap_steal_avail(count * mmap_desc_size,
	    mmap_desc_size, NULL);
	memset((void *)pa, 0, count * mmap_desc_size);

	/*
	 * Establish new mappings.  Apparently some EFI code relies on
	 * the offset between code and data remaining the same so pick
	 * virtual addresses for normal memory that meet that
	 * constraint.  Other mappings are simply tagged to the end of
	 * the last normal memory mapping.
	 */
	src = mmap;
	dst = (EFI_MEMORY_DESCRIPTOR *)pa;
	for (i = 0; i < mmap_size / mmap_desc_size; i++) {
		if (src->Attribute & EFI_MEMORY_RUNTIME) {
			if (src->Attribute & EFI_MEMORY_WB) {
				src->VirtualStart = virt_start +
				    (src->PhysicalStart - phys_start);
			} else {
				src->VirtualStart = virt_start +
				     (phys_end - phys_start);
				phys_end += src->NumberOfPages * PAGE_SIZE;
			}
			memcpy(dst, src, mmap_desc_size);
			dst = NextMemoryDescriptor(dst, mmap_desc_size);
		}
		src = NextMemoryDescriptor(src, mmap_desc_size);
	}

	/* Install new mappings. */
	dst = (EFI_MEMORY_DESCRIPTOR *)pa;
	status = rs->SetVirtualAddressMap(count * mmap_desc_size,
	    mmap_desc_size, mmap_desc_ver, dst);
	if (status != EFI_SUCCESS)
		printf("SetVirtualAddressMap failed: %lu\n", status);
}

char bootargs[256];

void
collect_kernel_args(char *args)
{
	/* Make a local copy of the bootargs */
	strlcpy(bootargs, args, sizeof(bootargs));
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
		    PMAP_CACHE_DEV);

	virtual_avail = va;

	return 0;
}

// debug function, not certain where this should go

void
dumpregs(struct trapframe *frame)
{
	int i;
	for (i = 0; i < 30; i+=2) {
		printf("x%02d: 0x%016lx 0x%016lx\n",
		    i, frame->tf_x[i], frame->tf_x[i+1]);
	}
	printf("sp: 0x%016lx\n", frame->tf_sp);
	printf("lr: 0x%016lx\n", frame->tf_lr);
	printf("pc: 0x%016lx\n", frame->tf_elr);
	printf("spsr: 0x%016lx\n", frame->tf_spsr);
}

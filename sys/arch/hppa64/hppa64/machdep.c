/*	$OpenBSD: machdep.c,v 1.35 2011/04/16 22:02:32 kettenis Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/device.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/timeout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/core.h>
#include <sys/kcore.h>
#include <sys/extent.h>
#include <sys/timetc.h>

#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm.h>
#include <uvm/uvm_page.h>
#include <uvm/uvm_swap.h>

#include <dev/cons.h>
#include <dev/clock_subr.h>

#include <machine/pdc.h>
#include <machine/iomod.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/autoconf.h>
#include <machine/kcore.h>
#include <machine/fpu.h>

#ifdef DDB
#include <machine/db_machdep.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

#include <hppa/dev/cpudevs.h>

/*
 * Patchable buffer cache parameters
 */
#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 10
#endif /* BUFCACHEPERCENT */

#ifdef BUFPAGES
int bufpages = BUFPAGES;
#else
int bufpages = 0;
#endif
int bufcachepercent = BUFCACHEPERCENT;

/*
 * Different kinds of flags used throughout the kernel.
 */
int cold = 1;			/* unset when engine is up to go */
extern int msgbufmapped;	/* set when safe to use msgbuf */

/*
 * cache configuration, for most machines is the same
 * numbers, so it makes sense to do defines w/ numbers depending
 * on configured cpu types in the kernel
 */
int icache_stride, icache_line_mask;
int dcache_stride, dcache_line_mask;

/*
 * things to not kill
 */
volatile u_int8_t *machine_ledaddr;
int machine_ledword, machine_leds;
struct cpu_info cpu0_info;

/*
 * CPU params (should be the same for all cpus in the system)
 */
struct pdc_cache pdc_cache PDC_ALIGNMENT;
struct pdc_model pdc_model PDC_ALIGNMENT;

	/* w/ a little deviation should be the same for all installed cpus */
u_int	cpu_ticksnum, cpu_ticksdenom;

	/* exported info */
char	machine[] = MACHINE;
char	cpu_model[128];
int	cpu_hvers;
enum hppa_cpu_type cpu_type;
const char *cpu_typename;
u_int	fpu_version;

dev_t	bootdev;
int	physmem, resvmem, resvphysmem, esym;

/*
 * Things for MI glue to stick on.
 */
struct user *proc0paddr;
long mem_ex_storage[EXTENT_FIXED_STORAGE_SIZE(32) / sizeof(long)];
struct extent *hppa_ex;
struct pool hppa_fppl;
struct hppa_fpstate proc0fpstate;
struct consdev *cn_tab;

struct vm_map *exec_map = NULL;
struct vm_map *phys_map = NULL;
/* Virtual page frame for /dev/mem (see mem.c) */
vaddr_t vmmap;

void delay_init(void);
static __inline void fall(int, int, int, int, int);
void dumpsys(void);
void hpmc_dump(void);
void cpuid(void);

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int	safepri = 0;

/*
 * wide used hardware params
 */
struct pdc_hwtlb pdc_hwtlb PDC_ALIGNMENT;
struct pdc_coproc pdc_coproc PDC_ALIGNMENT;
struct pdc_coherence pdc_coherence PDC_ALIGNMENT;
struct pdc_spidb pdc_spidbits PDC_ALIGNMENT;
struct pdc_model pdc_model PDC_ALIGNMENT;

#ifdef DEBUG
int sigdebug = 0;
pid_t sigpid = 0;
#define SDB_FOLLOW	0x01
#endif

struct uvm_constraint_range  dma_constraint = { 0x0, (paddr_t)-1 };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

int	hppa_cpuspeed(int *mhz);

int
hppa_cpuspeed(int *mhz)
{
	*mhz = PAGE0->mem_10msec / 10000;

	return (0);
}

void
hppa_init(paddr_t start)
{
	extern int kernel_text;
	int error;
	paddr_t	avail_end;

	pdc_init();	/* init PDC iface, so we can call em easy */

	delay_init();	/* calculate cpu clock ratio */

	cpuid();

	/* Enable wide mode for PSW defaults. */
	if ((error = pdc_call((iodcio_t)pdc, 0, PDC_PSW, PDC_PSW_SETDEFAULTS,
	    0x2 /* PDC WIDE BIT */)) < 0)
		panic("Failed to enable wide mode for PSW defaults: %d\n",
		    error);

	/* cache parameters */
	if ((error = pdc_call((iodcio_t)pdc, 0, PDC_CACHE, PDC_CACHE_DFLT,
	    &pdc_cache)) < 0) {
#ifdef DEBUG
		printf("WARNING: PDC_CACHE error %d\n", error);
#endif
	}

	dcache_line_mask = pdc_cache.dc_conf.cc_line * 16 - 1;
	dcache_stride = pdc_cache.dc_stride;
	icache_line_mask = pdc_cache.ic_conf.cc_line * 16 - 1;
	icache_stride = pdc_cache.ic_stride;

	/* cache coherence params (pbably available for 8k only) */
	error = pdc_call((iodcio_t)pdc, 0, PDC_CACHE, PDC_CACHE_SETCS,
	    &pdc_coherence, 1, 1, 1, 1);
#ifdef DEBUG
	printf ("PDC_CACHE_SETCS: %d, %d, %d, %d (%d)\n",
	    pdc_coherence.ia_cst, pdc_coherence.da_cst,
	    pdc_coherence.ita_cst, pdc_coherence.dta_cst, error);
#endif
	error = pdc_call((iodcio_t)pdc, 0, PDC_CACHE, PDC_CACHE_GETSPIDB,
	    &pdc_spidbits, 0, 0, 0, 0);
	printf("SPID bits: 0x%x, error = %d\n", pdc_spidbits.spidbits, error);

#if 0
TODO hpmc/toc/pfr
	/* setup hpmc handler */
	{
		extern u_int hpmc_v[];	/* from locore.s */
		u_int *p = hpmc_v;

		if (pdc_call((iodcio_t)pdc, 0, PDC_INSTR, PDC_INSTR_DFLT, p))
			*p = 0x08000240;

		p[6] = (u_int)&hpmc_dump;
		p[7] = 32;
		p[5] = -(p[0] + p[1] + p[2] + p[3] + p[4] + p[6] + p[7]);
	}

	{
		extern u_int hppa_toc[], hppa_toc_end[];
		u_int cksum, *p;

		for (cksum = 0, p = hppa_toc; p < hppa_toc_end; p++)
			cksum += *p;

		*p = cksum;
		PAGE0->ivec_toc = (u_int)&hppa_toc[0];
		PAGE0->ivec_toclen = (hppa_toc_end - hppa_toc + 1) * 4;
	}

	{
		extern u_int hppa_pfr[], hppa_pfr_end[];
		u_int cksum, *p;

		for (cksum = 0, p = hppa_pfr; p < hppa_pfr_end; p++)
			cksum += *p;

		*p = cksum;
		PAGE0->ivec_mempf = (u_int)&hppa_pfr[0];
		PAGE0->ivec_mempflen = (hppa_pfr_end - hppa_pfr + 1) * 4;
	}
#endif
	avail_end = trunc_page(PAGE0->imm_max_mem);
	if (avail_end > 0x4000000)
		avail_end = 0x4000000;
	physmem = atop(avail_end);
	resvmem = atop(((vaddr_t)&kernel_text));

	/* we hope this won't fail */
	hppa_ex = extent_create("mem", 0, HPPA_PHYSMAP, M_DEVBUF,
	    (caddr_t)mem_ex_storage, sizeof(mem_ex_storage),
	    EX_NOCOALESCE|EX_NOWAIT);
	if (extent_alloc_region(hppa_ex, 0, (vaddr_t)PAGE0->imm_max_mem,
	    EX_NOWAIT))
		panic("cannot reserve main memory");

	/* sets resvphysmem */
	pmap_bootstrap(round_page(start));

	/* space has been reserved in pmap_bootstrap() */
	msgbufp = (struct msgbuf *)((vaddr_t)ptoa(physmem) -
	    round_page(MSGBUFSIZE));
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));
	msgbufmapped = 1;

	/* they say PDC_COPROC might turn fault light on */
	pdc_call((iodcio_t)pdc, 0, PDC_CHASSIS, PDC_CHASSIS_DISP,
	    PDC_OSTAT(PDC_OSTAT_RUN) | 0xCEC0);

	cpu_cpuspeed = &hppa_cpuspeed;

#ifdef DDB
	ddb_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif
	ptlball();
	ficacheall();
	fdcacheall();

	proc0paddr->u_pcb.pcb_fpstate = &proc0fpstate;
	pool_init(&hppa_fppl, sizeof(struct hppa_fpstate), 16, 0, 0,
	    "hppafp", NULL);
}

void
cpuid()
{
	extern u_int fpu_enable;
	struct pdc_cpuid pdc_cpuid PDC_ALIGNMENT;
	int error;

	/* identify system type */
	if ((error = pdc_call((iodcio_t)pdc, 0, PDC_MODEL, PDC_MODEL_INFO,
	    &pdc_model)) < 0) {
#ifdef DEBUG
		printf("WARNING: PDC_MODEL error %d\n", error);
#endif
		pdc_model.hvers = 0;
	}

	bzero(&pdc_cpuid, sizeof(pdc_cpuid));
	if (pdc_call((iodcio_t)pdc, 0, PDC_MODEL, PDC_MODEL_CPUID,
	    &pdc_cpuid, 0, 0, 0, 0) >= 0) {

		/* patch for old 8200 */
		if (pdc_cpuid.version == HPPA_CPU_PCXU &&
		    pdc_cpuid.revision > 0x0d)
			pdc_cpuid.version = HPPA_CPU_PCXUP;

		cpu_type = pdc_cpuid.version;
	}

	/* locate coprocessors and SFUs */
	bzero(&pdc_coproc, sizeof(pdc_coproc));
	if ((error = pdc_call((iodcio_t)pdc, 0, PDC_COPROC, PDC_COPROC_DFLT,
	    &pdc_coproc, 0, 0, 0, 0)) < 0)
		printf("WARNING: PDC_COPROC error %d\n", error);
	else {
		printf("pdc_coproc: 0x%x, 0x%x; model %x rev %x\n",
		    pdc_coproc.ccr_enable, pdc_coproc.ccr_present,
		    pdc_coproc.fpu_model, pdc_coproc.fpu_revision);
		fpu_enable = pdc_coproc.ccr_enable & 0xc0;

		/* a kludge to detect PCXW */
		if (pdc_coproc.fpu_model == HPPA_FPU_PCXW)
			cpu_type = HPPA_CPU_PCXW;
	}

	if (!cpu_type)
		printf("WARNING: UNKNOWN CPU TYPE; GOOD LUCK\n");
	cpu_typename = hppa_mod_info(HPPA_TYPE_CPU, cpu_type);

	if (pdc_model.arch_rev != 8)
		panic("CANNOT RUN 64BIT KERNEL on 32BIT CPU");

	{
		const char *p;
		char buf[32];

		cpu_hvers = pdc_model.hvers >> 4;
		if (!cpu_hvers) {
			p = "(UNKNOWN)";
		} else {
			p = hppa_mod_info(HPPA_TYPE_BOARD, cpu_hvers);
			if (!p) {
				snprintf(buf, sizeof buf, "(UNKNOWN 0x%x)",
				    cpu_hvers);
				p = buf;
			}
		}

		snprintf(cpu_model, sizeof cpu_model,
		    "HP 9000/%s PA-RISC 2.0", p);
	}
#ifdef DEBUG
	printf("%s\n", cpu_model);
#endif
}

void
cpu_startup(void)
{
	vaddr_t minaddr, maxaddr;

	/*
	 * psychodelic kingdom come
	 * ... run, run, run
	 * psychodelic kings and queens
	 * join me in this one love dream
	 */
	printf("%s%s\n", version, cpu_model);
	printf("real mem = %lu (%luMB)\n", ptoa((psize_t)physmem),
	    ptoa((psize_t)physmem) / 1024 / 1024);
	printf("rsvd mem = %u (%uKB)\n", ptoa(resvmem), ptoa(resvmem) / 1024);

printf("here3\n");
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

printf("here4\n");
	/*
	 * Allocate a submap for physio
	 */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);

printf("here5\n");
	printf("avail mem = %lu (%luMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free) / 1024 / 1024);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();
	vmmap = uvm_km_valloc_wait(kernel_map, NBPG);

printf("here6\n");
	/*
	 * Configure the system.
	 */
	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}
printf("here7\n");
}

/*
 * compute cpu clock ratio such as:
 *	cpu_ticksnum / cpu_ticksdenom = t + delta
 *	delta -> 0
 */
void
delay_init(void)
{
	u_long num, denom, delta, mdelta;

	mdelta = ULONG_MAX;
	for (denom = 1; denom < 1000; denom++) {
		num = (PAGE0->mem_10msec * denom) / 10000;
		delta = num * 10000 / denom - PAGE0->mem_10msec;
		if (!delta) {
			cpu_ticksdenom = denom;
			cpu_ticksnum = num;
			break;
		} else if (delta < mdelta) {
			cpu_ticksdenom = denom;
			cpu_ticksnum = num;
			mdelta = delta;
		}
	}
printf("nom=%lu denom=%lu\n", cpu_ticksnum, cpu_ticksdenom);
}

void
delay(u_int us)
{
	u_long start, end, n;

	start = mfctl(CR_ITMR);
	while (us) {
		n = min(1000, us);
		end = start + n * cpu_ticksnum / cpu_ticksdenom;

		/* N.B. Interval Timer may wrap around */
		if (end < start)
			do
				start = mfctl(CR_ITMR);
			while (start > end);

		do
			start = mfctl(CR_ITMR);
		while (start < end);

		us -= n;
	}
}

static __inline void
fall(int c_base, int c_count, int c_loop, int c_stride, int data)
{
	int loop;

	for (; c_count--; c_base += c_stride)
		for (loop = c_loop; loop--; )
			if (data)
				__asm __volatile("fdce 0(%%sr0,%0)"
				    :: "r" (c_base));
			else
				__asm __volatile("fice 0(%%sr0,%0)"
				    :: "r" (c_base));
}

void
ficacheall(void)
{
	/*
	 * Flush the instruction, then data cache.
	 */
	fall(pdc_cache.ic_base, pdc_cache.ic_count, pdc_cache.ic_loop,
	    pdc_cache.ic_stride, 0);
	sync_caches();
}

void
fdcacheall(void)
{
	fall(pdc_cache.dc_base, pdc_cache.dc_count, pdc_cache.dc_loop,
	    pdc_cache.dc_stride, 1);
	sync_caches();
}

void
ptlball(void)
{
	pa_space_t sp;
	int i, j, k;

	/* instruction TLB */
	sp = pdc_cache.it_sp_base;
	for (i = 0; i < pdc_cache.it_sp_count; i++) {
		vaddr_t off = pdc_cache.it_off_base;
		for (j = 0; j < pdc_cache.it_off_count; j++) {
			for (k = 0; k < pdc_cache.it_loop; k++)
				pitlb(sp, off);
			off += pdc_cache.it_off_stride;
		}
		sp += pdc_cache.it_sp_stride;
	}

	/* data TLB */
	sp = pdc_cache.dt_sp_base;
	for (i = 0; i < pdc_cache.dt_sp_count; i++) {
		vaddr_t off = pdc_cache.dt_off_base;
		for (j = 0; j < pdc_cache.dt_off_count; j++) {
			for (k = 0; k < pdc_cache.dt_loop; k++)
				pdtlb(sp, off);
			off += pdc_cache.dt_off_stride;
		}
		sp += pdc_cache.dt_sp_stride;
	}
}

void
boot(int howto)
{
	/* If system is cold, just halt. */
	if (cold) {
		/* (Unless the user explicitly asked for reboot.) */
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
	} else {

		boothowto = howto | (boothowto & RB_HALT);

		if (!(howto & RB_NOSYNC)) {
			vfs_shutdown();
			/*
			 * If we've been adjusting the clock, the todr
			 * will be out of synch; adjust it now unless
			 * the system was sitting in ddb.
			 */
			if ((howto & RB_TIMEBAD) == 0)
				resettodr();
			else
				printf("WARNING: not updating battery clock\n");
		}

		/* XXX probably save howto into stable storage */

		uvm_shutdown();
		splhigh();

		if (howto & RB_DUMP)
			dumpsys();

		doshutdownhooks();
	}

	/* in case we came on powerfail interrupt */
	if (cold_hook)
		(*cold_hook)(HPPA_COLD_COLD);

	if (howto & RB_HALT) {
		if (howto & RB_POWERDOWN && cold_hook) {
			printf("Powering off...");
			DELAY(2000000);
			(*cold_hook)(HPPA_COLD_OFF);
			DELAY(1000000);
		}

		printf("System halted!\n");
		DELAY(2000000);
		__asm __volatile("stwas %0, 0(%1)"
		    :: "r" (CMD_STOP), "r" (HPPA_LBCAST + iomod_command));
	} else {
		printf("rebooting...");
		DELAY(2000000);

		/* ask firmware to reset */
                pdc_call((iodcio_t)pdc, 0, PDC_BROADCAST_RESET, PDC_DO_RESET);

		__asm __volatile(".export hppa_reset, entry\n\t"
		    ".label hppa_reset");
		__asm __volatile("stwas %0, 0(%1)"
		    :: "r" (CMD_RESET), "r" (HPPA_LBCAST + iomod_command));
	}

	for(;;); /* loop while bus reset is comming up */
	/* NOTREACHED */
}

u_long	dumpmag = 0x8fca0101;	/* magic number */
int	dumpsize = 0;		/* pages */
long	dumplo = 0;		/* blocks */

/*
 * cpu_dumpsize: calculate size of machine-dependent kernel core dump headers.
 */
int
cpu_dumpsize(void)
{
	int size;

	size = ALIGN(sizeof(kcore_seg_t)) + ALIGN(sizeof(cpu_kcore_hdr_t));
	if (roundup(size, dbtob(1)) != dbtob(1))
		return -1;

	return 1;
}

/*
 * Called from HPMC handler in locore
 */
void
hpmc_dump(void)
{
	printf("HPMC\n");

	cold = 0;
	boot(RB_NOSYNC);
}

int
cpu_dump(void)
{
	long buf[dbtob(1) / sizeof (long)];
	kcore_seg_t	*segp;
	cpu_kcore_hdr_t	*cpuhdrp;

	segp = (kcore_seg_t *)buf;
	cpuhdrp = (cpu_kcore_hdr_t *)&buf[ALIGN(sizeof(*segp)) / sizeof (long)];

	/*
	 * Generate a segment header.
	 */
	CORE_SETMAGIC(*segp, KCORE_MAGIC, MID_MACHINE, CORE_CPU);
	segp->c_size = dbtob(1) - ALIGN(sizeof(*segp));

	/*
	 * Add the machine-dependent header info
	 */
	/* nothing for now */

	return (bdevsw[major(dumpdev)].d_dump)
	    (dumpdev, dumplo, (caddr_t)buf, dbtob(1));
}

/*
 * Dump the kernel's image to the swap partition.
 */
#define	BYTES_PER_DUMP	NBPG

void
dumpsys(void)
{
	int psize, bytes, i, n;
	caddr_t maddr;
	daddr64_t blkno;
	int (*dump)(dev_t, daddr64_t, caddr_t, size_t);
	int error;

	/* Save registers
	savectx(&dumppcb); */

	if (dumpsize == 0)
		dumpconf();
	if (dumplo <= 0) {
		printf("\ndump to dev %x not possible\n", dumpdev);
		return;
	}
	printf("\ndumping to dev %x, offset %ld\n", dumpdev, dumplo);

#ifdef UVM_SWAP_ENCRYPT
	uvm_swap_finicrypt_all();
#endif

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}

	if (!(error = cpu_dump())) {

		bytes = ptoa(physmem);
		maddr = NULL;
		blkno = dumplo + cpu_dumpsize();
		dump = bdevsw[major(dumpdev)].d_dump;
		for (i = 0; i < bytes; i += n) {

			/* Print out how many MBs we are to go. */
			n = bytes - i;
			if (n && (n % (1024*1024)) == 0)
				printf("%d ", n / (1024 * 1024));

			/* Limit size for next transfer. */
			if (n > BYTES_PER_DUMP)
				n = BYTES_PER_DUMP;

			if ((error = (*dump)(dumpdev, blkno, maddr, n)))
				break;
			maddr += n;
			blkno += btodb(n);
		}
	}

	switch (error) {
	case ENXIO:	printf("device bad\n");			break;
	case EFAULT:	printf("device not ready\n");		break;
	case EINVAL:	printf("area improper\n");		break;
	case EIO:	printf("i/o error\n");			break;
	case EINTR:	printf("aborted from console\n");	break;
	case 0:		printf("succeeded\n");			break;
	default:	printf("error %d\n", error);		break;
	}
}

/* bcopy(), error on fault */
int
kcopy(const void *from, void *to, size_t size)
{
	return spcopy(HPPA_SID_KERNEL, from, HPPA_SID_KERNEL, to, size);
}

int
copystr(const void *src, void *dst, size_t size, size_t *lenp)
{
	return spstrcpy(HPPA_SID_KERNEL, src, HPPA_SID_KERNEL, dst, size, lenp);
}

int
copyinstr(const void *src, void *dst, size_t size, size_t *lenp)
{
	return spstrcpy(curproc->p_addr->u_pcb.pcb_space, src,
	    HPPA_SID_KERNEL, dst, size, lenp);
}


int
copyoutstr(const void *src, void *dst, size_t size, size_t *lenp)
{
	return spstrcpy(HPPA_SID_KERNEL, src,
	    curproc->p_addr->u_pcb.pcb_space, dst, size, lenp);
}


int
copyin(const void *src, void *dst, size_t size)
{
	return spcopy(curproc->p_addr->u_pcb.pcb_space, src,
	    HPPA_SID_KERNEL, dst, size);
}

int
copyout(const void *src, void *dst, size_t size)
{
	return spcopy(HPPA_SID_KERNEL, src,
	    curproc->p_addr->u_pcb.pcb_space, dst, size);
}

/*
 * Set registers on exec.
 */
void
setregs(struct proc *p, struct exec_package *pack, u_long stack,
    register_t *retval)
{
	struct trapframe *tf = p->p_md.md_regs;
	struct pcb *pcb = &p->p_addr->u_pcb;
	register_t zero;

	tf->tf_flags = TFF_SYS|TFF_LAST;
	tf->tf_iioq[1] = 4 +
	    (tf->tf_iioq[0] = pack->ep_entry | HPPA_PC_PRIV_USER);
	tf->tf_rp = 0;
	tf->tf_args[0] = (u_long)PS_STRINGS;
	tf->tf_args[1] = tf->tf_args[2] = 0; /* XXX dynload stuff */

	/* setup terminal stack frame */
	stack = (stack + 0x1f) & ~0x1f;
	tf->tf_r3 = stack;
	tf->tf_sp = stack += HPPA_FRAME_SIZE;
	tf->tf_ret1 = stack - 16;	/* ap */
	zero = 0;
	copyout(&zero, (caddr_t)(stack - HPPA_FRAME_SIZE), sizeof(register_t));
	copyout(&zero, (caddr_t)(stack + HPPA_FRAME_RP), sizeof(register_t));

	/* reset any of the pending FPU exceptions */
	fpu_proc_flush(p);
	pcb->pcb_fpstate->hfp_regs.fpr_regs[0] =
	    ((u_int64_t)HPPA_FPU_INIT) << 32;
	pcb->pcb_fpstate->hfp_regs.fpr_regs[1] = 0;
	pcb->pcb_fpstate->hfp_regs.fpr_regs[2] = 0;
	pcb->pcb_fpstate->hfp_regs.fpr_regs[3] = 0;

	retval[1] = 0;
}

/*
 * Send an interrupt to process.
 */
void
sendsig(sig_t catcher, int sig, int mask, u_long code, int type,
    union sigval val)
{
	struct proc *p = curproc;
	struct trapframe *tf = p->p_md.md_regs;
	struct sigacts *psp = p->p_sigacts;
	struct sigcontext ksc;
	siginfo_t ksi;
	register_t scp, sip, zero;
	int sss;

	/* TODO sendsig */

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sendsig: %s[%d] sig %d catcher %p\n",
		    p->p_comm, p->p_pid, sig, catcher);
#endif

	/* Save the FPU context first. */
	fpu_proc_save(p);

	ksc.sc_onstack = p->p_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate space for the signal handler context.
	 */
	if ((p->p_sigstk.ss_flags & SS_DISABLE) == 0 && !ksc.sc_onstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		scp = (register_t)p->p_sigstk.ss_sp;
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	} else
		scp = (tf->tf_sp + 63) & ~63;

	sss = (sizeof(ksc) + 63) & ~63;
	sip = 0;
	if (psp->ps_siginfo & sigmask(sig)) {
		sip = scp + sizeof(ksc);
		sss += (sizeof(ksi) + 63) & ~63;
	}

#ifdef DEBUG
	if ((tf->tf_iioq[0] & ~PAGE_MASK) == SYSCALLGATE)
		printf("sendsig: interrupted syscall at 0x%x:0x%x, flags %b\n",
		    tf->tf_iioq[0], tf->tf_iioq[1], tf->tf_ipsw, PSL_BITS);
#endif

	ksc.sc_mask = mask;
	ksc.sc_fp = scp + sss;
	ksc.sc_ps = tf->tf_ipsw;
	ksc.sc_pcoqh = tf->tf_iioq[0];
	ksc.sc_pcoqt = tf->tf_iioq[1];
	bcopy(tf, &ksc.sc_regs[0], 32*8);
	ksc.sc_regs[0] = tf->tf_sar;
	bcopy(&p->p_addr->u_pcb.pcb_fpstate->hfp_regs, ksc.sc_fpregs,
	    sizeof(ksc.sc_fpregs));

	sss += HPPA_FRAME_SIZE;
	tf->tf_args[0] = sig;
	tf->tf_args[1] = sip;
	tf->tf_args[2] = tf->tf_r4 = scp;
	tf->tf_args[3] = (register_t)catcher;
	tf->tf_sp = scp + sss;
	tf->tf_ipsw &= ~(PSL_N|PSL_B);
	tf->tf_iioq[0] = HPPA_PC_PRIV_USER | p->p_sigcode;
	tf->tf_iioq[1] = tf->tf_iioq[0] + 4;
	/* disable tracing in the trapframe */

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sendsig(%d): sig %d scp %p fp %p sp 0x%x\n",
		    p->p_pid, sig, scp, ksc.sc_fp, (register_t)scp + sss);
#endif

	if (copyout(&ksc, (void *)scp, sizeof(ksc)))
		sigexit(p, SIGILL);

	if (sip) {
		initsiginfo(&ksi, sig, code, type, val);
		if (copyout(&ksi, (void *)sip, sizeof(ksi)))
			sigexit(p, SIGILL);
	}

	zero = 0;
	if (copyout(&zero, (caddr_t)scp + sss - HPPA_FRAME_SIZE,
	    sizeof(register_t)))
		sigexit(p, SIGILL);

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sendsig(%d): pc 0x%x, catcher 0x%x\n", p->p_pid,
		    tf->tf_iioq[0], tf->tf_args[3]);
#endif
}

int
sys_sigreturn(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext *scp, ksc;
	struct trapframe *tf = p->p_md.md_regs;
	int error;

	/* TODO sigreturn */

	scp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sigreturn: pid %d, scp %p\n", p->p_pid, scp);
#endif

	/* flush the FPU ctx first */
	fpu_proc_flush(p);

	if ((error = copyin((caddr_t)scp, (caddr_t)&ksc, sizeof ksc)))
		return (error);

#define PSL_MBS (PSL_C|PSL_Q|PSL_P|PSL_D|PSL_I)
#define PSL_MBZ (PSL_Y|PSL_Z|PSL_S|PSL_X|PSL_M|PSL_R)
	if ((ksc.sc_ps & (PSL_MBS|PSL_MBZ)) != PSL_MBS)
		return (EINVAL);

	if (ksc.sc_onstack)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = ksc.sc_mask &~ sigcantmask;

	tf->tf_sar = ksc.sc_regs[0];
	ksc.sc_regs[0] = tf->tf_flags;
	bcopy(&ksc.sc_regs[0], tf, 32*8);
	bcopy(ksc.sc_fpregs, &p->p_addr->u_pcb.pcb_fpstate->hfp_regs,
	    sizeof(ksc.sc_fpregs));

	tf->tf_iioq[0] = ksc.sc_pcoqh;
	tf->tf_iioq[1] = ksc.sc_pcoqt;
	tf->tf_ipsw = ksc.sc_ps;

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) && (!sigpid || p->p_pid == sigpid))
		printf("sigreturn(%d): returns\n", p->p_pid);
#endif
	return (EJUSTRETURN);
}

/*
 * machine dependent system variables.
 */
int
cpu_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen, struct proc *p)
{
	dev_t consdev;

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);	/* overloaded */
	switch (name[0]) {
	case CPU_CONSDEV:
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
		    sizeof consdev));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}


/*
 * consinit:
 * initialize the system console.
 */
void
consinit(void)
{
	/*
	 * Initial console setup has been done in pdc_init().
	 */
}

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	struct cpu_info *ci = curcpu();

	if (ci->ci_cpl < wantipl)
		splassert_fail(wantipl, ci->ci_cpl, func);
}
#endif

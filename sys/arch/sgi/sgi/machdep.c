/*	$OpenBSD: machdep.c,v 1.12 2004/09/20 10:31:16 pefo Exp $ */

/*
 * Copyright (c) 2003-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/exec_olf.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef MFS
#include <ufs/mfs/mfs_extern.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>
#include <ddb/db_interface.h>

#include <machine/pte.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/pio.h>
#include <machine/psl.h>
#include <machine/autoconf.h>
#include <machine/memconf.h>
#include <machine/regnum.h>

#include <machine/rm7000.h>

#include <dev/cons.h>

#include <mips64/arcbios.h>
#include <mips64/archtype.h>
#include <machine/bus.h>

#include <sgi/localbus/macebus.h>

extern struct consdev *cn_tab;
extern char kernel_text[];
extern void makebootdev(const char *);
extern void stacktrace(void);

/* the following is used externally (sysctl_hw) */
char	machine[] = MACHINE;		/* machine "architecture" */
char	machine_arch[] = MACHINE_ARCH;	/* cpu "architecture" */
char	cpu_model[30];
#ifdef APERTURE
#if defined(INSECURE) || defined(DEBUG)
int allowaperture = 1;
#else
int allowaperture = 0;
#endif
#endif

/*
 * Declare these as initialized data so we can patch them.
 */
#ifndef	NBUF
#define NBUF 0			/* Can be changed in config */
#endif
#ifndef	BUFPAGES
#define BUFPAGES 0		/* Can be changed in config */
#endif

int	nswbuf = 0;
int	nbuf = NBUF;
int	bufpages = BUFPAGES;

vm_map_t exec_map;
vm_map_t phys_map;

int	msgbufmapped;		/* set when safe to use msgbuf */
caddr_t	msgbufbase;

int	physmem;		/* max supported memory, changes to actual */
int	rsvdmem;		/* reserved memory not usable */
int	ncpu = 1;		/* At least one cpu in the system */
struct	user *proc0paddr;
struct	user *curprocpaddr;
int	console_ok;		/* set when console initialized */

int32_t *environment;
struct sys_rec sys_config;


/* ddb symbol init stuff */
caddr_t	ssym;
caddr_t	esym;
caddr_t	ekern;

struct phys_mem_desc mem_layout[MAXMEMSEGS];

caddr_t mips_init(int, int32_t *);
void initcpu(void);
void dumpsys(void);
void dumpconf(void);
caddr_t allocsys(caddr_t);

static void dobootopts(int, int32_t *);
static int atoi(const char *, int, const char **);

#if BYTE_ORDER == BIG_ENDIAN
int	my_endian = 1;
#else
int	my_endian = 0;
#endif


/*
 * Do all the stuff that locore normally does before calling main().
 * Reset mapping and set up mapping to hardware and init "wired" reg.
 */

caddr_t
mips_init(int argc, int32_t *argv)
{
	char *cp;
	int i;
	unsigned firstaddr;
	caddr_t sd;
	struct tlb tlb;
	extern char start[], edata[], end[];
	extern char tlb_miss[], e_tlb_miss[];
	extern char tlb_miss_tramp[], e_tlb_miss_tramp[];
	extern char xtlb_miss_tramp[], e_xtlb_miss_tramp[];
	extern char exception[], e_exception[];

	/*
	 *  Clean up any mess.
	 */
	Bios_FlushAllCaches();

	/*
	 * Clear the compiled BSS segment in OpenBSD code
	 */
	bzero(edata, end-edata);

	/*
	 *  Reserve symol table space. If invalid pointers no table.
	 */
	ssym = (char *)*(u_int64_t *)end;
	esym = (char *)*((u_int64_t *)end + 1);
	ekern = esym;
	if (((long)ssym - (long)end) < 0 ||
	    ((long)ssym - (long)end) > 0x1000 ||
	    ssym[0] != ELFMAG0 || ssym[1] != ELFMAG1 ||
	    ssym[2] != ELFMAG2 || ssym[3] != ELFMAG3 ) {
		ssym = NULL;
		esym = NULL;
		ekern = end;
	}

	/*
	 *  Initialize the system type and set up memory layout
	 */
	bios_ident();

	/*
	 * Determine system type and set up configuration record data.
	 */
	if (sys_config.system_type == SGI_O2) {
		bios_putstring("Found SGI-IP32, setting up.\n");
		strlcpy(cpu_model, "SGI O2", sizeof(cpu_model));
		sys_config.cons_ioaddr[0] = 0x00390000;	/*XXX*/
		sys_config.cons_ioaddr[1] = 0x00398000;	/*XXX*/
		sys_config.cons_baudclk = 1843200;		/*XXX*/
		sys_config.cons_iot = &macebus_tag;
		sys_config.local.bus_base = 0x0;		/*XXX*/
#if defined(_LP64)
		sys_config.pci_io[0].bus_base = 0xffffffff00000000;/*XXX*/
		sys_config.pci_mem[0].bus_base = 0xffffffff00000000;/*XXX*/
#else
		sys_config.pci_io[0].bus_base = 0x00000000;/*XXX*/
		sys_config.pci_mem[0].bus_base = 0x00000000;/*XXX*/
#endif
		sys_config.pci_mem[0].bus_base_dma = 0x00000000;/*XXX*/
		sys_config.pci_mem[0].bus_reverse = my_endian;
		sys_config.cpu[0].tlbwired = 2;
		sys_config.cpu[0].clock = 200000000;  /* Reasonable default */
	} else {
		bios_putstring("Unsupported system!!!\n");
		while(1);
	}

	/*
	 *  Use cpufrequency from bios to start with.
	 */
	cp = Bios_GetEnvironmentVariable("cpufreq");
	if (cp) {
		i = atoi(cp, 10, NULL);
		if (i > 100)
			sys_config.cpu[0].clock = i * 1000000;
	}

	/*
	 *  Set pagesize to enable use of page macros and functions.
	 *  Commit available memory to UVM system
	 */
	uvmexp.pagesize = 4096;
	uvm_setpagesize();

	for(i = 0; i < MAXMEMSEGS && mem_layout[i].mem_first_page != 0; i++) {
		u_int32_t fp, lp;
		u_int32_t firstkernpage = atop(KSEG0_TO_PHYS(start));
		u_int32_t lastkernpage = atop(KSEG0_TO_PHYS(ekern));

		fp = mem_layout[i].mem_first_page;
		lp = mem_layout[i].mem_last_page - 1;

		/* Account for kernel and kernel symbol table */
		if (fp >= firstkernpage && lp < lastkernpage)
			continue;	/* In kernel */

		if (lp < firstkernpage || fp > lastkernpage) {
			uvm_page_physload(fp, lp, fp, lp, VM_FREELIST_DEFAULT);
			continue;	/* Outside kernel */
		}

		if (fp > firstkernpage) 
			fp = lastkernpage + 1;
		else if (lp < lastkernpage)
			lp = firstkernpage - 1;
		else { /* Need to split! */
			u_int32_t xp = firstkernpage - 1;
			uvm_page_physload(fp, xp, fp, xp, VM_FREELIST_DEFAULT);
			fp = lastkernpage + 1;
		}
		if (lp >= fp)
			uvm_page_physload(fp, lp, fp, lp, VM_FREELIST_DEFAULT);
	}

	/*
	 *  Figure out where we was booted from.
	 */
	cp = Bios_GetEnvironmentVariable("OSLoadPartition");
	if (cp == NULL)
		cp = "unknown";
	makebootdev(cp);

	/*
	 * Look at arguments passed to us and compute boothowto.
	 * Default to SINGLE and ASKNAME if no args or
	 * SINGLE and DFLTROOT if this is a ramdisk kernel.
	 */
#ifdef RAMDISK_HOOKS
	boothowto = RB_SINGLE | RB_DFLTROOT;
#else
	boothowto = RB_SINGLE | RB_ASKNAME;
#endif /* RAMDISK_HOOKS */

	dobootopts(argc, argv);

	/* Check l3cache size and disable (hard) if non present. */
	if (Bios_GetEnvironmentVariable("l3cache") != 0) {
		i = atoi(Bios_GetEnvironmentVariable("l3cache"), 10, NULL);
		CpuTertiaryCacheSize = 1024 * 1024 * i;
	} else {
		CpuTertiaryCacheSize = 0;
	}

	sys_config.cpu[0].type = (cp0_get_prid() >> 8) & 0xff;
	sys_config.cpu[0].vers_maj = (cp0_get_prid() >> 4) & 0x0f;
	sys_config.cpu[0].vers_min = cp0_get_prid() & 0x0f;
	sys_config.cpu[0].fptype = (cp1_get_prid() >> 8) & 0xff;
	sys_config.cpu[0].fpvers_maj = (cp1_get_prid() >> 4) & 0x0f;
	sys_config.cpu[0].fpvers_min = cp1_get_prid() & 0x0f;

	/*
	 *  Configure TLB.
	 */
	switch(sys_config.cpu[0].type) {
	case MIPS_RM7000:
		/* Rev A (version >= 2) CPU's have 64 TLB entries. */
		if (sys_config.cpu[0].vers_maj < 2) {
			sys_config.cpu[0].tlbsize = 48;
		} else {
			sys_config.cpu[0].tlbsize = 64;
		}
		break;

	case MIPS_R10000:
	case MIPS_R12000:
	case MIPS_R14000:
		sys_config.cpu[0].tlbsize = 64;
		break;

	default:
		sys_config.cpu[0].tlbsize = 48;
		break;
	}

	/*
	 *  Configure Cache.
	 */
	switch(sys_config.cpu[0].type) {
	case MIPS_R10000:
	case MIPS_R12000:
	case MIPS_R14000:
		sys_config.cpu[0].cfg_reg = Mips10k_ConfigCache();
		sys_config._SyncCache = Mips10k_SyncCache;
		sys_config._InvalidateICache = Mips10k_InvalidateICache;
		sys_config._InvalidateICachePage = Mips10k_InvalidateICachePage;
		sys_config._SyncDCachePage = Mips10k_SyncDCachePage;
		sys_config._HitSyncDCache = Mips10k_HitSyncDCache;
		sys_config._IOSyncDCache = Mips10k_IOSyncDCache;
		sys_config._HitInvalidateDCache = Mips10k_HitInvalidateDCache;
		break;

	default:
		sys_config.cpu[0].cfg_reg = Mips5k_ConfigCache();
		sys_config._SyncCache = Mips5k_SyncCache;
		sys_config._InvalidateICache = Mips5k_InvalidateICache;
		sys_config._InvalidateICachePage = Mips5k_InvalidateICachePage;
		sys_config._SyncDCachePage = Mips5k_SyncDCachePage;
		sys_config._HitSyncDCache = Mips5k_HitSyncDCache;
		sys_config._IOSyncDCache = Mips5k_IOSyncDCache;
		sys_config._HitInvalidateDCache = Mips5k_HitInvalidateDCache;
		break;
	}

	tlb_set_wired(0);
	tlb_flush(sys_config.cpu[0].tlbsize);
	tlb_set_wired(sys_config.cpu[0].tlbwired);

	/* XXX Save the following as an example on how to optimize I/O mapping */

	/*
	 *  Set up some fixed mappings. These are so frequently
	 *  used so faulting them in will waste to many cycles.
	 */
	if (sys_config.system_type == MOMENTUM_CP7000G ||
	    sys_config.system_type == MOMENTUM_CP7000 ||
	    sys_config.system_type == GALILEO_EV64240) {
		tlb.tlb_mask = PG_SIZE_16M;
#if defined(LP64)
		tlb.tlb_hi = vad_to_vpn(0xfffffffffc000000) | 1;
		tlb.tlb_lo0 = vad_to_pfn(0xfffffffff4000000) | PG_IOPAGE;
#else
		tlb.tlb_hi = vad_to_vpn(0xfc000000) | 1;
		tlb.tlb_lo0 = vad_to_pfn(0xf4000000) | PG_IOPAGE;
#endif
		tlb.tlb_lo1 = vad_to_pfn(sys_config.cons_ioaddr[0]) | PG_IOPAGE;
		tlb_write_indexed(2, &tlb);

		if (sys_config.system_type == GALILEO_EV64240) {
			tlb.tlb_mask = PG_SIZE_16M;
			tlb.tlb_hi = vad_to_vpn(0xf8000000) | 1;
			tlb.tlb_lo0 = vad_to_pfn(sys_config.pci_io[0].bus_base) | PG_IOPAGE;
			tlb.tlb_lo1 = vad_to_pfn(sys_config.pci_mem[0].bus_base) | PG_IOPAGE;
			tlb_write_indexed(3, &tlb);
		}
	}
/* XXX */

	/*
	 *  Get a console, very early but after initial mapping setup.
	 */
	bios_putstring("Initial setup done, switching console.\n\n");
	consinit();

	/*
	 * Init message buffer.
	 */
	msgbufbase = (caddr_t)pmap_steal_memory(MSGBUFSIZE, NULL,NULL);
	initmsgbuf(msgbufbase, MSGBUFSIZE);

	/*
	 * Allocate U page(s) for proc[0], pm_tlbpid 1.
	 */
	proc0.p_addr = proc0paddr = curprocpaddr =
		(struct user *)pmap_steal_memory(USPACE, NULL,NULL);
	proc0.p_md.md_regs = (struct trap_frame *)&proc0paddr->u_pcb.pcb_regs;
	firstaddr = KSEG0_TO_PHYS(proc0.p_addr);
	tlb_set_pid(1);

	/*
	 * Allocate system data structures.
	 */
	i = (vsize_t)allocsys(NULL);
	sd = (caddr_t)pmap_steal_memory(i, NULL, NULL);
	allocsys(sd);

	/*
	 * Bootstrap VM system.
	 */
	pmap_bootstrap();

	/*
	 * Copy down exception vector code. If code is to large
	 * copy down trampolines instead of doing a panic.
	 */
	if (e_tlb_miss - tlb_miss > 0x80) {
		bcopy(tlb_miss_tramp, (char *)TLB_MISS_EXC_VEC,
		    e_tlb_miss_tramp - tlb_miss_tramp);
		bcopy(xtlb_miss_tramp, (char *)XTLB_MISS_EXC_VEC,
		    e_xtlb_miss_tramp - xtlb_miss_tramp);
	} else {
		bcopy(tlb_miss, (char *)TLB_MISS_EXC_VEC,
		    e_tlb_miss - tlb_miss);
	}

	bcopy(exception, (char *)CACHE_ERR_EXC_VEC, e_exception - exception);
	bcopy(exception, (char *)GEN_EXC_VEC, e_exception - exception);

#ifdef DDB
	db_machine_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif

	/*
	 * Clear out the I and D caches.
	 */
	Mips_SyncCache();

	/*
	 *  Return new stack pointer.
	 */
	return ((caddr_t)proc0paddr + USPACE - 64);
}

/*
 * Allocate space for system data structures. Doesn't need to be mapped.
 */
caddr_t
allocsys(caddr_t v)
{
	caddr_t start;

	start = v;

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

	/*
	 * Determine how many buffers to allocate.
	 */
	if (bufpages == 0) {
		bufpages = (physmem / (100/BUFCACHEPERCENT));
	}
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	/* Restrict to at most 70% filled kvm */
	if (nbuf > (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) / MAXBSIZE * 7 / 10) {
		nbuf = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) / MAXBSIZE * 7 / 10;
	}

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / PAGE_SIZE) {
		bufpages = nbuf * MAXBSIZE / PAGE_SIZE;
	}

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* even */
		if (nswbuf > 256) {
			nswbuf = 256;
		}
	}

	valloc(buf, struct buf, nbuf);

	/*
	 * Clear allocated memory.
	 */
	if (start != 0) {
		bzero(start, v - start);
	}

	return(v);
}


/*
 *  Decode boot options.
 */
static void
dobootopts(int argc, int32_t *argv)
{
	char *cp;
	int i;

	/* XXX Should this be done differently, eg env vs. args? */
	for (i = 1; i < argc; i++) {
		cp = (char *)(long)argv[i];
		if (cp != NULL && strncmp(cp, "OSLoadOptions=", 14) == 0) {
			if (strcmp(&cp[14], "auto") == 0)
					boothowto &= ~(RB_SINGLE|RB_ASKNAME);
			else if (strcmp(&cp[14], "single") == 0)
					boothowto |= RB_SINGLE;
			else if (strcmp(&cp[14], "debug") == 0)
					boothowto |= RB_KDB;
		}
	}

	cp = Bios_GetEnvironmentVariable("ConsoleOut");
	if (cp != NULL && strncmp(cp, "serial", 6) == 0)
		boothowto |= RB_SERCONS;
}


/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit()
{
	if (console_ok) {
		return;
	}
	cninit();
	console_ok = 1;
}

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup()
{
	unsigned i;
	int base, residual;
	vaddr_t minaddr, maxaddr;
	vsize_t size;
#ifdef DEBUG
	extern int pmapdebugflag;
	int opmapdebugflag = pmapdebugflag;

	pmapdebugflag = 0;	/* Shut up pmap debug during bootstrap */
#endif

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("real mem = %d\n", ptoa(physmem));
	printf("rsvd mem = %d\n", ptoa(rsvdmem));

	/*
	 * Allocate virtual address space for file I/O buffers.
	 * Note they are different than the array of headers, 'buf',
	 * and usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
			NULL, UVM_UNKNOWN_OFFSET, 0,
			UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
			UVM_ADV_NORMAL, 0)) != KERN_SUCCESS) {
		panic("cpu_startup: cannot allocate VM for buffers");
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	if (base >= MAXBSIZE / PAGE_SIZE) {
		/* don't want to alloc more physical mem than needed */
		base = MAXBSIZE / PAGE_SIZE;
		residual = 0;
	}

	for (i = 0; i < nbuf; i++) {
		vsize_t curbufsize;
		vaddr_t curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		curbuf = (vaddr_t)buffers + i * MAXBSIZE;
		curbufsize = PAGE_SIZE * (i < residual ? base+1 : base);

		while (curbufsize) {
			struct vm_page *pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("cpu_startup: not enough memory for"
					" buffer cache");
			pmap_kenter_pa(curbuf, VM_PAGE_TO_PHYS(pg),
					VM_PROT_READ|VM_PROT_WRITE);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr, 16 * NCARGS,
					TRUE, FALSE, NULL);
	/* Allocate a submap for physio */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, TRUE, FALSE, NULL);

#ifdef DEBUG
	pmapdebugflag = opmapdebugflag;
#endif
	printf("avail mem = %d\n", ptoa(uvmexp.free));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * PAGE_SIZE);
	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

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
		return ENOTDIR;		/* overloaded */

	switch (name[0]) {
	case CPU_ALLOWAPERTURE:
#ifdef APERTURE
	if (securelevel > 0)
		return sysctl_rdint(oldp, oldlenp, newp, allowaperture);
	else
		return sysctl_int(oldp, oldlenp, newp, newlen, &allowaperture);
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif
	default:
		return EOPNOTSUPP;
	}
}

/*
 * Set registers on exec for native exec format. For o64/64.
 */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	extern struct proc *machFPCurProcPtr;
#if 0
/* XXX should check validity of header and perhaps be 32/64 indep. */
	Elf64_Ehdr *eh = pack->ep_hdr;

	if ((((eh->e_flags & EF_MIPS_ABI) != E_MIPS_ABI_NONE) &&
	    ((eh->e_flags & EF_MIPS_ABI) != E_MIPS_ABI_O32)) ||
	    ((eh->e_flags & EF_MIPS_ARCH) >= E_MIPS_ARCH_3) ||
	    (eh->e_ident[EI_CLASS] != ELFCLASS32)) {
		p->p_md.md_flags |= MDP_O32;
	}
#endif

#if !defined(__LP64__)
	p->p_md.md_flags |= MDP_O32;
#else
	p->p_md.md_flags &= ~MDP_O32;
#endif

	bzero((caddr_t)p->p_md.md_regs, sizeof(struct trap_frame));
	p->p_md.md_regs->sp = stack;
	p->p_md.md_regs->pc = pack->ep_entry & ~3;
	p->p_md.md_regs->t9 = pack->ep_entry & ~3; /* abicall req */
#if defined(__LP64__)
	p->p_md.md_regs->sr = SR_FR_32|SR_XX|SR_KSU_USER|SR_UX|SR_EXL|SR_INT_ENAB;
#else
	p->p_md.md_regs->sr = SR_KSU_USER|SR_XX|SR_EXL|SR_INT_ENAB;
#endif
	p->p_md.md_regs->sr |= (idle_mask << 8) & SR_INT_MASK;
	p->p_md.md_regs->ic = idle_mask & IC_INT_MASK;
	p->p_md.md_flags &= ~MDP_FPUSED;
	if (machFPCurProcPtr == p)
		machFPCurProcPtr = (struct proc *)0;
	p->p_md.md_ss_addr = 0;
	p->p_md.md_pc_ctrl = 0;
	p->p_md.md_watch_1 = 0;
	p->p_md.md_watch_2 = 0;
}


int	waittime = -1;

void
boot(int howto)
{

	/* take a snap shot before clobbering any registers */
	if (curproc)
		savectx(curproc->p_addr, 0);

#ifdef DEBUG
	if (panicstr)
		stacktrace();
#endif

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;
		/* fill curproc with live object */
		if (curproc == NULL)
			curproc = &proc0;
		/*
		 * Synchronize the disks....
		 */
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}
	(void) splhigh();		/* extreme priority */

	if (howto & RB_HALT) {
		if (howto & RB_POWERDOWN) {
			printf("System Power Down.\n");
			delay(1000000);
			Bios_PowerDown();
		} else {
			printf("System Halt.\n");
			delay(1000000);
			Bios_EnterInteractiveMode();
		}
		printf("Didn't want to die!!! Reset manually.\n");
		while(1); /* Forever */
	} else {
		if (howto & RB_DUMP)
			dumpsys();
		printf("System restart.\n");
		delay(1000000);
		Bios_Reboot();
		printf("Restart failed!!! Reset manually.\n");
		while(1); /* Forever */
	}
	/*NOTREACHED*/
}

int	dumpmag = (int)0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumplo = 0;

void
dumpconf()
{
	int nblks;

	dumpsize = ptoa(physmem);
	if (dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize) {
		nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
		if (dumpsize > btoc(dbtob(nblks - dumplo)))
			dumpsize = btoc(dbtob(nblks - dumplo));
		else if (dumplo == 0)
			dumplo = nblks - btodb(ctob(physmem));
	}
	/*
	 * Don't dump on the first page
	 * in case the dump device includes a disk label.
	 */
	if (dumplo < btodb(PAGE_SIZE))
		dumplo = btodb(PAGE_SIZE);
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
void
dumpsys()
{

	msgbufmapped = 0;
	if (dumpdev == NODEV)
		return;
	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo < 0)
		return;
	printf("\ndumping to dev %x, offset %d\n", dumpdev, dumplo);
	printf("dump not yet implemented");
#if 0 /* XXX HAVE TO FIX XXX */
	switch (error = (*bdevsw[major(dumpdev)].d_dump)(dumpdev, dumplo,)) {

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

	default:
		printf("error %d\n", error);
		break;

	case 0:
		printf("succeeded\n");
	}
#endif
}

void
initcpu()
{
}

/*
 * Convert "xx:xx:xx:xx:xx:xx" string to ethernet hardware address.
 */
void
enaddr_aton(const char *s, u_int8_t *a)
{
	int i;

	if (s != NULL) {
		for(i = 0; i < 6; i++) {
			a[i] = atoi(s, 16, &s);
			if (*s == ':')
				s++;
		}
	}
}

/*
 * Convert an ASCII string into an integer.
 */
static int
atoi(const char *s, int b, const char **o)
{
	int c;
	unsigned base = b, d;
	int neg = 0, val = 0;

	if (s == NULL || *s == 0) {
		if (o != NULL)
			*o = s;
		return 0;
	}

	/* skip spaces if any */
	do {
		c = *s++;
	} while (c == ' ' || c == '\t');

	/* parse sign, allow more than one (compat) */
	while (c == '-') {
		neg = !neg;
		c = *s++;
	}

	/* parse base specification, if any */
	if (c == '0') {
		c = *s++;
		switch (c) {
		case 'X':
		case 'x':
			base = 16;
			c = *s++;
			break;
		case 'B':
		case 'b':
			base = 2;
			c = *s++;
			break;
		default:
			base = 8;
		}
	}

	/* parse number proper */
	for (;;) {
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'z')
			d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'Z')
			d = c - 'A' + 10;
		else
			break;
		val *= base;
		val += d;
		c = *s++;
	}
	if (neg)
		val = -val;
	if (o != NULL)
		*o = s - 1;
	return val;
}

/*
 *  RM7000 Performance counter support.
 */

int
rm7k_perfcntr(cmd, arg1, arg2, arg3)
	int cmd;
	long  arg1, arg2, arg3;
{
	int result;
	quad_t cntval;
	struct proc *p = curproc;


	switch(cmd) {
	case PCNT_FNC_SELECT:
		if ((arg1 & 0xff) > PCNT_SRC_MAX ||
		   (arg1 & ~(PCNT_CE|PCNT_UM|PCNT_KM|0xff)) != 0) {
			result = EINVAL;
			break;
		}
printf("perfcnt select %x, proc %p\n", arg1, p);
		p->p_md.md_pc_count = 0;
		p->p_md.md_pc_spill = 0;
		p->p_md.md_pc_ctrl = arg1;
		result = 0;
		break;

	case PCNT_FNC_READ:
		cntval = p->p_md.md_pc_count;
		cntval += (quad_t)p->p_md.md_pc_spill << 31;
		result = copyout(&cntval, (void *)arg1, sizeof(cntval));
		break;

	default:
printf("perfcnt error %d\n", cmd);
		result = -1;
		break;
	}
	return(result);
}

/*
 *  Called when the performance counter d31 gets set.
 *  Increase spill value and reset d31.
 */
void
rm7k_perfintr(trapframe)
	struct trap_frame *trapframe;
{
	struct proc *p = curproc;

	printf("perfintr proc %p!\n", p);
	cp0_setperfcount(cp0_getperfcount() & 0x7fffffff);
	if (p != NULL) {
		p->p_md.md_pc_spill++;
	}
}

int
rm7k_watchintr(trapframe)
	struct trap_frame *trapframe;
{
	return(0);
}

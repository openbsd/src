/*	$OpenBSD: machdep.c,v 1.21 2010/06/27 03:03:48 thib Exp $ */

/*
 * Copyright (c) 2009, 2010 Miodrag Vallat.
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
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/msgbuf.h>
#include <sys/tty.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/exec_elf.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/db_machdep.h>
#include <ddb/db_interface.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <machine/memconf.h>
#include <machine/pmon.h>

#include <dev/cons.h>

#include <mips64/archtype.h>

/* The following is used externally (sysctl_hw) */
char	machine[] = MACHINE;		/* Machine "architecture" */
char	cpu_model[30];
char	pmon_bootp[80];

/*
 * Declare these as initialized data so we can patch them.
 */
#ifndef	BUFCACHEPERCENT
#define	BUFCACHEPERCENT	5	/* Can be changed in config. */
#endif
#ifndef	BUFPAGES
#define BUFPAGES 0		/* Can be changed in config. */
#endif
int	bufpages = BUFPAGES;
int	bufcachepercent = BUFCACHEPERCENT;

/*
 * Even though the system is 64bit, the hardware is constrained to up
 * to 2G of contigous physical memory (direct 2GB DMA area), so there
 * is no particular constraint. paddr_t is long so: 
 */
struct uvm_constraint_range  dma_constraint = { 0x0, 0xffffffffUL };
struct uvm_constraint_range *uvm_md_constraints[] = { NULL };

vm_map_t exec_map;
vm_map_t phys_map;

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

caddr_t	msgbufbase;
vaddr_t	uncached_base;

int	physmem;		/* Max supported memory, changes to actual. */
int	ncpu = 1;		/* At least one CPU in the system. */
struct	user *proc0paddr;
int	kbd_reset;

const struct platform *sys_platform;
struct cpu_hwinfo bootcpu_hwinfo;
uint loongson_ver;

/* Pointers to the start and end of the symbol table. */
caddr_t	ssym;
caddr_t	esym;
caddr_t	ekern;

struct phys_mem_desc mem_layout[MAXMEMSEGS];

static u_long atoi(const char *, uint);
static void dobootopts(int);

void	build_trampoline(vaddr_t, vaddr_t);
void	dumpsys(void);
void	dumpconf(void);
extern	void parsepmonbp(void);
vaddr_t	mips_init(int32_t, int32_t, int32_t, int32_t, char *);

extern	void loongson2e_setup(u_long, u_long);
extern	void loongson2f_setup(u_long, u_long);

cons_decl(pmon);

struct consdev pmoncons = {
	NULL,
	NULL,
	pmoncngetc,
	pmoncnputc,
	nullcnpollc,
	NULL,
	makedev(0, 0),
	CN_DEAD
};

/*
 * List of supported system types, from the ``Version'' environment
 * variable.
 */

struct bonito_flavour {
	const char *prefix;
	const struct platform *platform;
};

extern const struct platform fuloong_platform;
extern const struct platform gdium_platform;
extern const struct platform generic2e_platform;
extern const struct platform lynloong_platform;
extern const struct platform yeeloong_platform;

const struct bonito_flavour bonito_flavours[] = {
	/* Lemote Fuloong 2F mini-PC */
	{ "LM6003",	&fuloong_platform },
	{ "LM6004",	&fuloong_platform },
	/* EMTEC Gdium Liberty 1000 */
	{ "Gdium",	&gdium_platform },
	/* Lemote Yeeloong 8.9" netbook */
	{ "LM8089",	&yeeloong_platform },
	/* supposedly Lemote Yeeloong 10.1" netbook, but those found so far
	   report themselves as LM8089 */
	{ "LM8101",	&yeeloong_platform },
	/* Lemote Lynloong all-in-one computer */
	{ "LM9001",	&lynloong_platform },
	{ NULL }
};

/*
 * Do all the stuff that locore normally does before calling main().
 * Reset mapping and set up mapping to hardware and init "wired" reg.
 */

vaddr_t
mips_init(int32_t argc, int32_t argv, int32_t envp, int32_t cv,
    char *boot_esym)
{
	uint prid;
	u_long memlo, memhi, cpuspeed;
	vaddr_t xtlb_handler;
	const char *envvar;
	int i;
	const struct bonito_flavour *f;

	extern char start[], edata[], end[];
	extern char exception[], e_exception[];
	extern char *hw_vendor, *hw_prod;
	extern void xtlb_miss;

	/*
	 * Clear the compiled BSS segment in OpenBSD code.
	 * PMON is supposed to have done this, though.
	 */

	bzero(edata, end - edata);

	/*
	 * Set up early console output.
	 */

	pmon_init(argc, argv, envp, cv);
	cn_tab = &pmoncons;

	/*
	 * Reserve space for the symbol table, if it exists.
	 */

	/* Attempt to locate ELF header and symbol table after kernel. */
	if (end[0] == ELFMAG0 && end[1] == ELFMAG1 &&
	    end[2] == ELFMAG2 && end[3] == ELFMAG3) {
		/* ELF header exists directly after kernel. */
		ssym = end;
		esym = boot_esym;
		ekern = esym;
	} else {
		ssym = (char *)(vaddr_t)*(int32_t *)end;
		if (((long)ssym - (long)end) >= 0 &&
		    ((long)ssym - (long)end) <= 0x1000 &&
		    ssym[0] == ELFMAG0 && ssym[1] == ELFMAG1 &&
		    ssym[2] == ELFMAG2 && ssym[3] == ELFMAG3) {
			/* Pointers exist directly after kernel. */
			esym = (char *)(vaddr_t)*((int32_t *)end + 1);
			ekern = esym;
		} else {
			/* Pointers aren't setup either... */
			ssym = NULL;
			esym = NULL;
			ekern = end;
		}
	}

	/*
	 * Try and figure out what kind of hardware we are.
	 */

	envvar = pmon_getenv("systype");
	if (envvar == NULL) {
		pmon_printf("Unable to figure out system type!\n");
		goto unsupported;
	}
	if (strcmp(envvar, "Bonito") != 0) {
		pmon_printf("This kernel doesn't support system type \"%s\".\n",
		    envvar);
		goto unsupported;
	}

	/*
	 * While the kernel supports other processor types than Loongson,
	 * we are not expecting a Bonito-based system with a different
	 * processor.  Just to be on the safe side, refuse to run on
	 * non Loongson2 processors for now.
	 */

	prid = cp0_get_prid();
	switch ((prid >> 8) & 0xff) {
	case MIPS_LOONGSON2:
		loongson_ver = 0x2c + (prid & 0xff);
		if (loongson_ver == 0x2e || loongson_ver == 0x2f)
			break;
		/* FALLTHROUGH */
	default:
		pmon_printf("This kernel doesn't support processor type 0x%x"
		    ", version %d.%d.\n",
		    (prid >> 8) & 0xff, (prid >> 4) & 0x0f, prid & 0x0f);
		goto unsupported;
	}

	/*
	 * Try to figure out what particular machine we run on, depending
	 * on the PMON version information.
	 */

	envvar = pmon_getenv("Version");
	if (envvar == NULL) {
		/*
		 * If this is a 2E system, use the generic code and hope
		 * for the best.
		 */
		if (loongson_ver == 0x2e) {
			sys_platform = &generic2e_platform;
		} else {
			pmon_printf("Unable to figure out model!\n");
			goto unsupported;
		}
	} else {
		for (f = bonito_flavours; f->prefix != NULL; f++)
			if (strncmp(envvar, f->prefix, strlen(f->prefix)) ==
			    0) {
				sys_platform = f->platform;
				break;
			}

		if (sys_platform == NULL) {
			pmon_printf("This kernel doesn't support model \"%s\"."
			    "\n", envvar);
			goto unsupported;
		}
	}

	hw_vendor = sys_platform->vendor;
	hw_prod = sys_platform->product;
	pmon_printf("Found %s %s, setting up.\n", hw_vendor, hw_prod);

	snprintf(cpu_model, sizeof cpu_model, "Loongson %X", loongson_ver);

	/*
	 * Figure out processor clock speed.
	 * Hopefully the processor speed, in Hertz, will not overflow
	 * uint32_t...
	 */

	cpuspeed = 0;
	envvar = pmon_getenv("cpuclock");
	if (envvar != NULL)
		cpuspeed = atoi(envvar, 10);	/* speed in Hz */
	if (cpuspeed < 100 * 1000000)
		cpuspeed = 797000000;  /* Reasonable default */
	bootcpu_hwinfo.clock = cpuspeed;

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */

	boothowto = RB_AUTOBOOT;
	dobootopts(argc);

	/*
	 * Figure out memory information.
	 * PMON reports it in two chunks, the memory under the 256MB
	 * CKSEG limit, and memory above that limit.  We need to do the
	 * math ourselves.
	 */

	envvar = pmon_getenv("memsize");
	if (envvar == NULL) {
		pmon_printf("Could not get memory information"
		    " from the firmware\n");
		goto unsupported;
	}
	memlo = atoi(envvar, 10);	/* size in MB */
	if (memlo < 0 || memlo > 256) {
		pmon_printf("Incorrect low memory size `%s'\n", envvar);
		goto unsupported;
	}

	if (memlo == 256) {
		envvar = pmon_getenv("highmemsize");
		if (envvar == NULL)
			memhi = 0;
		else
			memhi = atoi(envvar, 10);	/* size in MB */
		if (memhi < 0 || memhi > (64 * 1024) - 256) {
			pmon_printf("Incorrect high memory size `%s'\n",
			    envvar);
			/* better expose the problem than limit to 256MB */
			goto unsupported;
		}
	}

	uncached_base = PHYS_TO_XKPHYS(0, CCA_NC);

	switch (loongson_ver) {
	case 0x2e:
		loongson2e_setup(memlo, memhi);
		break;
	default:
	case 0x2f:
		loongson2f_setup(memlo, memhi);
		break;
	}

	if (sys_platform->setup != NULL)
		(*(sys_platform->setup))();

	/*
	 * PMON functions should no longer be used from now on.
	 */

	/*
	 * Set pagesize to enable use of page macros and functions.
	 * Commit available memory to UVM system.
	 */

	uvmexp.pagesize = PAGE_SIZE;
	uvm_setpagesize();

	for (i = 0; i < MAXMEMSEGS && mem_layout[i].mem_last_page != 0; i++) {
		uint64_t fp, lp;
		uint64_t firstkernpage, lastkernpage;
		unsigned int freelist;
		paddr_t firstkernpa, lastkernpa;

		/* kernel is linked in CKSEG0 */
		firstkernpa = CKSEG0_TO_PHYS((vaddr_t)start);
		lastkernpa = CKSEG0_TO_PHYS((vaddr_t)ekern);

		firstkernpage = atop(trunc_page(firstkernpa)) +
		    mem_layout[0].mem_first_page - 1;
		lastkernpage = atop(round_page(lastkernpa)) +
		    mem_layout[0].mem_first_page - 1;

		fp = mem_layout[i].mem_first_page;
		lp = mem_layout[i].mem_last_page;
		freelist = mem_layout[i].mem_freelist;

		/* Account for kernel and kernel symbol table. */
		if (fp >= firstkernpage && lp < lastkernpage)
			continue;	/* In kernel. */

		if (lp < firstkernpage || fp > lastkernpage) {
			uvm_page_physload(fp, lp, fp, lp, freelist);
			continue;	/* Outside kernel. */
		}

		if (fp >= firstkernpage)
			fp = lastkernpage;
		else if (lp < lastkernpage)
			lp = firstkernpage;
		else { /* Need to split! */
			uint64_t xp = firstkernpage;
			uvm_page_physload(fp, xp, fp, xp, freelist);
			fp = lastkernpage;
		}
		if (lp > fp) {
			uvm_page_physload(fp, lp, fp, lp, freelist);
		}
	}

	bootcpu_hwinfo.c0prid = prid;
	bootcpu_hwinfo.type = (prid >> 8) & 0xff;
	/* FPU reports itself as type 5, version 0.1... */
	bootcpu_hwinfo.c1prid = bootcpu_hwinfo.c0prid;
	bootcpu_hwinfo.tlbsize = 64;

	/*
	 * Configure cache.
	 */

	Loongson2_ConfigCache(curcpu());
	Loongson2_SyncCache(curcpu());

	tlb_set_page_mask(TLB_PAGE_MASK);
	tlb_set_wired(0);
	tlb_flush(bootcpu_hwinfo.tlbsize);
	tlb_set_wired(UPAGES / 2);

	/*
	 * Get a console, very early but after initial mapping setup.
	 */

	consinit();
	printf("Initial setup done, switching console.\n");

	/*
	 * Init message buffer.
	 */

	msgbufbase = (caddr_t)pmap_steal_memory(MSGBUFSIZE, NULL,NULL);
	initmsgbuf(msgbufbase, MSGBUFSIZE);

	/*
	 * Allocate U page(s) for proc[0], pm_tlbpid 1.
	 */

	proc0.p_addr = proc0paddr = curcpu()->ci_curprocpaddr =
	    (struct user *)pmap_steal_memory(USPACE, NULL, NULL);
	proc0.p_md.md_regs = (struct trap_frame *)&proc0paddr->u_pcb.pcb_regs;
	tlb_set_pid(1);

	/*
	 * Bootstrap VM system.
	 */

	pmap_bootstrap();

	/*
	 * Copy down exception vector code.
	 */

	bcopy(exception, (char *)CACHE_ERR_EXC_VEC, e_exception - exception);
	bcopy(exception, (char *)GEN_EXC_VEC, e_exception - exception);

	/*
	 * Build proper TLB refill handler trampolines.
	 */

	xtlb_handler = (vaddr_t)&xtlb_miss;
	build_trampoline(TLB_MISS_EXC_VEC, xtlb_handler);

	/*
	 * Turn off bootstrap exception vectors.
	 * (this is done by PMON already, but it doesn't hurt to be safe)
	 */

	setsr(getsr() & ~SR_BOOT_EXC_VEC);
	proc0.p_md.md_regs->sr = getsr();

#ifdef DDB
	db_machine_init();
	if (boothowto & RB_KDB)
		Debugger();
#endif

	/*
	 * Return the new kernel stack pointer.
	 */

	return ((vaddr_t)proc0paddr + USPACE - 64);

unsupported:
	pmon_printf("Halting system.\nPress enter to return to PMON\n");
	cngetc();
	return 0;	/* causes us to return to pmon */
}

/*
 * Build a tlb trampoline
 */
void
build_trampoline(vaddr_t addr, vaddr_t dest)
{
	const uint32_t insns[] = {
		0x3c1a0000,	/* lui k0, imm16 */
		0x675a0000,	/* daddiu k0, k0, imm16 */
		0x001ad438,	/* dsll k0, k0, 0x10 */
		0x675a0000,	/* daddiu k0, k0, imm16 */
		0x001ad438,	/* dsll k0, k0, 0x10 */
		0x675a0000,	/* daddiu k0, k0, imm16 */
		0x03400008,	/* jr k0 */
		0x00000000	/* nop */
	};
	uint32_t *dst = (uint32_t *)addr;
	const uint32_t *src = insns;
	uint32_t a, b, c, d;

	/*
	 * Decompose the handler address in the four components which,
	 * added with sign extension, will produce the correct address.
	 */
	d = dest & 0xffff;
	dest >>= 16;
	if (d & 0x8000)
		dest++;
	c = dest & 0xffff;
	dest >>= 16;
	if (c & 0x8000)
		dest++;
	b = dest & 0xffff;
	dest >>= 16;
	if (b & 0x8000)
		dest++;
	a = dest & 0xffff;

	/*
	 * Build the trampoline, skipping noop computations.
	 */
	*dst++ = *src++ | a;
	if (b != 0)
		*dst++ = *src++ | b;
	else
		src++;
	*dst++ = *src++;
	if (c != 0)
		*dst++ = *src++ | c;
	else
		src++;
	*dst++ = *src++;
	if (d != 0)
		*dst++ = *src++ | d;
	else
		src++;
	*dst++ = *src++;
	*dst++ = *src++;

	/*
	 * Note that we keep the delay slot instruction a nop, instead
	 * of branching to the second instruction of the handler and
	 * having its first instruction in the delay slot, so that the
	 * tlb handler is free to use k0 immediately.
	 */
}

/*
 * Decode boot options.
 */
static void
dobootopts(int argc)
{
	const char *arg;
	const char *cp;
	int ignore = 1;
	int i;

	/*
	 * Parse the boot command line.
	 *
	 * It should be of the form `boot [flags] filename [args]', so we
	 * need to ignore flags to the boot command.
	 * To achieve this, we ignore argc[0], which is the `boot' command
	 * itself, and ignore arguments starting with dashes until the
	 * boot file has been found.
	 */

	if (argc != 0) {
		arg = pmon_getarg(0);
		if (arg == NULL)
			return;
		/* if `go', not `boot', then no path and command options */
		if (*arg == 'g')
			ignore = 0;
	}
	for (i = 1; i < argc; i++) {
		arg = pmon_getarg(i);
		if (arg == NULL)
			continue;

		/* device path */
		if (*arg == '/' || strncmp(arg, "tftp://", 7) == 0) {
			if (*pmon_bootp == '\0') {
				strlcpy(pmon_bootp, arg, sizeof pmon_bootp);
				parsepmonbp();
			}
			ignore = 0;	/* further options are for the kernel */
			continue;
		}

		/* not an option, or not a kernel option */
		if (*arg != '-' || ignore)
			continue;

		for (cp = arg + 1; *cp != '\0'; cp++)
			switch (*cp) {
			case '-':
				break;
			case 'a':
				boothowto |= RB_ASKNAME;
				break;
			case 'c':
				boothowto |= RB_CONFIG;
				break;
			case 'd':
				boothowto |= RB_KDB;
				break;
			case 's':
				boothowto |= RB_SINGLE;
				break;
			default:
				pmon_printf("unrecognized option `%c'", *cp);
				break;
			}
	}

	/*
	 * Consider parsing the `karg' environment variable here too?
	 */
}


/*
 * Console initialization: called early on from main, before vm init or startup.
 * Do enough configuration to choose and initialize a console.
 */
void
consinit()
{
	static int console_ok = 0;

	if (console_ok == 0) {
		cninit();
		console_ok = 1;
	}
}

/*
 * cpu_startup: allocate memory for variable-sized tables, initialize CPU, and 
 * do auto-configuration.
 */
void
cpu_startup()
{
	vaddr_t minaddr, maxaddr;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("real mem = %u (%uMB)\n", ptoa((psize_t)physmem),
	    ptoa((psize_t)physmem)/1024/1024);

	/*
	 * Allocate a submap for exec arguments. This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    16 * NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);
	/* Allocate a submap for physio. */
	phys_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, 0, FALSE, NULL);

	printf("avail mem = %u (%uMB)\n", ptoa(uvmexp.free),
	    ptoa(uvmexp.free)/1024/1024);

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
 * Machine dependent system variables.
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
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return ENOTDIR;		/* Overloaded */

	switch (name[0]) {
	case CPU_KBDRESET:
		if (securelevel > 0)
			return (sysctl_rdint(oldp, oldlenp, newp, kbd_reset));
		return (sysctl_int(oldp, oldlenp, newp, newlen, &kbd_reset));
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
	struct cpu_info *ci = curcpu();

	bzero((caddr_t)p->p_md.md_regs, sizeof(struct trap_frame));
	p->p_md.md_regs->sp = stack;
	p->p_md.md_regs->pc = pack->ep_entry & ~3;
	p->p_md.md_regs->t9 = pack->ep_entry & ~3; /* abicall req */
	p->p_md.md_regs->sr = SR_FR_32 | SR_XX | SR_KSU_USER | SR_KX | SR_UX |
	    SR_EXL | SR_INT_ENAB;
	p->p_md.md_regs->sr |= idle_mask & SR_INT_MASK;
	p->p_md.md_regs->ic = (idle_mask << 8) & IC_INT_MASK;
	p->p_md.md_flags &= ~MDP_FPUSED;
	if (ci->ci_fpuproc == p)
		ci->ci_fpuproc = NULL;
	p->p_md.md_ss_addr = 0;
	p->p_md.md_pc_ctrl = 0;
	p->p_md.md_watch_1 = 0;
	p->p_md.md_watch_2 = 0;

	retval[1] = 0;
}


int	waittime = -1;

void
boot(int howto)
{

	/* Take a snapshot before clobbering any registers. */
	if (curproc)
		savectx(curproc->p_addr, 0);

	if (cold) {
		/*
		 * If the system is cold, just halt, unless the user
		 * explicitly asked for reboot.
		 */
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;
		/* fill curproc with live object */
		if (curproc == NULL)
			curproc = &proc0;
		/*
		 * Synchronize the disks...
		 */
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr will be out of
		 * sync; adjust it now.
		 */
		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}

	uvm_shutdown();
	(void) splhigh();		/* Extreme priority. */

	if (howto & RB_DUMP)
		dumpsys();

haltsys:
	doshutdownhooks();

	if (howto & RB_HALT) {
		if (howto & RB_POWERDOWN) {
			if (sys_platform->powerdown != NULL) {
				printf("System Power Down.\n");
				(*(sys_platform->powerdown))();
			} else {
				printf("System Power Down not supported,"
				    " halting system.\n");
			}
		} else
			printf("System Halt.\n");
	} else {
		void (*__reset)(void) = (void (*)(void))RESET_EXC_VEC;
		printf("System restart.\n");
		if (sys_platform->reset != NULL)
			(*(sys_platform->reset))();
		(void)disableintr();
		tlb_set_wired(0);
		tlb_flush(bootcpu_hwinfo.tlbsize);
		__reset();
	}

	for (;;) ;
	/*NOTREACHED*/
}

u_long	dumpmag = 0x8fca0101;	/* Magic number for savecore. */
int	dumpsize = 0;			/* Also for savecore. */
long	dumplo = 0;

void
dumpconf(void)
{
	int nblks;

	if (dumpdev == NODEV ||
	    (nblks = (bdevsw[major(dumpdev)].d_psize)(dumpdev)) == 0)
		return;
	if (nblks <= ctod(1))
		return;

	dumpsize = ptoa(physmem);
	if (dumpsize > atop(round_page(dbtob(nblks - dumplo))))
		dumpsize = atop(round_page(dbtob(nblks - dumplo)));
	else if (dumplo == 0)
		dumplo = nblks - btodb(ptoa(physmem));

	/*
	 * Don't dump on the first page in case the dump device includes a 
	 * disk label.
	 */
	if (dumplo < btodb(PAGE_SIZE))
		dumplo = btodb(PAGE_SIZE);
}

void
dumpsys()
{
	/* XXX TBD */
}

/*
 * Convert an ASCII string into an integer.
 */
static u_long
atoi(const char *s, uint b)
{
	int c;
	uint base = b, d;
	int neg = 0;
	u_long val = 0;

	if (s == NULL || *s == '\0')
		return 0;

	/* Skip spaces if any. */
	do {
		c = *s++;
	} while (c == ' ' || c == '\t');

	/* Parse sign, allow more than one (compat). */
	while (c == '-') {
		neg = !neg;
		c = *s++;
	}

	/* Parse base specification, if any. */
	if (base == 0 && c == '0') {
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
			break;
		}
	}

	/* Parse number proper. */
	for (;;) {
		if (c >= '0' && c <= '9')
			d = c - '0';
		else if (c >= 'a' && c <= 'z')
			d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'Z')
			d = c - 'A' + 10;
		else
			break;
		if (d >= base)
			break;
		val *= base;
		val += d;
		c = *s++;
	}

	return neg ? -val : val;
}

/*
 * Early console through pmon routines.
 */

int
pmoncngetc(dev_t dev)
{
	/*
	 * PMON does not give us a getc routine.  So try to get a whole line
	 * and return it char by char, trying not to lose the \n.  Kind
	 * of ugly but should work.
	 *
	 * Note that one could theoretically use pmon_read(STDIN, &c, 1)
	 * but the value of STDIN within PMON is not a constant and there
	 * does not seem to be a way of letting us know which value to use.
	 */
	static char buf[1 + PMON_MAXLN];
	static char *bufpos = buf;
	int c;

	if (*bufpos == '\0') {
		bufpos = buf;
		if (pmon_gets(buf) == NULL) {
			/* either an empty line or EOF. assume the former */
			return (int)'\n';
		} else {
			/* put back the \n sign */
			buf[strlen(buf)] = '\n';
		}
	}

	c = (int)*bufpos++;
	if (bufpos - buf > PMON_MAXLN) {
		bufpos = buf;
		*bufpos = '\0';
	}

	return c;
}

void
pmoncnputc(dev_t dev, int c)
{
	if (c == '\n')
		pmon_printf("\n");
	else
		pmon_printf("%c", c);
}

/*	$OpenBSD: sun3_startup.c,v 1.11 2000/03/02 23:02:14 todd Exp $	*/
/*	$NetBSD: sun3_startup.c,v 1.55 1996/11/20 18:57:38 gwr Exp $	*/

/*-
 * Copyright (c) 1996 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Adam Glass and Gordon W. Ross.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/user.h>
#include <sys/exec_aout.h>
#include <sys/msgbuf.h>

#include <vm/vm.h>

#include <machine/control.h>
#include <machine/cpu.h>
#include <machine/dvma.h>
#include <machine/idprom.h>
#include <machine/machdep.h>
#include <machine/mon.h>
#include <machine/obio.h>
#include <machine/obmem.h>
#include <machine/pmap.h>
#include <machine/pte.h>

#include "vector.h"
#include "interreg.h"

/* This is defined in locore.s */
extern char kernel_text[];

/* These are defined by the linker */
extern char etext[], edata[], end[];
char *esym;	/* DDB */

/*
 * Globals shared with the pmap code.
 * XXX - should reexamine this...
 */
vm_offset_t virtual_avail, virtual_end;
vm_offset_t avail_start, avail_end;
/* used to skip the Sun3/50 video RAM */
vm_offset_t hole_start, hole_size;
int cache_size;

/*
 * Now our own stuff.
 */
void **old_vector_table;

unsigned char cpu_machine_id = 0;
char *cpu_string = NULL;
int cpu_has_vme = 0;

vm_offset_t high_segment_free_start = 0;
vm_offset_t high_segment_free_end = 0;

struct msgbuf *msgbufp = NULL;
extern vm_offset_t tmp_vpages[];
extern int physmem;
unsigned char *interrupt_reg;

vm_offset_t proc0_user_pa;
struct user *proc0paddr;	/* proc[0] pcb address (u-area VA) */
extern struct pcb *curpcb;

extern vm_offset_t dumppage_pa;
extern vm_offset_t dumppage_va;

void sun3_bootstrap __P((struct exec));

static void sun3_mode_monitor __P((void));
static void sun3_mode_normal __P((void));
static void sun3_mon_init __P((vm_offset_t sva, vm_offset_t eva, int keep));
static void sun3_monitor_hooks __P((void));
static void sun3_context_equiv __P((void));
static void sun3_save_symtab __P((struct exec *kehp));
static void sun3_verify_hardware __P((void));
static void sun3_vm_init __P((struct exec *kehp));
static void tracedump __P((int));
static void v_handler __P((int addr, char *str));

static void internal_configure __P((void));

vm_offset_t
high_segment_alloc(npages)
	int npages;
{
	vm_offset_t va, tmp;

	if (npages == 0)
		mon_panic("panic: request for high segment allocation of 0 pages");
	if (high_segment_free_start == high_segment_free_end) return NULL;

	va = high_segment_free_start + (npages*NBPG);
	if (va > high_segment_free_end) return NULL;
	tmp = high_segment_free_start;
	high_segment_free_start = va;
	return tmp;
}

/*
 * Prepare for running the PROM monitor
 */
static void sun3_mode_monitor()
{
	/* Install PROM vector table and enable NMI clock. */
	/* XXX - Disable watchdog action? */
	set_clk_mode(0, IREG_CLOCK_ENAB_5, 0);
	setvbr(old_vector_table);
	set_clk_mode(IREG_CLOCK_ENAB_7, 0, 1);
}

/*
 * Prepare for running the kernel
 */
static void
sun3_mode_normal()
{
	/* Install our vector table and disable the NMI clock. */
	set_clk_mode(0, IREG_CLOCK_ENAB_7, 0);
	setvbr((void **) vector_table);
	set_clk_mode(IREG_CLOCK_ENAB_5, 0, 1);
}

/*
 * This function takes care of restoring enough of the
 * hardware state to allow the PROM to run normally.
 * The PROM needs: NMI enabled, it's own vector table.
 * In case of a temporary "drop into PROM", this will
 * also put our hardware state back into place after
 * the PROM "c" (continue) command is given.
 */
void
sun3_mon_abort()
{
	int s = splhigh();

	sun3_mode_monitor();
	mon_printf("kernel stop: enter c to continue or g0 to panic\n");
	delay(100000);

	/*
	 * Drop into the PROM in a way that allows a continue.
	 * That's what the PROM function (romp->abortEntry) is for,
	 * but that wants to be entered as a trap hander, so just
	 * stuff it into the PROM interrupt vector for trap zero
	 * and then do a trap.  Needs PROM vector table in RAM.
	 */
	old_vector_table[32] = (void *)romp->abortEntry;
	asm(" trap #0 ; _sun3_mon_continued: nop");

	/* We have continued from a PROM abort! */

	sun3_mode_normal();
	splx(s);
}

void
sun3_mon_halt()
{
	(void) splhigh();
	sun3_mode_monitor();
	mon_exit_to_mon();
	/*NOTREACHED*/
}

void
sun3_mon_reboot(bootstring)
	char *bootstring;
{
	(void) splhigh();
	sun3_mode_monitor();
	mon_reboot(bootstring);
	mon_exit_to_mon();
	/*NOTREACHED*/
}

/*
 * Duplicate all mappings in the current context into
 * every other context.  We have to let the PROM do the
 * actual segmap manipulation because we can only switch
 * the MMU context after we are sure that the kernel text
 * is identically mapped in all contexts.  The PROM can
 * do the job using hardware-dependent tricks...
 */
static void
sun3_context_equiv()
{
	unsigned int sme;
	int x;
	vm_offset_t va;

#ifdef	DIAGNOSTIC
	/* Near the beginning of locore.s we set context zero. */
	if (get_context() != 0)
		mon_panic("sun3_context_equiv: not in context zero?\n");
	/* Note: PROM setcxsegmap function needs sfc=dfs=FC_CONTROL */
	if (getsfc() != FC_CONTROL)
		mon_panic("sun3_context_equiv: sfc != FC_CONTROL?\n");
	if (getdfc() != FC_CONTROL)
		mon_panic("sun3_context_equiv: dfc != FC_CONTROL?\n");
#endif

	for (x = 1; x < NCONTEXT; x++) {
		for (va = 0; va < (vm_offset_t) (NBSG * NSEGMAP); va += NBSG) {
			sme = get_segmap(va);
			mon_setcxsegmap(x, va, sme);
		}
	}
}

static void
sun3_mon_init(sva, eva, keep)
vm_offset_t sva, eva;
int keep;	/* true: steal, false: clear */
{
	vm_offset_t pgva, endseg;
	int pte, valid;
	unsigned char sme;

	sva &= ~(NBSG-1);

	while (sva < eva) {
		sme = get_segmap(sva);
		if (sme != SEGINV) {
#ifdef	DEBUG
			mon_printf("mon va=0x%x seg=0x%x\n", sva, sme);
#endif
			valid = 0;
			endseg = sva + NBSG;
			for (pgva = sva; pgva < endseg; pgva += NBPG) {
				pte = get_pte(pgva);
				if (pte & PG_VALID) {
					valid++;
#ifdef	DEBUG
					mon_printf("mon va=0x%x pte=0x%x\n", pgva, pte);
#endif
				}
			}
			if (keep && valid)
				sun3_reserve_pmeg(sme);
			else set_segmap(sva, SEGINV);
		}
		sva += NBSG;
	}
}

#if defined(DDB) && !defined(SYMTAB_SPACE)
/*
 * Preserve DDB symbols and strings by setting esym.
 */
void
sun3_save_symtab(kehp)
	struct exec *kehp;	/* kernel exec header */
{
	int x, *symsz, *strsz;
	char *endp;
	char *errdesc = "?";

	/*
	 * First, sanity-check the exec header.
	 */
	mon_printf("sun3_save_symtab: ");
	if ((kehp->a_midmag & 0xFFF0) != 0x100) {
		errdesc = "magic";
		goto err;
	}
	/* Boundary between text and data varries a little. */
	x = kehp->a_text + kehp->a_data;
	if (x != (edata - kernel_text)) {
		errdesc = "a_text+a_data";
		goto err;
	}
	if (kehp->a_bss != (end - edata)) {
		errdesc = "a_bss";
		goto err;
	}
	if (kehp->a_entry != (int)kernel_text) {
		errdesc = "a_entry";
		goto err;
	}
	if (kehp->a_trsize || kehp->a_drsize) {
		errdesc = "a_Xrsize";
		goto err;
	}
	/* The exec header looks OK... */

	/* Check the symtab length word. */
	endp = end;
	symsz = (int*)endp;
	if (kehp->a_syms != *symsz) {
		errdesc = "a_syms";
		goto err;
	}
	endp += sizeof(int);	/* past length word */
	endp += *symsz;			/* past nlist array */

	/* Check the string table length. */
	strsz = (int*)endp;
	if ((*strsz < 4) || (*strsz > 0x80000)) {
		errdesc = "strsize";
		goto err;
	}

	/* Success!  We have a valid symbol table! */
	endp += *strsz;			/* past strings */
	esym = endp;
	mon_printf(" found %d + %d\n", *symsz, *strsz);
	return;

 err:
	mon_printf(" no symbols (bad %s)\n", errdesc);
}
#endif	/* DDB && !SYMTAB_SPACE */

/*
 * This is called just before pmap_bootstrap()
 * (from sun3_bootstrap(), below) to initialize enough
 * to allow the VM system to run normally.  This involves
 * allocating some physical pages and virtual space for
 * special purposes, etc. by advancing avail_start and
 * virtual_avail past the "stolen" pages.  Note that
 * the kernel should never take a fault on any page
 * between [ KERNBASE .. virtual_avail ] and this is
 * checked in trap.c for kernel-mode MMU faults.
 */
void
sun3_vm_init(kehp)
	struct exec *kehp;	/* kernel exec header */
{
	vm_offset_t va, eva, pte;
	unsigned int sme;

	/*
	 * Determine the range of kernel virtual space available.
	 * This is just page-aligned for now, so we can allocate
	 * some special-purpose pages before rounding to a segment.
	 */
	esym = end;
#if defined(DDB) && !defined(SYMTAB_SPACE)
	/* This will advance esym past the symbols. */
	sun3_save_symtab(kehp);
#endif
	virtual_avail = m68k_round_page(esym);
	virtual_end = VM_MAX_KERNEL_ADDRESS;

	/*
	 * Determine the range of physical memory available.
	 * Physical memory at zero was remapped to KERNBASE.
	 */
	avail_start = virtual_avail - KERNBASE;
	if (romp->romvecVersion < 1) {
		mon_printf("WARNING: ancient PROM version=%d\n",
				   romp->romvecVersion);
		/* Guess that PROM version 0.X used two pages. */
		avail_end = *romp->memorySize - (2*NBPG);
	} else {
		/* PROM version 1 or later. */
		avail_end = *romp->memoryAvail;
	}
	avail_end = m68k_trunc_page(avail_end);

	/*
	 * Steal some special-purpose, already mapped pages.
	 * First take pages that are already mapped as
	 * VA -> PA+KERNBASE since that's convenient.
	 */

	/*
	 * Message buffer page (msgbuf).
	 * This is put in physical page zero so it
	 * is always in the same place after reboot.
	 */
	va = KERNBASE;
	/* Make it non-cached. */
	pte = get_pte(va);
	pte |= PG_NC;
	set_pte(va, pte);
	/* offset by half a page to avoid PROM scribbles */
	msgbufp = (struct msgbuf *)(va + 0x1000);
	initmsgbuf((caddr_t)msgbufp, round_page(MSGBUFSIZE));

	/*
	 * Virtual and physical pages for proc[0] u-area (already mapped)
	 */
	proc0paddr = (struct user *) virtual_avail;
	proc0_user_pa = avail_start;
	virtual_avail += UPAGES*NBPG;
	avail_start   += UPAGES*NBPG;
#if 0
	/* Make them non-cached.
	 * XXX - Make these non-cached at their full-time mapping address.
	 * XXX - Still need to do that? -gwr
	 */
	va = (vm_offset_t) proc0paddr;
	while (va < virtual_avail) {
		pte = get_pte(va);
		pte |= PG_NC;
		set_pte(va, pte);
		va += NBPG;
	}
#endif

	/*
	 * Virtual and physical page used by dumpsys()
	 */
	dumppage_va = virtual_avail;
	dumppage_pa = avail_start;
	virtual_avail += NBPG;
	avail_start   += NBPG;

	/*
	 * XXX - Make sure avail_start is within the low 1M range
	 * that the Sun PROM guarantees will be mapped in?
	 * Make sure it is below avail_end as well?
	 */

	/*
	 * Now steal some virtual addresses, but
	 * not the physical pages behind them.
	 */
	va = virtual_avail;	/* will clear PTEs from here */

	/*
	 * vpages array:  just some virtual addresses for
	 * temporary mappings in the pmap module (two pages)
	 */
	tmp_vpages[0] = virtual_avail;
	virtual_avail += NBPG;
	tmp_vpages[1] = virtual_avail;
	virtual_avail += NBPG;

	/*
	 * Done allocating PAGES of virtual space, so
	 * clean out the rest of the last used segment.
	 * After this point, virtual_avail is seg-aligned.
	 */
	virtual_avail = m68k_round_seg(virtual_avail);
	while (va < virtual_avail) {
		set_pte(va, PG_INVAL);
		va += NBPG;
	}

	/*
	 * Now that we are done stealing physical pages, etc.
	 * figure out which PMEGs are used by those mappings
	 * and reserve them -- but first, init PMEG management.
	 */
	sun3_pmeg_init();

	/*
	 * Reserve PMEGS for kernel text/data/bss
	 * and the misc pages taken above.
	 */
	va = VM_MIN_KERNEL_ADDRESS;
	while (va < virtual_avail) {
		sme = get_segmap(va);
		if (sme == SEGINV)
			mon_panic("kernel text/data/bss not mapped\n");
		sun3_reserve_pmeg(sme);
		va += NBSG;
	}

	/*
	 * Unmap kernel virtual space (only segments.  if it squished ptes,
	 * bad things might happen).  Also, make sure to leave no valid
	 * segmap entries in the MMU unless pmeg_array records them.
	 */
	va = virtual_avail;
	while (va < virtual_end) {
		set_segmap(va, SEGINV);
		va += NBSG;
	}

	/*
	 * Clear-out pmegs left in DVMA space by the PROM.
	 * DO NOT kill the last one! (owned by the PROM!)
	 */
	va  = m68k_trunc_seg(DVMA_SPACE_START);
	eva = m68k_trunc_seg(DVMA_SPACE_END);  /* Yes trunc! */
	while (va < eva) {
		set_segmap(va, SEGINV);
		va += NBSG;
	}

	/*
	 * Reserve PMEGs used by the PROM monitor:
	 *   need to preserve/protect mappings between
	 *		MONSTART, MONEND.
	 *   free up any pmegs in this range which have no mappings
	 *   deal with the awful MONSHORTSEG/MONSHORTPAGE
	 */
	sun3_mon_init(MONSTART, MONEND, TRUE);

	/*
	 * Make sure the hole between MONEND, MONSHORTSEG is clear.
	 */
	sun3_mon_init(MONEND, MONSHORTSEG, FALSE);

	/*
	 * MONSHORTSEG contains MONSHORTPAGE which is some stupid page
	 * allocated by the PROM monitor.  (PROM data)
	 * We use some of the segment for our u-area mapping.
	 */
	sme = get_segmap(MONSHORTSEG);
	sun3_reserve_pmeg(sme);
	high_segment_free_start = MONSHORTSEG;
	high_segment_free_end = MONSHORTPAGE;

	for (va = high_segment_free_start;
		 va < high_segment_free_end;
		 va += NBPG)
		set_pte(va, PG_INVAL);

	/*
	 * Initialize the "u-area" pages.
	 * Must initialize p_addr before autoconfig or
	 * the fault handler will get a NULL reference.
	 */
	bzero((caddr_t)proc0paddr, USPACE);
	proc0.p_addr = proc0paddr;
	curproc = &proc0;
	curpcb = &proc0paddr->u_pcb;

	/*
	 * XXX  It might be possible to move much of what is
	 * XXX  done after this point into pmap_bootstrap...
	 */

	/*
	 * unmap user virtual segments
	 */
	va = 0;
	while (va < KERNBASE) {	/* starts and ends on segment boundries */
		set_segmap(va, SEGINV);
		va += NBSG;
	}

	/*
	 * Verify protection bits on kernel text/data/bss
	 * All of kernel text, data, and bss are cached.
	 * Text is read-only (except in db_write_ktext).
	 *
	 * Note that the Sun PROM initialized the memory
	 * mapping with everything non-cached...
	 */

	/* text */
	va = (vm_offset_t) kernel_text;
	eva = m68k_trunc_page(etext);
	while (va < eva) {
		pte = get_pte(va);
		if ((pte & (PG_VALID|PG_TYPE)) != PG_VALID) {
			mon_printf("invalid page at 0x%x\n", va);
		}
		pte &= ~(PG_WRITE|PG_NC);
		/* Kernel text is read-only */
		pte |= (PG_SYSTEM);
		set_pte(va, pte);
		va += NBPG;
	}

	/* data and bss */
	eva = m68k_round_page(end);
	while (va < eva) {
		pte = get_pte(va);
		if ((pte & (PG_VALID|PG_TYPE)) != PG_VALID) {
			mon_printf("invalid page at 0x%x\n", va);
		}
		pte &= ~(PG_NC);
		pte |= (PG_SYSTEM | PG_WRITE);
		set_pte(va, pte);
		va += NBPG;
	}

	/* Finally, duplicate the mappings into all contexts. */
	sun3_context_equiv();
}


/*
 * XXX - Should empirically estimate the divisor...
 * Note that the value of delay_divisor is roughly
 * 2048 / cpuclock	(where cpuclock is in MHz).
 */
int delay_divisor = 82;		/* assume the fastest (3/260) */

void
sun3_verify_hardware()
{
	unsigned char machtype;
	int cpu_match = 0;

	if (idprom_init())
		mon_panic("idprom_init failed\n");

	machtype = identity_prom.idp_machtype;
	if ((machtype & CPU_ARCH_MASK) != SUN3_ARCH)
		mon_panic("not a sun3?\n");

	cpu_machine_id = machtype & SUN3_IMPL_MASK;
	switch (cpu_machine_id) {

	case SUN3_MACH_50 :
		cpu_match++;
		hole_start = OBMEM_BW50_ADDR;
		hole_size  = OBMEM_BW2_SIZE;
		cpu_string = "50";
		delay_divisor = 128;	/* 16 MHz */
		break;

	case SUN3_MACH_60 :
		cpu_match++;
		cpu_string = "60";
		delay_divisor = 102;	/* 20 MHz */
		break;

	case SUN3_MACH_110:
		cpu_match++;
		cpu_string = "110";
		delay_divisor = 120;	/* 17 MHz */
		cpu_has_vme = TRUE;
		break;

	case SUN3_MACH_160:
		cpu_match++;
		cpu_string = "160";
		delay_divisor = 120;	/* 17 MHz */
		cpu_has_vme = TRUE;
		break;

	case SUN3_MACH_260:
		cpu_match++;
		cpu_string = "260";
		delay_divisor = 82; 	/* 25 MHz */
		cpu_has_vme = TRUE;
#ifdef	HAVECACHE
		cache_size = 0x10000;	/* 64K */
#endif
		break;

	case SUN3_MACH_E  :
		cpu_match++;
		cpu_string = "E";
		delay_divisor = 102;	/* 20 MHz  XXX: Correct? */
		cpu_has_vme = TRUE;
		break;

	default:
		mon_panic("unknown sun3 model\n");
	}
	if (!cpu_match)
		mon_panic("kernel not configured for the Sun 3 model\n");
}

/*
 * Print out a traceback for the caller - can be called anywhere
 * within the kernel or from the monitor by typing "g4" (for sun-2
 * compatibility) or "w trace".  This causes the monitor to call
 * the v_handler() routine which will call tracedump() for these cases.
 */
struct funcall_frame {
	struct funcall_frame *fr_savfp;
	int fr_savpc;
	int fr_arg[1];
};
/*VARARGS0*/
void
tracedump(x1)
	int x1;
{
	struct funcall_frame *fp = (struct funcall_frame *)(&x1 - 2);
	u_int stackpage = ((u_int)fp) & ~PGOFSET;

	mon_printf("Begin traceback...fp = %x\n", fp);
	do {
		if (fp == fp->fr_savfp) {
			mon_printf("FP loop at %x", fp);
			break;
		}
		mon_printf("Called from %x, fp=%x, args=%x %x %x %x\n",
				   fp->fr_savpc, fp->fr_savfp,
				   fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2], fp->fr_arg[3]);
		fp = fp->fr_savfp;
	} while ( (((u_int)fp) & ~PGOFSET) == stackpage);
	mon_printf("End traceback...\n");
}

/*
 * Handler for monitor vector cmd -
 * For now we just implement the old "g0" and "g4"
 * commands and a printf hack.  [lifted from freed cmu mach3 sun3 port]
 */
void
v_handler(addr, str)
int addr;
char *str;
{

	switch (*str) {
	case '\0':
		/*
		 * No (non-hex) letter was specified on
		 * command line, use only the number given
		 */
		switch (addr) {
		case 0:			/* old g0 */
		case 0xd:		/* 'd'ump short hand */
			sun3_mode_normal();
			panic("zero");
			/*NOTREACHED*/

		case 4:			/* old g4 */
			goto do_trace;
			break;

		default:
			goto err;
		}
		break;

	case 'p':			/* 'p'rint string command */
	case 'P':
		mon_printf("%s\n", (char *)addr);
		break;

	case '%':			/* p'%'int anything a la printf */
		mon_printf(str, addr);
		mon_printf("\n");
		break;

	do_trace:
	case 't':			/* 't'race kernel stack */
	case 'T':
		tracedump(addr);
		break;

	case 'u':			/* d'u'mp hack ('d' look like hex) */
	case 'U':
		goto err;
		break;

	default:
	err:
		mon_printf("Don't understand 0x%x '%s'\n", addr, str);
	}
}

/*
 * Set the PROM vector handler (for g0, g4, etc.)
 * and set boothowto from the PROM arg strings.
 *
 * Note, args are always:
 * argv[0] = boot_device	(i.e. "sd(0,0,0)")
 * argv[1] = options	(i.e. "-ds" or NULL)
 * argv[2] = NULL
 */
void
sun3_monitor_hooks()
{
	MachMonBootParam *bpp;
	char **argp;
	char *p;

	if (romp->romvecVersion >= 2)
		*romp->vector_cmd = v_handler;

	/* Set boothowto flags from PROM args. */
	bpp = *romp->bootParam;
	argp = bpp->argPtr;

	/* Skip argp[0] (the device string) */
	argp++;

	/* Have options? */
	if (*argp == NULL)
		return;
	p = *argp;
	if (*p == '-') {
		/* yes, parse options */
#ifdef	DEBUG
		mon_printf("boot option: %s\n", p);
#endif
		for (++p; *p; p++) {
			switch (*p) {
			case 'a':
				boothowto |= RB_ASKNAME;
				break;
			case 's':
				boothowto |= RB_SINGLE;
				break;
			case 'd':
				boothowto |= RB_KDB;
				break;
			}
		}
		argp++;
	}

#ifdef	DEBUG
	/* Have init name? */
	if (*argp == NULL)
		return;
	p = *argp;
	mon_printf("boot initpath: %s\n", p);
#endif
}

/*
 * Find mappings for devices that are needed before autoconfiguration.
 * First the obio module finds and records useful PROM mappings, then
 * the necessary drivers are given a chance to use those recorded.
 */
static void
internal_configure()
{
	obio_init();	/* find and record PROM mappings in OBIO space */
	/* Drivers that use those OBIO mappings from the PROM */
	zs_init();
	eeprom_init();
	intreg_init();
	clock_init();
}

/*
 * This is called from locore.s just after the kernel is remapped
 * to its proper address, but before the call to main().
 */
void
sun3_bootstrap(keh)
	struct exec keh;	/* kernel exec header */
{
	extern int cold;

	/* First, Clear BSS. */
	bzero(edata, end - edata);

	cold = 1;

	sun3_monitor_hooks();	/* set v_handler, get boothowto */

	sun3_verify_hardware();	/* get CPU type, etc. */

	sun3_vm_init(&keh);		/* handle kernel mapping, etc. */

	pmap_bootstrap();		/* bootstrap pmap module */

	internal_configure();	/* stuff that can't wait for configure() */

	/*
	 * Point interrupts/exceptions to our table.
	 * This is done after internal_configure/isr_init finds
	 * the interrupt register and disables the NMI clock so
	 * it will not cause "spurrious level 7" complaints.
	 */
	old_vector_table = getvbr();
	setvbr((void **) vector_table);

	/* Interrupts are enabled in locore.s just after this return. */
}

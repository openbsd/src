/*	$NetBSD: machdep.c,v 1.67 1996/10/23 20:04:40 mhitch Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department, The Mach Operating System project at
 * Carnegie-Mellon University and Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)machdep.c	8.3 (Berkeley) 1/12/94
 */

/* from: Utah Hdr: machdep.c 1.63 91/04/24 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/ioctl.h>
#include <sys/tty.h>
#include <sys/device.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <machine/dc7085cons.h>

#include <pmax/stand/dec_prom.h>

#include <pmax/dev/ascreg.h>

#include <machine/autoconf.h>
#include <machine/locore.h>

#include <pmax/pmax/clockreg.h>
#include <pmax/pmax/kn01.h>
#include <pmax/pmax/kn02.h>
#include <pmax/pmax/kmin.h>
#include <pmax/pmax/maxine.h>
#include <pmax/pmax/kn03.h>
#include <pmax/pmax/asic.h>
#include <pmax/pmax/turbochannel.h>
#include <pmax/pmax/pmaxtype.h>
#include <pmax/pmax/cons.h>


#include "pm.h"
#include "cfb.h"
#include "mfb.h"
#include "xcfb.h"
#include "sfb.h"
#include "dtop.h"
#include "scc.h"
#include "le_ioasic.h"
#include "asc.h"

extern void fbPutc();

/* Will scan from max to min, inclusive */
static int tc_max_slot = KN02_TC_MAX;
static int tc_min_slot = KN02_TC_MIN;
static u_int tc_slot_phys_base [TC_MAX_SLOTS] = {
	/* use 3max for default values */
	KN02_PHYS_TC_0_START, KN02_PHYS_TC_1_START,
	KN02_PHYS_TC_2_START, KN02_PHYS_TC_3_START,
	KN02_PHYS_TC_4_START, KN02_PHYS_TC_5_START,
	KN02_PHYS_TC_6_START, KN02_PHYS_TC_7_START
};

/* the following is used externally (sysctl_hw) */
char	machine[] = "pmax";	/* cpu "architecture" */
char	cpu_model[30];

vm_map_t buffer_map;

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
int	msgbufmapped = 0;	/* set when safe to use msgbuf */
int	maxmem;			/* max memory per process */
int	physmem;		/* max supported memory, changes to actual */
int	physmem_boardmax;	/* {model,simm}-specific bound on physmem */
int	pmax_boardtype;		/* Mother board type */
u_long	le_iomem;		/* 128K for lance chip via. ASIC */
u_long	asc_iomem;		/* and 7 * 8K buffers for the scsi */
u_long	ioasic_base;		/* Base address of I/O asic */
const	struct callback *callv;	/* pointer to PROM entry points */

extern void	(*tc_enable_interrupt)  __P ((u_int slotno,
					      int (*handler) __P((void *sc)),
					      void *sc, int onoff)); 
void	(*tc_enable_interrupt) __P ((u_int slotno,
				     int (*handler) __P ((void *sc)),
				     void *sc, int onoff));
extern	int (*mips_hardware_intr)();

int	kn02_intr(), kmin_intr(), xine_intr();

#ifdef DS3100
extern int	kn01_intr();
void	kn01_enable_intr  __P ((u_int slotno,
				int (*handler) __P ((intr_arg_t sc)),
				intr_arg_t sc, int onoff));
#endif /* DS3100 */

#ifdef DS5100 /* mipsmate */
# include <pmax/pmax/kn230var.h>   /* kn230_establish_intr(), kn230_intr() */
#endif

#ifdef DS5000_240
int	kn03_intr();
#endif

extern	int Mach_spl0(), Mach_spl1(), Mach_spl2(), Mach_spl3(), splhigh();
int	(*Mach_splbio)() = splhigh;
int	(*Mach_splnet)() = splhigh;
int	(*Mach_spltty)() = splhigh;
int	(*Mach_splimp)() = splhigh;
int	(*Mach_splclock)() = splhigh;
int	(*Mach_splstatclock)() = splhigh;
extern	volatile struct chiptime *Mach_clock_addr;
u_long	kmin_tc3_imask, xine_tc3_imask;

#ifdef DS5000_240
u_long	kn03_tc3_imask;
extern u_long latched_cycle_cnt;
#endif

tc_option_t tc_slot_info[TC_MAX_LOGICAL_SLOTS];
static	void asic_init();
extern	void RemconsInit();

#ifdef DS5000_200
void	kn02_enable_intr __P ((u_int slotno,
			       int (*handler) __P((intr_arg_t sc)),
			       intr_arg_t sc, int onoff));
#endif /*DS5000_200*/

#ifdef DS5000_100
void	kmin_enable_intr __P ((u_int slotno, int (*handler) (intr_arg_t sc),
			     intr_arg_t sc, int onoff));
#endif /*DS5000_100*/

#ifdef DS5000_25
void	xine_enable_intr __P ((u_int slotno, int (*handler) (intr_arg_t sc),
			    intr_arg_t sc, int onoff));
#endif /*DS5000_25*/

#ifdef DS5000_240
void	kn03_enable_intr __P ((u_int slotno, int (*handler) (intr_arg_t sc),
			       intr_arg_t sc, int onoff));
#endif /*DS5000_240*/

#if defined(DS5000_200) || defined(DS5000_25) || defined(DS5000_100) || \
    defined(DS5000_240)
volatile u_int *Mach_reset_addr;
#endif /* DS5000_200 || DS5000_25 || DS5000_100 || DS5000_240 */


void	prom_halt __P((int, char *))   __attribute__((__noreturn__));


/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int	safepri = PSL_LOWIPL;

struct	user *proc0paddr;
struct	proc nullproc;		/* for use by swtch_exit() */

/*
 * Do all the stuff that locore normally does before calling main().
 * Process arguments passed to us by the prom monitor.
 * Return the first page address following the system.
 */
void
mach_init(argc, argv, code, cv)
	int argc;
	char *argv[];
	u_int code;
	const struct callback *cv;
{
	register char *cp;
	register int i;
	register unsigned firstaddr;
	register caddr_t v;
	caddr_t start;
	extern char edata[], end[];
	extern char MachUTLBMiss[], MachUTLBMissEnd[];
	extern char mips_R2000_exception[], mips_R2000_exceptionEnd[];

	/* clear the BSS segment */
	v = (caddr_t)mips_round_page(end);
	bzero(edata, v - edata);

	/* Initialize callv so we can do PROM output... */
	if (code == DEC_PROM_MAGIC) {
		callv = cv;
	} else {
		callv = &callvec;
	}

	/* check for direct boot from DS5000 PROM */
	if (argc > 0 && strcmp(argv[0], "boot") == 0) {
		argc--;
		argv++;
	}

	/*
	 * Copy exception-dispatch code down to exception vector.
	 * Initialize locore-function vector.
	 * Clear out the I and D caches.
	 */
#ifdef notyet
	/* XXX locore doesn't set up cpu type early enough for this */
	mips_vector_init();
#else
	mips1_vector_init();
#endif

	/* look at argv[0] and compute bootdev */
	makebootdev(argv[0]);

	/*
	 * Look at arguments passed to us and compute boothowto.
	 */
#ifdef GENERIC
	boothowto = RB_SINGLE | RB_ASKNAME;
#else
	boothowto = RB_SINGLE;
#endif
#ifdef KADB
	boothowto |= RB_KDB;
#endif
	if (argc > 1) {
		for (i = 1; i < argc; i++) {
			for (cp = argv[i]; *cp; cp++) {
				switch (*cp) {
				case 'a': /* autoboot */
					boothowto &= ~RB_SINGLE;
					break;

				case 'd': /* use compiled in default root */
					boothowto |= RB_DFLTROOT;
					break;

				case 'm': /* mini root present in memory */
					boothowto |= RB_MINIROOT;
					break;

				case 'n': /* ask for names */
					boothowto |= RB_ASKNAME;
					break;

				case 'N': /* don't ask for names */
					boothowto &= ~RB_ASKNAME;
				}
			}
		}
	}

#ifdef MFS
	/*
	 * Check to see if a mini-root was loaded into memory. It resides
	 * at the start of the next page just after the end of BSS.
	 */
	if (boothowto & RB_MINIROOT) {
		boothowto |= RB_DFLTROOT;
		v += mfs_initminiroot(v);
	}
#endif

	/*
	 * Init mapping for u page(s) for proc[0], pm_tlbpid 1.
	 */
	start = v;
	curproc->p_addr = proc0paddr = (struct user *)v;
	curproc->p_md.md_regs = proc0paddr->u_pcb.pcb_regs;
	firstaddr = MIPS_KSEG0_TO_PHYS(v);
	for (i = 0; i < UPAGES; i++) {
		MachTLBWriteIndexed(i,
			(UADDR + (i << PGSHIFT)) | (1 << MIPS_TLB_PID_SHIFT),
			curproc->p_md.md_upte[i] = firstaddr | PG_V | PG_M);
		firstaddr += NBPG;
	}
	v += UPAGES * NBPG;
	MachSetPID(1);

	/*
	 * init nullproc for swtch_exit().
	 * init mapping for u page(s), pm_tlbpid 0
	 * This could be used for an idle process.
	 */
	nullproc.p_addr = (struct user *)v;
	nullproc.p_md.md_regs = nullproc.p_addr->u_pcb.pcb_regs;
	bcopy("nullproc", nullproc.p_comm, sizeof("nullproc"));
	for (i = 0; i < UPAGES; i++) {
		nullproc.p_md.md_upte[i] = firstaddr | PG_V | PG_M;
		firstaddr += NBPG;
	}
	v += UPAGES * NBPG;

	/* clear pages for u areas */
	bzero(start, v - start);

	/*
	 * Determine what model of computer we are running on.
	 */
	if (code == DEC_PROM_MAGIC) {
		i = (*cv->_getsysid)();
		cp = "";
	} else {
		if (cp = (*callv->_getenv)("systype"))
			i = atoi(cp);
		else {
			cp = "";
			i = 0;
		}
	}
	/* check for MIPS based platform */
	if (((i >> 24) & 0xFF) != 0x82) {
		printf("Unknown System type '%s' 0x%x\n", cp, i);
		boot(RB_HALT | RB_NOSYNC);
	}

	/*
	 * Initialize physmem_boardmax; assume no SIMM-bank limits.
	 * Adjst later in model-specific code if necessary.
	 */
	physmem_boardmax = MIPS_MAX_MEM_ADDR;

	/* check what model platform we are running on */
	pmax_boardtype = ((i >> 16) & 0xff);

	switch (pmax_boardtype) {

#ifdef DS3100
	case DS_PMAX:	/* DS3100 Pmax */
		/*
		 * Set up interrupt handling and I/O addresses.
		 */
		mips_hardware_intr = kn01_intr;
		tc_enable_interrupt = kn01_enable_intr; /*XXX*/
		Mach_splbio = Mach_spl0;
		Mach_splnet = Mach_spl1;
		Mach_spltty = Mach_spl2;
		Mach_splimp = splhigh; /*XXX Mach_spl1(), if not for malloc()*/
		Mach_splclock = Mach_spl3;
		Mach_splstatclock = Mach_spl3;
		Mach_clock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(KN01_SYS_CLOCK);
		strcpy(cpu_model, "3100");
		break;
#endif /* DS3100 */


#ifdef DS5100
	case DS_MIPSMATE:	/* DS5100 aka mipsmate aka kn230 */
		/* XXX just a guess */
		/*
		 * Set up interrupt handling and I/O addresses.
		 */
		mips_hardware_intr = kn230_intr;
		tc_enable_interrupt = kn01_enable_intr; /*XXX*/
		Mach_splbio = Mach_spl0;
		Mach_splnet = Mach_spl1;
		Mach_spltty = Mach_spl2;
		Mach_splimp = Mach_spl2;
		Mach_splclock = Mach_spl3;
		Mach_splstatclock = Mach_spl3;
		Mach_clock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(KN01_SYS_CLOCK);
		strcpy(cpu_model, "5100");
		break;
#endif /* DS5100 */

#ifdef DS5000_200
	case DS_3MAX:	/* DS5000/200 3max */
		{
		volatile int *csr_addr =
			(volatile int *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CSR);

		Mach_reset_addr =
		    (unsigned *)MIPS_PHYS_TO_KSEG1(KN02_SYS_ERRADR);
		/* clear any memory errors from new-config probes */
		*Mach_reset_addr = 0;

		/*
		 * Enable ECC memory correction, turn off LEDs, and
		 * disable all TURBOchannel interrupts.
		 */
		i = *csr_addr;
		*csr_addr = (i & ~(KN02_CSR_WRESERVED | KN02_CSR_IOINTEN)) |
			KN02_CSR_CORRECT | 0xff;
		mips_hardware_intr = kn02_intr;
		tc_enable_interrupt = kn02_enable_intr;
		Mach_splbio = Mach_spl0;
		Mach_splnet = Mach_spl0;
		Mach_spltty = Mach_spl0;
		Mach_splimp = Mach_spl0;
		Mach_splclock = Mach_spl1;
		Mach_splstatclock = Mach_spl1;
		Mach_clock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(KN02_SYS_CLOCK);

		}
		strcpy(cpu_model, "5000/200");
		break;
#endif /* DS5000_200 */

#ifdef DS5000_100
	case DS_3MIN:	/* DS5000/1xx 3min */
		tc_max_slot = KMIN_TC_MAX;
		tc_min_slot = KMIN_TC_MIN;
		tc_slot_phys_base[0] = KMIN_PHYS_TC_0_START;
		tc_slot_phys_base[1] = KMIN_PHYS_TC_1_START;
		tc_slot_phys_base[2] = KMIN_PHYS_TC_2_START;
		ioasic_base = MIPS_PHYS_TO_KSEG1(KMIN_SYS_ASIC);
		mips_hardware_intr = kmin_intr;
		tc_enable_interrupt = kmin_enable_intr;
		kmin_tc3_imask = (KMIN_INTR_CLOCK | KMIN_INTR_PSWARN |
			KMIN_INTR_TIMEOUT);

		/*
		 * Since all the motherboard interrupts come through the
		 * I/O ASIC, it has to be turned off for all the spls and
		 * since we don't know what kinds of devices are in the
		 * turbochannel option slots, just splhigh().
		 */
		Mach_splbio = splhigh;
		Mach_splnet = splhigh;
		Mach_spltty = splhigh;
		Mach_splimp = splhigh;
		Mach_splclock = splhigh;
		Mach_splstatclock = splhigh;
		Mach_clock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(KMIN_SYS_CLOCK);


		/*
		 * Initialize interrupts.
		 */
		*(u_int *)IOASIC_REG_IMSK(ioasic_base) = KMIN_IM0;
		*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;

		/* clear any memory errors from probes */
		Mach_reset_addr =
		    (u_int*)MIPS_PHYS_TO_KSEG1(KMIN_REG_TIMEOUT);
		(*Mach_reset_addr) = 0;

		strcpy(cpu_model, "5000/1xx");

		/*
		 * The kmin memory hardware seems to wrap  memory addresses
		 * with 4Mbyte SIMMs, which causes the physmem computation
		 * to lose.  Find out how big the SIMMS are and set
		 * max_	physmem accordingly.
		 * XXX Do MAXINEs lose the same way?
		 */
		physmem_boardmax = KMIN_PHYS_MEMORY_END + 1;
		if ((*(int*)(MIPS_PHYS_TO_KSEG1(KMIN_REG_MSR)) &
		     KMIN_MSR_SIZE_16Mb) == 0)
			physmem_boardmax = physmem_boardmax >> 2;
		physmem_boardmax = MIPS_PHYS_TO_KSEG1(physmem_boardmax);

		break;
#endif /* ds5000_100 */

#ifdef DS5000_25
	case DS_MAXINE:	/* DS5000/xx maxine */
		tc_max_slot = XINE_TC_MAX;
		tc_min_slot = XINE_TC_MIN;
		tc_slot_phys_base[0] = XINE_PHYS_TC_0_START;
		tc_slot_phys_base[1] = XINE_PHYS_TC_1_START;
		ioasic_base = MIPS_PHYS_TO_KSEG1(XINE_SYS_ASIC);
		mips_hardware_intr = xine_intr;
		tc_enable_interrupt = xine_enable_intr;
		Mach_splbio = Mach_spl3;
		Mach_splnet = Mach_spl3;
		Mach_spltty = Mach_spl3;
		Mach_splimp = Mach_spl3;
		Mach_splclock = Mach_spl1;
		Mach_splstatclock = Mach_spl1;
		Mach_clock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(XINE_SYS_CLOCK);

		/*
		 * Initialize interrupts.
		 */
		*(u_int *)IOASIC_REG_IMSK(ioasic_base) = XINE_IM0;
		*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;
		/* clear any memory errors from probes */
		Mach_reset_addr =
		    (u_int*)MIPS_PHYS_TO_KSEG1(XINE_REG_TIMEOUT);
		(*Mach_reset_addr) = 0;
		strcpy(cpu_model, "5000/25");
		break;
#endif /*DS5000_25*/

#ifdef DS5000_240
	case DS_3MAXPLUS:	/* DS5000/240 3max+ */
		tc_max_slot = KN03_TC_MAX;
		tc_min_slot = KN03_TC_MIN;
		tc_slot_phys_base[0] = KN03_PHYS_TC_0_START;
		tc_slot_phys_base[1] = KN03_PHYS_TC_1_START;
		tc_slot_phys_base[2] = KN03_PHYS_TC_2_START;
		ioasic_base = MIPS_PHYS_TO_KSEG1(KN03_SYS_ASIC);
		mips_hardware_intr = kn03_intr;
		tc_enable_interrupt = kn03_enable_intr;
		Mach_reset_addr =
		    (u_int *)MIPS_PHYS_TO_KSEG1(KN03_SYS_ERRADR);
		*Mach_reset_addr = 0;

		/*
		 * Reset interrupts, clear any errors from newconf probes
		 */

		Mach_splbio = Mach_spl0;
		Mach_splnet = Mach_spl0;
		Mach_spltty = Mach_spl0;
		Mach_splimp = Mach_spl0;
		Mach_splclock = Mach_spl1;
		Mach_splstatclock = Mach_spl1;
		Mach_clock_addr = (volatile struct chiptime *)
			MIPS_PHYS_TO_KSEG1(KN03_SYS_CLOCK);

		asic_init(0);
		/*
		 * Initialize interrupts.
		 */
		kn03_tc3_imask = KN03_IM0 &
			~(KN03_INTR_TC_0|KN03_INTR_TC_1|KN03_INTR_TC_2);
		*(u_int *)IOASIC_REG_IMSK(ioasic_base) = kn03_tc3_imask;
		*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;
		wbflush();
		/* XXX hard-reset LANCE */
		 *(u_int *)IOASIC_REG_CSR(ioasic_base) |= 0x100;

		/* clear any memory errors from probes */
		*Mach_reset_addr = 0;
		strcpy(cpu_model, "5000/240");
		break;
#endif /* DS5000_240 */

	default:
		printf("kernel not configured for systype 0x%x\n", i);
		boot(RB_HALT | RB_NOSYNC);
	}

	/*
	 * Find out how much memory is available.
	 * Be careful to save and restore the original contents for msgbuf.
	 */
	physmem = btoc((vm_offset_t)v - KERNBASE);
	cp = (char *)MIPS_PHYS_TO_KSEG1(physmem << PGSHIFT);	
	while (cp < (char *)physmem_boardmax) {
	  	int j;
		if (badaddr(cp, 4))
			break;
		i = *(int *)cp;
		j = ((int *)cp)[4];
		*(int *)cp = 0xa5a5a5a5;
		/*
		 * Data will persist on the bus if we read it right away.
		 * Have to be tricky here.
		 */
		((int *)cp)[4] = 0x5a5a5a5a;
		wbflush();
		if (*(int *)cp != 0xa5a5a5a5)
			break;
		*(int *)cp = i;
		((int *)cp)[4] = j;
		cp += NBPG;
		physmem++;
	}

	maxmem = physmem;

#if NLE_IOASIC > 0
	/*
	 * Grab 128K at the top of physical memory for the lance chip
	 * on machines where it does dma through the I/O ASIC.
	 * It must be physically contiguous and aligned on a 128K boundary.
	 */
	if (pmax_boardtype == DS_3MIN || pmax_boardtype == DS_MAXINE ||
		pmax_boardtype == DS_3MAXPLUS) {
		maxmem -= btoc(128 * 1024);
		le_iomem = (maxmem << PGSHIFT);
	}
#endif /* NLE_IOASIC */
#if NASC > 0
	/*
	 * Ditto for the scsi chip. There is probably a way to make asc.c
	 * do dma without these buffers, but it would require major
	 * re-engineering of the asc driver.
	 * They must be 8K in size and page aligned.
	 * (now 16K, as that's how big clustered FFS reads/writes get).
	 */
	if (pmax_boardtype == DS_3MIN || pmax_boardtype == DS_MAXINE ||
		pmax_boardtype == DS_3MAXPLUS) {
		maxmem -= btoc(ASC_NCMD * (16 *1024));
		asc_iomem = (maxmem << PGSHIFT);
	}
#endif /* NASC */

	/*
	 * Initialize error message buffer (at end of core).
	 */
	maxmem -= btoc(sizeof (struct msgbuf));
	msgbufp = (struct msgbuf *)(MIPS_PHYS_TO_KSEG0(maxmem << PGSHIFT));
	msgbufmapped = 1;

	/*
	 * Allocate space for system data structures.
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 *
	 * These data structures are allocated here instead of cpu_startup()
	 * because physical memory is directly addressable. We don't have
	 * to map these into virtual address space.
	 */
	start = v;

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))
#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(callout, struct callout, ncallout);
	valloc(swapmap, struct map, nswapmap = maxproc * 2);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
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
	 * We allocate more buffer space than the BSD standard of
	 * using 10% of memory for the first 2 Meg, 5% of remaining.
	 * We just allocate a flat 10%.  Ensure a minimum of 16 buffers.
	 * We allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0) {
		if (physmem < btoc(2 * 1024 * 1024))
			bufpages = physmem / (10 * CLSIZE);
		else
			bufpages = (btoc(2 * 1024 * 1024) + physmem) /
			    ((100/BUFCACHEPERCENT) * CLSIZE);
	}
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}

	/* Restrict to at most 70% filled kvm */
	if (nbuf * MAXBSIZE >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) * 7 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 7 / 10;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / CLBYTES)
		bufpages = nbuf * MAXBSIZE / CLBYTES;

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);

	/*
	 * Clear allocated memory.
	 */
	bzero(start, v - start);

	/*
	 * Initialize the virtual memory system.
	 */
	pmap_bootstrap((vm_offset_t)v);

}



/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup()
{
	register unsigned i;
	register caddr_t v;
	int base, residual;
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;

	pmapdebug = 0;
#endif

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	printf("real mem = %d\n", ctob(physmem));

	/*
	 * Allocate virtual address space for file I/O buffers.
	 * Note they are different than the array of headers, 'buf',
	 * and usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
				   &maxaddr, size, TRUE);
	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
			&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	if (base >= MAXBSIZE / CLBYTES) {
		/* don't want to alloc more physical mem than needed */
		base = MAXBSIZE / CLBYTES;
		residual = 0;
	}

	for (i = 0; i < nbuf; i++) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base+1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16 * NCARGS, TRUE);
	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 VM_PHYS_SIZE, TRUE);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
				   M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE);
	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];
	callout[i-1].c_next = NULL;

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %d\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Set up CPU-specific registers, cache, etc.
	 */
	initcpu();

	/*
	 * Configure the system.
	 */
	configure();
}

/*
 * machine dependent system variables.
 */
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
	case CPU_CONSDEV:
		return (sysctl_rdstruct(oldp, oldlenp, newp, &cn_tab->cn_dev,
		    sizeof cn_tab->cn_dev));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * Set registers on exec.
 * Clear all registers except sp, pc, and t9.
 * $sp is set to the stack pointer passed in.  $pc is set to the entry
 * point given by the exec_package passed in, as is $t9 (used for PIC
 * code by the MIPS elf abi).
 */
void
setregs(p, pack, stack, retval)
	register struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	extern struct proc *machFPCurProcPtr;

	bzero((caddr_t)p->p_md.md_regs, (FSR + 1) * sizeof(int));
	p->p_md.md_regs[SP] = stack;
	p->p_md.md_regs[PC] = pack->ep_entry & ~3;
        p->p_md.md_regs[T9] = pack->ep_entry & ~3; /* abicall requirement */
	p->p_md.md_regs[PS] = PSL_USERSET;
	p->p_md.md_flags & ~MDP_FPUSED;
	if (machFPCurProcPtr == p)
		machFPCurProcPtr = (struct proc *)0;
}

/*
 * WARNING: code in locore.s assumes the layout shown for sf_signum
 * thru sf_handler so... don't screw with them!
 */
struct sigframe {
	int	sf_signum;		/* signo for handler */
	siginfo_t *sf_sip;		/* pointer to siginfo_t */
	struct	sigcontext *sf_scp;	/* context ptr for handler */
	sig_t	sf_handler;		/* handler addr for u_sigc */
	struct	sigcontext sf_sc;	/* actual context */
	siginfo_t sf_si;
};

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{
	register struct proc *p = curproc;
	register struct sigframe *fp;
	register int *regs;
	register struct sigacts *psp = p->p_sigacts;
	int oonstack, fsize;
	struct sigcontext ksc;
	extern char sigcode[], esigcode[];

	regs = p->p_md.md_regs;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in data space, the
	 * call to grow() is a nop, and the copyout()
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	fsize = sizeof(struct sigframe);
	if (!(psp->ps_siginfo & sigmask(sig)))
		fsize -= sizeof(siginfo_t);
	if ((psp->ps_flags & SAS_ALTSTACK) &&
	    (psp->ps_sigstk.ss_flags & SS_ONSTACK) == 0 &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
					 psp->ps_sigstk.ss_size - fsize);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)(regs[SP] - fsize);
	if ((unsigned)fp <= USRSTACK - ctob(p->p_vmspace->vm_ssize)) 
		(void)grow(p, (unsigned)fp);
#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    (sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d ssp %x usp %x scp %x\n",
		       p->p_pid, sig, &oonstack, fp, &fp->sf_sc);
#endif
	/*
	 * Build the signal context to be used by sigreturn.
	 */
	ksc.sc_onstack = oonstack;
	ksc.sc_mask = mask;
	ksc.sc_pc = regs[PC];
	ksc.mullo = regs [MULLO];
	ksc.mulhi = regs [MULHI];
	ksc.sc_regs[ZERO] = 0xACEDBADE;		/* magic number */
	bcopy((caddr_t)&regs[1], (caddr_t)&ksc.sc_regs[1],
		sizeof(ksc.sc_regs) - sizeof(int));
	ksc.sc_fpused = p->p_md.md_flags & MDP_FPUSED;
	if (ksc.sc_fpused) {
		extern struct proc *machFPCurProcPtr;

		/* if FPU has current state, save it first */
		if (p == machFPCurProcPtr)
			MachSaveCurFPState(p);
		bcopy((caddr_t)&p->p_md.md_regs[F0], (caddr_t)ksc.sc_fpregs,
			sizeof(ksc.sc_fpregs));
	}

	if (psp->ps_siginfo & sigmask(sig)) {
		siginfo_t si;

		initsiginfo(&si, sig, code, type, val);
		if (copyout((caddr_t)&si, (caddr_t)&fp->sf_si, sizeof si))
			goto bail;
	}

	if (copyout((caddr_t)&ksc, (caddr_t)&fp->sf_sc, sizeof(ksc))) {
bail:
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		SIGACTION(p, SIGILL) = SIG_DFL;
		sig = sigmask(SIGILL);
		p->p_sigignore &= ~sig;
		p->p_sigcatch &= ~sig;
		p->p_sigmask &= ~sig;
		psignal(p, SIGILL);
		return;
	}
	/* 
	 * Build the argument list for the signal handler.
	 */
	regs[A0] = sig;
	regs[A1] = (psp->ps_siginfo & sigmask(sig)) ? (int)&fp->sf_si : NULL;
	regs[A2] = (int)&fp->sf_sc;
	regs[A3] = (int)catcher;

	regs[PC] = (int)catcher;
	regs[T9] = (int)catcher;
	regs[SP] = (int)fp;
	/*
	 * Signal trampoline code is at base of user stack.
	 */
	regs[RA] = (int)PS_STRINGS - (esigcode - sigcode);
#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    (sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d returns\n",
		       p->p_pid, sig);
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper priviledges or to cause
 * a machine fault.
 */
/* ARGSUSED */
int
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	register struct sigcontext *scp;
	register int *regs;
	struct sigcontext ksc;
	int error;

	scp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
	regs = p->p_md.md_regs;
	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	error = copyin((caddr_t)scp, (caddr_t)&ksc, sizeof(ksc));
	if (error || ksc.sc_regs[ZERO] != 0xACEDBADE) {
#ifdef DEBUG
		if (!(sigdebug & SDB_FOLLOW))
			printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
		printf("  old sp %x ra %x pc %x\n",
			regs[SP], regs[RA], regs[PC]);
		printf("  new sp %x ra %x pc %x err %d z %x\n",
			ksc.sc_regs[SP], ksc.sc_regs[RA], ksc.sc_regs[PC],
			error, ksc.sc_regs[ZERO]);
#endif
		return (EINVAL);
	}
	scp = &ksc;
	/*
	 * Restore the user supplied information
	 */
	if (scp->sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask &~ sigcantmask;
	regs[PC] = scp->sc_pc;
	regs[MULLO] = scp->mullo;
	regs[MULHI] = scp->mulhi;
	bcopy((caddr_t)&scp->sc_regs[1], (caddr_t)&regs[1],
		sizeof(scp->sc_regs) - sizeof(int));
	if (scp->sc_fpused)
		bcopy((caddr_t)scp->sc_fpregs, (caddr_t)&p->p_md.md_regs[F0],
			sizeof(scp->sc_fpregs));
	return (EJUSTRETURN);
}

int	waittime = -1;
struct pcb dumppcb;


/*
 * These variables are needed by /sbin/savecore
 */
int	dumpmag = (int)0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumplo = 0;

void
dumpconf()
{
	int nblks;

	dumpsize = physmem;
	if (dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize) {
		nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
		if (dumpsize > btoc(dbtob(nblks - dumplo)))
			dumpsize = btoc(dbtob(nblks - dumplo));
		else if (dumplo == 0)
			dumplo = nblks - btodb(ctob(physmem));
	}
	/*
	 * Don't dump on the first CLBYTES (why CLBYTES?)
	 * in case the dump device includes a disk label.
	 */
	if (dumplo < btodb(CLBYTES))
		dumplo = btodb(CLBYTES);
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
void
dumpsys()
{
	int error;

	/* Save registers. */
	savectx((struct user *)&dumppcb, 0);

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
	printf("dump ");
	/*
	 * XXX
	 * All but first arguments to  dump() bogus.
	 * What should blkno, va, size be?
	 */
	error = (*bdevsw[major(dumpdev)].d_dump)(dumpdev, 0, 0, 0);
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

	default:
		printf("error %d\n", error);
		break;

	case 0:
		printf("succeeded\n");
	}
}


/*
 * call PROM to halt or reboot.
 */
volatile void
prom_halt(howto, bootstr)
	int howto;
	char *bootstr;

{
	if (callv != &callvec) {
		if (howto & RB_HALT)
			(*callv->_rex)('h');
		else {
			(*callv->_rex)('b');
		}
	} else if (howto & RB_HALT) {
		volatile void (*f)() = (volatile void (*)())DEC_PROM_REINIT;

		(*f)();	/* jump back to prom monitor */
	} else {
		volatile void (*f)() = (volatile void (*)())DEC_PROM_AUTOBOOT;
		(*f)();	/* jump back to prom monitor and do 'auto' cmd */
	}

	while(1) ;	/* fool gcc */
	/*NOTREACHED*/
}

void
boot(howto)
	register int howto;
{
	extern int cold;

	/* take a snap shot before clobbering any registers */
	if (curproc)
		savectx(curproc->p_addr, 0);

#ifdef DEBUG
	if (panicstr)
		stacktrace();
#endif

	/* If system is cold, just halt. */
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

	/* If "always halt" was specified as a boot flag, obey. */
	if ((boothowto & RB_HALT) != 0)
		howto |= RB_HALT;

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		/*
		 * Synchronize the disks....
		 */
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now unless
		 * the system was sitting in ddb.
		 */
		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}

	/* Disable interrupts. */
	splhigh();

	/* If rebooting and a dump is requested do it. */
#if 0
	if ((howto & (RB_DUMP | RB_HALT)) == RB_DUMP)
#else
	if (howto & RB_DUMP)
#endif
		dumpsys();

	/* run any shutdown hooks */
	doshutdownhooks();

haltsys:

	/* Finally, halt/reboot the system. */
	printf("%s\n\n", howto & RB_HALT ? "halted." : "rebooting...");
	prom_halt(howto & RB_HALT, NULL);
	/*NOTREACHED*/
}


/*
 * Read a high-resolution clock, if one is available, and return
 * the current microsecond offset from time-of-day.
 */

#ifndef DS5000_240
# define clkread() (0)
#else

/*
 * IOASIC TC cycle counter, latched on every interrupt from RTC chip.
 */
u_long latched_cycle_cnt;

/*
 * On a Decstation 5000/240,  use the turbochannel bus-cycle counter
 * to interpolate micro-seconds since the  last RTC clock tick.
 * The interpolation base is the copy of the bus cycle-counter taken
 * by the RTC interrupt handler.
 * XXX on XINE, use the microsecond free-running counter.
 *
 */
static inline u_long
clkread()
{

	register u_long usec, cycles;	/* really 32 bits? */

	/* only support 5k/240 TC bus counter */
	if (pmax_boardtype != DS_3MAXPLUS) {
		return (0);
	}

	cycles = *(u_long*)IOASIC_REG_CTR(ioasic_base);

	/* Compute difference in cycle count from last hardclock() to now */
#if 1
	/* my code, using u_ints */
	cycles = cycles - latched_cycle_cnt;
#else
	/* Mills code, using (signed) ints */
	if (cycles >= latched_cycle_cnt)
		cycles = cycles - latched_cycle_cnt;
	else
		cycles = latched_cycle_cnt - cycles;
#endif

	/*
	 * Scale from 40ns to microseconds.
	 * Avoid a kernel FP divide (by 25) using the approximation 
	 * 1/25 = 40/1000 =~ 41/ 1024, which is good to 0.0975 %
	 */
	usec = cycles + (cycles << 3) + (cycles << 5);
	usec = usec >> 10;

#ifdef CLOCK_DEBUG
	if (usec > 3906 +4) {
		 addlog("clkread: usec %d, counter=%lx\n",
			 usec, latched_cycle_cnt);
		stacktrace();
	}
#endif /*CLOCK_DEBUG*/
	return usec;
}

#if 0
void
microset()
{
		latched_cycle_cnt = *(u_long*)(IOASIC_REG_CTR(ioasic_base));
}
#endif
#endif /*DS5000_240*/


/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  Unfortunately, we can't read the hardware registers.
 * We guarantee that the time will be greater than the value obtained by a
 * previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int s = splclock();
	static struct timeval lasttime;
	register long usec;


	*tvp = time;
	tvp->tv_usec += clkread();
	if (tvp->tv_usec >= 1000000) {
		tvp->tv_usec -= 1000000;
		tvp->tv_sec++;
	}

	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

int
initcpu()
{
	register volatile struct chiptime *c;
	int i;

#if defined(DS5000_200) || defined(DS5000_25) || defined(DS5000_100) || \
    defined(DS5000_240)
	/* Reset after bus errors during probe */
	if (Mach_reset_addr) {
		*Mach_reset_addr = 0;
		wbflush();
	}
#endif

	/* clear any pending interrupts */
	switch (pmax_boardtype) {
	case DS_PMAX:
		break;	/* nothing to  do for KN01. */
	case DS_3MAXPLUS:
	case DS_3MIN:
	case DS_MAXINE:
		*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;
		break;
	case DS_3MAX:
		*(u_int *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CHKSYN) = 0;
		wbflush();
		break;
	default:
		printf("initcpu(): unknown system type 0x%x\n", pmax_boardtype);
		break;
	}

	/*
	 * With newconf, this should be  done elswhere, but without it
	 * we hang (?)
	 */
#if 1 /*XXX*/
	/* disable clock interrupts (until startrtclock()) */
	if (Mach_clock_addr) {
	c = Mach_clock_addr;
	c->regb = REGB_DATA_MODE | REGB_HOURS_FORMAT;
	i = c->regc;
	}
	return (i);
#endif
}

/*
 * Convert an ASCII string into an integer.
 */
int
atoi(s)
	char *s;
{
	int c;
	unsigned base = 10, d;
	int neg = 0, val = 0;

	if (s == 0 || (c = *s++) == 0)
		goto out;

	/* skip spaces if any */
	while (c == ' ' || c == '\t')
		c = *s++;

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
			break;
		case 'B':
		case 'b':
			base = 2;
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
out:
	return val;	
}


#ifdef DS3100

/*
 * Enable an interrupt from a slot on the KN01 internal bus.
 *
 * The 4.4bsd kn01 interrupt handler hard-codes r3000 CAUSE register
 * bits to particular device interrupt handlers.  We may choose to store
 * function and softc pointers at some future point.
 */
void
kn01_enable_intr(slotno, handler, sc, on)
	register unsigned int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	/*
	 */
	if (on)  {
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;
	} else {
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}
}
#endif /* DS3100 */


#ifdef DS5000_200

/*
 * Enable/Disable interrupts for a TURBOchannel slot on the 3MAX.
 */
void
kn02_enable_intr(slotno, handler, sc, on)
	register u_int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	register volatile int *p_csr =
		(volatile int *)MIPS_PHYS_TO_KSEG1(KN02_SYS_CSR);
	int csr;
	int s;

#if 0
	printf("3MAX enable_intr: imask %x, %sabling slot %d, sc %p\n",
	       kn03_tc3_imask, (on? "en" : "dis"), slotno, sc);
#endif

	if (slotno > TC_MAX_LOGICAL_SLOTS)
		panic("kn02_enable_intr: bogus slot %d\n", slotno);

	if (on)  {
		/*printf("kn02: slot %d handler 0x%x\n", slotno, handler);*/
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;
	} else {
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}

	slotno = 1 << (slotno + KN02_CSR_IOINTEN_SHIFT);
	s = Mach_spl0();
	csr = *p_csr & ~(KN02_CSR_WRESERVED | 0xFF);
	if (on)
		*p_csr = csr | slotno;
	else
		*p_csr = csr & ~slotno;
	splx(s);
}
#endif /*DS5000_200*/

#ifdef DS5000_100
/*
 *	Object:
 *		kmin_enable_intr		EXPORTED function
 *
 *	Enable/Disable interrupts from a TURBOchannel slot.
 *
 *	We pretend we actually have 8 slots even if we really have
 *	only 4: TCslots 0-2 maps to slots 0-2, TCslot3 maps to
 *	slots 3-7 (see pmax/tc/ds-asic-conf.c).
 *
 *	3MIN TURBOchannel interrupts are unlike other decstations,
 *	in that interrupt requests from the option slots (0-2) map
 *	directly to R3000 interrupt lines, not to IOASIC interrupt
 *	bits.  If it weren't for that, the 3MIN and 3MAXPLUS could
 *	share   interrupt handlers and interrupt-enable code
 */
void
kmin_enable_intr(slotno, handler, sc, on)
	register unsigned int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	register unsigned mask;

	switch (slotno) {
		/* slots 0-2 don't interrupt through the IOASIC. */
	case 0:
		mask = MIPS_INT_MASK_0;	break;
	case 1:
		mask = MIPS_INT_MASK_1; break;
	case 2:
		mask = MIPS_INT_MASK_2; break;

	case KMIN_SCSI_SLOT:
		mask = (KMIN_INTR_SCSI | KMIN_INTR_SCSI_PTR_LOAD |
			KMIN_INTR_SCSI_OVRUN | KMIN_INTR_SCSI_READ_E);
		break;

	case KMIN_LANCE_SLOT:
		mask = KMIN_INTR_LANCE;
		break;
	case KMIN_SCC0_SLOT:
		mask = KMIN_INTR_SCC_0;
		break;
	case KMIN_SCC1_SLOT:
		mask = KMIN_INTR_SCC_1;
		break;
	case KMIN_ASIC_SLOT:
		mask = KMIN_INTR_ASIC;
		break;
	default:
		return;
	}

#if defined(DEBUG) || defined(DIAGNOSTIC)
	printf("3MIN: imask %x, %sabling slot %d, sc %x addr 0x%x\n",
	       kmin_tc3_imask, (on? "en" : "dis"), slotno, sc, handler);
#endif

	/*
	 * Enable the interrupt  handler, and if it's an IOASIC
	 * slot, set the IOASIC interrupt mask.
	 * Otherwise, set the appropriate spl level in the R3000
	 * register.
	 * Be careful to set handlers  before enabling, and disable
	 * interrupts before clearing handlers.
	 */

	if (on) {
		/* Set the interrupt handler and argument ... */
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;

		/* ... and set the relevant mask */
		if (slotno <= 2) {
			/* it's an option slot */
			int s = splhigh();
			printf("Enabling 3MIN tcslot %d (UNTESTED)\n", slotno);
			s  |= mask;
			splx(s);
		} else {
			/* it's a baseboard device going via the ASIC */
			kmin_tc3_imask |= mask;
		}
	} else {
		/* Clear the relevant mask... */
		if (slotno <= 2) {	
			/* it's an option slot */
			int s = splhigh();
			printf("kmin_intr: cannot disable option slot %d\n",
				slotno);
			s &= ~mask;
			splx(s);
		} else {
			/* it's a baseboard device going via the ASIC */
			kmin_tc3_imask &= ~mask;
		}
		/* ... and clear the handler */
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}
}
#endif /*DS5000_100*/


#ifdef DS5000_25
/*
 *	Object:
 *		xine_enable_intr		EXPORTED function
 *
 *	Enable/Disable interrupts from a TURBOchannel slot.
 *
 *	We pretend we actually have 11 slots even if we really have
 *	only 3: TCslots 0-1 maps to slots 0-1, TCslot 2 is used for
 *	the system (TCslot3), TCslot3 maps to slots 3-10
 *	 (see pmax/tc/ds-asic-conf.c).
 *	Note that all these interrupts come in via the IMR.
 */
void
xine_enable_intr(slotno, handler, sc, on)
	register unsigned int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	register unsigned mask;

	switch (slotno) {
	case 0:			/* a real slot, but  */
		mask = XINE_INTR_TC_0;
		break;
	case 1:			/* a real slot, but */
		mask = XINE_INTR_TC_1;
		break;
	case XINE_FLOPPY_SLOT:
		mask = XINE_INTR_FLOPPY;
		break;
	case XINE_SCSI_SLOT:
		mask = (XINE_INTR_SCSI | XINE_INTR_SCSI_PTR_LOAD |
			XINE_INTR_SCSI_OVRUN | XINE_INTR_SCSI_READ_E);
		break;
	case XINE_LANCE_SLOT:
		mask = XINE_INTR_LANCE;
		break;
	case XINE_SCC0_SLOT:
		mask = XINE_INTR_SCC_0;
		break;
	case XINE_DTOP_SLOT:
		mask = XINE_INTR_DTOP_RX;
		break;
	case XINE_ISDN_SLOT:
		mask = XINE_INTR_ISDN;
		break;
	case XINE_ASIC_SLOT:
		mask = XINE_INTR_ASIC;
		break;
	default:
		return;/* ignore */
	}

	if (on) {
		xine_tc3_imask |= mask;
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;
	} else {
		xine_tc3_imask &= ~mask;
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}
	*(u_int *)IOASIC_REG_IMSK(ioasic_base) = xine_tc3_imask;
}
#endif /*DS5000_25*/

#ifdef DS5000_240
void
kn03_tc_reset()
{
/*
	 * Reset interrupts, clear any errors from newconf probes
	 */
	*(u_int *)IOASIC_REG_INTR(ioasic_base) = 0;
	*(unsigned *)MIPS_PHYS_TO_KSEG1(KN03_SYS_ERRADR) = 0;
}


/*
 *	Object:
 *		kn03_enable_intr		EXPORTED function
 *
 *	Enable/Disable interrupts from a TURBOchannel slot.
 *
 *	We pretend we actually have 8 slots even if we really have
 *	only 4: TCslots 0-2 maps to slots 0-2, TCslot3 maps to
 *	slots 3-7 (see pmax/tc/ds-asic-conf.c).
 */
void
kn03_enable_intr(slotno, handler, sc, on)
	register unsigned int slotno;
	int (*handler) __P((void* softc));
	void *sc;
	int on;
{
	register unsigned mask;

#if 0
	printf("3MAXPLUS: imask %x, %sabling slot %d, unit %d addr 0x%x\n",
	       kn03_tc3_imask, (on? "en" : "dis"), slotno, unit, handler);
#endif

	switch (slotno) {
	case 0:
		mask = KN03_INTR_TC_0;
		break;
	case 1:
		mask = KN03_INTR_TC_1;
		break;
	case 2:
		mask = KN03_INTR_TC_2;
		break;
	case KN03_SCSI_SLOT:
		mask = (KN03_INTR_SCSI | KN03_INTR_SCSI_PTR_LOAD |
			KN03_INTR_SCSI_OVRUN | KN03_INTR_SCSI_READ_E);
		break;
	case KN03_LANCE_SLOT:
		mask = KN03_INTR_LANCE;
		mask |= IOASIC_INTR_LANCE_READ_E;
		break;
	case KN03_SCC0_SLOT:
		mask = KN03_INTR_SCC_0;
		break;
	case KN03_SCC1_SLOT:
		mask = KN03_INTR_SCC_1;
		break;
	case KN03_ASIC_SLOT:
		mask = KN03_INTR_ASIC;
		break;
	default:
#ifdef DIAGNOSTIC
		printf("warning: enabling unknown intr %x\n", slotno);
#endif
		goto done;
	}
	if (on) {
		kn03_tc3_imask |= mask;
		tc_slot_info[slotno].intr = handler;
		tc_slot_info[slotno].sc = sc;

	} else {
		kn03_tc3_imask &= ~mask;
		tc_slot_info[slotno].intr = 0;
		tc_slot_info[slotno].sc = 0;
	}
done:
	*(u_int *)IOASIC_REG_IMSK(ioasic_base) = kn03_tc3_imask;
	wbflush();
}
#endif /* DS5000_240 */


/*
 * Initialize the I/O asic
 */
static void
asic_init(isa_maxine)
	int isa_maxine;
{
	volatile u_int *decoder;

	/* These are common between 3min and maxine */
	decoder = (volatile u_int *)IOASIC_REG_LANCE_DECODE(ioasic_base);
	*decoder = KMIN_LANCE_CONFIG;

	/* set the SCSI DMA configuration map */
	decoder = (volatile u_int *) IOASIC_REG_SCSI_DECODE(ioasic_base);
	(*decoder) = 0x00000000e;
}

/*	$OpenBSD: cpu.h,v 1.13 2000/02/10 20:05:40 mickey Exp $	*/

/* 
 * Copyright (c) 1988-1994, The University of Utah and
 * the Computer Systems Laboratory at the University of Utah (CSL).
 * All rights reserved.
 *
 * Permission to use, copy, modify and distribute this software is hereby
 * granted provided that (1) source code retains these copyright, permission,
 * and disclaimer notices, and (2) redistributions including binaries
 * reproduce the notices in supporting documentation, and (3) all advertising
 * materials mentioning features or use of this software display the following
 * acknowledgement: ``This product includes software developed by the
 * Computer Systems Laboratory at the University of Utah.''
 *
 * THE UNIVERSITY OF UTAH AND CSL ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS
 * IS" CONDITION.  THE UNIVERSITY OF UTAH AND CSL DISCLAIM ANY LIABILITY OF
 * ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: cpu.h 1.19 94/12/16$
 */

#ifndef	_MACHINE_CPU_H_
#define	_MACHINE_CPU_H_

#include <machine/trap.h>
#include <machine/frame.h>

/*
 * Exported definitions unique to hp700/PA-RISC cpu support.
 */

/*
 * definitions of cpu-dependent requirements
 * referenced in generic code
 */
#undef	COPY_SIGCODE		/* copy sigcode above user stack in exec */

#define	HPPA_IOSPACE	0xf0000000
#define	HPPA_IOBCAST	0xfffc0000
#define	HPPA_PDC_LOW	0xef000000
#define	HPPA_PDC_HIGH	0xf1000000
#define	HPPA_FPA	0xfff80000
#define	HPPA_FLEX_DATA	0xfff80001
#define	HPPA_DMA_ENABLE	0x00000001
#define	HPPA_FLEX_MASK	0xfffc0000
#define	HPPA_SPA_ENABLE	0x00000020
#define	HPPA_NMODSPBUS	64

#define	clockframe	trapframe
#define	CLKF_BASEPRI(framep)	((framep)->tf_eiem == ~0U)
#define	CLKF_PC(framep)		((framep)->tf_iioq_head)
#define	CLKF_INTR(framep)	((framep)->tf_flags & TFF_INTR)
#define	CLKF_USERMODE(framep)	((framep)->tf_flags & T_USER)
#define	CLKF_SYSCALL(framep)	((framep)->tf_flags & TFF_SYS)

#define	signotify(p)		(void)(p)
#define	need_resched()		{(void)1;}
#define	need_proftick(p)	{(void)(p);}

#ifndef _LOCORE
#ifdef _KERNEL
#define DELAY(x) delay(x)
void	delay __P((u_int us));
void	hppa_init __P((paddr_t start));
void	trap __P((int type, struct trapframe *frame));
int	kvtop __P((const caddr_t va));
int	dma_cachectl __P((caddr_t p, int size));
int	spcopy __P((pa_space_t ssp, const void *src,
		    pa_space_t dsp, void *dst, size_t size));
int	spstrcpy __P((pa_space_t ssp, const void *src,
		      pa_space_t dsp, void *dst, size_t size, size_t *rsize));
int	copy_on_fault __P((void));
void child_return __P((struct proc *p));
void	switch_trampoline __P((void));
void	switch_exit __P((struct proc *p));
int	cpu_dumpsize __P((void));
int	cpu_dump __P((void));
#endif

/*
 * Boot arguments stuff
 */

#define	BOOTARG_LEN	(NBPG)
#define	BOOTARG_OFF	(0x10000)

/*
 * CTL_MACHDEP definitions.
 */
#define	CPU_CONSDEV		1	/* dev_t: console terminal device */
#define	CPU_MAXID		2	/* number of valid machdep ids */

#define CTL_MACHDEP_NAMES { \
	{ 0, 0 }, \
	{ "console_device", CTLTYPE_STRUCT }, \
}
#endif

#endif /* _MACHINE_CPU_H_ */

/*      $OpenBSD: cpu.h,v 1.9 1999/01/10 13:34:20 niklas Exp $      */
/*      $NetBSD: cpu.h,v 1.24 1997/07/26 10:12:40 ragge Exp $      */

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden
 * All rights reserved.
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
 *      This product includes software developed at Ludd, University of Lule}
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/device.h>

#include <machine/mtpr.h>
#include <machine/pcb.h>
#include <machine/uvax.h>

#define enablertclock()
#define	cpu_wait(p)
#define	cpu_swapout(p)

/*
 * All cpu-dependent info is kept in this struct. Pointer to the
 * struct for the current cpu is set up in locore.c.
 */
struct	cpu_dep {
	void	(*cpu_steal_pages) __P((void)); /* pmap init before mm is on */
	void	(*cpu_clock) __P((void)); /* CPU dep RT clock start */
	int	(*cpu_mchk) __P((caddr_t));   /* Machine check handling */
	void	(*cpu_memerr) __P((void)); /* Memory subsystem errors */
	    /* Autoconfiguration */
	void	(*cpu_conf) __P((struct device *, struct device *, void *));
	int	(*cpu_clkread) __P((time_t));	/* Read cpu clock time */
	void	(*cpu_clkwrite) __P((void));	/* Write system time to cpu */
	int	cpu_vups;	/* speed of cpu */
	u_char  *cpu_intreq;	/* Used on some VAXstations */
	u_char  *cpu_intclr;	/* Used on some VAXstations */
	u_char  *cpu_intmsk;	/* Used on some VAXstations */
	struct	uc_map *cpu_map; /* Map containing important addresses */
	void	(*cpu_halt) __P((void)); /* Cpu dependent halt call */
	void	(*cpu_reboot) __P((int)); /* Cpu dependent reboot call */
};

extern struct cpu_dep *dep_call; /* Holds pointer to current CPU struct. */

struct clockframe {
        int     pc;
        int     ps;
};

extern struct device *booted_from;
extern int cold;
extern int mastercpu;
extern int bootdev;

#define	setsoftnet()	mtpr(12,PR_SIRR)
#define setsoftclock()	mtpr(8,PR_SIRR)
#define	todr()		mfpr(PR_TODR)
/*
 * Preempt the current process if in interrupt from user mode,
 * or after the current trap/syscall if in system mode.
 */

#define need_resched(){ \
	want_resched++; \
	mtpr(AST_OK,PR_ASTLVL); \
	}

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */

#define signotify(p)     mtpr(AST_OK,PR_ASTLVL);

extern	int     want_resched;   /* resched() was called */

/*
 * Give a profiling tick to the current process when the user profiling
 * buffer pages are invalid.  On the hp300, request an ast to send us
 * through trap, marking the proc as needing a profiling tick.
 */
#define need_proftick(p) {(p)->p_flag |= P_OWEUPC; mtpr(AST_OK,PR_ASTLVL); }

/* Some low-level prototypes */
int	badaddr __P((caddr_t, int));
void	cpu_swapin __P((struct proc *));
int	hp_getdev __P((int, int, char **));
int	ra_getdev __P((int, int, int, char **));
void	configure __P((void));
void	dumpconf __P((void));
void	dumpsys __P((void));
void	setroot __P((void));
void	setconf __P((void));
void	swapconf __P((void));
#ifdef DDB
int	kdbrint __P((int));
#endif

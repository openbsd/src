/*	$OpenBSD: freebsd_machdep.h,v 1.7 2003/06/02 23:27:47 millert Exp $	*/
/*	$NetBSD: freebsd_machdep.h,v 1.1 1995/10/10 01:22:35 mycroft Exp $	*/

/*
 * Copyright (c) 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)signal.h	8.1 (Berkeley) 6/11/93
 *	from: Id: signal.h,v 1.4 1994/08/21 04:55:30 paul Exp 
 *
 *	from: @(#)frame.h	5.2 (Berkeley) 1/18/91
 *	from: Id: frame.h,v 1.10 1995/03/16 18:11:42 bde Exp 
 */
#ifndef _FREEBSD_MACHDEP_H
#define _FREEBSD_MACHDEP_H

/*
 * signal support
 */

struct freebsd_sigcontext {
	int	sc_onstack;		/* sigstack state to restore */
	int	sc_mask;		/* signal mask to restore */
	int	sc_esp;			/* machine state */
	int	sc_ebp;
	int	sc_isp;
	int	sc_eip;
	int	sc_eflags;
	int	sc_es;
	int	sc_ds;
	int	sc_cs;
	int	sc_ss;
	int	sc_edi;
	int	sc_esi;
	int	sc_ebx;
	int	sc_edx;
	int	sc_ecx;
	int	sc_eax;
};

struct freebsd_sigframe {
	int	sf_signum;
	int	sf_code;
	struct	freebsd_sigcontext *sf_scp;
	char	*sf_addr;
	sig_t	sf_handler;
	struct	freebsd_sigcontext sf_sc;
};

/*
 * freebsd_ptrace(2) support
 */

#define	FREEBSD_USRSTACK	0xefbfe000 /* USRSTACK */
#define	FREEBSD_U_AR0_OFFSET	0x0000045c /* offsetof(struct user, u_ar0) */
#define	FREEBSD_U_SAVEFP_OFFSET	0x00000070
	/* offsetof(struct user, u_pcb) + offsetof(struct pcb, pcb_savefpu) */

/* Exception/Trap Stack Frame */
struct freebsd_trapframe {
	int	tf_es;
	int	tf_ds;
	int	tf_edi;
	int	tf_esi;
	int	tf_ebp;
	int	tf_isp;
	int	tf_ebx;
	int	tf_edx;
	int	tf_ecx;
	int	tf_eax;
	int	tf_trapno;
	/* below portion defined in 386 hardware */
	int	tf_err;
	int	tf_eip;
	int	tf_cs;
	int	tf_eflags;
	/* below only when transitting rings (e.g. user to kernel) */
	int	tf_esp;
	int	tf_ss;
};

/* Environment information of floating point unit */
struct freebsd_env87 {
	long	en_cw;		/* control word (16bits) */
	long	en_sw;		/* status word (16bits) */
	long	en_tw;		/* tag word (16bits) */
	long	en_fip;		/* floating point instruction pointer */
	u_short	en_fcs;		/* floating code segment selector */
	u_short	en_opcode;	/* opcode last executed (11 bits ) */
	long	en_foo;		/* floating operand offset */
	long	en_fos;		/* floating operand segment selector */
};

/* Contents of each floating point accumulator */
struct freebsd_fpacc87 {
#ifdef dontdef /* too unportable */
	u_long	fp_mantlo;	/* mantissa low (31:0) */
	u_long	fp_manthi;	/* mantissa high (63:32) */
	int	fp_exp:15;	/* exponent */
	int	fp_sgn:1;	/* mantissa sign */
#else
	u_char	fp_bytes[10];
#endif
};

/* Floating point context */
struct freebsd_save87 {
	struct freebsd_env87 sv_env;	/* floating point control/status */
	struct freebsd_fpacc87 sv_ac[8];	/* accumulator contents, 0-7 */
	u_long	sv_ex_sw;		/* status word for last exception */
	/*
	 * Bogus padding for emulators.  Emulators should use their own
	 * struct and arrange to store into this struct (ending here)
	 * before it is inspected for ptracing or for core dumps.  Some
	 * emulators overwrite the whole struct.  We have no good way of
	 * knowing how much padding to leave.  Leave just enough for the
	 * GPL emulator's i387_union (176 bytes total).
	 */
	u_char	sv_pad[64];	/* padding; used by emulators */
};

struct freebsd_ptrace_reg {
	struct freebsd_trapframe freebsd_ptrace_regs;
	struct freebsd_save87 freebsd_ptrace_fpregs;
};

/* sys/i386/include/exec.h */
#define FREEBSD___LDPGSZ	4096

#ifdef _KERNEL
void freebsd_sendsig(sig_t, int, int, u_long, int, union sigval);
#endif

#endif /* _FREEBSD_MACHDEP_H */

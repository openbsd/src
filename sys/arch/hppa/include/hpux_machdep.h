/*	$OpenBSD: hpux_machdep.h,v 1.2 2005/03/26 20:37:24 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _HPPA_HPUX_MACHDEP_H_
#define _HPPA_HPUX_MACHDEP_H_

struct hpux_sigcontext {
	int		sc_syscall;
	int		sc_onstack;
	int		sc_omask;
	char		sc_scact; /* action to take on return from a syscall */
	char		sc_eosys;
	u_short		sc_err;
	register_t	sc_ret0;
	register_t	sc_ret1;
	register_t	sc_args[4];	/* args for the handler */

	/* HP/UX trapframe kinda thing */
	int		sc_tfflags;
	register_t	sc_regs[62];
	int		sc_pad;
	int		sc_fpregs[64];
	int		sc_resv[32];

#if 0
	int		sc_spare[8];
	u_int		sc_flags;
	u_int		sc_ctxptr;
	hpux_sigset_t	sc_sigmask;
	hpux_stack_t	sc_stack;
	hpux_siginfo_t	sc_si;
#endif

	/* call frame follows */
	register_t	sc_frame[4+8];
};

#define	HPUX_SIGCONTEXT_GETCTX	0x01	/* created by getcontext() */

/* trapframe flags */
#define	HPUX_TFF_TRAP		0x0001
#define	HPUX_TFF_SYSCALL	0x0002
#define	HPUX_TFF_INTR		0x0004
#define	HPUX_TFF_ARGSVALID	0x0010
#define	HPUX_TFF_WIDEREGS	0x0040

int hpux_cpu_makecmds(struct proc *p, struct exec_package *epp);
int hpux_cpu_vmcmd(struct proc *p, struct exec_vmcmd *ev);
int hpux_cpu_sysconf_arch(void);
int hpux_sys_getcontext(struct proc *p, void *v, register_t *retval);
int hpux_to_bsd_uoff(int *off, int *isps, struct proc *p);
void hpux_setregs(struct proc *p, struct exec_package *pack,
    u_long stack, register_t *retval);
void hpux_sendsig(sig_t catcher, int sig, int mask, u_long code,
    int type, union sigval val);

#endif /* _HPPA_HPUX_MACHDEP_H_ */

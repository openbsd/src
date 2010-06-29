/*	$OpenBSD: hpux_machdep.c,v 1.5 2010/06/29 00:50:40 jsing Exp $	*/

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

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/signalvar.h>

#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_sig.h>
#include <compat/hpux/hpux_util.h>
#include <compat/hpux/hpux_syscallargs.h>

#include <machine/hpux_machdep.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>

int
hpux_cpu_sysconf_arch(void)
{
	extern int cpu_model_hpux;	/* machdep.c */

	return cpu_model_hpux;
}

int
hpux_sys_getcontext(struct proc *p, void *v, register_t *retval)
{
	struct hpux_sys_getcontext_args *uap = v;
	int len = SCARG(uap, len);

	if (len <= 0)
		return (EINVAL);

	if (len > 4)
		len = 4;

	if (!copyout("FPU", SCARG(uap, buf), len))
		*retval = len;

	return (0);
}

void
hpux_setregs(struct proc *p, struct exec_package *pack, u_long stack,
    register_t *retval)
{
	extern int cpu_model_hpux;	/* machdep.c */
	extern u_int fpu_version;	/* from machdep.c */
	struct ps_strings arginfo;	/* XXX copy back in from the stack */
	struct hpux_keybits {
		int	kb_cpuver;
		int	kb_fpustat;
		int	kb_nbits;
		int	kb_bits[2];
	} hpux_keybits;
	struct trapframe *tf = p->p_md.md_regs;
	struct pcb *pcb = &p->p_addr->u_pcb;
	register_t zero;

	if (copyin((char *)PS_STRINGS, &arginfo, sizeof(arginfo)))
		sigexit(p, SIGILL);

	stack = (stack + 0x1f) & ~0x1f;
	hpux_keybits.kb_cpuver = cpu_model_hpux;
	hpux_keybits.kb_fpustat = fpu_version;
	hpux_keybits.kb_nbits = 1;
	hpux_keybits.kb_bits[0] = 0;	/* TODO report half-word insns */
	hpux_keybits.kb_bits[1] = -1;
	if (copyout(&hpux_keybits, (void *)stack, sizeof(hpux_keybits)))
		sigexit(p, SIGILL);

	tf->tf_flags = TFF_SYS|TFF_LAST;
	tf->tf_iioq_tail = 4 +
	    (tf->tf_iioq_head = pack->ep_entry | HPPA_PC_PRIV_USER);
	tf->tf_rp = 0;
	tf->tf_arg0 = (register_t)arginfo.ps_nargvstr;
	tf->tf_arg1 = (register_t)arginfo.ps_argvstr;
	tf->tf_arg2 = (register_t)arginfo.ps_envstr;
	tf->tf_arg3 = stack;	/* keybits */
	stack += sizeof(hpux_keybits);

	/* setup terminal stack frame */
	stack = (stack + 0x1f) & ~0x1f;
	tf->tf_r3 = stack;
	tf->tf_sp = stack += HPPA_FRAME_SIZE;
	zero = 0;
	copyout(&zero, (caddr_t)(stack - HPPA_FRAME_SIZE), sizeof(register_t));
	copyout(&zero, (caddr_t)(stack + HPPA_FRAME_CRP), sizeof(register_t));

	/* reset any of the pending FPU exceptions */
	pcb->pcb_fpregs->fpr_regs[0] = ((u_int64_t)HPPA_FPU_INIT) << 32;
	pcb->pcb_fpregs->fpr_regs[1] = 0;
	pcb->pcb_fpregs->fpr_regs[2] = 0;
	pcb->pcb_fpregs->fpr_regs[3] = 0;
	if (tf->tf_cr30 == curcpu()->ci_fpu_state) {
		curcpu()->ci_fpu_state = 0;
		/* force an fpu ctxsw, we won't be hugged by the cpu_switch */
		mtctl(0, CR_CCR);
	}
	retval[1] = 0;
}

int
hpux_sigsetreturn(struct proc *p, void *v, register_t *retval)
{
	struct hpux_sigsetreturn_args /* {
		syscallarg(caddr_t) addr;
		syscallarg(int) cookie;
		syscallarg(int) len;
	} */ *uap = v;
	struct pcb *pcb = &p->p_addr->u_pcb;

	/* XXX should fail on second call? */

	pcb->pcb_sigreturn = SCARG(uap, addr);
	pcb->pcb_srcookie = SCARG(uap, cookie);
	pcb->pcb_sclen = SCARG(uap, len);

printf("hpux_sigsetreturn(%p, %x, %d)\n", SCARG(uap, addr), SCARG(uap, cookie), SCARG(uap, len));

	retval[0] = 0;
	return (0);
}

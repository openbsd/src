/*	$OpenBSD: hpux_sig2.c,v 1.1 2004/09/19 21:56:18 mickey Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff
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
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/signalvar.h>
#include <sys/syscallargs.h>

#include <compat/hpux/hpux.h>
#include <compat/hpux/hpux_sig.h>
#include <compat/hpux/hpux_util.h>
#include <compat/hpux/hppa/hpux_syscallargs.h>

int
hpux_sys_sigaltstack(struct proc *p, void *v, register_t *retval)
{
	struct hpux_sys_sigaltstack_args /* {
		syscallarg(struct hpux_sigaltstack *) nss;
		syscallarg(struct hpux_sigaltstack *) oss;
	} */ *uap = v;
	struct sys_sigaltstack_args saa;
	hpux_stack_t hsa;
	stack_t *psa, sa;
	caddr_t sg;
	int error;

	if ((error = copyin(SCARG(uap, nss), &hsa, sizeof hsa)))
		return (error);

	sa.ss_sp = hsa.ss_sp;
	sa.ss_size = hsa.ss_size;
	sa.ss_flags = hsa.ss_flags & SS_ONSTACK;
	if (hsa.ss_flags & HPUX_SS_DISABLE)
		sa.ss_flags |= SS_DISABLE;

	sg = stackgap_init(p->p_emul);
	psa = stackgap_alloc(&sg, 2 * sizeof(struct sigaltstack));
	SCARG(&saa, nss) = &psa[0];
	SCARG(&saa, oss) = &psa[1];

	if ((error = copyout(&sa, psa, sizeof sa)))
		return (error);

	if ((error = sys_sigaltstack(p, &saa, retval)))
		return (error);

	if ((error = copyin(SCARG(&saa, oss), &sa, sizeof sa)))
		return (error);

	hsa.ss_sp = sa.ss_sp;
	hsa.ss_flags = sa.ss_flags & SS_ONSTACK;
	if (sa.ss_flags & SS_DISABLE)
		hsa.ss_flags |= HPUX_SS_DISABLE;
	hsa.ss_size = sa.ss_size;

	return (copyout(&hsa, SCARG(uap, oss), sizeof hsa));
}

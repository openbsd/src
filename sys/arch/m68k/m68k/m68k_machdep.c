/*	$OpenBSD: m68k_machdep.c,v 1.5 2003/01/05 01:51:27 miod Exp $	*/
/*	$NetBSD: m68k_machdep.c,v 1.3 1997/06/12 09:57:04 veego Exp $	*/

/*-
 * Copyright (c) 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Bernd Ernesti.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/ktrace.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/reg.h>

/* the following is used externally (sysctl_hw) */
char	machine_arch[] = MACHINE_ARCH;	/* from <machine/param.h> */

/*
 * Process the tail end of a fork() for the child
 */
void
child_return(arg)
	void *arg;
{
	struct proc *p = (struct proc *)arg;
	struct frame *f = (struct frame *)p->p_md.md_regs;

	f->f_regs[D0] = 0;
	f->f_sr &= ~PSL_C;	/* carry bit */
	f->f_format = FMT0;

	userret(p, f, p->p_sticks, 0, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, SYS_fork, 0, 0);
#endif
}

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int oldipl;

	__asm __volatile ("movew sr,%0" : "=&d" (oldipl));

	oldipl = PSLTOIPL(oldipl);

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
		/*
		 * If the splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		_spl(PSL_S | IPLTOPSL(wantipl));
	}
}
#endif

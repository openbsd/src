/*	$OpenBSD: process_machdep.c,v 1.3 1999/06/18 05:19:52 mickey Exp $	*/

/*
 * Copyright (c) 1999 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/user.h>

int
process_read_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	bcopy (p->p_md.md_regs, regs, sizeof(*regs));
	regs->r_regs[0] = 0;
	return 0;
}

int
process_write_regs(p, regs)
	struct proc *p;
	struct reg *regs;
{
	bcopy (&regs[1], &p->p_md.md_regs->tf_r1, sizeof(*regs) - sizeof(*regs));
	return 0;
}

int
process_read_fpregs(p, fpregs)
	struct proc *p;
	struct fpreg *fpregs;
{
	bcopy (p->p_addr->u_pcb.pcb_fpregs, fpregs, sizeof(*fpregs));
	return 0;
}

int
process_write_fpregs(p, fpregs)
	struct proc *p;
	struct fpreg *fpregs;
{
	bcopy (fpregs, p->p_addr->u_pcb.pcb_fpregs, sizeof(*fpregs));
	return 0;
}

int
process_sstep(p, sstep)
	struct proc *p;
	int sstep;
{
	/* TODO */
	return EINVAL;
}

int
process_set_pc(p, addr)
	struct proc *p;
	caddr_t addr;
{
	if (!USERMODE(addr))	/* XXX */
		return EINVAL;
	p->p_md.md_regs->tf_iioq_head = (register_t)addr;
	return 0;
}


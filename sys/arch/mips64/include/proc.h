/*	$OpenBSD: proc.h,v 1.5 2009/12/07 19:01:03 miod Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)proc.h	8.1 (Berkeley) 6/10/93
 */

#ifndef	_MIPS_PROC_H_
#define	_MIPS_PROC_H_

/*
 * Machine-dependent part of the proc structure.
 */
struct mdproc {
	struct trap_frame *md_regs;	/* registers on current frame */
	volatile int md_astpending;	/* AST pending for this process */
	int	md_flags;		/* machine-dependent flags */
	long	md_ss_addr;		/* single step address for ptrace */
	int	md_ss_instr;		/* single step instruction for ptrace */
	vaddr_t	md_uarea;		/* allocated uarea virtual addr */
/* The following is RM7000 dependent, but kept in for compatibility */
	int	md_pc_ctrl;		/* performance counter control */
	int	md_pc_count;		/* performance counter */
	int	md_pc_spill;		/* performance counter spill */
	quad_t	md_watch_1;
	quad_t	md_watch_2;
	int	md_watch_m;
};

/* md_flags */
#define	MDP_FPUSED	0x00000001	/* floating point coprocessor used */
#define	MDP_PERF	0x00010000	/* Performance counter used */
#define	MDP_WATCH1	0x00020000	/* Watch register 1 used */
#define	MDP_WATCH2	0x00040000	/* Watch register 1 used */
#define	MDP_FORKSAVE	0x0000ffff	/* Flags to save when forking */

#endif	/* !_MIPS_PROC_H_ */

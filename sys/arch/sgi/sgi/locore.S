/*	$OpenBSD: locore.S,v 1.5 2004/10/20 12:49:15 pefo Exp $ */

/*
 * Copyright (c) 2001-2004 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/cpustate.h>
#include <machine/pte.h>

#include "assym.h"

	.set	noreorder		# Noreorder is default style!

	.globl	idle_stack	# idle stack just below kernel.
idle_stack:

	.globl	start
	.globl	kernel_text
kernel_text = start
start:
	mfc0    v0, COP_0_STATUS_REG
	li	v1, ~SR_INT_ENAB
	and	v0, v1
	mtc0    v0, COP_0_STATUS_REG    # disable all interrupts

	mtc0	zero, COP_0_CAUSE_REG	# Clear soft interrupts

/*
 * Initialize stack and call machine startup.
 */
	LA	sp, start - FRAMESZ(CF_SZ)
	LA	gp, _gp
	jal	mips_init			# mips_init(argc, argv, envp)
	sw	zero, CF_RA_OFFS(sp)		# Zero out old ra for debugger

	move	sp, v0				# switch to new stack
	jal	main				# main(regs)
	move	a0, zero
	PANIC("Startup failed!")


/*
 * Interrupt counters for vmstat.
 */
	.data
	.globl intrcnt
	.globl eintrcnt
	.globl intrnames
	.globl eintrnames
intrnames:
	.asciiz	"softclock"
	.asciiz	"softnet"
	.asciiz	"local_dma"
	.asciiz	"local_dev"
	.asciiz	"isa_dev"
	.asciiz	"isa_nmi"
	.asciiz	"clock"
	.asciiz	"statclock"
eintrnames:
	.align	3
intrcnt:
	.word	0,0,0,0,0,0,0,0
eintrcnt:

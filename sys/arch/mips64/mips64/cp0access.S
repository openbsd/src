/*	$OpenBSD: cp0access.S,v 1.3 2004/08/10 20:15:47 deraadt Exp $ */

/*
 * Copyright (c) 2001-2003 Opsycon AB  (www.opsycon.se / www.opsycon.com)
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

/*
 *  Low level code to manage processor specific registers.
 */

#include <sys/errno.h>
#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/regnum.h>
#include <machine/pte.h>

#include "assym.h"

	.set	noreorder		# Noreorder is default style!

/*
 * Set/clear software interrupt.
 */

LEAF(setsoftintr0)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	or	v0, v0, SOFT_INT_MASK_0	# set soft clock interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(setsoftintr0)

LEAF(clearsoftintr0)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	and	v0, v0, ~SOFT_INT_MASK_0	# clear soft clock interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(clearsoftintr0)

LEAF(setsoftintr1)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	or	v0, v0, SOFT_INT_MASK_1	# set soft net interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(setsoftintr1)

LEAF(clearsoftintr1)
	mfc0	v0, COP_0_CAUSE_REG	# read cause register
	nop
	and	v0, v0, ~SOFT_INT_MASK_1	# clear soft net interrupt
	mtc0	v0, COP_0_CAUSE_REG	# save it
	j	ra
	nop
END(clearsoftintr1)

/*
 * Set/change interrupt priority routines.
 * These routines return the previous state.
 */

LEAF(enableintr)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	nop
	or	v1, v0, SR_INT_ENAB
	mtc0	v1, COP_0_STATUS_REG	# enable all interrupts
	j	ra
	nop
END(enableintr)

LEAF(disableintr)
	mfc0	v0, COP_0_STATUS_REG	# read status register
	nop
	and	v1, v0, ~SR_INT_ENAB
	mtc0	v1, COP_0_STATUS_REG	# disable all interrupts
	nop				# Propagate new status
	nop
	nop
	nop				# so int's are off before
	nop				# return to caller.
	nop				# total 10 nops to handle
	nop				# RM7000 1.x bug.
	nop
	nop
	nop
	j	ra
	nop
END(disableintr)

LEAF(updateimask)
	lw	t0, idle_mask
	not	a0, a0			# 1 means masked so invert.
	and	a0, t0			# never upgrade to higher than max
#ifndef IMASK_EXTERNAL
	cfc0	v0, COP_0_ICR
	li	v1, ~IC_INT_MASK
	and	v1, v0
	and	v0, a0, IC_INT_MASK
	or	v1, v0
	ctc0	v1, COP_0_ICR
#endif
	mfc0	v0, COP_0_STATUS_REG
	li	v1, ~SR_INT_MASK
	and	v1, v0
	sll	v0, a0, 8
	and	v0, SR_INT_MASK
	or	v1, v0
	mtc0	v1, COP_0_STATUS_REG
	nop				# total 10 nops to handle
	nop				# RM7000 1.x bug.
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	jr	ra
	move	v0, v1
END(updateimask)

LEAF(setsr)
	mtc0	a0, COP_0_STATUS_REG
	nop				# total 10 nops to handle
	nop				# RM7000 1.x bug.
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	nop
	jr	ra
	move	v0, a0
END(setsr)

LEAF(cp0_get_prid)
	mfc0	v0, COP_0_PRID
	j	ra
	nop
END(cp0_get_prid)

LEAF(cp0_get_count)
	mfc0	v0, COP_0_COUNT
	j	ra
	nop
END(cp0_get_count)

LEAF(cp0_set_compare)
	mtc0	a0, COP_0_COMPARE
	j	ra
	nop
END(cp0_set_compare)

LEAF(cp0_getperfcount)
	mfc0	v0, COP_0_PC_COUNT
	nop; nop
	j	ra
	nop
END(cp0_getperfcount)

LEAF(cp0_setperfcount)
	mtc0	a0, COP_0_PC_COUNT
	nop; nop
	j	ra
	nop
END(cp0_setperfcount)

LEAF(cp0_setperfctrl)
	mtc0	a0, COP_0_PC_CTRL
	nop; nop
	j	ra
	nop
END(cp0_setperfctrl)

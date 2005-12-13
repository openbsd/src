/*	$OpenBSD: microtime.S,v 1.5 2005/12/13 00:18:19 jsg Exp $	*/
/*	$NetBSD: microtime.S,v 1.1 2003/04/26 18:39:30 fvdl Exp $	*/

/*-
 * Copyright (c) 1993 The Regents of the University of California.
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
 */

#include <machine/asm.h>
#include <dev/isa/isareg.h>
#include <dev/ic/i8253reg.h>

#include "assym.h"

#define	IRQ_BIT(irq_num)	(1 << ((irq_num) & 7))
#define	IRQ_BYTE(irq_num)	((irq_num) >> 3)

ENTRY(i8254_microtime)
	# clear registers and do whatever we can up front
	xorl	%edx,%edx
	movl	$(TIMER_SEL0|TIMER_LATCH),%eax

	cli				# disable interrupts

	# select timer 0 and latch its counter
	outb	%al,$IO_TIMER1+TIMER_MODE
	inb	$IO_ICU1,%al		# as close to timer latch as possible
	movb	%al,%ch			# %ch is current ICU mask

	# Read counter value into [%al %dl], LSB first
	inb	$IO_TIMER1+TIMER_CNTR0,%al
	movb	%al,%dl			# %dl has LSB
	inb	$IO_TIMER1+TIMER_CNTR0,%al	# %al has MSB

	# save state of IIR in ICU, and of ipending, for later perusal
	movb	CPUVAR(IPENDING) + IRQ_BYTE(0),%cl
	
	# save the current value of _time
	movq	_C_LABEL(time)(%rip),%r8	# get time.tv_sec
	movq	(_C_LABEL(time)+8)(%rip),%r9	#  and time.tv_usec

	sti				# enable interrupts, we're done

	# At this point we've collected all the state we need to
	# compute the time.  First figure out if we've got a pending
	# interrupt.  If the IRQ0 bit is set in ipending we've taken
	# a clock interrupt without incrementing time, so we bump
	# time.tv_usec by a tick.  Otherwise if the ICU shows a pending
	# interrupt for IRQ0 we (or the caller) may have blocked an interrupt
	# with the cli.  If the counter is not a very small value (3 as
	# a heuristic), i.e. in pre-interrupt state, we add a tick to
	# time.tv_usec

	testb	$IRQ_BIT(0),%cl		# pending interrupt?
	jnz	1f			# yes, increment count

	testb	$IRQ_BIT(0),%ch		# hardware interrupt pending?
	jz	2f			# no, continue
	testb	%al,%al			# MSB zero?
	jnz	1f			# no, add a tick
	cmpb	$3,%dl			# is this small number?
	jbe	2f			# yes, continue
1:	addq	_C_LABEL(isa_timer_tick)(%rip),%r9	# add a tick

	# We've corrected for pending interrupts.  Now do a table lookup
	# based on each of the high and low order counter bytes to increment
	# time.tv_usec
2: 	leaq	_C_LABEL(isa_timer_msb_table)(%rip),%rsi
	movw	(%rsi,%rax,2),%ax
	leaq	_C_LABEL(isa_timer_lsb_table)(%rip),%rsi
	subw	(%rsi,%rdx,2),%ax
	addq	%rax,%r9		# add msb increment

	# Normalize the struct timeval.  We know the previous increments
	# will be less than a second, so we'll only need to adjust accordingly
	cmpq	$1000000,%r9	# carry in timeval?
	jb	3f
	subq	$1000000,%r9	# adjust usec
	incq	%r8		# bump sec
	
3: 	movq	%r8,(%rdi)	# tvp->tv_sec = sec
	movq	%r9,8(%rdi)	# tvp->tv_usec = usec

	ret

/*	$OpenBSD: microtime.s,v 1.13 1998/08/27 05:00:34 deraadt Exp $	*/
/*	$NetBSD: microtime.s,v 1.16 1995/04/17 12:06:47 cgd Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WArRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
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
#include <i386/isa/timerreg.h>

#define	IRQ_BIT(irq_num)	(1 << ((irq_num) % 8))
#define	IRQ_BYTE(irq_num)	((irq_num) / 8)

/*
 * Use a higher resolution version of microtime if HZ is not
 * overridden (i.e. it is 100Hz).
 */
#ifndef HZ
ENTRY(microtime)

#if (defined(I586_CPU) || defined(I686_CPU)) && defined(NTP) 
	movl	_pentium_mhz, %ecx
	testl	%ecx, %ecx
	jne	pentium_microtime
#else
	xorl	%ecx,%ecx
#endif
	movb	$(TIMER_SEL0|TIMER_LATCH),%al

	pushfl
	cli				# disable interrupts

	outb	%al,$TIMER_MODE		# latch timer 0's counter

	# Read counter value into ecx, LSB first
	xorl	%ecx,%ecx
	inb	$TIMER_CNTR0,%al
	movb	%al,%cl
	inb	$TIMER_CNTR0,%al
	movb	%al,%ch

	# Now check for counter overflow.  This is tricky because the
	# timer chip doesn't let us atomically read the current counter
	# value and the output state (i.e., overflow state).  We have
	# to read the ICU interrupt request register (IRR) to see if the
	# overflow has occured.  Because we lack atomicity, we use
	# the (very accurate) heuristic that we do not check for
	# overflow if the value read is close to 0.
	# E.g., if we just checked the IRR, we might read a non-overflowing
	# value close to 0, experience overflow, then read this overflow
	# from the IRR, and mistakenly add a correction to the "close
	# to zero" value.
	#
	# We compare the counter value to the heuristic constant 12.
	# If the counter value is less than this, we assume the counter
	# didn't overflow between disabling clock interrupts and latching
	# the counter value above.  For example, we assume that the first 3
	# instructions take less than 12 microseconds to execute.
	#
	# (We used to check for overflow only if the value read was close to
	# the timer limit, but this doesn't work very well if we're at the
	# clock's ipl or higher.)
	#
	# Otherwise, the counter might have overflowed.  We check for this
	# condition by reading the interrupt request register out of the ICU.
	# If it overflowed, we add in one clock period.

	movl	$11932,%edx	# counter limit

	testb	$IRQ_BIT(0),_ipending + IRQ_BYTE(0)
	jnz	1f

	cmpl	$12,%ecx	# check for potential overflow
	jbe	2f
	
	inb	$IO_ICU1,%al	# read IRR in ICU
	testb	$IRQ_BIT(0),%al	# is a timer interrupt pending?
	jz	2f

1:	subl	%edx,%ecx	# add another tick
	
2:	subl	%ecx,%edx	# subtract counter value from counter limit

	# Divide by 1193280/1000000.  We use a fast approximation of 4096/3433.
	# For values of hz more than 100, this has a maximum error of 2us.

	leal	(%edx,%edx,2),%eax	# a = 3d
	leal	(%edx,%eax,4),%eax	# a = 4a + d = 13d
	movl	%eax,%ecx
	shll	$5,%ecx
	addl	%ecx,%eax		# a = 33a    = 429d
	leal	(%edx,%eax,8),%eax	# a = 8a + d = 3433d
	shrl	$12,%eax		# a = a/4096 = 3433d/4096

common_microtime:
	movl	_time,%edx	# get time.tv_sec
	addl	_time+4,%eax	# add time.tv_usec

	popfl			# enable interrupts
	
	cmpl	$1000000,%eax	# carry in timeval?
	jb	3f
	subl	$1000000,%eax	# adjust usec
	incl	%edx		# bump sec
	
3:	movl	4(%esp),%ecx	# load timeval pointer arg
	movl	%edx,(%ecx)	# tvp->tv_sec = sec
	movl	%eax,4(%ecx)	# tvp->tv_usec = usec

	ret

#if defined(I586_CPU) || defined(I686_CPU)
	.data
	.globl	_pentium_base_tsc
	.comm	_pentium_base_tsc,8
	.text

#if defined (NTP)
	.align	2, 0x90
pentium_microtime:
	pushfl
	cli
	.byte	0x0f, 0x31	# RDTSC
	subl	_pentium_base_tsc,%eax
	sbbl	_pentium_base_tsc+4,%edx
	/*
	 * correct the high word first so we won't
	 * receive a result overflow aka div/0 fault
	 */
	pushl	%eax
	movl	%edx, %eax
	shll	$16, %edx
	divw	%cx
	movzwl	%dx, %edx
	popl	%eax
	divl	%ecx
	jmp	common_microtime
#endif
#endif

#endif

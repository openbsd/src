/*	$OpenBSD: intr_template.c,v 1.7 2009/11/27 00:08:27 syuu Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Common interrupt dispatcher bowels.
 *
 * This file is not a standalone file; to use it, define the following
 * macros and #include <sgi/sgi/intr_template.c>:
 *
 * INTR_FUNCTIONNAME	interrupt handler function name
 * MASK_FUNCTIONNAME	interrupt mask computation function name
 * INTR_GETMASKS	logic to get `imr', `isr', and initialize `bit'
 * INTR_HANDLER(bit)	logic to access intrhand array head for `bit'
 * INTR_IMASK(ipl)	logic to access imask array for `ipl'
 * INTR_LOCAL_DECLS	local declarations (may be empty)
 * MASK_LOCAL_DECLS	local declarations (may be empty)
 * INTR_MASKPENDING	logic to mask `isr'
 * INTR_MASKRESTORE	logic to reset `imr'
 * INTR_MASKSIZE	size of interrupt mask in bits
 * INTR_SPURIOUS(bit)	print a spurious interrupt message for `bit'
 *
 * The following macros are optional:
 * INTR_HANDLER_SKIP(ih)	nonzero to skip intrhand invocation
 */

/*
 * Recompute interrupt masks.
 */
void
MASK_FUNCTIONNAME()
{
	int irq, level;
	struct intrhand *q;
	uint intrlevel[INTR_MASKSIZE];

	MASK_LOCAL_DECLS

	/* First, figure out which levels each IRQ uses. */
	for (irq = 0; irq < INTR_MASKSIZE; irq++) {
		uint levels = 0;
		for (q = INTR_HANDLER(irq); q != NULL; q = q->ih_next)
			levels |= 1 << q->ih_level;
		intrlevel[irq] = levels;
	}

	/*
	 * Then figure out which IRQs use each level.
	 * Note that we make sure never to overwrite imask[IPL_HIGH], in
	 * case an interrupt occurs during intr_disestablish() and causes
	 * an unfortunate splx() while we are here recomputing the masks.
	 */
	for (level = IPL_NONE; level < IPL_HIGH; level++) {
		uint64_t irqs = 0;
		for (irq = 0; irq < INTR_MASKSIZE; irq++)
			if (intrlevel[irq] & (1 << level))
				irqs |= 1UL << irq;
		INTR_IMASK(level) = irqs;
	}

	/*
	 * There are tty, network and disk drivers that use free() at interrupt
	 * time, so vm > (tty | net | bio).
	 *
	 * Enforce a hierarchy that gives slow devices a better chance at not
	 * dropping data.
	 */
	INTR_IMASK(IPL_NET) |= INTR_IMASK(IPL_BIO);
	INTR_IMASK(IPL_TTY) |= INTR_IMASK(IPL_NET);
	INTR_IMASK(IPL_VM) |= INTR_IMASK(IPL_TTY);
	INTR_IMASK(IPL_CLOCK) |= INTR_IMASK(IPL_VM);

	/*
	 * These are pseudo-levels.
	 */
	INTR_IMASK(IPL_NONE) = 0;
	INTR_IMASK(IPL_HIGH) = -1UL;
}

/*
 * Interrupt dispatcher.
 */
uint32_t
INTR_FUNCTIONNAME(uint32_t hwpend, struct trap_frame *frame)
{
	struct cpu_info *ci = curcpu();
	uint64_t imr, isr, mask;
	int ipl;
	int bit;
	struct intrhand *ih;
	int rc;
	INTR_LOCAL_DECLS

	INTR_GETMASKS;

	isr &= imr;
	if (isr == 0)
		return 0;	/* not for us */

	/*
	 * Mask all pending interrupts.
	 */
	INTR_MASKPENDING;

	/*
	 * If interrupts are spl-masked, mask them and wait for splx()
	 * to reenable them when necessary.
	 */
	if ((mask = isr & INTR_IMASK(frame->ipl)) != 0) {
		isr &= ~mask;
		imr &= ~mask;
	}

	/*
	 * Now process allowed interrupts.
	 */
	if (isr != 0) {
		int lvl, bitno;
		uint64_t tmpisr;

		__asm__ (".set noreorder\n");
		ipl = ci->ci_ipl;
		__asm__ ("sync\n\t.set reorder\n");

		/* Service higher level interrupts first */
		for (lvl = IPL_HIGH - 1; lvl != IPL_NONE; lvl--) {
			tmpisr = isr & (INTR_IMASK(lvl) ^ INTR_IMASK(lvl - 1));
			if (tmpisr == 0)
				continue;
			for (bitno = bit, mask = 1UL << bitno; mask != 0;
			    bitno--, mask >>= 1) {
				if ((tmpisr & mask) == 0)
					continue;

				rc = 0;
				for (ih = INTR_HANDLER(bitno); ih != NULL;
				    ih = ih->ih_next) {
#if defined(INTR_HANDLER_SKIP)
					if (INTR_HANDLER_SKIP(ih) != 0)
						continue;
#endif
					splraise(ih->ih_level);
					if ((*ih->ih_fun)(ih->ih_arg) != 0) {
						rc = 1;
						atomic_add_uint64(&ih->ih_count.ec_count, 1);
					}
					__asm__ (".set noreorder\n");
					ci->ci_ipl = ipl;
					__asm__ ("sync\n\t.set reorder\n");
				}
				if (rc == 0)
					INTR_SPURIOUS(bitno);

				isr ^= mask;
				if ((tmpisr ^= mask) == 0)
					break;
			}
		}

		/*
		 * Reenable interrupts which have been serviced.
		 */
		INTR_MASKRESTORE;
	}

	return hwpend;
}

#undef	INTR_FUNCTIONNAME
#undef	MASK_FUNCTIONNAME
#undef	INTR_GETMASKS
#undef	INTR_HANDLER
#undef	INTR_HANDLER_SKIP
#undef	INTR_IMASK
#undef	INTR_LOCAL_DECLS
#undef	MASK_LOCAL_DECLS
#undef	INTR_MASKPENDING
#undef	INTR_MASKRESTORE
#undef	INTR_SPURIOUS

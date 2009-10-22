/*	$OpenBSD: intr_template.c,v 1.1 2009/10/22 22:08:54 miod Exp $	*/

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
 * INTR_GETMASKS	logic to get `imr', `isr', and initialize `bit'
 * INTR_HANDLER(bit)	logic to access intrhand array head for `bit'
 * INTR_IMASK(ipl)	logic to access imask array for `ipl'
 * INTR_LOCAL_DECLS	local declarations (may be empty)
 * INTR_MASKPENDING	logic to mask `isr'
 * INTR_MASKRESTORE	logic to reset `imr'
 * INTR_SPURIOUS(bit)	print a spurious interrupt message for `bit'
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
			for (bitno = bit, mask = 1UL << bitno; tmpisr != 0;
			    bitno--, mask >>= 1) {
				if ((tmpisr & mask) == 0)
					continue;

				rc = 0;
				for (ih = INTR_HANDLER(bitno); ih != NULL;
				    ih = ih->ih_next) {
					splraise(ih->ih_level);
					ih->frame = frame;
					if ((*ih->ih_fun)(ih->ih_arg) != 0) {
						rc = 1;
						ih->ih_count.ec_count++;
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
#undef	INTR_GETMASKS
#undef	INTR_HANDLER
#undef	INTR_IMASK
#undef	INTR_LOCAL_DECLS
#undef	INTR_MASKPENDING
#undef	INTR_MASKRESTORE
#undef	INTR_SPURIOUS

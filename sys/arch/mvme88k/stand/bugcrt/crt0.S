/*	$OpenBSD: crt0.S,v 1.1 2008/04/02 21:53:18 miod Exp $	*/

/*
 * Copyright (c) 2008 Miodrag Vallat.
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

#define	_KERNEL
#define	_LOCORE
#include <machine/asm.h>

	text

#ifdef	STAGE1

ASLOCAL(image)
	/*
	 * Binaries loaded *from disk* by the BUG start with two special words.
	 *
	 * The first word contains the initial stack address, and the second
	 * word contains the initial instruction pointer.
	 *
	 * If the initial instruction pointer is below the address the file was
	 * loaded at (which is found in the VID block or in the NIOT settings),
	 * it is interpreted as an offset within the loaded image.
	 */
	.long	_ASM_LABEL(image) - 0x10
	.long	_ASM_LABEL(start) - _ASM_LABEL(image)

#endif	/* STAGE1 */

#ifdef	STAGE2
	/*
	 * bootxx runs the second-stage binary at its entry + 8, in case
	 * it contains the special two words.
	 */
	NOP
	NOP
#endif

ASGLOBAL(start)
	/*
	 * Registers on entry:
	 * r2	boot device lun
	 * r3	boot controller lun
	 * r4	cold/warm boot mode (`IPL\01')
	 * r5	boot controller address
	 * r6	entry point of loaded program (i.e. address of start)
	 * r7	disk boot: address of media configuration block, zero if none
	 *	net boot: boot information packet address
	 * r8	start of command line (loaded file if netboot)
	 * r9	end of command line
	 * r10	net boot: start of optional argument
	 * r11	net boot: end of optional argument
	 * r12	net boot: bit 0 set if autoboot
	 * r31	stack, as setup above
	 */

#if defined(STAGE2) || defined(NETBOOT)

	/*
	 * We first relocate ourselves to our preferred address.
	 * This helps netbooting if the NIOT address (defaulting to
	 * 001f0000) does not match ours, or if we are loaded by an
	 * old stage1 at the wrong address.
	 */

	or.u	r20, r0,  hi16(_ASM_LABEL(start))
	or	r20, r20, lo16(_ASM_LABEL(start))
	cmp	r21, r6,  r20
	bb1	eq,  r21, reloc_done

	or.u	r22, r0,  hi16(_C_LABEL(end))
	or	r22, r22, lo16(_C_LABEL(end))

	bb1.n	lt,  r21, reloc_down
	 or	r25, r0,  r6

	/*
	 * Relocate from r25 to r20, moving up
	 */
reloc_up:
	ld	r24, r25, r0
	st	r24, r20, r0
	add	r20, r20, 4
	cmp	r21, r20, r22
	bb1.n	ne,  r21, reloc_up
	 add	r25, r25, 4
	br	reloc_done

	/*
	 * Relocate from r25 to r20, moving down
	 */
reloc_down:
	subu	r26, r22, r20	/* _end - start */
	addu	r25, r25, r26
1:
	subu	r25, r25, 4
	ld	r24, r25, r0
	subu	r22, r22, 4
	st	r24, r22, r0
	cmp	r21, r20, r22
	bb1	ne,  r21, 1b

reloc_done:
	or.u	r20, r0,  hi16(_ASM_LABEL(start_relocated))
	or	r20, r20, lo16(_ASM_LABEL(start_relocated))
	jmp.n	r20
	 or	r6,  r20, r0	/* for stack initialization below */

ASLOCAL(start_relocated)

#endif	/* STAGE2 || NETBOOT */

	/*
	 * Clear BSS.
	 */
	or.u	r20, r0,  hi16(_C_LABEL(edata))
	or	r20, r20, lo16(_C_LABEL(edata))
	or.u	r22, r0,  hi16(_C_LABEL(end))
	or	r22, r22, lo16(_C_LABEL(end))
1:	st	r0,  r20, r0
	add	r20, r20, 4
	cmp	r21, r20, r22
	bb1	ne,  r21, 1b

	/*
	 * Setup our stack - it might not have been set up correctly.
	 * We'll use the memory below text as stack, aligned to 0x10
	 * bytes.
	 */
	clr	r31, r6,  4<0>

	/*
	 * Save our arguments so that all registers are available to
	 * C code.
	 */
	or.u	r20, r0,  hi16(_C_LABEL(bugargs))
	or	r20, r20, lo16(_C_LABEL(bugargs))

	st	r2,  r20, 0x00		/* dev_lun */
	st	r3,  r20, 0x04		/* ctrl_lun */
	st	r4,  r20, 0x08		/* flags */
	st	r5,  r20, 0x0c		/* ctrl_addr */
	st	r6,  r20, 0x10		/* entry */
	st	r7,  r20, 0x14		/* conf_blk */
#ifdef	NETBOOT
	st	r10, r20, 0x18		/* arg_start */
	st	r11, r20, 0x1c		/* arg_end */
#else
	st	r8,  r20, 0x18		/* arg_start */
	st	r9,  r20, 0x1c		/* arg_end */
#endif	/* NETBOOT */

	/*
	 * NUL-terminate arg_end, if it is not.
	 */
#ifdef	NETBOOT
	st.b	r0,  r11, r0
#else
	st.b	r0,  r9,  r0
#endif

	/*
	 * PSR when booted from the BUG has SFU1..SFU7 disabled.
	 * Enable SFU1 (the FPU) for the C code to be able to use it
	 * if necessary.
	 */
	ldcr	r20, PSR
	clr	r20, r20, 1<PSR_FPU_DISABLE_BIT>
	stcr	r20, PSR
	NOP
	
	bsr	_C_LABEL(main)
	bsr	_C_LABEL(_rtt)
1:	br	1b

	data
GLOBAL(bugargs)
	.space	4			/* dev_lun */
	.space	4			/* ctrl_lun */
	.space	4			/* flags */
	.space	4			/* ctrl_addr */
	.space	4			/* entry */
	.space	4			/* conf_blk */
	.space	4			/* arg_start */
	.space	4			/* arg_end */

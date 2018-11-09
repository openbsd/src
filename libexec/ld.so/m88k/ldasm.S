/*	$OpenBSD: ldasm.S,v 1.28 2018/11/09 06:16:37 guenther Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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
 * Copyright (c) 2006 Dale Rahn
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

#include <machine/asm.h>
#include <sys/syscall.h>

#define	AUX_base	7
#define	AUX_entry	9

/*
 * ld.so entry point.
 * On entry: r31 points to the kernel argument struct (argv, argv, environ).
 * The environment pointers list is followed by an array of AuxInfo
 * structs filled by the kernel.
 */
ENTRY(_dl_start)
	/*
	 * Two nop because execution may skip up to two instructions.
	 * See setregs() in the kernel for details.
	 *
	 * Note that I have been hacking m88k for years and still fall
	 * into this trap, every time. -- miod
	 */
	or	%r0,  %r0,  %r0
	or	%r0,  %r0,  %r0

	/*
	 * Get some room for the contiguous AUX pointers array.
	  */
	or	%r30, %r31, 0
	subu	%r31, %r31, (4 * (AUX_entry + 1))

	/*
	 * Align the stack to a 16 byte boundary.
	 */
	clr	%r31, %r31, 4<0>

	/*
	 * Invoke _dl_boot_bind
	 */
	or	%r2,  %r30, 0		| kernel args
	or	%r3,  %r31, 0		| array base
	bsr	1f			| the following instruction is skipped
	bsr	_DYNAMIC		| but gives us the pc-relative offset
1:	ld	%r6, %r1, 0		| fetch branch instruction
	mak	%r5, %r6, 26<2>		| pick branch offset and shift left by 2
	addu	%r4, %r5, %r1
	bsr	_dl_boot_bind

	ld	%r2,  %r30, 0		| argc
	addu	%r6,  %r30, 4 + 4
	lda	%r3,  %r6[%r2]		| envp
	ld	%r4,  %r31, 4 * AUX_base| ldoff
	or	%r5,  %r31, 0		| array base
	bsr.n	_dl_boot
	 addu	%r2,  %r30, 4		| argv

	or	%r31, %r30, 0
	bsr	1f			| the following instruction is skipped
	bcnd	eq0, %r0, _dl_dtors	| but gives us the pc-relative offset
1:	ld.h	%r5, %r1, 2		| fetch branch offset (low 16 bits)
	jmp.n	%r2
	 lda	%r5, %r1[%r5]		| cleanup
END(_dl_start)

/*
 * PLT resolver entry point.
 * Invoked with the following stack frame:
 *	r31(0x00)	zero
 *	r31(0x04)	ELF object
 *	r31(0x08)	saved r11
 *	r31(0x0c)	saved r1
 * All registers but r1 and r11 must be preserved. The resolver must return
 * to the resolved address with r1 restored.
 */
#define	OBJECT_OFFSET	(4 * 1)
#define	R11_OFFSET	(4 * 2)
#define	R1_OFFSET	(4 * 3)
#define	PLT_FRAME_SIZE	(4 * 4)
#define REG_SIZE	(4 * 12)
ENTRY(_dl_bind_start)
	/*
	 * Preserve caller-saved registers.
	 */
	subu	%r31, %r31, REG_SIZE
	st.d	%r2,  %r31, 4 * 0
	st.d	%r4,  %r31, 4 * 2
	st.d	%r6,  %r31, 4 * 4
	st.d	%r8,  %r31, 4 * 6
	st	%r10, %r31, 4 * 8
	st.d	%r12, %r31, 4 * 10

	/*
	 * Invoke resolver entry point.
	 */
	ld	%r2,  %r31, REG_SIZE + OBJECT_OFFSET
	bsr.n	_dl_bind
	 ld	%r3,  %r31, REG_SIZE + R11_OFFSET	| reloff

	/*
	 * Preserve return address.
	 */
	or	%r11, %r2,  %r0

	/*
	 * Restore caller-saved registers.
	 */
	ld.d	%r12, %r31, 4 * 10
	ld	%r10, %r31, 4 * 8
	ld.d	%r8,  %r31, 4 * 6
	ld.d	%r6,  %r31, 4 * 4
	ld.d	%r4,  %r31, 4 * 2
	ld.d	%r2,  %r31, 4 * 0
	ld	%r1,  %r31, REG_SIZE + R1_OFFSET

	jmp.n	%r11
	 addu	%r31, %r31, REG_SIZE + PLT_FRAME_SIZE
END(_dl_bind_start)

ENTRY(_dl_cerror)
	jmp.n	%r1
	 subu	%r2, %r0, %r2
END(_dl_cerror)

ENTRY(_dl_cacheflush)
	tb0	0, %r0, 451
	br	_dl_cerror		| never hit
	jmp	%r1
END(_dl_cacheflush)

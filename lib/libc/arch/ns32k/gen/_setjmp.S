/* 
 * Copyright (c) 1992 Helsinki University of Technology
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * HELSINKI UNIVERSITY OF TECHNOLOGY ALLOWS FREE USE OF THIS SOFTWARE IN
 * ITS "AS IS" CONDITION. HELSINKI UNIVERSITY OF TECHNOLOGY DISCLAIMS ANY
 * LIABILITY OF ANY KIND FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE
 * USE OF THIS SOFTWARE.
 */

#include <machine/asm.h>
#include <machine/jmpbuf.h>

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: _setjmp.S,v 1.2 1996/08/19 08:16:37 tholo Exp $"
#endif /* LIBC_SCCS */

ENTRY(_setjmp)
	movd	4(sp), r2		/* jmp_buf */
	movd	0(sp), JMP_BUF_PC(r2)	/* pc of caller */

	sprd	sp, JMP_BUF_SP(r2)
	sprd	fp, JMP_BUF_FP(r2)
	sprd	sb, JMP_BUF_SB(r2)
	movd	r3, JMP_BUF_R3(r2)	/* save registers r3-r7 */
	movd	r4, JMP_BUF_R4(r2)
	movd	r5, JMP_BUF_R5(r2)
	movd	r6, JMP_BUF_R6(r2)
	movd	r7, JMP_BUF_R7(r2)

	movqd	0, r0
	ret	0

ENTRY(_longjmp)
	movd	4(sp), r2		/* jmp_buf */
	movd	8(sp), r0		/* value */

	lprd	sp, JMP_BUF_SP(r2)
	lprd	fp, JMP_BUF_FP(r2)
	lprd	sb, JMP_BUF_SB(r2)
	movd	JMP_BUF_R3(r2), r3	/* load registers r3-r7 */
	movd	JMP_BUF_R4(r2), r4
	movd	JMP_BUF_R5(r2), r5
	movd	JMP_BUF_R6(r2), r6
	movd	JMP_BUF_R7(r2), r7

	movd	JMP_BUF_PC(r2),	0(sp)	/* patch return pc */

	cmpqd	0, r0
	bne	nonzero
	movqd	1, r0
nonzero:
	ret	0


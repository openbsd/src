/*	$OpenBSD: uthread_machdep_asm.S,v 1.2 2004/02/21 05:29:16 drahn Exp $	*/

/*
 * Copyright (c) 2004 Dale Rahn. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */  

#include <machine/asm.h>

/* THESE MUST BE IN SYNC WITH uthread_machdep.c:struct frame */
#define	OFFSET_R	0	/* offsetof(struct frame, r) */
#define OFFSET_FP	((12-4)*4)
#define OFFSET_LR	(OFFSET_FP+4)
#define OFFSET_CPSR	(OFFSET_LR+4)
#define OFFSET_FPR	(OFFSET_CPSR+4)
#define OFFSET_FS	(OFFSET_FPR+(6*8))
#define FRAME_SIZE	(OFFSET_FS+4)


/*
 * r0 is pointer to new save area - restore from here
 * r1 is pointer to old save area - put pointer to stack save area here
 */
ENTRY(_thread_machdep_switch)
	mov	ip, sp
	sub	sp, sp, #FRAME_SIZE+4
	add	r2, sp, #4
	stmia	r2, {r4-r11}
	str	fp, [r2, #OFFSET_FP]
	str	lr, [r2, #OFFSET_LR]

	mrs     r3, cpsr_all
	str	r3, [r2, #OFFSET_CPSR]
#ifndef __VFP_FP__
	sfm	f4, 4, [r2, #OFFSET_FPR]
	rfs	r3
	str	r3, [r2, #OFFSET_FS] 
#endif

	str	sp, [r1, #0]
	ldr	sp, [r0, #0]
	add	r2, sp, #4
	
	ldmfd	r2, {r4-r11}
	ldr	fp, [r2, #OFFSET_FP]
	ldr	lr, [r2, #OFFSET_LR]

	ldr	r3, [r2, #OFFSET_CPSR]
	msr     cpsr_f, r3
#ifndef __VFP_FP__
	lfm	f4, 4, [r2, #OFFSET_FPR]
	ldr	r3, [r2, #OFFSET_FS] 
	wfs	r3
#endif

	add	sp, sp, #FRAME_SIZE+4
	mov	pc, lr

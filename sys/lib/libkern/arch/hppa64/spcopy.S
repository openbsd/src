/* This is a generated file. DO NOT EDIT. */
/*
 * Generated from:
 *
 *	OpenBSD: bcopy.m4
 */
/*
 * Copyright (c) 1999,2004 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#undef _LOCORE
#define _LOCORE
#include <machine/asm.h>
#include <machine/frame.h>


#ifdef _KERNEL
#include <assym.h>

/*
 * int spcopy (pa_space_t ssp, const void *src, pa_space_t dsp, void *dst,
 *              size_t size)
 * do a space to space bcopy.
 *
 * assumes that spaces do not clash, otherwise we lose
 */
	.import	copy_on_fault, code
LEAF_ENTRY(spcopy)
	sub,<>	%r0, arg4, %r0
	bv	%r0(%rp)
	nop

	std	%rp, HPPA_FRAME_RP(%sp)
	ldo	HPPA_FRAME_SIZE(%sp), %sp
	/* setup fault handler */
	mfctl	%cr24, %r1
	ldd	CI_CURPROC(%r1), %r1
	ldil	L%copy_on_fault, %r21
	ldd	P_ADDR(%r1), %r2
	ldo	R%copy_on_fault(%r21), %r21
	ldd	PCB_ONFAULT+U_PCB(%r2), %r1
	std	%r21, PCB_ONFAULT+U_PCB(%r2)

	mtsp	%arg0, %sr1
	mtsp	%arg2, %sr2

	copy	arg4, %ret0
	
	comib,>=,n 15, %ret0, $spcopy.byte

	extrd,u	%arg1, 63, 2, %r20
	extrd,u	%arg3, 63, 2, %r19
	add	%ret0, %r19, %ret0
	comb,<> %r20, %r19, $spcopy.unaligned
	depd	%r0, 63, 2, %arg1
	
	addi	-16, %ret0, %ret0
$spcopy.loop16a
	ldw	 32(%sr1, %arg1), %r0
	ldws,ma	4(%sr1, %arg1), %r22
	ldws,ma	4(%sr1, %arg1), %r21
	ldws,ma	4(%sr1, %arg1), %r20
	ldws,ma	4(%sr1, %arg1), %r19
	stbys,b,m %r22, 4(%sr2, %arg3)
	stws,ma	%r21, 4(%sr2, %arg3)
	stws,ma	%r20, 4(%sr2, %arg3)
	addib,>= -16, %ret0, $spcopy.loop16a
	stws,ma	%r19, 4(%sr2, %arg3)

	addib,<,n 12, %ret0, $spcopy.cleanup
$spcopy.word
	ldws,ma	4(%sr1, %arg1), %r22
	addib,>= -4, %ret0, $spcopy.word
	stws,ma	%r22, 4(%sr2, %arg3)

$spcopy.cleanup
	addib,=,n 4, %ret0, $spcopy.done
	ldws	0(%sr1, %arg1), %r22
	add	%arg3, %ret0, %arg3
	b	$spcopy.done
	stbys,e	%r22, 0(%sr2, %arg3)

$spcopy.unaligned
	sub,>=	%r19, %r20, %r21
	ldwm	4(%sr1, %arg1), %ret1
	zdep	%r21, 28, 29, %r22
	mtsar	%r22
	
	addi	-16, %ret0, %ret0
$spcopy.loop16u
	ldw	 32(%sr1, %arg1), %r0
	ldws,ma	4(%sr1, %arg1), %r22
	ldws,ma	4(%sr1, %arg1), %r21
	ldws,ma	4(%sr1, %arg1), %r20
	ldws,ma	4(%sr1, %arg1), %r19
	vshd	%ret1, %r22, %r31
	stbys,b,m %r31, 4(%sr2, %arg3)
	vshd	%r22, %r21, %r31
	stws,ma	%r31, 4(%sr2, %arg3)
	vshd	%r21, %r20, %r31
	stws,ma	%r31, 4(%sr2, %arg3)
	vshd	%r20, %r19, %r31
	stws,ma	%r31, 4(%sr2, %arg3)
	addib,>= -16, %ret0, $spcopy.loop16u
	copy	%r19, %ret1

	addib,<,n 12, %ret0, $spcopy.cleanup_un
$spcopy.word_un
	ldws,ma	4(%sr1, %arg1), %r22
	vshd	%ret1, %r22, %r21
	addib,<	-4, %ret0, $spcopy.cleanup1_un
	stws,ma	%r21, 4(%sr2, %arg3)
	ldws,ma	4(%sr1, %arg1), %ret1
	vshd	%r22, %ret1, %r21
	addib,>= -4, %ret0, $spcopy.word_un
	stws,ma	%r21, 4(%sr2, %arg3)

$spcopy.cleanup_un
	addib,<=,n 4, %ret0, $spcopy.done
	mfctl	%sar, %r19
	add	%arg3, %ret0, %arg3
	extru	%r19, 28, 2, %r19
	sub,<=	%ret0, %r19, %r0
	ldws,ma	4(%sr1, %arg1), %r22
	vshd	%ret1, %r22, %r21
	b	$spcopy.done
	stbys,e	%r21, 0(%sr2, %arg3)

$spcopy.cleanup1_un
	b	$spcopy.cleanup_un
	copy	%r22, %ret1

$spcopy.byte
	comb,>=,n %r0, %ret0, $spcopy.done
$spcopy.byte_loop
	ldbs,ma	1(%sr1, %arg1), %r22
	addib,<> -1, %ret0, $spcopy.byte_loop
	stbs,ma	%r22, 1(%sr2, %arg3)
$spcopy.done


	mtsp	%r0, %sr1
	mtsp	%r0, %sr2
	/* reset fault handler */
	std	%r1, PCB_ONFAULT+U_PCB(%r2)
	ldo	-HPPA_FRAME_SIZE(%sp), %sp
	ldd	HPPA_FRAME_RP(%sp), %rp
	bv	%r0(%rp)
	copy	%r0, %ret0
EXIT(spcopy)
#endif

	.end

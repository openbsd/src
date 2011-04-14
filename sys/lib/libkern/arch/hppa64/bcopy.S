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


LEAF_ENTRY(memcpy)
ALTENTRY(memmove)
	copy	%arg0, %r22
	copy	%arg1, %arg0
	copy	%r22, %arg1
	copy	%arg0, %ret0
ALTENTRY(ovbcopy)
ALTENTRY(bcopy)
	comb,>,n %arg1, %arg0, $bcopy.reverse
	
	comib,>=,n 15, %arg2, $bcopy_f.byte

	extrd	%arg0, 63, 2, %r20
	extrd	%arg1, 63, 2, %r19
	add	%arg2, %r19, %arg2
	comb,<> %r20, %r19, $bcopy_f.unaligned
	depd	%r0, 63, 2, %arg0
	
	addi	-16, %arg2, %arg2
$bcopy_f.loop16a
	ldw	 32(%sr0, %arg0), %r0
	ldws,ma	4(%sr0, %arg0), %r22
	ldws,ma	4(%sr0, %arg0), %r21
	ldws,ma	4(%sr0, %arg0), %r20
	ldws,ma	4(%sr0, %arg0), %r19
	stbys,b,m %r22, 4(%sr0, %arg1)
	stws,ma	%r21, 4(%sr0, %arg1)
	stws,ma	%r20, 4(%sr0, %arg1)
	addib,>= -16, %arg2, $bcopy_f.loop16a
	stws,ma	%r19, 4(%sr0, %arg1)

	addib,<,n 12, %arg2, $bcopy_f.cleanup
$bcopy_f.word
	ldws,ma	4(%sr0, %arg0), %r22
	addib,>= -4, %arg2, $bcopy_f.word
	stws,ma	%r22, 4(%sr0, %arg1)

$bcopy_f.cleanup
	addib,=,n 4, %arg2, $bcopy_f.done
	ldws	0(%sr0, %arg0), %r22
	add	%arg1, %arg2, %arg1
	b	$bcopy_f.done
	stbys,e	%r22, 0(%sr0, %arg1)

$bcopy_f.unaligned
	sub,>=	%r19, %r20, %r21
	ldwm	4(%sr0, %arg0), %ret1
	zdep	%r21, 28, 29, %r22
	mtsar	%r22
	
	addi	-16, %arg2, %arg2
$bcopy_f.loop16u
	ldw	 32(%sr0, %arg0), %r0
	ldws,ma	4(%sr0, %arg0), %r22
	ldws,ma	4(%sr0, %arg0), %r21
	ldws,ma	4(%sr0, %arg0), %r20
	ldws,ma	4(%sr0, %arg0), %r19
	vshd	%ret1, %r22, %r31
	stbys,b,m %r31, 4(%sr0, %arg1)
	vshd	%r22, %r21, %r31
	stws,ma	%r31, 4(%sr0, %arg1)
	vshd	%r21, %r20, %r31
	stws,ma	%r31, 4(%sr0, %arg1)
	vshd	%r20, %r19, %r31
	stws,ma	%r31, 4(%sr0, %arg1)
	addib,>= -16, %arg2, $bcopy_f.loop16u
	copy	%r19, %ret1

	addib,<,n 12, %arg2, $bcopy_f.cleanup_un
$bcopy_f.word_un
	ldws,ma	4(%sr0, %arg0), %r22
	vshd	%ret1, %r22, %r21
	addib,<	-4, %arg2, $bcopy_f.cleanup1_un
	stws,ma	%r21, 4(%sr0, %arg1)
	ldws,ma	4(%sr0, %arg0), %ret1
	vshd	%r22, %ret1, %r21
	addib,>= -4, %arg2, $bcopy_f.word_un
	stws,ma	%r21, 4(%sr0, %arg1)

$bcopy_f.cleanup_un
	addib,<=,n 4, %arg2, $bcopy_f.done
	mfctl	%sar, %r19
	add	%arg1, %arg2, %arg1
	extru	%r19, 28, 2, %r19
	sub,<=	%arg2, %r19, %r0
	ldws,ma	4(%sr0, %arg0), %r22
	vshd	%ret1, %r22, %r21
	b	$bcopy_f.done
	stbys,e	%r21, 0(%sr0, %arg1)

$bcopy_f.cleanup1_un
	b	$bcopy_f.cleanup_un
	copy	%r22, %ret1

$bcopy_f.byte
	comb,>=,n %r0, %arg2, $bcopy_f.done
$bcopy_f.byte_loop
	ldbs,ma	1(%sr0, %arg0), %r22
	addib,<> -1, %arg2, $bcopy_f.byte_loop
	stbs,ma	%r22, 1(%sr0, %arg1)
$bcopy_f.done

	bv	%r0(%rp)
	nop
$bcopy.reverse
	add	%arg0, %arg2, %arg0
	add	%arg1, %arg2, %arg1


$bcopy_r.byte
	comb,>=,n %r0, %arg2, $bcopy_r.done
$bcopy_r.byte_loop
	ldbs,mb	-1(%sr0, %arg0), %r22
	addib,<> -1, %arg2, $bcopy_r.byte_loop
	stbs,mb	%r22, -1(%sr0, %arg1)
$bcopy_r.done

	bv	%r0(%rp)
	nop
EXIT(memcpy)

	.end

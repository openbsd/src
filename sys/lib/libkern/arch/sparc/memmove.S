/*	$OpenBSD: memmove.S,v 1.6 2014/11/09 16:41:26 miod Exp $	*/

/*
 * Copyright (c) 1996
 * 	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
 *	This product includes software developed by Harvard University.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 *	This product includes software developed by Harvard University.
 *	This product includes software developed by Paul Kranenburg.
 * 4. Neither the name of the University nor the names of its contributors
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

#include <machine/param.h>
#include <machine/asm.h>

/*
 * GNU assembler does not understand `.empty' directive; Sun assembler
 * gripes about labels without it.  To allow cross-compilation using
 * the Sun assembler, and because .empty directives are useful documentation,
 * we use this trick.
 */
#ifdef SUN_AS
#define	EMPTY	.empty
#else
#define	EMPTY	/* .empty */
#endif

/* use as needed to align things on longword boundaries */
#define	_ALIGN	.align 4

#define	BCOPY_SMALL	32	/* if < 32, copy by bytes */

/*
 * memcpy(dst, src, len).  Assumes regions do not overlap; returns dst.
 */
ENTRY(memcpy)
	/*
	 * Swap args, because we may end up in bcopy.
	 */
	mov	%o0, %o5	! save return value
	mov	%o1, %o0
	mov	%o5, %o1
Lbcopy_old:
	cmp	%o2, BCOPY_SMALL
Lbcopy_start:
	bge,a	Lbcopy_fancy	! if >= this many, go be fancy.
	btst	7, %o0		! (part of being fancy)

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
	EMPTY
0:
	inc	%o0
	ldsb	[%o0 - 1], %o4	!	(++dst)[-1] = *src++;
	stb	%o4, [%o1]
	deccc	%o2
	bge	0b
	inc	%o1
1:
	retl
	 mov	%o5, %o0	! return (dst)
	/* NOTREACHED */

	/*
	 * Plenty of data to copy, so try to do it optimally.
	 */
Lbcopy_fancy:
	! check for common case first: everything lines up.
!	btst	7, %o0		! done already
	bne	1f
	EMPTY
	btst	7, %o1
	be,a	Lbcopy_doubles
	dec	8, %o2		! if all lined up, len -= 8, goto bcopy_doubles

	! If the low bits match, we can make these line up.
1:
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3		! if (t & 1) {
	be,a	1f
	btst	1, %o0		! [delay slot: if (src & 1)]

	! low bits do not match, must copy by bytes.
0:
	ldsb	[%o0], %o4	!	do {
	inc	%o0		!		(++dst)[-1] = *src++;
	inc	%o1
	deccc	%o2
	bnz	0b		!	} while (--len != 0);
	stb	%o4, [%o1 - 1]
	retl
	 mov	%o5, %o0	! return (dst)
	/* NOTREACHED */

	! lowest bit matches, so we can copy by words, if nothing else
1:
	be,a	1f		! if (src & 1) {
	btst	2, %o3		! [delay slot: if (t & 2)]

	! although low bits match, both are 1: must copy 1 byte to align
	ldsb	[%o0], %o4	!	*dst++ = *src++;
	stb	%o4, [%o1]
	inc	%o0
	inc	%o1
	dec	%o2		!	len--;
	btst	2, %o3		! } [if (t & 2)]
1:
	be,a	1f		! if (t & 2) {
	btst	2, %o0		! [delay slot: if (src & 2)]
	dec	2, %o2		!	len -= 2;
0:
	ldsh	[%o0], %o4	!	do {
	sth	%o4, [%o1]	!		*(short *)dst = *(short *)src;
	inc	2, %o0		!		dst += 2, src += 2;
	deccc	2, %o2		!	} while ((len -= 2) >= 0);
	bge	0b
	inc	2, %o1
	b	Lbcopy_mopb	!	goto mop_up_byte;
	btst	1, %o2		! } [delay slot: if (len & 1)]
	/* NOTREACHED */

	! low two bits match, so we can copy by longwords
1:
	be,a	1f		! if (src & 2) {
	btst	4, %o3		! [delay slot: if (t & 4)]

	! although low 2 bits match, they are 10: must copy one short to align
	ldsh	[%o0], %o4	!	(*short *)dst = *(short *)src;
	sth	%o4, [%o1]
	inc	2, %o0		!	dst += 2;
	inc	2, %o1		!	src += 2;
	dec	2, %o2		!	len -= 2;
	btst	4, %o3		! } [if (t & 4)]
1:
	be,a	1f		! if (t & 4) {
	btst	4, %o0		! [delay slot: if (src & 4)]
	dec	4, %o2		!	len -= 4;
0:
	ld	[%o0], %o4	!	do {
	st	%o4, [%o1]	!		*(int *)dst = *(int *)src;
	inc	4, %o0		!		dst += 4, src += 4;
	deccc	4, %o2		!	} while ((len -= 4) >= 0);
	bge	0b
	inc	4, %o1
	b	Lbcopy_mopw	!	goto mop_up_word_and_byte;
	btst	2, %o2		! } [delay slot: if (len & 2)]
	/* NOTREACHED */

	! low three bits match, so we can copy by doublewords
1:
	be	1f		! if (src & 4) {
	dec	8, %o2		! [delay slot: len -= 8]
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	st	%o4, [%o1]
	inc	4, %o0		!	dst += 4, src += 4, len -= 4;
	inc	4, %o1
	dec	4, %o2		! }
1:
Lbcopy_doubles:
	mov	%o5, %o3	! save return value
1:
	ldd	[%o0], %o4	! do {
	std	%o4, [%o1]	!	*(double *)dst = *(double *)src;
	inc	8, %o0		!	dst += 8, src += 8;
	deccc	8, %o2		! } while ((len -= 8) >= 0);
	bge	1b
	inc	8, %o1

	! check for a usual case again (save work)
	btst	7, %o2		! if ((len & 7) == 0)
	be	Lbcopy_done	!	goto bcopy_done;
	mov	%o3, %o5	! [delay slot: restore return value]

	btst	4, %o2		! if ((len & 4) == 0)
	be,a	Lbcopy_mopw	!	goto mop_up_word_and_byte;
	btst	2, %o2		! [delay slot: if (len & 2)]
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	st	%o4, [%o1]
	inc	4, %o0		!	dst += 4;
	inc	4, %o1		!	src += 4;
	btst	2, %o2		! } [if (len & 2)]

1:
	! mop up trailing word (if present) and byte (if present).
Lbcopy_mopw:
	be	Lbcopy_mopb	! no word, go mop up byte
	btst	1, %o2		! [delay slot: if (len & 1)]
	ldsh	[%o0], %o4	! *(short *)dst = *(short *)src;
	be	Lbcopy_done	! if ((len & 1) == 0) goto done;
	sth	%o4, [%o1]
	ldsb	[%o0 + 2], %o4	! dst[2] = src[2];
	stb	%o4, [%o1 + 2]
	retl
	 mov	%o5, %o0	! return (dst)
	/* NOTREACHED */

	! mop up trailing byte (if present).
Lbcopy_mopb:
	bne,a	1f
	ldsb	[%o0], %o4

Lbcopy_done:
	retl
	 mov	%o5, %o0	! return (dst)

1:
	stb	%o4,[%o1]
	retl
	 mov	%o5, %o0	! return (dst)

/*
 * memmove(dst, src, len).  Handles overlap; returns dst.
 */
ENTRY(memmove)
	/*
	 * Swap args and continue to bcopy.
	 */
	mov	%o0, %o5	! save dst
	mov	%o1, %o0
	mov	%o5, %o1
/*
 * bcopy(src, dst, len): regions may overlap.
 */
ENTRY(bcopy)
	cmp	%o0, %o1	! src < dst?
	bgeu	Lbcopy_start	! no, go copy forwards as via old bcopy
	cmp	%o2, BCOPY_SMALL! (check length for doublecopy first)

	/*
	 * Since src comes before dst, and the regions might overlap,
	 * we have to do the copy starting at the end and working backwards.
	 */
	add	%o2, %o0, %o0	! src += len
	add	%o2, %o1, %o1	! dst += len
	bge,a	Lback_fancy	! if len >= BCOPY_SMALL, go be fancy
	btst	3, %o0

	/*
	 * Not much to copy, just do it a byte at a time.
	 */
	deccc	%o2		! while (--len >= 0)
	bl	1f
	EMPTY
0:
	dec	%o0		!	*--dst = *--src;
	ldsb	[%o0], %o4
	dec	%o1
	deccc	%o2
	bge	0b
	stb	%o4, [%o1]
1:
	retl
	 mov	%o5, %o0	! return (dst)

	/*
	 * Plenty to copy, try to be optimal.
	 * We only bother with word/halfword/byte copies here.
	 */
Lback_fancy:
!	btst	3, %o0		! done already
	bnz	1f		! if ((src & 3) == 0 &&
	btst	3, %o1		!     (dst & 3) == 0)
	bz,a	Lback_words	!	goto words;
	dec	4, %o2		! (done early for word copy)

1:
	/*
	 * See if the low bits match.
	 */
	xor	%o0, %o1, %o3	! t = src ^ dst;
	btst	1, %o3
	bz,a	3f		! if (t & 1) == 0, can do better
	btst	1, %o0

	/*
	 * Nope; gotta do byte copy.
	 */
2:
	dec	%o0		! do {
	ldsb	[%o0], %o4	!	*--dst = *--src;
	dec	%o1
	deccc	%o2		! } while (--len != 0);
	bnz	2b
	stb	%o4, [%o1]
	retl
	 mov	%o5, %o0	! return (dst)

3:
	/*
	 * Can do halfword or word copy, but might have to copy 1 byte first.
	 */
!	btst	1, %o0		! done earlier
	bz,a	4f		! if (src & 1) {	/* copy 1 byte */
	btst	2, %o3		! (done early)
	dec	%o0		!	*--dst = *--src;
	ldsb	[%o0], %o4
	dec	%o1
	stb	%o4, [%o1]
	dec	%o2		!	len--;
	btst	2, %o3		! }

4:
	/*
	 * See if we can do a word copy ((t&2) == 0).
	 */
!	btst	2, %o3		! done earlier
	bz,a	6f		! if (t & 2) == 0, can do word copy
	btst	2, %o0		! (src&2, done early)

	/*
	 * Gotta do halfword copy.
	 */
	dec	2, %o2		! len -= 2;
5:
	dec	2, %o0		! do {
	ldsh	[%o0], %o4	!	src -= 2;
	dec	2, %o1		!	dst -= 2;
	deccc	2, %o2		!	*(short *)dst = *(short *)src;
	bge	5b		! } while ((len -= 2) >= 0);
	sth	%o4, [%o1]
	b	Lback_mopb	! goto mop_up_byte;
	btst	1, %o2		! (len&1, done early)

6:
	/*
	 * We can do word copies, but we might have to copy
	 * one halfword first.
	 */
!	btst	2, %o0		! done already
	bz	7f		! if (src & 2) {
	dec	4, %o2		! (len -= 4, done early)
	dec	2, %o0		!	src -= 2, dst -= 2;
	ldsh	[%o0], %o4	!	*(short *)dst = *(short *)src;
	dec	2, %o1
	sth	%o4, [%o1]
	dec	2, %o2		!	len -= 2;
				! }

7:
Lback_words:
	/*
	 * Do word copies (backwards), then mop up trailing halfword
	 * and byte if any.
	 */
!	dec	4, %o2		! len -= 4, done already
0:				! do {
	dec	4, %o0		!	src -= 4;
	dec	4, %o1		!	src -= 4;
	ld	[%o0], %o4	!	*(int *)dst = *(int *)src;
	deccc	4, %o2		! } while ((len -= 4) >= 0);
	bge	0b
	st	%o4, [%o1]

	/*
	 * Check for trailing shortword.
	 */
	btst	2, %o2		! if (len & 2) {
	bz,a	1f
	btst	1, %o2		! (len&1, done early)
	dec	2, %o0		!	src -= 2, dst -= 2;
	ldsh	[%o0], %o4	!	*(short *)dst = *(short *)src;
	dec	2, %o1
	sth	%o4, [%o1]	! }
	btst	1, %o2

	/*
	 * Check for trailing byte.
	 */
1:
Lback_mopb:
!	btst	1, %o2		! (done already)
	bnz,a	1f		! if (len & 1) {
	ldsb	[%o0 - 1], %o4	!	b = src[-1];
	retl
	 mov	%o5, %o0	! return (dst)

1:
	stb	%o4, [%o1 - 1]	! }
	retl			!	dst[-1] = b;
	 mov	%o5, %o0	! return (dst)

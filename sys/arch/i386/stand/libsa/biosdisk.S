/*	$OpenBSD: biosdisk.S,v 1.7 1997/04/18 17:48:23 mickey Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <machine/asm.h>
#define	_LOCORE
#include "debug_md.h"
#include "biosdev.h"
#undef	_LOCORE

/*
 * int biosread(dev,cyl,head,sect,nsect,buf)
 *	read number of sectors from disk
 */
ENTRY(biosread)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ecx
	pushl	%ebx
	pushl	%edi
	pushl	%es

	movb	8(%ebp), %dl	# device
	movw	12(%ebp), %cx
	xchgb	%ch, %cl	# cylinder; the highest 2 bits of cyl is in %cl
	rorb	$2, %cl
	movb	16(%ebp), %dh	# head
	movb	20(%ebp), %al
	orb	%al, %cl
	incb	%cl		# sector; sec starts from 1, not 0
	movb	$0x2, %ah	# subfunction
	movb	24(%ebp), %al	# number of sectors
	movl	28(%ebp), %ebx	# offset
	movl	%ebx, %edi	# split off for seg:off pair
	andl	$0xf, %ebx	# atomic read for up to 64k
	shrl	$4, %edi

	BIOSINT(0x13)
	movb	$0, %al
	jnc	1f
	movb	%ah, %al
1:
	movzbl	%al, %eax	# return value in %ax

	popl	%es
	popl	%edi
	popl	%ebx
	popl	%ecx
	popl	%ebp
	ret

ENTRY(bioswrite)
	movl	$1, %eax
	ret

/*
#
# u_int16_t biosdinfo():  return a word that represents the
#	max number of sectors and  heads and drives for this device
#
*/

ENTRY(biosdinfo)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx

	movb	8(%ebp), %dl		# drive #
	movb	$0x08, %ah		# ask for disk info
	BIOSINT(0x13)
	movb	$1, %ah			# max head
	movb	$18, %al		# max sector
	jc	1f

	/* form a word w/ nhead/nsect packed */
	movb	%dh, %ah		# max head
	andb	$0x3f, %cl		# mask of cylinder gunk
	movb	%cl, %al		# max sector (and # sectors)
1:
	popl	%ecx
	popl	%ebx
	popl	%ebp
	ret


/*
#
# biosdreset(): reset disk system
#
*/

ENTRY(biosdreset)
	pushl	%ebx
	pushl	%ecx

	movb	$0x00, %ah		# reset disk system
	BIOSINT(0x13)

	popl	%ecx
	popl	%ebx
	ret


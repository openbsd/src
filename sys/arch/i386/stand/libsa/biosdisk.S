/*	$OpenBSD: biosdisk.S,v 1.13 1997/07/30 19:40:56 flipk Exp $	*/

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
#include <machine/biosvar.h>
#include "debug_md.h"
#undef	_LOCORE
#include <assym.h>


	.globl	_C_LABEL(errno)	/* XXX used for bios errors */
	.globl	_C_LABEL(BIOS_regs)
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
	movl	%edi, _C_LABEL(BIOS_regs)+biosr_es

	DOINT(0x13)
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
 *
 * u_int16_t biosdinfo():  return a word that represents the
 *	max number of sectors and  heads and drives for this device
 *
 */

ENTRY(biosdinfo)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ebx
	pushl	%ecx

	movb	8(%ebp), %dl		# drive #
	movb	$0x08, %ah		# ask for disk info
	DOINT(0x13)
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
 *
 * biosdreset(): reset disk system
 *
 */

ENTRY(biosdreset)
	pushl	%ebx
	pushl	%ecx

	movb	$0x00, %ah		# reset disk system
	movb	8(%ebp), %dl		# drive #
	DOINT(0x13)

	popl	%ecx
	popl	%ebx
	ret

/*
 * int EDDcheck(dev)
 *	check the features supported by the bios for the drive
 */
ENTRY(EDDcheck)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ecx
	pushl	%ebx

	movb	$0x44, %ah
	movw	$0x55aa, %bx
	movl	8(%ebp), %edx
	DOINT(0x13)
	movl	$-1, %eax
	jc	1f
	cmpw	$0xaa55, %bx
	jne	1f
	xorl	%eax, %eax
	movw	%cx, %ax	/* API support bitmap */
1:
	popl	%ebx
	popl	%ecx
	popl	%ebp
	ret

	.data

ddp:	.byte	0x10	# size of packet
	.byte	0	# reserved
	.word	0	# # of blocks to transfer
	.long	0	# address to read/write to
	.long	0,0	# start to read here

	.text
/*
 * int EDDread(dev,abs_sect,nsect,buf)
 *	read nsect starting on absolute sector #abs_sect into buf
 */
ENTRY(EDDread)
	pushl	%ebp
	movl	%esp, %ebp
	pushl	%ecx
	pushl	%ebx
	pushl	%esi

	movl	8(%ebp), %edx	/* drive */
	movl	ddp, %esi
	movl	20(%ebp), %eax	/* nblks */
	movw	%ax, 2(%esi)
	movl	12(%ebp), %eax	/* abs starting sector */
	movl	%eax, 8(%esi)
	movl	16(%ebp), %eax
	movl	%eax, 12(%esi)
	movl	24(%ebp), %eax	/* buffer */
	movl	%eax, 4(%esi)
	movb	$0x42, %ah
	DOINT(0x13)
	jnc	1f
	xorl	%eax, %eax

1:	movzbl	%ah, %eax
	movl	%eax, _C_LABEL(errno)
	movw	2(%esi), %ax	/* return number of blocks read ok */

	popl	%esi
	popl	%ebx
	popl	%ecx
	popl	%ebp
	ret

/*
 * int EDDwrite(dev,abs_sect,nsect,buf)
 *	read nsect starting on absolute sector #abs_sect into buf
 */
ENTRY(EDDwrite)
	movl	$19, _C_LABEL(errno)
	movl	$0, %eax
	ret


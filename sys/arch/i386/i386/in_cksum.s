/*
 * Copyright (c) 1996 Dave Richards <richards@zso.dec.com>
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
 *	This product includes software developed by Dave Richards.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <machine/asm.h>
#include "assym.h"

		.text
ENTRY(in_cksum)
		pushl	%ebp			# save %ebp
		pushl	%ebx			# save %ebx
		pushl	%esi			# save %esi
		pushl	%edi			# save %edi

		movl	20(%esp), %ebp		# %ebp := mp
		movl	24(%esp), %edi		# %edi := len
		xorl	%edx, %edx		# %edx := 0
		xorl	%ecx, %ecx		# %ecx := 0

in_cksum1:	orl	%edi, %edi		# if (%edi == 0)
		je	in_cksum47		#   goto in_cksum47

		orl	%ebp, %ebp		# if (%ebp == NULL)
		je	in_cksum49		#   panic()

		movl	M_DATA(%ebp), %esi	# %esi := %ebp->m_data
		movl	M_LEN(%ebp), %ebx	# %ebx := %ebp->m_len
		movl	M_NEXT(%ebp), %ebp	# %ebp := %ebp->m_next

		cmpl	%edi, %ebx		# %ebx := min(%ebx, %edi)
		jb	in_cksum3		#
		movl	%edi, %ebx		#

in_cksum3:	subl	%ebx, %edi		# %edi := %edi - %ebx

		cmpl	$4, %ebx		# if (%ebx < 4)
		jb	in_cksum42a		#   goto in_cksum42a

		movl	$3, %eax		# %eax := %esi & 3
		andl	%esi, %eax		#
		jmp	*table1(,%eax,4)	# switch (%eax)

in_cksum4:					# case 1:
		roll	$8, %edx		# byte swap
		xorb	$8, %cl			# re-align checksum
		addb	0(%esi), %dh		# checksum byte
		leal	-3(%ebx), %ebx		# %ebx := %ebx - 3
		adcw	1(%esi), %dx		# checksum word
		leal	3(%esi), %esi		# %esi := %esi + 3
		jmp	in_cksum7		# break

in_cksum5:					# case 2:
		addw	0(%esi), %dx		# checksum word
		leal	2(%esi), %esi		# %esi := %esi + 2
		leal	-2(%ebx), %ebx		# %ebx := %ebx - 2
		jmp	in_cksum7		# break

in_cksum6:					# case 3:
		roll	$8, %edx		# byte swap
		xorb	$8, %cl			# re-align checksum
		addb	0(%esi), %dh		# checksum byte
		leal	1(%esi), %esi		# %esi := %esi + 1
		leal	-1(%ebx), %ebx		# %ebx := %ebx - 1

in_cksum7:	adcl	$0, %edx		# complete checksum

in_cksum8:	movb	$3, %ch			# %ch := %bl & 3
		andb	%bl, %ch		#
		shrl	$2, %ebx		# %ebx := %ebx / 4
		je	in_cksum42		# ig (%ebx == 0)
						#   goto in_cksum42

in_cksum9:	movl	$31, %eax		# %eax := %ebx & 31
		andl	%ebx, %eax		#
		leal	(%esi,%eax,4), %esi	# %esi := %esi + %eax * 4
		jmp	*table2(,%eax,4)	# switch (%eax)

in_cksum10:	leal	128(%esi), %esi		# Ugh!
		movl	$32, %eax		# Ugh!
		adcl	-128(%esi), %edx	# checksum 128 bytes
in_cksum11:	adcl	-124(%esi), %edx	# checksum 124 bytes
in_cksum12:	adcl	-120(%esi), %edx	# checksum 120 bytes
in_cksum13:	adcl	-116(%esi), %edx	# checksum 116 bytes
in_cksum14:	adcl	-112(%esi), %edx	# checksum 112 bytes
in_cksum15:	adcl	-108(%esi), %edx	# checksum 108 bytes
in_cksum16:	adcl	-104(%esi), %edx	# checksum 104 bytes
in_cksum17:	adcl	-100(%esi), %edx	# checksum 100 bytes
in_cksum18:	adcl	-96(%esi), %edx		# checksum 96 bytes
in_cksum19:	adcl	-92(%esi), %edx		# checksum 92 bytes
in_cksum20:	adcl	-88(%esi), %edx		# checksum 88 bytes
in_cksum21:	adcl	-84(%esi), %edx		# checksum 84 bytes
in_cksum22:	adcl	-80(%esi), %edx		# checksum 80 bytes
in_cksum23:	adcl	-76(%esi), %edx		# checksum 76 bytes
in_cksum24:	adcl	-72(%esi), %edx		# checksum 72 bytes
in_cksum25:	adcl	-68(%esi), %edx		# checksum 68 bytes
in_cksum26:	adcl	-64(%esi), %edx		# checksum 64 bytes
in_cksum27:	adcl	-60(%esi), %edx		# checksum 60 bytes
in_cksum28:	adcl	-56(%esi), %edx		# checksum 56 bytes
in_cksum29:	adcl	-52(%esi), %edx		# checksum 52 bytes
in_cksum30:	adcl	-48(%esi), %edx		# checksum 48 bytes
in_cksum31:	adcl	-44(%esi), %edx		# checksum 44 bytes
in_cksum32:	adcl	-40(%esi), %edx		# checksum 40 bytes
in_cksum33:	adcl	-36(%esi), %edx		# checksum 36 bytes
in_cksum34:	adcl	-32(%esi), %edx		# checksum 32 bytes
in_cksum35:	adcl	-28(%esi), %edx		# checksum 28 bytes
in_cksum36:	adcl	-24(%esi), %edx		# checksum 24 bytes
in_cksum37:	adcl	-20(%esi), %edx		# checksum 20 bytes
in_cksum38:	adcl	-16(%esi), %edx		# checksum 16 bytes
in_cksum39:	adcl	-12(%esi), %edx		# checksum 12 bytes
in_cksum40:	adcl	-8(%esi), %edx		# checksum 8 bytes
in_cksum41:	adcl	-4(%esi), %edx		# checksum 4 bytes
		adcl	$0, %edx		# complete checksum

		subl	%eax, %ebx		# %ebx := %ebx - %eax
		jne	in_cksum9		# if (%ebx != 0)
						#   goto in_cksum9

in_cksum42:	movb	%ch, %bl		# %ebx := byte count
in_cksum42a:	jmp	*table3(,%ebx,4)	# switch (%ebx)

in_cksum43:					# case 1:
		roll	$8, %edx		# byte swap
		xorb	$8, %cl			# re-align checksum
		addb	0(%esi), %dh		# checksum byte
		jmp	in_cksum46		# break

in_cksum44:					# case 2:
		addw	0(%esi), %dx		# checksum word
		jmp	in_cksum46		# break

in_cksum45:					# case 3:
		xorb	$8, %cl			# re-align checksum
		addw	0(%esi), %dx		# checksum word
		adcw	$0, %dx			# complete checksum
		roll	$8, %edx		# byte swap
		addb	2(%esi), %dh		# checksum byte

in_cksum46:	adcl	$0, %edx		# complete checksum
		jmp	in_cksum1		# next mbuf

in_cksum47:	rorl	%cl, %edx		# re-align checksum
		movzwl	%dx, %eax		# add uppwe and lowe words
		shrl	$16, %edx		#
		addw	%dx, %ax		#
		adcw	$0, %ax			# complete checksum
		notw	%ax			# compute ones complement

in_cksum48:	popl	%edi			# restore %edi
		popl	%esi			# restore %esi
		popl	%ebx			# restore %ebx
		popl	%ebp			# restore %ebp
		ret				# return %eax

in_cksum49:	pushl	panic			# push panic string
		call	_panic			# panic()
		leal	4(%esp), %esp		#
		jmp	in_cksum48		#

		.data

		.align	2

table1:		.long	in_cksum8		# 4-byte aligned
		.long	in_cksum4		# checksum 3 bytes
		.long	in_cksum5		# checksum 2 bytes
		.long	in_cksum6		# checksum 1 byte

table2:		.long	in_cksum10		# checksum 128 bytes
		.long	in_cksum41		# checksum 4 bytes
		.long	in_cksum40		# checksum 8 bytes
		.long	in_cksum39		# checksum 12 bytes
		.long	in_cksum38		# checksum 16 bytes
		.long	in_cksum37		# checksum 20 bytes
		.long	in_cksum36		# checksum 24 bytes
		.long	in_cksum35		# checksum 28 bytes
		.long	in_cksum34		# checksum 32 bytes
		.long	in_cksum33		# checksum 36 bytes
		.long	in_cksum32		# checksum 40 bytes
		.long	in_cksum31		# checksum 44 bytes
		.long	in_cksum30		# checksum 48 bytes
		.long	in_cksum29		# checksum 52 bytes
		.long	in_cksum28		# checksum 56 bytes
		.long	in_cksum27		# checksum 60 bytes
		.long	in_cksum26		# checksum 64 bytes
		.long	in_cksum25		# checksum 68 bytes
		.long	in_cksum24		# checksum 72 bytes
		.long	in_cksum23		# checksum 76 bytes
		.long	in_cksum22		# checksum 80 bytes
		.long	in_cksum21		# checksum 84 bytes
		.long	in_cksum20		# checksum 88 bytes
		.long	in_cksum19		# checksum 92 bytes
		.long	in_cksum18		# checksum 96 bytes
		.long	in_cksum17		# checksum 100 bytes
		.long	in_cksum16		# checksum 104 bytes
		.long	in_cksum15		# checksum 108 bytes
		.long	in_cksum14		# checksum 112 bytes
		.long	in_cksum13		# checksum 116 bytes
		.long	in_cksum12		# checksum 120 bytes
		.long	in_cksum11		# checksum 124 bytes

table3:		.long	in_cksum1		# next mbuf
		.long	in_cksum43		# checksum 1 byte
		.long	in_cksum44		# checksum 2 bytes
		.long	in_cksum45		# checksum 3 bytes

panic:		.asciz	"in_cksum: mp == NULL"

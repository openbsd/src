/*	$OpenBSD: consio2.S,v 1.2 2008/08/12 17:23:21 miod Exp $ */
/*	$NetBSD: consio2.S,v 1.1 2002/02/24 01:04:24 matt Exp $ */
/*
 * Copyright (c) 1994, 1998 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

/*
 * int rom_putchar (int c)	==> putchar() using ROM-routines
 */
ENTRY(rom_putchar, R2)
	movl	4(ap), r2		# move argument to R2
	jsb	*_C_LABEL(rom_putc)	# write it
	ret				# that is all


/*
 * int rom_getchar (void)	==> getchar() using ROM-routines
 */
ENTRY(rom_getchar, R1)
loop:					# do {
	jsb	*_C_LABEL(rom_getc)	#   call the getc-routine
	tstl	r0			#   check if char ready
	beql	loop			# } while (R0 == 0)
	movl	r1, r0			# R1 holds char
	ret				# we are done

ENTRY(rom_testchar, 0)
	mnegl	$1,r0
	jsb	*_C_LABEL(rom_getc)
	tstl	r0
	beql	1f
	movl	r1,r0
1:	ret

ENTRY(_rtt, 0)
	halt


/*
 * int ka630_rom_getchar (void) ==> getchar() using ROM-routines on KA630
 */
ENTRY(ka630_rom_getchar, R11|R1)
	movl	_C_LABEL(ka630_conspage),r11  # load location of console page
1:					# do {
	jsb	*0x1C(r11)		#   call the getc-routine (KA630_GETC)
	blbc	r0,1b			# } while (R0 == 0)
	movl	r1,r0			# R1 holds char
	ret				# we are done

ENTRY(ka630_rom_testchar, R3|R1)
	movl	_C_LABEL(ka630_conspage),r3
	jsb	*0x1C(r3)
	blbc	r0,1f
	movl	r1,r0
1:	ret

/*
 * int ka630_rom_putchar (int c) ==> putchar() using ROM-routines on KA630
 */
ENTRY(ka630_rom_putchar, R11|R1)
	movl	_C_LABEL(ka630_conspage),r11
				# load location of console page
1:				# do {
	jsb	*0x20(r11)	#   is rom ready? (KA630_PUTC_POLL)
	blbc	r0,1b		# } while (R0 == 0)
	movl	4(ap),r1	# R1 holds char
	jsb	*0x24(r11)	# output character (KA630_PUTC)
	ret			# we are done

/*
 * int ka53_rom_getchar (void)	==> getchar() using ROM-routines on KA53
 */
ENTRY(ka53_rom_getchar, R11|R1)
	movl	_C_LABEL(ka53_conspage),r11
				# load location of console page
1:				# do {
	jsb	*0x64(r11)	#   test for char
	blbc	r0,1b		# } while (R0 == 0)
	jsb	*0x6c(r11)	# get the char
	ret			# we are done

ENTRY(ka53_rom_testchar, R3|R1)
	movl	_C_LABEL(ka53_conspage),r3
	jsb	*0x64(r3)
	blbc	r0,1f
	jsb	*0x6c(r3)	# get the char
1:	ret

/*
 * int ka53_rom_putchar (int c) ==> putchar() using ROM-routines on KA53
 */
ENTRY(ka53_rom_putchar, R11|R1)
	movl	_C_LABEL(ka53_conspage),r11
				# load location of console page
1:				# do {
	jsb	*0x20(r11)	#   is rom ready?
	blbc	r0,1b		# } whi	le (R0 == 0)
	movl	4(ap),r1	# R1 holds char
	jsb	*0x24(r11)	# output character
	ret			# we are done

/*
 * int ka60_rom_getchar (void)	==> getchar() using ROM-routines on KA60
 */
ENTRY(ka60_rom_getchar, R11|R10|R9|R8|R7|R6|R5|R4|R3|R2|R1)
1:
	movl	_C_LABEL(ka60_conspage),r0
				# load location of console page
				# do {
	jsb	*0xfc(r0)	#   get the char, if any
	blbc	r0,1b		# } while (R0 == 0)
	movl	r1,r0
	ret			# we are done

ENTRY(ka60_rom_testchar, R11|R10|R9|R8|R7|R6|R5|R4|R3|R2|R1)
	movl	_C_LABEL(ka60_conspage),r0
	jsb	*0xfc(r0)
	blbc	r0,1f
	movl	r1,r0
1:	ret

/*
 * int ka60_rom_putchar (int c) ==> putchar() using ROM-routines on KA60
 */
ENTRY(ka60_rom_putchar, R11|R10|R9|R8|R7|R6|R5|R4|R3|R2|R1)
1:
	movl	_C_LABEL(ka60_conspage),r0
				# load location of console page
	movl	4(ap),r1	# R1 holds char
	jsb	*0x114(r0)	# output character
	blbc	r0, 1b		# failed? retry
	ret			# we are done
